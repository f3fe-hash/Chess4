#pragma once

// Threads
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <memory>

#include <vector>
#include <queue>

// Errors
#include <exception>

#define MAX_THREADS 12
#define MAX_JOBS 1024


// The MultiThreadManager is an object for managing many
// concurrent, asyncronous jobs.
template<typename Return, typename Args>
class MultiThreadManager
{
    // Job
    struct ThreadJob
    {
        std::function<Return(const Args&)> main;
        int id;
    }

    // Thread
    struct Thread
    {
        std::thread thread;
        int id;
    };

    // Job Queue
    std::queue<ThreadJob> jobs;

    // Threads vector
    std::vector<Thread> threads;

    // Job results
    // NOTE: This does not need atomic operations, as each job
    // has a unique id, and this vector is never grown, except
    // for in the constructor. It does not need a lock.
    std::vector<Result> job_results;

    // Internal helpers
    ThreadJob DequeueJob();
    void Execute();

    int id;
    std::queue<int> ids;

public:
    MultiThreadManager();
    ~MultiThreadManager() = default;

    MultiThreadManager(const MultiThreadManager&) = default; // Copy

    // Queue a job for running in a thread
    int QueueJob(const std::function<Return(const Args&)>& job);

    // Wait for all jobs to finish.
    int WaitAllJobs();

    // Wait for jobs with specific ids to finish.
    int WaitJobIds(const std::vector<int>& ids);

    // Get the return value of a job.
    Return GetJobResult(const int id);

    // Get the return values of a group of jobs.
    std::vector<Return> GetJobsResult(const std::vector<int>& ids);
};
