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

	file:CTimer.h

		Class that measures time spend between events in clocks and walltime.

*************************************************************************************/

#ifndef _CTIMER_H
#define _CTIMER_H

#include "Utility.h"
#include "Misc.h"
#include "Mathext.h"
#include <cstdint>
#include <vector>
#include <ostream>
#include <cstdlib>
#include <atomic>
#include "system/SysStats.h"

namespace cpl
{
	namespace
	{
		typedef std::int64_t time_object;
		auto static getClocks = []() { return cpl::Misc::ClockCounter(); };
		auto static timeToMs = [](time_object time) { return cpl::Misc::TimeToMilisecs(time); };
		auto static getTime = []() { return cpl::Misc::TimeCounter(); };
	};
	class CTimer
	{
	private:


	public:
		template<class ClockObj, class TimeObj>
		class basic_time_event;

		typedef basic_time_event<time_object, time_object> time_event;
		typedef time_event TimeProxy;

		CTimer()
			: size(0)
		{
			if (!hasBeenTuned)
			{
				tune();
				hasBeenTuned = true;
			}
		}

		void start()
		{
			events.clear();
			events.reserve(size);
			startEvent = measure();
		}

		void setSize(std::size_t size)
		{
			this->size = size;
			events.reserve(size);
		}

		void postEvent()
		{
			events.push_back(measure());
		}

		template<typename T = double>
		T getTotalElapsedTime()
		{
			if (events.size())
				return timeToMs(static_cast<time_object>(((events[events.size() - 1] - startEvent) - delta).getTime()));
			else
				return 0;
		}

		template<typename T = double>
		T getTotalElapsedClocks()
		{
			if (events.size())
				return static_cast<time_object>(((events[events.size() - 1] - startEvent) - delta).getClocks());
			else
				return 0;
		}

		template<typename T = double>
		T getAverageElapsedTime()
		{
			T sum(0);
			if (events.size())
			{

				for (auto i = events.size() - 1; i; --i)
				{
					sum += (events[i] - events[i - 1] - delta).getTime();
				}

				sum += (startEvent - events[0] - delta).getTime();

				sum /= events.size();
			}
			return timeToMs(static_cast<time_object>(sum));
		}

		template<typename T = double>
		T getAverageElapsedClocks()
		{
			T sum(0);
			if (events.size())
			{

				for (auto i = events.size() - 1; i; --i)
				{
					sum += (events[i] - events[i - 1] - delta).getClocks();
				}

				sum += (startEvent - events[0] - delta).getClocks();

				sum /= events.size();
			}
			return (sum);
		}


		void postEvent(TimeProxy t)
		{
			events.push_back(t);
		}

		static TimeProxy measure()
		{
			return time_event(getClocks(), getTime());
		}

		template<typename T>
		static double clocksToMSec()
		{
			return 0;
		}

		static void tune()
		{
			// might also want to include postEvent() cycle time
			const unsigned measurements = 60;
			auto clockCold = getClocks();
			auto timeCold = getTime();
			cpl::CBoxFilter<double, measurements> clockFilter;
			{
				auto t1 = getClocks();
				clockCold = t1 - clockCold;
				for (unsigned i = 0; i < measurements; ++i)
				{
					auto const t2 = getClocks();
					auto const t3 = t2 - t1;
					clockFilter.setNext(static_cast<double>(t3));
					t1 = t2;
				}
			}
			cpl::CBoxFilter<double, measurements> timeFilter;
			{
				auto t1 = getTime();
				timeCold = t1 - timeCold;
				for (unsigned i = 0; i < measurements; ++i)
				{
					auto const t2 = getTime();
					auto const t3 = t2 - t1;
					timeFilter.setNext(static_cast<double>(t3));
					t1 = t2;
				}
			}
			delta = time_event
			(
				cpl::Math::round<time_object>((clockFilter.getAverage() + clockCold) / 2),
				cpl::Math::round<time_object>((timeFilter.getAverage() + timeCold) / 2)
			);
		}

		template<class ClockObj, class TimeObj>
		class basic_time_event
		{
		public:

			typedef ClockObj ClockTy;
			typedef TimeObj TimeTy;

			basic_time_event(ClockTy clocks, TimeObj time, const basic_time_event & r)
				: clocks(clocks), time(time), ref(&r)
			{

			}

			basic_time_event(ClockTy clocks, TimeObj time, const basic_time_event * r = nullptr)
				: clocks(clocks), time(time), ref(r)
			{

			}

			basic_time_event()
				: clocks(0), time(0), ref(nullptr)
			{

			}

			basic_time_event & set(ClockTy clocks, TimeObj time)
			{
				this->time = time;
				this->clocks = clocks;
				return *this;
			}

			basic_time_event & setRef(const basic_time_event & r)
			{
				ref = &r;
				return *this;
			}

			basic_time_event & setRef(const basic_time_event * r)
			{
				ref = r;
				return *this;
			}

			basic_time_event reference() const
			{
				if (ref)
				{
					return basic_time_event(clocks - ref->clocks, time - ref->time);
				}
				else
					return *this;
			}
			basic_time_event reference(const basic_time_event & other) const
			{
				return basic_time_event(clocks - other.clocks, time - other.time);
			}

			basic_time_event operator - (const basic_time_event & other) const
			{
				return reference(other);
			}

			ClockTy getClocks() const { return clocks; }
			TimeObj getTime() const { return time; }
		private:

			ClockTy clocks;
			TimeObj time;
			const basic_time_event * ref;
		};
		static time_event delta;
		static std::atomic<bool> hasBeenTuned;
		std::vector<time_event> events;
		time_event startEvent;
		std::size_t size;
	};


}; // cpl
#endif
