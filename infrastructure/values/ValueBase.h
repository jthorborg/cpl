#ifndef CPL_VALUEBASE_H
#define CPL_VALUEBASE_H

#include "../parameters/CustomFormatters.h"
#include "../parameters/CustomTransforms.h"
#include "../parameters/ParameterSystem.h"
#include "../../gui/Tools.h"
#include "../../Misc.h"
#include "../../state/Serialization.h"
#include "../../stdext.h"
#include <set>
#include <array>
#include <string>
#include <vector>
#include <memory>

namespace cpl
{

	typedef double ValueT;

	class ContextualName
	{
	public:
		virtual std::string getContextualName() { return empty_string; }
		virtual ~ContextualName() {}
	};

	class ValueEntityBase
		: public CSerializer::Serializable
		, public ContextualName
		, private Utility::CPubliclyNoncopyable
	{
	public:

		class ValueEntityListener /* : public virtual DestructionNotifier */
		{
		public:
			/// <summary>
			/// THe sender value is optional (may be zero), but it can be used to detect if you recived
			/// the message from yourself - for instance.
			/// </summary>
			virtual void valueEntityChanged(ValueEntityListener * sender, ValueEntityBase * value) = 0;
			virtual ~ValueEntityListener() {};
		};

		typedef ValueEntityListener Listener;

		virtual const VirtualTransformer<ValueT> & getTransformer() const = 0;
		VirtualTransformer<ValueT> & getTransformer() { return const_cast<VirtualTransformer<ValueT>&>(const_cast<const ValueEntityBase*>(this)->getTransformer()); }
		virtual VirtualFormatter<ValueT> & getFormatter() = 0;
		virtual ValueT getNormalizedValue() const = 0;
		virtual void setNormalizedValue(ValueT parameter) = 0;

		virtual void beginChangeGesture() {};
		virtual void endChangeGesture() {};
		virtual void addListener(ValueEntityListener * listener) = 0;
		virtual void removeListener(ValueEntityListener * listener) = 0;

		void serialize(CSerializer::Archiver & ar, Version version) override { ar << getNormalizedValue(); }
		void deserialize(CSerializer::Builder & builder, Version version) override
		{
			ValueT value;
			builder >> value;
			if (builder.getModifier(CSerializer::Modifiers::RestoreValue))
				setNormalizedValue(value);
		}

		ValueT getTransformedValue() const noexcept
		{
			return getTransformer().transform(getNormalizedValue());
		}

		void setTransformedValue(ValueT val)
		{
			setNormalizedValue(getTransformer().normalize(val));
		}

		template<typename EnumT>
		EnumT getAsTEnum() const noexcept
		{
			return enum_cast<EnumT>(getTransformedValue());
		}

		template<typename EnumT>
		void setAsTEnum(EnumT value) 
		{
			return setTransformedValue(enum_cast<ValueT>(value));
		}

		std::string getFormattedValue()
		{
			std::string ret;
			getFormatter().format(getTransformer().transform(getNormalizedValue()), ret);
			return ret;
		}

		bool setFormattedValue(const string_ref formattedValue)
		{
			ValueT val = 0;
			if (getFormatter().interpret(formattedValue, val))
			{
				setNormalizedValue(getTransformer().normalize(val));
				return true;
			}
			return false;
		}

		virtual ~ValueEntityBase() {}
	};

	class ValueGroup
		: public ContextualName
		, public CSerializer::Serializable
		, private Utility::CPubliclyNoncopyable
	{
	public:
		virtual ValueEntityBase & getValueIndex(std::size_t i) = 0;
		virtual std::size_t getNumValues() const noexcept = 0;

		void serialize(CSerializer::Archiver & archiver, cpl::Version v) override
		{
			for (std::size_t i = 0; i < getNumValues(); ++i)
				archiver << getValueIndex(i).getNormalizedValue();
		}

		void deserialize(CSerializer::Builder & builder, cpl::Version v) override
		{
			for (std::size_t i = 0; i < getNumValues(); ++i)
			{
				ValueT val;
				builder >> val;
				getValueIndex(i).setNormalizedValue(val);
			}
		}
	};


	class DefaultValueListenerEntity : public ValueEntityBase
	{
	public:

		virtual void addListener(ValueEntityListener * listener) override { listeners.insert(listener); }
		virtual void removeListener(ValueEntityListener * listener) override { listeners.erase(listener); }

	protected:

		void notifyListeners()
		{
			for (auto l : listeners)
				l->valueEntityChanged(nullptr, this);
		}

	private:
		std::set<ValueEntityListener *> listeners;
	};

	template<typename Transformer = VirtualTransformer<ValueT>, typename Formatter = VirtualFormatter<ValueT>>
	class SelfcontainedValue : public DefaultValueListenerEntity
	{
	public:
		SelfcontainedValue(Transformer * transformer, Formatter * formatter)
			: internalValue(0)
			, transformer(transformer)
			, formatter(formatter)
		{

		}

		virtual const VirtualTransformer<ValueT> & getTransformer() const override { return *transformer; }
		virtual VirtualFormatter<ValueT> & getFormatter() override { return *formatter; }
		virtual ValueT getNormalizedValue() const override { return internalValue; }
		virtual void setNormalizedValue(ValueT value) override { internalValue = value; notifyListeners(); }

	private:
		double internalValue;
		Transformer * transformer;
		Formatter * formatter;
	};

	template<typename Transformer = LinearRange<ValueT>, typename Formatter = BasicFormatter<ValueT>>
	class CompleteValue : public SelfcontainedValue<Transformer, Formatter>
	{
	public:

		CompleteValue()
			: SelfcontainedValue<Transformer, Formatter>(&hostedTransformer, &hostedFormatter)
		{

		}

	private:
		Transformer hostedTransformer;
		Formatter hostedFormatter;
	};

	/// <summary>
	/// Note: Lifetime is expected to be tied to the parameter reference, for now.
	/// </summary>
	template<typename ParameterView>
	class ParameterValueWrapper : public DefaultValueListenerEntity, ParameterView::Listener
	{
	public:

		ParameterValueWrapper(ParameterView * parameterToRef = nullptr)
			: parameterView(nullptr)
		{
			setParameterReference(parameterToRef);
		}

		void setParameterReference(ParameterView * parameterReference)
		{
			if (parameterView != nullptr)
			{
				parameterView->removeListener(this);
			}
			parameterView = parameterReference;
			if (parameterView != nullptr)
			{
				parameterView->addListener(this);
			}
		}

		void parameterChangedUI(Parameters::Handle, Parameters::Handle, ParameterView * parameterThatChanged) override
		{
			if (parameterView != parameterThatChanged)
			{
				CPL_RUNTIME_EXCEPTION("Unknown parameter callback; corruption");
			}

			notifyListeners();
		}

		ParameterView & getParameterView() noexcept { return *parameterView; }
		const ParameterView & getParameterView() const noexcept { return *parameterView; }
		// : See summary of <this>
		//~ParameterValueWrapper()
		//{
		//	setParameterReference(nullptr);
		//}

		const VirtualTransformer<ValueT> & getTransformer() const override { return parameterView->getTransformer(); }
		VirtualFormatter<ValueT> & getFormatter() override { return parameterView->getFormatter(); }
		ValueT getNormalizedValue() const override { return parameterView->template getValueNormalized<ValueT>(); }
		void setNormalizedValue(ValueT value) override { parameterView->updateFromUINormalized(static_cast<typename ParameterView::ValueType>(value)); }
		void beginChangeGesture() override { return parameterView->beginChangeGesture(); }
		void endChangeGesture() override { return parameterView->endChangeGesture(); }
		std::string getContextualName() override { return parameterView->getExportedName(); }

	private:
		ParameterView * parameterView;
	};


	// TODO: specialize on std::is_base_of<FormattedParameter, UIParamerView::ParameterType>
	template<typename ParameterView>
	class ParameterValue
		: public ParameterValueWrapper<ParameterView>
	{
		typedef typename ParameterView::ValueType ValueType;
		typedef typename ParameterView::ParameterType ParameterType;
		typedef typename Parameters::BundleUpdate<ParameterView>::Record Entry;

		friend class Updater;

	public:

		ParameterValue(std::string name, typename ParameterType::Transformer & tsf, typename ParameterType::Formatter & formatter)
			: parameter(std::move(name), tsf, &formatter)
		{

		}

		Parameters::SingleUpdate<ParameterView> * generateUpdateRegistrator(bool isAutomatable = true, bool canChangeOthers = false)
		{
			// TODO: reference-count at least?
			return new Updater(this, isAutomatable, canChangeOthers);
		}

		ParameterType parameter;

	private:

		class Updater : public Parameters::SingleUpdate<ParameterView>
		{
		public:
			Updater(ParameterValue<ParameterView> * parentToRef, bool isAutomatable = true, bool canChangeOthers = false)
				: parent(parentToRef)
			{
				entry.parameter = &parent->parameter;
				entry.shouldBeAutomatable = isAutomatable;
				entry.canChangeOthers = canChangeOthers;
				entry.uiParameterView = nullptr;

				Parameters::SingleUpdate<ParameterView>::parameterQuery = &entry;
			}

		private:
			virtual void parametersInstalled() override
			{
				parent->setParameterReference(entry.uiParameterView);
				delete this;
			}
			ParameterValue<ParameterView> * parent;
			Entry entry;
		};

	};

};

#endif
