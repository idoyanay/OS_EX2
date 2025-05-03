#include "uthreads.h"
#include <iostream>
#include <vector>
#include <string>
#include <assert.h>
#include <unistd.h>
#include <sys/time.h>

// Test suite for user-level threads library
// Compile with: g++ -std=c++11 -Wall uthreads_test.cpp -L. -luthreads -o test_uthreads

// Colors for better test output
#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"

#define QUANTUM_USECS 100000  // 100ms quantum

// Global variables for testing
int total_tests = 0;
int passed_tests = 0;
std::vector<std::string> failed_tests;

// Synchronized printing to avoid race conditions
void safe_print(const std::string& msg) {
    sigset_t mask, old_mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGVTALRM);
    sigprocmask(SIG_BLOCK, &mask, &old_mask);
    
    std::cout << msg << std::endl;
    
    sigprocmask(SIG_SETMASK, &old_mask, NULL);
}

// Test helper function
bool run_test(const std::string& test_name, bool (*test_func)()) {
    std::cout << BLUE << "[TEST] " << test_name << "... " << RESET;
    total_tests++;

    bool result = test_func();
    if (result) {
        std::cout << GREEN << "PASSED" << RESET << std::endl;
        passed_tests++;
    } else {
        std::cout << RED << "FAILED" << RESET << std::endl;
        failed_tests.push_back(test_name);
    }
    return result;
}

// Helper functions for thread operations
int thread_counter = 0;
struct ThreadInfo {
    int tid;
    int iterations;
    bool blocked;
    bool resumed;
    bool slept;
};

std::vector<ThreadInfo> thread_info(MAX_THREAD_NUM);

// Thread function that just counts iterations
void thread_func() {
    int tid = uthread_get_tid();
    thread_info[tid].tid = tid;
    
    while (thread_info[tid].iterations < 5) {
        thread_info[tid].iterations++;
        safe_print("Thread " + std::to_string(tid) + " iteration " + 
                   std::to_string(thread_info[tid].iterations));
        
        // Yield to give other threads a chance
        uthread_yield();
    }
    
    // Thread finishes by terminating itself
    uthread_terminate(tid);
}

// Thread function that blocks itself after some iterations
void blocking_thread_func() {
    int tid = uthread_get_tid();
    thread_info[tid].tid = tid;
    
    while (thread_info[tid].iterations < 3) {
        thread_info[tid].iterations++;
        safe_print("Blocking thread " + std::to_string(tid) + " iteration " + 
                   std::to_string(thread_info[tid].iterations));
        
        if (thread_info[tid].iterations == 2) {
            safe_print("Thread " + std::to_string(tid) + " blocking itself");
            thread_info[tid].blocked = true;
            uthread_block(tid);
        }
        
        uthread_yield();
    }
    
    uthread_terminate(tid);
}

// Thread function that sleeps
void sleeping_thread_func() {
    int tid = uthread_get_tid();
    thread_info[tid].tid = tid;
    
    while (thread_info[tid].iterations < 3) {
        thread_info[tid].iterations++;
        safe_print("Sleeping thread " + std::to_string(tid) + " iteration " + 
                   std::to_string(thread_info[tid].iterations));
        
        if (thread_info[tid].iterations == 2) {
            safe_print("Thread " + std::to_string(tid) + " sleeping for 3 quantums");
            thread_info[tid].slept = true;
            uthread_sleep(3);
            safe_print("Thread " + std::to_string(tid) + " woke up");
        }
        
        uthread_yield();
    }
    
    uthread_terminate(tid);
}

// TEST 1: Basic initialization and termination
bool test_init_and_terminate() {
    // Reset all thread info
    for (int i = 0; i < MAX_THREAD_NUM; i++) {
        thread_info[i] = {0, 0, false, false, false};
    }
    
    // Initialize the library with a quantum of 100ms
    int ret = uthread_init(QUANTUM_USECS);
    if (ret != 0) {
        std::cout << "Failed to initialize thread library" << std::endl;
        return false;
    }
    
    // Verify that the main thread's ID is 0
    int main_tid = uthread_get_tid();
    if (main_tid != 0) {
        std::cout << "Main thread ID is not 0" << std::endl;
        return false;
    }
    
    // Terminate the library
    uthread_terminate(0);
    return true;
}

// TEST 2: Thread creation and basic scheduling
bool test_thread_creation() {
    // Reset all thread info
    for (int i = 0; i < MAX_THREAD_NUM; i++) {
        thread_info[i] = {0, 0, false, false, false};
    }
    
    uthread_init(QUANTUM_USECS);
    
    // Create a few threads
    int tid1 = uthread_spawn(thread_func);
    int tid2 = uthread_spawn(thread_func);
    int tid3 = uthread_spawn(thread_func);
    
    if (tid1 != 1 || tid2 != 2 || tid3 != 3) {
        std::cout << "Thread IDs not assigned correctly" << std::endl;
        uthread_terminate(0);
        return false;
    }
    
    // Let threads run for a while
    for (int i = 0; i < 20; i++) {
        uthread_yield();
    }
    
    // Check if all threads completed their iterations
    bool all_completed = true;
    for (int i = 1; i <= 3; i++) {
        if (thread_info[i].iterations < 5) {
            all_completed = false;
            break;
        }
    }
    
    uthread_terminate(0);
    return all_completed;
}

// TEST 3: Thread ID reuse
bool test_thread_id_reuse() {
    // Reset all thread info
    for (int i = 0; i < MAX_THREAD_NUM; i++) {
        thread_info[i] = {0, 0, false, false, false};
    }
    
    uthread_init(QUANTUM_USECS);
    
    // Create and terminate threads to free up IDs
    int tid1 = uthread_spawn(thread_func);
    int tid2 = uthread_spawn(thread_func);
    
    // Terminate thread 1
    uthread_terminate(tid1);
    
    // Spawn a new thread, should get the ID of the terminated thread
    int tid3 = uthread_spawn(thread_func);
    
    bool id_reused = (tid3 == tid1);
    
    // Clean up
    uthread_terminate(0);
    
    return id_reused;
}

// TEST 4: Block and resume functionality
bool test_block_and_resume() {
    // Reset all thread info
    for (int i = 0; i < MAX_THREAD_NUM; i++) {
        thread_info[i] = {0, 0, false, false, false};
    }
    
    uthread_init(QUANTUM_USECS);
    
    // Create a thread that blocks itself
    int tid = uthread_spawn(blocking_thread_func);
    
    // Let it run until it blocks itself
    while (!thread_info[tid].blocked) {
        uthread_yield();
    }
    
    // Make sure it's blocked at iteration 2
    if (thread_info[tid].iterations != 2) {
        std::cout << "Thread did not block at expected iteration" << std::endl;
        uthread_terminate(0);
        return false;
    }
    
    // Resume the thread
    thread_info[tid].resumed = true;
    uthread_resume(tid);
    
    // Let it complete
    for (int i = 0; i < 10; i++) {
        uthread_yield();
    }
    
    // Check if it completed all iterations
    bool completed = (thread_info[tid].iterations == 3);
    
    // Clean up
    uthread_terminate(0);
    
    return completed;
}

// TEST 5: Sleep functionality
bool test_sleep() {
    // Reset all thread info
    for (int i = 0; i < MAX_THREAD_NUM; i++) {
        thread_info[i] = {0, 0, false, false, false};
    }
    
    uthread_init(QUANTUM_USECS);
    
    // Create a thread that sleeps
    int tid = uthread_spawn(sleeping_thread_func);
    
    // Let it run until it sleeps
    while (!thread_info[tid].slept) {
        uthread_yield();
    }
    
    // Make sure it's sleeping at iteration 2
    if (thread_info[tid].iterations != 2) {
        std::cout << "Thread did not sleep at expected iteration" << std::endl;
        uthread_terminate(0);
        return false;
    }
    
    // Wait for it to wake up and finish
    for (int i = 0; i < 15; i++) {
        uthread_yield();
    }
    
    // Check if it completed all iterations
    bool completed = (thread_info[tid].iterations == 3);
    
    // Clean up
    uthread_terminate(0);
    
    return completed;
}

// TEST 6: Test maximum number of threads
bool test_max_threads() {
    // Reset all thread info
    for (int i = 0; i < MAX_THREAD_NUM; i++) {
        thread_info[i] = {0, 0, false, false, false};
    }
    
    uthread_init(QUANTUM_USECS);
    
    // Create the maximum number of threads (minus the main thread)
    std::vector<int> tids;
    for (int i = 1; i < MAX_THREAD_NUM; i++) {
        int tid = uthread_spawn(thread_func);
        if (tid == -1) {
            std::cout << "Failed to create thread " << i << " (should succeed)" << std::endl;
            uthread_terminate(0);
            return false;
        }
        tids.push_back(tid);
    }
    
    // Try to create one more thread, should fail
    int extra_tid = uthread_spawn(thread_func);
    bool max_enforced = (extra_tid == -1);
    
    // Clean up
    uthread_terminate(0);
    
    return max_enforced;
}

// TEST 7: Test invalid inputs
bool test_invalid_inputs() {
    bool passed = true;
    
    // Test invalid quantum
    if (uthread_init(-1) != -1) {
        std::cout << "uthread_init should fail with negative quantum" << std::endl;
        passed = false;
    }
    
    // Initialize with valid quantum
    uthread_init(QUANTUM_USECS);
    
    // Test invalid thread operations
    if (uthread_terminate(MAX_THREAD_NUM) != -1) {
        std::cout << "uthread_terminate should fail with invalid tid" << std::endl;
        passed = false;
    }
    
    if (uthread_block(MAX_THREAD_NUM) != -1) {
        std::cout << "uthread_block should fail with invalid tid" << std::endl;
        passed = false;
    }
    
    if (uthread_resume(MAX_THREAD_NUM) != -1) {
        std::cout << "uthread_resume should fail with invalid tid" << std::endl;
        passed = false;
    }
    
    // Cannot block the main thread (tid 0)
    if (uthread_block(0) != -1) {
        std::cout << "uthread_block should fail when trying to block main thread" << std::endl;
        passed = false;
    }
    
    // Clean up
    uthread_terminate(0);
    
    return passed;
}

// TEST 8: Test thread priority and scheduling
bool test_scheduling() {
    // Reset all thread info
    for (int i = 0; i < MAX_THREAD_NUM; i++) {
        thread_info[i] = {0, 0, false, false, false};
    }
    
    uthread_init(QUANTUM_USECS);
    
    // Create some threads
    int tid1 = uthread_spawn(thread_func);
    int tid2 = uthread_spawn(thread_func);
    int tid3 = uthread_spawn(thread_func);
    
    // Let them run for a while
    for (int i = 0; i < 20; i++) {
        uthread_yield();
    }
    
    // Verify that scheduling was fair
    int min_iterations = std::min(thread_info[tid1].iterations,
                         std::min(thread_info[tid2].iterations,
                                  thread_info[tid3].iterations));
    
    int max_iterations = std::max(thread_info[tid1].iterations,
                         std::max(thread_info[tid2].iterations,
                                  thread_info[tid3].iterations));
    
    // The difference should not be too large in a fair scheduler
    bool fair_scheduling = (max_iterations - min_iterations <= 2);
    
    // Clean up
    uthread_terminate(0);
    
    return fair_scheduling;
}

// TEST 9: Test blocking a thread when it's already blocked
bool test_double_block() {
    // Reset all thread info
    for (int i = 0; i < MAX_THREAD_NUM; i++) {
        thread_info[i] = {0, 0, false, false, false};
    }
    
    uthread_init(QUANTUM_USECS);
    
    // Create a thread
    int tid = uthread_spawn(thread_func);
    
    // Block it
    uthread_block(tid);
    
    // Try to block it again, should not cause errors
    int ret = uthread_block(tid);
    
    // Resume it
    uthread_resume(tid);
    
    // Let it finish
    for (int i = 0; i < 10; i++) {
        uthread_yield();
    }
    
    // Clean up
    uthread_terminate(0);
    
    // The second block should succeed according to spec (it's already blocked)
    return (ret == 0);
}

// Main function to run all tests
int main() {
    std::cout << YELLOW << "===== User-Level Threads Library Test Suite =====" << RESET << std::endl;

    // Run all tests
    run_test("Test init and terminate", test_init_and_terminate);
    run_test("Test thread creation", test_thread_creation);
    run_test("Test thread ID reuse", test_thread_id_reuse);
    run_test("Test block and resume", test_block_and_resume);
    run_test("Test sleep functionality", test_sleep);
    run_test("Test maximum threads", test_max_threads);
    run_test("Test invalid inputs", test_invalid_inputs);
    run_test("Test scheduling fairness", test_scheduling);
    run_test("Test double blocking", test_double_block);

    // Print summary
    std::cout << YELLOW << "===== Test Results =====" << RESET << std::endl;
    std::cout << "Total tests: " << total_tests << std::endl;
    std::cout << "Passed tests: " << passed_tests << std::endl;
    std::cout << "Failed tests: " << (total_tests - passed_tests) << std::endl;
    
    if (!failed_tests.empty()) {
        std::cout << RED << "Failed tests:" << RESET << std::endl;
        for (const auto& test : failed_tests) {
            std::cout << "  - " << test << std::endl;
        }
    }
    
    return (passed_tests == total_tests) ? 0 : 1;
}