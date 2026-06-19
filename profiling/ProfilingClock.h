/*************************************************************************************

	cpl - cross-platform library - v. 0.1.0.

	Copyright (C) 2026 Janus Lynggaard Thorborg (www.jthorborg.com)

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

	file:ProfilingClock.h

		Layer 0 of the profiler: the timebase.

		Contract we agreed on (derive the code from this):
		  - now() is a GLOBAL, dumb free function returning raw absolute steady_clock
		    ticks. No epoch lookup, no context, no lock, no syscall => RT-safe.
		  - The stored type is the native integer rep of steady_clock (the numerator).
		    The period (denominator) is a compile-time std::ratio, stored nowhere.
		  - No float / no division on the hot path. Convert to seconds, and subtract
		    the per-run epoch, ONLY at display/export (epoch lives later, in the sink).

*************************************************************************************/

#ifndef CPL_PROFILINGCLOCK_H
#define CPL_PROFILINGCLOCK_H

#include <chrono>
#include <cstdint>

namespace cpl
{
	namespace Profiling
	{
		using std_clock = std::chrono::steady_clock;
		using Timestamp = std_clock::time_point;   // a point on the timeline (span start)
		using Elapsed = std_clock::duration;     // a length (total / self)

		inline Timestamp now() noexcept
		{
			return std_clock::now();
		}

		// TODO(you, later/at-display): the conversion helpers
		//   ticks -> seconds (using steady_clock::period), and span-start relative to
		//   a captured epoch. These are NOT on the hot path.
	}
}; // cpl

#endif
