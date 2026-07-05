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

		Various models for aggregating or smoothing profiled data for display or analysis.

*************************************************************************************/

#ifndef CPL_PROFILINGMODEL_H
#define CPL_PROFILINGMODEL_H

#include "Profiling.h"
#include <unordered_map>
#include <vector>
#include <stack>
#include <map>

namespace cpl
{
	namespace Profiling
	{
		/// <summary>
		/// An exponentially smoothed model tracking profiling spans and hieararchies over time and smoothing their durations / relative positions.
		/// </summary>
		class EWMAModel
		{
			using Scalar = float;

		public:
			using Seconds = Seconds<Scalar>;

		private:

			static constexpr int touchedSentinel = -1;
			static constexpr int untouchedSentinel = -2;

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
				Seconds self{}, total{}, parentOffset{};
				Seconds absentSeconds = (Seconds)touchedSentinel;

				// TODO: No longer tracking 'work'

				ChildNodeContainer<ModelNode> children;
			};

			using OrdinalLookup = std::pair<ModelNode*, Region::Identifier>;

			struct OrdinalLookupHash
			{
				size_t operator() (const OrdinalLookup& p) const
				{
					return std::hash<ModelNode*>()(p.first) + static_cast<std::uint16_t>(p.second);
				}
			};

			using OrdinalTracker = std::unordered_map<std::pair<ModelNode*, Region::Identifier>, std::uint32_t, OrdinalLookupHash>;

			struct Root : public ModelNode
			{
				double budget{};

				std::optional<Timestamp> frameStart, lastCommitTs;
				std::optional<Seconds> deltaT;
				OrdinalTracker ordinalTracker;
			};

		public:

			/// <summary>
			/// Drain all <see cref="FrameSnapshot"/> enqueued in the <paramref name="lane"/> somewhere else, and process them, updating the model.
			/// </summary>
			void consume(Lane& lane)
			{
				auto& root = lanes[lane.name];

				lane.drain(
					[&, this](const FrameSnapshot& snapshot)
					{
						// only record the start of the first (in a possible string of meta) frame(s).
						if (!root.frameStart)
						{
							root.frameStart = snapshot.startTs;

							if (root.lastCommitTs)
								root.deltaT = *root.frameStart - *root.lastCommitTs;
						}

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
			
			/// <summary>
			/// Sets how fast the model should update, where 1 ~= 63% progression towards the new state over 1 second.
			/// 0 yields an instant response (no smoothing).
			/// </summary>
			void setTimeConstant(Seconds seconds)
			{
				ewmaConstant = seconds;
			}

		private:

			void EWMA(Seconds& state, Seconds input, std::optional<Scalar> coeff)
			{
				if (coeff.has_value())
				{				
					state += (input - state) * *coeff;
				}
				else
				{
					state = input;
				}
			}

			void accumulate(Root& root, const FrameSnapshot& snapshot)
			{
				std::stack<std::pair<ModelNode*, Timestamp>> tree;
				// Everything is measured against the first frame, meta or not, for now.
				tree.push({ &root, *root.frameStart });

				// complement of retained fraction
				const auto coeff = root.deltaT ? 1 - std::exp(-*root.deltaT / ewmaConstant) : std::optional<Scalar>();

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
					auto& treetop = tree.top().first;
					const auto parentStart = tree.top().second;

					auto& children = treetop->children;

					// find first fitting ordinal... does not seem especially smart..
					auto ordinal = root.ordinalTracker[{treetop, span.region}]++;

					// get or spawn nth child tree with this region on this path
					auto& keyedChild = children[{ span.region, ordinal}];

					// finally, add data to staging / scratch
					keyedChild.absentSeconds = (Seconds)touchedSentinel;

					EWMA(keyedChild.self, span.self, coeff);
					EWMA(keyedChild.total, span.total, coeff);
					EWMA(keyedChild.parentOffset, span.start - parentStart, coeff);

					// push so if next is deeper it builds upon the current ordinal child
					tree.push({ &keyedChild, span.start });
				}
			}

			void prune(ChildNodeContainer<ModelNode>& nodes, Seconds deltaT /* kept for ghosts in the future */)
			{
				for (auto it = nodes.begin(); it != nodes.end();)
				{
					if (it->second.absentSeconds == (Seconds)untouchedSentinel)
					{
						// Could assert all children weren't touched..
						it = nodes.erase(it);
					}
					else
					{
						it->second.absentSeconds = (Seconds)untouchedSentinel;
						prune(it->second.children, deltaT);
						++it;
					}
				}
			}

			void commit(Root& root, double budget)
			{
				if (root.deltaT)
					prune(root.children, *root.deltaT);

				root.ordinalTracker.clear();
				root.budget = budget;
				root.lastCommitTs = root.frameStart;
				root.frameStart = std::nullopt;
			}

			// Identify lanes by name. The model doesn't have to link back to the lanes then.
			std::map<std::string, Root> lanes;
			Seconds ewmaConstant = (Seconds)1;
		};
	}
} // cpl

#endif
