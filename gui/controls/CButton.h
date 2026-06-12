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

	file:CButton.h

		A simple button that shows some text. Has ability to be 'sticky'.

*************************************************************************************/

#ifndef CPL_CBUTTON_H
#define CPL_CBUTTON_H

#include "ControlBase.h"
#include "ValueControl.h"

namespace cpl
{

	class CButton
		: public juce::Button
		, public ValueControl<ValueEntityBase, CompleteValue<BooleanRange<ValueT>, BasicFormatter<ValueT>>>
	{
		juce::String texts[2];
		bool toggle;
		typedef ValueControl<ValueEntityBase, CompleteValue<BooleanRange<ValueT>, BasicFormatter<ValueT>>> Base;
	public:

		CButton(ValueEntityBase * valueToReferTo = nullptr, bool takeOwnership = false);
		~CButton();

		virtual void clicked() override;
		void setToggleable(bool isAble);
		void setSingleText(std::string input);
		void setTexts(std::string toggled, std::string untoggled);
		void setUntoggledText(std::string);
		void setToggledText(std::string);
		std::string bGetTitle() const override;
		void bSetTitle(std::string) override;

	private:
		void onValueObjectChange(ValueEntityListener * sender, ValueEntityBase * object) override;
		void paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override;

	};
};
#endif