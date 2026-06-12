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

	file:CSignalGeneratorWidget.cpp

		Implementation of CSignalGeneratorWidget.h

*************************************************************************************/

#include "CSignalGeneratorWidget.h"

namespace cpl
{
	CSignalGeneratorWidget::CSignalGeneratorWidget(SignalGeneratorValue * value, bool takeOwnership)
		: ValueControl<SignalGeneratorValue, CompleteSignalGeneratorValue>(this, value, takeOwnership)
		, ktype(&getValueReference().getValueIndex(SignalGeneratorValue::Index::Type), false)
		, kamplitude(&getValueReference().getValueIndex(SignalGeneratorValue::Index::Amplitude), false)
		, kdcOffset(&getValueReference().getValueIndex(SignalGeneratorValue::Index::DCOffset), false)
		, kperChannel(&getValueReference().getValueIndex(SignalGeneratorValue::Index::PerChannel), false)
	{
		enableTooltip(true);
		addAndMakeVisible(layout);
		initUI();
		bSetIsDefaultResettable(true);
	}


	void CSignalGeneratorWidget::onControlSerialization(CSerializer::Archiver & ar, Version version)
	{
		ar << ktype;
		ar << kamplitude;
		ar << kdcOffset;
		ar << kperChannel;
	}

	void CSignalGeneratorWidget::onControlDeserialization(CSerializer::Builder & ar, Version version)
	{
		ar >> ktype;
		ar >> kamplitude;
		ar >> kdcOffset;
		ar >> kperChannel;
	}

	void CSignalGeneratorWidget::initUI()
	{
		ktype.bSetTitle("Signal Generator");
		kamplitude.bSetTitle("Amplitude");
		kdcOffset.bSetTitle("DC offset");
		kperChannel.bSetTitle("Per-channel");

		kperChannel.setToggleable(true);

		ktype.bSetDescription("The type of signal to generate. Includes sine waves at standard frequencies, noise variants, and digital patterns for testing.");
		kamplitude.bSetDescription("The output amplitude of the generated signal, displayed in dB. Range from -180 dB to 12 dB.");
		kdcOffset.bSetDescription("A DC offset added to the signal output. Useful for testing bias and offset behavior.");
		kperChannel.bSetDescription("When enabled, each channel maintains independent phase and noise state. When disabled, all channels share the same signal.");

		bSetDescription("A widget for controlling SignalGenerator parameters, including signal type, amplitude, DC offset, and per-channel mode.");

		layout.addControl(&ktype, 0);
		layout.addControl(&kamplitude, 1);
		layout.addControl(&kdcOffset, 0);
		layout.addControl(&kperChannel, 1);

		auto size = layout.getSuggestedSize();
		setSize(size.first, size.second);

		ktype.bForceEvent();
		kamplitude.bForceEvent();
		kdcOffset.bForceEvent();
		kperChannel.bForceEvent();
	}

};
