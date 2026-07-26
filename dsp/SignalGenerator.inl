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

	file:SignalGenerator.inl

		Templated implementation of SignalGenerator.

*************************************************************************************/

#ifndef CPL_SIGNALGENERATOR_INL
	#define CPL_SIGNALGENERATOR_INL

	#include "SignalGenerator.h"
	#include <cmath>
	#include <cpl/Mathext.h>
	#include <cpl/simd/simd_math.h>

	namespace cpl::dsp
	{
		inline double getFrequencyForType(SignalGenerator::Type type)
		{
			switch (type)
			{
				case SignalGenerator::Type::Sine1:		return 1.0;
				case SignalGenerator::Type::Sine10:		return 10.0;
				case SignalGenerator::Type::Sine100:	return 100.0;
				case SignalGenerator::Type::Sine440:	return 440.0;
				case SignalGenerator::Type::Sine1000:	return 1000.0;
				case SignalGenerator::Type::Sine10000:	return 10000.0;
				case SignalGenerator::Type::Sine20000:	return 20000.0;
				case SignalGenerator::Type::Sine30000:	return 30000.0;
				default:								return 0.0;
			}
		}

		template<typename T>
		void SignalGenerator::process(T** inout, index_t numFrames, const ProcessConfig& config)
		{
			CPL_PROFILE("SignalGenerator::Process");

			const index_t numChannels = numChannels_;

			switch (config.type)
			{
				case Type::None:
					return;

				case Type::BrownNoise:
					processNoiseKernel(
						inout,
						numFrames,
						config,
						[this](double white, index_t ch)
						{
							auto& state = brownState[ch];
							state += white * 0.2;
							state *= 0.995;
							// Allow the brownian noise to walk around a bit, but not too much
							state = std::clamp(state, -10.0, 10.0);

							return state * 0.1;
						}
					);
					return;
				
				case Type::PinkNoise:
					processNoiseKernel(
						inout, 
						numFrames, 
						config,
						[this](double white, index_t ch)
						{
							double output = 0.0;
							pinkState[ch][0] = 0.99886 * pinkState[ch][0] + white * 0.0555179;
							output += pinkState[ch][0] * 0.0793798;
							pinkState[ch][1] = 0.99332 * pinkState[ch][1] + white * 0.0294507;
							output += pinkState[ch][1] * 0.1807693;
							pinkState[ch][2] = 0.96900 * pinkState[ch][2] + white * 0.0153031;
							output += pinkState[ch][2] * 0.3013481;
							pinkState[ch][3] = 0.86650 * pinkState[ch][3] + white * 0.0310432;
							output += pinkState[ch][3] * 0.4429402;
							pinkState[ch][4] = 0.55000 * pinkState[ch][4] + white * 0.0782301;
							output += pinkState[ch][4] * 0.7027619;

							return output;
						}
					);
					return;

				case Type::WhiteNoise:
					processNoiseKernel(
						inout,
						numFrames,
						config,
						[this](double white, index_t ch)
						{
							return white;
						}
					);					
					return;

				case Type::BlueNoise:
					processNoiseKernel(
						inout,
						numFrames,
						config,
						[this](double white, index_t ch)
						{
							const double blue = (white - bluePrevWhite[ch]) * 0.5;
							bluePrevWhite[ch] = white;

							return blue;
						}
					);
					return;

				case Type::VioletNoise:
					processNoiseKernel(
						inout,
						numFrames,
						config,
						[this](double white, index_t ch)
						{
							const double blue = (white - bluePrevWhite[ch]) * 0.5;
							bluePrevWhite[ch] = white;

							const double violet = (blue - violetPrevBlue[ch]) * 0.5;
							violetPrevBlue[ch] = blue;

							return violet;
						}
					);
					return;

				case Type::Sine1:
				case Type::Sine10:
				case Type::Sine100:
				case Type::Sine440:
				case Type::Sine1000:
				case Type::Sine10000:
				case Type::Sine20000:
				case Type::Sine30000:
				{
					const double freq = getFrequencyForType(config.type);
					processOscillators(inout, numFrames, freq, config);
					return;
				}

				case Type::PerfectTriangle200:
					processSweepedWave<T, Type::PerfectTriangle200>(inout, numFrames, config);
					return;

				case Type::PerfectSawtoothUp200:
					processSweepedWave<T, Type::PerfectSawtoothUp200>(inout, numFrames, config);
					return;

				case Type::PerfectSawtoothDown200:
					processSweepedWave<T, Type::PerfectSawtoothDown200>(inout, numFrames, config);
					return;

				case Type::PerfectSquare200:
				case Type::PerfectSquare200Duty25:
				case Type::PerfectSquare200Duty10:
				case Type::NyquistOscillator:
				case Type::Dirac1:
				case Type::Dirac4:
				case Type::Dirac8:
				case Type::Dirac32:
					processSquareWave(inout, numFrames, config.type, config);
					return;

				default:
					return;
			}
		}

		template<typename T>
		void SignalGenerator::processOscillators(T** inout, index_t numFrames, double freq, const ProcessConfig& config)
		{
			if (freq <= 0.0 || numChannels_ == 0)
				return;

			const auto tau = cpl::simd::consts<double>::tau;
			const double phaseInc = freq * tau / sampleRate_;

			if (config.perChannel)
			{
				for (index_t frame = 0; frame < numFrames; ++frame)
				{
					for (index_t ch = 0; ch < numChannels_; ++ch)
					{
						const double out = std::sin(phase[ch]) * config.amplitude + config.dcOffset;
						inout[ch][frame] = static_cast<T>(out);
						phase[ch] += phaseInc;
						if (phase[ch] >= tau) phase[ch] -= tau;
					}
				}
			}
			else
			{
				for (index_t frame = 0; frame < numFrames; ++frame)
				{
					const double out = std::sin(phase[0]) * config.amplitude + config.dcOffset;
					for (index_t ch = 0; ch < numChannels_; ++ch)
						inout[ch][frame] = static_cast<T>(out);
					phase[0] += phaseInc;
					if (phase[0] >= tau) phase[0] -= tau;
				}
			}
		}

		template<typename T, class Kernel>
		void SignalGenerator::processNoiseKernel(T** inout, index_t numFrames, const ProcessConfig& config, Kernel&& kernel)
		{
			const index_t numChannels = numChannels_;

			const double amplitude = config.amplitude;
			const double dcOffset = config.dcOffset;

			if (config.perChannel)
			{
				for (index_t ch = 0; ch < numChannels; ++ch)
				{
					std::uniform_real_distribution<double> dist(-1.0, 1.0);
					for (index_t frame = 0; frame < numFrames; ++frame)
					{
						const double white = dist(rng[ch]);
						const double out = kernel(white, ch);
						inout[ch][frame] = static_cast<T>(out * config.amplitude + config.dcOffset);
					}
				}
			}
			else
			{
				std::uniform_real_distribution<double> dist(-1.0, 1.0);
				for (index_t frame = 0; frame < numFrames; ++frame)
				{
					const double white = dist(rng[0]);

					const double out = kernel(white, 0);
					for (index_t ch = 0; ch < numChannels; ++ch)
						inout[ch][frame] = static_cast<T>(out * config.amplitude + config.dcOffset);
				}
			}
		}

		struct PhasedWaveConfig
		{
			SignalGenerator::index_t period;
			SignalGenerator::index_t phase0Length;
			double value0;
			double value1;
		};

		inline PhasedWaveConfig getPhasedWaveConfig(SignalGenerator::Type type, double sampleRate)
		{
			switch (type)
			{
			case SignalGenerator::Type::PerfectSquare200:
				return { 200, 100, 1.0, -1.0 };

			case SignalGenerator::Type::PerfectSquare200Duty25:
				return { 200, 50, 1.0, -1.0 };

			case SignalGenerator::Type::PerfectSquare200Duty10:
				return { 200, 20, 1.0, -1.0 };

			case SignalGenerator::Type::NyquistOscillator:
				return { 2, 1, 1.0, -1.0 };

			case SignalGenerator::Type::Dirac1:
			{
				auto period = static_cast<SignalGenerator::index_t>(std::ceil(sampleRate / 1.0));
				return { period, 1, 1.0, 0.0 };
			}

			case SignalGenerator::Type::Dirac4:
			{
				auto period = static_cast<SignalGenerator::index_t>(std::ceil(sampleRate / 4.0));
				return { period, 1, 1.0, 0.0 };
			}

			case SignalGenerator::Type::Dirac8:
			{
				auto period = static_cast<SignalGenerator::index_t>(std::ceil(sampleRate / 8.0));
				return { period, 1, 1.0, 0.0 };
			}

			case SignalGenerator::Type::Dirac32:
			{
				auto period = static_cast<SignalGenerator::index_t>(std::ceil(sampleRate / 32.0));
				return { period, 1, 1.0, 0.0 };
			}

			default:
				return { 1, 1, 0.0, 0.0 };
			}
		}

		template<typename T>
		void SignalGenerator::processSquareWave(T** inout, index_t numFrames, Type type, const ProcessConfig& config)
		{
			const index_t numChannels = numChannels_;
			const auto wave = getPhasedWaveConfig(type, sampleRate_);

			if (config.perChannel)
			{
				for (index_t frame = 0; frame < numFrames; ++frame)
				{
					for (index_t ch = 0; ch < numChannels; ++ch)
					{
						const index_t pos = (frameIndex + ch * wave.period / 4) % wave.period;
						const double sample = pos < wave.phase0Length ? wave.value0 : wave.value1;
						inout[ch][frame] = static_cast<T>(sample * config.amplitude + config.dcOffset);
					}
					++frameIndex;
				}
			}
			else
			{
				for (index_t frame = 0; frame < numFrames; ++frame)
				{
					const index_t pos = frameIndex % wave.period;
					const double sample = pos < wave.phase0Length ? wave.value0 : wave.value1;
					++frameIndex;

					const double out = sample * config.amplitude + config.dcOffset;
					for (index_t ch = 0; ch < numChannels; ++ch)
						inout[ch][frame] = static_cast<T>(out);
				}
			}
		}

		struct LinearSweepPhase
		{
			static constexpr SignalGenerator::index_t totalPeriod = 200;
			SignalGenerator::index_t period;
			double startValue;
			double endValue;
		};

		template<SignalGenerator::Type>
		struct LinearSweepConfig;

		template<>
		struct LinearSweepConfig<SignalGenerator::Type::PerfectTriangle200>
		{
			static constexpr std::array<LinearSweepPhase, 2> phases{ {
				{ LinearSweepPhase::totalPeriod / 2, -1.0, 1.0 },
				{ LinearSweepPhase::totalPeriod / 2, 1.0, -1.0 }
			} };
		};

		template<>
		struct LinearSweepConfig<SignalGenerator::Type::PerfectSawtoothUp200>
		{
			static constexpr std::array<LinearSweepPhase, 2> phases{ {
				{ LinearSweepPhase::totalPeriod, -1.0, 1.0 }
			} };
		};

		template<>
		struct LinearSweepConfig<SignalGenerator::Type::PerfectSawtoothDown200>
		{
			static constexpr std::array<LinearSweepPhase, 2> phases{ {
				{ LinearSweepPhase::totalPeriod, 1.0, -1.0 }
			} };
		};

		template<SignalGenerator::Type WaveType>
		inline double getSampleForSweep(SignalGenerator::index_t pos)
		{
			using Config = LinearSweepConfig<WaveType>;

			SignalGenerator::index_t accumulated = 0;
			for (SignalGenerator::index_t p = 0; p < Config::phases.size(); ++p)
			{
				if (pos < accumulated + Config::phases[p].period)
				{
					const SignalGenerator::index_t localPos = pos - accumulated;
					const double t = static_cast<double>(localPos) / static_cast<double>(Config::phases[p].period - 1);
					return Config::phases[p].startValue + t * (Config::phases[p].endValue - Config::phases[p].startValue);
				}
				accumulated += Config::phases[p].period;
			}
			return 0.0;
		}

		template<typename T, SignalGenerator::Type WaveType>
		void SignalGenerator::processSweepedWave(T** inout, index_t numFrames, const ProcessConfig& config)
		{
			const index_t numChannels = numChannels_;

			if (config.perChannel)
			{
				for (index_t frame = 0; frame < numFrames; ++frame)
				{
					for (index_t ch = 0; ch < numChannels; ++ch)
					{
						const index_t pos = (frameIndex + ch * LinearSweepPhase::totalPeriod / 4) % LinearSweepPhase::totalPeriod;
						const double sample = getSampleForSweep<WaveType>(pos);
						inout[ch][frame] = static_cast<T>(sample * config.amplitude + config.dcOffset);
					}
					++frameIndex;
				}
			}
			else
			{
				for (index_t frame = 0; frame < numFrames; ++frame)
				{
					const index_t pos = frameIndex % LinearSweepPhase::totalPeriod;
					const double sample = getSampleForSweep<WaveType>(pos);
					++frameIndex;

					const double out = sample * config.amplitude + config.dcOffset;
					for (index_t ch = 0; ch < numChannels; ++ch)
						inout[ch][frame] = static_cast<T>(out);
				}
			}
		}

	}

#endif
