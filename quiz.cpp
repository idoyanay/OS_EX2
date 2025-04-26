#include <iostream>
#include <unistd.h>
#include <thread>
#include <mutex>

#define N 20


#include <atomic>
#include <condition_variable>


class pair_lock
{
    std::mutex mtx;
    std::condition_variable cv_enter;
    std::condition_variable cv_exit;
    int waiting;       // Number of threads waiting to enter
    int inside;        // Number of threads currently inside
    int releasing;     // Number of threads trying to release

public:
    pair_lock(void);
    void lock(void);
    void release(void);

private:
    // nothing private extra
};

pair_lock::pair_lock(void)
{
    waiting = 0;
    inside = 0;
    releasing = 0;
}

void pair_lock::lock(void)
{
    std::unique_lock<std::mutex> lock(mtx);

    // If already two inside, wait for them to finish first
    while (inside == 2)
    {
        cv_exit.wait(lock);
    }

    waiting++;

    if (waiting < 2)
    {
        // First thread: wait for second thread
        cv_enter.wait(lock, [this]() { return waiting == 2; });
    }
    else
    {
        // Second thread arrived: notify first thread
        cv_enter.notify_one();
    }

    // Now both are ready to enter
    inside++;
}

void pair_lock::release(void)
{
    std::unique_lock<std::mutex> lock(mtx);

    releasing++;

    if (releasing < 2)
    {
        // First thread to release: wait for partner
        cv_exit.wait(lock, [this]() { return releasing == 2; });
    }
    else
    {
        // Second thread to release: notify the first
        cv_exit.notify_one();
    }

    // After both released, reset for next pair
    inside = 0;
    waiting = 0;
    releasing = 0;

    // Wake up any threads waiting for the next pair
    cv_exit.notify_all();
}



void thread_func(pair_lock &pl, std::mutex &mtx, int &inside, int tid)
{
    pl.lock();
    inside = 0;
    usleep(300);
    mtx.lock();
    int t = inside++;
    mtx.unlock();
    usleep(300);
    if(inside == 2)
    {
        if(t == 0) std::cout << "OK" << std::endl;
    }
    else
    {
        if(t == 0) std::cout << "FAIL - there are " << inside << " threads inside the critical section" << std::endl;
    }
    pl.release();
}

int main(int argc, char *argv[])
{
    pair_lock pl;
    std::mutex mtx;

    std::jthread threads[N];

    int inside = 0;
    for(int i = 0; i < N; i++)
    {
        threads[i] = std::jthread(thread_func, std::ref(pl), std::ref(mtx), std::ref(inside), i);
    }
    return 0;
}