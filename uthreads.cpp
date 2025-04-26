/**
 * Implementation of the uthreads library.
 * Authors: Ido Yanay, Omri Baum.
 */

#include "uthreads.h"

#include <iostream>
#include <cstdlib>     // for exit()
#include <list>        // for the list of READY threads
#include <queue>       // for std::priority_queue
#include <csetjmp>     // for sigjmp_buf
#include <setjmp.h>
#include <csignal>     // for sigemptyset
#include <sys/time.h>  // for itimerval


 
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

// this following structure will contatain the tid that have been used, for givin there tid to new threads.
std::priority_queue<int, std::vector<int>, std::greater<int>> unused_tid;

int threads_counter;
Thread* running_thread;
int quantum_per_thread;


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


// TODO: every time that usign this function, there is a negligible time that the thread is not running, 
// but the timer is working due to the time that take the return to go. 
// maybe copy-paste the code to every use of the function instead of general function.
int set_thread_timer(void){
    struct itimerval it_value;
    // the timer will not reset after the first call when initializing the it_interval like this
    it_value.it_interval.tv_sec = 0;
    it_value.it_interval.tv_usec = 0;

    it_value.it_value.tv_sec = 0;
    it_value.it_value.tv_usec = quantum_per_thread;
    return setitimer(ITIMER_VIRTUAL, &it_value, NULL);
}

void end_of_quantum(int sig)
{
    if (threads_counter == 1){
        return; // nothing to do
    }
    
    running_thread = ready_threads.front(); // setting the thread to run
    ready_threads.pop_front(); // removing the thread from the list
    ready_threads.push_back(running_thread); // pushing the thread to the end of the list
    if(set_thread_timer != 0){
        print_error("setitimer failed", PrintType::SYSTEM_ERR); // this call will end the run with exit(1)
    }
    siglongjmp(running_thread->env, 1); // jumping to the thread's context

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

    quantum_per_thread = quantum_usecs;
    threads_counter = 1;
    running_thread = new Thread{0, ThreadState::RUNNING, {}, 0, {}}; // initializing main thread
    sigsetjmp(running_thread->env, 1); // Save current CPU context

    struct sigaction sa = {0};
    sa.sa_handler = &end_of_quantum;
    if (sigaction(SIGVTALRM, &sa, NULL) < 0)
    {
        print_error("uthread_init: sigaction failed", PrintType::SYSTEM_ERR); // TODO: make sure this is the type of error
    }

    if(set_thread_timer != 0){
        print_error("uthread_init: setitimer failed", PrintType::SYSTEM_ERR); // this call will end the run with exit(1)
    }
    return 0;
 }


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

/**
 * @brief Terminates the thread with ID tid and deletes it from all relevant control structures.
 *
 * All the resources allocated by the library for this thread should be released. If no thread with ID tid exists it
 * is considered an error. Terminating the main thread (tid == 0) will result in the termination of the entire
 * process using exit(0) (after releasing the assigned library memory).
 *
 * @return The function returns 0 if the thread was successfully terminated and -1 otherwise. If a thread terminates
 * itself or the main thread is terminated, the function does not return.
*/
int uthread_terminate(int tid);
 