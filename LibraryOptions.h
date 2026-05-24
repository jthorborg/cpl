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

	file:LibraryOptions.h

		Compile-time switches for the library.

		#define CPL_HINT_FONT:
			if application is GUI, includes FreeType and uses
			it for getting outlines for glyphs.
		#define CPL_THROW_ON_NO_RESOURCE:
			if set, will throw an exception if cpl cannot find
			a needed resource.
		#define CPL_TRACEGUARD_ENTRYPOINTS
			if set, a top-level handler will be installed at common entry points
			(before user code in audio threads, async threads, opengl rendering etc.),
			that will catch soft- and hardware exceptions, display messages and log
			the exceptions with stacktraces etc.

*************************************************************************************/

#ifndef CPL_LIBRARYOPTIONS_H
#define CPL_LIBRARYOPTIONS_H
#include "MacroConstants.h"
#if defined(CPL_JUCE) && defined(_DEBUG)
#define DONT_SET_USING_JUCE_NAMESPACE 1
//#define TYPEFACE_BITMAP_RENDERING
//#define CPL_HINT_FONT
#define JUCE_ENABLE_LIVE_CONSTANT_EDITOR 1
#endif
//#define CPL_THROW_ON_NO_RESOURCE

#ifdef CPL_WINDOWS
#define CPL_MINIMUM_WINDOWS_SUPPORT NTDDI_WINXP
#else
#define CPL_MINIMUM_WINDOWS_SUPPORT 0
#endif
#define CPL_CACHEALIGNMENT 64

#define CPL_REMOVE_CWARN

#ifdef CPL_MAC
//#define CPL_CMOD_USECF
#endif

#endif
