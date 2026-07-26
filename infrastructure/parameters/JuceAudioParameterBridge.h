#ifndef CPL_JUCEAUDIOPARAMETERBRIDGE_H
#define CPL_JUCEAUDIOPARAMETERBRIDGE_H

#include "ParameterSystem.h"
#include "../../Common.h" // Brings in JUCE

namespace cpl
{
	template<class T, typename InternalFrameworkType, typename BaseParameterT>
	class JuceAudioParameter : public juce::AudioProcessorParameter
	{
	public:

		typedef typename cpl::ParameterGroup<T, InternalFrameworkType, BaseParameterT>::ParameterView View;
		typedef typename cpl::ParameterGroup<T, InternalFrameworkType, BaseParameterT>::Formatter Formatter;
		typedef T ValueType;

		JuceAudioParameter(View& baseParameter, int versionHint)
			: juce::AudioProcessorParameter(versionHint)
			, view(baseParameter)
		{
		}

	private:

		View& view;

		float getValue() const override // Normalized
		{
			return view.template getValueNormalized<float>();
		}

		void setValue(float newValue) override
		{
			view.updateFromHostNormalized(static_cast<T>(newValue));
		}

		float getDefaultValue() const override
		{
			return 0;
		}

		juce::String getName(int maximumStringLength) const override
		{
			return view.getExportedName();
		}

		juce::String getLabel() const override
		{
			auto unit = view.getFormatter().getUnit();

			return juce::String { unit.data(), unit.size()};
		}

		int getNumSteps() const override
		{
			return view.getTransformer().getQuantization();
		}

		bool isDiscrete() const override
		{
			return getNumSteps() != -1;
		}

		bool isBoolean() const override
		{
			if (auto boolFormatter = dynamic_cast<BooleanFormatter<ValueType>*>(&view.getFormatter()))
			{
				return true;
			}

			return false;
		}

		juce::String getText(float normalisedValue, int /*maximumStringLength*/) const override
		{
			auto& formatter = view.getFormatter();
			auto& transformer = view.getTransformer();

			std::string output;

			FormattingFlags flags = FormattingFlags::defaultFlags & ~FormattingFlags::includeUnit;

			if (formatter.format(transformer.transform(static_cast<ValueType>(normalisedValue)), output, flags))
				return output;

			return "<error>";
		}

		float getValueForText(const String& text) const override
		{
			auto& formatter = view.getFormatter();
			auto& transformer = view.getTransformer();

			ValueType interpretedValue;
			if (formatter.interpret(text.toStdString(), interpretedValue))
			{
				return static_cast<float>(transformer.normalize(interpretedValue));
			}

			return -1;
		}

		bool isAutomatable() const override
		{
			return view.isParameterAutomated();
		}

		bool isMetaParameter() const override
		{
			return view.canParameterChangeOthers();
		}

		juce::AudioProcessorParameter::Category getCategory() const override
		{
			return juce::AudioProcessorParameter::Category::genericParameter;
		}
	};

	/// <summary>
	/// Exports every parameter in the group to the host, carrying each parameter's release cohort across as the
	/// juce::AudioProcessorParameter version hint. See ParameterGroup::setVersionCohort() for what a cohort means
	/// and why it must never be derived from the current program version.
	/// </summary>
	template<class T, typename InternalFrameworkType, typename BaseParameterT>
	inline void bridgeJuceAudioProcessorParameters(juce::AudioProcessor& processor, ParameterGroup<T, InternalFrameworkType, BaseParameterT>& group)
	{
		for (auto& param : group)
		{
			processor.addParameter(new JuceAudioParameter<T, InternalFrameworkType, BaseParameterT>(param, param.getVersionCohort()));
		}
	}
};

#endif
