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

	file:ProfilingModel.h

		Consumer-side (off-RT) aggregation of profiling Lanes into a persistent, smoothed
		call-tree. Pure consumer utility - never touched by a producer, never on an RT
		thread; depends on the always-on core in Profiling.h but is kept separate so the
		"producers pay zero" boundary stays crisp.

		Settled design contract (student/teacher session 2026-06-28):

		 - Drain a Lane's FrameSnapshots and integrate each. Spans inside a snapshot are
		   POST-ORDER (appended on exit, LIFO): children precede parents, the root exits
		   last. Reconstruct via a stack: front->back, pop everything deeper than the
		   current span (= its children, in reversed/LIFO order), then push the current.
		   Tolerate orphans: on MaxSpans overflow the OUTERMOST spans are dropped, so a
		   snapshot may be a forest, not a single-rooted tree.

		 - Persistent model is a FOREST: one root per lane. A node is keyed by PATH of
		   (region, ordinal) pairs, where 'ordinal' is the occurrence index of that region
		   among its siblings in execution order (recovered consumer-side from array order;
		   the recorder does NOT count collisions). Same region under different parents, or
		   repeated as a sibling, stays a distinct node. Positional identity is inherent and
		   imperfect (kill the 1st of N same-region siblings and the survivors shift up, the
		   last fades) - accepted limitation.

		 - Model holds SMOOTHED SCALARS ONLY (self/total/work + structure). NO stored
		   positions. Layout is a render-time top-down pass that distributes each parent's
		   width across children proportional to total and normalizes any overshoot into the
		   parent's width, so geometry closes by construction and the EWMA lag never overflows.

		 - Smoothing: asymmetric EWMA, alpha = exp(-dt/tau), dt from consecutive frame
		   startTs (WALL-TIME weighted, never per-frame). Present node -> fast attack / slow
		   release. Absent node -> release-only toward zero (sample = 0): the bar shrinks and
		   GCs below an epsilon. Decay-to-zero is for truthfulness + fade-out feel; geometry
		   is already handled at render.

		 - META-FRAME FOLDING: a snapshot with frame.work == 0 is meta work (e.g. a
		   MixGraphListener cycle where the sidechain hasn't filled yet). It has no load
		   denominator and a different tree shape, so integrating it as its own EWMA sample
		   would spuriously fade the real nodes. So integrate is TWO stages:
		     1. reconstruct + ACCUMULATE every snapshot (meta or real) into a persistent
		        path-keyed scratch tally (sum self/total/work).
		     2. COMMIT only when frame.work > 0: blend scratch into EWMA, decay untouched
		        nodes, GC, reset scratch. dt spans the whole folded window (last commit->now).
		   Ordinal stays per-snapshot, so the same call across folded cycles sums by path
		   (not split into siblings). Shared-prefix inflation (prefix ran N times, leaf once)
		   is truthful = per-real-output load. Watch: if work never arrives the scratch
		   magnitude grows unbounded -> commit spike; cap with a max-fold-time later if needed.

*************************************************************************************/

#ifndef CPL_PROFILINGMODEL_H
#define CPL_PROFILINGMODEL_H

#include "Profiling.h"
#include <unordered_map>
#include <vector>
#include <stack>

namespace cpl
{
	namespace Profiling
	{
		class EWMAModel
		{
			static constexpr int touchedSentinel = -1;

			using Key = std::pair<Region::Identifier, std::uint16_t /* ordinal*/>;

			struct KeyHash
			{
				size_t operator() (const Key& p) const
				{
					return static_cast<std::uint16_t>(p.first) << 16 | p.second;
				}
			};

			template<class Node>
			using ChildNodeContainer = std::unordered_map<Key, Node, KeyHash>; // Opportunity to use flat_map later on when C++ standard upgrades.

			struct ModelNode
			{
				Elapsed stagingSelf{}, stagingTotal{};
				std::uint32_t stagingWork{};

				float self{}, total{}, work{};

				float absentSeconds = touchedSentinel;

				ChildNodeContainer<ModelNode> children;
			};

			struct Root : public ModelNode
			{
				// Identify lanes by name. The model doesn't have to link back to the lanes then.
				std::string displayName;
				double budget{};

				std::optional<Timestamp> frameStart, lastCommitTs;
				std::optional<Elapsed> deltaT;
			};

			using OrdinalLookup = std::pair<ModelNode*, Region::Identifier>;

			struct OrdinalLookupHash
			{
				size_t operator() (const OrdinalLookup& p) const
				{
					return std::hash<ModelNode*>()(p.first) + static_cast<std::uint16_t>(p.second);
				}
			};

		public:
			void consume(Lane& lane)
			{
				auto& root = getRoot(lane);

				lane.drain(
					[&, this](const FrameSnapshot& snapshot)
					{
						// only record the start of the first (in a possible string of meta) frame(s).
						if (!root.frameStart)
							root.frameStart = snapshot.startTs;

						// Handle broken snapshots later.
						if (snapshot.droppedSpans > 0)
							return;

						accumulate(root, snapshot);

						if (snapshot.work == 0)
							return;

						auto denominator = lane.workUnitsPerSecond.load(std::memory_order_acquire);

						CPL_RUNTIME_ASSERTION(denominator > 0);

						commit(root, snapshot.work / denominator);
					}
				);
			}

		private:

			void accumulate(Root& root, const FrameSnapshot& snapshot)
			{
				std::stack<ModelNode*> tree;
				tree.push(&root);

				// this isn't tracked state to cause snapshots to fold correctly.
				std::unordered_map<std::pair<ModelNode*, Region::Identifier>, std::uint32_t, OrdinalLookupHash> ordinalTracker;

				// iterate in reverse order to start from roots and reconstruct backwards.
				// the recording format is in postfix "notation", so we can evaluate in one pass
				// with the consequence being children are stored in reverse order. 
				for (auto i = snapshot.spanCount; i --> 0;)
				{
					auto span = snapshot.spans[i];
					// trim the tree if we've exited out
					while (tree.size() > span.depth + 1) // (tree always has +1 entry)
						tree.pop();

					// re-enter tree
					auto& children = tree.top()->children;

					// find first fitting ordinal... does not seem especially smart..
					auto ordinal = ordinalTracker[{tree.top(), span.region}]++;

					// get or spawn nth child tree with this region on this path
					auto& keyedChild = children[{ span.region, ordinal}];

					// finally, add data to staging / scratch
					keyedChild.absentSeconds = touchedSentinel;
					keyedChild.stagingSelf += span.self;
					keyedChild.stagingTotal += span.total;
					keyedChild.stagingWork += span.work;

					// push so if next is deeper it builds upon the current ordinal child
					tree.push(&keyedChild);
				}
			}

			void commit(Root& root, double budget)
			{
				if (root.lastCommitTs)
					root.deltaT = *root.frameStart - *root.lastCommitTs;

				// BODY TODO

				root.budget = budget;
				root.lastCommitTs = root.frameStart;
				root.frameStart = std::nullopt;
			}

			Root& getRoot(Lane& lane)
			{
				Root* root = nullptr;

				for (auto& candidate : lanes)
				{
					if (candidate.displayName == lane.name)
					{
						root = &candidate;
						break;
					}
				}

				if (root == nullptr)
				{
					root = &lanes.emplace_back();
					root->displayName = lane.name;
				}
				
				return *root;
			}

			std::vector<Root> lanes;
		};
	}
} // cpl

#endif
