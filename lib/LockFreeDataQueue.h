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

	file:LockFreeDataQueue.h

		A lock free data queue (SPSC), meaning it stores complete T's (not pointers).
		The queue has ownership of the elements, so it's good for objects that are complex
		to copy.

*************************************************************************************/

#ifndef _LOCKFREE_DATAQUEUE_H
#define _LOCKFREE_DATAQUEUE_H

#include "readerwriterqueue/readerwriterqueue.h"
#include <vector>
#include "../CMutex.h"
#include "../Utility.h"
#include <atomic>
#include "weak_atomic.h"

#if ATOMIC_LLONG_LOCK_FREE != 2
#pragma message cwarn("warning: Atomic integer operations are not lock-free for this platform!") 
#endif

#if ATOMIC_POINTER_LOCK_FREE != 2
#pragma message cwarn("warning: Atomic pointer operations are not lock-free for this platform!") 
#endif

namespace cpl
{
	template<typename T>
	class LockFreeDataQueue
	{
	public:
		struct ElementAccess;
		friend struct ElementAccess;

		LockFreeDataQueue(std::size_t initialSize)
			: queue(new moodycamel::ReaderWriterQueue<T *>(initialSize))
			, oldQueue(nullptr)
			, currentNumElements(initialSize)
			, freeElements(initialSize)
			, lastFailedElement(nullptr)
		{
			insertDataElements(initialSize);
		}

		/// <summary>
		/// The only access to the queue you should ever have. 
		/// </summary>
		struct ElementAccess : private cpl::Utility::CNoncopyable
		{
		public:
			ElementAccess() : data(nullptr), parent(nullptr), isPop(false) {}

			friend class LockFreeDataQueue;

			T * getData()
			{
				#ifdef _DEBUG
				if (!parent || !data)
					CPL_RUNTIME_EXCEPTION("Un-initialized element access to queue.");
				#endif
				return data;
			}

			~ElementAccess()
			{
				if (parent && data)
				{
					if (isPop)
					{
						parent->freeElements.enqueue(data);
					}
					else
					{
						if (allocate)
						{
							parent->queue.load()->enqueue(data);
						}
						else if (!parent->queue.load()->try_enqueue(data))
						{
							parent->enqueuedQueueAllocations = true;
							parent->lastFailedElement = data;
						}
					}
				}
			}
		private:

			void initialize(bool isPopped, T * dataToHold, LockFreeDataQueue & parentQueue, bool forceAllocation = false)
			{
				#if _DEBUG
				if (dataToHold && parent)
				{
					// You reached this point if you try to access more than one element of this queue using a single 
					// elementaccess. The system is stack-based, so it's destructor must run to ensure exception-safety.
					// You're probably scoping the element wrongly.
					CPL_RUNTIME_EXCEPTION("Double-initialized element access to queue.");
				}
				#endif
				isPop = isPopped;
				data = dataToHold;
				parent = &parentQueue;
				allocate = forceAllocation;
			}

			T * data;
			LockFreeDataQueue * parent;

			/// <summary>
			/// If this access to any element is an element whom is popped from the queue (ie. to be consumed),
			/// this member is set, causing the destructor to push it into the free elements queue. Otherwise, the 
			/// access is considered to be 'produced', thus it will be pushed to the producer queue.
			/// </summary>
			bool isPop;
			bool allocate;
		};

		/// <summary>
		/// PRODUCER ONLY.
		/// Tries to fill the elementaccess with a T that, on ElementAccess destruction, will enqueue the data to the producer thread.
		/// if allocOnFail is false (always is for now), it will never allocate memory and the complexity is deterministic (wait-free).
		/// If enqueueNewAllocations is set, the consumer thread might increase the size at another time, if this call fails.
		/// </summary>
		template<bool allocOnFail = true, bool enqueueNewAllocations = true>
		bool acquireFreeElement(ElementAccess & d)
		{
			T * data;

			if (lastFailedElement)
			{
				d.initialize(false, lastFailedElement, *this);
				lastFailedElement = nullptr;
				return true;
			}
			else if (freeElements.try_dequeue(data))
			{
				d.initialize(false, data, *this);
				return true;
			}
			else if(allocOnFail)
			{
				d.initialize(false, new T(), *this, true);
				return true;
			}
			else if (enqueueNewAllocations)
			{
				enqueuedDataAllocations = true;
			}
			return false;
		}

		/// <summary>
		/// CONSUMER ONLY.
		/// If true is returned, ElementAccess is filled with the next element enqueued from acquireFreeElement.
		/// When parameter ElementAccess is destroyed, it will automatically be pushed to the free elements queue of the parent. 
		/// </summary>
		/// <param name="d"></param>
		/// <returns></returns>
		bool popElement(ElementAccess & d)
		{
			T * data;
			if (oldQueue)
			{
				if (oldQueue->try_dequeue(data))
				{
					d.initialize(true, data, *this);
					return true;
				}

			}
			// note that if the old queue is empty, and the new has an enqueued element, we can conclusively
			// prove that it is safe to delete the old queue since it is (a) empty and (b) the thread state 
			// for the producer is updated such that it uses all the new entities and will never use the old again.
			// if we successfully dequeue an element, we can delete the old (if it exists).
			if (queue.load()->try_dequeue(data))
			{
				d.initialize(true, data, *this);
				if (oldQueue)
				{
					delete oldQueue;
					oldQueue = nullptr;
				}
				return true;
			}
			return false;
		}

		/// <summary>
		/// CONSUMER ONLY.
		/// If any operations has failed and signaled the need for more allocations, this might be done now.
		/// May allocate memory.
		/// 
		/// Will grow the queue to minimumSize at least. If growth is set, then:
		///		if space_used > total_space * growthRequirement then grow(max(minimumSize, size() * growthFactor)
		/// 
		/// Thus you can use this function to automatically grow the queue if it begins to fill up, thereby
		/// avoiding a full queue. This will NOT delete any enqueued elements, issue locks or in any way mess up 
		/// ordering of concurrently enqueued elements.
		/// 
		/// returns true if the queue was grown, otherwise false.
		/// 
		/// Note: You should always grow the queue preemptively, that is, before you dequeue it fully.
		/// </summary>
		/// <returns></returns>
		bool grow(std::size_t minimumSize = 0, bool growth = false, float growthRequirement = 1.0f, int growthFactor = 2)
		{
			// old resize not done yet!
			if (oldQueue)
				return false;

			auto currentSpaceFilled = queue.load()->size_approx();

			std::size_t newSize(currentNumElements);

			if ((growth && currentSpaceFilled > currentNumElements * growthRequirement) || enqueuedDataAllocations || enqueuedQueueAllocations)
			{
				newSize *= growthFactor;
			}

			newSize = std::max(newSize, minimumSize);

			if (newSize > currentNumElements)
			{
				oldQueue = queue.exchange(new moodycamel::ReaderWriterQueue<T *>(newSize));
				insertDataElements(newSize - currentNumElements);
				enqueuedDataAllocations = enqueuedQueueAllocations = false;
				currentNumElements = newSize;
				return true;
			}
			return false;
		}

		std::size_t size() const noexcept
		{
			return currentNumElements;
		}

		std::size_t enqueuededElements() const noexcept
		{
			return queue.load()->size_approx();
		}

		~LockFreeDataQueue()
		{

			T * element;
			auto * q = queue.load();
			while (freeElements.try_dequeue(element))
				delete element;

			while (q->try_dequeue(element))
				delete element;

			if (oldQueue)
			{
				while (oldQueue->try_dequeue(element))
					delete element;
				delete oldQueue;
			}

			delete q;

			if (lastFailedElement)
				delete lastFailedElement;
		}

	private:

		void insertDataElements(std::size_t elements)
		{
			for (std::size_t i = 0; i < elements; ++i)
			{
				freeElements.enqueue(new T());
			}
		}


		std::atomic<moodycamel::ReaderWriterQueue<T *> *> queue;
		moodycamel::ReaderWriterQueue<T *> * oldQueue;
		moodycamel::ReaderWriterQueue<T *> freeElements;
		// If we dequeue an element, and fail to push it, store it here in limbo until hopefully used
		// Note that since there can only be one ElementAccess (pushing), we only ever need one limbo slot
		T* lastFailedElement;
		std::size_t currentNumElements;

		/// <summary>
		/// If set, try to grow the freeElements queue
		/// </summary>
		weak_atomic<bool> enqueuedDataAllocations;
		/// <summary>
		/// If set, try to grow the queue.
		/// </summary>
		weak_atomic<bool> enqueuedQueueAllocations;
	};


	template<typename T>
	using CLockFreeDataQueue = LockFreeDataQueue<T>;
};
#endif
