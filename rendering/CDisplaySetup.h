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

	file: CDisplaySetup.h

		Manages a list of all connected screens and their respective properties.
		This list should automagically be updated through OS hooks.
		Update() should only be called from the main thread, if needed.

 *************************************************************************************/

#ifndef _CDISPLAYSETUP_H
#define _CDISPLAYSETUP_H

#include "SubpixelRendering.h"
#include "../gui/Tools.h"
#include <vector>
#include <memory>

namespace cpl
{
	namespace rendering
	{
		class CDisplaySetup : public DestructionNotifier
		{
		public:

			struct DisplayEvent
			{
				std::atomic<void *> hook;
				std::atomic<bool> eventHasBeenPosted;
			} systemHook;

			struct DisplayData
			{
				friend class CDisplaySetup;
				friend class std::vector<DisplayData>;

				bool isRenderingCompatibleTo(const DisplayData & other) const
				{
					if (this == &other)
						return true;
					else if (bounds == other.bounds &&
						displayMatrixOrder == other.displayMatrixOrder &&
						scale == other.scale &&
						screenRotation == other.screenRotation)
					{
						return true;
					}
					return false;
				}
				// whether this monitor is a candidate for subpixel rendering
				bool isApplicableForSubpixels;
				// monitors can duplicate their content to others:
				// this field is true if the content will look the same on the other screen
				// (ie, subpixel ordering is the same for all duplicates)
				bool isDuplicatesCompatible;
				// whether this display is duplicated.
				bool isDisplayDuplicated;
				// the system contrast for UI fonts.
				double fontGamma;
				// the actual order of the R-G-B subpixels.
				// use this in conjunction with rendering::RGBToDisplayPixelMap
				// to map RGB index to subpixel indexes of the monitor..
				LCDMatrixOrientation displayMatrixOrder;
				// the rotation of the monitor, given in degrees. Positive degrees denote counter-clockwise rotation.
				double screenRotation;
				// symbolic name for the rotation.
				Orientation screenOrientation;
				// a gammescale that maps corrects intensity to the system scale - use this for font rendering.
				// rest should be done automatically by sRGB spaces.
				LutGammaScale gammaScale;
				// the total area of this monitor.
				juce::Rectangle<int> bounds;
				// ..
				bool isMainMonitor;
				// scale tells us what scaling should be used when rendering to match the UI,
				// eg. for retina displays, this is at minimum 1.5
				// dpi refers to the physical size of the display
				double scale, dpi;
				// the index that identifies this display.
				int index;
				// a list of monitors that display the same content as this one.
				std::vector<const DisplayData *> duplicates;

				/*private:
					DisplayData(const DisplayData &);
					~DisplayData();
					DisplayData(DisplayData &&);
					DisplayData & operator = (const DisplayData &);
					DisplayData & operator = (DisplayData &&);*/

			private:

				DisplayData()
					:
					isApplicableForSubpixels(false),
					fontGamma(1.2),
					displayMatrixOrder(LCDMatrixOrientation::RGB),
					screenRotation(0.0),
					screenOrientation(Orientation::Landscape),
					gammaScale(fontGamma),
					isMainMonitor(true),
					scale(1.0),
					dpi(72.0),
					isDisplayDuplicated(false),
					isDuplicatesCompatible(true),
					index(0)
				{

				}
			};

			static CDisplaySetup & instance();

			const DisplayData & displayFromPoint(std::pair<int, int> pos) const;
			const DisplayData & displayFromPoint(juce::Point<int> pos) const {
				return displayFromPoint(std::make_pair(pos.getX(), pos.getY()));
			}
			const DisplayData & displayFromIndex(std::size_t index) const;

			std::vector<const DisplayData *> getDuplicateDisplaysFor(const DisplayData &) const;

			const DisplayData & getMainDisplay() const;
			const DisplayData * begin() const { return &displays[0]; }
			const DisplayData * end() const { return &displays[0] + displays.size(); };

			std::size_t numDisplays() const { return displays.size(); }

			void update();
			DisplayEvent & getSystemHook() { return systemHook; }
		private:
			void installMessageHook();
			void removeMessageHook();
			DisplayData defaultDevice;
			static void Deleter();
			~CDisplaySetup();
			CDisplaySetup();
			std::vector<DisplayData> displays;
			static CDisplaySetup * internalInstance;
			double defaultFontGamma;


		};

	}; // {} rendering
}; // {} cpl
#endif
