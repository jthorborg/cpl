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

	file:CDisplaySetup.cpp

		Implementation of CDisplaySetup.h

 *************************************************************************************/

#include "../MacroConstants.h"
#ifdef CPL_WINDOWS
#include "DisplayOrientationWindows.cpp"
#elif defined(CPL_MAC)
#include <CoreGraphics/CGDisplayConfiguration.h>
#include <IOKit/graphics/IOGraphicsLib.h>
#elif defined(CPL_UNIXC)
#include "DisplayOrientationLinux.cpp"
#else
 // add linux, osx, android, ios here.
#error "Update displayorientations to your current platform!"
#endif
#include "../PlatformSpecific.h"
#include "../MacSupport.h"

#include "CDisplaySetup.h"
#include "../gui/Tools.h"

namespace cpl
{
	namespace rendering
	{
		CDisplaySetup * CDisplaySetup::internalInstance = nullptr;

		void CDisplaySetup::Deleter()
		{
			if (internalInstance)
			{
				delete internalInstance;
			}
		}

		const CDisplaySetup::DisplayData & CDisplaySetup::displayFromPoint(std::pair<int, int> pos) const
		{
			juce::Point<int> point(pos.first, pos.second);
			for (auto & display : displays)
			{
				if (display.bounds.contains(point))
				{
					return display;
				}
			}
			return getMainDisplay();
		}

		const CDisplaySetup::DisplayData & CDisplaySetup::displayFromIndex(std::size_t index) const
		{
			return displays.at(index);
		}


		std::vector<const CDisplaySetup::DisplayData *> CDisplaySetup::getDuplicateDisplaysFor(const CDisplaySetup::DisplayData & initialDisplay) const
		{

			std::vector<const DisplayData *> displayList;

			for (auto & display : displays)
			{
				// first line checks if they are at same position as well - ie. duplicated.
				if (&display != &initialDisplay && display.bounds == initialDisplay.bounds)
				{
					displayList.push_back(&display);
				}
			}
			return displayList;
		}

		const CDisplaySetup::DisplayData & CDisplaySetup::getMainDisplay() const
		{
			for (auto & display : displays)
			{
				if (display.isMainMonitor)
					return display;
			}
			return defaultDevice;
		}

		CDisplaySetup & CDisplaySetup::instance()
		{
			// TODO: threadsafe?
			if (internalInstance == nullptr)
			{
				internalInstance = new CDisplaySetup();
				atexit(&CDisplaySetup::Deleter);
			}
			return *internalInstance;
		}

		CDisplaySetup::CDisplaySetup()
			: defaultFontGamma(1.2), systemHook()
		{
			update();
			// set the hooking to happen in the main thread when its ready and set
			GUIUtils::FutureMainEvent(100, [&]() { installMessageHook(); }, this);
		}

		CDisplaySetup::~CDisplaySetup()
		{
			notifyDestruction();
			if (systemHook.hook)
			{
				removeMessageHook();
			}
		}

		#ifdef CPL_WINDOWS

		LRESULT CALLBACK MessageHook(int code, WPARAM wParam, LPARAM lParam)
		{
			auto & displayInstance = CDisplaySetup::instance();


			// code doesn't really matter, we dont want to interfere with anything
			// just be notified on display actions.
			// wParam also doesn't matter (whether the event is posted by this thread)

			const CWPRETSTRUCT * msg = reinterpret_cast<const CWPRETSTRUCT*>(lParam);

			switch (msg->message)
			{
				case WM_SETTINGCHANGE:
				case WM_DISPLAYCHANGE:
					if (!CDisplaySetup::instance().getSystemHook().eventHasBeenPosted.load(std::memory_order_acquire))
					{
						// avoid posting it multiple times:
						CDisplaySetup::instance().getSystemHook().eventHasBeenPosted.store(true, std::memory_order_release);

						// spawn the event
						GUIUtils::FutureMainEvent(1000, []() { CDisplaySetup::instance().update(); }, &CDisplaySetup::instance());
					}
				default:
					break;

			}

			return CallNextHookEx((HHOOK)displayInstance.getSystemHook().hook.load(std::memory_order_acquire), code, wParam, lParam);
		}
		#elif defined(CPL_MAC)

		void MessageHook(CGDirectDisplayID display, CGDisplayChangeSummaryFlags flags, void *userInfo)
		{
			// https://developer.apple.com/library/mac/documentation/GraphicsImaging/Reference/Quartz_Services_Ref/index.html#//apple_ref/c/tdef/CGDisplayReconfigurationCallBack
			if (flags & kCGDisplayBeginConfigurationFlag) {
				// first iteration; ignore. -- displays not changed yet.
			}
			else if (!CDisplaySetup::instance().getSystemHook().eventHasBeenPosted)
			{
				// avoid posting it multiple times:
				CDisplaySetup::instance().getSystemHook().eventHasBeenPosted.store(true, std::memory_order_release);

				// spawn the event
				GUIUtils::FutureMainEvent(1000, []() { CDisplaySetup::instance().update(); }, &CDisplaySetup::instance());
			}
		}

		#endif


		void CDisplaySetup::installMessageHook()
		{
			#ifdef CPL_WINDOWS
			systemHook.hook = SetWindowsHookEx(
				WH_CALLWNDPROCRET,
				MessageHook,
				GetModuleHandle(0),
				GetCurrentThreadId()
			);
			#elif defined(CPL_MAC)
			CGDisplayRegisterReconfigurationCallback(&MessageHook, nullptr);
			#endif
		}

		void CDisplaySetup::removeMessageHook()
		{
			#ifdef CPL_WINDOWS
			UnhookWindowsHookEx((HHOOK)systemHook.hook.load(std::memory_order_acquire));
			#elif defined(CPL_MAC)
			CGDisplayRemoveReconfigurationCallback(&MessageHook, nullptr);
			#endif
		}

		void CDisplaySetup::update()
		{
			displays.clear();
			DBG("Updated displays...");
			bool systemUsesSubpixelSmoothing = false;
			double finalGamma = defaultFontGamma;
			double rotation;
			#ifdef CPL_WINDOWS
			BOOL systemSmoothing = FALSE;
			// antialiased text set?
			if (SystemParametersInfo(SPI_GETFONTSMOOTHING, 0, &systemSmoothing, 0)
				&& systemSmoothing)
			{
				UINT smoothingType = 0;
				// clear type enabled?
				if (SystemParametersInfo(SPI_GETFONTSMOOTHINGTYPE, 0, &smoothingType, 0)
					&& smoothingType == FE_FONTSMOOTHINGCLEARTYPE)
				{
					UINT systemGamma = 0; // 1000 - 2200

					systemUsesSubpixelSmoothing = true;
					// collect gamma correction
					if (SystemParametersInfo(SPI_GETFONTSMOOTHINGCONTRAST, 0, &systemGamma, 0))
					{
						systemGamma = cpl::Math::confineTo<std::uint16_t>((std::uint16_t)systemGamma, 1000, 2200);
						finalGamma = systemGamma / 1000.0;
					}
				}
			}
			#endif

			DisplayData currentMonitorInfo;
			int count = 0;
			for (auto & display : juce::Desktop::getInstance().getDisplays().displays)
			{

				bool thisIndexUsesSubpixels = false;
				auto displayOrigin = display.userArea.getPosition();
				// default to RGB...
				LCDMatrixOrientation displayOrientation = LCDMatrixOrientation::RGB;
				#ifdef CPL_WINDOWS
				UINT systemMatrixOrder = 0;
				if (SystemParametersInfo(SPI_GETFONTSMOOTHINGORIENTATION, 0, &systemMatrixOrder, 0))
				{
					switch (systemMatrixOrder)
					{
						case FE_FONTSMOOTHINGORIENTATIONBGR:
							displayOrientation = LCDMatrixOrientation::BGR;
							thisIndexUsesSubpixels = true;
							break;
						case FE_FONTSMOOTHINGORIENTATIONRGB:
							displayOrientation = LCDMatrixOrientation::RGB;
							thisIndexUsesSubpixels = true;
							break;
						default:
							thisIndexUsesSubpixels = false;
					};
				}
				#endif
				#ifndef CPL_MAC
				if (GetScreenOrientation({displayOrigin.getX(), displayOrigin.getY()}, rotation))
				{
					currentMonitorInfo.screenOrientation = RadsToOrientation(rotation);
					currentMonitorInfo.screenRotation = rotation;

				}

				#else
				// enter the realms of objective-c
				OSXExtendedScreenInfo info {0};
				if (GetExtendedScreenInfo(displayOrigin.getX(), displayOrigin.getY(), &info))
				{
					switch (info.subpixelOrientation)
					{
						case kDisplaySubPixelLayoutUndefined:
							// fall through - we default to RGB.
						case kDisplaySubPixelLayoutRGB:
							systemUsesSubpixelSmoothing = thisIndexUsesSubpixels = true;
							displayOrientation = LCDMatrixOrientation::RGB;
							break;
						case kDisplaySubPixelLayoutBGR:
							systemUsesSubpixelSmoothing = thisIndexUsesSubpixels = true;
							displayOrientation = LCDMatrixOrientation::BGR;
							break;
						default:
							thisIndexUsesSubpixels = false;
							break;
					};
					if (info.averageGamma <= 1.2)
						info.averageGamma = 1.4;
					finalGamma = info.averageGamma;

					//if(!info.displayIsDigital)
					//	thisIndexUsesSubpixels = false;

					currentMonitorInfo.screenOrientation = RadsToOrientation(info.screenRotation);
					currentMonitorInfo.screenRotation = info.screenRotation;

				}
				#endif

				currentMonitorInfo.displayMatrixOrder = displayOrientation;

				if (systemUsesSubpixelSmoothing &&
					(currentMonitorInfo.screenRotation == 0 || currentMonitorInfo.screenRotation == 180) &&
					thisIndexUsesSubpixels)
				{
					currentMonitorInfo.isApplicableForSubpixels = true;
				}
				else
				{
					// well, if you want to add support for vertical
					// subpixel rendering (useless) do it here.
					currentMonitorInfo.isApplicableForSubpixels = false;
				}
				currentMonitorInfo.bounds = display.totalArea;
				currentMonitorInfo.dpi = display.dpi;
				currentMonitorInfo.scale = display.scale;
				currentMonitorInfo.fontGamma = finalGamma;
				currentMonitorInfo.gammaScale.setGamma(finalGamma);
				currentMonitorInfo.isMainMonitor = display.isMain;
				currentMonitorInfo.index = count++;
				displays.push_back(currentMonitorInfo);

			}

			// set what displays has duplicates and check compability.
			for (auto & display : displays)
			{
				display.isDuplicatesCompatible = true;
				auto duplicates = getDuplicateDisplaysFor(display);
				if (duplicates.size())
				{
					display.isDisplayDuplicated = true;
					display.duplicates = duplicates;
					for (auto & duplicate : duplicates)
					{
						if (!display.isRenderingCompatibleTo(*duplicate))
						{
							display.isDuplicatesCompatible = false;
							break;
						}
					}
				}
			}

			// clear message post flag:
			systemHook.eventHasBeenPosted = false;
		}


	}; // {} rendering
}; // {} cpl
