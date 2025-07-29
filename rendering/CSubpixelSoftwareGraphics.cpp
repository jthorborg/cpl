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

	file:CSubpixelSoftwareGraphics.h

	Implementation of CSubpixelSoftwareGraphics.h

*************************************************************************************/

#include "CSubpixelSoftwareGraphics.h"
#include "SubpixelRendering.h"
#include "CSubpixelScanlineRenderer.h"
#include "../stdext.h"
#include "CDisplaySetup.h"
#include <memory>

namespace cpl
{
	namespace rendering
	{
		// here you can specialse the renderer to different weigths, gamma scales, etc.
		template<typename PixelType, LCDMatrixOrientation orientation>
		using Renderer = CSubpixelScanlineRenderer < PixelType, orientation, WeightMap<5, 16>, LinearGammaScale<std::uint8_t> >;

		// default place to stop.
		float CSubpixelSoftwareGraphics::maxHeight = 100.f;

		const int RGBToDisplayPixelMap<LCDMatrixOrientation::RGB, false>::map[3] =
		{
			0,
			1,
			2
		};

		const int RGBToDisplayPixelMap<LCDMatrixOrientation::RGB, true>::map[3] =
		{
			2,
			1,
			0
		};

		const int RGBToDisplayPixelMap<LCDMatrixOrientation::BGR, false>::map[3] =
		{
			2,
			1,
			0
		};

		const int RGBToDisplayPixelMap<LCDMatrixOrientation::BGR, true>::map[3] =
		{
			0,
			1,
			2
		};

		const int RGBToDisplayPixelMap<LCDMatrixOrientation::GBR, false>::map[3] =
		{
			2,
			0,
			1
		};

		const int RGBToDisplayPixelMap<LCDMatrixOrientation::GBR, true>::map[3] =
		{
			1,
			0,
			2
		};

		const int RGBToDisplayPixelMap<LCDMatrixOrientation::RBG, false>::map[3] =
		{
			0,
			2,
			1
		};

		const int RGBToDisplayPixelMap<LCDMatrixOrientation::RBG, true>::map[3] =
		{
			1,
			2,
			0
		};

		void CSubpixelSoftwareGraphics::setAntialiasingTransition(float heightToStopSubpixels)
		{
			maxHeight = std::min(0.0f, heightToStopSubpixels);
		}

		CSubpixelSoftwareGraphics::CSubpixelSoftwareGraphics
		(
			const juce::Image & imageToRenderOn,
			const juce::Point<int> & origin,
			const juce::RectangleList<int> & initialClip,
			bool allowAlphaDrawing
		)
			: LowLevelGraphicsSoftwareRenderer(imageToRenderOn, origin, initialClip)
			, buffer(imageToRenderOn)
			, origin(origin)
			, startingClip(initialClip)
			, displayInfo(CDisplaySetup::instance())
		{
			// throw if we're given an alpha image.
			// note that technically it doesn't make sense to draw on an ARGB image
			// we simply "ignore" the alpha channel, this feature exists because
			// most operating systems draw with 32-bit BMPs even though they only
			// use the RGB channels.
			if (!allowAlphaDrawing && imageToRenderOn.getFormat() != imageToRenderOn.RGB)
			{
				throw std::runtime_error(
					"CSubpixelSoftwareGraphics(): Image to render on was not RGB!"
				);
			}
		}

		void CSubpixelSoftwareGraphics::drawGlyphs(
			juce::Span<const uint16_t> glyphs,
			juce::Span<const juce::Point<float>> positions,
			const juce::AffineTransform& t)
		{
			using namespace juce;

			for (std::size_t i = 0; i < glyphs.size(); ++i)
			{
				auto affine = AffineTransform::translation(positions[i]).followedBy(t);
				if (!tryToDrawGlyph(glyphs[i], affine))
					LowLevelGraphicsSoftwareRenderer::drawGlyph(glyphs[i], affine);
			}
		}


		bool CSubpixelSoftwareGraphics::tryToDrawGlyph(int glyphNumber, const AffineTransform & z)
		{
			using namespace juce::RenderingHelpers;

			// only colours supported now
			if (!stack->fillType.isColour())
				return false;

			// check if size is over maxHeight (the point where we stop subpixel rendering)
			auto font = getFont();
			if (font.getHeight() >= maxHeight)
				return false;

			// obtain transform
			auto & transform = stack->transform;
			// starting point
			juce::Point<float> pos(z.getTranslationX(), z.getTranslationY());

			// find what display our glyph resides on.
			// Note this is wrong - need to somehow figure out the global position
			// of our renderer context, so we can associate with a monitor!
			auto & currentMonitor = displayInfo.displayFromPoint(std::make_pair(0, 0));

			// check this as well - will be possible once the former point is implemented
			bool glyphSpansMultipleMonitors = false;

			if (glyphSpansMultipleMonitors || !currentMonitor.isApplicableForSubpixels || !currentMonitor.isDuplicatesCompatible)
				return false;

			// lcd ordering of pixels
			auto matrixOrder = currentMonitor.displayMatrixOrder;

			// here we handle the current rotation of this monitor.
			// if it is RGB but rotated PI radians, we simply invert the pixel
			// matrix orientation to BGR! If it is rotated inbetween, well,
			// we wouldn't have reached this point! Then
			// currentMonitor.isApplicableForSubpixels will be false.
			if (currentMonitor.screenRotation == 180)
			{
				matrixOrder = InvertedMatrixOrientation(matrixOrder);
			}

			// get the glyph outlines
			if (z.isOnlyTranslation() && !transform.isRotated)
			{
				// get the glyph from the cache
				auto& cache = GlyphCache::getInstance();

				// get the current font and scale it 3 times horizontally.
				font.setHorizontalScale(font.getHorizontalScale() * 3);

				// scale the font according to monitor scaling...
				if (!transform.isOnlyTranslated)
				{
					pos = transform.transformed(pos);
					font.setHeight(font.getHeight() * transform.complexTransform.mat11);

					const float xScale = transform.complexTransform.mat00 / transform.complexTransform.mat11;
					if (std::abs(xScale - 1.0f) > 0.01f)
						font.setHorizontalScale(font.getHorizontalScale() * xScale);
				}
				else
				{
					// add the offset.. linear drawing
					pos += transform.offset.toFloat();
				}

				for (auto& layer : cache.get(font, glyphNumber))
				{
					if (auto* colourLayer = std::get_if<juce::ColourLayer>(&layer.layer))
					{
						const auto initialFill = stack->fillType;
						const ScopeGuard scope{ [&] { this->stack->setFillType(initialFill); } };

						if (auto fill = colourLayer->colour)
							stack->setFillType(*fill);

						if (!RenderEdgeLayer(matrixOrder, pos, colourLayer->clip))
							return false;
					}
					else
					{
						// contains emojis or something
						return false;
					}
				}
			}
			else // here we have to create a new edgetable - it is rotated/skewed.
			{
				// fix this code. looks funny.
				return false;
				/*
				auto font = getFont();
				font.setHorizontalScale(font.getHorizontalScale() * 3);
				const float fontHeight = font.getHeight();

				juce::AffineTransform t(transform.getTransformWith(
					juce::AffineTransform::scale(fontHeight * font.getHorizontalScale(), fontHeight)
					.followedBy(z)));

				outlines.reset(font.getTypeface()->getEdgeTableForGlyph(glyphNumber, t, fontHeight));

				// delete it later.
				outlines.get_deleter().shared = false;
				*/
			}

			return true;
		}

		bool CSubpixelSoftwareGraphics::RenderEdgeLayer(LCDMatrixOrientation matrixOrder, const juce::Point<float>& pos, const juce::EdgeTable& outlines)
		{
			// the colour to draw with
			auto& colour = stack->fillType.colour;

			// obtain bitmap buffer.
			Image::BitmapData destData(buffer, Image::BitmapData::readWrite);

			// we support a custom gamma scale for each monitor (see currentMonitor.gammaScale), but since in practice
			// it isn't really used (and the code doesn't work yet) we just create a simple
			// gamma correction here - the same used in JUCE, albeit adjusts itself to the system setting...
			{
				//auto & gammaScale = currentMonitor.gammaScale;
			}

			LinearGammaScale<std::uint8_t> gammaScale;
			gammaScale.setGammaCorrection(colour.getBrightness(), 1.8f /*currentMonitor.fontGamma*/);

			// create the correct renderer
			switch (matrixOrder)
			{
			case LCDMatrixOrientation::RGB:
			{
				// create renderer,
				switch (buffer.getFormat())
				{
				case juce::Image::RGB:
				{

					Renderer<juce::PixelRGB, LCDMatrixOrientation::RGB>
						subpixelRenderer(destData, colour, pos, startingClip, gammaScale);

					outlines.iterate(subpixelRenderer);
					break;
				}
				case juce::Image::ARGB:
				{

					Renderer<PixelARGB, LCDMatrixOrientation::RGB>
						subpixelRenderer(destData, colour, pos, startingClip, gammaScale);

					outlines.iterate(subpixelRenderer);
					break;
				}
				// unknown image type
				default:
					return false;
				}
				break;

			}
			case LCDMatrixOrientation::BGR:
			{
				// create renderer,
				switch (buffer.getFormat())
				{
				case juce::Image::RGB:
				{

					Renderer<PixelRGB, LCDMatrixOrientation::BGR>
						subpixelRenderer(destData, colour, pos, startingClip, gammaScale);

					outlines.iterate(subpixelRenderer);
					break;
				}
				case juce::Image::ARGB:
				{

					Renderer<PixelARGB, LCDMatrixOrientation::BGR>
						subpixelRenderer(destData, colour, pos, startingClip, gammaScale);

					outlines.iterate(subpixelRenderer);
					break;
				}
				// unknown image type
				default:
					return false;
				}
				break;
			}
			// unknown pixel matrix of screen
			default:
				return false;
			}
			// succesfully rendered something.
			return true;
		}

	}; // {} rendering
}; // {} cpl
