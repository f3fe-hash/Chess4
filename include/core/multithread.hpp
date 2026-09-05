#pragma once

// Threads
#include <functional>
#include <thread>
#include <condition_variable>
#include <mutex>

#include <vector>
#include <queue>

// Errors
#include <exception>

constexpr int MAX_THREADS = 12;
constexpr int MAX_JOBS = 1024;


// The MultiThreadManager is an object for managing many
// concurrent, asyncronous jobs.
template<typename Return, typename Args>
class MultiThreadManager
{
    // Job
    struct ThreadJob
    {
        std::function<Return(const Args&)> main;
        Args args;
        int id;
    };

    std::queue<ThreadJob> jobs;
    std::vector<std::thread> threads;
    std::vector<Return> job_results;
    std::vector<bool> job_done;
    std::vector<bool> job_in_use;
    std::queue<int> ids;

    std::mutex mutex;
    std::condition_variable job_available;
    std::condition_variable job_completed;
    bool stopping = false;
    size_t pending_jobs = 0;

    void Execute();

public:
    explicit MultiThreadManager(bool start_workers = true)
    {
        job_results.resize(MAX_JOBS);
        job_done.resize(MAX_JOBS, false);
        job_in_use.resize(MAX_JOBS, false);

        for (int id = 0; id < MAX_JOBS; ++id)
            ids.push(id);

        if (start_workers)
        {
            threads.reserve(MAX_THREADS);
            for (int i = 0; i < MAX_THREADS; ++i)
                threads.emplace_back(&MultiThreadManager::Execute, this);
        }
    }

    ~MultiThreadManager()
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            stopping = true;
        }
        job_available.notify_all();

        for (std::thread& thread : threads)
        {
            if (thread.joinable())
                thread.join();
        }
    }

    MultiThreadManager(const MultiThreadManager&) = delete;
    MultiThreadManager& operator=(const MultiThreadManager&) = delete;

    // Queue a job for running in a thread
    int QueueJob(
        const std::function<Return(const Args&)>& job,
        const Args& args)
    {
        std::lock_guard<std::mutex> lock(mutex);

        if (ids.empty())
            throw std::runtime_error("No more job ids in the queue.");

        const int id = ids.front();
        ids.pop();

        jobs.push(ThreadJob{job, args, id});
        job_done[id] = false;
        job_in_use[id] = true;
        ++pending_jobs;

        job_available.notify_one();
        return id;
    }

    // Wait for all jobs to finish.
    int WaitAllJobs()
    {
        std::unique_lock<std::mutex> lock(mutex);
        job_completed.wait(lock, [this]() { return pending_jobs == 0; });

        for (int id = 0; id < MAX_JOBS; ++id)
        {
            if (job_in_use[id])
            {
                job_in_use[id] = false;
                ids.push(id);
            }
        }

        return 0;
    }

    // Wait for jobs with specific ids to finish.
    int WaitJobIds(const std::vector<int>& job_ids)
    {
        std::unique_lock<std::mutex> lock(mutex);

        for (const int id : job_ids)
        {
            if (id < 0 || id >= MAX_JOBS || !job_in_use[id])
                throw std::out_of_range("Invalid job id.");
        }

        job_completed.wait(lock, [this, &job_ids]()
        {
            for (const int id : job_ids)
            {
                if (!job_done[id])
                    return false;
            }
            return true;
        });

        for (const int id : job_ids)
        {
            if (job_in_use[id])
            {
                job_in_use[id] = false;
                ids.push(id);
            }
        }

        return 0;
    }

    // Get the return value of a job.
    Return GetJobResult(const int id)
    {
        std::lock_guard<std::mutex> lock(mutex);
        return job_results.at(id);
    }

    // Get the return values of a group of jobs.
    std::vector<Return> GetJobsResult(const std::vector<int>& job_ids)
    {
        std::lock_guard<std::mutex> lock(mutex);
        std::vector<Return> results;
        results.reserve(job_ids.size());

        for (const int id : job_ids)
            results.push_back(job_results.at(id));

        return results;
    }
};


template<typename Return, typename Args>
void MultiThreadManager<Return, Args>::Execute()
{
    while (true)
    {
        ThreadJob job;

        {
            std::unique_lock<std::mutex> lock(mutex);
            job_available.wait(lock, [this]()
            {
                return stopping || !jobs.empty();
            });

            if (stopping && jobs.empty())
                return;

            job = std::move(jobs.front());
            jobs.pop();
        }

        Return result{};
        try
        {
            result = job.main(job.args);
        }
        catch (...)
        {
            // Keep the worker alive and make failed jobs observable as a
            // default result to the caller.
        }

        {
            std::lock_guard<std::mutex> lock(mutex);
            job_results[job.id] = std::move(result);
            job_done[job.id] = true;
            --pending_jobs;
        }
        job_completed.notify_all();
    }
}
