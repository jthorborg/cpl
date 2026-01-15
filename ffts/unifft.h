/*************************************************************************************

	cpl - cross-platform library - v. 0.1.0.

	Copyright (C) 2022 Janus Lynggaard Thorborg (www.jthorborg.com)

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

	file:unifft.h

		A uniform API fast fourier transform.

*************************************************************************************/

#ifndef CPL_UNIFFT_H
#define CPL_UNIFFT_H

#include <memory>
#include "../lib/uarray.h"
#include "../lib/AlignedAllocator.h"
#include "pffft/pffft.h"
#include "pffft/pffft_double.h"
#include <mutex>
#include <map>

namespace cpl
{
	namespace dsp
	{
		namespace detail
		{
			template<typename T>
			struct uni_traits;

			template<> struct uni_traits<float>
			{ 
				using type = float; 
				using setup = PFFFT_Setup; 

				static int min_size(pffft_transform_t kind) { return pffft_min_fft_size(kind); }
				static setup* create(int n, pffft_transform_t kind) { return pffft_new_setup(n, kind); }

				static void transform_ordered(setup* s, const float* input, float* output, float* work, pffft_direction_t direction)
				{
					pffft_transform_ordered(s, input, output, work, direction);
				}

				struct deleter
				{
					void operator()(setup* s) { pffft_destroy_setup(s); }
				};
			};

			template<> struct uni_traits<double> 
			{ 
				using type = double;
				using setup = PFFFTD_Setup; 

				static int min_size(pffft_transform_t kind) { return pffftd_min_fft_size(kind); }
				static setup* create(int n, pffft_transform_t kind) { return pffftd_new_setup(n, kind); }

				static void transform_ordered(setup* s, const double* input, double* output, double* work, pffft_direction_t direction)
				{
					pffftd_transform_ordered(s, input, output, work, direction);
				}

				struct deleter
				{
					void operator()(setup* s) { pffftd_destroy_setup(s); }
				};
			};

			template<> struct uni_traits<std::complex<float>> : uni_traits<float> { };
			template<> struct uni_traits<std::complex<double>> : uni_traits<double> { };

		}

		template<typename T>
		class UniFFT
		{
			typedef typename detail::uni_traits<T> traits;
			typedef typename traits::setup setup;
			typedef typename std::unique_ptr<setup, typename traits::deleter> scoped_setup;

		public:

			typedef typename traits::type Scalar;
			typedef std::complex<Scalar> Complex;
			static constexpr bool is_complex = std::is_same_v<T, Complex>;
			static constexpr std::size_t factor = is_complex ? 1 : 2;

			UniFFT(std::size_t n)
				: sharedSetup(getSetup(n))
				, size(n)
			{

			}

			UniFFT()
				: sharedSetup(getSetup(minSize()))
				, size(minSize())
			{

			}

			void forward(uarray<const T> input, uarray<Complex> output, uarray<Complex> work) const
			{
				CPL_RUNTIME_ASSERTION(input.size() == output.size());
				CPL_RUNTIME_ASSERTION(input.size() == size);
				CPL_RUNTIME_ASSERTION(work.size() == size);

				traits::transform_ordered(
					const_cast<setup*>(sharedSetup), 
					input.template reinterpret<Scalar>().data(),
					output.template reinterpret<Scalar>().data(),
					work.template reinterpret<Scalar>().data(),
					PFFFT_FORWARD
				);
			}

			template<bool Scale = true>
			void inverse(uarray<const Complex> input, uarray<T> output, uarray<Complex> work) const
			{
				CPL_RUNTIME_ASSERTION(input.size() == output.size());
				CPL_RUNTIME_ASSERTION(input.size() == size);
				CPL_RUNTIME_ASSERTION(work.size() == size);

				if (Scale)
				{
					Scalar scale = Scalar(1) / input.size();
					auto cout = output.template reinterpret<Complex>();

					for (std::size_t i = 0; i < size / 4; i += 4)
					{
						cout[i + 0] = input[i + 0] * scale;
						cout[i + 1] = input[i + 1] * scale;
						cout[i + 2] = input[i + 2] * scale;
						cout[i + 3] = input[i + 3] * scale;
					}

					traits::transform_ordered(
						const_cast<setup*>(sharedSetup), 
						cout.template reinterpret<Scalar>().data(),
						cout.template reinterpret<Scalar>().data(),
						work.template reinterpret<Scalar>().data(),
						PFFFT_BACKWARD
					);
				}
				else
				{
					traits::transform_ordered(
						const_cast<setup*>(sharedSetup), 
						input.template reinterpret<Scalar>().data(),
						output.template reinterpret<Scalar>().data(),
						work.template reinterpret<Scalar>().data(),
						PFFFT_BACKWARD
					);
				}
			}

			static std::size_t minSize()
			{
				return traits::min_size(getType());
			}

		private:

			static constexpr pffft_transform_t getType()
			{
				return is_complex ? PFFFT_COMPLEX : PFFFT_REAL;
			}

			static const setup* getSetup(std::size_t n)
			{
				static std::mutex mutex;
				static std::map<std::size_t, scoped_setup> setups;

				std::lock_guard<std::mutex> lock(mutex);

				if (auto it = setups.find(n); it != setups.end())
				{
					return it->second.get();
				}

				auto scoped = scoped_setup(traits::create(static_cast<int>(n), getType()));
				auto pointer = scoped.get();

				setups[n] = std::move(scoped);

				return pointer;
			}

			const setup* sharedSetup;
			std::size_t size;
		};
	}

}

#endif
