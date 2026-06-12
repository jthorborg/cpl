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

	file:SubpixelRendering.h

		Utilities and types needed for rendering subpixel graphics.

 *************************************************************************************/

#ifndef CPL_SUBPIXELRENDERING_H
#define CPL_SUBPIXELRENDERING_H
#include <cstdint>
#include "../Types.h"
#include "DisplayOrientation.h"
#include <limits>

namespace cpl
{
	namespace rendering
	{

		enum class LCDMatrixOrientation
		{
			RGB,
			BGR,
			GBR,
			RBG
		};

		inline LCDMatrixOrientation InvertedMatrixOrientation(LCDMatrixOrientation o)
		{
			if (o == LCDMatrixOrientation::RGB)
				return LCDMatrixOrientation::BGR;
			else if (o == LCDMatrixOrientation::BGR)
				return LCDMatrixOrientation::RGB;
			else if (o == LCDMatrixOrientation::GBR)
				return LCDMatrixOrientation::RGB;
			else if (o == LCDMatrixOrientation::RBG)
				return LCDMatrixOrientation::GBR;

			// throw here?
			return LCDMatrixOrientation::BGR;
		}

		/*
			if you want to access a certain sub pixel on a display
			with a certain layout using a kernel of a certain size,
			through a software bitmap with a certain endianness,
			the map found in this struct remaps an offset around zero to
			the correct offset (with modulo).

		*/
		template<LCDMatrixOrientation layout, bool littleEndian>
		struct RGBToDisplayPixelMap;


		template<> struct RGBToDisplayPixelMap < LCDMatrixOrientation::RGB, false>
		{
			/* substitute to constexpr when compiler support is added
				{
					0,
					1,
					2
				};
			*/
			static const int map[3];
		};

		template<> struct RGBToDisplayPixelMap < LCDMatrixOrientation::RGB, true>
		{
			/* substitute to constexpr when compiler support is added
				{
					2,
					1,
					0
				};
			*/
			static const int map[3];
		};

		template<> struct RGBToDisplayPixelMap < LCDMatrixOrientation::BGR, false>
		{
			/* substitute to constexpr when compiler support is added
				{
					2,
					1,
					0
				};
			*/
			static const int map[3];
		};

		template<> struct RGBToDisplayPixelMap < LCDMatrixOrientation::BGR, true>
		{
			/* substitute to constexpr when compiler support is added
				{
					0,
					1,
					2
				};
			*/
			static const int map[3];
		};

		template<> struct RGBToDisplayPixelMap < LCDMatrixOrientation::GBR, false>
		{
			/* substitute to constexpr when compiler support is added
				{
					2,
					0,
					1
				};
			*/
			static const int map[3];
		};

		template<> struct RGBToDisplayPixelMap < LCDMatrixOrientation::GBR, true>
		{
			/* substitute to constexpr when compiler support is added
				{
					1,
					0,
					2
				};
			*/
			static const int map[3];
		};

		template<> struct RGBToDisplayPixelMap < LCDMatrixOrientation::RBG, false>
		{
			/* substitute to constexpr when compiler support is added
				{
					0,
					2,
					1
				};
			*/
			static const int map[3];
		};

		template<> struct RGBToDisplayPixelMap < LCDMatrixOrientation::RBG, true>
		{
			/* substitute to constexpr when compiler support is added
				{
					1,
					2,
					0
				};
			*/
			static const int map[3];
		};

		/* ----- WEIGHTMAPs -----------
			This struct must have the following typedefs:
				Pixel -> the type of pixel (often unsigned char)
				IntType -> the general integer type

			The following members:
				static const centerIndex -> alphaMap[centerIndex] is the center of the map
				static const size -> the size of the alphaMap
				Pixel alphaMap[size] -> the actual alpha map.

			the following methods:
				inline void addIntensityToMap(std::uint_fast16_t alphaLevel) noexcept
					-> where alphaLevel is a 16-bit level of intensity.

				inline void clearAndShuffle(IntType numSteps) noexcept
					-> clears a certain number of steps in the map.

				inline void clear() noexcept
					-> clears the map.

			Note: all members must be public. Must have standard_layout type.

		*/
		template<int size, int denominator>
		struct WeightMap;

		template<> struct WeightMap<5, 16>
		{
			typedef std::uint8_t Pixel;
			typedef std::int_fast32_t IntType;
			std::uint8_t alphaMap[5];
			// make constexpr some day.
			//static const std::uint8_t lut[5];
			static const std::size_t centerIndex = 5 >> 1;
			static const IntType size = 5;

			WeightMap() : alphaMap() {}

			inline void addIntensityToMap(std::uint_fast16_t alphaLevel) noexcept
			{
				auto const low = static_cast<Pixel>((alphaLevel + 0x800) >> 12); // 1/16
				auto const medium = static_cast<Pixel>((alphaLevel + 0x200) >> 10); // 4/16
				auto const high = static_cast<Pixel>((alphaLevel * 96 + 0x8000) >> 16); // 6/16

				alphaMap[0] += low;
				alphaMap[1] += medium;
				alphaMap[2] += high;
				alphaMap[3] += medium;
				alphaMap[4] += low;
			}

			inline void clearAndShuffle(IntType numSteps) noexcept
			{
				// shuffle buffer around
				for (IntType i = numSteps; i < size; ++i)
				{
					alphaMap[i - numSteps] = alphaMap[i];
				}
				// clear unused alpha map entries
				for (IntType i = 0; i < numSteps; ++i)
				{
					auto index = (size - i) - 1;
					alphaMap[index] = 0;
				}

			}

			inline void clear() noexcept
			{
				alphaMap[0] = alphaMap[1] = alphaMap[2] = alphaMap[3] = alphaMap[4] = 0;
				//std::memset(alphaMap, 0, size);
			}
		};

		template<> struct WeightMap<5, 9>
		{
			typedef std::uint8_t Pixel;
			typedef std::int_fast32_t IntType;
			Pixel alphaMap[5];
			// make constexpr some day.
			//static const std::uint8_t lut[5];
			static const std::size_t centerIndex = 5 >> 1;
			static const IntType size = 5;

			WeightMap() : alphaMap() {}

			inline void addIntensityToMap(std::uint_fast16_t alphaLevel) noexcept
			{
				/*  this is actually calculated at runtime, even on release settings.
					kept here for reference.

					static const std::uint8_t weightLut5[3] =
					{
						cpl::Math::round<Pixel>(std::numeric_limits<Pixel>::max() * 1.0 / 9.0),
						cpl::Math::round<Pixel>(std::numeric_limits<Pixel>::max() * 2.0 / 9.0),
						cpl::Math::round<Pixel>(std::numeric_limits<Pixel>::max() * 3.0 / 9.0)
					};
					auto const low = cpl::Math::roundedMul(alphaLevel, weightLut5[0]);
					auto const medium = cpl::Math::roundedMul(alphaLevel, weightLut5[1]);
					auto const high = cpl::Math::roundedMul(alphaLevel, weightLut5[2]);
				*/

				auto const low = static_cast<Pixel>((static_cast<std::uint_fast32_t>(alphaLevel) * 28 + 0x8000) >> 16);
				auto const medium = static_cast<Pixel>((static_cast<std::uint_fast32_t>(alphaLevel) * 57 + 0x8000) >> 16);
				auto const high = static_cast<Pixel>((static_cast<std::uint_fast32_t>(alphaLevel) * 85 + 0x8000) >> 16);

				alphaMap[0] += low;
				alphaMap[1] += medium;
				alphaMap[2] += high;
				alphaMap[3] += medium;
				alphaMap[4] += low;
			}

			inline void clearAndShuffle(IntType numSteps) noexcept
			{
				// shuffle buffer around
				for (IntType i = numSteps; i < size; ++i)
				{
					alphaMap[i - numSteps] = alphaMap[i];
				}
				// clear unused alpha map entries
				for (IntType i = 0; i < numSteps; ++i)
				{
					alphaMap[(size - i) - 1] = 0;
				}

			}

			inline void clear() noexcept
			{
				std::memset(alphaMap, 0, sizeof(alphaMap));
			}
		};


		template<> struct WeightMap<3, 9>
		{
			typedef std::uint8_t Pixel;
			typedef std::int_fast32_t IntType;
			Pixel alphaMap[3];
			// make constexpr some day.
			//static const std::uint8_t lut[5];
			static const std::size_t centerIndex = 3 >> 1;
			static const IntType size = 3;

			WeightMap() : alphaMap() {}

			inline void addIntensityToMap(std::uint_fast16_t alphaLevel) noexcept
			{

				auto const high = static_cast<Pixel>((static_cast<std::uint_fast32_t>(alphaLevel) * 85 + 0x8000) >> 16);

				alphaMap[0] += high;
				alphaMap[1] += high;
				alphaMap[2] += high;
			}

			inline void clearAndShuffle(IntType numSteps) noexcept
			{
				// shuffle buffer around
				for (IntType i = numSteps; i < size; ++i)
				{
					alphaMap[i - numSteps] = alphaMap[i];
				}
				// clear unused alpha map entries
				for (IntType i = 0; i < numSteps; ++i)
				{
					alphaMap[(size - i) - 1] = 0;
				}

			}

			inline void clear() noexcept
			{
				std::memset(alphaMap, 0, sizeof(alphaMap));
			}
		};


		struct LutGammaScale
		{
			typedef std::uint8_t Pixel;
			// intermediate type for multiplications
			typedef std::uint16_t IntPixel;
			static const std::size_t PixelMax = std::numeric_limits<unsigned char>::max();
			static const std::size_t size = PixelMax >> 4;

			typedef std::uint_fast16_t WType;
			static const IntPixel base = PixelMax + 1;
			static const WType numDigits = (size + 1) / 4;
			// one more to ease calculations
			IntPixel lut[size + 2];

			LutGammaScale(double gamma = 1.4f)
			{

				setGamma(gamma);

			}

			void setGamma(double newGamma)
			{
				double correction = 1.0 / newGamma;
				for (std::size_t i = 0; i < (size + 1); ++i)
				{
					auto a = i / double(size + 1);
					auto expo = std::pow(a, correction);
					lut[i] = static_cast<IntPixel>(base * expo + 0.49);
				}
				lut[size + 1] = base;

			}

			template<typename T>
			inline typename std::enable_if<std::is_same<T, Pixel>::value, Pixel>::type
				operator ()(T input) const noexcept
			{
				WType ya, yb;
				WType index = input >> numDigits;
				ya = lut[index];
				yb = lut[index + 1];
				return static_cast<Pixel>(ya + ((yb - ya) * (input - (index << numDigits)) >> numDigits));
			}
		};

		/*
			This is the most correct gamma scale, adheres to standard correction values.
			Usual gamma correction values are in between 1.2 and 2.2
		*/
		template<typename T>
		struct PowerGammaScale
		{
			typedef T ValueType;

			typedef typename cpl::Types::mul_promotion<ValueType>::type IntermediateType;
			const ValueType PixelMax = std::numeric_limits<ValueType>::max();
			const double power;

			PowerGammaScale(double gammaCorrection)
				: power(1.0 / gammaCorrection)
			{

			}

			inline ValueType operator()(ValueType input) const noexcept
			{
				return static_cast<ValueType>
					(
						PixelMax * std::pow(static_cast<double>(input) / PixelMax, power) + 0.499
						);
			}

		};

		/*
			This gamma scale mimics the standard one found in juce,
			if setGammaCorrection(colour.getBrightness(), 1.6f) is set
		*/
		template<typename T>
		struct LinearGammaScale
		{
			typedef T ValueType;

			typedef typename cpl::Types::mul_promotion<ValueType>::type IntermediateType;

			const ValueType PixelMax = std::numeric_limits<ValueType>::max();

			LinearGammaScale(IntermediateType correction)
				: gammaCorrection(correction)
			{

			}

			LinearGammaScale()
				: gammaCorrection(PixelMax)
			{

			}

			inline void setGammaCorrection(float brightness, float gammaScale)
			{
				gammaCorrection = PixelMax;
				brightness -= 0.5f;

				// weird.. 1.6 is the reference, but it is a bit too high.
				// 1.2 is the usual minimum font gamma, so at gamma 1.4 were at normal.
				gammaScale = 1.6f * (gammaScale / 1.4f);

				if (brightness > 0)
				{
					gammaCorrection += static_cast<IntermediateType>(gammaScale * brightness * PixelMax);
				}
			}

			inline ValueType operator()(ValueType input) const noexcept
			{
				return static_cast<ValueType>
					(
						std::min<IntermediateType>(PixelMax, (gammaCorrection * input) >> 8)
						);
			}

			IntermediateType gammaCorrection;
		};

	}; // {} rendering
}; // {} cpl
#endif