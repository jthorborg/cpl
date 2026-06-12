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

	 file:CEditSpaceSpawner.h

		A class that automatically handles opening of edit spaces for any controls
		contained in a view.

 *************************************************************************************/

#ifndef CPL_CEDITSPACESPAWNER_H
#define CPL_CEDITSPACESPAWNER_H

#include "../Common.h"
#include "../Utility.h"
#include "CBaseControl.h"
#include <string>
#include "Tools.h"
#include "CCtrlEditSpace.h"
#include "../CMutex.h"
#include "DesignBase.h"

namespace cpl
{

	class CEditSpaceSpawner
		:
		public juce::MouseListener,
		public Utility::CNoncopyable,
		public Utility::DestructionServer<CCtrlEditSpace>::Client,
		public juce::ComponentListener
	{
	public:
		CEditSpaceSpawner(juce::Component & parentToControl);
		~CEditSpaceSpawner();

	protected:

		void appearWith(juce::Component & component);
		void disappear();
		virtual void onObjectDestruction(const CCtrlEditSpace::ObjectProxy & dyingSpace) override;
		virtual void componentMovedOrResized(Component &component, bool wasMoved, bool wasResized) override;
		virtual void mouseDoubleClick(const juce::MouseEvent & e) override;

		virtual void mouseDown(const juce::MouseEvent & e) override;

		bool recursionEdit;
		juce::Component & parent;
		std::unique_ptr<cpl::CCtrlEditSpace> currentEditSpace;

	private:
		CEditSpaceSpawner(const CEditSpaceSpawner &) = delete;

		class OpaqueComponent
			: public juce::Component
		{
			void paint(juce::Graphics & g) override
			{
				g.fillAll(GetColour(ColourEntry::Deactivated));

			}


		};

		OpaqueComponent dialog;
	};

};
#endif