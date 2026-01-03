/*************************************************************************************

	Audio Programming Environment VST.

		VST is a trademark of Steinberg Media Technologies GmbH.

	Copyright (C) 2013 Janus Lynggaard Thorborg [LightBridge Studios]

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

	file:CCtrlEditSpace.cpp

		Implementation of CCtrlEditSpace.h

*************************************************************************************/

#include "CCtrlEditSpace.h"
#include "NewStuffAndLook.h"
#include "Tools.h"
#include "controls/Controls.h"
#include <exception>
#include <typeinfo>

namespace cpl
{
	void CCtrlEditSpace::valueChanged(const CBaseControl * ctrl)
	{
		if (ctrl == parentControl)
		{
			resetToControl();
		}
		else if (ctrl == switchWithOld.get())
		{
			CSerializer newValue;
			parentControl->serialize(newValue, cpl::programInfo.version);
			parentControl->deserialize(oldValue, cpl::programInfo.version);
			parentControl->bForceEvent();
			oldValue = newValue;

		}
	}

	CCtrlEditSpace::CCtrlEditSpace(cpl::CBaseControl * parent)
		: parentControl(parent)
		, exitAfterAnimation(false)
		, inputValueWasValid(false)
		, toolTip("Control Edit Space: Interface for editing the values of controls precisely.")
		, expanderButton(new CTriangleButton())
		, compactWidth(120)
		, compactHeight(25)
		, fullWidth(200)
		, fullHeight(120)
		, compactMode(true)
	{
		if (!parent)
		{
			CPL_RUNTIME_EXCEPTION("Null pointer passed to constructor - trying to control non-existant control");
		}

		// store old value of control:
		parent->serialize(oldValue, programInfo.version);
		switchWithOld.reset(new CButton());
		switchWithOld->setTexts("Switch to A", "Switch to B");
		switchWithOld->setToggleable(true);
		switchWithOld->bSetDescription("Switch back to the other settings (A/B compare).");
		switchWithOld->enableTooltip(true);
		switchWithOld->bAddChangeListener(this);

		iconSucces.setImage(CVectorResource::renderSVGToImage("icons/svg/successtick.svg", juce::Rectangle<int>(15, 15), cpl::GetColour(cpl::ColourEntry::Success)));
		iconError.setImage(CVectorResource::renderSVGToImage("icons/svg/errorcross.svg", juce::Rectangle<int>(15, 15), cpl::GetColour(cpl::ColourEntry::Error)));
		iconSucces.setOpaque(false);
		iconSucces.setAlpha(0);
		iconError.setOpaque(false);
		iconError.setAlpha(0);


		fmtValueLabel.setEditable(true);
		fmtValueLabel.setFont(CLookAndFeel_CPL::defaultLook().getStdFont());

		fmtValueLabel.addListener(this);

		intValueLabel.setEditable(true);
		intValueLabel.setFont(CLookAndFeel_CPL::defaultLook().getStdFont());

		intValueLabel.addListener(this);


		//setWantsKeyboardFocus(true);
		parentControl->bAddChangeListener(this);

		getAnimator().addChangeListener(this);

		expanderButton->addListener(this);

		exportedControlName = parentControl->bGetExportedName();

		addChildComponent(intValueLabel);
		addChildComponent(switchWithOld.get());
		addAndMakeVisible(iconSucces);
		addAndMakeVisible(iconError);
		addAndMakeVisible(fmtValueLabel);
		addAndMakeVisible(errorVisualizer);
		addAndMakeVisible(expanderButton.get());
		setBounds(0, 0, compactWidth, compactHeight);

		// JUCE 8 asserts if you attempt to grab keyboard focus without fully existing yet, 
		// so have this happen after message queue processing in a bit.
		cpl::GUIUtils::MainEvent(*this, [this]() { createSimpleViewEditor(); });
	}


	juce::String CCtrlEditSpace::bGetToolTip() const
	{
		return toolTip;
	}

	void CCtrlEditSpace::paint(juce::Graphics & g)
	{
		g.fillAll(cpl::GetColour(cpl::ColourEntry::Deactivated));
		g.setColour(cpl::GetColour(cpl::ColourEntry::Separator));
		g.drawVerticalLine(getWidth() - (compactHeight - 1), 0.f, (float)getHeight() - 1);

		if (!compactMode)
		{
			g.setColour(cpl::GetColour(cpl::ColourEntry::Separator));
			g.drawHorizontalLine(elementHeight, 0.f, getWidth() - 1.f);
			// draw title of control
			//g.setFont(cpl::systemFont); EDIT_TYPE_NEWFONTS
			g.setFont(TextSize::normalText);
			g.setColour(cpl::GetColour(cpl::ColourEntry::AuxillaryText));
			auto titleRect = getBounds().withPosition(5, 1).withHeight(elementHeight);
			g.drawText("Editing ", titleRect.withRight(47), juce::Justification::centredLeft);
			g.setColour(cpl::GetColour(cpl::ColourEntry::SelectedText));

			juce::String titleText;

			if (exportedControlName.size())
			{
				titleText = exportedControlName;
			}
			else
			{
				titleText = Misc::DemangledTypeName(*parentControl);
				if (titleText.startsWith("class "))
					titleText = titleText.substring(6);

				titleText = "(?) " + titleText;
			}

			g.drawText(titleText, titleRect.withLeft(50).withRight(getWidth() - elementHeight), juce::Justification::centredLeft);

			// draw insides, starting with internal value
			auto elementPos = titleRect.withY(titleRect.getY() + elementHeight);
			g.setColour(cpl::GetColour(cpl::ColourEntry::AuxillaryText));
			g.drawText("Semantic value:", elementPos, juce::Justification::centredLeft);

			g.drawText("Internal value:", elementPos.withY(cpl::Math::round<int>(elementPos.getY() + elementHeight * 1.5)), juce::Justification::centredLeft);
		}
		g.setColour(cpl::GetColour(cpl::ColourEntry::Separator));
		g.drawRect(0, 0, getWidth(), getHeight());

	}

	void CCtrlEditSpace::resized()
	{

		expanderButton->setBounds
		(
			centerRectInsideRegion
			(
				getBounds().withPosition(0, 0).withLeft(getWidth() - (compactHeight)).withBottom(compactHeight - 1),
				10,
				7.5
			)
		);

		if (!expanderButton->getToggleState())
		{
			fmtValueLabel.setBounds(1, 1, getWidth() - compactHeight, elementHeight);
			intValueLabel.setVisible(false);
			switchWithOld->setVisible(false);
		}
		else
		{
			switchWithOld->setVisible(true);
			auto badCodingStyleConst = elementHeight - 5;
			fmtValueLabel.setBounds(5, badCodingStyleConst + elementHeight, getWidth() - (compactHeight + 5), elementHeight);
			intValueLabel.setBounds(5, fmtValueLabel.getBounds().getY() + badCodingStyleConst * 2, getWidth() - (compactHeight + 5), elementHeight);
			switchWithOld->setTopLeftPosition(5, intValueLabel.getBottom() + 5);

			iconError.setBounds(getWidth() - 20, intValueLabel.getBounds().getY() + 3, 15, 15);

			intValueLabel.setVisible(true);

		}
		errorVisualizer.setBounds(getBounds().withPosition(0, 0));
	}

	void CCtrlEditSpace::createSimpleViewEditor()
	{
		if (compactMode && isVisible())
		{
			fmtValueLabel.showEditor();
		}
	}
	void CCtrlEditSpace::editorShown(Label * curLabel, TextEditor & editor)
	{
		if (curLabel == &fmtValueLabel)
			editor.addListener(this);
		if (curLabel == &intValueLabel)
			editor.addListener(this);
		editor.setScrollToShowCursor(false);

	}
	void CCtrlEditSpace::animateSucces(juce::Component * objectThatWasModified)
	{
		if (getAnimator().isAnimating(&errorVisualizer))
		{
			// already animating this, try again in some time
			GUIUtils::FutureMainEvent(500, [&]() { animateSucces(objectThatWasModified); }, this);
		}
		if (compactMode)
		{
			exitAfterAnimation = compactMode;
			inputValueWasValid = true;
			errorVisualizer.borderColour = cpl::GetColour(cpl::ColourEntry::Success);
			errorVisualizer.borderSize = 4.f;
			errorVisualizer.setAlpha(1.f);
			errorVisualizer.isActive = true;
			errorVisualizer.repaint();

			getAnimator().animateComponent(&errorVisualizer, errorVisualizer.getBounds(), 0.f, 300, false, 1.0, 1.0);
		}
		else
		{
			inputValueWasValid = true;
			iconSucces.setAlpha(1.f);
			iconSucces.setBounds(getWidth() - 20, objectThatWasModified->getBounds().getY() + 3, 15, 15);
			getAnimator().animateComponent(&iconSucces, iconSucces.getBounds(), 0.f, 300, false, 1.0, 1.0);
		}
	}

	void CCtrlEditSpace::buttonClicked(juce::Button * b)
	{
		if (b == expanderButton.get())
		{
			compactMode = !expanderButton->getToggleState();
			setMode(compactMode);
		}
		fmtValueLabel.showEditor();
		repaint();
	}


	void CCtrlEditSpace::animateError(juce::Component * objectThatWasModified)
	{
		if (getAnimator().isAnimating(&errorVisualizer))
		{
			// already animating this, try again in some time
			GUIUtils::FutureMainEvent(500, [&]() { animateError(objectThatWasModified); }, this);
		}
		if (compactMode)
		{
			exitAfterAnimation = inputValueWasValid = false;
			errorVisualizer.borderColour = cpl::GetColour(ColourEntry::Error);
			errorVisualizer.borderSize = 4.f;
			errorVisualizer.setAlpha(1.f);
			errorVisualizer.isActive = true;
			errorVisualizer.repaint();

			getAnimator().animateComponent(&errorVisualizer, errorVisualizer.getBounds(), 0.f, 300, false, 1.0, 1.0);
		}
		else
		{
			inputValueWasValid = true;
			iconError.setAlpha(1.f);
			iconError.setBounds(getWidth() - 20, objectThatWasModified->getBounds().getY() + 3, 15, 15);
			getAnimator().animateComponent(&iconError, iconError.getBounds(), 0.f, 300, false, 1.0, 1.0);
		}
	}

	void CCtrlEditSpace::textEditorReturnKeyPressed(TextEditor & editor)
	{
		if (&editor == fmtValueLabel.getCurrentTextEditor())
		{
			// this is where we try to interpret a formatted value
			// to an internal range.

			// note: we use the value from the editor, because the label may not
			// have been updated yet.
			if ((inputValueWasValid = interpretAndSet(editor.getText().toStdString())))
			{
				animateSucces(&fmtValueLabel);
			}
			else
				animateError(&fmtValueLabel);
		}
		else if (&editor == intValueLabel.getCurrentTextEditor())
		{
			// here we try to map an input string to [0, 1] range.
			// the cbasecontrol provides a static method for this
			iCtrlPrec_t val(0);
			auto succes = CBaseControl::bMapStringToInternal(editor.getText().toStdString(), val);


			if (succes)
			{
				parentControl->bSetValue(val);
				animateSucces(&intValueLabel);
			}
			else
				animateError(&intValueLabel);

		}
	}
	iCtrlPrec_t CCtrlEditSpace::interpretString(const std::string & value)
	{
		iCtrlPrec_t ret(0);
		parentControl->bInterpret(value, ret);
		return ret;
	}
	bool CCtrlEditSpace::interpretAndSet(const std::string & value)
	{
		return parentControl->bInterpretAndSet(value, false);
	}
	std::string CCtrlEditSpace::getStringFrom(iCtrlPrec_t value)
	{
		std::string ret;
		if (parentControl->bFormatValue(ret, cpl::Math::confineTo<iCtrlPrec_t>(value, 0.0, 1.0)))
			return ret;
		else
			return "<error>";
	}
	std::string CCtrlEditSpace::getValueString()
	{
		return getStringFrom(parentControl->bGetValue());
	}
	void CCtrlEditSpace::setInternal(iCtrlPrec_t value)
	{
		parentControl->bSetInternal(cpl::Math::confineTo<iCtrlPrec_t>(value, 0.0, 1.0));
	}
	iCtrlPrec_t CCtrlEditSpace::getValue()
	{
		return parentControl->bGetValue();
	}

	void CCtrlEditSpace::focusLost(FocusChangeType cause)
	{
		// commit suicide
		delete this;
	}

	void CCtrlEditSpace::focusOfChildComponentChanged(FocusChangeType cause)
	{
		// commit suicide
		if (!hasKeyboardFocus(true) && !parentControl->bGetView()->hasKeyboardFocus(true))
			delete this;
	}

	void CCtrlEditSpace::visibilityChanged()
	{

		//fmtValueLabel.showEditor();

		resetToControl();

	}

	void CCtrlEditSpace::resetToControl()
	{
		std::string fmtText, intText;
		auto val = parentControl->bGetValue();
		CBaseControl::bMapIntValueToString(intText, val);
		parentControl->bFormatValue(fmtText, val);

		fmtValueLabel.setText(fmtText, juce::NotificationType::dontSendNotification);
		intValueLabel.setText(intText, juce::NotificationType::dontSendNotification);
	}

	bool CCtrlEditSpace::grabFocus()
	{
		fmtValueLabel.showEditor();
		fmtValueLabel.grabKeyboardFocus();
		return true;
	}

	void CCtrlEditSpace::looseFocus()
	{

		unfocusAllComponents();
	}

	void CCtrlEditSpace::setMaximumSize(int width, int height)
	{
		maximumSize.setXY(width, height);
	}
	void CCtrlEditSpace::setMode(bool shouldBeCompact)
	{
		if (shouldBeCompact)
		{
			compactMode = true;
			setSize(compactWidth, compactHeight);
		}
		else
		{
			compactMode = false;
			setSize(fullWidth, fullHeight);
		}

	}
	juce::String CCtrlEditSpace::bGetToolTipForChild(const Component * c) const
	{
		if (GUIUtils::ViewContains(&intValueLabel, c))
		{
			return "The internal 0.0 to 1.0 range of all controls.";
		}
		else if (GUIUtils::ViewContains(&fmtValueLabel, c))
		{
			return "What the internal value represents.";
		}
		return {};
	}
	void CCtrlEditSpace::onObjectDestruction(const CBaseControl::ObjectProxy & ctrl)
	{

		if (ctrl == parentControl)
		{
			// if the control ceases to exists, we have no meaningful existance.
			// attempt suicide here.
			parentControl = nullptr;
			delete this;
		}
	}

	CCtrlEditSpace::~CCtrlEditSpace()
	{
		notifyDestruction();
		// warning - the control may have been deleted at this point
		// if so, the callback on onDeathDestruction sets the parentControl pointer to zero.
		if (parentControl)
			parentControl->bRemoveChangeListener(this);

		getAnimator().removeChangeListener(this);
	}
	CBaseControl * CCtrlEditSpace::getBaseControl()
	{
		return parentControl;
	}
	void CCtrlEditSpace::labelTextChanged(juce::Label *labelThatHasChanged)
	{
		if (labelThatHasChanged == &fmtValueLabel)
		{
			auto const & editText = fmtValueLabel.getText();
			if (editText.length())
				interpretAndSet(editText.toStdString());

		}
	}

	void CCtrlEditSpace::changeListenerCallback(ChangeBroadcaster *source)
	{
		auto & animator = juce::Desktop::getInstance().getAnimator();
		if (source == &animator)
		{
			if (!animator.isAnimating(&errorVisualizer))
			{
				// animation finished, hide component
				errorVisualizer.isActive = false;
				if (inputValueWasValid && exitAfterAnimation)
					delete this;

			}
		}
	}
};