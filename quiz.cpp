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
    int waiting;      // Threads waiting to pair
    int inside;       // Threads inside critical section
    int releasing;    // Threads currently releasing
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

    // If 2 threads already inside, wait for them to finish
    while (inside == 2)
    {
        cv.wait(lock);
    }

    waiting++;

    if (waiting < 2)
    {
        // First thread waits for the second
        cv.wait(lock, [this]() { return waiting == 2; });
    }
    else
    {
        // Second thread wakes the first
        cv.notify_one();
    }

    inside++;

    // After forming a pair, immediately prevent others from entering
    if (inside == 2)
    {
        waiting = 0; // Reset waiting counter ONLY after both entered
    }
}

void pair_lock::release(void)
{
    std::unique_lock<std::mutex> lock(mtx);

    releasing++;

    if (releasing < 2)
    {
        // First thread to release waits for second
        cv.wait(lock, [this]() { return releasing == 2; });
    }
    else
    {
        // Second thread wakes first
        cv.notify_one();
    }

    inside--;

    if (inside == 0)
    {
        // After both finished, reset releasing and allow new pair
        releasing = 0;
        cv.notify_all();
    }
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