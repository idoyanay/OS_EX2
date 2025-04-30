/**
 * Implementation of the uthreads library.
 * Authors: Ido Yanay, Omri Baum.
 * 
 * Notes (for implementation ): 
 * 1. need to check if realy need blocking itimer signals when runnign librarys fucntions.
 * 2. check the sigsetjmp in the init function.
 * 
 * TODO -- delte the notes
 */

#include "uthreads.h"

#include <iostream>
#include <cstdlib>     // for exit()
#include <list>        // for the list of READY threads
#include <queue>       // for std::priority_queue
#include <set>         // for std::set
#include <csetjmp>     // for sigjmp_buf
#include <setjmp.h>
#include <csignal>     // for sigemptyset
#include <sys/time.h>  // for itimerval


 
 // --- typedefs, enums, structs, and decleratoins for the internal use in the library --- //
typedef unsigned long address_t;    // for the translation function
#define JB_SP 6
#define JB_PC 7
enum class PrintType { SYSTEM_ERR, THREAD_LIB_ERR }; // print type for the error printing
enum class ThreadState { RUNNING, READY, WAITING }; // state of the thread

// struct that contain all the relevant data
struct Thread { 
    int tid;
    ThreadState state;          // (READY, RUNNING, BLOCKED)
    sigjmp_buf env;             // CPU context (saved)
    int quantum_count;          // How many quantums this thread has run
    char stack[STACK_SIZE];     // Stack memory (only needed for non-main threads)
};

static struct itimerval timer;                  // timer object for all the threads
static std::list<Thread*> unblocked_threads;    // double-linkedList for the UNBLOCKED threads. the first one (front) will be the running.
static std::list<Thread*> blocked_threads;      // double-linkedList for the BLOCKED threads
static sigset_t sigvtalrm_set;                  // signals-set for storing the ITIMER signal, for blocking when calling a library function

static std::set<int> unused_tid;                // set of unused_tid, so when a new thread is adding when there was already 
                                                // other thread that had terminated, it will get his value. (note - the set is sorted from min to max)

static int quantum_per_thread;                  // global value (init in the init-function) for the sig-handler to use




 // ------------------------------------------------------------------------- //
 
void print_error(const std::string& msg, PrintType type) {
    /**
     * function for printing the error that the pdf specified. use this for generic and not doing typing errors.
     */
    if (type == PrintType::SYSTEM_ERR) {
        std::cerr << "system error: " << msg << std::endl;
        std::exit(1); // Immediately exit with failure
    } else if (type == PrintType::THREAD_LIB_ERR) {
        std::cerr << "thread library error: " << msg << std::endl;
        // Do NOT exit — the calling function should handle the return value
    }
 }




void end_of_quantum(int sig)
{
    /**
     * the signal-hanlder for ITMIER_VIRTUAL (the signal the represents the end of the quantum for the running thread).
     * first, updating the runnign thread - only if there are others ready threads.
     * after that, config the itimer for running again for the next thread.
     * at the end, jumping to the new thread - if we changed (meaning there was more then 1 ready thread)
     */
    std::cout<<"detected itimer signal"<<std::endl;
    if (unblocked_threads.size() > 1){ // if there is another ready thread
        unblocked_threads.front()->state = ThreadState::READY; // updating the running thread
        unblocked_threads.push_back(unblocked_threads.front()); // pushing the thread to the end of the list
        unblocked_threads.pop_front(); // removing the thread from the list
        unblocked_threads.front()->state = ThreadState::RUNNING; // updating the runnign thread
        
    }
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = quantum_per_thread;
    if(setitimer(ITIMER_VIRTUAL, &timer, NULL) != 0){ // check if restarting the timer had faild
        print_error("setitimer failed", PrintType::SYSTEM_ERR); // this call will end the run with exit(1)
    }
    if (unblocked_threads.size() > 1){ // check if needs to jump to other thread because of changing
        siglongjmp(unblocked_threads.front()->env, 1); // jumping to the thread's context
    }
    

}

address_t translate_address(address_t addr)
{
    /**
     * 'black-box' given in the examples
     */
    address_t ret;
    asm volatile("xor    %%fs:0x30,%0\n"
        "rol    $0x11,%0\n"
                 : "=g" (ret)
                 : "0" (addr));
    return ret;
}

void setup_thread(char* stack, thread_entry_point entry_point, sigjmp_buf& env)
{
    /**
     * setup thread like in the example
     */
    address_t sp = (address_t) stack + STACK_SIZE - sizeof(address_t);
    address_t pc = (address_t) entry_point;
    sigsetjmp(env, 1);
    (env->__jmpbuf)[JB_SP] = translate_address(sp);
    (env->__jmpbuf)[JB_PC] = translate_address(pc);
    sigemptyset(&env->__saved_mask);
}
 

void init_itimer_sigset()
{
    /**
     * initialize the sigset that will used for blocking the itimer signal when entering library functions.
     */
    if (sigemptyset(&sigvtalrm_set) < 0) {
        print_error("sigemptyset failed", PrintType::SYSTEM_ERR);
    }
    if (sigaddset(&sigvtalrm_set, SIGVTALRM) < 0) {
        print_error("sigaddset failed", PrintType::SYSTEM_ERR);
    }
}

void block_timer_signal()
{
    /**
     * blocking itimer signal for allowing the current thread to use the library function and not getting undefined behavior
     */
    sigset_t oldset;
    if (sigprocmask(SIG_BLOCK, &sigvtalrm_set, &oldset) < 0) {
        print_error("sigprocmask block failed", PrintType::SYSTEM_ERR);
    }
}

void unblock_timer_signal()
{
    /**
     * unblocking (only used after blocking)
     */
    sigset_t oldset;
    if (sigprocmask(SIG_UNBLOCK, &sigvtalrm_set, &oldset) < 0) {
        print_error("sigprocmask unblock failed", PrintType::SYSTEM_ERR);
    }
}

int uthread_init(int quantum_usecs) 
{
    /**
     * Function flow: checking input, init sigset and quantum-global, create main thread, updaiting sig-hangler, updaiting itimer.
     */
    block_timer_signal();
    if (quantum_usecs <= 0) { 
        print_error("uthread_init: quantum_usecs must be positive", PrintType::THREAD_LIB_ERR);
        return -1;
    }

    init_itimer_sigset(); // init the sigset for later blocking and unblocking the itimer-signal
    quantum_per_thread = quantum_usecs; // updaiting for the sig-handler to use
    unblocked_threads.push_front(new Thread{0, ThreadState::RUNNING, {}, 0, {}}); // initializing main thread
    if(sigsetjmp(unblocked_threads.front()->env, 1) == 0){ // Save current CPU context // TODO - this line needs checking. maybe needs to setjmp later.

        // create and update the sig-handler
        struct sigaction sa = {0};
        sa.sa_handler = &end_of_quantum;
        if (sigaction(SIGVTALRM, &sa, NULL) < 0)
        {
            print_error("uthread_init: sigaction failed", PrintType::SYSTEM_ERR); // TODO: make sure this is the type of error
        }


        // -- the timer will not reset after the first call when initializing the it_interval like this. 
        // only the main thread needs to do this.
        timer.it_interval.tv_sec = 0;
        timer.it_interval.tv_usec = 0;
        //--
        timer.it_value.tv_sec = 0;
        timer.it_value.tv_usec = quantum_per_thread;
        if(setitimer(ITIMER_VIRTUAL, &timer, NULL) != 0){
            print_error("uthread_init: setitimer failed", PrintType::SYSTEM_ERR); // this call will end the run with exit(1)
        }
        unblock_timer_signal();
        return 0;
    }
    return 0;

 }


int uthread_spawn(thread_entry_point entry_point){
    /**
     * Function flow: block itimer-signal, checking MAX_THREADS and input, updaiting new tid, create and update the new thread, unblock itimer-signal
     */
    block_timer_signal();

    
    
    // ------- debug ----------//
    struct itimerval current;
    if (getitimer(ITIMER_VIRTUAL, &current) == -1) {
        perror("getitimer failed");
        return -1;
    }
    std::cout << "current usec remain in itimer: "<<current.it_value.tv_usec<<std::endl;
    
    int num_threads = unblocked_threads.size() + blocked_threads.size();
    if(num_threads >= MAX_THREAD_NUM) { // check if the number of threads is already at the maximum 
        print_error("uthread_spawn: reached maximum number of threads", PrintType::THREAD_LIB_ERR);
        return -1;
    }
    else if(!entry_point){ // check if entry_point is null
        print_error("uthread_spawn: entry_point is null", PrintType::THREAD_LIB_ERR);
        return -1;
    }
    
    int tid;
    if (!unused_tid.empty()) { // if there is an tid that had terminated and we need to take his tid
        tid = *unused_tid.begin(); // get the smallest TID
        unused_tid.erase(unused_tid.begin()); // remove it from the set
    } else {
        tid = num_threads; // the tid's starts from 0, so the number of current threads is the new tid.
    }
    Thread *new_thread = new Thread{tid, ThreadState::READY, {}, 0, {}}; // create new thread
    setup_thread(new_thread->stack, entry_point, new_thread->env); // setup the new thread
    unblocked_threads.push_back(new_thread); // add the new thread to the ready threads list
    
    unblock_timer_signal();
    return new_thread->tid;
}

void terminate_program(){
    /**
     * terminate the program when terminte function called with tid==0. deleting all the Threads, because they are on the heap.
     */
    for (Thread* t : blocked_threads) {
        delete t;
    }
    blocked_threads.clear();

    for (Thread* t : unblocked_threads) {
        delete t;
    }
    unblocked_threads.clear();
    exit(0);
}



std::list<Thread*>::iterator find_thread_in_list(std::list<Thread*>& lst, int wanted_tid) {
    /**
     * find thread based on tid. on success, return the iterator that points to the thread. on fail return lst.end().
     */
    for (auto it = lst.begin(); it != lst.end(); ++it) {
	if ((*it)->tid == wanted_tid) {
             return it;  // found
        }
    }
    return lst.end();  // not found
}

int delete_from_list(std::list<Thread*>& lst, int tid){
    /**
     * delete the wanted thread based on the tid, from the lst. return 0 if succseeded (the thread in the lst) and -1 on fail.
     */
    auto thread_itr = find_thread_in_list(lst, tid);
    if(thread_itr != lst.end()){
	unused_tid.insert((*thread_itr)->tid); // adding the tid of the terminated thread to the unused.
        delete *thread_itr; //delete content and resources
        lst.erase(thread_itr);
        return 0;
    }
    return -1;
}

int uthread_terminate(int tid){
    /**
     * Function flow: check if tid==0 for terminating the whole program. checking if tid is the tid of the running thread (requare more updates).
     *                trying to delte the thread fits to the tid from the lists of theads. if not succseeded, means that the tid is not valid.
     */


    block_timer_signal();
    if(tid == 0){
        terminate_program();
    }
    
    if(tid == unblocked_threads.front()->tid){
        unused_tid.insert(unblocked_threads.front()->tid); // adding the tid of the terminated thread to the unused.
        delete unblocked_threads.front();
        unblocked_threads.pop_front(); // it is gurenteed (writen in the forum) that the main thread will not be blocked. so, if tid != 0 and we got here then the list.size>2.
        unblocked_threads.front()->state = ThreadState::RUNNING;
        siglongjmp(unblocked_threads.front()->env, 1); // the function not return, moving to the next thread.
        
    }

    if(delete_from_list(unblocked_threads, tid) != 0){
	if(delete_from_list(blocked_threads, tid) != 0){ // meaning that the tid is not the running one, and not in any list
	    print_error("uthread_terminate: tid is not in use", PrintType::THREAD_LIB_ERR);
	    return -1;
        }
    }
    unblock_timer_signal();
    return 0;
}


/**
 * @brief Blocks the thread with ID tid. The thread may be resumed later using uthread_resume.
 *
 * If no thread with ID tid exists it is considered as an error. In addition, it is an error to try blocking the
 * main thread (tid == 0). If a thread blocks itself, a scheduling decision should be made. Blocking a thread in
 * BLOCKED state has no effect and is not considered an error.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_block(int tid){
    return 1;
}


/**
 * @brief Resumes a blocked thread with ID tid and moves it to the READY state.
 *
 * Resuming a thread in a RUNNING or READY state has no effect and is not considered as an error. If no thread with
 * ID tid exists it is considered an error.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_resume(int tid){
    return 1;
}


/**
 * @brief Blocks the RUNNING thread for num_quantums quantums.
 *
 * Immediately after the RUNNING thread transitions to the BLOCKED state a scheduling decision should be made.
 * After the sleeping time is over, the thread should go back to the end of the READY queue.
 * If the thread which was just RUNNING should also be added to the READY queue, or if multiple threads wake up 
 * at the same time, the order in which they're added to the end of the READY queue doesn't matter.
 * The number of quantums refers to the number of times a new quantum starts, regardless of the reason. Specifically,
 * the quantum of the thread which has made the call to uthread_sleep isn’t counted.
 * It is considered an error if the main thread (tid == 0) calls this function.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_sleep(int num_quantums){
    return 1;
}


/**
 * @brief Returns the thread ID of the calling thread.
 *
 * @return The ID of the calling thread.
*/
int uthread_get_tid(){
    return 1;
}


/**
 * @brief Returns the total number of quantums since the library was initialized, including the current quantum.
 *
 * Right after the call to uthread_init, the value should be 1.
 * Each time a new quantum starts, regardless of the reason, this number should be increased by 1.
 *
 * @return The total number of quantums.
*/
int uthread_get_total_quantums(){
    return 1;
}


/**
 * @brief Returns the number of quantums the thread with ID tid was in RUNNING state.
 *
 * On the first time a thread runs, the function should return 1. Every additional quantum that the thread starts should
 * increase this value by 1 (so if the thread with ID tid is in RUNNING state when this function is called, include
 * also the current quantum). If no thread with ID tid exists it is considered an error.
 *
 * @return On success, return the number of quantums of the thread with ID tid. On failure, return -1.
*/
int uthread_get_quantums(int tid){
    return 1;
}
