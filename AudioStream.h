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

	file:AudioStream.h

		A bridge system allowing efficient audio data processing between a producer thread
		and consumers that either get a callback or exclusive access to a buffer.

		Note that the system is only lockfree for the producer when using the async option.

*************************************************************************************/

#ifndef CPL_AUDIOSTREAM_H
#define CPL_AUDIOSTREAM_H

#include "Utility.h"
#include <vector>
#include <mutex>
#include "AtomicCompability.h"
#include "ConcurrentServices.h"
#include "lib/BlockingLockFreeQueue.h"
#include "lib/CLIFOStream.h"
#include "profiling/Profiling.h"
#include "CProcessorTimer.h"
#include <deque>
#include <algorithm>
#include <numeric>
#include "Protected.h"
#include <variant>
#include <functional>
#include <optional>

namespace cpl
{
	template<std::size_t channels, bool biased>
	struct AudioChannelIterator;

	namespace detail
	{
		void launchThread(std::function<void()>&& function);
	}

	template<typename T, std::size_t PacketSize = 64>
	class AudioStream : private Utility::CNoncopyable
	{
	private:

		/// <summary>
		/// A simple blob of audio channel data transmitted,
		/// used for transmitting data from real time threads
		/// to worker threads.
		/// </summary>
		struct AudioPacket
		{
		public:

			enum class PackingType : std::uint8_t
			{
				None,
				/// <summary>
				/// For N channels, every N + K belongs to the Kth channel
				/// </summary>
				AudioPacketInterleaved,
				/// <summary>
				/// For N channels of M size, every N + K * M belongs to the Kth channel 
				/// </summary>
				AudioPacketSeparate
			};

			static constexpr std::size_t data_alignment = 8;
			static constexpr std::size_t element_size = sizeof(T);

			static_assert(PacketSize > data_alignment + sizeof(T), "Audio packet cannot hold a single element");

			AudioPacket(PackingType channelConfiguration, std::uint8_t numChannels, std::uint16_t elementsUsed)
				: size(elementsUsed), channels(numChannels), packing(channelConfiguration)
			{
				static_assert(sizeof(AudioPacket) == PacketSize, "Wrong packing");
			}

			AudioPacket(AudioPacket&& other)
				: size(other.size), channels(other.channels), packing(other.packing)
			{
				std::memcpy(buffer, other.buffer, getTotalSamples() * sizeof(T));
			}

			AudioPacket& operator =(AudioPacket&& other)
			{
				// TODO: copy less?
				std::memcpy(this, &other, sizeof(*this));
				return *this;
			} 

			static constexpr std::size_t getCapacityForChannels(std::size_t channels) noexcept { return capacity / channels; }
			constexpr std::size_t getChannelCount() const noexcept { return channels; }
			constexpr std::size_t getNumFrames() const noexcept { return size / channels; }
			constexpr std::size_t getTotalSamples() const noexcept { return size; }

			constexpr T* begin() noexcept { return buffer; }
			constexpr T* end() noexcept { return buffer + size; }
			constexpr const T* begin() const noexcept { return buffer; }
			constexpr const T* end() const noexcept { return buffer + size; }
			constexpr PackingType packingType() const noexcept { return packing; }

		private:

			static constexpr std::uint32_t capacity = (PacketSize - data_alignment) / element_size;

			/// <summary>
			/// The total number of samples.
			/// </summary>
			std::uint16_t size;
			std::uint8_t channels;
			PackingType packing;

			alignas(data_alignment) T buffer[capacity];
		};

		struct ArrangementData
		{
#ifdef CPL_JUCE
			ArrangementData(const juce::AudioPlayHead::CurrentPositionInfo& cpi)
				: beatsPerMinute(cpi.bpm)
				, signatureDenominator(cpi.timeSigDenominator)
				, signatureNumerator(cpi.timeSigNumerator)
			{}
#endif
			ArrangementData() : beatsPerMinute(), signatureDenominator(), signatureNumerator() {}

			inline friend bool operator == (const ArrangementData& left, const ArrangementData& right)
			{
				return left.beatsPerMinute == right.beatsPerMinute && left.signatureDenominator == right.signatureDenominator && left.signatureNumerator == right.signatureNumerator;
			}

			inline friend bool operator != (const ArrangementData& left, const ArrangementData& right)
			{
				return !(left == right);
			}

			double beatsPerMinute;
			std::uint16_t signatureDenominator;
			std::uint16_t signatureNumerator;
		};

		struct TransportData
		{
#ifdef CPL_JUCE
			TransportData(const juce::AudioPlayHead::CurrentPositionInfo& cpi)
				: samplePosition(cpi.timeInSamples)
				, isPlaying(cpi.isPlaying)
				, isLooping(cpi.isLooping)
				, isRecording(cpi.isRecording)
			{
			}
#endif
			inline friend bool operator == (const TransportData& left, const TransportData& right)
			{
				return left.samplePosition == right.samplePosition && left.isPlaying == right.isPlaying && left.isLooping == right.isLooping && left.isRecording == right.isRecording;
			}

			inline friend bool operator != (const TransportData& left, const TransportData& right)
			{
				return !(left == right);
			}

			TransportData() : samplePosition(), isPlaying(), isLooping(), isRecording() {}
			std::int64_t samplePosition;
			std::uint16_t isPlaying : 1, isLooping : 1, isRecording : 1;
		};

		struct ChannelNameData
		{
			ChannelNameData(std::size_t index, std::string&& contents)
				: channelIndex(index), name(std::move(contents))
			{

			}

			std::size_t channelIndex;
			std::string name;
		};

	public:

		struct ProducerInfo
		{
			double sampleRate {};

			std::uint32_t anticipatedSize {};
			std::uint8_t channels {};
			bool isSuspended {};
		};

		struct ConsumerInfo
		{
			ConsumerInfo() = default;

			std::uint64_t
				audioHistorySize {},
				audioHistoryCapacity {};

			bool
				/// <summary>
				/// If true, stores the last audioHistorySize samples in a circular buffer.
				/// </summary>
				storeAudioHistory {},
				/// <summary>
				/// If set, the async subsystem will block on the audio history buffers until
				/// they are released back into the stream - this blocks async audio updates,
				/// listener updates etc. as well.
				/// If not, samples will instead get queued up for insertion into the history
				/// buffers
				/// </summary>
				blockOnHistoryBuffer {};
		};

		/// <summary>
		/// Holds info about the audio stream.
		/// </summary>
		struct AudioStreamInfo : public ProducerInfo, public ConsumerInfo
		{
			AudioStreamInfo() = default;
		};

		using Info = AudioStreamInfo;

	private:

		typedef std::variant<ProducerInfo, AudioPacket, ArrangementData, TransportData, ChannelNameData> ProducerFrame;
		static_assert(std::is_move_constructible_v<ProducerFrame> && std::is_move_assignable_v<ProducerFrame>);

		struct ChannelMatrix
		{
			void ensureSize(std::size_t channels, std::size_t samples)
			{
				buffer.resize(channels);
				pointer.resize(channels);

				for (std::size_t c = 0; c < channels; ++c)
				{
					buffer[c].resize(samples);
					pointer[c] = buffer[c].data();
				}
			}

			bool isEmpty() const noexcept { return containedSamples == 0; }

			void ensureChannels(std::size_t channels)
			{
				ensureSize(channels, buffer.size() ? buffer[0].size() : 0);
			}

			void resetOffsets()
			{
				containedSamples = 0;
			}

			void insertFrameIntoBuffer(const AudioPacket & frame)
			{
				const auto numSamples = frame.getNumFrames();
				const auto numChannels = frame.getChannelCount();

				ensureSize(numChannels, numSamples + containedSamples);

				switch (frame.packingType())
				{
				case AudioPacket::PackingType::AudioPacketSeparate:
				{
					for (std::size_t c = 0; c < numChannels; ++c)
					{
						std::memcpy(
							buffer[c].data() + containedSamples,
							frame.begin() + c * numSamples,
							numSamples * sizeof(T)
						);
					}

					break;
				}

				case AudioPacket::PackingType::AudioPacketInterleaved:
				{
					for (std::size_t c = 0; c < numChannels; ++c)
					{
						for (std::size_t n = 0; n < numSamples; ++n)
						{
							buffer[c][n + containedSamples] = *(frame.begin() + n * numChannels + c);
						}
					}

					break;
				}
				}

				containedSamples += numSamples;
			}

			std::vector<T*> pointer;
			std::size_t containedSamples = 0;
			// TODO: Linearize
			std::vector<std::vector<T>> buffer;
		};

	public:

		struct PerformanceMeasurements
		{
			double consumerOverhead;
			double producerOverhead;
			double consumerUsage;
			double producerUsage;

			/// <summary>
			/// Returns the number of dropped frames from the audio thread,
			/// due to the FIFO being filled up as the async thread hasn't
			/// catched up (due to being blocked or simply have too much work).
			/// </summary>
			std::uint64_t droppedFrames;
		};

		static const std::size_t packetSize = PacketSize;
		static const std::size_t storageAlignment = 32;

		typedef typename cpl::CLIFOStream<T, storageAlignment> AudioBuffer;
		typedef typename cpl::CLIFOStream<T, storageAlignment>::ProxyView AudioBufferView;
		typedef T DataType;
		typedef typename AudioBuffer::IteratorBase::const_iterator BufferIterator;
		typedef typename AudioBuffer::IteratorBase::const_iterator CBufferIterator;
		typedef AudioStream<T, PacketSize> StreamType;
		typedef CBlockingLockFreeQueue<ProducerFrame> FrameQueue;

		struct Playhead
		{
			#ifdef CPL_JUCE
			Playhead(const juce::AudioPlayHead::CurrentPositionInfo & info, double sampleRate) : sampleRate(sampleRate), arrangement(info), transport(info) {}
			#endif
			friend class AudioStream<T, PacketSize>;

			void advance(cpl::ssize_t samples)
			{
				steadyClock += samples;

				if (transport.isPlaying)
					transport.samplePosition += samples;
			}

			std::uint64_t getSteadyClock() const noexcept { return steadyClock; }
			bool isPlaying() const noexcept { return transport.isPlaying; }
			bool isLooping() const noexcept { return transport.isLooping; }
			bool isRecording() const noexcept { return transport.isRecording; }

			double getBPM() const noexcept { return arrangement.beatsPerMinute; }
			std::pair<int, int> getSignature() const noexcept { return std::make_pair(arrangement.signatureNumerator, arrangement.signatureDenominator); }
			std::int64_t getPositionInSamples() const noexcept { return transport.samplePosition; }
			double getPositionInSeconds() const noexcept { return getPositionInSamples() / sampleRate; }

			static Playhead empty() { return{}; }

			void copyVolatileData(const Playhead& other)
			{
				sampleRate = other.sampleRate;
				arrangement = other.arrangement;
				transport = other.transport;
			}

		private:

			Playhead() {}
			double sampleRate = 0;
			ArrangementData arrangement;
			TransportData transport;
			std::uint64_t steadyClock = 0;
		};

		/// <summary>
		/// When you iterate over a audio buffer using ended iterators,
		/// there will be this number of iterator iterations.
		/// </summary>
		static const std::size_t bufferIndices = AudioBuffer::IteratorBase::iterator_indices;
		/// <summary>
		/// Provides a constant view of the internal audio buffers, synchronized.
		/// The interface is built on RAII, the data is valid as long as this struct is in scope.
		/// Same principle for AudioBufferViews.
		/// </summary>
		struct AudioBufferAccess
		{
		public:
			typedef StreamType InheritStreamType;
			typedef typename AudioBuffer::IteratorBase::iterator iterator;
			typedef typename AudioBuffer::IteratorBase::const_iterator const_iterator;

			AudioBufferAccess(std::unique_lock<std::mutex>&& l, const std::vector<AudioBuffer>& audioChannels, const Playhead& ph, const AudioStreamInfo& info)
				: lock(std::move(l)), audioChannels(audioChannels), playhead(ph), info(info)
			{

			}

			AudioBufferAccess(AudioBufferAccess&& other) = default;
			AudioBufferAccess& operator =(AudioBufferAccess&& other) = default;
			AudioBufferAccess(const AudioBufferAccess &) = delete;
			AudioBufferAccess& operator = (const AudioBufferAccess &) = delete;

			const AudioStreamInfo& getInfo() const noexcept
			{
				return info;
			}

			AudioBufferView getView(std::size_t channel) const
			{
				return audioChannels.at(channel).createProxyView();
			}

			std::size_t getNumChannels() const noexcept
			{
				return audioChannels.size();
			}

			std::size_t getNumSamples() const noexcept
			{
				return getNumChannels() ? audioChannels[0].getSize() : 0;
			}

			const Playhead& getPlayhead() const noexcept
			{
				return playhead;
			}

			/// <summary>
			/// Iterate over the samples in the buffers. If biased is set, the samples arrive in chronological order.
			/// The functor shall accept the following parameters:
			///		(std::size_t sampleFrame, AudioStream::Data & channel0, AudioStream::Data & channelN ...)
			/// </summary>
			template<std::size_t Channels, bool biased, typename Functor>
			inline void iterate(const Functor & f, std::size_t offset = 0)
			{
				AudioChannelIterator<Channels, biased>::run(*this, f, offset);
			}

			/// <summary>
			/// Iterate over the samples in the buffers. If biased is set, the samples arrive in chronological order.
			/// The functor shall accept the following parameters:
			///		(std::size_t sampleFrame, AudioStream::Data channel0, AudioStream::Data channelN ...)
			/// </summary>
			template<std::size_t Channels, bool biased, typename Functor>
			inline void iterate(const Functor & f, std::size_t offset = 0) const
			{
				AudioChannelIterator<Channels, biased>::run(*this, f, offset);
			}

		private:
			const Playhead& playhead;
			const AudioStreamInfo& info;
			const std::vector<AudioBuffer>& audioChannels;
			std::unique_lock<std::mutex> lock;
		};

		using BufferAccess = AudioBufferAccess;

		struct ListenerContext;
		class Reference;

		struct Handle
		{
			friend class Reference;

			inline friend bool operator == (const Handle& left, const Handle& right)
			{
				return left.handle == right.handle;
			}

			inline friend bool operator != (const Handle& left, const Handle& right)
			{
				return !(left == right);

			}

			inline friend bool operator < (const Handle& left, const Handle& right)
			{
				return left.handle < right.handle;
			}

			Handle(const Handle&) = default;
			Handle& operator=(const Handle&) = default;

		private:
			Handle(AudioStream* stream) : handle(stream) {}
			AudioStream* handle;
		};

		/// <summary>
		/// A class that enables listening callbacks on both real-time and async audio
		/// channels from a <see cref="AudioStream"/>
		/// </summary>
		class Listener : Utility::CNoncopyable
		{
		public:
			typedef AudioStream<T, PacketSize> Stream;

			/// <summary>
			/// Called when certain properties are changed in the stream.
			/// </summary>
			virtual void onStreamPropertiesChanged(ListenerContext& changedSource, const typename Stream::AudioStreamInfo & before) {}
			/// <summary>
			/// </summary>
			virtual void onStreamAudio(ListenerContext& source, T ** buffer, std::size_t numChannels, std::size_t numSamples) { }
			/// <summary>
			/// Called when the current source being listened to died.
			/// You're not required to remove yourself as a listener.
			/// While you can obtain buffer views here, it's undefined behaviour to let them escape this callback.
			/// </summary>
			virtual void onStreamDied(ListenerContext& dyingSource) { (void)dyingSource; };

			virtual ~Listener() {};
		};

		class Reference : cpl::Utility::CNoncopyable
		{
			friend class AudioStream<T, PacketSize>;
		protected:
			std::shared_ptr<StreamType> stream;
			Reference() = default;
			Reference(Reference&& ref) noexcept
				: stream(std::move(ref.stream))
			{
			}
		public:
			Handle getHandle() const noexcept { return { stream.get() }; }
		};

		class Output final : public Reference
		{
		public:

			friend struct ListenerContext;
			friend class AudioStream<T, PacketSize>;

			/// <summary>
			/// Adds a listener that to receive callbacks going forward.
			/// It isn't guaranteed to happen instantly.
			/// The stream will acquire ownership until the listener is removed.
			/// </summary>
			void addListener(std::shared_ptr<Listener> listener)
			{
				std::lock_guard<std::mutex> lock(inputCommandMutex);
				this->stream->outputListenerCount.fetch_add(1);
				inputListeners.emplace_back(ListenerCommand{ std::move(listener), true });
				inputChanges = true;
			}

			/// <summary>
			/// Takes note to remove a particular listener when possible if it was previously
			/// added, it might not happen instantly.
			/// </summary>
			void removeListener(std::shared_ptr<Listener> listener)
			{
				std::lock_guard<std::mutex> lock(inputCommandMutex);
				this->stream->outputListenerCount.fetch_add(-1);

				inputListeners.emplace_back(ListenerCommand{ std::move(listener), false });
				inputChanges = true;
			}

			/// <summary>
			/// Returns a view of the audio history for all channels the last N samples (see setAudioHistorySize)
			/// References are only guaranteed to be valid while AudioBufferAccess is in scope.
			/// May acquire a lock in the returned class, so don't call it from real-time threads.
			///
			/// Ensures exclusive access while it is hold.
			/// </summary>
			AudioBufferAccess getAudioBufferViews()
			{
				return { std::unique_lock<std::mutex>(aBufferMutex), audioHistoryBuffers, bufferPlayhead, bufferInfo };
			}

			/// <summary>
			/// Safe to call from any thread. Changes will take effect when the next set of audio is processed
			/// </summary>
			template<class ModifierFunc>
			void modifyConsumerInfo(ModifierFunc&& func)
			{
				std::lock_guard<std::mutex> lock(inputCommandMutex);
				func(inputInfo);
				inputInfo.audioHistorySize = std::min(inputInfo.audioHistorySize, inputInfo.audioHistoryCapacity);
				inputChanges = true;
				consumerInfoChange = true;
			}

			PerformanceMeasurements getPerfMeasures() const noexcept
			{
				PerformanceMeasurements measures;

				measures.consumerOverhead = consumerOverhead;
				measures.producerOverhead = this->stream->producerOverhead;
				measures.consumerUsage = consumerUsage;
				measures.producerUsage = this->stream->producerUsage;
				measures.droppedFrames = this->stream->droppedFrames;

				return measures;
			}

			/// <summary>
			/// Report how many samples are postponed for delivery in case the async system had to compete with locks on
			/// <see cref="getAudioBufferViews"/> due to <see cref="ConsumerInfo::blockOnHistoryBuffer"/> being false.
			/// </summary>
			std::size_t getNumDeferredSamples() const noexcept
			{
				return numDeferredAsyncSamples;
			}

			std::size_t getApproximateInFlightPackets() const noexcept
			{
				return this->stream->audioFifo.get() ? this->stream->audioFifo->enqueuededElements() : 0;
			}

			Output(Output&& other) = default;
			Output& operator =(Output&& other) = default;
			~Output();

		private:

			Output(std::shared_ptr<Profiling::Lane>&& lane)
				: profilerLane(std::move(lane))
			{

			}

			static std::shared_ptr<Output> makeOutput(std::shared_ptr<Profiling::Lane> lane)
			{
				return std::shared_ptr<Output>(new Output(std::move(lane)));
			}

			void beginFrameProcessing();
			void handleFrame(ProducerFrame&& frame);
			void endFrameProcessing();
			void ensureAudioHistoryStorage(std::size_t channels, std::size_t pSize, std::size_t pCapacity, const std::lock_guard<std::mutex>&);
			void mergeInputToBuffer(const std::unique_lock<std::mutex>&, ChannelMatrix* realInputs, const Playhead& playheadToAssign, const AudioStreamInfo& infoToAssign);

			struct ListenerCommand
			{
				std::shared_ptr<Listener> listener;
				bool wasAdded;
			};

			Playhead playhead, bufferPlayhead, deferredCheckpointPlayhead;
			AudioStreamInfo info, oldInfo, bufferInfo, deferredCheckpointBufferInfo;
			ChannelMatrix audioInput;
			std::vector<std::vector<T>> deferredAudioInput;
			std::vector<AudioBuffer> audioHistoryBuffers;
			relaxed_atomic<double> consumerOverhead, consumerUsage;
			CProcessorTimer overhead, all;

			std::vector<ListenerCommand> inputListeners;
			std::vector<std::shared_ptr<Listener>> listeners;
			std::mutex inputCommandMutex;
			ConsumerInfo inputInfo;
			weak_atomic<bool> inputChanges;

			bool producerInfoChange, consumerInfoChange;

			weak_atomic<std::size_t> numDeferredAsyncSamples;
			std::vector<std::string> channelNames;
			std::mutex aBufferMutex;
			std::shared_ptr<Profiling::Lane> profilerLane;
			std::optional<Profiling::ProfilerFrame> profilerFrame;
		};

		struct FrameBatch
		{
			FrameBatch(std::shared_ptr<Output>&& out)
				: output(std::move(out)), stream(nullptr)
			{
				if (output)
					output->beginFrameProcessing();
			}

			bool submitFrame(ProducerFrame&& frame)
			{
				if (output)
					output->handleFrame(std::move(frame));
				else if(stream)
					return stream->publishFrame(std::move(frame));

				return true;
			}

			~FrameBatch()
			{
				if (output)
					output->endFrameProcessing();
			}

		protected:

			template <typename Other>
			static bool hasContents(const std::weak_ptr<Other>& w)
			{
				return w.owner_before(std::weak_ptr<Other>{}) || std::weak_ptr<Other>{}.owner_before(w);
			}

			FrameBatch(AudioStream& audioStream)
				: stream(&audioStream)
			{
				if (hasContents(audioStream.output))
				{
					output = audioStream.output.lock();
					// if the output died concurrently between the test and the lock.
					if (output)
						output->beginFrameProcessing();
					else
						stream = nullptr;
				}
			}

		private:

			AudioStream* stream;
			std::shared_ptr<Output> output;
		};

		struct ExclusiveDebugScope
		{
			ExclusiveDebugScope(std::atomic_flag& flag)
				: flag(flag)
			{
				if (flag.test_and_set(std::memory_order_release))
				{
					flag.clear();
					CPL_RUNTIME_EXCEPTION("Re-entrancy / concurrency detected in audio stream producer");
				}
			}

			~ExclusiveDebugScope() noexcept(false)
			{
				// TODO: use test() in C++20
				if (!flag.test_and_set(std::memory_order_release))
				{
					CPL_RUNTIME_EXCEPTION("Re-entrancy / concurrency detected in audio stream producer");
				}

				flag.clear();
			}

			std::atomic_flag& flag;
		};

		class Input;

		struct InputFrameBatch : public FrameBatch
		{
			InputFrameBatch(Input& input);

		private:

			ExclusiveDebugScope debugScope;
		};

		class Input final : public Reference
		{
			friend class AudioStream<T, PacketSize>;
			friend struct InputFrameBatch;

		public:
#ifdef CPL_JUCE
			void processIncomingRTAudio(const T* const * buffer, std::size_t numChannels, std::size_t numSamples, juce::AudioPlayHead& ph)
			{
				juce::AudioPlayHead::CurrentPositionInfo cpi;
				ph.getCurrentPosition(cpi);
				processIncomingRTAudio(buffer, numChannels, numSamples, { cpi, internalInfo.sampleRate });
			}
#endif
			void processIncomingRTAudio(const T* const * buffer, std::size_t numChannels, std::size_t numSamples, const Playhead& ph);

			/// <summary>
			/// This must be called at least once, before streaming starts.
			/// It is not safe to call this function concurrently
			/// - decide on one thread, controlling it.
			/// </summary>
			template<typename ModifierFunc>
			void initializeInfo(ModifierFunc&& func, InputFrameBatch& batch)
			{
				func(internalInfo);
				batch.submitFrame(ProducerFrame(internalInfo));			
			}

			/// <summary>
			/// This must be called at least once, before streaming starts.
			/// It is not safe to call this function concurrently
			/// - decide on one thread, controlling it.
			/// </summary>
			template<typename ModifierFunc>
			void initializeInfo(ModifierFunc&& func)
			{
				InputFrameBatch batch(*this);
				initializeInfo(std::move(func), batch);
			}

			void enqueueChannelName(std::size_t index, std::string&& name, InputFrameBatch& batch)
			{
				ProducerFrame frame;
				frame.template emplace<ChannelNameData>(ChannelNameData{ index, std::move(name) });

				batch.submitFrame(std::move(frame));
			}

			void enqueueChannelName(std::size_t index, std::string&& name)
			{
				InputFrameBatch batch(*this);
				enqueueChannelName(index, std::move(name), batch);
			}

			/// <summary>
			/// Checks to see if there currently is anyone listening to the output.
			/// If not, you're free to skip calling <see cref="processIncomingRTAudio"/>
			/// until the next time this returns true, in which case the input will remember
			/// to repush playheads.
			/// 
			/// This can save on overhead.
			/// </summary>
			bool isAnyoneListening()
			{
				int listenerCount = this->stream->outputListenerCount;
				CPL_RUNTIME_ASSERTION(listenerCount >= 0);

				haltedDueToNoListeners = listenerCount == 0;

				return !haltedDueToNoListeners;
			}

			~Input()
			{
				if (this->stream)
					this->stream->inputDestroyed();
			}

			Input(Input&&) = default;
			Input& operator=(Input&&) = default;

		private:

			struct moveable_flag : public std::atomic_flag
			{
				moveable_flag(moveable_flag&& other) noexcept
				{
					// TODO: Fix in C++20 when we have test()
					std::memcpy(this, &other, sizeof(*this));
				}

				moveable_flag& operator == (moveable_flag&& other)
				{
					// TODO: Fix in C++20 when we have test()
					std::memcpy(this, &other, sizeof(*this));
				}

				moveable_flag()
				{
					clear();
				}
			};

			Input() = default;

			Playhead playhead;
			ProducerInfo internalInfo;
			bool framesWereDropped{}, problemsPushingPlayHead{}, haltedDueToNoListeners{};
			mutable moveable_flag reentrancy;
		};

		struct ListenerContext
		{
			friend class Output;
			/// <summary>
			/// Returns the current number of async samples that 'has' happened asynchronously, but still haven't been posted
			/// into the audio buffers, since the current AUdioStreamInfo::blockOnHistoryBuffers is set to false and something has blocked the audio buffers meanwhile.
			/// Safe and wait-free to call from any thread, guaranteed not to change in asynchronous callbacks.
			/// </summary>
			/// <returns></returns>
			std::size_t getNumDeferredSamples() const noexcept
			{
				return parent.numDeferredAsyncSamples;
			}

			const std::vector<std::string>& getChannelNames() const noexcept
			{
				return parent.channelNames;
			}

			const Playhead& getPlayhead() const noexcept
			{
				return parent.playhead;
			}

			/// <summary>
			/// Returns a view of the audio history for all channels the last N samples (see setAudioHistorySize)
			/// References are only guaranteed to be valid while AudioBufferAccess is in scope.
			/// May acquire a lock in the returned class, so don't call it from real-time threads.
			///
			/// Ensures exclusive access while it is hold.
			/// </summary>
			/// <param name="consumeDeferredData">
			/// If set, the playhead and circular audio data of the returned audio buffer is spooled to include
			/// any input that is been deferred. This has no effect if <see cref="ConsumerInfo::blockOnHistoryBuffer"/> is set.
			/// </param>
			AudioBufferAccess getAudioBufferViews(bool consumeDeferredData)
			{
				std::unique_lock<std::mutex> lock(parent.aBufferMutex);

				if (consumeDeferredData)
				{
					parent.mergeInputToBuffer(lock, nullptr, parent.deferredCheckpointPlayhead, parent.deferredCheckpointBufferInfo);
					return { std::move(lock), parent.audioHistoryBuffers, parent.bufferPlayhead, parent.bufferInfo};
				}
				else
				{
					return { std::move(lock), parent.audioHistoryBuffers, parent.bufferPlayhead, parent.bufferInfo };
				}
			}

			const AudioStreamInfo& getInfo() const noexcept
			{
				return parent.info;
			}

			Handle getHandle() const noexcept { return parent.getHandle(); }

		private:
			ListenerContext(Output& output) : parent(output) { }
			Output& parent;
		};

		typedef std::tuple<Input, std::shared_ptr<Output>> IO;

		/// <summary>
		/// The async subsystem enables access to a callback on a background thread, similar
		/// to audio callbacks, however, it is not real-time and can be blocked.
		/// Integrity of audio stream is not guaranteed, especially if you block it for longer times,
		/// however it should run almost as fast and synced as the audio thread, with a minimal overhead.
		/// The subsystem also continuously updates a circular buffer which you can lock.
		///
		/// Fifo sizes refer to the buffer size of the lock free fifo. The fifo stores <see cref="AudioFrames"/>.
		/// </summary>
		/// <param name="async">
		/// Defaults to false. Prefer true if you have any sort of blocking operations as a listener
		/// on the output, if you want to lock the fifo buffer or if you want to offload the audio processing to a background thread.
		/// </param>
		/// <param name="initialFifoSize">
		/// How many frames fit in the fifo. The size of the frame is large given by the template PacketSize.
		/// </param>
		/// <param name="maxFifoSize">
		/// How large the queue can grow before it should just overflow. Units are the same as <paramref name="initialFifoSize"/>.
		/// </param>
		/// <param name="profilerLanes">
		/// Optional <see cref="Profiling::Lane"/>s to be used to make profiler frames on the output.
		/// Note that frames are only constructed if the lane exists, so profiled work on non-existing lanes combined with <paramref name="async"/> = false
		/// will attribute towards any existing <see cref="Profiling::ProfilerFrame"/> further up the stack.
		/// </param>
		static IO create(
			std::optional<bool> async = std::nullopt,
			std::optional<size_t> initialFifoSize = std::nullopt,
			std::optional<std::size_t> maxFifoSize = std::nullopt,
			std::shared_ptr<Profiling::Lane> outputProfilerLane = nullptr
		)
		{
			async = async.value_or(false);
			initialFifoSize = initialFifoSize.value_or(20);
			maxFifoSize = maxFifoSize.value_or(1000);

			auto output = Output::makeOutput(outputProfilerLane);
			std::weak_ptr<Output> weakOutput = output;
			
			std::shared_ptr<AudioStream> stream;

			if (*async)
			{
				stream = std::shared_ptr<AudioStream>(new AudioStream(initialFifoSize.value(), maxFifoSize.value()));
				detail::launchThread([stream, weakOutput]() { asyncAudioSystem(stream, weakOutput); });
			}
			else
			{
				stream = std::shared_ptr<AudioStream>(new AudioStream(weakOutput));
			}

			Input input;
			input.stream = stream;
			output->stream = std::move(stream);

			return std::make_tuple(std::move(input), std::move(output));
		}
			   		
	private:

		AudioStream(size_t initialFifoSize, std::size_t maxFifoSize)
			: audioFifo(std::make_unique<FrameQueue>(initialFifoSize, maxFifoSize))
		{

		}

		AudioStream(std::weak_ptr<Output> output)
			: output(output)
		{

		}

		static inline void lpFilterTimeToMeasurement(relaxed_atomic<double>& old, double newTime, double timeFraction)
		{
			const double coeff = std::pow(0.3, timeFraction);
			newTime /= timeFraction;
			old = newTime + coeff * (old - newTime);
		}
		
		// use FrameBatch unless internally calling.
		bool publishFrame(ProducerFrame&& frame)
		{
			if (!audioFifo->pushElement(std::move(frame)))
			{
				droppedFrames.fetch_add(1);
				return false;
			}

			return true;
		}

		void inputDestroyed()
		{
			if (audioFifo)
			{
				audioFifo->releaseConsumer();
			}
		}

		static void asyncAudioSystem(std::shared_ptr<AudioStream> stream, std::weak_ptr<Output> output);

		relaxed_atomic<double> producerOverhead{}, producerUsage{};
		relaxed_atomic<std::size_t> droppedFrames{};
		relaxed_atomic<int> outputListenerCount;

		std::weak_ptr<Output> output;
		std::unique_ptr<FrameQueue> audioFifo;
	};

};

#include "AudioStream.inl"

#endif
