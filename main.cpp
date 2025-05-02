#include "uthreads.h"
#include <iostream>

// --- Thread A function ---
void thread_func_A() {
    for (int i = 0; i < 3; ++i) {
        std::cout << "[Thread A] Iteration " << i << std::endl;
    }
    std::cout << "[Thread A] Terminating self." << std::endl;
    uthread_terminate(uthread_get_tid()); // Terminate self
}

// --- Thread B function ---
void thread_func_B() {
    for (int i = 0; i < 2; ++i) {
        std::cout << "[Thread B] Iteration " << i << std::endl;
    }
    std::cout << "[Thread B] Terminating self." << std::endl;
    uthread_terminate(uthread_get_tid()); // Terminate self
}

// --- Main function ---
int main() {
    std::cout << "[MAIN] Initializing uthreads..." << std::endl;
    if (uthread_init(100000) == -1) {
        std::cerr << "[MAIN] uthread_init failed." << std::endl;
        return 1;
    }

    std::cout << "[MAIN] Spawning Thread A..." << std::endl;
    int tid_A = uthread_spawn(thread_func_A);
    if (tid_A == -1) {
        std::cerr << "[MAIN] Failed to spawn Thread A." << std::endl;
        return 1;
    }

    std::cout << "[MAIN] Spawning Thread B..." << std::endl;
    int tid_B = uthread_spawn(thread_func_B);
    if (tid_B == -1) {
        std::cerr << "[MAIN] Failed to spawn Thread B." << std::endl;
        return 1;
    }

    std::cout << "[MAIN] All threads spawned. Main thread remains alive..." << std::endl;

    // Busy loop to allow virtual time to pass (so signals fire)
    while (true) {
        volatile int sink = 0;
        for (int i = 0; i < 1000000; ++i) {
            sink += i;
        }
    }

    return 0;
}
