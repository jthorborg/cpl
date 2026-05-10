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

	file:CDSPWindowWidget.cpp

		Source code for CDSPWindowWidget.h

*************************************************************************************/

#include "CDSPWindowWidget.h"
#include "../../ffts.h"

namespace cpl
{
	const double dbMin = 200;
	const double dbMax = -50;
	const double betaMin = -1.5;
	const double betaMax = 6;
	const auto spaceForAnalyzer = 128;
	const auto oversamplingFactor = 4;

	const double fftDBMin = -130;
	const double fftDBMax = 0;

	CDSPWindowWidget::WindowAnalyzer::WindowAnalyzer(CDSPWindowWidget & w) : p(w) { }

	void CDSPWindowWidget::WindowAnalyzer::paint(juce::Graphics & g)
	{
		auto const N = spaceForAnalyzer / oversamplingFactor;
		auto const ON = spaceForAnalyzer;
		alignas(32) std::array<double, N> window;
		alignas(32) std::array<std::complex<double>, ON> fftBuf;


		double wscale = p.getValueReference().generateWindow<double>(window, window.size()) / N;

		g.setColour(GetColour(ColourEntry::SelectedText).withMultipliedBrightness(0.8f));

		double top = 0;
		double bot = getHeight();

		double y1 = bot, minW = window[0], maxW = window[0];

		for (std::size_t n = 1; n < N; ++n)
		{
			auto const wN = window[n];
			minW = std::min(wN, minW);
			maxW = std::max(wN, maxW);
			fftBuf[n] = wN;
		}

		if (minW == maxW)
			minW = 0;
		// only scale bottom if it's out of range.
		else if (minW > 0)
			minW = 0;

		if (maxW == 0.0)
			maxW = 1.0;



		signaldust::DustFFT_fwdDa((double*)fftBuf.data(), ON);
		dsp::fftshift(fftBuf.data(), ON);


		double fftMin = std::pow(10, fftDBMin / 20.0);
		double fftMax = std::pow(10, fftDBMax / 20.0);


		y1 = std::abs(fftBuf[0]) * wscale;
		y1 = Math::UnityScale::Inv::exp(y1, fftMin, fftMax);

		// scale to graphics size
		y1 = Math::UnityScale::linear(y1, bot, top);

		if (std::fpclassify(y1) != FP_NORMAL)
		{
			y1 = bot;
		}

		for (std::size_t n = 1; n < ON; ++n)
		{
			// get magnitude and scale to unity
			double y2 = std::abs(fftBuf[n]) * wscale;
			// place in logarithmic domain
			y2 = Math::UnityScale::Inv::exp(y2, fftMin, fftMax);

			// scale to graphics size
			y2 = Math::UnityScale::linear(y2, bot - 0.5f, top + 0.5f);

			if (!std::isnormal(y2))
			{
				auto s = std::copysign(1.0, y2);
				if (fftBuf[n] == 0.0)
					y2 = bot;
				else if (s > 0 && n != ON / 2)
					y2 = bot;
				else
					y2 = top;
			}

			g.drawLine({(float)n - 1, (float)y1, float(n), (float)y2});

			y1 = y2;
		}

		g.setColour(GetColour(ColourEntry::ControlText));


		y1 = window[0];
		y1 = Math::UnityScale::Inv::linear(y1, minW, maxW);
		y1 = Math::UnityScale::linear(y1, bot, top);

		if (!std::isnormal(y1) && y1 != 0.0)
		{
			y1 = bot;
		}

		for (std::size_t n = 1; n < ON; ++n)
		{
			// interpolate window value
			double y2 = dsp::linearFilter<double>(window.data(), static_cast<Types::fsint_t>(window.size()), (double)n / oversamplingFactor);
			// normalize to [-1, 1]
			y2 = Math::UnityScale::Inv::linear(y2, minW, maxW);
			// scale to graphics size
			y2 = Math::UnityScale::linear(y2, bot - 0.5f, top + 0.5f);


			g.drawLine({(float)n - 1, (float)y1, float(n), (float)y2});

			y1 = y2;
		}


	}

	CDSPWindowWidget::CDSPWindowWidget(WindowDesignValue * value, bool takeOwnership)
		: ValueControl<WindowDesignValue, CompleteWindowDesign>(this, value, takeOwnership)
		, analyzer(*this)
		, kwindowList(&getValueReference().getValueIndex(WindowDesignValue::Index::Type), false)
		, ksymmetryList(&getValueReference().getValueIndex(WindowDesignValue::Index::Shape), false)
		, kalpha(&getValueReference().getValueIndex(WindowDesignValue::Index::Alpha), false)
		, kbeta(&getValueReference().getValueIndex(WindowDesignValue::Index::Beta), false)
	{
		initControls();
		enableTooltip();
		bSetIsDefaultResettable(true);
	}


	void CDSPWindowWidget::setWindowOptions(ChoiceOptions c)
	{
		if (c == ChoiceOptions::All)
		{
			// enable all windows
			for (std::size_t i = 0; i < (size_t)cpl::dsp::WindowTypes::End; i++)
			{
				kwindowList.setEnabledStateFor(i, true);
			}
		}
		else if (c == ChoiceOptions::FiniteDFTWindows)
		{
			// disable the windows unsupported by resonating algorithms
			for (std::size_t i = 0; i < (size_t)cpl::dsp::WindowTypes::End; i++)
			{
				if (cpl::dsp::windowHasFiniteDFT((cpl::dsp::WindowTypes)i))
					kwindowList.setEnabledStateFor(i, true);
				else
					kwindowList.setEnabledStateFor(i, false);
			}
		}

	}

	void CDSPWindowWidget::onControlSerialization(CSerializer::Archiver & ar, Version version)
	{
		ar << kalpha;
		ar << kbeta;
		ar << kwindowList;
		ar << ksymmetryList;
	}

	void CDSPWindowWidget::onControlDeserialization(CSerializer::Builder & ar, Version version)
	{
		ar >> kalpha;
		ar >> kbeta;
		ar >> kwindowList;
		ar >> ksymmetryList;
	}

	void CDSPWindowWidget::onValueObjectChange(ValueEntityListener * sender, ValueEntityBase * value)
	{
		analyzer.repaint();
	}

	void CDSPWindowWidget::resized()
	{
		analyzer.setBounds(layout.getRight(), 0, spaceForAnalyzer, getHeight());
	}

	void CDSPWindowWidget::initControls()
	{
		kwindowList.bSetTitle("Window function");
		ksymmetryList.bSetTitle("Symmetry");
		kalpha.bSetTitle(u8"\u03B1"); // greek small alpha
		kbeta.bSetTitle(u8"\u03B2"); // greek small beta

		kwindowList.bSetDescription("The window function describes a kernel applied to the input signal that alters the spectral leakage through controlling the ratio between main lobe width and side-lobes, including inherit patterns.");
		ksymmetryList.bSetDescription("The symmetry of a window function alters its frequency-domain representation. "
			"Symmetric (period: N-1) is often used for filtering, while periodic (period: N) is often used for spectral analysis, having a slight numerical advantage. "
			"DFT-Even is a special case for even-sized windows, mimicking the periodic design. For larger values of N, they all converge.");
		kalpha.bSetDescription("The alpha parameter generally controls the ratio between the main lobe and the side lobes, for windows that support it. A value of -100 dB sets the sidelobes to -100 dB, "
			"while positive values for alpha may alter the slope sign of the side lobes.");
		kbeta.bSetDescription("The beta parameter alters the shape of window. For gaussian windows, it specifics the sigma parameter; for ultraspherical windows, it specifies the slope coefficient of the sidelobes.");

		bSetDescription("The DSP window widget allows you to design a window function used in a process, to alter the shape and characteristics of the frequency-domain representation to your needs.");

		layout.addControl(&kwindowList, 0);
		layout.addControl(&ksymmetryList, 1);
		layout.addControl(&kalpha, 0);
		layout.addControl(&kbeta, 1);

		setSize(layout.getSuggestedSize().first + spaceForAnalyzer, layout.getSuggestedSize().second);

		addAndMakeVisible(analyzer);
		addAndMakeVisible(layout);

	}
};
