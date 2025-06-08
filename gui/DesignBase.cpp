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

	file:DesignBase.cpp

		Source code for DesignBase.h

*************************************************************************************/

#include "../LibraryOptions.h"
#include "../Common.h"
#include "DesignBase.h"
#include "../rendering/CSubpixelSoftwareGraphics.h"
#if defined(CPL_HINT_FONT)
#include "../vf_lib/vf_gui/vf_FreeTypeFaces.h"
#include "../FreeType/FreeTypeAmalgam.h"
#endif



namespace cpl
{
	using namespace juce;

	/*********************************************************************************************

		Data section.

	*********************************************************************************************/

	// associative size of texts

	namespace TextSize
	{
		float smallerText = 10.5f;
		float smallText = 12.f;
		float normalText = 13.5f;
		float largeText = 15.f;
	};
	#ifdef fhdjalsfhdjlskfdsa
	namespace TextSize
	{
		float smallerText = 10.3f;
		float smallText = 11.7f;
		float normalText = 12.8f;
		float largeText = 16.1;
	};
	#endif



	#ifdef CPL_JUCE

	// standard size of square and rectangle controls.
	namespace ControlSize
	{
		BoundingRect Square {80, 80};
		BoundingRect Rectangle {120, 40};
	};


	// grey-dark color set
	/*
	namespace cpl
	{
	Colour colourDeactivated	(187, 187, 187);
	Colour colourActivated		(223, 223, 223);
	Colour colourAux			(203, 203, 203);
	Colour colourAuxFont		(115, 115, 115);
	Colour colourSeparator		(115, 115, 115);
	Colour colourSelFont		(0,		0,	0);
	};
	*/

	// dark-white scheme
	SchemeColour defaultColours[] =
	{
		{
			{ 26, 26, 26 },		//Colour colourDeactivated(26, 26, 26);
			"Deactivated",
			"Fill colour for deactivated controls or areas."
		},
		{
			{ 40, 40, 40 },		//Colour colourNormal(40, 40, 40);
			"Normal",
			"The fundamental colour, others are shades off."
		},
		{
			{ 50, 50, 50 },		//Colour colourActivated(50, 50, 50);
			"Activated",
			"Fill colour for activated controls or areas."
		},
		{
			{ 203, 203, 203 },	//Colour colourAux(203, 203, 203);
			"Auxillary",
			"Brighter colour that contrasts others, and are used for backgrounds."
		},
		{
			{ 128, 128, 128 },	//Colour colourAuxFont(128, 128, 128);
			"Auxillary Text",
			"Used for most text."
		},
		{
			{ 75, 75, 75 },		//Colour colourSeparator(75, 75, 75);
			"Separator",
			"Used for seperating/dividing sections of other colours."
		},
		{
			{ 183, 183, 132 },	//Colour colourSelFont(153, 153, 102);
			"Selected Text",
			"Colour of text, that is selected."
		},
		{
			{ 0, 0x7F, 0 },		//Colour colourSuccess(0, 0x7F, 0);
			"Success",
			"Colour that indicates success."
		},
		{
			{ 0x7F, 0, 0 },		//Colour colourError(0x7F, 0, 0);
			"Error",
			"Colour that indicates error."
		},
		{
			{ 0xfa, 0xfa, 0xd2 },		//Colour colourCtrlText(0xfffafad2);
			"Control Text",
			"Colour of controls' text."
		}

	};

	//juce::Font systemFont("Verdana", TextSize::normalText, juce::Font::plain);
	//juce::Font systemFont;
	#endif

	struct ColourMapEntry
	{
		int ID;
		ColourEntry colour;


	};

	/*		fmtValueLabel.setColour(fmtValueLabel.textColourId, colourAuxFont);
	fmtValueLabel.setColour(juce::TextEditor::focusedOutlineColourId, colourAux);
	fmtValueLabel.setColour(juce::TextEditor::outlineColourId, colourActivated);
	fmtValueLabel.setColour(juce::TextEditor::textColourId, colourAuxFont);
	fmtValueLabel.setColour(juce::TextEditor::highlightedTextColourId, colourSelFont);
	fmtValueLabel.setColour(CaretComponent::caretColourId, colourAux.brighter(0.2f));*/

	ColourMapEntry ColourMap[] =
	{
		// popup menus
		{ PopupMenu::backgroundColourId, ColourEntry::Deactivated },
		{ PopupMenu::textColourId, ColourEntry::AuxillaryText },

		// combo-boxes
		{ ComboBox::backgroundColourId, ColourEntry::Deactivated },
		{ ComboBox::buttonColourId, ColourEntry::Separator },
		{ ComboBox::arrowColourId, ColourEntry::Auxillary },
		{ ComboBox::outlineColourId, ColourEntry::Separator },
		{ ComboBox::textColourId, ColourEntry::AuxillaryText },

		// text editors
		{ juce::TextEditor::backgroundColourId, ColourEntry::Deactivated },
		{ juce::TextEditor::focusedOutlineColourId, ColourEntry::Auxillary },
		{ juce::TextEditor::highlightColourId, ColourEntry::Activated },
		{ juce::TextEditor::outlineColourId, ColourEntry::Activated },
		{ juce::TextEditor::textColourId, ColourEntry::AuxillaryText },
		{ juce::TextEditor::highlightedTextColourId, ColourEntry::SelectedText },

		{ juce::Slider::textBoxTextColourId, ColourEntry::AuxillaryText },
		{ juce::Slider::textBoxBackgroundColourId, ColourEntry::Deactivated },
		{ juce::Slider::textBoxHighlightColourId, ColourEntry::SelectedText },
		{ juce::Slider::textBoxOutlineColourId, ColourEntry::Separator },

		// carets and such
		{ juce::CaretComponent::caretColourId, ColourEntry::ControlText },

		// colour selectors
		{ ColourSelector::backgroundColourId, ColourEntry::Deactivated },
		{ ColourSelector::labelTextColourId, ColourEntry::AuxillaryText },

		// labels
		{ juce::Label::textColourId, ColourEntry::AuxillaryText },
		{ juce::Label::textWhenEditingColourId, ColourEntry::AuxillaryText },
		// tool tips
		{ CToolTipWindow::backgroundColourId, ColourEntry::Deactivated },
		{ CToolTipWindow::outlineColourId, ColourEntry::Separator },
		{ CToolTipWindow::textColourId, ColourEntry::SelectedText }
	};


	CLookAndFeel_CPL::CLookAndFeel_CPL()
		: tryToRenderSubpixel(true)
	{
		for (auto & colour : defaultColours)
		{
			colours.push_back(colour);
		}

		updateColours();
		setUsingNativeAlertWindows(true);


		LookAndFeel::setDefaultLookAndFeel(this);
		#if defined(CPL_HINT_FONT)
		//this->setDefaultSansSerifTypefaceName("Verdana");
		// Add the TrueType font "Helvetica Neue LT Com 65 Medium" and
		// use hinting when the font height is between 7 and 12 inclusive.

		juce::File stdFont(Misc::GetDirectoryPath() + "/resources/fonts/Verdana.ttf");

		if (juce::ScopedPointer<juce::FileInputStream> stream = stdFont.createInputStream())
		{
			auto size = stream->getTotalLength();
			auto & block = loadedFonts[stdFont.getFileNameWithoutExtension().toStdString()];
			block.resize(size);
			if (size == stream->read(block.data(), size))
			{

				FreeTypeFaces::addFaceFromMemory(
					7.f, 18.f, false,
					block.data(),
					size);
			}
			else
			{
				block.clear();

			}
		}
		#else
		//setDefaultSansSerifTypefaceName("Source Sans Pro");
		#endif

	}

	void CLookAndFeel_CPL::updateColours()
	{
		for (ColourMapEntry & entry : ColourMap)
		{
			setColour(entry.ID, colours[(std::size_t)entry.colour].colour);
		}

	}

	SchemeColour & CLookAndFeel_CPL::getSchemeColour(ColourEntry entry)
	{
		return colours.at((std::size_t)entry);
	}

	SchemeColour & CLookAndFeel_CPL::getSchemeColour(std::size_t entry)
	{
		return colours.at(entry);
	}

	juce::Font CLookAndFeel_CPL::getStdFont()
	{
		#ifdef CPL_HINT_FONT
		return Font();
		#else
		return Font();
		//return Font("Verdana", TextSize::normalText, Font::plain);
		#endif
	}
	CLookAndFeel_CPL::~CLookAndFeel_CPL() {};

	juce::Font CLookAndFeel_CPL::getPopupMenuFont()
	{
		return getStdFont();
	}

	juce::Font CLookAndFeel_CPL::getComboBoxFont(ComboBox &)
	{
		return getStdFont();
	}

	void CLookAndFeel_CPL::drawComboBox(Graphics & g, int width, int height, bool isButtonDown, int buttonX, int buttonY, int buttonW, int buttonH, ComboBox & c)
	{
		const int triangleSize = cpl::Math::round<int>(buttonH * 0.5);
		g.fillAll(cpl::GetColour(cpl::ColourEntry::Deactivated));

		juce::Path triangleVertices;

		auto const yO = buttonH * 0.25f;
		auto const xO = c.getWidth() - buttonH + yO;

		const bool isPopped = c.isPopupActive();
		// displacement
		auto const d1 = isButtonDown ? 1 : 0;
		triangleVertices.addTriangle
		(
			d1 + xO + triangleSize, d1 + yO + 0,
			d1 + xO + triangleSize, d1 + yO + triangleSize,
			d1 + xO + 0, d1 + yO + triangleSize * 0.5f
		);

		triangleVertices.applyTransform(AffineTransform::identity.rotated(float(isPopped * -M_PI * 0.5), d1 + xO + triangleSize * 0.5f, d1 + yO + triangleSize * 0.5f));
		g.setColour(cpl::GetColour(cpl::ColourEntry::Activated).brighter(
			c.isMouseOverOrDragging() * 0.1f + 0.2f + 0.2f * isPopped)
		);
		g.fillPath(triangleVertices);

	};
	CLookAndFeel_CPL & CLookAndFeel_CPL::defaultLook()
	{
		static CLookAndFeel_CPL staticData;
		return staticData;
	}

	const std::vector<char> & CLookAndFeel_CPL::getFaceMemory(const std::string_view s)
	{
		auto it = loadedFonts.find(s);
		if(it != loadedFonts.end())
			return it->second;

		return loadedFonts[std::string(s)];
	}

	void CLookAndFeel_CPL::setShouldRenderSubpixels(bool shouldRender)
	{
		tryToRenderSubpixel = shouldRender;
	}
	/*
	virtual juce::LowLevelGraphicsContext * LookAndFeel::createGraphicsContextAdvanced(
		const Image & buffer, const Point<int> origin, const RectangleList<int> clip, bool isScreenContext, const Point<int> componentPosition)
	{
		// default implementation just forwards:
		return createGraphicsContext(buffer, origin, clip);
	}*/

	std::unique_ptr<juce::LowLevelGraphicsContext> CLookAndFeel_CPL::createGraphicsContext(
		const Image &imageToRenderOn,
		Point<int> origin,
		const RectangleList< int > &initialClip)
	{
		if (tryToRenderSubpixel/* && imageToRenderOn.getFormat() == imageToRenderOn.RGB*/)
		{
			return std::make_unique<rendering::CSubpixelSoftwareGraphics>(imageToRenderOn, origin, initialClip);
		}

		return juce::LookAndFeel_V3::createGraphicsContext(imageToRenderOn, origin, initialClip);

	}

	Typeface::Ptr CLookAndFeel_CPL::getTypefaceForFont(Font const& font)
	{
		#ifdef CPL_HINT_FONT
		//return LookAndFeel::getTypefaceForFont(font);
		Typeface::Ptr tf;

		String faceName(font.getTypefaceName());

		// Make requests for the default sans serif font use our
		// FreeType hinted font instead.

		if (faceName == Font::getDefaultSansSerifFontName())
		{
			// Create a new Font identical to the old one, then
			// switch the name to our hinted font.

			Font f(font);

			// You'll need to know the exact name embedded in the font. There
			// are a variety of free programs for retrieving this information.

			f.setTypefaceName("Verdana");

			// Now get the hinted typeface.
			#if defined(CPL_HINT_FONT)
			tf = FreeTypeFaces::createTypefaceForFont(f);
			#else
			auto & fontFace = loadedFonts["SourceSansPro-Regular"];
			tf = Typeface::createSystemTypefaceFor(fontFace.data(), fontFace.size());
			#endif
		}

		// If we got here without creating a new typeface
		// then just use the default LookAndFeel behavior.

		if (tf)
		{
			return tf;
		}
		else
			#endif
		{
			return LookAndFeel::getTypefaceForFont(font);
		}

	}
};
