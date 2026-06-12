/*
 ==============================================================================

 This file is part of the JUCE library.
 Copyright (c) 2013 - Raw Material Software Ltd.

 Permission is granted to use this software under the terms of either:
 a) the GPL v2 (or any later version)
 b) the Affero GPL v3

 Details of these licenses can be found at: www.gnu.org/licenses

 JUCE is distributed in the hope that it will be useful, but WITHOUT ANY
 WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

 ------------------------------------------------------------------------------

 To release a closed-source product which uses JUCE, commercial licenses are
 available: visit www.juce.com for more information.

 ==============================================================================
 */


 /*************************************************************************************

	 file:CToolTip.h

		 This is really just a duplicate of juce::ToolTip, because some widgets
		 in JUCE uses the tooltips to display non-tooltip data, thus overriding
		 (and not displaying) tool tip text. So we have two classes now!
		 This class is detected only by cpl::TopView.

 *************************************************************************************/

#ifndef CPL_CTOOLTIP_H
#define CPL_CTOOLTIP_H
namespace cpl
{
	using namespace juce;
	//==============================================================================
	/**
	 A window that displays a pop-up tooltip when the mouse hovers over another component.

	 To enable tooltips in your app, just create a single instance of a TooltipWindow
	 object. Note that if you instantiate more than one instance of this class, you'll
	 end up with multiple tooltips being shown!

	 The TooltipWindow object will then stay invisible, waiting until the mouse
	 hovers for the specified length of time - it will then see if it's currently
	 over a component which implements the TooltipClient interface, and if so,
	 it will make itself visible to show the tooltip in the appropriate place.

	 @see TooltipClient, SettableTooltipClient
	 */
	class CToolTipClient
	{
	public:
		/** Destructor. */
		virtual ~CToolTipClient() {}

		/** Returns the string that this object wants to show as its tooltip. */
		virtual String bGetToolTip() const = 0;
		virtual String bGetToolTipForChild(const Component *) const { return {}; }
	};

	class CToolTipWindow : public Component,
		private Timer
	{
	public:
		//==============================================================================
		/** Creates a tooltip window.

		 Make sure your app only creates one instance of this class, otherwise you'll
		 get multiple overlaid tooltips appearing. The window will initially be invisible
		 and will make itself visible when it needs to display a tip.

		 To change the style of tooltips, see the LookAndFeel class for its tooltip
		 methods.

		 @param parentComponent  if set to 0, the TooltipWindow will appear on the desktop,
		 otherwise the tooltip will be added to the given parent
		 component.
		 @param millisecondsBeforeTipAppears     the time for which the mouse has to stay still
		 before a tooltip will be shown

		 @see TooltipClient, LookAndFeel::drawTooltip, LookAndFeel::getTooltipSize
		 */
		explicit CToolTipWindow(Component* parentComponent = nullptr,
			int millisecondsBeforeTipAppears = 700);

		/** Destructor. */
		~CToolTipWindow();

		//==============================================================================
		/** Changes the time before the tip appears.
		 This lets you change the value that was set in the constructor.
		 */
		void setMillisecondsBeforeTipAppears(int newTimeMs = 700) noexcept;

		/** Can be called to manually force a tip to be shown at a particular location. */
		void displayTip(juce::Point<int> screenPosition, const String& text);

		/** Can be called to manually hide the tip if it's showing. */
		void hideTip();

		//==============================================================================
		/** A set of colour IDs to use to change the colour of various aspects of the tooltip.

		 These constants can be used either via the Component::setColour(), or LookAndFeel::setColour()
		 methods.

		 @see Component::setColour, Component::findColour, LookAndFeel::setColour, LookAndFeel::findColour
		 */
		enum ColourIds
		{
			backgroundColourId = 0x1001b00,    /**< The colour to fill the background with. */
			textColourId = 0x1001c00,    /**< The colour to use for the text. */
			outlineColourId = 0x1001c10     /**< The colour to use to draw an outline around the tooltip. */
		};

		//==============================================================================
		/** This abstract base class is implemented by LookAndFeel classes to provide
		 window drawing functionality.
		 */
		struct JUCE_API  LookAndFeelMethods
		{
			virtual ~LookAndFeelMethods() {}

			virtual void getTooltipSize(const String& tipText, int& width, int& height) = 0;
			virtual void drawTooltip(Graphics&, const String& text, int width, int height) = 0;
		};

	private:
		//==============================================================================
		int millisecondsBeforeTipAppears;
		juce::Point<int> lastMousePos;
		int mouseClicks, mouseWheelMoves;
		unsigned int lastCompChangeTime, lastHideTime;
		Component* lastComponentUnderMouse;
		String tipShowing, lastTipUnderMouse;

		void paint(Graphics&) override;
		void mouseEnter(const MouseEvent&) override;
		void timerCallback() override;
		void updatePosition(const String&, juce::Point<int>, const juce::Rectangle<int>&);

		static String getTipFor(Component*);

		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CToolTipWindow)
	};
};

#endif   // JUCE_TOOLTIPWINDOW_H_INCLUDED
