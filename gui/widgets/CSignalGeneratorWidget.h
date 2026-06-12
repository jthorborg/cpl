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

	file:CSignalGeneratorWidget.h

		A widget for controlling SignalGenerator parameters.

*************************************************************************************/

#ifndef CPL_CSIGNALGENERATORWIDGET_H
#define CPL_CSIGNALGENERATORWIDGET_H

#include "WidgetBase.h"
#include "../../infrastructure/values/SignalGeneratorValue.h"

namespace cpl
{

	class CSignalGeneratorWidget
		: public juce::Component
		, public ValueControl<SignalGeneratorValue, CompleteSignalGeneratorValue>
	{

	public:

		CSignalGeneratorWidget(SignalGeneratorValue * value = nullptr, bool takeOwnership = false);

	protected:

		virtual void onControlSerialization(CSerializer::Archiver & ar, Version version) override;
		virtual void onControlDeserialization(CSerializer::Builder & ar, Version version) override;

		void initUI();

		CValueComboBox ktype;
		CValueKnobSlider kamplitude;
		CValueKnobSlider kdcOffset;
		CButton kperChannel;
		MatrixSection layout;
	};

};

#endif
