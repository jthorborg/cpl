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

	file:core.h

*************************************************************************************/

#ifndef CPL_CORE_H
#define CPL_CORE_H

#include "MacroConstants.h"
#include "lib/string_ref.h"
#include "lib/uarray.h"
#include <utility>
#include <cstdio>

namespace cpl
{
	template<typename... Args>
	inline std::string format(const cpl::string_ref format, Args&& ... args)
	{
		using namespace std;
		size_t size = snprintf(nullptr, 0, format.data(), std::forward<Args>(args)...) + 1;
		string ret;
		ret.resize(size);
		snprintf(ret.data(), size, format.data(), std::forward<Args>(args)...);
		return ret;
	}

	template<std::size_t Size, typename... Args>
	inline int sprintfs(char (&dest)[Size], const cpl::string_ref format, Args&&... args)
	{
		return std::snprintf(dest, Size, format.data(), std::forward<Args>(args)...);
	}
} 

#endif
