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

*************************************************************************************/

#ifndef CPL_PROFILING_H
#define CPL_PROFILING_H

#include <string_view>

// Compile-time master gate. OFF => every macro/instrumentation point below
// expands to nothing (no string literals emitted); a new cpl-based plugin pays zero beyond empty support structures.
#ifndef CPL_PROFILING
	#define CPL_PROFILING 0
#else

	#include <atomic>
	#include <optional>
	#include <array>
	#include <limits>

	#include "../MacroConstants.h"
	#include "../Exceptions.h"
	#include "../lib/LockFreeDataQueue.h"
	#include "ProfilingClock.h"

	#ifndef CPL_PROFILING_MAXREGIONS 
	#define CPL_PROFILING_MAXREGIONS 255
	#endif

	#ifndef CPL_PROFILING_MAXDEPTH 
	#define CPL_PROFILING_MAXDEPTH 16 // sizeof ThreadState = 400
	#endif

	#ifndef CPL_PROFILING_MAXSPANS
	#define CPL_PROFILING_MAXSPANS 127 // sizeof FrameSnapshot = 4096
	#endif

#endif

namespace cpl
{
	namespace Profiling
	{
#if CPL_PROFILING
		constexpr static std::size_t MaxRegions = CPL_PROFILING_MAXREGIONS;
		constexpr static std::size_t MaxDepth = CPL_PROFILING_MAXDEPTH; 
		constexpr static std::size_t MaxSpans = CPL_PROFILING_MAXSPANS;

		struct Region
		{
			static_assert(MaxRegions <= std::numeric_limits<std::uint8_t>::max());

			enum class Identifier : std::uint8_t { Invalid = 0, Overflow = 1, First = 2, };
			const char* name;
			/* WorkUnit kind; */
		};

		struct Span
		{
			Timestamp start;
			Elapsed total;
			Elapsed self;
			// How much of the 'kind' was done, eg. samples - someone later will normalize over a sampling rate.
			std::uint32_t work;
			
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
			{
				regions[next].name = name;
				return static_cast<Region::Identifier>(next);
			}

			return Region::Identifier::Overflow;
		}

		inline const Region& resolveRegion(Region::Identifier identifier)
		{
			return regions[static_cast<std::size_t>(identifier)];
		}

		struct FrameSnapshot
		{
			static_assert(MaxSpans <= std::numeric_limits<std::uint16_t>::max());

			std::array<Span, MaxSpans> spans;
			std::uint16_t spanCount{};
			std::uint16_t droppedSpans{};

			// mostly for redundancy: consumer can see if there's a discontinuity
			std::uint32_t frameNumber;

			float work;
			float workDenominator;

			Timestamp startTs, stopTs;

			bool isCadenceFrame() const noexcept { return workDenominator == 0; }
		};

		class Lane
		{
			friend class ProfilerFrame;
			friend class EWMAModel;

		public:
			Lane(std::string_view name, bool isRealTime)
				: queue(8)
				, name(name)
				, frameCounter(0)
				, isRealTime(isRealTime)
				, enabled(false)
			{

			}

			// Single-drainer invariant: the lane queues are SPSC, so calls to this function
			// must be serialized with any other usage (like profiling models)
			void clear()
			{
				while (true)
				{
					Storage s;

					if (!queue.popElement(s))
						return;
				}
			}


			void setEnabled(bool shouldBeEnabled) noexcept { enabled = shouldBeEnabled; }

		private:

			template<typename IntegratingFunctor>
			void drain(IntegratingFunctor&& f)
			{
				queue.grow();

				while (true)
				{
					Storage s;

					if (!queue.popElement(s))
						return;

					f(*s.getData());
				}
			}

			typedef LockFreeDataQueue<FrameSnapshot>::ElementAccess Storage;

			LockFreeDataQueue<FrameSnapshot> queue;
			const std::string name;
			std::uint32_t frameCounter;
			const bool isRealTime;
			/// <summary>
			/// If enabled, created <see cref="ProfilerFrame"/>s will store data.
			/// Otherwise, the lane will only be drained and all profiling earlies out.
			/// </summary>
			cpl::weak_atomic<bool> enabled;
		};

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
			static_assert(MaxDepth <= std::numeric_limits<std::uint8_t>::max());
			Open stack[MaxDepth];
			// free-running open-count; array writes guarded by < MaxDepth
			std::uint8_t depth;    
			// depth when the current frame started
			std::uint8_t priorDepth;    

			FrameSnapshot* frame;
		};

		inline thread_local ThreadState tls{};   // constant-initialized POD, one per thread

		class ProfilerFrame
		{
		public:

			ProfilerFrame(Lane* lane)
			{
				CPL_RUNTIME_ASSERTION((tls.frame || tls.depth == 0) && "Unbalanced profiling TLS state");

				auto startT = now();
				// save/restore these regardless.
				priorFrame = tls.frame;
				priorDepth = tls.priorDepth;

				auto frameCount = lane ? lane->frameCounter++ : 0;

				storage.emplace();

				// Use the non-allocating path to avoid infinite growth if noone ever drains the profiler lanes.
				// Could even be argued we shouldn't grow either.
				const bool acquired = lane && lane->enabled && lane->queue.acquireFreeElement<false, true>(*storage);

				if (!acquired)
				{
					// important to disengage destructor
					storage.reset();
					// kill all subsequent profiling to not misattribute it to parent frame
					tls.frame = nullptr;
					return;
				}

				auto* snapshot = storage->getData();

				snapshot->frameNumber = frameCount;
				snapshot->startTs = startT;
				snapshot->droppedSpans = snapshot->spanCount = 0;
				// by default, frames are interpreted as cadence
				snapshot->work = 0;
				snapshot->workDenominator = 0;

				tls.priorDepth = tls.depth;
				tls.frame = snapshot;
			}

			/// <summary>
			/// By default, frames are interpreted as "cadence-like" which means they don't have deadline semantics
			/// and resulting budget computations are completely derived from their occurance and duration.
			/// Use this to encode how much work this represents as a fraction of how much must be done in a second.
			/// </summary>
			/// <param name="work">
			/// Some amount of work. It's possible to express zero work while having a denominator, in which case this frame
			/// may be interpreted as stitching to the next workful frame.
			/// </param>
			/// <param name="denominator">
			/// The amount of work per second, must always be specified but can change over time.
			/// </param>
			void setWork(float work, float denominator)
			{
				if (storage)
				{
					if (!(denominator > 0))
						CPL_RUNTIME_EXCEPTION("Work cannot be expressed over 0 or less time");

					if (!(work >= 0))
						CPL_RUNTIME_EXCEPTION("Work cannot be negative");

					auto& data = *storage->getData();

					// Until having thought more about this, don't submit work twice
					if (!data.isCadenceFrame())
						CPL_RUNTIME_EXCEPTION("Setting work twice");

					data.work = work;
					data.workDenominator = denominator;
				}
			}

			~ProfilerFrame()
			{
				if (storage)
					storage->getData()->stopTs = now();

				tls.priorDepth = priorDepth;
				tls.frame = priorFrame;
			}

		private:
			FrameSnapshot* priorFrame;
			std::optional<Lane::Storage> storage;
			std::uint32_t priorDepth;
		};

		// enter: push an Open at the current depth (array write guarded), then advance depth.
		inline void enter(Region::Identifier region) noexcept
		{
			if (!tls.frame)
				return;

			auto idx = tls.depth;

			if (idx < MaxDepth) 
				tls.stack[idx] = Open { now(), Elapsed{0}, region };

			// free-running: increment even when overflowing, to balance
			tls.depth++;
		}

		// exit: pop, compute total/self, deposit total into the parent's childTime, build the Span.
		// Returns the finished Span; Layer 3 will publish it to the bound lane instead.
		inline void exit(std::uint32_t work) noexcept
		{
			if (!tls.frame)
				return;

			// free-running: decrement even when overflowing
			tls.depth--;
			auto idx = tls.depth;
			auto& storage = *tls.frame;

			if (idx >= MaxDepth)
			{
				// we never opened a slot for this one, the region registers as Invalid
				storage.droppedSpans++;
				return;
			}

			Open& o = tls.stack[idx];
			auto total = now() - o.enterTs;
			auto self  = total - o.childTime;

			// if we are a child, deposit to parent
			if (idx > tls.priorDepth)
				tls.stack[idx - 1].childTime += total;

			if (storage.spanCount == storage.spans.size())
			{
				if (storage.droppedSpans < std::numeric_limits<decltype(storage.droppedSpans)>::max())
					storage.droppedSpans++;
				return;
			}

			storage.spans[storage.spanCount++] = Span { o.enterTs, total, self, work, o.region, static_cast<std::uint8_t>(idx - tls.priorDepth) };
		}

		class SpanScope
		{
		public:

			SpanScope(Region::Identifier identifier, std::uint32_t work)
				: work(work)
			{
				enter(identifier);
			}

			~SpanScope()
			{
				exit(work);
			}

		private:
			std::uint32_t work;
		};

		inline Region::Identifier loadOrAssignRegion(const char* name, std::atomic<Region::Identifier>& cached)
		{
			auto tempRegion = cached.load(std::memory_order_acquire);
			
			// assume loaded early return
			if (tempRegion != cpl::Profiling::Region::Identifier::Invalid)
				return tempRegion;

			tempRegion = cpl::Profiling::registerRegion(name); 
			auto expectedRegion = cpl::Profiling::Region::Identifier::Invalid; 

			if (!cached.compare_exchange_strong(expectedRegion, tempRegion, std::memory_order_release))
				tempRegion = expectedRegion; // potentially relinquish the old (only potentially possible)

			return tempRegion;
		}
#else
		class Lane
		{
		public:

			Lane(std::string_view name, bool isRealTime)
			{

			}

			void clear() { }
			void setEnabled(bool shouldBeEnabled) noexcept {  }

		private:
		};

		class ProfilerFrame
		{
		public:

			ProfilerFrame(Lane* lane) {}
			void setWork(float work, float denominator) {}
			~ProfilerFrame() {}

		private:
		};
#endif
	}
}; // cpl

#if CPL_PROFILING

#define CPL_PROFILE_INTERNAL(name, cachedName, work) \
	static std::atomic<cpl::Profiling::Region::Identifier> cachedName { cpl::Profiling::Region::Identifier::Invalid }; \
	cpl::Profiling::SpanScope CPL_CONCAT(scope, __COUNTER__) (cpl::Profiling::loadOrAssignRegion(name, cachedName), work);

// Not exception safe
#define CPL_PROFILE_EXPRESSION_INTERNAL(expression, cachedName) \
	static std::atomic<cpl::Profiling::Region::Identifier> cachedName { cpl::Profiling::Region::Identifier::Invalid }; \
	cpl::Profiling::enter(cpl::Profiling::loadOrAssignRegion(#expression, cachedName)); \
	expression; \
	cpl::Profiling::exit(0);

#define CPL_PROFILE(name) CPL_PROFILE_INTERNAL(name, CPL_CONCAT(profilerCached, __COUNTER__), 0)
#define CPL_PROFILE_BEGIN(name) { CPL_PROFILE_INTERNAL(name, CPL_CONCAT(profilerCached, __COUNTER__), 0)
#define CPL_PROFILE_END }
#define CPL_PROFILE_WORK(name, work) CPL_PROFILE_INTERNAL(name, CPL_CONCAT(profilerCached, __COUNTER__), work)
#define CPL_PROFILE_EXPRESSION(expr) CPL_PROFILE_EXPRESSION_INTERNAL(expr, CPL_CONCAT(profilerCached, __COUNTER__))

#else

#define CPL_PROFILE(name)
#define CPL_PROFILE_WORK(name, work)
#define CPL_PROFILE_EXPRESSION(expr) expr
#define CPL_PROFILE_BEGIN(name) {
#define CPL_PROFILE_END }

#endif

#endif
