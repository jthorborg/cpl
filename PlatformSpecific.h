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

	file:Platformspecific.h

		Defines operating system and includes common system files.
		Defines some types according to system.

*************************************************************************************/

#ifndef CPL_PLATFORMDEPENDENT_H
#define CPL_PLATFORMDEPENDENT_H

#include "MacroConstants.h"

#ifdef CPL_WINDOWS

#include <windows.h>
#include <tchar.h>
#include <intrin.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef CPL_JUCE

#include <juce_opengl/juce_opengl.h>

namespace cpl
{
	using namespace juce::gl;
}

#else

#include <gl/gl.h>
#include <gl/glu.h>

#endif

#elif defined(CPL_UNIXC)

#include <dlfcn.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/time.h>
#include <fcntl.h>
#include <dirent.h>

#ifdef CPL_MAC

#include <mach-o/dyld.h>
#include <mach/mach_time.h>
#include "MacSupport.h"
#include <sys/sysctl.h>

#ifndef CPL_JUCE
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#endif

#endif

#include <setjmp.h>

#endif

#ifndef CPL_MSVC
#include <cfenv>
// find similar header (set fpoint mask) for non-mscv on windows
#ifdef __x86_64__
#include <xmmintrin.h>
#elif defined(__aarch64__) || defined(__arm64__)
#define SIMDE_ENABLE_NATIVE_ALIASES
#include "external/simde/simde/x86/sse.h"
#endif
#endif

#endif
