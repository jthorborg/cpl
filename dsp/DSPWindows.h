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

	file:DSPWindows.h

		Interface for DSP windows, as well as implementations of common windows.

*************************************************************************************/

#ifndef DSPWINDOWS_H
#define DSPWINDOWS_H

#include "../Common.h"
#include "../Utility.h"
#include <vector>
#include <memory>
#include <map>
#include "../dsp.h"
#include "../octave/signal/octave_signal.h"
#include <array>
#include "../ffts.h"
#include <numeric>
#include "../lib/AlignedAllocator.h"

namespace cpl
{
	namespace dsp
	{

		/// <summary>
		/// Kinds of windows. These can be used to index other relevant functions.
		/// </summary>
		enum class WindowTypes
		{
			Rectangular,
			Hann,
			Hamming,
			FlatTop,
			Blackman,
			ExactBlackman,
			Triangular,
			Parzen,
			Nuttall,
			BlackmanNuttall,
			BlackmanHarris,
			Gaussian,
			DolphChebyshev,
			Kaiser,
			Ultraspherical,
			Welch,
			Poisson,
			HannPoisson,
			Lanczos,
			End
		};

		namespace Windows
		{
			/// <summary>
			/// Controls the symmetry of window functions.
			/// </summary>
			enum class Shape
			{
				/// <summary>
				/// Often used for filtering, specifying a N-1 period
				/// </summary>
				Symmetric,
				/// <summary>
				/// Often used for spectral analysis, specifiying a N period
				/// </summary>
				Periodic,
				/// <summary>
				/// Offsets the phase of the window by 0.5 / N -1, when possible.
				/// Period is N-1, making a symmetric window.
				/// </summary>
				DFTEven

			};

			/// <summary>
			/// Returns the name of the window type, as a string.
			/// Invertible by enumFromString
			/// </summary>
			inline const char * stringFromEnum(WindowTypes w)
			{
				switch (w)
				{
					case cpl::dsp::WindowTypes::Rectangular: return "Rectangular";
					case cpl::dsp::WindowTypes::Hann: return "Hann";
					case cpl::dsp::WindowTypes::Hamming: return "Hamming";
					case cpl::dsp::WindowTypes::FlatTop: return "Flat Top";
					case cpl::dsp::WindowTypes::Blackman: return "Blackman";
					case cpl::dsp::WindowTypes::ExactBlackman: return "Exact Blackman";
					case cpl::dsp::WindowTypes::Triangular: return "Triangular";
					case cpl::dsp::WindowTypes::Parzen: return "Parzen";
					case cpl::dsp::WindowTypes::Nuttall: return "Nuttall";
					case cpl::dsp::WindowTypes::BlackmanNuttall: return "Blackman-Nuttall";
					case cpl::dsp::WindowTypes::BlackmanHarris: return "Blackman-Harris";
					case cpl::dsp::WindowTypes::Gaussian: return "Gaussian";
					case cpl::dsp::WindowTypes::DolphChebyshev: return "Dolph-Chebyshev";
					case cpl::dsp::WindowTypes::Kaiser: return "Kaiser";
					case cpl::dsp::WindowTypes::Ultraspherical: return "Ultraspherical";
					case cpl::dsp::WindowTypes::Welch: return "Welch";
					case cpl::dsp::WindowTypes::Poisson: return "Poisson";
					case cpl::dsp::WindowTypes::HannPoisson: return "Hann-Poisson";
					case cpl::dsp::WindowTypes::Lanczos: return "Lanczos";
					case cpl::dsp::WindowTypes::End:
						break;
					default:
						break;
				}

				return "<Erroneous window>";
			}

			namespace
			{

				typedef std::map<std::string, WindowTypes, std::less<>> WMap;

				inline WMap InitializeWindowMap()
				{
					WMap wm;

					for (std::size_t i = 0; i < (std::size_t)WindowTypes::End; ++i)
					{
						auto w = (WindowTypes)i;
						wm[stringFromEnum(w)] = w;
					}
					return wm;
				}

				const WMap WindowNameMap = InitializeWindowMap();


				template<typename T, class InOutVector>
				static T naiveWindowScale(InOutVector & w, std::size_t N)
				{
					// avoids potential zero-division.
					return N ? (T)N / std::accumulate(&w[0], &w[0] + N, (T)0) : T();
				}
			};

			/// <summary>
			/// Matches a window name with a WindowType, returning it if possible, and
			/// returning a Rectangular window if not possible.
			/// </summary>
			inline WindowTypes enumFromString(const std::string_view w)
			{
				auto it = WindowNameMap.find(w);
				if (it == WindowNameMap.end())
					return WindowTypes::Rectangular;
				else
					return it->second;
			}

			template<WindowTypes wclass>
			class Generator;

			template<typename T, typename LengthType, typename InOutVector>
			void generalizedCosineSequence(InOutVector & v, LengthType N, Shape symmetry, T a0, T a1)
			{
				T K = T(symmetry == Shape::Periodic ? N : N - 1);
				const auto piK = T(M_PI / K);
				T offset = symmetry == Shape::DFTEven ? piK : 0;
				T scale = 1 / (a0 + a1);
				for (std::size_t n = 0; n < N; ++n)
					v[n] = scale * (a0 - a1 * std::cos(offset + n * 2 * piK));
			}

			template<typename T, typename LengthType, typename InOutVector>
			void generalizedCosineSequence(InOutVector & v, LengthType N, Shape symmetry, T a0, T a1, T a2)
			{
				T K = T(symmetry == Shape::Periodic ? N : N - 1);
				const auto piK = T(M_PI / K);
				T offset = symmetry == Shape::DFTEven ? piK : 0;
				T scale = 1 / (a0 + a1 + a2);
				for (std::size_t n = 0; n < N; ++n)
					v[n] = scale * (a0 - a1 * std::cos(offset + n * 2 * piK) + a2 * std::cos(offset + n * 4 * piK));
			}

			template<typename T, typename LengthType, typename InOutVector>
			void generalizedCosineSequence(InOutVector & v, LengthType N, Shape symmetry, T a0, T a1, T a2, T a3)
			{
				T K = T(symmetry == Shape::Periodic ? N : N - 1);
				const auto piK = T(M_PI / K);
				T offset = symmetry == Shape::DFTEven ? piK : 0;
				T scale = 1 / (a0 + a1 + a2 + a3);
				for (std::size_t n = 0; n < N; ++n)
					v[n] = scale * (a0 - a1 * std::cos(offset + n * 2 * piK) + a2 * std::cos(offset + n * 4 * piK) - a3 * std::cos(offset + n * 6 * piK));
			}

			template<typename T, typename LengthType, typename InOutVector>
			void generalizedCosineSequence(InOutVector & v, LengthType N, Shape symmetry, T a0, T a1, T a2, T a3, T a4)
			{
				T K = T(symmetry == Shape::Periodic ? N : N - 1);
				const auto piK = T(M_PI / K);
				T offset = symmetry == Shape::DFTEven ? piK : 0;
				T scale = 1 / (a0 + a1 + a2 + a3 + a4);
				for (std::size_t n = 0; n < N; ++n)
					v[n] = scale * (a0 - a1 * std::cos(offset + n * 2 * piK) + a2 * std::cos(offset + n * 4 * piK) - a3 * std::cos(offset + n * 6 * piK) + a4 * std::cos(offset + n * 8 * piK));
			}

			template<>
			class Generator<WindowTypes::Rectangular>
			{
			public:
				template<typename T, class InOutVector>
				static void generate(InOutVector & w, std::size_t N, Shape symmetry = Shape::Symmetric, T alpha = T(), T beta = T())
				{
					if (N == 0)
						return;
					for (std::size_t n = 0; n < N; ++n) w[n] = 1;
				}

				template<typename T, class InOutVector>
				static T scale(const InOutVector & w, std::size_t N, Shape symmetry = Shape::Symmetric, T alpha = T(), T beta = T())
				{
					return 1;
				}

				static const bool hasFiniteDFT = true;
			};

			template<>
			class Generator<WindowTypes::Hann>
			{
			public:
				template<typename T, class InOutVector>
				static void generate(InOutVector & w, std::size_t N, Shape symmetry = Shape::Symmetric, T alpha = T(), T beta = T())
				{
					if (N == 0)
						return;
					generalizedCosineSequence(w, N, symmetry, (T)0.5, (T)0.5);
				}

				template<typename T, class InOutVector>
				static T scale(const InOutVector & w, std::size_t N, Shape symmetry = Shape::Symmetric, T alpha = T(), T beta = T())
				{
					switch (symmetry)
					{
						case Shape::Symmetric: return (T)2.0078431372549028;
						case Shape::Periodic: return (T)2;
						case Shape::DFTEven: return (T)2.0078425397061168;
						default:
							return 0;
					}
				}

				static const bool hasFiniteDFT = true;
			};

			template<>
			class Generator<WindowTypes::Hamming>
			{
			public:
				template<typename T, class InOutVector>
				static void generate(InOutVector & w, std::size_t N, Shape symmetry = Shape::Symmetric, T alpha = T(), T beta = T())
				{
					if (N == 0)
						return;
					generalizedCosineSequence(w, N, symmetry, (T)(25.0 / 46.0), (T)(21.0 / 46.0));
				}

				template<typename T, class InOutVector>
				static T scale(const InOutVector & w, std::size_t N, Shape symmetry = Shape::Symmetric, T alpha = T(), T beta = T())
				{
					switch (symmetry)
					{
						case Shape::Symmetric: return (T)1.8460573757642269;
						case Shape::Periodic: return (T)1.84;
						case Shape::DFTEven: return (T)1.8460569145574548;
						default:
							return 0;
					}
				}

				static const bool hasFiniteDFT = true;
			};

			template<>
			class Generator<WindowTypes::Blackman>
			{
			public:
				template<typename T, class InOutVector>
				static void generate(InOutVector & w, std::size_t N, Shape symmetry = Shape::Symmetric, T alpha = T(), T beta = T())
				{
					if (N == 0)
						return;
					generalizedCosineSequence(w, N, symmetry, (T)0.42, (T)0.5, (T)0.08);
				}

				template<typename T, class InOutVector>
				static T scale(const InOutVector & w, std::size_t N, Shape symmetry = Shape::Symmetric, T alpha = T(), T beta = T())
				{
					switch (symmetry)
					{
						case Shape::Symmetric: return (T)2.3902894491129789;
						case Shape::Periodic: return (T)2.3809523809523818;
						case Shape::DFTEven: return (T)2.3902887377453803;
						default:
							return 0;
					}

				}

				static const bool hasFiniteDFT = true;
			};

			template<>
			class Generator<WindowTypes::ExactBlackman>
			{
			public:
				template<typename T, class InOutVector>
				static void generate(InOutVector & w, std::size_t N, Shape symmetry = Shape::Symmetric, T alpha = T(), T beta = T())
				{
					if (N == 0)
						return;
					generalizedCosineSequence(w, N, symmetry, (T)(7938.0 / 18608.0), (T)(9240.0 / 18608.0), (T)(1430.0 / 18608.0));
				}

				template<typename T, class InOutVector>
				static T scale(const InOutVector & w, std::size_t N, Shape symmetry = Shape::Symmetric, T alpha = T(), T beta = T())
				{
					switch (symmetry)
					{
						case Shape::Symmetric: return (T)2.3532113037576110;
						case Shape::Periodic: return (T)2.3441672965482483;
						case Shape::DFTEven: return (T)2.3532106147611507;
						default:
							return 0;
					}

				}

				static const bool hasFiniteDFT = true;
			};

			template<>
			class Generator<WindowTypes::BlackmanHarris>
			{
			public:
				template<typename T, class InOutVector>
				static void generate(InOutVector & w, std::size_t N, Shape symmetry = Shape::Symmetric, T alpha = T(), T beta = T())
				{
					if (N == 0)
						return;
					generalizedCosineSequence(w, N, symmetry, (T)(0.35875), (T)(0.48829), (T)(0.14128), (T)0.01168);
				}

				template<typename T, class InOutVector>
				static T scale(const InOutVector & w, std::size_t N, Shape symmetry = Shape::Symmetric, T alpha = T(), T beta = T())
				{
					switch (symmetry)
					{
						case Shape::Symmetric: return (T)2.7983858123588292;
						case Shape::Periodic: return (T)2.7874564459930311;
						case Shape::DFTEven: return (T)2.7983849796786058;
						default:
							return 0;
					}

				}

				static const bool hasFiniteDFT = true;
			};

			template<>
			class Generator<WindowTypes::Nuttall>
			{
			public:
				template<typename T, class InOutVector>
				static void generate(InOutVector & w, std::size_t N, Shape symmetry = Shape::Symmetric, T alpha = T(), T beta = T())
				{
					if (N == 0)
						return;
					generalizedCosineSequence(w, N, symmetry, (T)0.355768, (T)0.487396, (T)0.144232, (T)0.0126048);
				}

				template<typename T, class InOutVector>
				static T scale(const InOutVector & w, std::size_t N, Shape symmetry = Shape::Symmetric, T alpha = T(), T beta = T())
				{
					switch (symmetry)
					{
						case Shape::Symmetric: return (T)2.8218433852406424;
						case Shape::Periodic: return (T)2.8108205347304982;
						case Shape::DFTEven: return (T)2.8218425454375429;
						default:
							return 0;
					}
				}

				static const bool hasFiniteDFT = true;
			};

			template<>
			class Generator<WindowTypes::BlackmanNuttall>
			{
			public:
				template<typename T, class InOutVector>
				static void generate(InOutVector & w, std::size_t N, Shape symmetry = Shape::Symmetric, T alpha = T(), T beta = T())
				{
					if (N == 0)
						return;
					generalizedCosineSequence(w, N, symmetry, (T)0.3635819, (T)0.4891775, (T)0.1365995, (T)0.0106411);
				}

				template<typename T, class InOutVector>
				static T scale(const InOutVector & w, std::size_t N, Shape symmetry = Shape::Symmetric, T alpha = T(), T beta = T())
				{
					switch (symmetry)
					{
						case Shape::Symmetric: return (T)2.7611870672387337;
						case Shape::Periodic: return (T)2.7504119429487552;
						case Shape::DFTEven: return (T)2.7611862463124877;
						default:
							return 0;
					}
				}

				static const bool hasFiniteDFT = true;
			};

			template<>
			class Generator<WindowTypes::FlatTop>
			{
			public:
				template<typename T, class InOutVector>
				static void generate(InOutVector & w, std::size_t N, Shape symmetry = Shape::Symmetric, T alpha = T(), T beta = T())
				{
					if (N == 0)
						return;
					generalizedCosineSequence(w, N, symmetry, (T)1.0, (T)1.93, (T)1.29, (T)0.388, (T)0.028);
				}

				template<typename T, class InOutVector>
				static T scale(const InOutVector & output, std::size_t N, Shape symmetry = Shape::Symmetric, T alpha = T(), T beta = T())
				{
					switch (symmetry)
					{
						default:
							//case Shape::Symmetric: return (T)4.7095873015873009;
						case Shape::Periodic: return (T)4.6360000000000001;
							//case Shape::DFTEven: return (T)4.7094943766265196;
							return 0;
					}
				}

				static const bool hasFiniteDFT = true;
			};

			template<>
			class Generator<WindowTypes::Triangular>
			{
			public:
				template<typename T, class InOutVector>
				static void generate(InOutVector & w, std::size_t N, Shape symmetry = Shape::Symmetric, T alpha = T(), T beta = T())
				{
					if (N == 0)
						return;
					T K = T(symmetry == Shape::Periodic ? N : N - 1);
					T offset = symmetry == Shape::DFTEven ? (T)(0.5 / K) : 0;
					for (std::size_t n = 0; n < N; ++n)
						w[n] = (T)1 - std::abs(((n + offset) - (K) * (T)0.5) / (N * (T)0.5));
				}

				template<typename T, class InOutVector>
				static T scale(const InOutVector & w, std::size_t N, Shape symmetry = Shape::Symmetric, T alpha = T(), T beta = T())
				{
					return 2;
				}

				static const bool hasFiniteDFT = false;
			};

			template<>
			class Generator<WindowTypes::Welch>
			{
			public:
				template<typename T, class InOutVector>
				static void generate(InOutVector & w, std::size_t N, Shape symmetry = Shape::Symmetric, T alpha = T(), T beta = T())
				{
					if (N == 0)
						return;
					T K = T(symmetry == Shape::Periodic ? N : N - 1);
					T offset = symmetry == Shape::DFTEven ? (T)(0.5 / K) : 0;
					for (std::size_t n = 0; n < N; ++n)
						w[n] = 1 - Math::square(((n + offset) - K * (T)0.5) / (N * (T)0.5));
				}

				template<typename T, class InOutVector>
				static T scale(const InOutVector & w, std::size_t N, Shape symmetry = Shape::Symmetric, T alpha = T(), T beta = T())
				{
					return naiveWindowScale<T>(w, N);
				}

				static const bool hasFiniteDFT = false;
			};

			template<>
			class Generator<WindowTypes::Parzen>
			{
			public:
				template<typename T, class InOutVector>
				static void generate(InOutVector & w, std::size_t N, Shape symmetry = Shape::Symmetric, T alpha = T(), T beta = T())
				{
					if (N == 0)
						return;
					T K = T(symmetry == Shape::Periodic ? N : N - 1);
					T offset = symmetry == Shape::DFTEven ? (T)(0.5 / K) : 0;
					for (std::size_t i = 0; i < N; ++i)
					{
						T n = (i + offset) - N / 2;
						T an = std::abs(n);
						if (an > (N / 4))
						{
							w[i] = 2 * Math::cube(1 - 2 * an / K);
						}
						else
						{
							w[i] = 1 - 6 * Math::square(2 * an / K) + 6 * Math::cube(2 * an / K);
						}
					}

				}

				template<typename T, class InOutVector>
				static T scale(const InOutVector & output, std::size_t N, Shape symmetry = Shape::Symmetric, T alpha = T(), T beta = T())
				{
					switch (symmetry)
					{
						case Shape::Symmetric: return (T)2.7089944223825162;
						case Shape::Periodic: return (T)2.6666666666666665;
						case Shape::DFTEven: return (T)2.7089948809620541;
						default:
							return 0;
					}
				}
				static const bool hasFiniteDFT = false;
			};

			template<>
			class Generator<WindowTypes::DolphChebyshev>
			{
			public:
				template<typename T, class InOutVector>
				static void generate(InOutVector & w, std::size_t N, Shape symmetry = Shape::Symmetric, T alpha = T(), T beta = T())
				{
					if (N == 0)
						return;
					/*// http://practicalcryptography.com/miscellaneous/machine-learning/implementing-dolph-chebyshev-window/
					T K = (T)N;
					std::size_t nn, i;
					T M, n, sum = 0, max = 0;
					alpha = std::max((T)0, alpha); // negative values are not solvable
					T tg = std::pow((T)10, alpha / (T)20);  // 1/r term [2], 10^gamma [2]
					T x0 = std::cosh(((T)1.0 / (K - 1)) * std::acosh(tg));

					M = (T)((N - 1) / 2);

					// handle even length windows
					// periodic cases doesn't really work with expected transform results,
					// but the window is automatically DFTEven when needed.
					if ((N & 1) == 0)
						M += (T)0.5;

					for (nn = 0; nn < (N / 2 + 1); nn++)
					{
						n = nn - M;
						sum = 0;
						for (i = 1; i <= M; i++)
						{
							sum += Math::cheby_poly(K - 1, x0 * std::cos((T)M_PI * i / N)) * std::cos(2 * n * (T)M_PI * i / N);
						}
						w[nn] = tg + 2 * sum;
						w[N - nn - 1] = w[nn];
						max = std::max(max, w[nn]);
					}

					// normalise everything
					for (nn = 0; nn<N; nn++)
						w[nn] /= max;
					return; */

					// the previous method is n^2 slow and unoptimized.
					// until it is fixed, we'll just use the ultraspherical window, as 
					// dolph-chebyshev windows are a special case of ultraspherical windows, with mu = 0

					double xmu = 0;
					octave::signal::ultraspherical_window((int)N, &w[0], 0, std::abs(alpha), alpha < 0 ? octave::signal::uswpt_AttFirst : octave::signal::uswpt_AttLast, 0, &xmu);


				}

				template<typename T, class InOutVector>
				static T scale(const InOutVector & w, std::size_t N, Shape symmetry = Shape::Symmetric, T alpha = T(), T beta = T())
				{
					return naiveWindowScale<T>(w, N);
				}

				static const bool hasFiniteDFT = false;
			};

			template<>
			class Generator<WindowTypes::Gaussian>
			{
			public:
				template<typename T, class InOutVector>
				static void generate(InOutVector & w, std::size_t N, Shape symmetry = Shape::Symmetric, T alpha = T(), T beta = T())
				{
					if (N == 0)
						return;
					beta = std::max(std::min(beta, (T)0.5), (T)0.001);
					T K = T(symmetry == Shape::Periodic ? N : N - 1);
					T offset = symmetry == Shape::DFTEven ? (T)(0.5 / K) : 0;
					for (std::size_t n = 0; n < N; ++n)
						w[n] = std::exp(T(-0.5) * cpl::Math::square((((n + offset) - K * T(0.5)) / (beta * N * T(0.5)))));
				}

				template<typename T, class InOutVector>
				static T scale(const InOutVector & w, std::size_t N, Shape symmetry = Shape::Symmetric, T alpha = T(), T beta = T())
				{
					return naiveWindowScale<T>(w, N);
				}

				static const bool hasFiniteDFT = false;
			};

			template<>
			class Generator<WindowTypes::Kaiser>
			{
			public:
				template<typename T, class InOutVector, bool alphaIsScaledBy20 = true>
				static void generate(InOutVector & w, std::size_t N, Shape symmetry = Shape::Symmetric, T alpha = T(), T beta = T())
				{
					if (N == 0)
						return;
					T K = T(symmetry == Shape::Periodic ? N : N - 1);
					T offset = symmetry == Shape::DFTEven ? (T)(0.5 / K) : 0;

					if (alphaIsScaledBy20)
						alpha /= 20;
					T malpha = T(M_PI * alpha);
					T denom = Math::i0(malpha);
					for (std::size_t n = 0; n < N; ++n)
					{
						auto phase = cpl::Math::square(((T)(2 * n + offset)) / K - 1);
						if (phase > (T)1) // handles negative input for sqrt(), avoiding nan values.
							phase -= (phase - 1) * 2;

						w[n] = Math::i0(malpha * std::sqrt(1 - phase)) / denom;
					}

				}

				template<typename T, class InOutVector>
				static T scale(const InOutVector & w, std::size_t N, Shape symmetry = Shape::Symmetric, T alpha = T(3), T beta = T())
				{
					return naiveWindowScale<T>(w, N);
				}

				static const bool hasFiniteDFT = false;
			};

			template<>
			class Generator<WindowTypes::Ultraspherical>
			{
			public:
				template<typename T, class InOutVector>
				static void generate(InOutVector & w, std::size_t N, Shape symmetry = Shape::Symmetric, T alpha = T(), T beta = T())
				{
					if (N == 0)
						return;
					double xmu = 0;
					beta = Math::confineTo(beta, -1.5, 6); // rough empirical testing, analytical success depends on N as well, but this should produce valid windows for every N
					octave::signal::ultraspherical_window((int)N, &w[0], beta, std::abs(alpha), alpha < 0 ? octave::signal::uswpt_AttFirst : octave::signal::uswpt_AttLast, 0, &xmu);
				}

				template<typename T, class InOutVector>
				static T scale(const InOutVector & w, std::size_t N, Shape symmetry = Shape::Symmetric, T alpha = T(3), T beta = T())
				{
					return naiveWindowScale<T>(w, N);
				}

				static const bool hasFiniteDFT = false;
			};

			template<>
			class Generator<WindowTypes::Poisson>
			{
			public:
				template<typename T, class InOutVector>
				static void generate(InOutVector & w, std::size_t N, Shape symmetry = Shape::Symmetric, T alpha = T(), T beta = T())
				{
					if (N == 0)
						return;
					T K = T(symmetry == Shape::Periodic ? N : N - 1);
					T offset = symmetry == Shape::DFTEven ? (T)(0.5 / K) : 0;
					T rtau = 1 / (T(N * 0.5) * (T)8.69 / alpha);

					for (std::size_t n = 0; n < N; ++n)
						w[n] = std::exp(-rtau * std::abs((n + offset) - K * T(0.5)));
				}

				template<typename T, class InOutVector>
				static T scale(const InOutVector & w, std::size_t N, Shape symmetry = Shape::Symmetric, T alpha = T(), T beta = T())
				{
					return naiveWindowScale<T>(w, N);
				}

				static const bool hasFiniteDFT = false;
			};

			template<>
			class Generator<WindowTypes::HannPoisson>
			{
			public:
				template<typename T, class InOutVector>
				static void generate(InOutVector & w, std::size_t N, Shape symmetry = Shape::Symmetric, T alpha = T(), T beta = T())
				{
					using consts = simd::consts<T>;
					if (N == 0)
						return;
					T K = T(symmetry == Shape::Periodic ? N : N - 1);
					T offset = symmetry == Shape::DFTEven ? consts::half / K : 0;
					T rtau = 1 / (N * consts::half * (T)8.69 / alpha);

					for (std::size_t n = 0; n < N; ++n)
						w[n] = (consts::half - consts::half * std::cos(offset + n * consts::tau / K)) * std::exp(-rtau * std::abs((n + offset) - K * consts::half));
				}

				template<typename T, class InOutVector>
				static T scale(const InOutVector & w, std::size_t N, Shape symmetry = Shape::Symmetric, T alpha = T(), T beta = T())
				{
					return naiveWindowScale<T>(w, N);
				}

				static const bool hasFiniteDFT = false;
			};

			template<>
			class Generator<WindowTypes::Lanczos>
			{
			public:
				template<typename T, class InOutVector>
				static void generate(InOutVector & w, std::size_t N, Shape symmetry = Shape::Symmetric, T alpha = T(), T beta = T())
				{
					using consts = simd::consts<T>;

					if (N == 0)
						return;
					T K = T(symmetry == Shape::Periodic ? N : N - 1);
					T offset = symmetry == Shape::DFTEven ? consts::half / K : 0;

					for (std::size_t n = 0; n < N; ++n)
						w[n] = scresponse<true>((offset + consts::two * n / K) - 1);
				}

				template<typename T, class InOutVector>
				static T scale(const InOutVector & w, std::size_t N, Shape symmetry = Shape::Symmetric, T alpha = T(), T beta = T())
				{
					return naiveWindowScale<T>(w, N);
				}

				static const bool hasFiniteDFT = false;
			};


			template<typename T, WindowTypes wclass>
			class DFTCoeffs;


			template<typename T> class DFTCoeffs<T, WindowTypes::Rectangular> {
			public: static const T coeffs[1];
			};

			template<typename T> class DFTCoeffs<T, WindowTypes::Hann> {
			public: static const T coeffs[3];
			};

			template<typename T> class DFTCoeffs<T, WindowTypes::Hamming> {
			public: static const T coeffs[3];
			};

			template<typename T> class DFTCoeffs<T, WindowTypes::Blackman> {
			public: static const T coeffs[5];
			};

			template<typename T> class DFTCoeffs<T, WindowTypes::ExactBlackman> {
			public: static const T coeffs[5];
			};

			template<typename T> class DFTCoeffs<T, WindowTypes::BlackmanHarris> {
			public: static const T coeffs[7];
			};

			template<typename T> class DFTCoeffs<T, WindowTypes::BlackmanNuttall> {
			public: static const T coeffs[7];
			};

			template<typename T> class DFTCoeffs<T, WindowTypes::Nuttall> {
			public: static const T coeffs[7];
			};

			template<typename T> class DFTCoeffs<T, WindowTypes::FlatTop> {
			public: static const T coeffs[9];
			};


			template<typename T>
			const T DFTCoeffs<T, WindowTypes::Rectangular>::coeffs[1] =
			{
				static_cast<T>(1)
			};


			template<typename T>
			const T DFTCoeffs<T, WindowTypes::Hann>::coeffs[3] =
			{
				static_cast<T>(-0.5),
				static_cast<T>(1),
				static_cast<T>(-0.5)
			};

			template<typename T>
			const T DFTCoeffs<T, WindowTypes::Hamming>::coeffs[3] =
			{
				static_cast<T>(-0.42),
				static_cast<T>(1),
				static_cast<T>(-0.42)
			};

			template<typename T>
			const T DFTCoeffs<T, WindowTypes::Blackman>::coeffs[5] =
			{
				static_cast<T>(0.095238095238095219),
				static_cast<T>(-0.59523809523809523),
				static_cast<T>(1),
				static_cast<T>(-0.59523809523809523),
				static_cast<T>(0.095238095238095219)
			};

			template<typename T>
			const T DFTCoeffs<T, WindowTypes::ExactBlackman>::coeffs[5] =
			{
				static_cast<T>(0.090073066263542487),
				static_cast<T>(-0.58201058201058220),
				static_cast<T>(1),
				static_cast<T>(-0.58201058201058220),
				static_cast<T>(0.090073066263542487)
			};

			template<typename T>
			const T DFTCoeffs<T, WindowTypes::BlackmanNuttall>::coeffs[7] =
			{
				static_cast<T>(-0.014633704263055934),
				static_cast<T>(0.18785244810041429),
				static_cast<T>(-0.67271981911090739),
				static_cast<T>(1),
				static_cast<T>(-0.67271981911090739),
				static_cast<T>(0.18785244810041429),
				static_cast<T>(-0.014633704263055934)
			};

			template<typename T>
			const T DFTCoeffs<T, WindowTypes::BlackmanHarris>::coeffs[7] =
			{
				static_cast<T>(-0.016278745644599243),
				static_cast<T>(0.19690592334494775),
				static_cast<T>(-0.68054355400696875),
				static_cast<T>(1),
				static_cast<T>(-0.68054355400696875),
				static_cast<T>(0.19690592334494775),
				static_cast<T>(-0.016278745644599243)
			};


			template<typename T>
			const T DFTCoeffs<T, WindowTypes::Nuttall>::coeffs[7] =
			{
				static_cast<T>(-0.017714915338085432),
				static_cast<T>(0.20270513368262466),
				static_cast<T>(-0.68499134267275319),
				static_cast<T>(1),
				static_cast<T>(-0.68499134267275319),
				static_cast<T>(0.20270513368262466),
				static_cast<T>(-0.017714915338085432)
			};

			template<typename T>
			const T DFTCoeffs<T, WindowTypes::FlatTop>::coeffs[9] =
			{
				static_cast<T>(0.014),
				static_cast<T>(-0.194),
				static_cast<T>(0.645),
				static_cast<T>(-0.965),
				static_cast<T>(1),
				static_cast<T>(-0.965),
				static_cast<T>(0.645),
				static_cast<T>(-0.194),
				static_cast<T>(0.014)
			};
		};


		/// <summary>
		/// Calculates the specified window coefficients, and stores them in the InOutVector, that is assumed to be a random access indexable buffer containing N elements of T.
		/// Deterministic and wait-free in runtime
		///	</summary>
		/// <param name="alpha">
		/// The alpha parameter generally specifies the attenuation of the sidelobes for windows that support it: 100 will set sidelobes to -100dB.
		/// Specifying negative values will alter the spectrum slope sign of the attenuation, for windows that support it.
		/// </param>
		/// <param name="beta">
		/// The beta parameter alters the shape of the transform. 
		/// For gaussian windows, it specifics the sigma parameter (normal distribution) whoose domain is [0.5, 0]
		/// For ultraspherical windows, it specifies the slope coefficient of the sidelobes, where negative values give falling lobes towards the main lobe,
		/// reversed for positive values. Domain is [-1.5, 6[
		/// 
		/// Note, the domain will always be restricted to valid values.
		/// </param>
		template<WindowTypes wclass, typename T, class InOutVector>
		void windowFunction(InOutVector & w, std::size_t N, Windows::Shape symmetry = Windows::Shape::Symmetric, T alpha = T(), T beta = T())
		{
			return Windows::Generator<wclass>::generate(w, N, symmetry, alpha, beta);
		}

		/// <summary>
		/// Calculates the specified window coefficients, and stores them in the InOutVector, that is assumed to be a random access indexable buffer containing N elements of T.
		///	</summary>
		/// <param name="alpha">
		/// The alpha parameter generally specifies the attenuation of the sidelobes for windows that support it: 100 will set sidelobes to -100dB.
		/// Specifying negative values will alter the spectrum slope sign of the attenuation, for windows that support it.
		/// </param>
		/// <param name="beta">
		/// The beta parameter alters the shape of the transform. 
		/// For gaussian windows, it specifics the sigma parameter (normal distribution) whoose domain is [0, 0.5]
		/// For ultraspherical windows, it specifies the slope coefficient of the sidelobes, where negative values give falling lobes towards the main lobe,
		/// reversed for positive values. Domain is [-1.5, 6[
		/// 
		/// Note, the domain will always be restricted to valid values.
		/// </param>
		template<typename T, class InOutVector>
		typename std::enable_if<!std::is_integral<T>::value, void>::type
			windowFunction(WindowTypes wclass, InOutVector & w, std::size_t N, Windows::Shape symmetry = Windows::Shape::Symmetric, T alpha = T(), T beta = T())
		{
			typedef WindowTypes W;

			switch (wclass)
			{
				case W::Rectangular:
					return windowFunction<W::Rectangular>(w, N, symmetry, alpha, beta); break;
				case W::Hann:
					return windowFunction<W::Hann>(w, N, symmetry, alpha, beta); break;
				case W::Hamming:
					return windowFunction<W::Hamming>(w, N, symmetry, alpha, beta); break;
				case W::FlatTop:
					return windowFunction<W::FlatTop>(w, N, symmetry, alpha, beta); break;
				case W::Blackman:
					return windowFunction<W::Blackman>(w, N, symmetry, alpha, beta); break;
				case W::ExactBlackman:
					return windowFunction<W::ExactBlackman>(w, N, symmetry, alpha, beta); break;
				case W::Triangular:
					return windowFunction<W::Triangular>(w, N, symmetry, alpha, beta); break;
				case W::Parzen:
					return windowFunction<W::Parzen>(w, N, symmetry, alpha, beta); break;
				case W::Nuttall:
					return windowFunction<W::Nuttall>(w, N, symmetry, alpha, beta); break;
				case W::BlackmanNuttall:
					return windowFunction<W::BlackmanNuttall>(w, N, symmetry, alpha, beta); break;
				case W::BlackmanHarris:
					return windowFunction<W::BlackmanHarris>(w, N, symmetry, alpha, beta); break;
				case W::Gaussian:
					return windowFunction<W::Gaussian>(w, N, symmetry, alpha, beta); break;
				case W::DolphChebyshev:
					return windowFunction<W::DolphChebyshev>(w, N, symmetry, alpha, beta); break;
				case W::Kaiser:
					return windowFunction<W::Kaiser>(w, N, symmetry, alpha, beta); break;
				case W::Ultraspherical:
					return windowFunction<W::Ultraspherical>(w, N, symmetry, alpha, beta); break;
				case W::Welch:
					return windowFunction<W::Welch>(w, N, symmetry, alpha, beta); break;
				case W::Poisson:
					return windowFunction<W::Poisson>(w, N, symmetry, alpha, beta); break;
				case W::HannPoisson:
					return windowFunction<W::HannPoisson>(w, N, symmetry, alpha, beta); break;
				case W::Lanczos:
					return windowFunction<W::Lanczos>(w, N, symmetry, alpha, beta); break;
				case W::End:

				default:
					break;
			}
		}
		/// <summary>
		/// Returns the scaling coefficient the specified window will produce.
		/// Scale a transform by this amount to preserve the original peak power in a transformed spectrum.
		/// For parameters, see windowFunction.
		/// Deterministic and wait-free in runtime
		/// </summary>
		template<WindowTypes wclass, typename T, class InOutVector>
		T windowScale(InOutVector & w, std::size_t N, Windows::Shape symmetry = Windows::Shape::Symmetric, T alpha = T(), T beta = T())
		{
			return Windows::Generator<wclass>::scale(w, N, symmetry, alpha, beta);
		}

		/// <summary>
		/// Returns the scaling coefficient the specified window will produce.
		/// Scale a transform by this amount to preserve the original peak power in a transformed spectrum.
		/// For parameters, see windowFunction.
		/// Deterministic and wait-free in runtime.
		/// </summary>
		template<typename T, class InOutVector>
		typename std::enable_if<!std::is_integral<T>::value, T>::type
			windowScale(WindowTypes wclass, InOutVector & w, std::size_t N, Windows::Shape symmetry = Windows::Shape::Symmetric, T alpha = T(), T beta = T())
		{
			typedef WindowTypes W;

			switch (wclass)
			{
				case W::Rectangular:
					return windowScale<W::Rectangular>(w, N, symmetry, alpha, beta); break;
				case W::Hann:
					return windowScale<W::Hann>(w, N, symmetry, alpha, beta); break;
				case W::Hamming:
					return windowScale<W::Hamming>(w, N, symmetry, alpha, beta); break;
				case W::FlatTop:
					return windowScale<W::FlatTop>(w, N, symmetry, alpha, beta); break;
				case W::Blackman:
					return windowScale<W::Blackman>(w, N, symmetry, alpha, beta); break;
				case W::ExactBlackman:
					return windowScale<W::ExactBlackman>(w, N, symmetry, alpha, beta); break;
				case W::Triangular:
					return windowScale<W::Triangular>(w, N, symmetry, alpha, beta); break;
				case W::Parzen:
					return windowScale<W::Parzen>(w, N, symmetry, alpha, beta); break;
				case W::Nuttall:
					return windowScale<W::Nuttall>(w, N, symmetry, alpha, beta); break;
				case W::BlackmanNuttall:
					return windowScale<W::BlackmanNuttall>(w, N, symmetry, alpha, beta); break;
				case W::BlackmanHarris:
					return windowScale<W::BlackmanHarris>(w, N, symmetry, alpha, beta); break;
				case W::Gaussian:
					return windowScale<W::Gaussian>(w, N, symmetry, alpha, beta); break;
				case W::DolphChebyshev:
					return windowScale<W::DolphChebyshev>(w, N, symmetry, alpha, beta); break;
				case W::Kaiser:
					return windowScale<W::Kaiser>(w, N, symmetry, alpha, beta); break;
				case W::Ultraspherical:
					return windowScale<W::Ultraspherical>(w, N, symmetry, alpha, beta); break;
				case W::Welch:
					return windowScale<W::Welch>(w, N, symmetry, alpha, beta); break;
				case W::Poisson:
					return windowScale<W::Poisson>(w, N, symmetry, alpha, beta); break;
				case W::HannPoisson:
					return windowScale<W::HannPoisson>(w, N, symmetry, alpha, beta); break;
				case W::Lanczos:
					return windowScale<W::Lanczos>(w, N, symmetry, alpha, beta); break;
				case W::End:

				default:
					return 0;
					break;
			}
		}

		/// <summary>
		/// Calculates the scalloping loss for the specified window, where the worst case loss for fourier transforms is at binOffset = 0.5.
		/// Higher values emulate non-evenly spaced filter banks.
		/// Non-deterministic in runtime (decimalDigitsPrecision scales runtime by O(n)).
		/// </summary>
		template<typename T>
		T windowScallopLoss(dsp::WindowTypes winType, int decimalDigitsPrecision, T binOffset = T(0.5), dsp::Windows::Shape shape = dsp::Windows::Shape::Symmetric, T alpha = T(), T beta = T())
		{
			switch (winType)
			{
				case WindowTypes::Rectangular:
					return scresponse<true>(binOffset);
					break;
				case WindowTypes::Hann:
					break;
				case WindowTypes::Hamming:
					break;
				case WindowTypes::FlatTop:
					break;
				case WindowTypes::Blackman:
					break;
				case WindowTypes::ExactBlackman:
					break;
				case WindowTypes::Triangular:
					break;
				case WindowTypes::Parzen:
					break;
				case WindowTypes::Nuttall:
					break;
				case WindowTypes::BlackmanNuttall:
					break;
				case WindowTypes::BlackmanHarris:
					break;
				case WindowTypes::Gaussian:
					break;
				case WindowTypes::DolphChebyshev:
					break;
				case WindowTypes::Kaiser:
					break;
				case WindowTypes::Ultraspherical:
					break;
				case WindowTypes::Welch:
					break;
				case WindowTypes::Poisson:
					break;
				case WindowTypes::HannPoisson:
					break;
				case WindowTypes::Lanczos:
					break;
				case WindowTypes::End:
				default:
					return 0.5;
			}

			auto N = 2 << (5 + decimalDigitsPrecision);

			aligned_vector<T, 32> win(N);

			T sum = 0;
			std::complex<T> csum;

			windowFunction<T>(winType, win, N, shape, alpha, beta);

			// compute SL sums:
			// https://www.utdallas.edu/~cpb021000/EE%204361/Great%20DSP%20Papers/Harris%20on%20Windows.pdf
			for (int i = 0; i < N; ++i)
			{
				csum += win[i] * std::polar(1.0, 2 * M_PI * binOffset / N * i);
				sum += win[i];
			}

			return std::abs(csum) / sum;
		}

		/// <summary>
		/// Calculates the scalloping loss for the specified window, where the worst case loss for fourier transforms is at binOffset = 0.5.
		/// Higher values emulate non-evenly spaced filter banks.
		/// Deterministic and wait-free in runtime, O(N)
		/// </summary>
		template<class InVector, typename T>
		T windowScallopLoss(const InVector & w, std::size_t N, T binOffset = T(0.5))
		{
			T sum = 0;
			std::complex<T> csum;

			// compute SL sums:
			// https://www.utdallas.edu/~cpb021000/EE%204361/Great%20DSP%20Papers/Harris%20on%20Windows.pdf
			for (int i = 0; i < N; ++i)
			{
				csum += w[i] * std::polar(1.0, 2 * M_PI * binOffset / N * i);
				sum += w[i];
			}

			return std::abs(csum) / sum;
		}


		/// <summary>
		/// Returns whether the specified window has a finite amount of non-zero fourier series.
		/// Deterministic and wait-free in runtime.
		/// </summary>
		template<WindowTypes wclass>
		inline constexpr bool windowHasFiniteDFT()
		{
			return Windows::Generator<wclass>::hasFiniteDFT;
		}

		/// <summary>
		/// Returns whether the specified window has a finite amount of non-zero fourier series.
		/// Deterministic and wait-free in runtime
		/// </summary>
		inline bool windowHasFiniteDFT(WindowTypes wclass)
		{
			typedef WindowTypes W;

			switch (wclass)
			{
				case W::Rectangular:
				case W::Hann:
				case W::Hamming:
				case W::FlatTop:
				case W::Blackman:
				case W::ExactBlackman:
				case W::Nuttall:
				case W::BlackmanNuttall:
				case W::BlackmanHarris:
					return true;
				default:
					return false;
			}
		}

		/// <summary>
		/// Returns a pair with a pointer+size to a finite fourier series of the transformed window.
		/// Deterministic and wait-free in runtime.
		/// </summary>
		template<typename T, WindowTypes wclass>
		std::pair<const T *, std::size_t> windowCoefficients()
		{
			static_assert(Windows::Generator<wclass>::hasFiniteDFT, "Window needs to have a deterministic coefficient sequence!");
			return std::make_pair(Windows::DFTCoeffs<T, wclass>::coeffs, std::extent<decltype(Windows::DFTCoeffs<T, wclass>::coeffs)>::value);
		}

		/// <summary>
		/// Returns a pair with a pointer+size to a finite fourier series of the transformed window.
		/// A rectangular window is returned, if none exists.
		/// Deterministic and wait-free in runtime.
		/// </summary>
		template<typename T>
		std::pair<const T *, std::size_t> windowCoefficients(WindowTypes wclass)
		{
			typedef WindowTypes W;
			switch (wclass)
			{
				case W::Hann: return windowCoefficients<T, W::Hann>();
				case W::Hamming: return windowCoefficients<T, W::Hamming>();
				case W::FlatTop: return windowCoefficients<T, W::FlatTop>();
				case W::Blackman: return windowCoefficients<T, W::Blackman>();
				case W::ExactBlackman: return windowCoefficients<T, W::ExactBlackman>();
				case W::Nuttall: return windowCoefficients<T, W::Nuttall>();
				case W::BlackmanNuttall: return windowCoefficients<T, W::BlackmanNuttall>();
				case W::BlackmanHarris: return windowCoefficients<T, W::BlackmanHarris>();
				default:
					return windowCoefficients<T, W::Rectangular>();
			}
		}
	};
};
#endif