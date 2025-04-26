/**
 * Implementation of the uthreads library.
 * Authors: Ido Yanay, Omri Baum.
 */

#include "uthreads.h"

#include <iostream>
#include <cstdlib>     // for exit()
#include <list>        // for the list of READY threads
#include <csetjmp>     // for sigjmp_buf
#include <setjmp.h>


 
 // --- typedefs, enums, structs, and decleratoins for the internal use in the library --- //
typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7
enum class PrintType { SYSTEM_ERR, THREAD_LIB_ERR };
enum class ThreadState { RUNNING, READY, WAITING };
struct Thread {
    int tid;
    ThreadState state;          // (READY, RUNNING, BLOCKED)
    sigjmp_buf env;             // CPU context (saved)
    int quantum_count;          // How many quantums this thread has run
    char stack[STACK_SIZE];     // Stack memory (only needed for non-main threads)
};

std::list<Thread*> ready_threads;
std::list<Thread*> blocked_threads;
int threads_counter;
Thread* running_thread;


 // ------------------------------------------------------------------------- //
 
void print_error(const std::string& msg, PrintType type) {
     if (type == PrintType::SYSTEM_ERR) {
         std::cerr << "system error: " << msg << std::endl;
         std::exit(1); // Immediately exit with failure
     } else if (type == PrintType::THREAD_LIB_ERR) {
         std::cerr << "thread library error: " << msg << std::endl;
         // Do NOT exit — the calling function should handle the return value
     }
 }

address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%fs:0x30,%0\n"
        "rol    $0x11,%0\n"
                 : "=g" (ret)
                 : "0" (addr));
    return ret;
}

void setup_thread(char *stack, thread_entry_point entry_point, sigjmp_buf env)
{
    address_t sp = (address_t) stack + STACK_SIZE - sizeof(address_t);
    address_t pc = (address_t) entry_point;
    sigsetjmp(env, 1);
    (env->__jmpbuf)[JB_SP] = translate_address(sp);
    (env->__jmpbuf)[JB_PC] = translate_address(pc);
    sigemptyset(&env->__saved_mask);
}
 

int uthread_init(int quantum_usecs) {
     if (quantum_usecs <= 0) { 
         print_error("uthread_init: quantum_usecs must be positive", PrintType::THREAD_LIB_ERR);
         return -1;
     }
     threads_counter = 1;
     running_thread = new Thread{0, ThreadState::RUNNING, {}, 0, {}}; // initializing main thread
     sigsetjmp(running_thread->env, 1); // Save current CPU context
     return 0;
 }

 /**
 * @brief Creates a new thread, whose entry point is the function entry_point with the signature
 * void entry_point(void).
 *
 * The thread is added to the end of the READY threads list.
 * The uthread_spawn function should fail if it would cause the number of concurrent threads to exceed the
 * limit (MAX_THREAD_NUM).
 * Each thread should be allocated with a stack of size STACK_SIZE bytes.
 * It is an error to call this function with a null entry_point.
 *
 * @return On success, return the ID of the created thread. On failure, return -1.
*/
int uthread_spawn(thread_entry_point entry_point){
    
    if(threads_counter == MAX_THREAD_NUM) { // check if the number of threads is already at the maximum
        print_error("uthread_spawn: reached maximum number of threads", PrintType::THREAD_LIB_ERR);
        return -1;
    }
    else if(!entry_point){ // check if entry_point is null
        print_error("uthread_spawn: entry_point is null", PrintType::THREAD_LIB_ERR);
        return -1;
    }
    
    Thread *new_thread = new Thread{threads_counter++, ThreadState::READY, {}, 0, {}}; // create new thread
    setup_thread(new_thread->stack, entry_point, new_thread->env); // setup the new thread
    ready_threads.push_back(new_thread); // add the new thread to the ready threads list
    return new_thread->tid;
}
 