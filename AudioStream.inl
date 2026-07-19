/*************************************************************************************

	cpl - cross-platform library - v. 0.1.0.

	Copyright (C) 2022 Janus Lynggaard Thorborg (www.jthorborg.com)

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

	file:AudioStream.inl

		Template definitions of AudioStream.h

*************************************************************************************/

#include <variant>

namespace cpl
{
	template<typename T, std::size_t PacketSize>
	inline AudioStream<T, PacketSize>::Output::~Output()
	{
		{
			// synchronize against any rogue buffer locks being held.
			std::lock_guard<std::mutex> lock(aBufferMutex);
		}

		ListenerContext ctx(*this);

		for (auto& listener : listeners)
		{
			listener->onStreamDied(ctx);
		}
	}

	template<typename T, std::size_t PacketSize>
	inline void AudioStream<T, PacketSize>::Output::beginFrameProcessing()
	{
		if (auto lane = profilerLane.get())
			profilerFrame.emplace(lane);

		overhead.start(); all.start();
		audioInput.resetOffsets();
		oldInfo = info;
	}

	template<typename T, std::size_t PacketSize>
	inline void AudioStream<T, PacketSize>::Output::handleFrame(AudioStream<T, PacketSize>::ProducerFrame&& frame)
	{
		if (const auto * audio = std::get_if<AudioPacket>(&frame))
		{
			CPL_PROFILE("AudioStream::Output::handleAudioPacket");

			audioInput.insertFrameIntoBuffer(*audio);
		}
		else
		{
			if (!audioInput.isEmpty())
			{
				// Potential discontinuity whilst we already batched up some samples.
				// We have to end and begin to produce a monotonic stream for any listeners.
				endFrameProcessing();
				beginFrameProcessing();
			}

#if defined(CPL_MAC) && MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_14
			if (auto arg = std::get_if<TransportData>(&frame))
			{
				playhead.transport = *arg;
			}
			else if (auto arg = std::get_if<ArrangementData>(&frame))
			{
				playhead.arrangement = *arg;
			}
			else if (auto arg = std::get_if<ChannelNameData>(&frame))
			{
				channelNames.resize(std::max(channelNames.size(), arg->channelIndex + 1));
				channelNames[arg->channelIndex] = std::move(arg->name);
			}
			else if (auto arg = std::get_if<ProducerInfo>(&frame))
			{
				static_cast<ProducerInfo&>(info) = *arg;
				producerInfoChange = true;
			}
			else
			{
				CPL_RUNTIME_ASSERTION(false && "unknown frame type");
			}
#else
			std::visit(
				[this](auto && arg)
				{
					CPL_PROFILE("AudioStream::Output::storeMetaPacket");

					using TArg = std::decay_t<decltype(arg)>;

					if constexpr (std::is_same_v<TArg, TransportData>)
					{
						playhead.transport = arg;
					}
					else if constexpr (std::is_same_v<TArg, ArrangementData>)
					{
						playhead.arrangement = arg;
					}
					else if constexpr (std::is_same_v<TArg, ChannelNameData>)
					{
						channelNames.resize(std::max(channelNames.size(), arg.channelIndex + 1));
						channelNames[arg.channelIndex] = std::move(arg.name);
					}
					else if constexpr (std::is_same_v<TArg, ProducerInfo>)
					{
						static_cast<ProducerInfo&>(info) = arg;
						producerInfoChange = true;
					}
					else
					{
						CPL_RUNTIME_ASSERTION(false && "unknown frame type");
					}
				},
				frame
			);
#endif
		}
	}

	template<typename T, std::size_t PacketSize>
	inline void AudioStream<T, PacketSize>::Output::endFrameProcessing()
	{
		CPL_PROFILE_BEGIN("AudioStream::Output::endFrameProcessing");

		auto channels = audioInput.buffer.size();

		bool signalChange = producerInfoChange;
		producerInfoChange = false;

		ListenerContext ctx(*this);

		if (!inputChanges)
		{
			signalChange = signalChange || (info.storeAudioHistory && audioHistoryBuffers.size() != channels);
		}
		else
		{
			CPL_PROFILE("::stream-properties-changed");

			std::lock_guard<std::mutex> lock(inputCommandMutex);

			if (consumerInfoChange)
			{
				signalChange = true;
				static_cast<ConsumerInfo&>(info) = inputInfo;
				consumerInfoChange = false;
			}

			signalChange = signalChange || (info.storeAudioHistory && audioHistoryBuffers.size() != channels);

			for (auto& listenerCommand : inputListeners)
			{
				if (listenerCommand.wasAdded)
				{
					auto& newListener = listeners.emplace_back(std::move(listenerCommand.listener));
					// otherwise, it will happen further down.
					if(!signalChange)
						newListener->onStreamPropertiesChanged(ctx, info);
				}
				else
				{
					if (auto it = std::find(listeners.begin(), listeners.end(), listenerCommand.listener); it != listeners.end())
						listeners.erase(it);
				}
			}

			inputListeners.clear();
			inputChanges = false;
			consumerInfoChange = false;
		}

		deferredAudioInput.resize(channels);

		{
			bool audioHistoryDifferent = channels != audioHistoryBuffers.size();

			if (channels)
			{
				// while this may be concurrently read from, noone can mutate them
				audioHistoryDifferent = audioHistoryDifferent || info.audioHistorySize != audioHistoryBuffers[0].getSize();
				audioHistoryDifferent = audioHistoryDifferent || info.audioHistoryCapacity != audioHistoryBuffers[0].getCapacity();
			}

			// resize audio history buffer here, so it takes effect,
			// before any async callers are notified.
			if (signalChange && info.storeAudioHistory && audioHistoryDifferent)
			{
				CPL_PROFILE("::ensure-audio-history-storage");

				ensureAudioHistoryStorage(
					channels,
					info.audioHistorySize,
					info.audioHistoryCapacity,
					std::lock_guard<std::mutex>(aBufferMutex)
				);
			}

			if (profilerFrame)
				profilerFrame->setWork(static_cast<float>(audioInput.containedSamples), static_cast<float>(info.sampleRate));

			overhead.pause();

			CPL_PROFILE_BEGIN("::on-listener-stream-audio");
			for (auto& listener : listeners)
			{
				if (signalChange)
					listener->onStreamPropertiesChanged(ctx, oldInfo);

				if (audioInput.containedSamples > 0)
				{
					listener->onStreamAudio
					(
						ctx,
						audioInput.pointer.data(),
						channels,
						audioInput.containedSamples
					);
				}
			}
			CPL_PROFILE_END;


			playhead.advance(audioInput.containedSamples);
			overhead.resume();
		}

		// Publish into circular buffer here.
		if (info.storeAudioHistory && info.audioHistorySize && channels)
		{
			CPL_PROFILE("::store-audio-history");

			std::unique_lock<std::mutex> bufferLock(aBufferMutex, std::try_to_lock);
			// decide whether to wait on the buffers
			if (!bufferLock.owns_lock() && info.blockOnHistoryBuffer)
				bufferLock.lock();

			if (bufferLock.owns_lock())
			{
				CPL_PROFILE("::merge-input-to-buffer");
				mergeInputToBuffer(bufferLock, &audioInput, playhead, info);
			}
			else
			{
				CPL_PROFILE("::defer-audio-history");

				// defer current samples to a later point in time.
				for (std::size_t i = 0; i < channels; ++i)
				{
					deferredAudioInput[i].insert
					(
						deferredAudioInput[i].end(),
						audioInput.buffer[i].begin(),
						audioInput.buffer[i].begin() + audioInput.containedSamples
					);
				}

				if (channels > 0)
					numDeferredAsyncSamples = deferredAudioInput[0].size();

				deferredCheckpointPlayhead = playhead;
				deferredCheckpointBufferInfo = info;
			}
		}
		CPL_PROFILE_END;
		profilerFrame.reset();

		// TODO: Remove all of this. post measurements.
		double timeFraction = (double)audioInput.containedSamples;
		if (std::isnormal(timeFraction))
		{
			timeFraction /= info.sampleRate;
			lpFilterTimeToMeasurement(consumerOverhead, overhead.clocksToCoreUsage(overhead.getTime()), timeFraction);
			lpFilterTimeToMeasurement(consumerUsage, all.clocksToCoreUsage(all.getTime()), timeFraction);
		}
	}

	template<typename T, std::size_t PacketSize>
	inline void AudioStream<T, PacketSize>::Output::mergeInputToBuffer(const std::unique_lock<std::mutex>& lock, ChannelMatrix* realInputs, const Playhead& playheadToAssign, const AudioStreamInfo& infoToAssign)
	{
		const auto channels = deferredAudioInput.size();

		CPL_RUNTIME_ASSERTION(channels == audioHistoryBuffers.size());
		CPL_RUNTIME_ASSERTION(lock.owns_lock());

		for (std::size_t i = 0; i < channels; ++i)
		{
			{
				auto&& w = audioHistoryBuffers[i].createWriter();

				// first, insert all the old stuff that happened while this buffer was blocked
				if (numDeferredAsyncSamples)
				{
#ifdef _DEBUG
					if (!realInputs)
					{
						// Just an interesting checkpoint.
						numDeferredAsyncSamples = numDeferredAsyncSamples;
					}
#endif
					w.copyIntoHead(deferredAudioInput[i].data(), deferredAudioInput[i].size());
				}

				// next, insert current samples (if any)
				if(realInputs)
					w.copyIntoHead(realInputs->buffer[i].data(), realInputs->containedSamples);
			}

			// clear up temporary deferred stuff
			deferredAudioInput[i].clear();
		}

		// bit redundant, but ensures it will be called.
		numDeferredAsyncSamples = 0;

		deferredCheckpointPlayhead = bufferPlayhead = playheadToAssign;
		deferredCheckpointBufferInfo = bufferInfo = infoToAssign;
	}

	template<typename T, std::size_t PacketSize>
	inline void AudioStream<T, PacketSize>::Output::ensureAudioHistoryStorage(std::size_t channels, std::size_t pSize, std::size_t pCapacity, const std::lock_guard<std::mutex>&)
	{
		if (audioHistoryBuffers.size() != channels)
		{
			audioHistoryBuffers.resize(channels);
			channelNames.resize(std::max(channelNames.size(), channels));
		}

		auto cursor = channels > 0 ? audioHistoryBuffers[0].getCursor() : 0;

		for (std::size_t i = 0; i < channels; ++i)
		{
			audioHistoryBuffers[i].setStorageRequirements(pSize, pCapacity, true, T());
			audioHistoryBuffers[i].setCursor(cursor);
		}
	}

	template<typename T, std::size_t PacketSize>
	inline AudioStream<T, PacketSize>::InputFrameBatch::InputFrameBatch(Input& input)
		: FrameBatch(*input.stream), debugScope(input.reentrancy)
	{
	}


	template<typename T, std::size_t PacketSize>
	inline void AudioStream<T, PacketSize>::Input::processIncomingRTAudio(const T* const* buffer, std::size_t numChannels, std::size_t numSamples, const AudioStream<T, PacketSize>::Playhead& ph)
	{
		if (internalInfo.isSuspended)
			return;

		CPL_PROFILE("AudioStream::Input::processIncomingAudio");

		CPL_RUNTIME_ASSERTION(numChannels == internalInfo.channels);

		InputFrameBatch batch(*this);

		cpl::CProcessorTimer overhead, all;
		overhead.start(); all.start();

		auto const timeFraction = double(numSamples) / internalInfo.sampleRate;

		auto oldPlayhead = playhead;
		playhead.copyVolatileData(ph);

		// remaining samples
		std::int64_t n = numSamples;

		ProducerFrame frame;

		bool anyNewProblemsPushingPlayHeads = false;
		bool discontinuity = framesWereDropped || problemsPushingPlayHead || haltedDueToNoListeners;

		if (discontinuity || playhead.transport != oldPlayhead.transport)
		{
			frame.template emplace<TransportData>(playhead.transport);
			if (!batch.submitFrame(std::move(frame)))
			{
				anyNewProblemsPushingPlayHeads = true;
			}
		}

		if (discontinuity || playhead.arrangement != oldPlayhead.arrangement)
		{
			frame.template emplace<ArrangementData>(playhead.arrangement);
			if (!batch.submitFrame(std::move(frame)))
			{
				anyNewProblemsPushingPlayHeads = true;
			}
		}

		playhead.advance(numSamples);

		problemsPushingPlayHead = anyNewProblemsPushingPlayHeads;

		bool didDropAnyFrames = false;

		// publish all data to audio consumer thread
		if (numChannels == 1)
		{
			constexpr std::int64_t singleChannelCapacity = static_cast<std::int64_t>(AudioPacket::getCapacityForChannels(1));

			while (n > 0)
			{
				CPL_PROFILE("::publish-mono-packet");

				auto const aSamples = std::min(singleChannelCapacity, n - std::max(std::int64_t(0), n - singleChannelCapacity));

				if (aSamples > 0)
				{
					auto& packet = frame.template emplace<AudioPacket>(AudioPacket::PackingType::AudioPacketSeparate, 1, static_cast<std::uint16_t>(aSamples));
					std::memcpy(packet.begin(), (buffer[0] + numSamples - n), static_cast<std::size_t>(aSamples * AudioPacket::element_size));

					if (!batch.submitFrame(std::move(frame)))
					{
						didDropAnyFrames = true;
						this->stream->droppedFrames.fetch_add(aSamples);
					}
				}

				n -= aSamples;
			}
		}
		else
		{
			const std::int64_t capacity = static_cast<std::int64_t>(AudioPacket::getCapacityForChannels(numChannels));

			while (n > 0)
			{
				CPL_PROFILE("::publish-multichannel-packet");

				auto const aSamples = std::min(capacity, n - std::max(std::int64_t(0), n - capacity));

				if (aSamples > 0)
				{
					auto& packet = frame.template emplace<AudioPacket>(AudioPacket::PackingType::AudioPacketSeparate, static_cast<std::uint8_t>(numChannels), static_cast<std::uint16_t>(aSamples * numChannels));

					auto const byteSize = static_cast<std::size_t>(aSamples * AudioPacket::element_size);

					for (std::size_t c = 0; c < numChannels; ++c)
					{
						std::memcpy(packet.begin() + aSamples * c, (buffer[c] + numSamples - n), byteSize);
					}

					if (!batch.submitFrame(std::move(frame)))
					{
						didDropAnyFrames = true;
						this->stream->droppedFrames.fetch_add(aSamples);
					}
				}

				n -= aSamples;
			}
		}

		if (didDropAnyFrames)
			framesWereDropped = true;

		// post new measures
		lpFilterTimeToMeasurement(this->stream->producerOverhead, overhead.clocksToCoreUsage(overhead.getTime()), timeFraction);
		lpFilterTimeToMeasurement(this->stream->producerUsage, all.clocksToCoreUsage(all.getTime()), timeFraction);
	}

	template<typename T, std::size_t PacketSize>
	inline void AudioStream<T, PacketSize>::asyncAudioSystem(std::shared_ptr<AudioStream<T, PacketSize>> stream, std::weak_ptr<Output> output)
	{
		_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);

		ProducerFrame recv;
		int pops(20);

		// when it returns false, its time to quit this thread.
		while (stream->audioFifo->popElementBlocking(recv))
		{
			FrameBatch batch(output.lock());

			// always resize queue before emptying
			if (pops++ > 10)
			{
				stream->audioFifo->grow(0, true, 0.3f, 2);
				pops = 0;
			}

			batch.submitFrame(std::move(recv));

			std::size_t numExtraEntries = 0;
			do
			{
				// each time we get into here, it's very likely there's a bunch of messages waiting.
				numExtraEntries = stream->audioFifo->enqueuededElements();

				for (size_t i = 0; i < numExtraEntries; i++)
				{
					if (!stream->audioFifo->popElementBlocking(recv))
						return;
					batch.submitFrame(std::move(recv));
				}
			} while (numExtraEntries != 0);
		}

		if (auto sh = output.lock())
		{
			// one final batch to trigger non-frame processing
			FrameBatch batch(std::move(sh));

		}
	}

	template<>
	struct AudioChannelIterator<2, true>
	{
		template<typename Functor, class StreamBufferAccess>
		static void run(StreamBufferAccess& access, const Functor& f, std::size_t offset = 0)
		{
			typedef typename StreamBufferAccess::InheritStreamType StreamType;
			typedef typename StreamType::BufferIterator BufferIterator;

			typename StreamType::AudioBufferView views[2] = {
				access.getView(offset + 0),
				access.getView(offset + 1)
			};

			for (std::size_t indice = 0, n = 0; indice < StreamType::bufferIndices; ++indice)
			{
				BufferIterator
					left = views[0].getItIndex(indice),
					right = views[1].getItIndex(indice);

				std::size_t range = views[0].getItRange(indice);

				while (range--)
				{
					f(n++, *left++, *right++);
				}
			}
		}

		template<typename Functor, class StreamBufferAccess>
		static void run(const StreamBufferAccess & access, const Functor & f, std::size_t offset = 0)
		{
			typedef typename StreamBufferAccess::InheritStreamType StreamType;
			typedef typename StreamType::BufferIterator BufferIterator;

			typename StreamType::AudioBufferView views[2] = {
				access.getView(offset + 0),
				access.getView(offset + 1)
			};

			for (std::size_t indice = 0, n = 0; indice < StreamType::bufferIndices; ++indice)
			{
				BufferIterator
					left = views[0].getItIndex(indice),
					right = views[1].getItIndex(indice);

				std::size_t range = views[0].getItRange(indice);

				while (range--)
				{
					f(n++, *left++, *right++);
				}
			}
		}
	};

	template<>
	struct AudioChannelIterator<1, true>
	{
		template<typename Functor, class StreamBufferAccess>
		static void run(const StreamBufferAccess& access, const Functor& f, std::size_t offset = 0)
		{
			typedef typename StreamBufferAccess::InheritStreamType StreamType;
			typedef typename StreamType::BufferIterator BufferIterator;

			typename StreamType::AudioBufferView view = access.getView(offset);

			for (std::size_t indice = 0, n = 0; indice < StreamType::bufferIndices; ++indice)
			{
				BufferIterator left = view.getItIndex(indice);

				std::size_t range = view.getItRange(indice);

				while (range--)
				{
					f(n++, *left++);
				}
			}
		}
	};
};
