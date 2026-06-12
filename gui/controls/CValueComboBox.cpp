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

	file:CValueComboBox.cpp

		Source code for CValueComboBox.h

*************************************************************************************/

#include "CValueComboBox.h"
#include "../../Mathext.h"

namespace cpl
{

	CValueComboBox::CValueComboBox(ValueEntityBase * valueToReferTo, bool takeOwnership)
		: ValueControl<ValueEntityBase, CompleteValue<LinearRange<ValueT>, BasicFormatter<ValueT>>>(this, valueToReferTo, takeOwnership)
	{
		int numValues = valueObject->getTransformer().getQuantization();
		if (numValues < 1)
			CPL_RUNTIME_EXCEPTION("Initializating a value combobox with a value with quantization < 1");
		std::vector<std::string> values(numValues);
		for (std::size_t i = 0; i < values.size(); ++i)
		{
			if (!valueObject->getFormatter().format(static_cast<ValueT>(i), values[i]))
				CPL_RUNTIME_EXCEPTION("Error formatting a value index");
		}

		bSetTitle(valueObject->getContextualName());

		setValues(std::move(values));

		initialize();
	}

	void CValueComboBox::initialize()
	{
		setSize(ControlSize::Rectangle.width, ControlSize::Rectangle.height);
		addAndMakeVisible(box);
		enableTooltip(true);
		onValueObjectChange(nullptr, valueObject.get());
		box.addListener(this);
		box.setRepaintsOnMouseActivity(true);
		//box.setSelectedId(1, dontSendNotification);
		bSetIsDefaultResettable(true);
	}

	bool CValueComboBox::queryResetOk()
	{
		return !box.isPopupActive();
	}

	void CValueComboBox::resized()
	{
		stringBounds = CRect(5, 0, getWidth(), std::min(20, getHeight() / 2));
		box.setBounds(0, stringBounds.getBottom(), getWidth(), getHeight() - stringBounds.getHeight());

	}
	void CValueComboBox::bSetTitle(std::string newTitle)
	{
		title = std::move(newTitle);
	}
	std::string CValueComboBox::bGetTitle() const
	{
		return title.toStdString();
	}

	void CValueComboBox::paint(juce::Graphics & g)
	{
		//g.setFont(systemFont.withHeight(TextSize::normalText)); EDIT_TYPE_NEWFONTS
		g.setFont(TextSize::normalText);
		g.setColour(cpl::GetColour(cpl::ColourEntry::ControlText));
		g.drawFittedText(title, stringBounds, juce::Justification::centredLeft, 1, 1);
	}

	int floatToInt2(iCtrlPrec_t inVal, int size)
	{
		inVal = cpl::Math::confineTo(inVal, 0.0, 1.0);
		if (1 > size)
			size = 1;
		return cpl::Math::round<int>(1 + inVal * (size - 1));
	}

	iCtrlPrec_t intToFloat2(int idx, int size)
	{
		if (2 > size)
			size = 2;
		idx = cpl::Math::confineTo(idx, 1, size);

		return iCtrlPrec_t(idx - 1) / (size - 1);
	}

	int CValueComboBox::getZeroBasedSelIndex() const
	{
		auto value = getZeroBasedSelIndex<int>();
		if (value < 0)
			value = 0;
		return value;
	}

	void CValueComboBox::setValues(std::vector<std::string> inputValues)
	{
		values = std::move(inputValues);
		auto currentIndex = box.getItemText(box.getSelectedItemIndex() - 1);
		box.clear(NotificationType::dontSendNotification);
		juce::StringArray arr;
		auto newIndex = -1; auto counter = 0;
		for (const auto & str : values)
		{
			counter++;
			if (currentIndex == juce::CharPointer_UTF8(str.c_str()))
				newIndex = counter;
			arr.add(str);
		}
		box.addItemList(arr, 1);
		if (newIndex != -1)
			box.setSelectedItemIndex(newIndex, NotificationType::dontSendNotification);
		//bSetInternal(bGetValue());
	}

	void CValueComboBox::onValueObjectChange(ValueEntityListener * sender, ValueEntityBase * value)
	{
		box.setSelectedId(floatToInt2(value->getNormalizedValue(), (int)values.size()), dontSendNotification);
	}

	void CValueComboBox::comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged)
	{
		valueObject->setNormalizedValue(intToFloat(box.getSelectedId(), (int)values.size()));
		baseControlValueChanged();
	}


	void CValueComboBox::baseControlValueChanged()
	{
		repaint();
		notifyListeners();
	}


	std::size_t CValueComboBox::indexOfValue(const std::string_view idx) const noexcept
	{
		for (std::size_t i = 0; i < values.size(); ++i)
		{
			if (values[i] == idx)
			{
				return i;
			}
		}
		return (std::size_t) - 1;
	}

	bool CValueComboBox::setEnabledStateFor(const std::string_view idx, bool toggle)
	{
		return setEnabledStateFor(indexOfValue(idx), toggle);
	}

	bool CValueComboBox::setEnabledStateFor(std::size_t entry, bool toggle)
	{
		if (entry < values.size() && entry != static_cast<std::size_t>(-1))
		{
			bool selectNew = box.getSelectedId() == int(entry + 1) && !toggle;
			box.setItemEnabled((int)entry + 1, !!toggle);

			// TODO: maybe make a more educated guess at some point.
			if (selectNew)
				bSetValue(0);
			return true;
		}
		return false;
	}
};
