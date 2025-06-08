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

	file:NewStuffAndLook.h

		Classes and widgets enqueded for implementation

*************************************************************************************/

#ifndef CPL_NEWSTUFFANDLOOK_H
#define CPL_NEWSTUFFANDLOOK_H

#include "GraphicComponents.h"
#include "../Resources.h"
#include <vector>

namespace cpl
{


	class CDoubleClickComponent
	{


	};


	class SimpleButton
	{

	};

	template<class ButtonType = SimpleButton>
	class CRowTabBar : public CBaseControl, public juce::Component
	{
	public:

		enum ColourID
		{
			ColourSelected,
			ColourUnselected
		};

		typedef ButtonType ButtonTy;

		void setColour(ColourID id, juce::Colour colour)
		{
			if (id < CPL_ARRAYSIZE(colours))
				colours[id] = colour;
		}

		virtual void addTab(std::string name)
		{
			if (name.size())
				buttons.emplace_back(std::move(name));
		}

		virtual void paint(juce::Graphics & g) override
		{

			//g.setColour(Colours::black);
			// fill rects

			auto hoverButton = getMouseHoverButton();
			auto height = getHeight() / buttons.size();
			g.setFont(TextSize::normalText);
			//g.getCurrentFont().getHeight() / 2 + height / 2
			juce::Rectangle<int> textRectangle(5, 0, getWidth(), height);
			for (int index = 0; index < buttons.size(); ++index)
			{
				textRectangle.setY(height * index);
				auto color = selectedIndex == index ? cpl::GetColour(cpl::ColourEntry::Activated) : cpl::GetColour(cpl::ColourEntry::Deactivated);
				Colour textColour = (selectedIndex == index ? cpl::GetColour(cpl::ColourEntry::SelectedText) : cpl::GetColour(cpl::ColourEntry::AuxillaryText));
				// enhance brightness
				if (index == hoverButton)
				{
					textColour = textColour.darker();
				}

				double pos = double(index) / (buttons.size());
				g.setColour(color);
				g.fillRect(0, pos * getHeight(), getWidth(), height);
				g.setColour(textColour);
				g.drawFittedText(buttons[index].c_str(), textRectangle, juce::Justification::centredLeft, 1);

				// 5, g.getCurrentFont().getHeight() / 2 + height/2 + height * index
				//g.drawSingleLineText(buttons[index].c_str(),);
			}
			return; // comment if you want black bars around
			g.setColour(Colours::black);
			// draw horizontal lines.
			for (int line = 1; line < buttons.size(); ++line)
			{
				double pos = double(line) / (buttons.size());
				g.drawHorizontalLine(pos * getHeight(), 0, getWidth());
			}
			if (selectedIndex != 0)
				g.drawVerticalLine(getWidth() - 1, 0, selectedIndex * height);
			if (buttons.size() > 1)
				g.drawVerticalLine(getWidth() - 1, (selectedIndex + 1) * height, getHeight());
		}

		CRowTabBar()
			: selectedIndex(0), maxTabHeight(25), CBaseControl(this), isMouseInside(false)
		{
			setRepaintsOnMouseActivity(true);
			mouseCoords[0] = mouseCoords[1] = 0;
			colours[0] = Colours::grey;
			colours[1] = Colours::lightgrey;
		}

		void setSelectedTab(int index)
		{
			if (buttons.size() > 1 && index >= 0 && index < buttons.size() && index != selectedIndex)
			{
				selectedIndex = index;
				bSetValue(double(index) / (buttons.size() - 1));
			}
		}

	protected:

		void mouseEnter(const juce::MouseEvent & e) override
		{
			isMouseInside = true;
		}

		void mouseExit(const juce::MouseEvent & e) override
		{
			isMouseInside = false;
		}

		void mouseMove(const juce::MouseEvent & e) override
		{
			mouseCoords[0] = e.x;
			mouseCoords[1] = e.y;
			if (hoverIndex != getMouseHoverButton())
				repaint();
		}

		void mouseDown(const juce::MouseEvent & e) override
		{

			mouseMove(e);
			setSelectedTab(getMouseHoverButton());
		}



		int getMouseHoverButton()
		{

			if (!isMouseInside)
				return -1;
			else
			{
				return cpl::Math::round<int>((buttons.size() - 1) * (double)mouseCoords[1] / getHeight());
			}
		}

		int selectedIndex;
		int hoverIndex;
		std::vector<std::string> buttons;

		juce::Colour colours[2];

		bool isMouseInside;
		int mouseCoords[2];
		double maxTabHeight;
	};
	template<class ButtonType = SimpleButton>
	class CTextTabBar : public CBaseControl, public juce::Component
	{
	public:

		class CTabBarListener
		{
		public:

			typedef CTextTabBar<ButtonType> BaseBarType;

			virtual void panelOpened(BaseBarType * object) {};
			virtual void panelClosed(BaseBarType * object) {};

			virtual void tabSelected(BaseBarType * object, int index) {};
			virtual void activeTabClicked(BaseBarType * object, int index) {};
			virtual ~CTabBarListener() {};
		};

		enum ColourID
		{
			ColourSelected,
			ColourUnselected
		};

		enum Type
		{
			Vertical,
			Horizontal
		};

		typedef ButtonType ButtonTy;

		void setColour(ColourID id, juce::Colour colour)
		{
			if (id < CPL_ARRAYSIZE(colours))
				colours[id] = colour;
		}

		virtual CTextTabBar & addTab(std::string name)
		{
			if (name.size())
			{
				buttons.emplace_back(std::move(name));
				renderTriangle();
			}
			return *this;
		}



		virtual void paint(juce::Graphics & g) override
		{

			//g.setColour(Colours::black);
			// fill rects



			auto hoverButton = getMouseHoverButton();
			// vertical or horizontal size
			juce::Font f("Verdana", 5, juce::Font::bold);
			g.setFont(f);
			//g.getCurrentFont().getHeight() / 2 + height / 2
			if (orientation == Vertical)
			{
				g.setFont(TextSize::normalText);
				auto size = float(getHeight()) / buttons.size();
				juce::Rectangle<int> textRectangle(5, 0, getWidth(), cpl::Math::round<int>(size - triangleSize));
				for (auto index = 0; index < (int)buttons.size(); ++index)
				{
					textRectangle.setY(cpl::Math::round<int>(size * index));
					juce::Colour textColour, color;
					if (!isIndeterminateState && selectedIndex == index)
					{
						color = cpl::GetColour(cpl::ColourEntry::Activated);
						textColour = cpl::GetColour(cpl::ColourEntry::SelectedText);
					}
					else
					{
						color = cpl::GetColour(cpl::ColourEntry::Deactivated);
						textColour = cpl::GetColour(cpl::ColourEntry::AuxillaryText);
					}
					// enhance brightness
					if (!isIndeterminateState && index == hoverButton)
					{
						textColour = textColour.darker();
					}

					float pos = float(index) / (buttons.size());
					g.setColour(color);
					g.fillRect(0.f, pos * getHeight(), (float)getWidth(), (float)size);
					g.setColour(textColour);
					g.drawFittedText(buttons[index].c_str(), textRectangle, juce::Justification::centredLeft, 1);

					// 5, g.getCurrentFont().getHeight() / 2 + height/2 + height * index
					//g.drawSingleLineText(buttons[index].c_str(),);
				}
				return; // comment if you want black bars around
				g.setColour(Colours::black);
				// draw horizontal lines.
				for (unsigned line = 1; line < buttons.size(); ++line)
				{
					float pos = float(line) / (buttons.size());
					g.drawHorizontalLine(cpl::Math::round<int>(pos * getHeight()), (float)0, (float)getWidth());
				}
				if (selectedIndex != 0)
					g.drawVerticalLine(getWidth() - 1, 0.f, (float)selectedIndex * size);
				if (buttons.size() > 1)
					g.drawVerticalLine(getWidth() - 1, (selectedIndex + 1.f) * size, (float)getHeight());
			}
			else
			{

				//float fontSize = JUCE_LIVE_CONSTANT(11.1);
				g.setFont(TextSize::normalText);
				auto size = (float)ceil(double(getWidth()) / buttons.size());
				juce::Rectangle<int> textRectangle(cornerOffset, 0, Math::round<int>(size - cornerOffset * 2), getHeight());
				for (auto index = 0; index < (int)buttons.size(); ++index)
				{
					textRectangle.setX(Math::round<int>(cornerOffset + size * index));
					juce::Colour textColour, color;
					if (!isIndeterminateState && selectedIndex == index)
					{
						color = GetColour(ColourEntry::Activated);
						textColour = GetColour(ColourEntry::SelectedText);
						textRectangle.setRight(Math::round<int>(triangleVertices.getBounds().getX() - cornerOffset));
					}
					else
					{
						textRectangle.setWidth(Math::round<int>(size - cornerOffset * 2));
						color = GetColour(ColourEntry::Deactivated);
						textColour = GetColour(ColourEntry::AuxillaryText);
					}
					// enhance brightness
					if (index == hoverButton)
					{
						textColour = textColour.brighter(0.2f);
					}

					g.setColour(color);
					// -1 size so it will be filled by separator
					g.fillRect(size * index, 0.f, (float)size - ((std::size_t)index == buttons.size() - 1 ? 0 : 1), (float)getHeight());
					g.setColour(textColour);
					g.drawFittedText(buttons[index].c_str(), textRectangle, juce::Justification::centredLeft, 1);

					g.setColour(GetColour(cpl::ColourEntry::Separator));
					// vertical borders
					g.drawVerticalLine(Math::round<int>(size * index + size), 0.0f, (float)getHeight());
				}
				g.setColour(GetColour(cpl::ColourEntry::Auxillary));
				// draw triangle
				if (isTriangleHovered)
					g.setOpacity(0.8f);
				else
					g.setOpacity(0.6f);

				if (!isIndeterminateState)
					g.fillPath(triangleVertices);

				return;

				// draw horizontal lines.
				for (auto index = 0; index < (int)buttons.size() - 1; ++index)
				{
					// vertical borders
					g.drawVerticalLine(Math::round<int>(size * index + size), 0.0f, (float)getHeight());
				}



				//p.addTriangle()

				/*if (selectedIndex != 0)
				g.drawVerticalLine(getWidth() - 1, 0, selectedIndex * size);
				if (buttons.size() > 1)
				g.drawVerticalLine(getWidth() - 1, (selectedIndex + 1) * size, getHeight());*/
			}

		}
		void addListener(CTabBarListener * list)
		{
			if (list)
				listeners.push_back(list);
		}
		CTextTabBar()
			: selectedIndex(0), maxTabHeight(25), CBaseControl(this), isMouseInside(false),
			orientation(Vertical), triangleSize(5), isTriangleHovered(false), isIndeterminateState(true), panelIsClosed(true)
		{
			setRepaintsOnMouseActivity(true);
			mouseCoords[0] = mouseCoords[1] = 0;
			colours[0] = Colours::grey;

			colours[1] = Colours::lightgrey;
		}
		void setOrientation(Type orientation)
		{
			this->orientation = orientation;
			renderTriangle();
		}
		void setSelectedTab(int index)
		{
			int size = (int)buttons.size();
			if (isIndeterminateState || (size > 0 && index >= 0 && index < size))
			{
				if (isIndeterminateState || index != selectedIndex)
				{
					isIndeterminateState = false;
					selectedIndex = index;
					bSetValue(double(index) / (buttons.size() - 1));
					renderTriangle();
					for (auto & listener : listeners)
						listener->tabSelected(this, index);
				}
				else
				{
					for (auto & listener : listeners)
						listener->activeTabClicked(this, index);
				}
			}
		}

		void openPanel()
		{
			if (panelIsClosed)
			{
				panelIsClosed = false;
				for (auto & listener : listeners)
					listener->panelOpened(this);
				renderTriangle();
				repaint();
			}
		}
		void closePanel()
		{
			if (!panelIsClosed)
			{
				panelIsClosed = true;
				for (auto & listener : listeners)
					listener->panelClosed(this);
				renderTriangle();
				repaint();
			}
		}
		bool isOpen() const
		{
			return !panelIsClosed;
		}

		int getNumTabs() const
		{
			return buttons.size();
		}

		int getSelectedTab() const
		{
			return selectedIndex;
		}

	protected:

		void resized() override
		{
			renderTriangle();
		}

		void mouseEnter(const juce::MouseEvent & e) override
		{
			isMouseInside = true;
		}

		void mouseExit(const juce::MouseEvent & e) override
		{
			isMouseInside = false;
			isTriangleHovered = false;
		}

		void mouseMove(const juce::MouseEvent & e) override
		{
			isMouseInside = true;
			mouseCoords[0] = e.x;
			mouseCoords[1] = e.y;
			auto currentHover = getMouseHoverButton();

			if (currentHover == selectedIndex)
			{
				isTriangleHovered = triangleVertices.getBounds().expanded(2.f).contains(e.position);
			}
			else
				isTriangleHovered = false;
			if (hoverIndex != currentHover)
				repaint();
		}

		void mouseDown(const juce::MouseEvent & e) override
		{
			mouseMove(e);
			if (isTriangleHovered)
			{
				panelIsClosed ? openPanel() : closePanel();
			}
			else
				setSelectedTab(getMouseHoverButton());
			mouseMove(e);
		}



		void renderTriangle()
		{
			if (!buttons.size())
				return;

			auto const offset = cornerOffset * 1.5f;

			triangleSize = float(orientation == Vertical ? getWidth() : getHeight()) - offset * 2;

			juce::Point<float> pos;
			juce::Point<float> originCenter;
			if (orientation == Vertical)
			{
				pos.setXY(offset, (1 + selectedIndex) * float(getHeight()) / buttons.size() - triangleSize - offset);
				originCenter.setXY(getWidth() / 2.0f, ((1 + selectedIndex) * float(getWidth()) / buttons.size()) - triangleSize - cornerOffset / 2.f);
			}
			else
			{
				pos.setXY((1 + selectedIndex) * float(getWidth()) / buttons.size() - triangleSize - offset, offset);
				originCenter.setXY(((1 + selectedIndex) * float(getWidth()) / buttons.size()) - triangleSize - cornerOffset / 2.f, getHeight() / 2.0f);
			}

			triangleVertices.clear();

			if ((getHeight() & 1) != 0)
				pos.y += 1;

			triangleVertices.addTriangle
			(
				pos.x + 0.f, pos.y + 0.f,
				pos.x + triangleSize, pos.y + 0.f,
				pos.x + triangleSize / 2, pos.y + triangleSize
			);

			if (orientation == Horizontal)
			{
				if (panelIsClosed)
				{
					triangleVertices.applyTransform(AffineTransform::identity.rotated((float)M_PI / 2, originCenter.getX(), originCenter.getY()));
					//triangleVertices.applyTransform(AffineTransform::identity.rotated(rotation, originCenter.getX(), originCenter.getY()));
				}
				else
				{

				}
			}
			else
			{
				if (panelIsClosed)
				{
					triangleVertices.applyTransform(AffineTransform::identity.rotated((float)M_PI / 2, originCenter.getX(), originCenter.getY()));
				}
				else
				{
					triangleVertices.applyTransform(AffineTransform::identity.rotated((float)-M_PI, originCenter.getX(), originCenter.getY()));
				}
			}

			//triangleVertices.applyTransform(AffineTransform::identity.rotated(rotation, originCenter.getX(), originCenter.getY()));

		}
		int getMouseHoverButton()
		{

			if (!isMouseInside)
				return -1;
			else
			{
				double fraction;
				if (orientation == Vertical)
					fraction = double(mouseCoords[1]) / getHeight();
				else
					fraction = double(mouseCoords[0]) / getWidth();
				return std::min<int>(int(buttons.size() * fraction), (int)buttons.size() - 1);
			}
		}
		static const int cornerOffset = 5;
		int selectedIndex;
		int hoverIndex;
		std::vector<std::string> buttons;
		// tabs at start doesn't have a selected index.
		bool isIndeterminateState;
		juce::Colour colours[2];
		Type orientation;
		bool isMouseInside;
		int mouseCoords[2];
		double maxTabHeight;
		bool isTriangleHovered;
		bool panelIsClosed;
		float triangleSize;
		juce::Path triangleVertices;
		std::vector<CTabBarListener*> listeners;
	};

	class CTriangleButton : public juce::Button
	{
	public:

		enum Direction
		{
			left, top, right, bottom
		};

		CTriangleButton()
			: juce::Button("TriangleButton"), hoverBrightness(0.2f)
		{
			dirs[0] = Direction::left;
			dirs[1] = Direction::bottom;
			colours[0] = colours[1] = cpl::GetColour(ColourEntry::Auxillary);
			setClickingTogglesState(true);
			setOpaque(false);
		}

		void setHoverBrightness(float newVal)
		{
			hoverBrightness = cpl::Math::confineTo(newVal, -1.f, 1.f);
		}

		void setActivatedDirection(Direction dir) { dirs[1] = dir; }
		void setDeactivatedDirection(Direction dir) { dirs[0] = dir; }
		void setActivatedColour(juce::Colour colour) { colours[1] = colour; }
		void setDeactivatedColour(juce::Colour colour) { colours[0] = colour; }

		void paintButton(Graphics& g, bool isMouseOverButton, bool isButtonDown) override
		{
			g.setColour(colours[isButtonDown].withMultipliedBrightness(1.f + isMouseOverButton * hoverBrightness));
			g.fillPath(triangleVertices);
		}


		void clicked() override
		{
			renderTriangle();
		}

		void resized() override
		{
			renderTriangle();
		}

		void renderTriangle()
		{
			triangleVertices.clear();

			triangleVertices.addTriangle
			(
				(float)getWidth(), (float)0,
				(float)getWidth(), (float)getHeight(),
				(float)0, getHeight() * 0.5f
			);

			triangleVertices.applyTransform(
				AffineTransform::identity.rotated(dirs[getToggleState()] * float(M_PI * 0.5), getWidth() * 0.5f, getHeight() * 0.5f)
			);
		}

	private:
		Direction dirs[2];
		juce::Colour colours[2];
		juce::Path triangleVertices;
		float hoverBrightness;
	};


	class CIconTabBar : public CTextTabBar<>
	{
	public:

		static const int iconOffset = 3;

		virtual void paint(juce::Graphics & g) override
		{

			g.setColour(cpl::GetColour(cpl::ColourEntry::Separator));
			g.fillAll();

			auto elementBorder = 1;
			auto hoverButton = getMouseHoverButton();
			auto height = (getHeight()) / (int)buttons.size();
			auto iconHeight = height - iconOffset * 2;
			//g.getCurrentFont().getHeight() / 2 + height / 2
			juce::Rectangle<int> iconRectangle(iconOffset, iconOffset, getWidth() - iconOffset * 2, iconHeight);
			for (int index = 0; index < (int)buttons.size(); ++index)
			{
				iconRectangle.setY(height * index + iconOffset);
				bool isSelectedIndex = selectedIndex == index;

				auto color = isSelectedIndex ?
					cpl::GetColour(cpl::ColourEntry::Activated) : cpl::GetColour(cpl::ColourEntry::Deactivated);
				Colour textColour = (isSelectedIndex ? cpl::GetColour(cpl::ColourEntry::SelectedText) : cpl::GetColour(cpl::ColourEntry::AuxillaryText));
				// enhance brightness
				if (index == hoverButton)
				{
					color = color.brighter(0.05f);
				}

				double pos = double(index) / (buttons.size());
				double nextPos = double(index + 1) / (buttons.size());
				auto heightThisEntry = int(getHeight() * nextPos) - int(getHeight() * pos);

				g.setColour(color);
				g.fillRect
				(
					0,
					(int)(pos * getHeight()) + (index == 0 ? 1 : 0), // creates a border at the top of the first element
					getWidth() - (isSelectedIndex ? 0 : elementBorder),
					heightThisEntry - elementBorder - (index == 0 ? 1 : 0)
				);
				vectors[(unsigned)index].changeFillColour(cpl::GetColour(cpl::ColourEntry::SelectedText));
				vectors[(unsigned)index].getDrawable()->drawWithin(g, iconRectangle.toFloat().withTrimmedRight(1), juce::RectanglePlacement::centred, isSelectedIndex ? 1 : 0.5f);
				//g.drawImageAt(vectors[(unsigned)index].getImage(), iconOffset, height * index + iconOffset);
				g.setOpacity(1.f);
			}
			return; // comment if you want black bars around
			g.setColour(cpl::GetColour(cpl::ColourEntry::Separator));
			// draw horizontal lines.
			//return;
			for (unsigned line = 0; line < buttons.size(); ++line)
			{
				double pos = double(line) / (buttons.size());
				g.drawLine(0.f, float(pos * getHeight()), (float)getWidth(), float(pos * getHeight()), 0.5f);
			}
			if (selectedIndex != 0)
				g.drawLine((float)getWidth() - 0.5f, (float)0, (float)getWidth() - 0.5f, (float)selectedIndex * height, 0.5f);
			if (buttons.size() > 1)
				g.drawLine((float)getWidth() - 0.5f, (float)(selectedIndex + 1) * height, (float)getWidth() - 0.5f, (float)getHeight(), 0.5f);
		}

		void resized() override
		{
			if (!buttons.size())
				return;
			if (!getWidth() || !getHeight())
				return;
			//auto hoverButton = getMouseHoverButton();

			auto height = (getHeight()) / (int)buttons.size();
			auto iconHeight = height - iconOffset * 2;
			//g.getCurrentFont().getHeight() / 2 + height / 2
			juce::Rectangle<float> iconRectangle((float)iconOffset, (float)iconOffset, (float)getWidth() - iconOffset * 2.0f, (float)iconHeight);

		}

		CTextTabBar<> & addTab(std::string name) override
		{
			if (name.size())
			{
				if (cpl::contains(buttons, name))
					CPL_RUNTIME_EXCEPTION("Multiple tabs with same unique name!");
				vectors.emplace_back(name);
				buttons.emplace_back(std::move(name));
				resized();
			}
			return *this;
		}


	private:
		std::vector<CVectorResource> vectors;

	};

	class CSVGButton : public juce::Button, public cpl::CBaseControl
	{
	public:
		CSVGButton()
			: Button("IconButton"), CBaseControl(this), pst(1.f)
		{
			setClickingTogglesState(true);
			enableTooltip(true);
		}

		void clicked() override
		{
			baseControlValueChanged();
		}

		void baseControlValueChanged() override
		{
			notifyListeners();
		}

		void setImage(std::string imagePath)
		{
			rsc.associate(imagePath);
			resized();
		}

		iCtrlPrec_t bGetValue() const override
		{
			return getToggleState() ? 1.0f : 0.f;
		}

		void bSetValue(iCtrlPrec_t val, bool sync = false) override
		{
			setToggleState(val > 0.5,
				sync ? juce::NotificationType::sendNotificationSync : juce::NotificationType::sendNotification);
		}

		void bSetInternal(iCtrlPrec_t val) override
		{
			setToggleState(val > 0.5, juce::NotificationType::dontSendNotification);
		}

		void paintButton(juce::Graphics & g, bool isMouseOverButton, bool isButtonDown) override
		{

			if (getToggleState() && !isButtonDown)
			{
				g.fillAll(cpl::GetColour(ColourEntry::Activated));
			}
			else if (isButtonDown)
			{
				g.fillAll(cpl::GetColour(ColourEntry::Deactivated).brighter(0.3f));
			}
			else if (isMouseOverButton)
			{
				g.fillAll(cpl::GetColour(ColourEntry::Deactivated).brighter(0.1f));
			}
			else
			{
				g.fillAll(cpl::GetColour(ColourEntry::Deactivated));
			}

			rsc.changeFillColour(cpl::GetColour(ColourEntry::SelectedText));

			rsc.getDrawable()->drawWithin(
				g,
				getBounds().withZeroOrigin().reduced(cornerOffset).toFloat(),
				juce::RectanglePlacement::centred,
				getToggleState() ? 1.0f : 0.5f
			);

		}

	private:
		static const int cornerOffset = 2;
		CVectorResource rsc;
		juce::PathStrokeType pst;
	};
};

#endif
