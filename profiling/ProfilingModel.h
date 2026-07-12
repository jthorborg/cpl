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
		public:

			using Scalar = float;
			using Seconds = Seconds<Scalar>;

		private:

			static constexpr int touchedSentinel = 0;
			static constexpr int ghostTimeout = 1;

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
				std::optional<Timestamp> frameStart, lastCommitTs;
				std::optional<Seconds> deltaT;

				Seconds duration {};
				std::optional<Seconds> budget;
				std::optional<Scalar> coeff;

				OrdinalTracker ordinalTracker;
				std::uint32_t lastFrameNumber{};
				decltype(Span::depth) maxDepthSeen{};
				std::optional<bool> isCadenceLike;
			};

		public:

			struct LaneData
			{

				const std::string& getName() const noexcept { return name; }

				Seconds deltaTime() const noexcept { return *root.deltaT; }
				Scalar usage() const noexcept { return duration() / budget(); }

				Seconds duration() const noexcept { return root.duration; }
				Seconds budget() const noexcept { return *root.budget; }

				int maxDepthSeen() const noexcept { return static_cast<int>(root.maxDepthSeen); }
				bool isCadenceLike() const noexcept { return *root.isCadenceLike; }

				struct Layout
				{
					friend struct LaneData;

					const int maxDepthLevelsInLayout() const noexcept { return layoutDepth; }
					const int maxDepthEver() const noexcept { return maxDepth; }

					Layout(Layout&& other) = default;
					Layout& operator=(Layout&& other) = default;

				private:

					struct Node
					{
						typedef ChildNodeContainer<ModelNode>::const_iterator NodeSlot;

						NodeSlot node;
						Profiling::Seconds<double> start;
						int depth;
					};

					Layout(std::vector<Node>&& nodes, int layoutDepth, int maxDepth)
						: nodes(std::move(nodes))
						, layoutDepth(layoutDepth)
						, maxDepth(maxDepth)
					{

					}

					std::vector<Node> nodes;
					int layoutDepth, maxDepth;
				};

				template<typename Functor>
				void visit(Functor&& visitor) const
				{
					visit(std::forward<Functor>(visitor), buildLayout());
				}

				// receives (int depth, Region::Identifier, Seconds start, Seconds self, Seconds total)
				template<typename Functor>
				void visit(Functor&& visitor, const Layout& layout) const
				{
					for (const auto& visit : layout.nodes)
					{
						visitor(
							visit.depth, 
							visit.node->first.first, 
							visit.start, 
							visit.node->second.self, 
							visit.node->second.total
						);
					}
				}

				Layout buildLayout(Scalar parentSelfPruningThreshold = -std::numeric_limits<Scalar>::infinity()) const
				{
					int maxDepthLayout = 0;

					struct NodeVisit
					{
						const ChildNodeContainer<ModelNode>& nodes;
						Profiling::Seconds<double> runningPosition;
						int depth;
					};

					std::vector<NodeVisit> stack;
					std::vector<Layout::Node> result;

					stack.emplace_back(NodeVisit{ root.children, {}, 0 });

					while (stack.size())
					{
						auto top = stack.back();
						stack.pop_back();

						for (auto it = top.nodes.begin(); it != top.nodes.end(); ++it)
						{
							auto& node = it->second;
							auto position = top.runningPosition + node.parentOffset;
							auto proportion = node.self / node.total;

							auto isKept = node.children.empty() || !std::isnormal(proportion) || proportion > parentSelfPruningThreshold;

							if (isKept)
							{
								maxDepthLayout = std::max(maxDepthLayout, top.depth + 1);
								result.emplace_back(Layout::Node { it, position, top.depth });
							}

							stack.emplace_back(NodeVisit { node.children, position, top.depth + (isKept ? 1 : 0) });
						}
					}

					return Layout(std::move(result), maxDepthLayout, maxDepthSeen());
				}

				LaneData(const LaneData& other) = default;
				LaneData& operator = (const LaneData& other) = default;

			private:

				friend class EWMAModel;
				
				LaneData(const std::string& name, const Root& root) : name(name), root(root) {}

				const std::string& name;
				const Root& root;
			};

			std::optional<LaneData> getLaneData(const Lane& lane) const noexcept
			{
				auto it = lanes.find(lane.name);
				if (it == lanes.end())
					return std::nullopt;

				const auto& root = it->second;

				if (!root.budget || !root.deltaT || !root.isCadenceLike)
					return std::nullopt;

				return LaneData { it->first, it->second };
			}

			/// <summary>
			/// Drain all <see cref="FrameSnapshot"/> enqueued in the <paramref name="lane"/> somewhere else, and process them, updating the model.
			/// </summary>
			void consume(Lane& lane)
			{
				auto& root = lanes[lane.name];

				lane.drain(
					[&, this](const FrameSnapshot& snapshot)
					{
						// Handle broken snapshots later.
						if (snapshot.droppedSpans > 0)
							return;

						// only record the start of the first (in a possible string of meta) frame(s).
						if (!root.frameStart)
						{
							root.frameStart = snapshot.startTs;

							if (root.lastCommitTs)
							{
								root.deltaT = *root.frameStart - *root.lastCommitTs;
								// complement of retained fraction
								root.coeff = 1 - std::exp(-*root.deltaT / ewmaConstant.load(std::memory_order_relaxed));
							}
						}

						accumulate(root, snapshot);

						auto duration = snapshot.stopTs - *root.frameStart;

						if (!root.isCadenceLike)
							root.isCadenceLike = snapshot.isCadenceFrame();

						if (snapshot.isCadenceFrame())
						{
							if (!*root.isCadenceLike)
								CPL_RUNTIME_EXCEPTION("Lane switched from deadline mode to cadence");

							commit(root, duration, root.deltaT);
						}
						else if (snapshot.work != 0)
						{
							if (*root.isCadenceLike)
								CPL_RUNTIME_EXCEPTION("Lane switched from cadence mode to deadline, did you forget to setWork(0, ...)?");

							commit(root, duration, Seconds(snapshot.work / snapshot.workDenominator));
						}

						root.lastFrameNumber = snapshot.frameNumber;
					}
				);
			}
			
			/// <summary>
			/// Sets how fast the model should update, where 1 ~= 63% progression towards the new state over 1 second.
			/// 0 yields an instant response (no smoothing).
			/// </summary>
			void setTimeConstant(Seconds seconds)
			{
				ewmaConstant.store(seconds, std::memory_order_relaxed);
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

				// iterate in reverse order to start from roots and reconstruct backwards.
				// the recording format is in postfix "notation", so we can evaluate in one pass
				// with the consequence being children are stored in reverse order. 
				for (auto i = snapshot.spanCount; i --> 0;)
				{
					auto span = snapshot.spans[i];
					root.maxDepthSeen = std::max(root.maxDepthSeen, span.depth);
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
					keyedChild.absentSeconds = (Seconds)touchedSentinel;

					// finally, average data to model storage
					EWMA(keyedChild.self, span.self, root.coeff);
					EWMA(keyedChild.total, span.total, root.coeff);
					EWMA(keyedChild.parentOffset, span.start - parentStart, root.coeff);

					// push so if next is deeper it builds upon the current ordinal child
					tree.push({ &keyedChild, span.start });
				}
			}

			void prune(ChildNodeContainer<ModelNode>& nodes, Seconds deltaT)
			{
				for (auto it = nodes.begin(); it != nodes.end();)
				{
					if (it->second.absentSeconds > Seconds(ghostTimeout))
					{
						// Could assert all children weren't touched..
						it = nodes.erase(it);
					}
					else
					{
						it->second.absentSeconds += deltaT;
						prune(it->second.children, deltaT);
						++it;
					}
				}
			}

			void commit(Root& root, Seconds duration, std::optional<Seconds> budget)
			{
				if (root.deltaT)
					prune(root.children, *root.deltaT);

				root.ordinalTracker.clear();
				
				EWMA(root.duration, duration, root.coeff);

				if (budget)
				{
					if (!root.budget)
						root.budget = budget;

					EWMA(*root.budget, *budget, root.coeff);
				}

				root.lastCommitTs = root.frameStart;
				root.frameStart = std::nullopt;
			}

			// Identify lanes by name. The model doesn't have to link back to the lanes then.
			std::map<std::string, Root> lanes;
			std::atomic<Seconds> ewmaConstant = (Seconds)1;

			static_assert(std::atomic<Seconds>::is_always_lock_free, "Need a different pattern for updating time constants");
		};
	}
} // cpl

#endif
