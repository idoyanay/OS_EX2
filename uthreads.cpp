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
 
 // struct that contain all the relevant data
 struct Thread { 
     int tid;
     sigjmp_buf env;             // CPU context (saved)
     char stack[STACK_SIZE];     // Stack memory (only needed for non-main threads)
     int wake_up_quantum;        // the 'time' for a sleeping thread to wake up
     int quantom_count;          // number of runnign quantoms for this thread
 };
 
 static struct itimerval timer;                  // timer object for all the threads
 static std::list<Thread*> unblocked_threads;    // double-linkedList for the UNBLOCKED threads. the first one (front) will be the running.
 static std::list<Thread*> blocked_threads;      // double-linkedList for the BLOCKED threads
 static sigset_t sigvtalrm_set;                  // signals-set for storing the ITIMER signal, for blocking when calling a library function
 
 static std::set<int> unused_tid;                // set of unused_tid, so when a new thread is adding when there was already 
                                                 // other thread that had terminated, it will get his value. (note - the set is sorted from min to max)
 
 static int quantum_per_thread;                  // global value (init in the init-function) for the sig-handler to use

 static int total_quantums = 0;                  // the total quantums that had been passed since uthreads_init
 
 
  // ------------------------------------------------------------------------- //
  
 
 
 
void print_error(const std::string& msg, PrintType type) 
{
    // function for printing the error that the pdf specified. use this for generic and not doing typing errors.
    if (type == PrintType::SYSTEM_ERR) {
        std::cerr << "system error: " << msg << std::endl;
        std::exit(1); // Immediately exit with failure
    } else if (type == PrintType::THREAD_LIB_ERR) {
        std::cerr << "thread library error: " << msg << std::endl;
        // Do NOT exit — the calling function should handle the return value
    }
}
 
 
void start_timer()
{
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 0;               // config the timer for one shot
    
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = quantum_per_thread;
    if(setitimer(ITIMER_VIRTUAL, &timer, NULL) != 0){ // check if restarting the timer had faild
        print_error("setitimer failed", PrintType::SYSTEM_ERR); // this call will end the run with exit(1)
    }
}
 
 

void wakeup_sleeping_threads()
{
    // Wake up any sleeping threads
    std::cout<<"enter wakeup_sleeping_threads"<<std::endl;
    for (auto thread_itr = blocked_threads.begin(); thread_itr != blocked_threads.end(); ) {
        if ((*thread_itr)->wake_up_quantum <= total_quantums && (*thread_itr)->wake_up_quantum != 0) {
            Thread* thread_ptr = *thread_itr;
            unblocked_threads.push_back(thread_ptr);
            blocked_threads.erase(thread_itr);
        }
        thread_itr++;
    }
    std::cout<<"exit wakeup_sleeping_threads"<<std::endl;
}


 
address_t translate_address(address_t addr)
{
    // 'black-box' given in the examples
    address_t ret;
    asm volatile("xor    %%fs:0x30,%0\n"
        "rol    $0x11,%0\n"
                : "=g" (ret)
                : "0" (addr));
    return ret;
}
 
void setup_thread(char* stack, thread_entry_point entry_point, sigjmp_buf& env)
{
    // setup thread like in the example
    address_t sp = (address_t) stack + STACK_SIZE - sizeof(address_t);
    address_t pc = (address_t) entry_point;
    sigsetjmp(env, 1);
    (env->__jmpbuf)[JB_SP] = translate_address(sp);
    (env->__jmpbuf)[JB_PC] = translate_address(pc);
    sigemptyset(&env->__saved_mask);
}
  
 
 void init_itimer_sigset()
 {
     // initialize the sigset that will used for blocking the itimer signal when entering library functions.
     if (sigemptyset(&sigvtalrm_set) < 0) {
         print_error("sigemptyset failed", PrintType::SYSTEM_ERR);
     }
     if (sigaddset(&sigvtalrm_set, SIGVTALRM) < 0) {
         print_error("sigaddset failed", PrintType::SYSTEM_ERR);
     }
 }
 
void block_timer_signal()
{
    // blocking itimer signal for allowing the current thread to use the library function and not getting undefined behavior
    if (sigprocmask(SIG_BLOCK, &sigvtalrm_set, nullptr) < 0) {
        print_error("sigprocmask block failed", PrintType::SYSTEM_ERR);
    }
}

void unblock_timer_signal()
{
    // unblocking (only used after blocking)
    if (sigprocmask(SIG_UNBLOCK, &sigvtalrm_set, nullptr) < 0) {
        print_error("sigprocmask unblock failed", PrintType::SYSTEM_ERR);
    }
}

void pre_jumping() 
{
    // putting together all the mendatory action before jumping to a new thread
    std::cout<<"enter pre jumping "<<std::endl;
    total_quantums++;
    unblocked_threads.front()->quantom_count++;
    wakeup_sleeping_threads();
    start_timer();
    std::cout<<"exit pre jumping "<<std::endl;
}
 

void end_of_quantum(int sig){    
    wakeup_sleeping_threads();

    Thread *prev_run = unblocked_threads.front();
    if (unblocked_threads.size() > 1){ // if there is another ready thread
        unblocked_threads.push_back(unblocked_threads.front()); // pushing the thread to the end of the list
        unblocked_threads.pop_front(); // removing the thread from the list
    }

    if (sigsetjmp(prev_run->env, 1) == 0){
        total_quantums++;
        unblocked_threads.front()->quantom_count++;
        start_timer();
        siglongjmp(unblocked_threads.front()->env, 1); // jumping to the thread's context
    }
    return;
}


int uthread_init(int quantum_usecs) 
{
    // Function flow: checking input, init sigset and quantum-global, create main thread, updaiting sig-hangler, updaiting itimer.
    if (quantum_usecs <= 0) { 
        print_error("uthread_init: quantum_usecs must be positive", PrintType::THREAD_LIB_ERR);
        return -1;
    }

    init_itimer_sigset(); // init the sigset for later blocking and unblocking the itimer-signal
    for (int i = 1; i < MAX_THREAD_NUM; ++i) { // init the unuset_tid (like a basket of all the 'free-tid' numbers). the 0 tid is already using
        unused_tid.insert(i);
    }
    quantum_per_thread = quantum_usecs; // updaiting for the sig-handler to use
    unblocked_threads.push_front(new Thread{0, {}, {}, 0, 0}); // initializing main thread
    if(sigsetjmp(unblocked_threads.front()->env, 1) == 0){ // Save current CPU context // TODO - this line needs checking. maybe needs to setjmp later.

        // create and update the sig-handler
        struct sigaction sa = {0};
        sa.sa_handler = &end_of_quantum;
        if (sigaction(SIGVTALRM, &sa, NULL) < 0)
        {
            print_error("uthread_init: sigaction failed", PrintType::SYSTEM_ERR); // TODO: make sure this is the type of error
        }
        pre_jumping();
    }
    return 0;
}
 
 
int uthread_spawn(thread_entry_point entry_point){
    // Function flow: block itimer-signal, checking MAX_THREADS and input, updaiting new tid, create and update the new thread, unblock itimer-signal
    block_timer_signal();

    int num_threads = unblocked_threads.size() + blocked_threads.size();
    if(num_threads >= MAX_THREAD_NUM) { // check if the number of threads is already at the maximum 
        print_error("uthread_spawn: reached maximum number of threads", PrintType::THREAD_LIB_ERR);
        return -1;
    }
    else if(!entry_point){ // check if entry_point is null
        print_error("uthread_spawn: entry_point is null", PrintType::THREAD_LIB_ERR);
        return -1;
    }
    
    int tid = *unused_tid.begin(); // get the smallest TID
    unused_tid.erase(unused_tid.begin()); // remove it from the set

    Thread *new_thread = new Thread{tid, {}, {}, 0, 0}; // create new thread
    setup_thread(new_thread->stack, entry_point, new_thread->env); // setup the new thread
    unblocked_threads.push_back(new_thread); // add the new thread to the ready threads list
    
    unblock_timer_signal();
    return new_thread->tid;
}

void terminate_program(){
    // terminate the program when terminte function called with tid==0. deleting all the Threads, because they are on the heap.
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
    // find thread based on tid. on success, return the iterator that points to the thread. on fail return lst.end().
    for (auto it = lst.begin(); it != lst.end(); ++it) {
        if ((*it)->tid == wanted_tid) {
            return it;  // found
        }
    }
    return lst.end();  // not found
}
 
int delete_from_list(std::list<Thread*>& lst, int tid){
    //delete the wanted thread based on the tid, from the lst. return 0 if succseeded (the thread in the lst) and -1 on fail.
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

    // Function flow: check if tid==0 for terminating the whole program. checking if tid is the tid of the running thread (requare more updates).
    //                   trying to delte the thread fits to the tid from the lists of theads. if not succseeded, means that the tid is not valid.

    block_timer_signal();
    if(tid == 0){
        terminate_program();
    }
    
    if(tid == unblocked_threads.front()->tid){
        // -- change the runnign thread to the next ready -- //
        unused_tid.insert(unblocked_threads.front()->tid); // adding the tid of the terminated thread to the unused.
        delete unblocked_threads.front();
        unblocked_threads.pop_front(); // it is gurenteed (writen in the forum) that the main thread will not be blocked. so, if tid != 0 and we got here then the list.size>2.
        
        // -- update teh total quantums, wake up sleeping threads, and start the timer for the new running thread.
        pre_jumping();
        unblock_timer_signal();
        siglongjmp(unblocked_threads.front()->env, 1); // the function not return, moving to the next thread.
    }
    // if got here, the tid either belong to ready or blocked thread, or not to any thread
    if(delete_from_list(unblocked_threads, tid) != 0){
        if(delete_from_list(blocked_threads, tid) != 0){ // meaning that the tid is not the running one, and not in any list
            print_error("uthread_terminate: tid is not in use", PrintType::THREAD_LIB_ERR);
            return -1;
        }
    }
    unblock_timer_signal();
    return 0;
}
 

int uthread_block(int tid){
    block_timer_signal();
    int ret_val = 0;
    bool unvalid_tid = unused_tid.find(tid) != unused_tid.end() || tid >= MAX_THREAD_NUM || tid <= 0;
    if( unvalid_tid){
        print_error("uthread_block: unvalid tid", PrintType::THREAD_LIB_ERR);
        ret_val = -1;
    }
    

    else if(unblocked_threads.front()->tid == tid){
        Thread* thread_ptr = unblocked_threads.front();
        blocked_threads.push_back(thread_ptr); // move to the blocked list
        unblocked_threads.pop_front();          // remove from the ready/running list
        if(sigsetjmp(thread_ptr->env, 1) == 0){
            pre_jumping();
            unblock_timer_signal();
            siglongjmp(unblocked_threads.front()->env, 1);
        }
    }
    else{ // meaning, if the wanted thread is valid and not the running one, need to find it in the unblocked list or do nothing
        auto thread_itr = find_thread_in_list(unblocked_threads, tid); 
        if(thread_itr != unblocked_threads.end()){  // if thread not block
            Thread* thread_ptr = *thread_itr;         
            unblocked_threads.erase(thread_itr); // remove from the ready/running list
            blocked_threads.push_back(thread_ptr);  // move to the blocked list
        }
    }
    unblock_timer_signal();
    return ret_val;
}
    
     
 
int uthread_resume(int tid){
    block_timer_signal();
    //check for unvalid tid
    bool unvalid_tid = unused_tid.find(tid) != unused_tid.end() || tid >= MAX_THREAD_NUM || tid < 0;
    if(unvalid_tid){
        print_error("uthread_resume: unvalid tid", PrintType::THREAD_LIB_ERR);
        return -1;
    }
    
    auto thread_itr = find_thread_in_list(blocked_threads, tid); 
    if(thread_itr != blocked_threads.end()){
        Thread* thread_ptr = *thread_itr;         // get the pointer
        blocked_threads.erase(thread_itr);        // remove from the blocked list
        unblocked_threads.push_back(thread_ptr);  // insert at the back of the ready list
    }
    unblock_timer_signal();
    return 0;
}
int uthread_sleep(int num_quantums){
    block_timer_signal(); // Block the timer signal to prevent interruptions.
    std::cout<<"enter uthread sleep with tid "<< unblocked_threads.front()->tid<<" for "<<num_quantums<<" quantums"<<std::endl;
    if(unblocked_threads.front()->tid == 0){ // Ensure the main thread is not trying to sleep.
        print_error("uthread_sleep: trying to put main thread to sleep", PrintType::THREAD_LIB_ERR);
        return -1;
    }
    Thread *prev_running = unblocked_threads.front();
    prev_running-> wake_up_quantum = total_quantums + num_quantums - 1; // Set the wake-up quantum for the thread.
    std::cout<<"set wake up quantum to "<<prev_running->wake_up_quantum<<" for thread "<<prev_running->tid<<std::endl;
    blocked_threads.push_back(prev_running); // Move the running thread to the blocked list.
    unblocked_threads.pop_front(); // Remove the thread from the unblocked list.
    if(sigsetjmp(prev_running->env, 1) == 0){ // Save the current thread's context.
        std::cout<<"jumping to thread "<<unblocked_threads.front()->tid<<std::endl;
        pre_jumping(); // Perform actions before switching threads.
        unblock_timer_signal(); // Unblock the timer signal before switching.
        siglongjmp(unblocked_threads.front()->env, 1); // Switch to the next thread's context.
    }
    unblock_timer_signal(); // Unblock the timer signal after execution.
    return 0;
}
 
int uthread_get_tid(){
    return unblocked_threads.front()->tid; // Return the ID of the currently running thread.
}
    
int uthread_get_total_quantums(){
    return total_quantums; // Return the total number of quantums since initialization.
}
    
int uthread_get_quantums(int tid){
    block_timer_signal(); // Block the timer signal to prevent interruptions.
    bool unvalid_tid = unused_tid.find(tid) != unused_tid.end() || tid >= MAX_THREAD_NUM || tid < 0; // Check if the tid is invalid.
    int ret_val;
    if(unvalid_tid){
        print_error("uthread_get_quantums: unvalid tid " + std::to_string(tid), PrintType::THREAD_LIB_ERR);
        ret_val = -1;
    }
    auto unblocked_itr = find_thread_in_list(unblocked_threads, tid);
    if(unblocked_itr != unblocked_threads.end()){
        ret_val = (*unblocked_itr) -> quantom_count; // Get the quantum count for the thread in the unblocked list.
    }
    else{
        auto blocked_itr = find_thread_in_list(blocked_threads, tid);
        ret_val = (*blocked_itr) -> quantom_count; // Get the quantum count for the thread in the blocked list.
    }
    unblock_timer_signal(); // Unblock the timer signal after execution.
    return ret_val;
}