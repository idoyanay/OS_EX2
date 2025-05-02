#include "uthreads.h"
#include <iostream>

// Thread A will block itself and wait to be resumed
void thread_func_A() {
    int tid = uthread_get_tid();
    std::cout << "[Thread A] My TID is " << tid << std::endl;

    std::cout << "[Thread A] Blocking myself.\n";
    uthread_block(tid);  // Should yield

    std::cout << "[Thread A] Resumed! Terminating.\n";
    uthread_terminate(tid);
}

// Thread B will run and then terminate itself
void thread_func_B() {
    int tid = uthread_get_tid();
    std::cout << "[Thread B] My TID is " << tid << std::endl;

    for (int i = 0; i < 2; ++i) {
        std::cout << "[Thread B] Loop " << i << std::endl;
    }

    std::cout << "[Thread B] Terminating.\n";
    uthread_terminate(tid);
}

// Thread C does nothing and gets terminated by main
void thread_func_C() {
    int tid = uthread_get_tid();
    std::cout << "[Thread C] My TID is " << tid << std::endl;

    std::cout << "[Thread C] Doing nothing. Waiting to be killed...\n";
    while (true) {
        // keep thread alive until terminated
        volatile int x = 0;
        for (int i = 0; i < 10000; ++i) x += i;
    }
}

int main() {
    std::cout << "[MAIN] Initializing uthreads...\n";
    if (uthread_init(100000) == -1) {
        std::cerr << "[MAIN] uthread_init failed.\n";
        return 1;
    }

    int tid_A = uthread_spawn(thread_func_A);
    int tid_B = uthread_spawn(thread_func_B);
    int tid_C = uthread_spawn(thread_func_C);

    if (tid_A == -1 || tid_B == -1 || tid_C == -1) {
        std::cerr << "[MAIN] Failed to spawn threads.\n";
        return 1;
    }

    std::cout << "[MAIN] Spawned threads: A=" << tid_A << ", B=" << tid_B << ", C=" << tid_C << std::endl;

    // Wait for A to block itself
    for (int i = 0; i < 3; ++i) {
        volatile int dummy = 0;
        for (int j = 0; j < 1000000; ++j) dummy += j;
    }

    std::cout << "[MAIN] Resuming Thread A...\n";
    uthread_resume(tid_A);

    // Try to resume invalid TID
    if (uthread_resume(999) == -1) {
        std::cout << "[MAIN] Correctly failed to resume invalid TID.\n";
    }

    // Try to block invalid TID
    if (uthread_block(999) == -1) {
        std::cout << "[MAIN] Correctly failed to block invalid TID.\n";
    }

    // Give Thread A time to run and terminate itself
    for (int i = 0; i < 3; ++i) {
        volatile int dummy = 0;
        for (int j = 0; j < 1000000; ++j) dummy += j;
    }

    std::cout << "[MAIN] Terminating Thread C manually (TID=" << tid_C << ")\n";
    if (uthread_terminate(tid_C) == 0) {
        std::cout << "[MAIN] Successfully terminated Thread C.\n";
    }

    std::cout << "[MAIN] Total quantums: " << uthread_get_total_quantums() << std::endl;
    std::cout << "[MAIN] Quantums for Thread B: " << uthread_get_quantums(tid_B) << std::endl;

    std::cout << "[MAIN] All tests done. Main thread terminating.\n";
    uthread_terminate(0);  // exits the process

    return 0;
}
