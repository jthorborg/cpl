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

	file:ConcurrentServices.h

		A set of multithreaded services enabling otherwise complex operations, like
		atomic data swaps of larger structures in lock-free fashion,
		making the services suitable for real-time multithreaded programming.

*************************************************************************************/

#ifndef CPL_CONCURRENTSERVICES_H
#define CPL_CONCURRENTSERVICES_H

#include <thread>
#include <vector>
#include <atomic>
#include <memory>
#include "Exceptions.h"

namespace cpl
{
	/// <summary>
	/// atomic_flag with reversed conditions.
	/// Usage:
	///		thread 1:
	///			abf = true;
	///		thread 2:
	///			if(abf.cas())
	///			{
	///				// ...
	///			}
	/// </summary>
	class ABoolFlag
	{
	public:
		ABoolFlag() : flag(false) {}
		ABoolFlag(ABoolFlag&& other) noexcept : flag(other.flag.load(std::memory_order_acquire)) {}
		ABoolFlag& operator =(ABoolFlag&& other) noexcept
		{
			flag.store(other.flag.load(std::memory_order_acquire));
			return *this;
		}

		ABoolFlag & operator = (bool val)
		{
			CPL_RUNTIME_ASSERTION(val && "Atomic bool flag reset through operator = .");
			flag.store(!!val, std::memory_order_release);
			return *this;
		}

		operator bool() const noexcept
		{
			return flag.load(std::memory_order_acquire);
		}

		/// <summary>
		/// Resets the flag to the input value, if it were set.
		/// Returns true if flag was set and reset.
		/// </summary>
		/// <param name="newVal"></param>
		/// <returns></returns>
		bool cas(bool newVal = false)
		{
			bool expected = !newVal;
			// TODO: figure out less strict ordering possible.
			return flag.compare_exchange_strong(expected, newVal, std::memory_order_acquire);
		}

	private:
		std::atomic<bool> flag;
	};

	/// <summary>
	/// A helper class that permits swapping in objects in a producer-consumer situation,
	/// where the 'consumer' is considered the thread, that actively uses the object, while
	/// the producer thread allows inserting new objects the consumer will use, obliviously.
	/// Example usage: Real-time thread has a list of listeners it calls each loop, however
	/// it cannot resize or alter these (cause of memory allocations).
	/// The producer, or non-real time thread swaps in a larger copy
	/// of the listeners any time it wants.
	/// All consumer operations are guaranteed real-time suitable (lock-free)
	/// </summary>
	template<class Object, class Delete = typename std::default_delete<Object>>
	class ConcurrentObjectSwapper : Utility::CNoncopyable
	{

	public:

		template<class Obj>
		struct ConcurrentEntry
		{
			ConcurrentEntry()
				: obj(), flag()
			{

			}

			std::atomic<Obj *> obj;
			std::atomic_bool flag;

			static_assert(ATOMIC_BOOL_LOCK_FREE != 0, "Atomic bools need to be lock free.");
			static_assert(ATOMIC_POINTER_LOCK_FREE != 0, "Atomic pointers need to be lock free.");

			void signalInUse() { flag.store(true); }
			bool isSignaled() const { return flag.load(); }
			void reset(Object * newObj) noexcept { obj.store(newObj); }
			bool hasContent() const noexcept { return !!obj.load(); }

			void clearAndDelete()
			{
				if (obj.load())
				{
					Delete()(obj);
				}
				else if (flag.load())
				{
					// no object stored, but flag is set /signaled?
					CPL_BREAKIFDEBUGGED();
				}
				obj = nullptr;
				flag.store(false);
			}


		};

		ConcurrentObjectSwapper()
		{
			current.store(wrappers);
			old = wrappers + 1;
		}

		/// <summary>
		/// Tries to remove any old objects, returning true if succesful 
		/// or false if none existed or the consumer thread hasn't called
		/// getObject yet.
		/// 'Producer' only.
		/// </summary>
		/// <returns></returns>
		bool tryRemoveOld()
		{
			auto newest = current.load();
			auto oldest = old;

			if (oldest->hasContent() && newest->hasContent() && newest->isSignaled())
			{
				oldest->clearAndDelete();
				return true;
			}

			return false;
		}

		/// <summary>
		/// If successful, Object is taken ownership of, and will be returned from getObject.
		/// Should never be called in a loop, as it potentially never returns true if nothing
		/// calls getObject.
		/// 'Producer' thread only.
		/// </summary>
		/// <param name="newObject"></param>
		/// <returns></returns>
		bool tryReplace(Object * newObject)
		{
			// TODO: move a tryRemoveOld in here?
			if (!old->hasContent())
			{
				old->reset(newObject);
				old = current.exchange(old);
				// TODO: insert release memory fence?
				return true;
			}

			return false;
		}
		/// <summary>
		/// Returns a null pointer if no objects have been stored yet.
		/// Otherwise, returns a pointer to the newest stored object through
		/// tryReplace.
		/// NOTICE: any objects returned previously through this call may be asynchronously
		/// invalidated, therefore you should only store the reference for this object in the current
		/// stack frame.
		/// 'Consumer' thread only.
		/// </summary>
		/// <returns></returns>
		Object * getObject()
		{
			// TODO: insert acquire fence?
			auto ce = current.load();
			Object * retptr = nullptr;
			if (ce->hasContent())
			{
				// implicit barriers on both operations, add fennce inbetween if ordering is weakened
				ce->signalInUse();
				retptr = ce->obj.load();
			}
			return retptr;
		}

		/// <summary>
		/// Returns a null pointer if no objects have been stored yet.
		/// Otherwise, returns a pointer to the newest stored object through tryReplace.
		/// Contrary to getObject, this function does not signal usage of the new object 
		/// - this HAS to be done through the consumer thread. As of such, this function
		/// can be called through both threads, however if getObject is never called,
		/// newer objects may never propagate as this class was never told it was safe to delete
		/// the older object.
		/// </summary>
		Object * getObjectWithoutSignaling()
		{
			// TODO: insert acquire fence?
			auto ce = current.load();
			Object * retptr = nullptr;
			if (ce->hasContent())
			{
				retptr = ce->obj;
			}
			return retptr;
		}

		/// <summary>
		/// Deletes any contained objects.
		/// </summary>
		~ConcurrentObjectSwapper()
		{
			wrappers[0].clearAndDelete();
			wrappers[1].clearAndDelete();
		}

	private:
		std::atomic<ConcurrentEntry<Object> *> current;
		ConcurrentEntry<Object> * old;
		ConcurrentEntry<Object> wrappers[2];
	};

};
#endif
