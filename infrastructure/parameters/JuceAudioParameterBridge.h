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
			const auto quantization = view.getTransformer().getQuantization();

			// Transformers report a non-positive quantization to mean "continuous". That's an internal sentinel,
			// not a step count - hosts expect JUCE's default resolution for a continuous parameter instead.
			return quantization > 0 ? quantization : juce::AudioProcessor::getDefaultNumParameterSteps();
		}

		bool isDiscrete() const override
		{
			return view.getTransformer().getQuantization() > 0;
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
	/// Exports every parameter in the group as a juce::AudioProcessorParameterGroup, ready to hand to
	/// juce::AudioProcessor::addParameterGroup(). Each parameter's release cohort is carried across as the
	/// juce::AudioProcessorParameter version hint - see ParameterGroup::setVersionCohort() for what a cohort
	/// means, and why it must never be derived from the current program version.
	///
	/// Children are emitted in registration order, and juce flattens a parameter tree depth-first in child
	/// order, so the resulting flat parameter list - and with it every parameter index a host sees - is
	/// identical to adding the parameters one by one.
	/// </summary>
	template<class T, typename InternalFrameworkType, typename BaseParameterT>
	inline std::unique_ptr<juce::AudioProcessorParameterGroup> createJuceParameterGroup(ParameterGroup<T, InternalFrameworkType, BaseParameterT>& group)
	{
		// Group identifiers must avoid separators like "." and must not read as plain integers, otherwise they
		// can collide with the legacy index-based parameter identifiers.
		auto juceGroup = std::make_unique<juce::AudioProcessorParameterGroup>(group.getName(), group.getName(), "");

		for (auto& param : group)
		{
			juceGroup->addChild(
				std::make_unique<JuceAudioParameter<T, InternalFrameworkType, BaseParameterT>>(param, param.getVersionCohort())
			);
		}

		return juceGroup;
	}
};

#endif
