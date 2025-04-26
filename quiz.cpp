#include <thread>
#include <mutex>
#include <condition_variable>
#include <iostream>
#include <unistd.h>

class pair_lock
{
public:
    pair_lock(void);
    void lock(void);
    void release(void);

private:
    std::mutex mtx;
    std::condition_variable cv;
    int waiting;        // How many threads are waiting to form a pair
    int inside;         // How many threads are currently inside critical section
    int release_count;  // How many threads already called release
};

pair_lock::pair_lock(void)
{
    waiting = 0;
    inside = 0;
    release_count = 0;
}

void pair_lock::lock(void)
{
    std::unique_lock<std::mutex> lock(mtx);

    while (inside == 2) // If already two inside, wait for them to finish
    {
        cv.wait(lock);
    }

    waiting++;

    if (waiting < 2)
    {
        // First thread: wait for partner
        cv.wait(lock, [this]() { return waiting == 2; });
    }
    else
    {
        // Second thread: notify the first
        cv.notify_one();
    }

    inside++;
}

void pair_lock::release(void)
{
    std::unique_lock<std::mutex> lock(mtx);

    release_count++;

    if (release_count < 2)
    {
        // First thread to call release waits for the partner
        cv.wait(lock, [this]() { return release_count == 2; });
    }
    else
    {
        // Second thread: notify the first thread waiting in release
        cv.notify_one();
    }

    // After both have released, reset for next pair
    inside = 0;
    waiting = 0;
    release_count = 0;

    // Wake up threads waiting to lock
    cv.notify_all();
}




#define N 20

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