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

	file:Profiling.h

		Umbrella header + compile-time configuration for the cpl runtime profiler.
		Always-on, nested-scope, lock-free (single-writer-per-lane) instrumentation.

		Map of the system (filled in layer by layer):
			ProfilingClock.h  - Layer 0: timebase (Timestamp + now())          <-- start here
			(Region/Span/Aggregate records)                                    - Layer 1
			(Open + TLS nesting stack)                                         - Layer 2
			(Lane + triple-buffered FrameSnapshot)                            - Layer 3
			(ProfilerSink)                                                    - Layer 4
			(ProfileFrame RAII binder + scope macros)                        - Layer 5

*************************************************************************************/

#ifndef CPL_PROFILING_H
#define CPL_PROFILING_H

#include "../MacroConstants.h"
#include "../Exceptions.h"

// Compile-time master gate. OFF => every macro/type/instrumentation point below
// expands to nothing (no string literals emitted); a new cpl-based plugin pays zero.
// Signalizer's build defines this to 1 so collection is always-on for end users.
#ifndef CPL_PROFILING
	#define CPL_PROFILING 0
#endif

#include "ProfilingClock.h"
#include <atomic>

namespace cpl
{
	namespace Profiling
	{
		constexpr static std::size_t MaxRegions = 256;
		constexpr static std::size_t MaxDepth = 8;
		constexpr static std::size_t MaxSpans = 64;

		struct Region
		{
			enum class Identifier : std::uint16_t { Invalid = 0, Overflow = 1, First = 2, };
			const char* name;
			/* WorkUnit kind; */
		};

		struct Span
		{
			Timestamp start;
			Elapsed total;
			Elapsed self;
			// How much of the 'kind' was done, eg. samples - someone later will normalize over a sampling rate.
			std::uint64_t work;
			
			// ascending alignment requirements
			Region::Identifier region;
			std::uint8_t depth;
		};

		inline Region regions[MaxRegions] = { "Invalid", "Overflow Marker", };

		inline Region::Identifier registerRegion(const char* name) noexcept
		{
			static std::atomic<std::uint32_t> counter{ 0 };

			auto next = counter.fetch_add(1, std::memory_order_relaxed) + static_cast<std::uint32_t>(Region::Identifier::First);

			if (next < MaxRegions)
				return static_cast<Region::Identifier>(next);

			return Region::Identifier::Overflow;
		}

		// ---- Layer 2: Open + per-thread nesting stack ------------------------------

		// Transient bookkeeping for a scope that is entered-but-not-yet-exited.
		// Mutable: children reach down and deposit into 'childTime' as they exit.
		// Dies on exit (its slot is reused by the next sibling at this depth).
		// 'depth' is NOT stored here - it is implied by the slot's position in the stack.
		struct Open
		{
			Timestamp enterTs;
			Elapsed childTime;          // running sum of children's *totals*
			Region::Identifier region;
		};

		// Per-thread transient nesting state. Pure bookkeeping, never published.
		// POD + constant-initialized (the {} below) => no dynamic-init guard on access:
		// every touch is a direct TLS-relative load, no first-touch ctor on the RT thread.
		struct ThreadState
		{
			Open stack[MaxDepth];
			std::uint32_t depth;        // free-running open-count; array writes guarded by < MaxDepth
			std::uint32_t droppedSpans; // bumped when depth overflowed MaxDepth (migrates to Lane in L3)
			// TODO(Layer 3): Lane* boundLane;
		};

		inline thread_local ThreadState tls{};   // constant-initialized POD, one per thread

		// enter: push an Open at the current depth (array write guarded), then advance depth.
		inline void enter(Region::Identifier region) noexcept
		{
			auto idx = tls.depth;

			if (idx < MaxDepth) 
				tls.stack[idx] = Open { now(), Elapsed{0}, region };

			// free-running: increment even when overflowing, to balance
			tls.depth++;
		}

		// exit: pop, compute total/self, deposit total into the parent's childTime, build the Span.
		// Returns the finished Span; Layer 3 will publish it to the bound lane instead.
		inline Span exit(std::uint64_t work) noexcept
		{
			// free-running: decrement even when overflowing
			tls.depth--;
			auto idx = tls.depth;
			
			if (idx >= MaxDepth)
			{
				// we never opened a slot for this one, the region registers as Invalid
				tls.droppedSpans++;
				return {};
			}

			Open& o = tls.stack[idx];
			auto total = now() - o.enterTs;
			auto self  = total - o.childTime;

			// if we are a child, deposit to parent
			if (idx > 0)
				tls.stack[idx - 1].childTime += total;

			return Span { o.enterTs, total, self, work, o.region, static_cast<std::uint8_t>(idx) };
		}

	}
}; // cpl

#endif
