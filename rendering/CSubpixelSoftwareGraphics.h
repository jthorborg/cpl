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

		A software renderer that tries to render subpixel antialised graphics
		(falls back to standard software renderer on errors).

*************************************************************************************/

#ifndef _CSUBPIXELSOFTWAREGRAPHICS_H
#define _CSUBPIXELSOFTWAREGRAPHICS_H
#include "../Common.h"
#include "SubpixelRendering.h"

namespace cpl
{
	namespace rendering
	{

		class CDisplaySetup;

		class CSubpixelSoftwareGraphics
			:
			public juce::LowLevelGraphicsSoftwareRenderer
		{

		public:

			CSubpixelSoftwareGraphics(const juce::Image & imageToRenderOn, const juce::Point<int> & origin,
				const juce::RectangleList<int> & initialClip, bool allowAlphaDrawing = true);

			virtual ~CSubpixelSoftwareGraphics() {};

			// overrides

			void drawGlyphs(Span<const uint16_t> glyphs,
				Span<const Point<float>> positions,
				const AffineTransform& t) override;

			// the height in points where to stop drawing subpixel aa-glyphs
			static void setAntialiasingTransition(float heightToStopSubpixels);

		private:

			bool tryToDrawGlyph(int glyphNumber, const AffineTransform & z);
			bool RenderEdgeLayer(LCDMatrixOrientation monitorMatrix, const juce::Point<float>& pos, const juce::EdgeTable& outlines);
			static float maxHeight;
			Point<int> origin;
			const juce::Image & buffer;
			const RectangleList<int> & startingClip;
			CDisplaySetup & displayInfo;

		};
	}; // {} rendering
}; // {} cpl
#endif