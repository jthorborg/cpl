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

	file:CKnobSlider.cpp

		Source code for CKnobSlider.h

*************************************************************************************/

#include "CKnobSlider.h"
#include "CKnobSliderEditor.h"
#include "../../simd/simd_consts.h"

namespace cpl
{

	std::unique_ptr<CCtrlEditSpace> CKnobSlider::bCreateEditSpace()
	{
		if (bGetEditSpacesAllowed())
		{
			return std::unique_ptr<CCtrlEditSpace>(new CKnobSliderEditor(this));
		}
		else
			return nullptr;
	}

	CKnobSlider::CKnobSlider()
		: juce::Slider("CKnobSlider")
		, CBaseControl(this)
		, oldStyle(Slider::SliderStyle::RotaryVerticalDrag)
		, laggedValue(0)
	{
		// IF you change the range, please change the scaling back to what is found in the comments.
		setRange(0.0, 1.0);
		bToggleEditSpaces(true);
		setTextBoxStyle(NoTextBox, 0, 0, 0);
		setIsKnob(true);
		enableTooltip(true);
		setVisible(true);
		setSliderStyle(Slider::SliderStyle::RotaryVerticalDrag);
		setPopupMenuEnabled(true);
		bSetIsDefaultResettable(true);
	}



	bool CKnobSlider::bStringToValue(const string_ref valueString, iCtrlPrec_t & val) const
	{
		return cpl::lexicalConversion(valueString, val);
	}
	bool CKnobSlider::bValueToString(std::string & valueString, iCtrlPrec_t val) const
	{
		return cpl::lexicalConversion(val, valueString);
	}

	juce::Rectangle<int> CKnobSlider::getTextRect() const
	{
		if (getHeight() >= ControlSize::Square.height)
		{
			return CRect(0, static_cast<int>(getHeight() * 0.75), getWidth(), static_cast<int>(getHeight() * 0.25));
		}
		else
		{
			auto sideKnobLength = ControlSize::Square.height / 2;
			return CRect(sideKnobLength + 5, getHeight() / 2, getWidth() - (sideKnobLength + 5), getHeight() / 2);
		}
	}

	juce::Rectangle<int> CKnobSlider::getTitleRect() const
	{
		if (getHeight() >= ControlSize::Square.height)
		{
			return CRect(getWidth(), static_cast<int>(getHeight() * 0.25));
		}
		else
		{
			auto sideKnobLength = ControlSize::Square.height / 2;
			return CRect(sideKnobLength + 5, 0, getWidth() - (sideKnobLength + 5), getHeight() / 2);
		}
	}

	void CKnobSlider::computePaths()
	{
		const float radius = jmin(getWidth() / 2, getHeight() / 2) - 1.0f;
		const float centreX = getHeight() * 0.5f;
		const float centreY = centreX;
		const float rx = centreX - radius;
		const float ry = centreY - radius;
		const float rw = radius * 2.0f;
		const float rotaryStartAngle = 2 * simd::consts<float>::pi * -0.4f;
		const float rotaryEndAngle = 2 * simd::consts<float>::pi * 0.4f;

		auto normalizedValue = std::clamp(bGetValue(), 0.0, 1.0);

		const float angle = static_cast<float>(
			normalizedValue * (rotaryEndAngle - rotaryStartAngle) + rotaryStartAngle
			);
		const float thickness = 0.7f;

		// pie fill
		pie.clear();
		pie.addPieSegment(rx, ry, rw, rw, rotaryStartAngle, angle, thickness * 0.9f);

		// pointer
		const float innerRadius = radius * 0.2f;
		const float pointerHeight = innerRadius * thickness;
		const float pointerLengthScale = 0.5f;

		pointer.clear();

		pointer.addRectangle(
			-pointerHeight * 0.5f + 2.f + (1 - pointerLengthScale) * radius * thickness,
			-pointerHeight * 0.5f,
			pointerLengthScale * radius * thickness,
			innerRadius * thickness);
		auto trans = AffineTransform::rotation(angle - simd::consts<float>::pi * 0.5f).translated(centreX, centreY);
		pointer.applyTransform(trans);

	}

	void CKnobSlider::paint(juce::Graphics& g)
	{
		auto newValue = bGetValue();

		if (isKnob)
		{
			if (newValue != laggedValue)
				computePaths();

			// code based on the v2 juce knob

			const bool isMouseOver = isMouseOverOrDragging() && isEnabled();
			const float thickness = 0.7f;

			// main fill
			g.setColour(GetColour(ColourEntry::Deactivated));
			g.fillEllipse(juce::Rectangle<int>(getHeight(), getHeight()).toFloat());

			// center fill
			g.setColour(GetColour(ColourEntry::Separator));
			g.fillEllipse(juce::Rectangle<int>(getHeight(), getHeight()).toFloat().reduced(getHeight() * (1 - thickness * 1.1f)));

			g.setColour(GetColour(ColourEntry::SelectedText)
				.withMultipliedBrightness(isMouseOver ? 0.8f : 0.7f));

			g.fillPath(pie);

			g.setColour(GetColour(ColourEntry::ControlText)
				.withMultipliedBrightness(isMouseOver ? 1.0f : 0.8f));

			g.fillPath(pointer);

			g.setFont(TextSize::smallerText);
			g.setColour(cpl::GetColour(cpl::ColourEntry::ControlText));

			// text
			g.drawText(bGetTitle(), getTitleRect(), juce::Justification::centredLeft, false);
			g.drawText(bGetText(), getTextRect(), juce::Justification::centredLeft, false);
		}
		else
		{

			g.fillAll(cpl::GetColour(cpl::ColourEntry::Activated).darker(0.6f));


			auto bounds = getBounds();
			auto rem = CRect(1, 1, bounds.getWidth() - 2, bounds.getHeight() - 2);
			g.setColour(cpl::GetColour(cpl::ColourEntry::Activated).darker(0.1f));
			g.fillRect(rem.withLeft(cpl::Math::round<int>(rem.getX() + rem.getWidth() * bGetValue())));

			g.setColour(cpl::GetColour(cpl::ColourEntry::Separator));
			//g.drawRect(getBounds().toFloat());
			g.setFont(TextSize::largeText);
			//g.setFont(systemFont.withHeight(TextSize::largeText)); EDIT_TYPE_NEWFONTS
			g.setColour(cpl::GetColour(cpl::ColourEntry::AuxillaryText));
			if (isMouseOverOrDragging())
			{
				g.drawText(bGetText(), getBounds().withPosition(5, 0), juce::Justification::centredLeft);
			}
			else
			{
				g.drawText(bGetTitle(), getBounds().withPosition(5, 0), juce::Justification::centredLeft);
			}

		}

		laggedValue = bGetValue();
	}

	iCtrlPrec_t CKnobSlider::bGetValue() const
	{
		return static_cast<iCtrlPrec_t>(getValue());
	}

	void CKnobSlider::onControlSerialization(CSerializer::Archiver & ar, Version version)
	{
		ar << bGetValue();
		ar << isKnob;
		ar << getVelocityBasedMode();
		ar << getMouseDragSensitivity();
		ar << getSliderStyle();
	}

	void CKnobSlider::onControlDeserialization(CSerializer::Builder & ar, Version version)
	{
		iCtrlPrec_t value(0);
		bool vel(false);
		int sens(0);
		Slider::SliderStyle style(Slider::SliderStyle::RotaryVerticalDrag);

		ar >> value;
		ar >> isKnob;
		ar >> vel;
		ar >> sens;
		ar >> style;

		if (ar.getModifier(CSerializer::Modifiers::RestoreSettings))
		{
			setIsKnob(isKnob);
			setVelocityBasedMode(vel);
			setMouseDragSensitivity(sens);
			setSliderStyle(style);
		}

		if (ar.getModifier(CSerializer::Modifiers::RestoreValue))
		{
			bSetValue(value, true);
		}

	}

	void CKnobSlider::bSetText(std::string in)
	{
		text = std::move(in);
	}
	void CKnobSlider::bSetInternal(iCtrlPrec_t newValue)
	{
		setValue(newValue, dontSendNotification);
	}

	void CKnobSlider::bSetTitle(std::string in)
	{
		title = std::move(in);
	}

	void CKnobSlider::bSetValue(iCtrlPrec_t newValue, bool sync)
	{
		setValue(newValue, sync ? sendNotificationSync : sendNotification);
	}

	void CKnobSlider::bRedraw()
	{
		bFormatValue(text, bGetValue());
		bSetText(text);
		repaint();
	}

	void CKnobSlider::setIsKnob(bool shouldBeKnob)
	{
		setSize(ControlSize::Rectangle.width, ControlSize::Rectangle.height);

		if (shouldBeKnob && !isKnob)
		{
			setSliderStyle(oldStyle);
		}
		else
		{
			oldStyle = getSliderStyle();
			setSliderStyle(Slider::SliderStyle::LinearHorizontal);
		}

		isKnob = !!shouldBeKnob;
	}

	bool CKnobSlider::getIsKnob() const
	{
		return isKnob;
	}

	std::string CKnobSlider::bGetTitle() const
	{
		return title;
	}

	std::string CKnobSlider::bGetText() const
	{
		return text;
	}

	void CKnobSlider::valueChanged()
	{
		// called whenever the slider changes.
		baseControlValueChanged();
	}

	void CKnobSlider::baseControlValueChanged()
	{
		bFormatValue(text, bGetValue());
		bSetText(text);
		notifyListeners();
		repaint();
	}

};
