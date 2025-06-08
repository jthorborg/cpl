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

	file:CComboBox.cpp

		Source code for CComboBox.h

*************************************************************************************/

#include "CComboBox.h"
#include "../../Mathext.h"

namespace cpl
{
	CComboBox::CComboBox(std::string name, const std::string_view inputValues)
		: CBaseControl(this), title(std::move(name)), internalValue(0.0), recursionFlag(false)
	{
		bToggleEditSpaces(true);
		setValues(inputValues);
		initialize();
	}

	CComboBox::CComboBox(std::string name, std::vector<std::string> inputValues)
		: CBaseControl(this), title(std::move(name)), values(std::move(inputValues)), internalValue(0.0), recursionFlag(false)
	{
		initialize();
	}


	CComboBox::CComboBox()
		: CBaseControl(this), internalValue(0.0), recursionFlag(false)
	{
		initialize();
	}

	void CComboBox::initialize()
	{
		setSize(ControlSize::Rectangle.width, ControlSize::Rectangle.height);
		addAndMakeVisible(box);
		enableTooltip(true);
		box.addListener(this);
		box.setRepaintsOnMouseActivity(true);
		//box.setSelectedId(1, dontSendNotification);
		bSetIsDefaultResettable(true);
	}

	bool CComboBox::queryResetOk()
	{
		return !box.isPopupActive();
	}

	void CComboBox::resized()
	{
		stringBounds = CRect(5, 0, getWidth(), std::min(20, getHeight() / 2));
		box.setBounds(0, stringBounds.getBottom(), getWidth(), getHeight() - stringBounds.getHeight());

	}
	void CComboBox::bSetTitle(std::string newTitle)
	{
		title = std::move(newTitle);
	}
	std::string CComboBox::bGetTitle() const
	{
		return title.toStdString();
	}


	void CComboBox::paint(juce::Graphics & g)
	{
		//g.setFont(systemFont.withHeight(TextSize::normalText)); EDIT_TYPE_NEWFONTS
		g.setFont(TextSize::normalText);
		g.setColour(cpl::GetColour(cpl::ColourEntry::ControlText));
		g.drawFittedText(title, stringBounds, juce::Justification::centredLeft, 1, 1);
	}


	void CComboBox::setValues(const std::string_view inputValues)
	{
		values.clear();
		std::size_t iter_pos(0);
		auto len = inputValues.size();
		for (std::size_t i = 0; i < len; i++)
		{
			if (inputValues[i] == '|')
			{
				values.emplace_back(inputValues.substr(iter_pos, i - iter_pos));
				iter_pos = i + 1;
			}
			else if (i == (len - 1))
			{
				values.emplace_back(inputValues.substr(iter_pos, i + 1 - iter_pos));

			}
		}
		setValues(values);
	}

	/*int floatToInt(iCtrlPrec_t inVal, int size)
	{
		if (1 >= size)
		{
			return 0;
		}
		else
		{
			return cpl::Math::round<int>(1 + inVal * size);
		}
	}

	iCtrlPrec_t intToFloat2(int idx, int size)
	{
		if (!idx)
			return 0;
		if (1 >= size)
		{
			return 0;
		}
		else
		{
			return iCtrlPrec_t(idx - 1) / (size - 1);
		}
	}*/

	int floatToInt(iCtrlPrec_t inVal, int size)
	{
		inVal = cpl::Math::confineTo(inVal, 0.0, 1.0);
		if (1 > size)
			size = 1;
		return cpl::Math::round<int>(1 + inVal * (size - 1));
	}

	iCtrlPrec_t intToFloat(int idx, int size)
	{
		if (2 > size)
			size = 2;
		idx = cpl::Math::confineTo(idx, 1, size);

		return iCtrlPrec_t(idx - 1) / (size - 1);
	}

	int CComboBox::getZeroBasedSelIndex() const
	{
		auto value = box.getSelectedId() - 1;
		if (value < 0)
			value = 0;
		return value;
	}

	void CComboBox::setValues(std::vector<std::string> inputValues)
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

	void CComboBox::bSetValue(iCtrlPrec_t val, bool sync)
	{
		box.setSelectedId(floatToInt(val, (int)values.size()),
			sync ? juce::NotificationType::sendNotificationSync : juce::NotificationType::sendNotification);
	}

	void CComboBox::bSetInternal(iCtrlPrec_t val)
	{
		box.setSelectedId(floatToInt(val, (int)values.size()), juce::dontSendNotification);
	}

	void CComboBox::comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged)
	{
		baseControlValueChanged();
	}


	void CComboBox::baseControlValueChanged()
	{
		notifyListeners();
	}

	iCtrlPrec_t CComboBox::bGetValue() const
	{
		return intToFloat(box.getSelectedId(), (int)values.size());
	}

	bool CComboBox::bValueToString(std::string & valueString, iCtrlPrec_t val) const
	{
		int idx = floatToInt(Math::confineTo<iCtrlPrec_t>(val, 0.0, 1.0), (int)values.size());
		valueString = values[(unsigned)idx - 1];
		return true;
	}


	bool CComboBox::bStringToValue(const string_ref valueString, iCtrlPrec_t & val) const
	{
		auto idx = indexOfValue(valueString);
		if (idx != static_cast<std::size_t>(-1))
		{
			// can never divide by zero here, since the for loop won't enter in that case
			//val = iCtrlPrec_t(i) / (values.size() > 1 ? values.size() - 1 : 1);
			val = intToFloat((int)idx + 1, (int)values.size());
			return true;
		}

		return false;
	}

	std::size_t CComboBox::indexOfValue(const std::string_view idx) const noexcept
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

	bool CComboBox::setEnabledStateFor(const std::string_view idx, bool toggle)
	{
		return setEnabledStateFor(indexOfValue(idx), toggle);
	}

	bool CComboBox::setEnabledStateFor(std::size_t entry, bool toggle)
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
