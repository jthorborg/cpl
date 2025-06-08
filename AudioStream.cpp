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

	file:AudioStream.cpp

		Thread manager for the audio stream async instances

*************************************************************************************/

#include <thread>
#include <map>
#include <mutex>
#include <vector>
#include "AudioStream.h"

namespace cpl
{
	std::mutex globalMutex;

	class ThreadManager
	{
	public:

		static void started(std::thread&& thread)
		{
			auto& tm = get();

			if (tm.beingDestroyed)
				throw std::runtime_error("cannot start threads at destruction");

			std::lock_guard<std::mutex> lock(globalMutex);

			auto id = thread.get_id();

			tm.threads[id] = std::move(thread);
		}

		static void stallExistence(std::thread::id id)
		{
			auto& tm = get();
			while (true)
			{
				{
					std::lock_guard<std::mutex> lock(globalMutex);

					if (tm.threads.find(id) != tm.threads.end())
						return;
				}

				cpl::Misc::Delay(0);
			}
		}

		static void ended(std::thread::id id)
		{
			std::lock_guard<std::mutex> lock(globalMutex);

			get().endedThreads.emplace_back(id);
		}

		static void janitorThreads()
		{
			auto& tm = get();

			std::lock_guard<std::mutex> lock(globalMutex);

			for (auto id : tm.endedThreads)
			{
				if (auto it = tm.threads.find(id); it != tm.threads.end())
				{
					it->second.join();

					tm.threads.erase(it);
				}
			}

			tm.endedThreads.clear();
		}

		~ThreadManager() noexcept(false)
		{
			beingDestroyed = true;

			janitorThreads();

			for (auto it = threads.begin(); it != threads.end(); ++it)
			{
				if (it->second.joinable())
					it->second.join();
			}

			janitorThreads();

			CPL_RUNTIME_ASSERTION(threads.size() == 0); // TODO: This can happen and hang the process.
		}

	private:

		static ThreadManager& get()
		{
			static ThreadManager manager;
			return manager;
		}

		std::map<std::thread::id, std::thread> threads;
		std::vector<std::thread::id> endedThreads;
		std::atomic_bool beingDestroyed;

		ThreadManager() {};
	};

	namespace detail
	{
		void entry(std::function<void()>&& callback)
		{
			ThreadManager::stallExistence(std::this_thread::get_id());

#ifdef CPL_TRACEGUARD_ENTRYPOINTS
			CPL_TRACEGUARD_START
#endif

				callback();

#ifdef CPL_TRACEGUARD_ENTRYPOINTS
			CPL_TRACEGUARD_STOP("audio stream thread");
#endif
			ThreadManager::janitorThreads();
			ThreadManager::ended(std::this_thread::get_id());
		}

		void launchThread(std::function<void()>&& function)
		{
			auto thr = std::thread(entry, std::move(function));
			ThreadManager::janitorThreads();
			ThreadManager::started(std::move(thr));
		}

		void janitorThreads()
		{
			ThreadManager::janitorThreads();
		}
	}
}


