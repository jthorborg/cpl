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

	file:CProcessorTimer.h

		Class that measures time spend between events in clocks and walltime.

*************************************************************************************/

#ifndef CPL_CPROCESSORTIMER_H
#define CPL_CPROCESSORTIMER_H

#include "MacroConstants.h"
#include "Misc.h"
#include "Mathext.h"
#include <cstdint>
#include <cstdlib>

#ifdef CPL_MAC
#define _CPL_USE_HCLOCK
#else
#include "system/SysStats.h"
#endif

namespace cpl
{
	/// <summary>
	/// A class that measures current core-clocks over time.
	/// Notice it may not be precise, if your thread is scheduled on another
	/// core.
	/// Useful when you want to measure real-time loop cpu usuage.
	/// </summary>
	class CProcessorTimer
	{
	public:
#ifdef _CPL_USE_HCLOCK
        typedef decltype(cpl::Misc::TimeCounter()) cclock_t;
#else
		typedef decltype(cpl::Misc::ClockCounter()) cclock_t;
#endif
        
		CProcessorTimer()
			: deltaT(), startT()
		{
		}

		/// <summary>
		/// Starts a new timing period. Calls reset() implicitly
		/// </summary>
		void start() noexcept
		{
			reset();
			startT = getClocks();
		}

		/// <summary>
		/// Ignores any time passed by until resume() is called.
		/// </summary>
		void pause() noexcept
		{
			deltaT = getClocks();
		}

		/// <summary>
		/// Resumes time measurement. See pause().
		/// </summary>
		void resume() noexcept
		{
			startT += getClocks() - deltaT;
		}

		/// <summary>
		/// Returns the number of clocks passed by since start() (excluding whatever happened between any pause/resumes)
		/// </summary>
		cclock_t getTime() const noexcept
		{
			return getClocks() - startT;
		}

		/// <summary>
		/// Resets any clocks. Use start().
		/// </summary>
		void reset()
		{
			startT = 0; deltaT = 0;
		}
        
        double coreUsage() const noexcept
        {
            return clocksToCoreUsage(getTime());
        }
        
		/// <summary>
		/// Returns a fraction, that represents how much of the core's capability was used 
		/// (ie. clocks_used / core_clocks_per_sec)
		/// </summary>
		static double clocksToCoreUsage(cclock_t clocks)
		{
#ifdef _CPL_USE_HCLOCK
            return cpl::Misc::TimeToSeconds(clocks);
#else
			return (0.001 * clocks) / (cpl::system::CProcessor::getMHz() * 1000);
#endif
		}

	private:

		static cclock_t getClocks()
		{
#ifdef _CPL_USE_HCLOCK
            return cpl::Misc::TimeCounter();
#else
			return cpl::Misc::ClockCounter();
#endif
		}

		cclock_t deltaT, startT;
	};


}; // cpl
#endif
