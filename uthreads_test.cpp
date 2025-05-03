#include "uthreads.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>

// Define colors for nicer test output
#define RESET "\033[0m"
#define GREEN "\033[32m"
#define RED "\033[31m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"

// Test counters
int total_tests = 0;
int passed_tests = 0;
int failed_tests = 0;

#define TEST_ASSERT(condition, message) \
    do { \
        total_tests++; \
        if (condition) { \
            passed_tests++; \
            printf("%s[PASS]%s %s\n", GREEN, RESET, message); \
        } else { \
            failed_tests++; \
            printf("%s[FAIL]%s %s\n", RED, RESET, message); \
            printf("  Line %d in %s\n", __LINE__, __FILE__); \
        } \
    } while (0)

// Global variables for testing
int global_counter = 0;
int quantum_usecs = 10000; // 10ms quantum
int thread_ids[MAX_THREAD_NUM];

// Thread functions for testing
void thread_sleep_test() {
    int tid = uthread_get_tid();
    printf("Thread %d is going to sleep for 2 quantums\n", tid);
    uthread_sleep(2);
    printf("Thread %d woke up\n", tid);
    global_counter++;
    while(1);
}

void thread_increment() {
    global_counter++;
    while(1);
}

void thread_long_run() {
    printf("Thread %d starting long run\n", uthread_get_tid());
    // Simulate long computation
    for (volatile int i = 0; i < 1000000; i++) {}
    global_counter++;
    printf("Thread %d finished long run\n", uthread_get_tid());
    while(1);
}

void thread_block_itself() {
    int tid = uthread_get_tid();
    printf("Thread %d is blocking itself\n", tid);
    uthread_block(tid);
    // This should only run after being resumed
    printf("Thread %d resumed after self-block\n", tid);
    global_counter++;
    while(1);
}

void thread_with_sleep_and_terminate() {
    int tid = uthread_get_tid();
    printf("Thread %d is going to sleep for 1 quantum and then terminate\n", tid);
    uthread_sleep(1);
    printf("Thread %d woke up and is now terminating itself\n", tid);
    uthread_terminate(tid);
    // This should never be reached
    global_counter = -999;
    while(1);
}

void thread_spawner() {
    int tid = uthread_get_tid();
    printf("Thread %d is spawning a new thread\n", tid);
    int new_tid = uthread_spawn(thread_increment);
    if (new_tid != -1) {
        thread_ids[new_tid] = 1;  // Mark as used
        printf("Thread %d spawned thread %d\n", tid, new_tid);
    }
    global_counter++;
    while(1);
}

// Test cases
void test_init() {
    printf("%s\n=== Testing initialization ===\n%s", BLUE, RESET);
    
    // Test with valid quantum
    int ret = uthread_init(quantum_usecs);
    TEST_ASSERT(ret == 0, "Initialize with valid quantum");
    
    // Initialize should have set total quantums to 1
    int total_q = uthread_get_total_quantums();
    TEST_ASSERT(total_q == 1, "Total quantums initialized to 1");
    
    // Main thread should have tid 0
    int tid = uthread_get_tid();
    TEST_ASSERT(tid == 0, "Main thread has tid 0");
    
    // Main thread should have 1 quantum
    int q = uthread_get_quantums(0);
    TEST_ASSERT(q == 1, "Main thread starts with 1 quantum");
}

void test_spawn() {
    printf("%s\n=== Testing thread spawning ===\n%s", BLUE, RESET);
    
    // Track thread IDs
    memset(thread_ids, 0, sizeof(thread_ids));
    thread_ids[0] = 1;  // Main thread
    
    // Spawn a few threads
    for (int i = 0; i < 5; i++) {
        int tid = uthread_spawn(thread_increment);
        TEST_ASSERT(tid > 0 && tid < MAX_THREAD_NUM, "Spawn returns valid tid");
        TEST_ASSERT(thread_ids[tid] == 0, "Spawn returns unused tid");
        thread_ids[tid] = 1;  // Mark as used
    }
    
    // Test thread ID reuse by terminating and respawning
    int tid_to_terminate = 1;  // First spawned thread
    int ret = uthread_terminate(tid_to_terminate);
    TEST_ASSERT(ret == 0, "Terminate thread succeeds");
    thread_ids[tid_to_terminate] = 0;  // Mark as free
    
    // Spawn a new thread, should reuse the lowest available ID
    int new_tid = uthread_spawn(thread_increment);
    TEST_ASSERT(new_tid == tid_to_terminate, "ID reuse works correctly");
    thread_ids[new_tid] = 1;  // Mark as used again
    
    // Try spawning MAX_THREAD_NUM threads (should fail at some point)

    int overflow_tid = -1;
    for (int i = 0; i < MAX_THREAD_NUM + 1; i++) {
        int tid = uthread_spawn(thread_increment);
        if (tid != -1) {
            thread_ids[tid] = 1;  // Mark as used
        } else {
            overflow_tid = i;
            break;
        }
    }
    
    TEST_ASSERT(overflow_tid != -1, "Spawn fails when exceeding MAX_THREAD_NUM");
    
    // Clean up by terminating all threads except main
    for (int i = 1; i < MAX_THREAD_NUM; i++) {
        if (thread_ids[i]) {
            uthread_terminate(i);
            thread_ids[i] = 0;
        }
    }
}

void test_block_resume() {
    printf("%s\n=== Testing block and resume ===\n%s", BLUE, RESET);
    
    // Reset thread IDs and counter
    memset(thread_ids, 0, sizeof(thread_ids));
    thread_ids[0] = 1;  // Main thread
    global_counter = 0;
    
    // Spawn a thread that blocks itself
    int block_tid = uthread_spawn(thread_block_itself);
    thread_ids[block_tid] = 1;

    TEST_ASSERT(block_tid > 0, "Spawn thread for block test");
    
    // Let the thread run and block itself
    while(uthread_get_tid() != block_tid);
    for (volatile int i = 0; i < 10000; i++) {}
    
    // Check that global_counter hasn't been incremented because thread is blocked
    TEST_ASSERT(global_counter == 0, "Thread is blocked and didn't increment counter");
    
    // Resume the thread
    int ret = uthread_resume(block_tid);
    for (volatile int i = 0; i < 2000000; i++) {}

    TEST_ASSERT(ret == 0, "Resume returns success");
    
    // Wait for thread to run
    while(uthread_get_tid() != block_tid);
    
    // Check that global_counter has been incremented after resume
    TEST_ASSERT(global_counter == 1, "Thread resumed and incremented counter");
    
    // Test block error conditions
    ret = uthread_block(0);
    TEST_ASSERT(ret == -1, "Cannot block main thread");
    
    ret = uthread_block(MAX_THREAD_NUM + 1);
    TEST_ASSERT(ret == -1, "Cannot block non-existent thread");
    
    // Clean up
    uthread_terminate(block_tid);
    thread_ids[block_tid] = 0;
}

void test_sleep() {
    printf("%s\n=== Testing sleep functionality ===\n%s", BLUE, RESET);
    
    // Reset thread IDs and counter
    memset(thread_ids, 0, sizeof(thread_ids));
    thread_ids[0] = 1;  // Main thread
    global_counter = 0;
    
    // Spawn a thread that sleeps
    int sleep_tid = uthread_spawn(thread_sleep_test);
    thread_ids[sleep_tid] = 1;
    TEST_ASSERT(sleep_tid > 0, "Spawn thread for sleep test");
    
    // Let the thread run and go to sleep
    int initial_quantums = uthread_get_total_quantums();
    
    // Wait enough time for sleep to complete (more than 2 quantums)
    while (uthread_get_total_quantums() < initial_quantums + 5) {
        for (volatile int i = 0; i < 100000; i++) {}
    }
    
    // Check that global_counter has been incremented after sleep
    TEST_ASSERT(global_counter == 1, "Thread woke up from sleep and incremented counter");
    
    // Test sleep error conditions
    int ret = uthread_sleep(2);
    TEST_ASSERT(ret == -1, "Main thread cannot sleep");
    
    // Clean up
    uthread_terminate(sleep_tid);
    thread_ids[sleep_tid] = 0;
}

void test_terminate() {
    printf("%s\n=== Testing thread termination ===\n%s", BLUE, RESET);
    
    // Reset thread IDs and counter
    memset(thread_ids, 0, sizeof(thread_ids));
    thread_ids[0] = 1;  // Main thread
    global_counter = 0;
    
    // Spawn a thread that will terminate itself
    int term_tid = uthread_spawn(thread_with_sleep_and_terminate);
    thread_ids[term_tid] = 1;
    TEST_ASSERT(term_tid > 0, "Spawn thread for self-termination test");
    
    // Let the thread run, sleep, and terminate
    int initial_quantums = uthread_get_total_quantums();
    while (uthread_get_total_quantums() < initial_quantums + 5) {
        for (volatile int i = 0; i < 100000; i++) {}
    }
    
    // Check that global_counter hasn't been set to -999
    TEST_ASSERT(global_counter == 0, "Thread terminated properly and didn't run code after terminate");
    
    // Try to terminate the already terminated thread (should fail)
    int ret = uthread_terminate(term_tid);
    TEST_ASSERT(ret == -1, "Cannot terminate already terminated thread");
    
    // Test terminate error conditions
    ret = uthread_terminate(MAX_THREAD_NUM + 1);
    TEST_ASSERT(ret == -1, "Cannot terminate non-existent thread");
    
    // Main thread termination is tested separately as it exits the program
}

void test_id_assignment() {
    printf("%s\n=== Testing thread ID assignment ===\n%s", BLUE, RESET);
    
    // Reset thread IDs
    memset(thread_ids, 0, sizeof(thread_ids));
    thread_ids[0] = 1;  // Main thread
    
    // Spawn threads with sequential IDs
    int tid1 = uthread_spawn(thread_increment);
    int tid2 = uthread_spawn(thread_increment);
    int tid3 = uthread_spawn(thread_increment);
    
    TEST_ASSERT(tid1 == 1, "First spawned thread has ID 1");
    TEST_ASSERT(tid2 == 2, "Second spawned thread has ID 2");
    TEST_ASSERT(tid3 == 3, "Third spawned thread has ID 3");
    
    // Terminate thread 2 and spawn a new one
    uthread_terminate(tid2);
    int tid4 = uthread_spawn(thread_increment);
    TEST_ASSERT(tid4 == tid2, "New thread gets ID of terminated thread");
    
    // Terminate threads in reverse order and check ID assignment
    uthread_terminate(tid3);
    uthread_terminate(tid4);
    uthread_terminate(tid1);
    
    int tid5 = uthread_spawn(thread_increment);
    TEST_ASSERT(tid5 == 1, "First available ID is reused");
    
    // Clean up
    uthread_terminate(tid5);
}

void test_quantums_counting() {
    printf("%s\n=== Testing quantum counting ===\n%s", BLUE, RESET);
    
    // Reset thread IDs
    memset(thread_ids, 0, sizeof(thread_ids));
    thread_ids[0] = 1;  // Main thread
    
    int initial_total = uthread_get_total_quantums();
    int initial_main = uthread_get_quantums(0);
    
    // Spawn threads that will run for multiple quantums
    int tid1 = uthread_spawn(thread_long_run);
    int tid2 = uthread_spawn(thread_long_run);
    
    // Wait for several quantums to pass
    for (volatile int i = 0; i < 5000000; i++) {}
    
    // Check that total quantums increased
    int new_total = uthread_get_total_quantums();
    TEST_ASSERT(new_total > initial_total, "Total quantums increased");
    
    // Check individual thread quantums
    int q_main = uthread_get_quantums(0);
    int q_tid1 = uthread_get_quantums(tid1);
    int q_tid2 = uthread_get_quantums(tid2);
    
    TEST_ASSERT(q_main >= initial_main, "Main thread quantums increased or stayed same");
    TEST_ASSERT(q_tid1 > 0, "Thread 1 ran for at least 1 quantum");
    TEST_ASSERT(q_tid2 > 0, "Thread 2 ran for at least 1 quantum");
    
    // Test quantums error conditions
    int ret = uthread_get_quantums(MAX_THREAD_NUM + 1);
    TEST_ASSERT(ret == -1, "Cannot get quantums of non-existent thread");
    
    // Clean up
    uthread_terminate(tid1);
    uthread_terminate(tid2);
}

void test_thread_functions_in_threads() {
    printf("%s\n=== Testing thread functions within threads ===\n%s", BLUE, RESET);
    
    // Reset thread IDs and counter
    memset(thread_ids, 0, sizeof(thread_ids));
    thread_ids[0] = 1;  // Main thread
    global_counter = 0;
    
    // Spawn a thread that will spawn another thread
    int spawner_tid = uthread_spawn(thread_spawner);
    thread_ids[spawner_tid] = 1;
    
    // Wait for threads to run
    for (volatile int i = 0; i < 2000000; i++) {}
    
    // Check that global_counter has been incremented at least twice
    // Once by spawner and once by its child
    TEST_ASSERT(global_counter >= 2, "Thread successfully spawned another thread");
    
    // Clean up - terminate all threads except main
    for (int i = 1; i < MAX_THREAD_NUM; i++) {
        if (thread_ids[i]) {
            uthread_terminate(i);
        }
    }
}

void test_edge_cases() {
    printf("%s\n=== Testing edge cases ===\n%s", BLUE, RESET);
    
    // Test block and resume edge cases
    int ret = uthread_block(99999);  // Non-existent thread
    TEST_ASSERT(ret == -1, "Cannot block non-existent thread");
    
    ret = uthread_resume(99999);  // Non-existent thread
    TEST_ASSERT(ret == -1, "Cannot resume non-existent thread");
    
    // Spawn a thread and test double blocking
    int tid = uthread_spawn(thread_increment);
    ret = uthread_block(tid);
    TEST_ASSERT(ret == 0, "First block succeeds");
    
    ret = uthread_block(tid);
    TEST_ASSERT(ret == 0, "Double blocking is not an error");
    
    // Test double resuming
    ret = uthread_resume(tid);
    TEST_ASSERT(ret == 0, "First resume succeeds");
    
    ret = uthread_resume(tid);
    TEST_ASSERT(ret == 0, "Resuming READY thread is not an error");
    
    // Clean up
    uthread_terminate(tid);
}

int main() {
    printf("%s==== User-Level Threads Library Test Suite ====\n%s", YELLOW, RESET);
    
    // Run tests
    test_init();
    test_spawn();
    test_block_resume();
    test_sleep();
    test_terminate();
    test_id_assignment();
    test_quantums_counting();
    test_thread_functions_in_threads();
    test_edge_cases();
    
    // Print summary
    printf("\n%s==== Test Summary ====\n%s", YELLOW, RESET);
    printf("Total tests: %d\n", total_tests);
    printf("%sPassed: %d%s\n", GREEN, passed_tests, RESET);
    if (failed_tests > 0) {
        printf("%sFailed: %d%s\n", RED, failed_tests, RESET);
    } else {
        printf("Failed: 0\n");
    }
    
    return (failed_tests > 0) ? 1 : 0;
}