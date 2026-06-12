/*************************************************************************************

	cpl - cross-platform library - v. 0.1.0.

	Copyright (C) 2016 Janus Lynggaard Thorborg (www.jthorborg.com)

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.

	See \licenses\ for additional details on licenses associated with this program.

 **************************************************************************************

	file:CColourControl.cpp

		Source code for CColourControl.h

 *************************************************************************************/

#include "CColourControl.h"
#include "CKnobSliderEditor.h"
#include "../CCtrlEditSpace.h"

namespace cpl
{

	juce::Colour ColourFromPixelARGB(juce::PixelARGB pixel)
	{
		return juce::Colour(pixel.getRed(), pixel.getGreen(), pixel.getBlue(), pixel.getAlpha());
	}

	const char * ColourToneTypes[] =
	{
		"RGB",
		"ARGB",
		"GreyTone",
		"Red",
		"Green",
		"Blue"
	};

	class CustomColourSelector
		:
		public juce::ColourSelector
	{
	public:
		static constexpr int TheNormalFlags = showSliders | showAlphaChannel | showColourAtTop | showColourspace | editableColour;

		CustomColourSelector(int flags = TheNormalFlags,
			int edgeGap = 4,
			int gapAroundColourSpaceComponent = 7)
			: juce::ColourSelector(flags, edgeGap, gapAroundColourSpaceComponent)
		{
			acquireLabelReferences();
		}

		juce::Slider * getColourSlider(ColourValue::Index i) { return colourSliders[i]; }

		void acquireLabelReferences()
		{
			for (int i = 0; i < getNumChildComponents(); ++i)
			{
				auto currentChild = getChildComponent(i);
				if (juce::Slider * s = dynamic_cast<juce::Slider*>(currentChild))
				{
					colourSliders.push_back(s);
				}
			}
		}

		void shrinkLabels()
		{
			// default juce::Slider layouts have enourmous labels
			// compared to the actual slider.
			// for this widget, we only need to display two characters,
			// so we shrink them a little bit.

			const char * names[] = {"r", "g", "b", "a"};

			for (std::size_t i = 0; i < colourSliders.size(); ++i)
			{
				auto s = colourSliders[i];
				auto currentWidth = s->getTextBoxWidth();
				auto currentHeight = s->getTextBoxWidth();
				auto currentPos = s->getTextBoxPosition();
				s->setTextBoxStyle(currentPos, false, currentWidth / 3, currentHeight);
				s->setName(names[i]);
				s->setLookAndFeel(&cpl::CLookAndFeel_CPL::defaultLook());
			}
		}

		std::vector<juce::Slider *> colourSliders;
	};

	class ColourEditor
		: public CKnobSliderEditor
		, private juce::Slider::Listener
		, private ValueEntityBase::ValueEntityListener
	{

	public:

		ColourEditor(CColourControl * parentControl)
			: CKnobSliderEditor(parentControl), parent(parentControl), selector(CustomColourSelector::TheNormalFlags, 5, 5),
			recursionFlagWeChanged(false), recursionFlagTheyChanged(false)
		{
			oldHeight = fullHeight;
			oldWidth = fullWidth;
			fullWidth = oldWidth + extraWidth;
			fullHeight = oldHeight + extraHeight;
			selector.addChangeListener(this);

			selector.setCurrentColour(parent->getControlColour(), juce::NotificationType::dontSendNotification);
			toolTip = "Colour editor space - adjust ARGB values of controls precisely.";

			setOpaque(false);
			//addAndMakeVisible(selector);

			colourValue = &parentControl->getValueReference();
			for (int i = 0; i < 4; i++)
			{
				selector.getColourSlider((ColourValue::Index)i)->addListener(this);
				colourValue->getValueIndex((ColourValue::Index)i).addListener(this);
			}

			selector.addMouseListener(this, true);

		}

		~ColourEditor()
		{
			for (int i = 0; i < 4; i++)
			{
				selector.getColourSlider((ColourValue::Index)i)->removeListener(this);
				colourValue->getValueIndex((ColourValue::Index)i).removeListener(this);
			}
		}

		virtual void resized() override
		{
			selector.setBounds(1, oldHeight, fullWidth - elementHeight - 3, extraHeight);
			CKnobSliderEditor::resized();
		}

		virtual void valueEntityChanged(ValueEntityListener * sender, ValueEntityBase * object) override
		{
			auto currentColour = selector.getCurrentColour();
			auto red =
				currentColour.getRed(),
				green = currentColour.getGreen(),
				blue = currentColour.getBlue(),
				alpha = currentColour.getAlpha();

			auto value = object->getNormalizedValue();
			std::uint8_t componentValue = value == 1.0 ? 0xFF : static_cast<std::uint8_t>(value * 0x100);

			if (object == &colourValue->getValueIndex(ColourValue::R))
			{
				red = componentValue;
			}
			else if (object == &colourValue->getValueIndex(ColourValue::G))
			{
				green = componentValue;
			}
			else if (object == &colourValue->getValueIndex(ColourValue::B))
			{
				blue = componentValue;
			}
			else if (object == &colourValue->getValueIndex(ColourValue::A))
			{
				alpha = componentValue;
			}


			selector.setCurrentColour({red, green, blue, alpha}, juce::NotificationType::dontSendNotification);
		}

		virtual void sliderDragStarted(juce::Slider * s) override
		{
			for (int i = 0; i < 4; ++i)
			{
				auto sliderIndex = selector.getColourSlider((ColourValue::Index)i);
				if (s == sliderIndex)
				{
					parent->getValueReference().getValueIndex((ColourValue::Index)i).beginChangeGesture();
					isGesturingAnySlider = true;
					break;
				}
			}
		}

		virtual void sliderDragEnded(juce::Slider * s) override
		{
			for (int i = 0; i < 4; ++i)
			{
				auto sliderIndex = selector.getColourSlider((ColourValue::Index)i);
				if (s == sliderIndex)
				{
					parent->getValueReference().getValueIndex((ColourValue::Index)i).endChangeGesture();
					isGesturingAnySlider = false;
					break;
				}
			}
		}

		virtual void mouseDown(const juce::MouseEvent & me) override
		{
			// clicked somewhere random
			if (me.eventComponent == this || me.eventComponent == &selector)
			{
				userClickingNonSliderComponent = false;
				return;
			}


			if (selector.isParentOf(me.eventComponent))
			{
				for (int i = 0; i < 4; i++)
				{
					auto possibleParent = selector.getColourSlider((ColourValue::Index)i);
					if (me.eventComponent == possibleParent || selector.getColourSlider((ColourValue::Index)i)->isParentOf(me.eventComponent))
					{
						userClickingNonSliderComponent = false;
						return;
					}
				}
			}
			// it seems it was inside the component, not the component itself and not the sliders!
			userClickingNonSliderComponent = true;
		}

		virtual void mouseDrag(const juce::MouseEvent & me) override
		{
			userIsDragging = true;
		}

		virtual void mouseUp(const juce::MouseEvent & me) override
		{
			userClickingNonSliderComponent = false;
			userIsDragging = false;
			if (stopGesturingOnMouseup)
			{
				for (int i = 0; i < 4; ++i)
					parent->getValueReference().getValueIndex((ColourValue::Index)i).endChangeGesture();

				isGesturingAnySlider = false;
				stopGesturingOnMouseup = false;
			}
		}

		virtual void sliderValueChanged(juce::Slider * s) override
		{
			if (userClickingNonSliderComponent && userIsDragging)
			{
				if (!isGesturingAnySlider)
				{
					// since user is not gesturing any sliders, but still dragging something, we start gesturing all sliders.
					stopGesturingOnMouseup = true;

					for (int i = 0; i < 4; ++i)
					{
						parent->getValueReference().getValueIndex((ColourValue::Index)i).beginChangeGesture();
					}

					isGesturingAnySlider = true;
				}
			}

			for (int i = 0; i < 4; ++i)
			{
				if (userClickingNonSliderComponent && userIsDragging)
				{
					if (s == selector.getColourSlider((ColourValue::Index)i))
					{
						auto value = (s->getValue() - s->getMinimum()) / (s->getMaximum() - s->getMinimum());
						parent->getValueReference().getValueIndex((ColourValue::Index)i).setNormalizedValue(value);
						break;
					}
				}
				else
				{
					if (s == selector.getColourSlider((ColourValue::Index)i))
					{
						auto value = (s->getValue() - s->getMinimum()) / (s->getMaximum() - s->getMinimum());
						parent->getValueReference().getValueIndex((ColourValue::Index)i).setNormalizedValue(value);
						break;
					}
				}

			}


		}

		virtual void setMode(bool newMode) override
		{
			if (!newMode)
			{
				selector.setCurrentColour(parent->getControlColour(), juce::NotificationType::dontSendNotification);
				addAndMakeVisible(&selector);
				selector.shrinkLabels();
			}
			else
			{
				removeChildComponent(&selector);
			}
			CKnobSliderEditor::setMode(newMode);
		}

	private:
		bool stopGesturingOnMouseup = false;
		bool userClickingNonSliderComponent = false;
		bool userIsDragging = false;
		bool isGesturingAnySlider = false;
		static const int extraHeight = 180;
		static const int extraWidth = 10;
		bool recursionFlagWeChanged;
		bool recursionFlagTheyChanged;
		int oldHeight, oldWidth;
		ColourValue * colourValue;
		CColourControl * parent;
		CustomColourSelector selector;
	};

	std::unique_ptr<CCtrlEditSpace> CColourControl::bCreateEditSpace()
	{
		if (bGetEditSpacesAllowed())
			return std::unique_ptr<CCtrlEditSpace>(new ColourEditor(this));
		else
			return nullptr;
	}

	CColourControl::CColourControl(ColourValue * valueToReferTo, bool takeOwnership)
		: valueObject(nullptr)
	{
		bToggleEditSpaces(true);
		setValueReference(valueToReferTo, takeOwnership);
		valueEntityChanged(nullptr, nullptr);
	}

	void CColourControl::setValueReference(ColourValue * valueToReferTo, bool takeOwnership)
	{
		bool resetAlpha = false;
		if (valueObject != nullptr)
		{
			for (int i = 0; i < 4; ++i)
				valueObject->getValueIndex((cpl::ColourValue::Index)i).removeListener(this);

			valueObject.reset(nullptr);
		}

		if (valueToReferTo == nullptr)
		{
			resetAlpha = true;
			valueToReferTo = new CompleteColour();
			takeOwnership = true;
		}

		valueObject.reset(valueToReferTo);
		valueObject.get_deleter().doDelete = takeOwnership;

		for (int i = 0; i < 4; ++i)
			valueObject->getValueIndex((cpl::ColourValue::Index)i).addListener(this);

		if (resetAlpha)
			valueObject->getValueIndex(cpl::ColourValue::A).setNormalizedValue(1.0);
	}

	CColourControl::~CColourControl()
	{
		for (int i = 0; i < 4; ++i)
			valueObject->getValueIndex((cpl::ColourValue::Index)i).removeListener(this);
	}

	void CColourControl::onControlSerialization(CSerializer::Archiver & ar, Version version)
	{
		CKnobSlider::onControlSerialization(ar, version);
		auto colour = valueObject->getAsJuceColour();
		std::uint8_t a = colour.getAlpha(), r = colour.getRed(), g = colour.getGreen(), b = colour.getBlue();
		ar << a; ar << r; ar << g; ar << b;
		ar << Serialization::Reserve(4);
	}
	void CColourControl::onControlDeserialization(CSerializer::Builder & ar, Version version)
	{
		CKnobSlider::onControlDeserialization(ar, version);
		std::uint8_t a, r, g, b;
		ar >> a; ar >> r; ar >> g; ar >> b;
		ar >> Serialization::Consume(4);
		if (ar.getModifier(CSerializer::Modifiers::RestoreValue))
			setControlColour(juce::Colour(r, g, b, a));
	}

	void CColourControl::baseControlValueChanged()
	{
		notifyListeners();
		repaint();
	}

	void CColourControl::valueEntityChanged(ValueEntityListener * sender, ValueEntityBase * value)
	{
		auto colourValue = valueObject->getAsJuceColour();
		auto sliderValue = colourValue.getARGB() / double(0xFFFFFFFF);
		getSlider().setValue(sliderValue, juce::NotificationType::dontSendNotification);
		baseControlValueChanged();
	}

	juce::Colour CColourControl::getControlColour()
	{
		return valueObject->getAsJuceColour();
	}

	juce::Colour CColourControl::getControlColourAsColour()
	{
		return getControlColour();
	}

	void CColourControl::setControlColour(juce::Colour newColour)
	{
		valueObject->setFromJuceColour(newColour);
	}

	bool CColourControl::bStringToValue(const string_ref valueString, iCtrlPrec_t & value) const
	{
		std::uint32_t result(0);
		char * endPtr = nullptr;
		result = static_cast<std::uint32_t>(strtoul(valueString.c_str(), &endPtr, 0));
		if (endPtr > valueString.c_str())
		{
			value = result / (double)0xFFFFFFFF;
			return true;
		}
		return false;
	}

	iCtrlPrec_t CColourControl::bGetValue() const
	{
		return valueObject->getAsJuceColour().getARGB() / (double)0xFFFFFFFF;
	}

	void CColourControl::bSetValue(iCtrlPrec_t val, bool sync)
	{
		setControlColour((juce::Colour)static_cast<std::uint32_t>(val >= 1.0 ? 0xFFFFFFFF : val * 0x100000000ul));
	}

	bool CColourControl::bValueToString(std::string & valueString, iCtrlPrec_t value) const
	{
		char text[30];
		sprintfs(text, "0x%.8X", static_cast<std::uint32_t>(value >= 1.0 ? 0xFFFFFFFF : value * 0x100000000ul));
		valueString = text;
		return true;
	}

	void CColourControl::paint(juce::Graphics & g)
	{
		CKnobSlider::paint(g);
		g.setColour(valueObject->getAsJuceColour());
		auto b = getTextRect().toFloat();

		g.fillRoundedRectangle(b.withTrimmedRight(5).withTrimmedBottom(2), 5.f);
	}

	void CColourControl::valueChanged()
	{
		bSetValue(Slider::getValue());
	}

	void CColourControl::startedDragging()
	{
		for (int i = 0; i < 4; ++i)
			valueObject->getValueIndex((ColourValue::Index)i).beginChangeGesture();
	}

	void CColourControl::stoppedDragging()
	{
		for (int i = 0; i < 4; ++i)
			valueObject->getValueIndex((ColourValue::Index)i).endChangeGesture();
	}

};
