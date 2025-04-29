#include "uthreads.h"
#include <iostream>

void thread_func1() {
    std::cout << "Thread 1 is running!" << std::endl;
    uthread_terminate(1); // Thread 1 terminates itself
}

void thread_func2() {
    std::cout << "Thread 2 is running!" << std::endl;
    uthread_terminate(2); // Thread 2 terminates itself
}

int main() {
    if (uthread_init(1000000) == -1) { // 1 second quantum
        std::cerr << "uthread_init failed." << std::endl;
        return 1;
    }

    int tid1 = uthread_spawn(thread_func1);
    if (tid1 == -1) {
        std::cerr << "uthread_spawn for thread 1 failed." << std::endl;
        return 1;
    }

    int tid2 = uthread_spawn(thread_func2);
    if (tid2 == -1) {
        std::cerr << "uthread_spawn for thread 2 failed." << std::endl;
        return 1;
    }

    // Main thread is just waiting while threads 1 and 2 terminate themselves
    while (true) {} // Infinite loop to keep main alive
}
