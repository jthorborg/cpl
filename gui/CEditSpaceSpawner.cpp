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

	file:CEditSpaceSpawner.cpp

		Implementation of CEditSpaceSpawner.h

*************************************************************************************/

#include "CEditSpaceSpawner.h"

namespace cpl
{

	CEditSpaceSpawner::CEditSpaceSpawner(juce::Component & parentToControl)
		: parent(parentToControl), recursionEdit(false)
	{
		parent.addMouseListener(this, true);
		dialog.setName(programInfo.name + " edit space");
		dialog.setOpaque(true);
		// JUCE 8 rounds corners of dropshadow peers...
		dialog.addToDesktop(/*juce::ComponentPeer::StyleFlags::windowHasDropShadow | */ juce::ComponentPeer::StyleFlags::windowIsTemporary);
		dialog.setVisible(false);
	}

	CEditSpaceSpawner::~CEditSpaceSpawner()
	{
		parent.removeMouseListener(this);
	}

	void CEditSpaceSpawner::onObjectDestruction(const CCtrlEditSpace::ObjectProxy & dyingSpace)
	{
		if (dyingSpace == currentEditSpace.get())
		{
			currentEditSpace.release();
			currentEditSpace = nullptr;
			disappear();
		}
	}
	void CEditSpaceSpawner::componentMovedOrResized(Component &component, bool wasMoved, bool wasResized)
	{
		// when we move a component, we recieve a callback again (possibly recursive).
		// this avoids the infinite chain of resizing.
		// -- condition removed since we don't move them actually
		if (recursionEdit)
		{
			recursionEdit = false;
		}
		/*else */if (&component == currentEditSpace.get() && !wasMoved)
		{
			auto bounds = currentEditSpace->getBaseControl()->bGetAbsSize();
			auto topleft = currentEditSpace->getBaseControl()->bGetView()->getScreenPosition();

			recursionEdit = true;

			dialog.setBounds(topleft.getX(), topleft.getY() + bounds.getHeight(),
				currentEditSpace->getWidth(), currentEditSpace->getHeight());
		}
	}

	void CEditSpaceSpawner::appearWith(juce::Component & component)
	{
		//DBG_BREAK();
		disappear();
		component.setTopLeftPosition(0, 0);
		dialog.setSize(component.getBounds().getWidth(), component.getBounds().getHeight());
		dialog.addChildComponent(component);
		component.setVisible(true);
		dialog.setVisible(true);
		dialog.toFront(true);
		dialog.setAlwaysOnTop(true);
		//dialog
	}

	void CEditSpaceSpawner::disappear()
	{
		dialog.removeAllChildren();
		dialog.setVisible(false);
	}
	void CEditSpaceSpawner::mouseDoubleClick(const juce::MouseEvent & e)
	{
		cpl::CBaseControl * control = dynamic_cast<cpl::CBaseControl *>(e.eventComponent);

		if (!control)
		{
			control = dynamic_cast<cpl::CBaseControl *>(e.eventComponent->getParentComponent());
		}

		if (control)
		{
			// check if control is owned by a current edit space
			// - in which case we dont want to destroy the current.

			if (auto parentComponent = e.eventComponent->getParentComponent())
			{
				if (dynamic_cast<cpl::CCtrlEditSpace *>(parentComponent))
				{
					return;
				}
			}

			currentEditSpace = control->bCreateEditSpace();
			if (currentEditSpace)
			{
				currentEditSpace->addClientDestructor(this);

				auto bounds = control->bGetAbsSize();
				auto topleft = control->bGetView()->getScreenPosition();

				currentEditSpace->addComponentListener(this);
				dialog.setTopLeftPosition(topleft.getX(), topleft.getY() + bounds.getHeight());
				appearWith(*currentEditSpace.get());
			}
		}
	}

	void CEditSpaceSpawner::mouseDown(const juce::MouseEvent & e)
	{
		if (currentEditSpace.get())
		{
			auto controlView = currentEditSpace->getBaseControl()->bGetView();
			// close the view if the
			if (
				e.eventComponent != currentEditSpace.get() &&
				!currentEditSpace->isParentOf(e.eventComponent) &&
				e.eventComponent != controlView &&
				!controlView->isParentOf(e.eventComponent)
				)
			{
				currentEditSpace->looseFocus();
				currentEditSpace = nullptr;
				disappear();
			}
		}
	}
};
