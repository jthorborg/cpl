#ifndef CPL_WINDOWDESIGNVALUE_H
#define CPL_WINDOWDESIGNVALUE_H

#include "ValueBase.h"
#include "../../dsp/DSPWindows.h"
#include "../../Core.h"

namespace cpl
{

	class WindowDesignValue : public ValueGroup
	{
	public:

		enum Index
		{
			Type,
			Shape,
			Alpha,
			Beta
		};

		static constexpr double
			dbMin = 200,
			dbMax = -50,
			betaMin = -1.5,
			betaMax = 6;

		template<typename ValueType>
		struct WindowTypeFormatTransformer
			: public VirtualFormatter<ValueType>
			, public ChoiceTransformer<ValueType>
		{

			WindowTypeFormatTransformer()
			{
				this->setQuantization(enum_cast<int>(cpl::dsp::WindowTypes::End) - 1);
			}

			virtual bool format(const ValueType & val, std::string & buf) override
			{
				auto wtype = enum_cast<cpl::dsp::WindowTypes>(val);
				buf = dsp::Windows::stringFromEnum(wtype);
				return true;
			}

			virtual bool interpret(const string_ref buf, ValueType & val) override
			{
				val = enum_cast<ValueType>(cpl::dsp::Windows::enumFromString(buf)) / enum_cast<ValueType>(cpl::dsp::WindowTypes::End);
				return true;
			}
		};

		template<typename ValueType>
		struct WindowShapeFormatTransformer
			: public ChoiceFormatter<ValueType>
			, public ChoiceTransformer<ValueType>
		{

			WindowShapeFormatTransformer()
				: ChoiceFormatter<ValueType>(*static_cast<ChoiceTransformer<ValueType> *>(this))
			{
				ChoiceFormatter<ValueType>::setValues({"Symmetric", "Periodic", "DFT-even"});
			}

		};

		template<typename ValueType>
		struct AlphaFormatter : public BasicFormatter<ValueType>
		{
		public:

			virtual bool format(const ValueType & val, std::string & buf) override
			{
				char buffer[1000];
				if (sprintfs(buffer, u8"%d dB (%.1f\u03B1)", int(std::round(val)), val / 20) > 0)
				{
					buf = buffer;
					return true;
				}
				return false;
			}
		};

		/// <summary>
		/// Generates the window according to the user-specified settings in the GUI.
		/// Safe, deterministic and wait-free to call from any thread.
		/// OutVector is a random-access indexable container of T, sized N.
		/// 
		/// Returns the time-domain scaling coefficient for the window.
		/// </summary>
		template<typename T, class OutVector>
		T generateWindow(OutVector & w, std::size_t N)
		{
			auto ltype = getWindowType();
			auto lsym = getWindowShape();
			auto lalpha = (T)getAlpha();
			auto lbeta = (T)getBeta();

			dsp::windowFunction(ltype, w, N, lsym, lalpha, lbeta);

			return dsp::windowScale(ltype, w, N, lsym, lalpha, lbeta);
		}

		dsp::WindowTypes getWindowType()
		{
			auto & value = getValueIndex(Type);
			return static_cast<cpl::dsp::WindowTypes>((int)value.getTransformer().transform(value.getNormalizedValue()));
		}

		dsp::Windows::Shape getWindowShape()
		{
			auto & value = getValueIndex(Shape);
			return static_cast<cpl::dsp::Windows::Shape>((int)value.getTransformer().transform(value.getNormalizedValue()));
		}

		ValueT getAlpha()
		{
			auto & value = getValueIndex(Alpha);
			return value.getTransformer().transform(value.getNormalizedValue());
		}

		ValueT getBeta()
		{
			auto & value = getValueIndex(Beta);
			return value.getTransformer().transform(value.getNormalizedValue());
		}

		virtual std::size_t getNumValues() const noexcept override { return 4; }
	};



	class CompleteWindowDesign
		: public WindowDesignValue
		, public WindowDesignValue::AlphaFormatter<ValueT>
	{
	public:
		CompleteWindowDesign()
			: dbRange(WindowDesignValue::dbMin, WindowDesignValue::dbMax)
			, betaRange(WindowDesignValue::betaMin, WindowDesignValue::betaMax)
			, type(&typeSemantics, &typeSemantics)
			, symmetry(&shapeSemantics, &shapeSemantics)
			, alpha(&dbRange, this)
			, beta(&betaRange, &betaFormatter)
		{

		}

		ValueEntityBase & getValueIndex(std::size_t i) override { switch (i) { default: case 0: return type; case 1: return symmetry; case 2: return alpha; case 3: return beta; } }

	private:
		WindowDesignValue::WindowShapeFormatTransformer<ValueT> shapeSemantics;
		WindowDesignValue::WindowTypeFormatTransformer<ValueT> typeSemantics;

		LinearRange<ValueT> dbRange;
		LinearRange<ValueT> betaRange;
		BasicFormatter<ValueT> betaFormatter;
	public:
		SelfcontainedValue<> type, symmetry, alpha, beta;
	};


	template<typename ParameterView>
	class ParameterWindowDesignValue
		: public WindowDesignValue
		, public WindowDesignValue::AlphaFormatter<typename ParameterView::ValueType>
		, public Parameters::BundleUpdate<ParameterView>
	{
	public:

		typedef typename ParameterView::ValueType ValueType;
		typedef typename ParameterView::ParameterType ParameterType;
		typedef typename Parameters::BundleUpdate<ParameterView>::Record Entry;

		class SharedBehaviour
		{
		public:
			SharedBehaviour() : context("DW.") {}

			WindowDesignValue::WindowTypeFormatTransformer<ValueType> windowType;
			WindowDesignValue::WindowShapeFormatTransformer<ValueType> windowShape;
			const std::string & getContext() { return context; }
		private:
			std::string context;
		};

		ParameterWindowDesignValue(SharedBehaviour & b, std::string name = "")
			: dbRange(WindowDesignValue::dbMin, WindowDesignValue::dbMax)
			, betaRange(WindowDesignValue::betaMin, WindowDesignValue::betaMax)
			, type("Tp", b.windowType, &b.windowType)
			, symmetry("Shp", b.windowShape, &b.windowShape)
			, alpha("A", dbRange, this)
			, beta("B", betaRange, &betaFormatter)
			, contextName(name)
			, behaviour(b)
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
			parameters->push_back(Entry {&symmetry, true, false});
			parameters->push_back(Entry {&alpha, true, false});
			parameters->push_back(Entry {&beta, true, false});

		}

		ValueEntityBase & getValueIndex(std::size_t i) override
		{
			return values[i];
		}

		virtual std::string getContextualName() override { return contextName; }

	private:
		LinearRange<ValueT> dbRange;
		LinearRange<ValueT> betaRange;
		SharedBehaviour & behaviour;
		BasicFormatter<ValueT> betaFormatter;
		std::unique_ptr<std::vector<Entry>> parameters;
		std::string contextName;
	public:

		std::array<ParameterValueWrapper<ParameterView>, 4> values;
		ParameterType type, symmetry, alpha, beta;
	};
};

#endif
