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

	file:JuceModelDisplay.h

		Utilities and juce::Components for displaying profiling models.

*************************************************************************************/

#ifndef CPL_JUCEMODELDISPLAY_H
#define CPL_JUCEMODELDISPLAY_H

#if !CPL_PROFILING
#error "Cannot use JuceModelDisplay.h without defining CPL_PROFILING"
#endif

#include "ProfilingModel.h"
#include "../Common.h"
#include "../Mathext.h"
#include "../gui/controls/Controls.h"

namespace cpl
{
	namespace Profiling
	{
		struct EWMALaneJuceRenderer
		{
			friend struct EWMAModel::LaneData; // Give access to operator()
			using Model = EWMAModel;
			using Scalar = float;

			constexpr static int space = 1;

		public:

			EWMALaneJuceRenderer(
				Model::LaneData data,
				juce::Rectangle<int> bounds,
				juce::Point<Scalar> zoom = juce::Point<Scalar>(0, 1),
				Scalar parentPruning = -std::numeric_limits<Scalar>::infinity(),
				std::optional<Model::Seconds> laneLength = std::nullopt // by default, the lane is "budget" long
			)
				: data(data)
				, layout(data.buildLayout(parentPruning))
				, bounds(bounds.toFloat())
				, window(zoom)
				, depthLevelsRequired(layout.maxDepthLevelsInLayout() + 1)
				, yPixelsForHeight(std::min(20, (bounds.getHeight() - space * depthLevelsRequired) / depthLevelsRequired))
				, length(laneLength.value_or(data.budget()))
			{

			}

			void paint(
				juce::Graphics& g,
				juce::Colour spanFill,
				juce::Colour outline,
				std::optional<juce::Colour> text = std::nullopt,
				std::optional<juce::Colour> hot = std::nullopt)
			{
				juce::Font old = g.getCurrentFont();
				juce::Font newFont;

				newFont.setTypefaceName(juce::Font::getDefaultMonospacedFontName());
				g.setFont(newFont);

				this->g = &g;
				spanColour = spanFill;
				textColour = text.value_or(spanColour.contrasting());
				hotColour = hot.value_or(spanColour);
				outlineColour = outline;

				// Render the name of the lane as a root node
				renderNode(-1, data.getName().c_str(), Model::Seconds(0), data.duration(), std::nullopt);
				// Then all the children.
				data.visit(*this, layout);

				char buffer[1024];

				cpl::sprintfs(
					buffer,
					"FPS: %6.2f (budget: %6.2f ms)\nDelta Time: %5.2f ms",
					1.0 / data.budget().count(),
					data.budget().count() * 1000,
					data.deltaTime().count() * 1000
				);

				auto topLeft = bounds.getTopLeft();

				g.setColour(textColour);
				g.drawMultiLineText(buffer, Math::round<int>(topLeft.x), Math::round<int>(topLeft.y + 20), 400);

				// lane outlines
				float paths[2] = { 5, 5 };

				auto x = secondsToX(Model::Seconds(0));
				g.drawDashedLine(
					{ x, bounds.getY() + bounds.getHeight() * 0.5f, x, bounds.getBottom() },
					paths,
					2, // elements in paths
					2 // line thickness
				);

				x = secondsToX(data.budget());
				g.drawDashedLine(
					{ x, bounds.getY() + bounds.getHeight() * 0.5f, x, bounds.getBottom() },
					paths,
					2, // elements in paths
					2 // line thickness
				);

				g.setFont(old);
			}

		private:

			juce::Colour spanColour;
			juce::Colour outlineColour;
			juce::Colour textColour;
			juce::Colour hotColour;

			juce::Graphics* g = nullptr;

			const Model::LaneData data;
			const Model::LaneData::Layout layout;
			const juce::Rectangle<Scalar> bounds;
			const juce::Point<Scalar> window;
			const int depthLevelsRequired;
			const int yPixelsForHeight;
			const Model::Seconds length;

			Scalar secondsToX(Model::Seconds seconds) const
			{
				const auto normalized = seconds / length;
				const auto zoomed = (normalized - window.getX()) / (window.getY() - window.getX());
				return bounds.getX() + zoomed * bounds.getWidth();
			}

			void operator() (int depth, Region::Identifier identifier, Model::Seconds start, Model::Seconds self, Model::Seconds total) const
			{
				renderNode(
					depth,
					resolveRegion(identifier).name,
					start,
					total,
					self
				);
			}

			void renderNode(int depth, const char* name, Model::Seconds start, Model::Seconds total, std::optional<Model::Seconds> self) const
			{
				auto left = secondsToX(start);
				auto right = secondsToX(start + total);

				// depth + 1 since root is painted as well below
				auto bottom = bounds.getBottom() - (depth + 1) * (yPixelsForHeight + space);
				auto top = bottom - yPixelsForHeight;

				auto rect = juce::Rectangle<float>(
					left, // x
					top, // y
					right - left, // width
					static_cast<float>(yPixelsForHeight - space) // height
				);

				// Have the text clip to borders to it doesn't disappear
				auto visibleRect = rect;
				auto borderingRect = rect;

				// early out if window completely culls
				if (!bounds.intersectRectangle(visibleRect))
					return;

				auto leftDiff = bounds.getX() - rect.getX();
				auto rightDiff = rect.getRight() - bounds.getRight();

				if (leftDiff > 1)
					borderingRect = borderingRect.withTrimmedLeft(leftDiff - 1);

				if (rightDiff > 1)
					borderingRect = borderingRect.withTrimmedRight(rightDiff - 1);

				// no 'self' means we measure against the whole frame.
				// really only here to reuse this function drawing for the root node.
				const auto target = self ? data.duration() : data.budget();
				auto selfProportion = std::clamp(self ? *self / total : total / target, 0.0f, 1.0f);

				const auto finalSpanColour = spanColour.interpolatedWith(hotColour, selfProportion);

				g->setColour(finalSpanColour);
				g->fillRect(visibleRect);

				g->setColour(outlineColour);
				g->drawRect(borderingRect);

				char buffer[2048];

				cpl::sprintfs(
					buffer,
					"%s %5.2f%%\t (%5.2f ms)",
					name,
					(total / target) * 100,
					total.count() * 1000
				);

				g->setColour(textColour);
				g->drawFittedText(buffer, visibleRect.toNearestInt(), juce::Justification::centred, 1, 1.0f);
			}
		};

		class EWMAProfilerComponent
			: public juce::Component
			, private juce::Timer
			, private ValueEntityBase::ValueEntityListener
		{
			static constexpr int kTimerFrequency = 30;
			static constexpr int kBorder = 5;
			static constexpr int kControlPaneHeight = 40;

		public:

			enum class TimeAxisModes
			{
				IndependentBudget,
				IndependentDuration,
				AlignedBudget,
				AlignedDuration
			};

			EWMAProfilerComponent(const std::vector<std::shared_ptr<Lane>>& lanes);
			~EWMAProfilerComponent();

			void setTimeAxisMode(TimeAxisModes mode);
			void setParentPruningProportion(double value);

		private:

			void valueEntityChanged(ValueEntityListener* sender, ValueEntityBase* value) override;
			void timerCallback() override;
			void paint(juce::Graphics& g) override;

			// zooms view offsets
			void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;
			// resets view offsets on left click, freezes on right click
			void mouseDoubleClick(const juce::MouseEvent& event) override;
			void mouseUp(const juce::MouseEvent& e) override;
			void mouseDown(const juce::MouseEvent& e) override;
			// translates view offsets
			void mouseDrag(const juce::MouseEvent& event) override;

			std::vector<std::shared_ptr<Lane>> lanes;
			EWMAModel model;
			juce::Point<double> viewOffsets{ 0, 1 };
			std::optional<double> priorDragPosition;

			ExponentialRange<ValueT> pruneRange;
			PercentageFormatter<ValueT> pruneFormatter;
			SelfcontainedValue<> pruneValue;
			CValueKnobSlider pruneControl;

			ChoiceTransformer<ValueT> timeRange;
			ChoiceFormatter<ValueT> timeChoices;
			SelfcontainedValue<> timeAxisValue;
			std::unique_ptr<CValueComboBox> timeAxisControl;
		};
	}
} // cpl

#endif
