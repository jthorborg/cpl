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

	file:JuceModelDisplay.cpp

		Implementation of JuceModelDisplay.h

*************************************************************************************/

#include "JuceModelDisplay.h"

namespace cpl
{
	namespace Profiling
	{
		EWMAProfilerComponent::EWMAProfilerComponent(const std::vector<std::shared_ptr<Lane>>& lanes)
			: lanes(lanes)
			, pruneRange(0.0001, 0.99)
			, pruneValue(&pruneRange, &pruneFormatter)
			, pruneControl(&pruneValue)
			, timeChoices(timeRange)
			, timeAxisValue(&timeRange, &timeChoices)
		{
			for (auto& lane : lanes)
			{
				// discard any potentially old frames in the queue, so we don't get a huge d/T right off the bat.
				lane->clear();
				lane->setEnabled(true);
			}

			// Each lane pools 8 snapshots and producers drop (never allocate) when full,
			// so a drain rate below the fastest producer only decimates - it doesn't break anything.
			startTimerHz(kTimerFrequency);
			timeChoices.setValues({ "Budget; independent", "Duration; independent", "Budget; aligned", "Duration; aligned" });

			timeAxisControl = std::make_unique<CValueComboBox>(&timeAxisValue);

			pruneControl.bSetTitle("Prune parents");
			pruneControl.bSetDescription("Avoid showing parent profiler sections whos self-time percentage is less than this");

			timeAxisControl->bSetTitle("Axis scaling");
			timeAxisControl->bSetDescription("Select how the lanes' time axes are scaled");

			setTimeAxisMode(TimeAxisModes::IndependentDuration);
			setParentPruningProportion(0.01);

			pruneControl.bSetPos(kBorder, kBorder);
			timeAxisControl->bSetPos(pruneControl.getRight() + kBorder, kBorder);

			pruneValue.addListener(this);
			timeAxisValue.addListener(this);

			addAndMakeVisible(*timeAxisControl);
			addAndMakeVisible(&pruneControl);
		}

		EWMAProfilerComponent::~EWMAProfilerComponent()
		{
			for (auto& lane : lanes)
				lane->setEnabled(false);
		}

		void EWMAProfilerComponent::setTimeAxisMode(TimeAxisModes mode)
		{
			timeAxisValue.setAsTEnum(mode);
		}

		void EWMAProfilerComponent::setParentPruningProportion(double value)
		{
			pruneValue.setTransformedValue(value);
		}
		
		void EWMAProfilerComponent::valueEntityChanged(ValueEntityListener* sender, ValueEntityBase* value)
		{
			repaint();
		}

		void EWMAProfilerComponent::timerCallback()
		{
			for (auto& lane : lanes)
				model.consume(*lane);

			repaint();
		}

		void EWMAProfilerComponent::paint(juce::Graphics& g)
		{
			const auto backgroundColour = GetColour(ColourEntry::Normal);
			const auto outlineColour = GetColour(ColourEntry::Separator);
			const auto foregroundColour = GetColour(ColourEntry::Auxillary);
			const auto textColour = GetColour(ColourEntry::ControlText);
			const auto hotColour = foregroundColour.interpolatedWith(GetColour(ColourEntry::Error), 0.25);

			g.fillAll(backgroundColour);

			auto localBounds = getLocalBounds()
				.withTrimmedTop(kControlPaneHeight + kBorder)
				.expanded(-kBorder, -kBorder);

			std::vector<EWMAModel::LaneData> laneDatas;
			laneDatas.reserve(lanes.size());

			for (auto& lane : lanes)
			{
				auto data = model.getLaneData(*lane);

				if (!data)
					continue;

				laneDatas.emplace_back(*data);
			}

			if (laneDatas.empty())
				return;

			// This isn't using the dynamic layout depth to avoid flickering allocation sizes.
			auto totalDepthsNeeded = std::accumulate(
				laneDatas.begin(),
				laneDatas.end(),
				0,
				[](int acc, const auto& l)
				{
					return acc + l.maxDepthSeen() + 5; // +1 for levels, +1 to ensure one slot inbetween all lanes, +3 for some bias
				}
			);

			auto maxBudget = std::max_element(laneDatas.begin(), laneDatas.end(), [](const auto& a, const auto& b) { return a.budget() < b.budget(); });
			auto maxDuration = std::max_element(laneDatas.begin(), laneDatas.end(), [](const auto& a, const auto& b) { return a.duration() < b.duration(); });

			auto scalingMode = timeAxisValue.getAsTEnum<TimeAxisModes>();

			auto currentBottom = localBounds.getY();

			for (const auto& data : laneDatas)
			{
				std::optional<Profiling::EWMAModel::Seconds> laneLength;

				switch (scalingMode)
				{
					//case IndependentBudget: // default
				case TimeAxisModes::IndependentDuration: laneLength = data.duration(); break;
				case TimeAxisModes::AlignedBudget: laneLength = maxBudget->budget(); break;
				case TimeAxisModes::AlignedDuration: laneLength = maxDuration->duration(); break;
				}

				auto proportionalSpaceNeeded = (data.maxDepthSeen() + 5.f) / totalDepthsNeeded;

				auto rect = localBounds
					.toFloat()
					.withHeight(proportionalSpaceNeeded * localBounds.getHeight())
					.withY(static_cast<float>(currentBottom));

				EWMALaneJuceRenderer renderer(
					data,
					rect.toNearestInt(),
					viewOffsets.toFloat(),
					static_cast<float>(pruneValue.getTransformedValue()),
					laneLength
				);

				renderer.paint(g, foregroundColour, outlineColour, std::nullopt, hotColour);

				currentBottom = rect.getBottom();
			}
		}

		// zooms view offsets
		void EWMAProfilerComponent::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
		{
			constexpr double increment = 1.2;
			auto position = event.position.getX() / (getWidth() - 1.0);

			double sign = wheel.isReversed ? 1 : -1;

			auto span = (viewOffsets.getY() - viewOffsets.getX());
			auto scale = std::pow(increment, sign * wheel.deltaY);
			auto delta = span - span * scale;

			viewOffsets.setX(viewOffsets.getX() + delta * position);
			viewOffsets.setY(viewOffsets.getY() - delta * (1 - position));

			// Increase time constant as the window gets smaller, but moderated.
			model.setTimeConstant(Profiling::EWMAModel::Seconds(std::sqrt(1 / span)));

			repaint();
		}

		// resets view offsets on left click, freezes on right click
		void EWMAProfilerComponent::mouseDoubleClick(const juce::MouseEvent& event)
		{
			if (event.mods.isLeftButtonDown())
			{
				viewOffsets = { 0, 1 };
			}
			else
			{
				// stop timer update
				if (isTimerRunning())
					stopTimer();
				else
					startTimerHz(kTimerFrequency);
			}

			repaint();
		}

		void EWMAProfilerComponent::mouseUp(const juce::MouseEvent& e)
		{
			if (e.mods.isLeftButtonDown())
				priorDragPosition.reset();
		}

		void EWMAProfilerComponent::mouseDown(const juce::MouseEvent& e)
		{
			if (e.mods.isLeftButtonDown())
				priorDragPosition = e.position.getX();
		}

		// translates view offsets
		void EWMAProfilerComponent::mouseDrag(const juce::MouseEvent& event)
		{
			if (!priorDragPosition)
				return;

			auto current = event.position.getX();

			auto delta = current - *priorDragPosition;
			auto fraction = -delta / (getWidth() - 1.0);
			auto span = viewOffsets.getY() - viewOffsets.getX();
			viewOffsets.addXY(fraction * span, fraction * span);

			priorDragPosition = current;

			repaint();
		}
	}
}

