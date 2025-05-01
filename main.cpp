#include "uthreads.h"
#include <iostream>
#include <unistd.h>  // for sleep (optional)

void thread_func1() {
    std::cout << "[T1] Thread 1 is running.\n";
    std::cout << "[T1] Terminating itself...\n";
    uthread_terminate(1);  // self-terminate
    std::cout << "[T1] Should never reach here!\n";
    while(true);
}

void thread_func2() {
    std::cout << "[T2] Thread 2 is running.\n";
    for (int i = 0; i < 2; ++i) {
        std::cout << "[T2] Loop " << i << "\n";
    }
    std::cout << "[T2] Done. Yielding back.\n";
    while(true);
    // Just return — will be terminated by main
}

void nop_delay(int value) {
    volatile int dummy = 0;
    for(int i = 0; i < value; ++i) {
        dummy += 1;
    }
}
int main() {
    std::cout << "[MAIN] Initializing uthreads...\n";
    if (uthread_init(100) == -1) {
        std::cerr << "[MAIN] uthread_init failed.\n";
        return 1;
    }

    std::cout << "[MAIN] Spawning thread 1...\n";
    int tid1 = uthread_spawn(thread_func1);
    if (tid1 == -1) {
        std::cerr << "[MAIN] Failed to spawn thread 1\n";
    }

    nop_delay(10000000);  // Simulate some work in main thread

    std::cout << "[MAIN] Spawning thread 2...\n";
    int tid2 = uthread_spawn(thread_func2);
    if (tid2 == -1) {
        std::cerr << "[MAIN] Failed to spawn thread 2\n";
    }

    nop_delay(10000000);  // Simulate some work in main thread
    // Try to terminate an invalid TID
    std::cout << "[MAIN] Trying to terminate non-existent TID 999...\n";
    int err = uthread_terminate(999);
    if (err == -1) {
        std::cout << "[MAIN] Correctly failed to terminate invalid TID.\n";
    } else {
        std::cerr << "[MAIN] ERROR: invalid TID termination should fail.\n";
    }

    // Give some time for thread 1 to self-terminate
    nop_delay(10000000);  // Simulate some work in main thread

    std::cout << "[MAIN] Terminating thread 2 (TID=" << tid2 << ")...\n";
    if (uthread_terminate(tid2) == 0) {
        std::cout << "[MAIN] Successfully terminated thread 2.\n";
    } else {
        std::cerr << "[MAIN] Failed to terminate thread 2.\n";
    }

    std::cout << "[MAIN] All tests done. Main thread exiting...\n";
    uthread_terminate(0);  // This will call exit(0), so nothing after it runs

    return 0;
}

