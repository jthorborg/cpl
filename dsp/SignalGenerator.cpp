/*************************************************************************************

	cpl - cross-platform library - v. 0.x.y

	Copyright (C) 2026 Janus Lynggaard Thorborg (www.jthorborg.com)

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

	See \licenses\  for additional details on licenses associated with this program.

**************************************************************************************

	file:SignalGenerator.cpp

		Non-templated implementation of SignalGenerator.

*************************************************************************************/

#include "SignalGenerator.h"

namespace cpl::dsp
{
	SignalGenerator::SignalGenerator()
		: numChannels_(0)
		, frameIndex(0)
		, sampleRate_(48000.0)
	{
	}

	const char* SignalGenerator::getNameForType(Type type)
	{
		switch (type)
		{
			case Type::None:							return "None";
			case Type::PinkNoise:						return "Pink Noise";
			case Type::BrownNoise:						return "Brown Noise";
			case Type::WhiteNoise:						return "White Noise";
			case Type::BlueNoise:						return "Blue Noise";
			case Type::VioletNoise:						return "Violet Noise";
			case Type::Reserved1:						return "";
			case Type::Reserved2:						return "";
			case Type::Sine1:							return "Sine 1 Hz";
			case Type::Sine10:							return "Sine 10 Hz";
			case Type::Sine100:							return "Sine 100 Hz";
			case Type::Sine440:							return "Sine 440 Hz";
			case Type::Sine1000:						return "Sine 1000 Hz";
			case Type::Sine10000:						return "Sine 10000 Hz";
			case Type::Sine20000:						return "Sine 20000 Hz";
			case Type::Sine30000:						return "Sine 30000 Hz";
			case Type::Reserved3:						return "";
			case Type::Reserved4:						return "";
			case Type::PerfectSquare200:				return "Perfect Square 200";
			case Type::PerfectSquare200Duty25:			return "Perfect Square 25%";
			case Type::PerfectSquare200Duty10:			return "Perfect Square 10%";
			case Type::PerfectTriangle200:				return "Perfect Triangle 200";
			case Type::PerfectSawtoothUp200:			return "Perfect Sawtooth 200";
			case Type::PerfectSawtoothDown200:			return "Perfect Sawtooth Down 200";
			case Type::NyquistOscillator:				return "Nyquist Oscillator";
			case Type::Reserved5:						return "";
			case Type::Reserved6:						return "";
			case Type::Dirac1:							return "Dirac 1s";
			case Type::Dirac4:							return "Dirac 1/4s";
			case Type::Dirac8:							return "Dirac 1/8s";
			case Type::Dirac32:							return "Dirac 1/32s";
			case Type::Reserved7:						return "";
			case Type::Reserved8:						return "";
			case Type::Reserved9:						return "";
			case Type::Reserved10:						return "";
			case Type::Reserved11:						return "";
			case Type::Reserved12:						return "";
			case Type::Reserved13:						return "";
			case Type::Reserved14:						return "";
			case Type::Reserved15:						return "";
			case Type::Reserved16:						return "";
			case Type::Reserved17:						return "";
			case Type::Reserved18:						return "";
			case Type::Reserved19:						return "";
			case Type::Reserved20:						return "";
			default:									return "Unknown";
		}
	}

	void SignalGenerator::reset(index_t numChannels, double sampleRate)
	{
		numChannels_ = numChannels;
		sampleRate_ = sampleRate;
		frameIndex = 0;

		phase = 0.0;

		pinkState.assign(numChannels, std::array<double, 5>{0.0});
		brownState.assign(numChannels, 0.0);
		bluePrevWhite.assign(numChannels, 0.0);
		violetPrevBlue.assign(numChannels, 0.0);
		rng.resize(numChannels);

		for (int ch = 0; ch < numChannels; ++ch)
			rng[ch].seed(static_cast<unsigned>(0x12345678u + static_cast<unsigned>(ch)));
	}
}
