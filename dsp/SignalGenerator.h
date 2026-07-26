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

	file:SignalGenerator.h

		Simple, standard utility for injecting test signals.

*************************************************************************************/

#ifndef CPL_SIGNALGENERATOR_H
	#define CPL_SIGNALGENERATOR_H

	#include <array>
	#include <random>
	#include <vector>
	#include <cpl/simd/simd_math.h>
	#include <cstdint>

namespace cpl::dsp
{
	class SignalGenerator
	{
	public:

		typedef std::uint32_t index_t;

		enum class Type
		{
			None,
			BrownNoise,
			PinkNoise,
			WhiteNoise,
			BlueNoise,
			VioletNoise,
			Reserved1,
			Reserved2,
			Sine1,
			Sine10,
			Sine100,
			Sine440,
			Sine1000,
			Sine10000,
			Sine20000,
			Sine30000,
			Reserved3,
			Reserved4,
			// +1 100 samples, -1 100 samples
			PerfectSquare200,
			// +1 50 samples, -1 150 samples
			PerfectSquare200Duty25,
			// +1 20 samples, -1 180 samples
			PerfectSquare200Duty10,
			// -1 to 1 over 100 samples, 1 to -1 over 100 samples
			PerfectTriangle200,
			// -1 to 1 over 200 samples
			PerfectSawtoothUp200,
			// 1 to -1 over 200 samples
			PerfectSawtoothDown200,
			// +/- 1 every sample
			NyquistOscillator,
			Reserved5,
			Reserved6,
			// +1 every 1/nth second
			Dirac1,
			Dirac4,
			Dirac8,
			Dirac32,
			Reserved7,
			Reserved8,
			Reserved9,
			Reserved10,
			Reserved11,
			Reserved12,
			Reserved13,
			Reserved14,
			Reserved15,
			Reserved16,
			Reserved17,
			Reserved18,
			Reserved19,
			Reserved20,
		};

		struct ProcessConfig
		{
			double amplitude = 1.0;
			double dcOffset = 0.0;
			Type type = Type::Sine1000;
			bool perChannel = true;
		};

		SignalGenerator();

		static const char* getNameForType(Type type);

		void reset(index_t numChannels, double sampleRate);

		template<typename T>
		void process(T** inout, index_t numFrames, const ProcessConfig& config);

	private:

		template<typename T>
		void processOscillators(T** inout, index_t numFrames, double freq, const ProcessConfig& config);

		template<typename T, class Kernel>
		void processNoiseKernel(T** inout, index_t numFrames, const ProcessConfig& config, Kernel&& kernel);

		template<typename T>
		void processSquareWave(T** inout, index_t numFrames, Type type, const ProcessConfig& config);

		template<typename T, Type WaveType>
		void processSweepedWave(T** inout, index_t numFrames, const ProcessConfig& config);

		index_t numChannels_;
		double phase;
		std::vector<std::array<double, 5>> pinkState;
		std::vector<double> brownState;
		std::vector<double> bluePrevWhite;
		std::vector<double> violetPrevBlue;
		index_t frameIndex;
		double sampleRate_;
		std::vector<std::mt19937> rng;
	};
}

#include "SignalGenerator.inl"

#endif
