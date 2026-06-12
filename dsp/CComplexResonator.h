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

	file:CComplexResonator.h

		A complex resonator system... More to come.

*************************************************************************************/

#ifndef CPL_CCOMPLEX_RESONATOR_H
#define CPL_CCOMPLEX_RESONATOR_H

#include "../simd.h"
#include "../LibraryOptions.h"
#include <vector>
#include "../Mathext.h"
#include "DSPWindows.h"
#include "../Utility.h"
#include "../lib/AlignedAllocator.h"

namespace cpl
{
	namespace dsp
	{
		template<typename T, std::size_t Channels = 1>
		class CComplexResonator
		{
		public:

			typedef T Scalar;

			static_assert(Channels > 0, "CComplexResonator needs at least one channel");
			static const std::size_t numChannels = Channels;

			class Constant
			{
			public:
				Constant()
					: numFilters(0)
					, numResonators()
					, centerFilter(0)
					, numVectors(0)
				{

					reallocBuffers(0, 1);
				}

				std::size_t getNumFilters() const noexcept
				{
					return numFilters;
				}

				/// <summary>
				/// Maps the internal resonators (and their vectors) to resonate at the frequencies specified in mappedHz.
				/// This call is SAFE, on any thread. However, it may acquire a mutex and reallocate memory.
				/// Values for mappedHz[n] > sampleRate/2 infers complex results
				/// </summary>
				/// <param name="mappedHz">
				/// A vector of size vSize of T. It is expected to be sorted.
				/// </param>
				/// <param name="vectors">
				/// Increases the amount of adjacent vectors around a single frequency, linearly spaced
				/// as fc +/- bw * v.
				/// This directly affects computation speed linearly, however more vectors give support
				/// for computing more exotic window functions in the time domain.
				/// Has to be an odd number. The center filter is implicit.
				/// </param>
				/// <param name="minNSize">
				/// Sets the minimum bound of the emulated window size (N). As the bandwidth is free and non-quantized, this may help
				/// in both ends of the spectre.
				/// </summary>
				/// <param name="maxNSize">
				/// Sets the minimum bound of the emulated window size (N). As the bandwidth is free and non-quantized, this may help
				/// in both ends of the spectre.
				/// </summary>
				template<typename Vector>
				void mapSystemHz(const Vector & mappedHz, std::size_t vSize, std::size_t vectors, T sampleRate, bool shouldHaveFreeQ, double minNSize, double maxNSize)
				{
					// 7 == state size of all members
					using namespace cpl;
					using namespace cpl::simd;

					CPL_RUNTIME_ASSERTION(sampleRate > 0);

					const auto minWindowSize = std::min(minNSize, maxNSize);
					const auto maxWindowSize = std::max(minNSize, maxNSize);

					const auto newData = reallocBuffers(vSize, vectors);

					std::size_t nR = numResonators;
					std::size_t vC = nR * 2; // space filled by a vector buf

					std::size_t k = 0;
					if (vSize == 1)
					{
						//auto const bandWidth = minWindowSize;
						//auto const Q = sampleRate / bandWidth;
						for (std::size_t v = 0; v < numVectors; ++v)
						{
							//auto const theta = mappedHz[i] + (z - (numVectors - 1) >> 1) * Q;
							//auto const omega = 2 * M_PI * theta / sampleRate;
						}
						k++;
					}
					else
					{

						//auto const Bq = (3 / qDBs) * M_E/12.0;

						for (k = 0; k < vSize; ++k)
						{

							auto const kM = std::size_t(k + 1) >= vSize ? vSize - 2 : k;
							auto hDiff = std::abs((double)mappedHz[kM + 1] - mappedHz[kM]);
							auto bandWidth = sampleRate / hDiff;

							if (!shouldHaveFreeQ)
								bandWidth = cpl::Math::confineTo<double>(bandWidth, minWindowSize, maxWindowSize);


							hDiff = (sampleRate) / bandWidth;

							// 3 dB law bandwidth of complex resonator
							// see jos' paper
							auto const r = exp(-consts<T>::pi * hDiff / sampleRate);

							N[k] = consts<T>::one / (1 - r);

							for (std::size_t v = 0; v < numVectors; ++v)
							{
								// so basically, for doing frequency-domain windowing using DFT-coefficients of the windows, we need filters that are linearly
								// spaced around the frequency like the FFT. DFT bins are spaced linearly like 0.5 / N.
								auto const omega = (consts<T>::tau * (mappedHz[k] + Math::mapAroundZero<Scalar>(v, numVectors) * hDiff * consts<T>::half)) / sampleRate;

								auto const realPart = r * std::cos(omega);
								auto const imagPart = r * std::sin(omega);

								coeff[v * vC + k + nR * real] = realPart; // coeffs.c[0].real()
								coeff[v * vC + k + nR * imag] = imagPart; // coeffs.c[0].imag()
							}

						}
					}
					// set remainder to zero
					for (; k < numResonators; ++k)
					{
						for (std::size_t v = 0; v < numVectors; ++v)
						{
							coeff[v * vC + k + real * nR] = coeff[v * vC + k + imag * nR] = (Scalar)0;
						}
					}

				}

			private:

				/// <summary>
				/// Increases the amount of adjacent vectors around a single frequency, linearly spaced
				/// as fc +/- bw * v.
				/// This directly affects computation speed linearly, however more vectors give support
				/// for computing more exotic window functions in the time domain.
				/// </summary>
				/// <param name="vectors">Has to be an odd number. The center filter is implicit. </param>
				bool reallocBuffers(std::size_t minimumSize, std::size_t vectors)
				{
					if (std::tie(numFilters, numVectors) == std::tie(minimumSize, vectors))
						return false;

					if (!(vectors & 0x1))
						CPL_RUNTIME_EXCEPTION("Invalid amount of vectors (even).");

					numVectors = vectors;
					centerFilter = (vectors - 1) >> 1;

					numFilters = minimumSize;
					// quantize to next multiple of 8, to ensure vectorization
					numResonators = numFilters + (8 - numFilters & 0x7);

					N.resize(numResonators);
					coeff.resize((real + imag) * 2 * numResonators * numVectors);

					return true;
				}

				friend class CComplexResonator<T, Channels>;
				inline Scalar getBandwidth(std::size_t resonator)
				{
					return N.at(resonator);
				}

				cpl::aligned_vector<Scalar, 32u> coeff;
				std::vector<Scalar> N;

				std::size_t centerFilter;
				std::size_t numVectors;

				std::size_t numFilters, numResonators;
			};


			CComplexResonator()
			{
				initiateResonatorScale();
			}


			/// <summary>
			/// Resonates the system (processing the data). Safe from any thread.
			/// Treats input data is real.
			/// </summary>
			template<typename V, class MultiVector>
			inline void resonateReal(const Constant& constant, const MultiVector & data, std::size_t numDataChannels, std::size_t numSamples)
			{
				numDataChannels = std::min(numChannels, numDataChannels);

				switch (numDataChannels)
				{
					case 1:
						switch (constant.numVectors)
						{
							case 1:
								internalWindowResonate1<V, MultiVector, 1>(constant, data, numSamples); break;
							case 3:
								internalWindowResonate3<V, MultiVector, 1>(constant, data, numSamples); break;
							case 5:
								internalWindowResonate<V, MultiVector, 1, 5>(constant, data, numSamples); break;
							case 7:
								internalWindowResonate<V, MultiVector, 1, 7>(constant, data, numSamples); break;
							case 9:
								internalWindowResonate<V, MultiVector, 1, 9>(constant, data, numSamples); break;
						}
						break;
					case 2:
						switch (constant.numVectors)
						{
							case 1:
								internalWindowResonate1<V, MultiVector, 2>(constant, data, numSamples); break;
							case 3:
								internalWindowResonate3<V, MultiVector, 2>(constant, data, numSamples); break;
							case 5:
								internalWindowResonate<V, MultiVector, 2, 5>(constant, data, numSamples); break;
							case 7:
								internalWindowResonate<V, MultiVector, 2, 7>(constant, data, numSamples); break;
							case 9:
								internalWindowResonate<V, MultiVector, 2, 9>(constant, data, numSamples); break;
						}
						break;
					default:
						CPL_RUNTIME_EXCEPTION("Unsupported number of channels.");
				}
			}


			/// <summary>
			/// Resonates the system (processing the data). Safe from any thread.
			/// </summary>
			/// <param name="data">
			/// A multidimensioal array of following (supported) dimensions: [2][numSamples]
			/// </param>
			template<typename V, class MultiVector>
			inline void resonateComplex(const Constant& constant, const MultiVector & data, std::size_t numSamples)
			{
				switch (constant.numVectors)
				{
					case 1:
						internalWindowComplexResonate<V, MultiVector, 1>(constant, data, numSamples); break;
					case 3:
						internalWindowComplexResonate<V, MultiVector, 3>(constant, data, numSamples); break;
					case 5:
						internalWindowComplexResonate<V, MultiVector, 5>(constant, data, numSamples); break;
					case 7:
						internalWindowComplexResonate<V, MultiVector, 7>(constant, data, numSamples); break;
					case 9:
						internalWindowComplexResonate<V, MultiVector, 9>(constant, data, numSamples); break;
				}

			}

			/// <summary>
			/// Gets the unwindowed resonance at the specified index.
			/// </summary>
			inline std::complex<Scalar> getResonanceAt(const Constant& c, std::size_t resonator, std::size_t channel) const
			{
				match(c);

				auto const gainCoeff = c.N.at(resonator) * 0.5; // bounds checking here.

				std::size_t nR = c.numResonators;
				std::size_t vC = nR * 2; // space filled by a vector buf
				std::size_t sC = vC * c.numVectors; // space filled by all vector bufs

				return std::complex<Scalar>
					(
						state[sC * channel + c.centerFilter * vC + resonator + real * nR] / gainCoeff,
						state[sC * channel + c.centerFilter * vC + resonator + imag * nR] / gainCoeff
						);
			}

			/// <summary>
			/// Gets the windowed resonance at the specified index.
			/// If the window is larger than the amout of vectors, it will be truncated.
			/// </summary>
			template<WindowTypes win>
			inline std::complex<Scalar> getWindowedResonanceAt(const Constant& c, std::size_t resonator, std::size_t channel)
			{
				match(c);

				Scalar gainCoeff = c.N.at(resonator) * 0.5; // 2^-3 (3 vectors)
				Scalar scale = resonatorScales[(int)win];
				Scalar realPart(0), imagPart(0);

				std::size_t nR = c.numResonators;
				std::size_t vC = nR * 2; // space filled by a vector buf
				std::size_t sC = vC * c.numVectors; // space filled by all vector bufs

				auto extent = std::extent<decltype(Windows::DFTCoeffs<T, win>::coeffs)>::value;

				std::size_t v = 0;
				std::size_t off = 0;
				// truncate window,
				if (extent > c.numVectors)
				{
					auto diff = extent - c.numVectors;
					diff = (diff - 1) >> 1;
					v = diff;
					extent -= diff;
				}
				else if (c.numVectors > extent) // or select the middle vectors.
				{
					auto diff = c.numVectors - extent;
					diff = (diff - 1) >> 1;
					off = diff;
				}

				for (; v < extent; ++v)
				{
					realPart += Windows::DFTCoeffs<T, win>::coeffs[v] * state[sC * channel + (v + off) * vC + resonator + real * nR];
					imagPart += Windows::DFTCoeffs<T, win>::coeffs[v] * state[sC * channel + (v + off) * vC + resonator + imag * nR];
				}

				return std::complex<Scalar>(scale * realPart / gainCoeff, scale * imagPart / gainCoeff);
			}
			/// <summary>
			/// Gets the windowed resonance at the specified index.
			/// If the window is larger than the amout of vectors, it will be truncated.
			/// <param name="out">
			/// Vector is a dimensional array of size * 2 (complex) * channels of T. Channels are separated.
			/// </param>
			/// </summary>
			template<typename V, class Vector>
			void getWholeWindowedState(const Constant& c, WindowTypes win, Vector & out, std::size_t outChannels, std::size_t outSize)
			{
				typedef WindowTypes w;
				switch (win)
				{
					case cpl::dsp::WindowTypes::Hann:
						getWholeWindowedState<w::Hann, V, Vector>(c, out, outChannels, outSize); break;
					case cpl::dsp::WindowTypes::Hamming:
						getWholeWindowedState<w::Hamming, V, Vector>(c, out, outChannels, outSize); break;
					case cpl::dsp::WindowTypes::FlatTop:
						getWholeWindowedState<w::FlatTop, V, Vector>(c, out, outChannels, outSize); break;
					case cpl::dsp::WindowTypes::Blackman:
						getWholeWindowedState<w::Blackman, V, Vector>(c, out, outChannels, outSize); break;
					case cpl::dsp::WindowTypes::ExactBlackman:
						getWholeWindowedState<w::ExactBlackman, V, Vector>(c, out, outChannels, outSize); break;
					case cpl::dsp::WindowTypes::Nuttall:
						getWholeWindowedState<w::Nuttall, V, Vector>(c, out, outChannels, outSize); break;
					case cpl::dsp::WindowTypes::BlackmanNuttall:
						getWholeWindowedState<w::BlackmanNuttall, V, Vector>(c, out, outChannels, outSize); break;
					case cpl::dsp::WindowTypes::BlackmanHarris:
						getWholeWindowedState<w::BlackmanHarris, V, Vector>(c, out, outChannels, outSize); break;
					default:
						getWholeWindowedState<w::Rectangular, V, Vector>(c, out, outChannels, outSize); break;
				}
			}

			/// <summary>
			/// Gets the windowed resonance at the specified index.
			/// If the window is larger than the amout of vectors, it will be truncated.
			/// <param name="out">
			/// Vector is a dimensional array of size * 2 (complex) * channels of T. Channels are separated.
			/// </param>
			/// </summary>
			template<WindowTypes win, typename V, class Vector>
			void getWholeWindowedState(const Constant& constant, Vector & out, std::size_t outChannels, std::size_t outSize)
			{
				match(constant);

				std::size_t maxResonators = std::min(constant.numResonators, outSize);
				std::size_t maxChannels = std::min(numChannels, outChannels);

				auto extent = std::extent<decltype(Windows::DFTCoeffs<T, win>::coeffs)>::value;

				std::size_t vStart = 0;
				std::size_t off = 0;
				Scalar scale = resonatorScales[(int)win];

				std::size_t nR = constant.numResonators;
				std::size_t vC = nR * 2; // space filled by a vector buf
				std::size_t sC = vC * constant.numVectors; // space filled by all vector bufs

				// truncate window,
				if (extent > constant.numVectors)
				{
					auto diff = extent - constant.numVectors;
					diff = (diff - 1);
					vStart = diff;
					extent -= diff;
				}
				else if (constant.numVectors > extent) // or select the middle vectors.
				{
					auto diff = constant.numVectors - extent;
					diff = (diff - 1) >> 1;
					off = diff;
				}

				for (std::size_t c = 0; c < maxChannels; c++)
				{
					for (std::size_t k = 0; k < maxResonators; k++)
					{
						Scalar gainCoeff = scale / (constant.N[k] * T(0.5)); // 2^-3 (3 vectors)

						Scalar realPart(0), imagPart(0);

						for (std::size_t v = vStart; v < extent; ++v)
						{
							realPart += Windows::DFTCoeffs<T, win>::coeffs[v] * state[sC * c + (v + off) * vC + k + real * nR];
							imagPart += Windows::DFTCoeffs<T, win>::coeffs[v] * state[sC * c + (v + off) * vC + k + imag * nR];
						}

						out[c * 2 * outSize + k * 2] = gainCoeff * realPart;
						out[c * 2 * outSize + k * 2 + 1] = gainCoeff * imagPart;

					}
				}

			}


			/// <summary>
			/// Resets the filters state to zero. Coefficients are untouched, reset them (indirectly) through mapSystemHz.
			/// Blocks the processing.
			/// </summary>
			void resetState()
			{
				std::fill(state.begin(), state.end(), Scalar());
			}


		private:

			void match(const Constant& constant)
			{
				state.resize((real + imag) * 2 * constant.numResonators * constant.numVectors * numChannels);
			}

			template<typename V, class MultiVector, std::size_t inputDataChannels, std::size_t staticVectors>
			void internalWindowResonate(const Constant& constant, const MultiVector & data, std::size_t numSamples)
			{
				match(constant);

				using namespace cpl;
				using namespace cpl::simd;

				auto const vfactor = suitable_container<V>::size;
				V t0;

				std::size_t nR = constant.numResonators;
				std::size_t vC = nR * 2; // space filled by a vector buf
				std::size_t sC = vC * constant.numVectors; // space filled by all vector bufs


				 //  iterate over each filter for each sample for each channel for each vector.
				for (Types::fint_t k = 0; k < constant.numFilters; k += vfactor)
				{

					// pointer to current sample
					const typename scalar_of<V>::type * audioInputs[numChannels];

					V p_r[staticVectors], p_i[staticVectors];
					V s_r[inputDataChannels][staticVectors], s_i[inputDataChannels][staticVectors];

					// and load them
					for (Types::fint_t v = 0; v < staticVectors; ++v)
					{
						p_r[v] = load<V>(&constant.coeff[v * vC + k + nR * real]); // cos: e^i*omega (real)
						p_i[v] = load<V>(&constant.coeff[v * vC + k + nR * imag]); // sin: e^i*omega (imag)

						for (Types::fint_t c = 0; c < inputDataChannels; ++c)
						{
							audioInputs[c] = &data[c][0];

							s_r[c][v] = load<V>(&state[sC * c + v * vC + k + nR * real]);
							s_i[c][v] = load<V>(&state[sC * c + v * vC + k + nR * imag]);
						}
					}
					for (Types::fint_t sample = 0; sample < numSamples; ++sample)
					{
						for (Types::fint_t c = 0; c < inputDataChannels; ++c)
						{
							// combing stage
							V input = broadcast<V>(audioInputs[c]);

							// v stage (m) (fc +- v * bw)
							for (Types::fint_t v = 0; v < staticVectors; ++v)
							{
								t0 = s_r[c][v] * p_r[v] - s_i[c][v] * p_i[v];
								s_i[c][v] = s_r[c][v] * p_i[v] + s_i[c][v] * p_r[v];
								s_r[c][v] = t0 + input;
							}

							audioInputs[c]++;
						}


					}
					for (Types::fint_t c = 0; c < inputDataChannels; ++c)
					{
						for (Types::fint_t v = 0; v < staticVectors; ++v)
						{
							store(&state[sC * c + v * vC + k + nR * real], s_r[c][v]); // state: e^i*omega (real)
							store(&state[sC * c + v * vC + k + nR * imag], s_i[c][v]); // state: e^i*omega (imag)
						}
					}
				}
			}

			template<typename V, class MultiVector, std::size_t inputDataChannels>
			void internalWindowResonate1(const Constant& constant, const MultiVector& data, std::size_t numSamples)
			{
				match(constant);

				using namespace cpl;
				using namespace cpl::simd;

				auto const vfactor = suitable_container<V>::size;
				V t0;

				std::size_t nR = constant.numResonators;
				std::size_t vC = nR * 2; // space filled by a vector buf
				std::size_t sC = vC; // space filled by all vector bufs


				 //  iterate over each filter for each sample for each channel for each vector.
				for (Types::fint_t k = 0; k < constant.numFilters; k += vfactor)
				{

					// pointer to current sample
					const typename scalar_of<V>::type* audioInputs[numChannels];

					V p_r, p_i;
					V s_r[inputDataChannels], s_i[inputDataChannels];

					// and load them

					p_r = load<V>(&constant.coeff[k + nR * real]); // cos: e^i*omega (real)
					p_i = load<V>(&constant.coeff[k + nR * imag]); // sin: e^i*omega (imag)

					for (Types::fint_t c = 0; c < inputDataChannels; ++c)
					{
						audioInputs[c] = &data[c][0];

						s_r[c] = load<V>(&state[sC * c + k + nR * real]);
						s_i[c] = load<V>(&state[sC * c + k + nR * imag]);
					}

					for (Types::fint_t sample = 0; sample < numSamples; ++sample)
					{
						for (Types::fint_t c = 0; c < inputDataChannels; ++c)
						{
							// combing stage
							V input = broadcast<V>(audioInputs[c]);

							// v stage (m) (fc +- v * bw)
							t0 = s_r[c] * p_r - s_i[c] * p_i;
							s_i[c] = s_r[c] * p_i + s_i[c] * p_r;
							s_r[c] = t0 + input;

							audioInputs[c]++;
						}
					}

					for (Types::fint_t c = 0; c < inputDataChannels; ++c)
					{
						store(&state[sC * c + k + nR * real], s_r[c]); // state: e^i*omega (real)
						store(&state[sC * c + k + nR * imag], s_i[c]); // state: e^i*omega (imag)
					}
				}
			}

			template<typename V, class MultiVector, std::size_t staticVectors>
			void internalWindowComplexResonate(const Constant& constant, const MultiVector & data, std::size_t numSamples)
			{
				match(constant);

				using namespace cpl;
				using namespace cpl::simd;

				auto const vfactor = suitable_container<V>::size;
				V t0;

				std::size_t nR = constant.numResonators;
				std::size_t vC = nR * 2; // space filled by a vector buf

				 //  iterate over each filter for each sample for each channel for each vector.
				for (Types::fint_t k = 0; k < constant.numFilters; k += vfactor)
				{

					// pointer to current sample
					const typename scalar_of<V>::type * audioInputs[numChannels];

					V p_r[staticVectors], p_i[staticVectors];
					V s_r[staticVectors], s_i[staticVectors];

					// and load them
					for (Types::fint_t v = 0; v < staticVectors; ++v)
					{
						p_r[v] = load<V>(&constant.coeff[v * vC + k + nR * real]); // cos: e^i*omega (real)
						p_i[v] = load<V>(&constant.coeff[v * vC + k + nR * imag]); // sin: e^i*omega (imag)


						s_r[v] = load<V>(&state[v * vC + k + nR * real]);
						s_i[v] = load<V>(&state[v * vC + k + nR * imag]);

						for (Types::fint_t c = 0; c < 2; ++c)
						{
							audioInputs[c] = &data[c][0];
						}
					}
					for (Types::fint_t sample = 0; sample < numSamples; ++sample)
					{
						// combing stage
						V
							real = broadcast<V>(audioInputs[0]++),
							imag = broadcast<V>(audioInputs[1]++);

						for (Types::fint_t v = 0; v < staticVectors; ++v)
						{
							t0 = s_r[v] * p_r[v] - s_i[v] * p_i[v];
							s_i[v] = s_r[v] * p_i[v] + s_i[v] * p_r[v];
							s_r[v] = t0 + real;
							s_i[v] += imag;
						}
					}

					for (Types::fint_t v = 0; v < staticVectors; ++v)
					{
						store(&state[v * vC + k + nR * real], s_r[v]); // state: e^i*omega (real)
						store(&state[v * vC + k + nR * imag], s_i[v]); // state: e^i*omega (imag)
					}
				}
			}

			template<typename V, class MultiVector, std::size_t inputDataChannels>
			void internalWindowResonate3(const Constant& constant, const MultiVector & data, std::size_t numSamples)
			{
				match(constant);

				using namespace cpl;
				using namespace cpl::simd;

				auto const vfactor = suitable_container<V>::size;
				V t0;

				std::size_t nR = constant.numResonators;
				std::size_t vC = nR * 2; // space filled by a vector buf
				std::size_t sC = vC * constant.numVectors; // space filled by all vector bufs

				//  iterate over each filter for each sample for each channel.
				for (Types::fint_t filter = 0; filter < constant.numFilters; filter += vfactor)
				{

					// pointer to current sample
					const typename scalar_of<V>::type * audioInputs[numChannels];

					// load coefficients
					const V
						p_m1_r = load<V>(&constant.coeff[0 * vC + filter + nR * real]), // coeff: e^i*omega-q (real)
						p_m1_i = load<V>(&constant.coeff[0 * vC + filter + nR * imag]), // coeff: e^i*omega-q (imag)
						p_m_r = load<V>(&constant.coeff[1 * vC + filter + nR * real]), // coeff: e^i*omega (real)
						p_m_i = load<V>(&constant.coeff[1 * vC + filter + nR * imag]), // coeff: e^i*omega (imag)
						p_p1_r = load<V>(&constant.coeff[2 * vC + filter + nR * real]), // coeff: e^i*omega+q (real)
						p_p1_i = load<V>(&constant.coeff[2 * vC + filter + nR * imag]); // coeff: e^i*omega+q (imag)

					// create states
					V
						s_m1_r[inputDataChannels],
						s_m1_i[inputDataChannels],
						s_m_r[inputDataChannels],
						s_m_i[inputDataChannels],
						s_p1_r[inputDataChannels],
						s_p1_i[inputDataChannels];

					// and load them
					for (Types::fint_t c = 0; c < inputDataChannels; ++c)
					{
						audioInputs[c] = &data[c][0];
						s_m1_r[c] = load<V>(&state[sC * c + 0 * vC + filter + nR * real]); // cos: e^i*omega-q (real)
						s_m1_i[c] = load<V>(&state[sC * c + 0 * vC + filter + nR * imag]); // sin: e^i*omega-q (imag)
						s_m_r[c] = load<V>(&state[sC * c + 1 * vC + filter + nR * real]); // cos: e^i*omega (real)
						s_m_i[c] = load<V>(&state[sC * c + 1 * vC + filter + nR * imag]); // sin: e^i*omega (imag)
						s_p1_r[c] = load<V>(&state[sC * c + 2 * vC + filter + nR * real]); // cos: e^i*omega+q (real)
						s_p1_i[c] = load<V>(&state[sC * c + 2 * vC + filter + nR * imag]); // sin: e^i*omega+q (imag)
					}

					for (Types::fint_t sample = 0; sample < numSamples; ++sample)
					{
						for (Types::fint_t c = 0; c < inputDataChannels; ++c)
						{
							// combing stage
							V input = broadcast<V>(audioInputs[c]);

							// -1 stage (m1) (fc - bw)
							t0 = s_m1_r[c] * p_m1_r - s_m1_i[c] * p_m1_i;
							s_m1_i[c] = s_m1_r[c] * p_m1_i + s_m1_i[c] * p_m1_r;
							s_m1_r[c] = t0 + input;

							// 0 stage (m) (fc)
							t0 = s_m_r[c] * p_m_r - s_m_i[c] * p_m_i;
							s_m_i[c] = s_m_r[c] * p_m_i + s_m_i[c] * p_m_r;
							s_m_r[c] = t0 + input;

							// +1 stage (p1) (fc + bw)
							t0 = s_p1_r[c] * p_p1_r - s_p1_i[c] * p_p1_i;
							s_p1_i[c] = s_p1_r[c] * p_p1_i + s_p1_i[c] * p_p1_r;
							s_p1_r[c] = t0 + input;

							audioInputs[c]++;
						}


					}
					for (Types::fint_t c = 0; c < inputDataChannels; ++c)
					{
						store(&state[sC * c + 0 * vC + filter + nR * real], s_m1_r[c]); // state: e^i*omega-q (real)
						store(&state[sC * c + 0 * vC + filter + nR * imag], s_m1_i[c]); // state: e^i*omega-q (imag)
						store(&state[sC * c + 1 * vC + filter + nR * real], s_m_r[c]); // state: e^i*omega (real)
						store(&state[sC * c + 1 * vC + filter + nR * imag], s_m_i[c]); // state: e^i*omega (imag)
						store(&state[sC * c + 2 * vC + filter + nR * real], s_p1_r[c]); // state: e^i*omega+q (real)
						store(&state[sC * c + 2 * vC + filter + nR * imag], s_p1_i[c]); // state: e^i*omega+q (imag)
					}
				}
			}

			static T resonatorScales[(std::size_t)WindowTypes::End];

			static int initiateResonatorScale()
			{
				T * dummy = nullptr;
				for (int i = 0; i < (int)WindowTypes::End; ++i)
				{
					if (windowHasFiniteDFT((WindowTypes)i))
						resonatorScales[i] = windowScale<T>((WindowTypes)i, dummy, 0, Windows::Shape::Periodic);
					else
						resonatorScales[i] = (T)1;
				}

				// handtuned correction for weirdness in IIR resonation.
				// if the resonator is not critically tuned, it's precision is exact to 4 decimal places, at least.
				resonatorScales[(int)WindowTypes::Hamming] = (T)1.7240448989724198867099599842733;
				resonatorScales[(int)WindowTypes::Blackman] /= (T)1.05428600;
				resonatorScales[(int)WindowTypes::ExactBlackman] /= (T)1.0641;
				resonatorScales[(int)WindowTypes::Nuttall] /= (T)1.10325217;
				resonatorScales[(int)WindowTypes::FlatTop] /= (T)1.50307810;
				resonatorScales[(int)WindowTypes::BlackmanNuttall] /= (T)1.09862804;
				resonatorScales[(int)WindowTypes::BlackmanHarris] /= (T)1.10081;

				return 0;
			}

			enum
			{
				real = 0, imag = 1
			};

			cpl::aligned_vector<Scalar, 32u> state;

		};
		template<typename T, std::size_t Channels>
		T CComplexResonator<T, Channels>::resonatorScales[(std::size_t)WindowTypes::End];

		template<typename T, std::size_t Channels>
		const std::size_t CComplexResonator<T, Channels>::numChannels;
	};
};
#endif
