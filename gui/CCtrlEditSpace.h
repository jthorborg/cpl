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

	file:CCtrlEditSpace.h

		An UI for CBaseControls that allows editing their internal and semantic values.
		Can be subclassed to extend functionality.

*************************************************************************************/

#ifndef CPL_CCTRLEDITSPACE_H
#define CPL_CCTRLEDITSPACE_H

#include "CBaseControl.h"
#include "BuildingBlocks.h"
#include "Tools.h"

namespace cpl
{

	class CTriangleButton;
	class CButton;
	/*
		The base class used to represent edit spaces for controls.
		Controls deriving from base controls should derive a class from this,
		if they want to add specific editing controls available
		(colour wheels come to mind).
	*/
	class CCtrlEditSpace
		:
		public juce::Component,
		public juce::Label::Listener,
		public CBaseControl::Listener,
		public juce::TextEditor::Listener,
		public Utility::DestructionServer<CCtrlEditSpace>,
		public juce::ChangeListener,
		public juce::Button::Listener,
		public CToolTipClient,
		public DestructionNotifier
	{

	public:


		const int elementHeight = 22;


		friend class CBaseControl;
		virtual iCtrlPrec_t interpretString(const std::string & value);
		virtual bool interpretAndSet(const std::string & value);
		virtual std::string getStringFrom(iCtrlPrec_t value);
		virtual std::string getValueString();
		virtual void setInternal(iCtrlPrec_t value);
		virtual iCtrlPrec_t getValue();

		void setMaximumSize(int width, int height);
		virtual void setMode(bool shouldBeCompact = true);

		virtual ~CCtrlEditSpace();
		virtual cpl::CBaseControl * getBaseControl();

		bool grabFocus();
		void looseFocus();
		void resetToControl();

		void animateSucces(juce::Component * objectThatWasModified);
		void animateError(juce::Component * objectThatWasModified);


	protected:
		// protected so we ensure someone doesn't stack allocate this.
		CCtrlEditSpace(cpl::CBaseControl * parent);


		// overrides
		virtual void paint(juce::Graphics & g) override;
		virtual void labelTextChanged(juce::Label *labelThatHasChanged) override;
		virtual void resized() override;
		virtual void visibilityChanged() override;
		virtual void textEditorReturnKeyPressed(TextEditor &) override;
		virtual void changeListenerCallback(ChangeBroadcaster *source) override;
		virtual void editorShown(Label *, TextEditor &) override;
		virtual void valueChanged(const CBaseControl * ctrl) override;
		virtual juce::String bGetToolTip() const override;
		virtual void buttonClicked(juce::Button *) override;
		virtual void focusOfChildComponentChanged(FocusChangeType cause) override;
		virtual void focusLost(FocusChangeType cause) override;
		virtual void createSimpleViewEditor();
		virtual juce::String bGetToolTipForChild(const Component * c) const override;
		virtual void onObjectDestruction(const CBaseControl::ObjectProxy & object) override;
		juce::ComponentAnimator & getAnimator() { return juce::Desktop::getInstance().getAnimator(); }


		SemanticBorder errorVisualizer;
		// only to avoid cyclic dependency
		std::unique_ptr<CTriangleButton> expanderButton;

		juce::DrawableImage
			iconSucces,
			iconError;

		int
			compactWidth,
			compactHeight,
			fullWidth,
			fullHeight;
		std::string toolTip;

	private:
		std::string exportedControlName;
		juce::Point<int> maximumSize;
		std::unique_ptr<cpl::CButton> switchWithOld;
		cpl::CBaseControl * parentControl;
		cpl::CSerializer oldValue;
		juce::Label intValueLabel;
		bool compactMode;
		volatile bool inputValueWasValid;
		volatile bool exitAfterAnimation;
		bool hasBeenInitialized;
		juce::Label fmtValueLabel;
	};


};
#endif