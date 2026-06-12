#ifndef CPL_SIGNALGENERATORVALUE_H
#define CPL_SIGNALGENERATORVALUE_H

#include "ValueBase.h"
#include "../../dsp/SignalGenerator.h"
#include "../../Mathext.h"
#include <cstring>

namespace cpl
{
	class SignalGeneratorValue : public ValueGroup
	{

	public:

		enum Index
		{
			Type,
			Amplitude,
			DCOffset,
			PerChannel
		};

		virtual std::size_t getNumValues() const noexcept override { return 4; }

		dsp::SignalGenerator::Type getType()
		{
			auto & value = getValueIndex(Type);
			return static_cast<dsp::SignalGenerator::Type>(
				static_cast<int>(value.getTransformer().transform(value.getNormalizedValue()))
			);
		}

		void setType(dsp::SignalGenerator::Type t)
		{
			getValueIndex(Type).setTransformedValue(static_cast<ValueT>(static_cast<int>(t)));
		}

		ValueT getAmplitude()
		{
			auto & value = getValueIndex(Amplitude);
			return value.getTransformer().transform(value.getNormalizedValue());
		}

		void setAmplitude(ValueT transformedValue)
		{
			getValueIndex(Amplitude).setTransformedValue(transformedValue);
		}

		ValueT getDCOffset()
		{
			auto & value = getValueIndex(DCOffset);
			return value.getTransformer().transform(value.getNormalizedValue());
		}

		void setDCOffset(ValueT transformedValue)
		{
			getValueIndex(DCOffset).setTransformedValue(transformedValue);
		}

		bool getPerChannel()
		{
			auto & value = getValueIndex(PerChannel);
			return value.getTransformer().transform(value.getNormalizedValue()) >= 0.5;
		}

		dsp::SignalGenerator::ProcessConfig deriveProcessingConfig()
		{
			dsp::SignalGenerator::ProcessConfig ret;
			ret.amplitude = getAmplitude();
			ret.dcOffset = getDCOffset();
			ret.perChannel = getPerChannel();
			ret.type = getType();

			return ret;
		}

	protected:

		template<typename ValueType>
		struct SignalGeneratorTypeSemantics
			: public VirtualFormatter<ValueType>
			, public ChoiceTransformer<ValueType>
		{
			SignalGeneratorTypeSemantics()
			{
				this->setQuantization(static_cast<int>(dsp::SignalGenerator::Type::Reserved20) + 1);
			}

			virtual bool format(const ValueType& val, std::string& buf, FormattingFlags flags) override
			{
				auto idx = static_cast<std::size_t>(std::round(val));
				auto type = static_cast<dsp::SignalGenerator::Type>(idx);
				buf = dsp::SignalGenerator::getNameForType(type);
				return true;
			}

			virtual bool interpret(const string_ref buf, ValueType& val) override
			{
				for (std::size_t i = 0; i <= static_cast<std::size_t>(dsp::SignalGenerator::Type::Reserved20); ++i)
				{
					if (buf == dsp::SignalGenerator::getNameForType(static_cast<dsp::SignalGenerator::Type>(i)))
					{
						val = static_cast<ValueType>(i);
						return true;
					}
				}
				return false;
			}
		};

		class SharedBehaviour
		{
		public:
			SharedBehaviour() 
				: context("SG.")
				, dcRange(-1, 1)
				, ampRange(cpl::Math::dbToFraction(-180.0), cpl::Math::dbToFraction(12.0))

			{
			}

			SignalGeneratorValue::SignalGeneratorTypeSemantics<ValueT> typeSemantics;
			LinearRange<ValueT> dcRange;
			ExponentialRange<ValueT> ampRange;
			BooleanRange<ValueT> boolRange;
			BooleanFormatter<ValueT> boolFormatter;
			BasicFormatter<ValueT> basicFormatter;
			DBFormatter<ValueT> dbFormatter;

			const std::string& getContext() { return context; }
		private:
			std::string context;
		};

		SharedBehaviour shared;
	};


	class CompleteSignalGeneratorValue
		: public SignalGeneratorValue
	{
	public:
		CompleteSignalGeneratorValue()
			: amplitude(&shared.ampRange, &shared.dbFormatter)
			, dcOffset(&shared.dcRange, &shared.basicFormatter)
			, perChannel(&shared.boolRange, &shared.boolFormatter)
			, type(&shared.typeSemantics, &shared.typeSemantics)
		{
			setDCOffset(0);
			setAmplitude(0.5);
		}

		ValueEntityBase & getValueIndex(std::size_t i) override { switch (i) { default: case 0: return type; case 1: return amplitude; case 2: return dcOffset; case 3: return perChannel; } }

	private:
	public:
		SelfcontainedValue<> type, amplitude, dcOffset, perChannel;
	};



	template<typename ParameterView>
	class ParameterSignalGeneratorValue
		: public SignalGeneratorValue
		, public Parameters::BundleUpdate<ParameterView>
	{
	public:

		typedef typename ParameterView::ValueType ValueType;
		typedef typename ParameterView::ParameterType ParameterType;
		typedef typename Parameters::BundleUpdate<ParameterView>::Record Entry;

		ParameterSignalGeneratorValue(std::string name = "")
			: amplitude("Amp", &shared.ampRange, &shared.dbFormatter)
			, dcOffset("DC ", &shared.dcRange, &shared.basicFormatter)
			, perChannel("PerCh", &shared.boolRange, &shared.boolFormatter)
			, type("Type", &shared.typeSemantics, &shared.typeSemantics)
			, contextName(std::move(name))
		{

		}

		virtual std::vector<Entry> & queryParameters() override
		{
			return *parameters.get();
		}

		virtual const std::string & getBundleContext() const noexcept override
		{
			return behaviour.getContext();
		}

		virtual void parametersInstalled() override
		{
			for (std::size_t i = 0; i < values.size(); ++i)
				values[i].setParameterReference(parameters->at(i).uiParameterView);

			parameters = nullptr;
		}

		void generateInfo() override
		{
			parameters = std::make_unique<std::vector<Entry>>();
			parameters->push_back(Entry {&type, true, false});
			parameters->push_back(Entry {&amplitude, true, false });
			parameters->push_back(Entry {&dcOffset, true, false });
			parameters->push_back(Entry {&perChannel, true, false});
		}

		ValueEntityBase & getValueIndex(std::size_t i) override
		{
			return values[i];
		}

		virtual std::string getContextualName() override { return contextName; }

	private:
		std::unique_ptr<std::vector<Entry>> parameters;
		std::string contextName;
	public:

		std::array<ParameterValueWrapper<ParameterView>, 4> values;
		ParameterType type, amplitude, dcOffset, perChannel;
	};
};

#endif
