#include "core/multithread.hpp"


template<typename Return, typename Args>
MultiThreadManager<Return, Args>::MultiThreadManager()
{
    threads.resize(MAX_THREADS);
    for (int i = 0; i < MAX_THREADS; i++)
    {
        // Create a thread that runs `ExecuteJob`
        Thread thread;
        thread.thread = std::thread(ExecuteJob);
        thread.id = i;

        threads[i] = thread;
    }

    job_results.resize(MAX_JOBS);
    for (int i = 0; i < MAX_JOBS; i++)
    {
        ids.push(i);
    }
}


template<typename Return, typename Args>
int MultiThreadManager<Return, Args>::QueueJob(const std::function<Return(const Args&)>& job)
{
    if (ids.size() == 0)
        throw std::runtime_error("No more job ids in the queue. Try increasing `MAX_JOBS`.");

    int id = 0;

    // Get the first id.
    {
        std::lock_guard<std::mutex> lock;
        id = ids.front();
        ids.pop();
    }

    // Push the job.
    {
        std::lock_guard<std::mutex> lock;

        ThreadJob tjob = {
            .main = job;
            .id = id;
        };

        jobs.push(tjob);
    }

    return id;
}


template<typename Return, typename Args>
int MultiThreadManager<Return, Args>::WaitAllJobs()
{
    std::lock_guard<std::mutex> lock;

    for (int id = 0; id < MAX_THREADS; id++)
        threads[id].thread.join();
}


// Wait for jobs with specific ids to finish.
template<typename Return, typename Args>
int MultiThreadManager<Return, Args>::WaitJobIds(const std::vector<int>& ids)
{
    std::lock_guard<std::mutex> lock;

    for (const int id : ids)
        threads[id].thread.join();
}


// Get the return value of a job.
template<typename Return, typename Args>
Return MultiThreadManager<Return, Args>::GetJobResult(const int id)
{
    return job_results[id];
}


// Get the return values of a group of jobs.
template<typename Return, typename Args>
std::vector<Return> MultiThreadManager<Return, Args>::GetJobsResult(const std::vector<int>& ids)
{
    std::vector<Return> returns;
    returns.reserve(ids.size());

    for (const int id : ids)
        returns.push_back(job_results[id]);
    
    return returns;
}


template<typename Return, typename Args>
MultiThreadManager<Return, Args>::ThreadJob MultiThreadManager<Return, Args>::DequeueJob()
{
    std::lock_guard<std::mutex> lock;

    ThreadJob job = jobs.front(); jobs.pop();
    return job;
}


template<typename Return, typename Args>
void MultiThreadManager<Return, Args>::Execute()
{
    // TODO: Implement
    // This function is the main thread function, so it
    // should de-queue jobs, then run them inside this
    // function.
}

