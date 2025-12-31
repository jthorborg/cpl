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

	file:JobSystem.h

		A simple multi-dependency job system.

		alternatively, https://github.com/vit-vit/ctpl

*************************************************************************************/

#ifndef CPL_JOBSYSTEM_H
#define CPL_JOBSYSTEM_H

#include "MacroConstants.h"
#include <functional>
#include <algorithm>
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>
#include <future>
#include <queue>
#include "lib/variable_array.h"

#ifndef CPL_MAC
#define CPL_HAVE_PAR_STD_ALGORITHMS
#include <execution>
#endif

namespace cpl
{
    class JobSystem
    {
        struct BaseJob
        {
            std::mutex graph;
            std::atomic_size_t parents;
            virtual void execute(int lane) = 0;
            virtual ~BaseJob() {}

            bool addDependent(std::shared_ptr<BaseJob> child)
            {
                std::lock_guard<std::mutex> lock(graph);

                if (!completed)
                {
                    children.emplace_back(std::move(child));
                    return true;
                }

                return false;
            }

        protected:
            std::vector<std::shared_ptr<BaseJob>> children;
            bool completed{};
        };

        template<typename R>
        struct Job : public BaseJob
        {
            std::packaged_task<R()> fn;

            void execute(int lane) override
            {
                std::lock_guard<std::mutex> lock(graph);

                if (completed)
                    return;

                fn();
                completed = true;

                for (auto& j : children)
                {
                    // we were the last parent?
                    if (j->parents.fetch_sub(1) == 1)
                    {
                        j->execute(lane);
                    }
                }
            }
        };

    public:

        JobSystem(std::size_t workers)
        {
            start(workers);
        }

        ~JobSystem()
        {
            shutdown();
        }

        struct Handle
        {
            friend class JobSystem;

        protected:
            std::weak_ptr<BaseJob> job;
        };

        template<typename T>
        struct Result : public Handle
        {
            friend class JobSystem;

        public:

            T complete()
            {
                return result.get();
            }

        private:
            std::future<T> result;
        };

        template<typename... Handles>
        Handle combineDependencies(Handles&&... handles)
        {
            return schedule([] {}, std::forward<Handles>(handles)...);
        }

        template<class Callable, typename... Handles>
        auto schedule(Callable&& callable, Handles&&... handles)
        {
            std::array<Handle, sizeof...(Handles)> baseHandles{ handles... };

            using R = std::decay_t<decltype(callable())>;

            auto job = std::make_shared<Job<R>>();
            job->fn = std::packaged_task<R(void)>(std::move(callable));
            job->parents = baseHandles.size();

            Result<R> ret;
            ret.result = job->fn.get_future();

            bool hasAnyParents = false;
            // prevent eager execution from parents during the execution of the following loop
            {
                std::lock_guard<std::mutex> executionLock(job->graph);

                for (auto& handle : baseHandles)
                {
                    if (auto parent = handle.job.lock())
                    {
                        if (parent->addDependent(job))
                        {
                            hasAnyParents = true;
                            continue;
                        }
                    }
                    job->parents--;
                }
            }

            if (!hasAnyParents)
            {
                push(std::move(job));
            }

            return ret;
        }

        void restart(std::size_t workers)
        {
            shutdown();
            start(workers);
        }

        static JobSystem& getShared();

        std::size_t concurrency() const noexcept
        {
            return nthreads.load(std::memory_order_acquire);
        }

    private:


        void start(std::size_t workers)
        {
            quit = false;

            for (std::size_t i = 0; i < workers; ++i)
            {
                threads.emplace_back(&JobSystem::entry, this, static_cast<int>(i));
            }

            cv.notify_all();
            nthreads.store(workers, std::memory_order_release);
        }

        void shutdown()
        {
            // TODO: concurrent shutdown?
            quit = true;
            cv.notify_all();

            for (auto& thread : threads)
                thread.join();

            threads.clear();

            std::lock_guard<std::mutex> lk(mutex);

            while (!jobs.empty())
            {
                auto job = jobs.front();
                job->execute(0);
                jobs.pop();
            }
        }

        void push(std::shared_ptr<BaseJob> job)
        {
            std::lock_guard<std::mutex> lk(mutex);
            jobs.push(std::move(job));

            cv.notify_one();
        }

        void entry(int lane)
        {
            std::unique_lock<std::mutex> lk(mutex);

            while (true)
            {
                while (true)
                {
                    if (quit)
                        return;

                    if (jobs.size())
                    {
                        auto front = jobs.front();
                        jobs.pop();

                        lk.unlock();
                        front->execute(lane);
                        lk.lock();
                        continue;
                    }

                    break;
                }

                cv.wait(lk, [this] { return quit || jobs.size(); });
            }
        }

        std::condition_variable cv;
        std::mutex mutex;
        std::queue<std::shared_ptr<BaseJob>> jobs;
        std::atomic_bool quit{ false };
        std::vector<std::thread> threads;
        std::atomic_size_t nthreads;
    };

    namespace jobs
    {
        template<class Callable, typename... Handles>
        auto schedule(Callable&& callable, Handles&&... handles)
        {
            return JobSystem::getShared().schedule(
                std::move(callable),
                std::forward<Handles>(handles)...
            );
        }       
        
        template<typename... Handles>
        JobSystem::Handle combineDependencies(Handles&&... handles)
        {
            return schedule([] {}, std::forward<Handles>(handles)...);
        }

        template<class Callable>
        void parallel_for(std::size_t n, Callable&& callable)
        {
            if (n == 0)
                return;

            if (n == 1)
            {
                callable(0);
                return;
            }

#ifdef CPL_HAVE_PAR_STD_ALGORITHMS

            cpl::variable_array<std::size_t> ns(n, [](std::size_t i) { return i; });

            std::for_each(std::execution::par_unseq, ns.begin(), ns.end(), std::forward<Callable>(callable));
#else

            auto& system = JobSystem::getShared();
            auto hw = std::min(n, system.concurrency());
            auto concurrency = std::max(static_cast<std::size_t>(1), n / hw);
            auto remainder = n - hw * concurrency;

            if (remainder == 0)
            {
                remainder = concurrency;
                hw--;
            }

            cpl::variable_array<cpl::JobSystem::Result<void>> handles(
                hw, 
                [&] (std::size_t i)
                {
                    return cpl::jobs::schedule(
                        [&, i]
                        {
                            for (std::size_t c = 0; c < concurrency; ++c)
                            {
                                callable(i * concurrency + c);
                            }
                        }
                    );
                }
            );

            for (std::size_t c = 0; c < remainder; ++c)
            {
                callable(hw * concurrency + c);
            }

            for (auto& r : handles)
                r.complete();
#endif
        }
    }

};

#endif
