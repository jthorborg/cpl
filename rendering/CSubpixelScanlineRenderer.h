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

	file:SubpixelScanlineRenderer.h

	A templated class that rasterises outlines from juce::EdgeTables. It downscales the
	outlines by 3, so if they're already scaled by 3, it will effectively render outlines
	subpixel-aliased (including positioning).

	Bugs:
		Outlines that are beyond -150 pixels will fuck up rendering (because of division
		round up rules around zero). Either add a larger offset or fix the algorithm.

*************************************************************************************/
#ifndef CPL_CSUBPIXELSCANLINERENDERER_H
#define CPL_CSUBPIXELSCANLINERENDERER_H

#include "../Common.h"
#include "SubpixelRendering.h"

namespace cpl
{
	namespace rendering
	{
		#ifdef _DEBUG
		#define __NOTHROW_IF_RELEASE
		#else
		#define __NOTHROW_IF_RELEASE noexcept
		#endif

		template<
			typename PixelType,
			LCDMatrixOrientation matrixOrder = LCDMatrixOrientation::RGB,
			class CustomWeightTable = WeightMap<5, 9>,
			class GammaLaw = LinearGammaScale<std::uint8_t>,
			typename CoordinateType = std::int_fast32_t
		>
			class CSubpixelScanlineRenderer
		{
			// adhering to std::is_standard_layout
		public:

			typedef PixelType Pixel;
			typedef CoordinateType IntType;
			typedef CustomWeightTable WeightTable;
			static const bool isLittleEndian = PixelType::indexR == 2;
			static const IntType alphaIndice =
				std::is_same<PixelType, juce::PixelARGB>::value ? (isLittleEndian ? 0 : 1) : 0;

			typedef RGBToDisplayPixelMap<matrixOrder, isLittleEndian> RGBToBMPMap;

			#ifdef CPL_MSVC
			static const std::uint8_t PixelMax = UCHAR_MAX;
			#else
			static constexpr std::uint8_t PixelMax = std::numeric_limits<std::uint8_t>::max();
			#endif


			CSubpixelScanlineRenderer
			(
				const juce::Image::BitmapData & data,
				juce::Colour colour,
				juce::Point<float> where,
				const juce::RectangleList<int> & rectangles,
				const GammaLaw & gammaFunctor = GammaLaw(),
				WeightTable weightFunctor = WeightTable()
			)
				:
				data(data),
				y(0),
				origin(cpl::Math::floorToNInf<int>(where.x), cpl::Math::round<int>(where.y)),
				alphaMap(),
				alphaPos(-1),
				subXOff(cpl::Math::round<IntType>(cpl::Math::frac(where.x) * 3)),
				lineIsBeingRendered(false),
				rectClip(rectangles),
				gamma(gammaFunctor),
				weight(weightFunctor)
			{
				if (isLittleEndian)
				{
					colourSetup[0] = colour.getBlue();
					colourSetup[1] = colour.getGreen();
					colourSetup[2] = colour.getRed();
				}
				else
				{
					colourSetup[0] = colour.getRed();
					colourSetup[1] = colour.getGreen();
					colourSetup[2] = colour.getBlue();
				}
				colourSetup[3] = colour.getAlpha();
			}



			inline void incrementalMove(IntType diff)
			{
				const IntType addedMagicNumber = 150;
				const IntType magicNumberDiv3 = addedMagicNumber / 3;

				#ifdef _DEBUG
				// some glyphs thinks its funny to have outlines way out of their own space.
				// this kinda fucks up division around zero and negatives, calculating the wrong offsets.
				// the 'fix' so far is to add a large enough constant to divisions.
				if (alphaPos < -addedMagicNumber)
					throw std::runtime_error(
						"Article being drawn is too much out of bounds. Please fix the division here."
					);
				#endif

				std::div_t downScaledX;

				for (IntType i = 0; i < diff; ++i)
				{
					// the absolute sub pixel were indexing
					auto subpixelPosition = alphaPos + subXOff + addedMagicNumber + i - (WeightTable::size >> 1);

					// keep these lines close
					downScaledX.quot = (subpixelPosition) / 3;
					downScaledX.rem = (subpixelPosition) % 3;

					auto finalX = origin.x + downScaledX.quot - magicNumberDiv3;
					auto finalY = origin.y + y;

					// unfortunately, we can only check collision at this point.
					if (!rectClip.containsPoint(finalX, finalY))
						continue;

					// get the pointer to the pixel - coordinates are verified at this point
					auto pixelPtr = data.getPixelPointer(finalX, finalY);

					// this maps the [0..2] subpixel offset to the correct
					// endianness and matrix orientation of the display.
					auto bmpEntry = bitmapMap.map[downScaledX.rem];

					// add the pixel position entry for bmps and the alphaIndice
					pixelPtr += bmpEntry + alphaIndice;
					// colour for this alpha index
					auto colour = colourSetup[bmpEntry];
					// cumulated alpha levels for this colour
					auto alphaLevel = weight.alphaMap[i];
					// alphablend with source
					*pixelPtr = cpl::Math::roundedMul(*pixelPtr, PixelMax - alphaLevel) + cpl::Math::roundedMul(alphaLevel, colour);

				}

				weight.clearAndShuffle(diff);

			}

			inline void moveToPos(IntType x)
			{
				// haven't started render yet, move to pos.
				if (!lineIsBeingRendered)
				{
					lineIsBeingRendered = true;
					alphaPos = x;
					return;
				}
				// shouldn't happen; ignore
				if (x == alphaPos)
					return;

				#ifdef _DEBUG
				if (x < alphaPos)
					throw std::runtime_error("Subpixel renderer moved backwards. Uh oh.");
				#endif

				// see how much we should render - and saturate to 5
				IntType diff = cpl::Math::confineTo<IntType>(x - alphaPos, 0, WeightTable::size);

				#ifdef _DEBUG
				if (!diff)
					throw std::runtime_error("Subpixel corrupt state.");
				#endif

				incrementalMove(diff);

				alphaPos = x;
			}
			inline void moveToStart() __NOTHROW_IF_RELEASE
			{
				alphaPos = 0;
				lineIsBeingRendered = false;
				weight.clear();
			}

			inline void rasterizeRestOfBuffer()
			{
				incrementalMove(WeightTable::size);
				alphaPos += WeightTable::size;
			}

			~CSubpixelScanlineRenderer()
			{
				rasterizeRestOfBuffer();
			}

			inline void setPixel(IntType x, IntType y, std::uint8_t alpha = PixelMax)
			{
				#ifdef _DEBUG
				if (this->y != y)
				{
					throw std::runtime_error("Subpixel renderer rendering on wrong scanline.");
				}
				#endif

				moveToPos(x);

				// filter weight map - note this is the WRONG place to add gamma correction
				// it only works because the default lineargammascale
				// ignores dark colours, which would otherwise get intensified!
				weight.addIntensityToMap(gamma(alpha) * colourSetup[3]);
			}

			// renderer interface
			inline void setEdgeTableYPos(IntType y)
			{
				rasterizeRestOfBuffer();
				this->y = y;
				moveToStart();

			}
			inline void handleEdgeTablePixel(IntType x, IntType alphaLevel)
			{
				setPixel(x, y, static_cast<std::uint8_t>(alphaLevel));
			}
			inline void handleEdgeTablePixelFull(IntType x)
			{
				setPixel(x, y);
			}
			inline void handleEdgeTableLine(IntType x, IntType width, IntType alphaLevel)
			{
				for (IntType i = 0; i < width; ++i)
				{
					setPixel(x + i, y, static_cast<std::uint8_t>(alphaLevel));
				}
			}
			inline void handleEdgeTableLineFull(IntType x, IntType width)
			{
				for (IntType i = 0; i < width; ++i)
				{
					setPixel(x + i, y);
				}
			}

			// variables.
			const juce::Image::BitmapData & data;
			const juce::Point<int> origin;
			const juce::RectangleList<int> & rectClip;
			const IntType subXOff;
			const GammaLaw & gamma;
			WeightTable weight;
			std::uint_fast16_t gammaMul;
			std::uint8_t colourSetup[4];
			std::uint8_t alphaMap[5];
			IntType alphaPos;
			IntType y;
			bool lineIsBeingRendered;

			RGBToBMPMap bitmapMap;
		};
	}; // {} rendering
} // {} cpl
#endif
