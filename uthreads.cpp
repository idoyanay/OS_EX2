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
 enum class ThreadState { RUNNING, READY, BLOCKED }; // state of the thread
 
 // struct that contain all the relevant data
 struct Thread { 
     int tid;
     ThreadState state;          // (READY, RUNNING, BLOCKED)
     sigjmp_buf env;             // CPU context (saved)
     char stack[STACK_SIZE];     // Stack memory (only needed for non-main threads)
     int wake_up_quantum;
     int quantom_count;          // number of runnign quantoms for this thread
 };
 
 static struct itimerval timer;                  // timer object for all the threads
 static std::list<Thread*> unblocked_threads;    // double-linkedList for the UNBLOCKED threads. the first one (front) will be the running.
 static std::list<Thread*> blocked_threads;      // double-linkedList for the BLOCKED threads
 static sigset_t sigvtalrm_set;                  // signals-set for storing the ITIMER signal, for blocking when calling a library function
 
 static std::set<int> unused_tid;                // set of unused_tid, so when a new thread is adding when there was already 
                                                 // other thread that had terminated, it will get his value. (note - the set is sorted from min to max)
 
 static int quantum_per_thread;                  // global value (init in the init-function) for the sig-handler to use
 
 static int total_quantums = 0;
 
 
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
 
 
 void start_timer(){
     timer.it_interval.tv_sec = 0;
     timer.it_interval.tv_usec = 0;               // one-shot
     
     timer.it_value.tv_sec = 0;
     timer.it_value.tv_usec = quantum_per_thread;
     if(setitimer(ITIMER_VIRTUAL, &timer, NULL) != 0){ // check if restarting the timer had faild
         print_error("setitimer failed", PrintType::SYSTEM_ERR); // this call will end the run with exit(1)
     }
 }
 
 
 
 void wakeup_sleeping_threads(){
     // Wake up any sleeping threads
     for (auto thread_itr = blocked_threads.begin(); thread_itr != blocked_threads.end(); ) {
         Thread* thread_ptr = *thread_itr;
         if (thread_ptr->wake_up_quantum <= total_quantums) {
             thread_ptr->state = ThreadState::READY;
             unblocked_threads.push_back(thread_ptr);
             blocked_threads.erase(thread_itr);
         } else {
             thread_itr++;
         }
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

void pre_junmping(){
    total_quantums++;
    unblocked_threads.front()->quantom_count++;
    wakeup_sleeping_threads();
    start_timer();
}
 

void end_of_quantum(int sig){
    // increase global variable as another thread about to run
    // std::cout<<"detected end of quantom for thread "<< unblocked_threads.front()->tid << std::endl;

    //wakeup any sleeping thread
    
    Thread *prev_run = unblocked_threads.front();
    if (unblocked_threads.size() > 1){ // if there is another ready thread
        unblocked_threads.front()->state = ThreadState::READY; // updating the running thread
        unblocked_threads.push_back(unblocked_threads.front()); // pushing the thread to the end of the list
        unblocked_threads.pop_front(); // removing the thread from the list
        unblocked_threads.front()->state = ThreadState::RUNNING; // updating the runnign thread
    }

    
    if (sigsetjmp(prev_run->env, 1) == 0){
        pre_junmping();
        // std::cout<<"jumping to "<<unblocked_threads.front()->tid << std::endl;
        siglongjmp(unblocked_threads.front()->env, 1); // jumping to the thread's context
        return;
    }
    // std::cout<<"return to thread "<<unblocked_threads.front()->tid << std::endl;
    return;

}


int uthread_init(int quantum_usecs) 
 {
     /**
      * Function flow: checking input, init sigset and quantum-global, create main thread, updaiting sig-hangler, updaiting itimer.
      */
     if (quantum_usecs <= 0) { 
         print_error("uthread_init: quantum_usecs must be positive", PrintType::THREAD_LIB_ERR);
         return -1;
     }
 
     init_itimer_sigset(); // init the sigset for later blocking and unblocking the itimer-signal
     for (int i = 1; i < 100; ++i) { // init the unuset_tid (like a basket of all the 'free-tid' numbers). the 0 tid is already using
        unused_tid.insert(i);
     }
     quantum_per_thread = quantum_usecs; // updaiting for the sig-handler to use
     unblocked_threads.push_front(new Thread{0, ThreadState::RUNNING, {}, {}, 0, 0}); // initializing main thread
     if(sigsetjmp(unblocked_threads.front()->env, 1) == 0){ // Save current CPU context // TODO - this line needs checking. maybe needs to setjmp later.
 
         // create and update the sig-handler
         struct sigaction sa = {0};
         sa.sa_handler = &end_of_quantum;
         if (sigaction(SIGVTALRM, &sa, NULL) < 0)
         {
             print_error("uthread_init: sigaction failed", PrintType::SYSTEM_ERR); // TODO: make sure this is the type of error
         }
 
         pre_junmping();
     }
     return 0;
  }
 
 
 int uthread_spawn(thread_entry_point entry_point){
     /**
      * Function flow: block itimer-signal, checking MAX_THREADS and input, updaiting new tid, create and update the new thread, unblock itimer-signal
      */
     block_timer_signal();
    //  std::cout<< "enter uthread_spawn" <<std::endl;
 
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

     Thread *new_thread = new Thread{tid, ThreadState::READY, {}, {}, 0, 0}; // create new thread
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
    //  std::cout<<"uthread_terminate("<<tid<<")"<<std::endl;
     if(tid == 0){
         terminate_program();
     }
     
     if(tid == unblocked_threads.front()->tid){
         // -- change the runnign thread to the next ready -- //
         unused_tid.insert(unblocked_threads.front()->tid); // adding the tid of the terminated thread to the unused.
         delete unblocked_threads.front();
         unblocked_threads.pop_front(); // it is gurenteed (writen in the forum) that the main thread will not be blocked. so, if tid != 0 and we got here then the list.size>2.
         unblocked_threads.front()->state = ThreadState::RUNNING;
         
         // -- update teh total quantums, wake up sleeping threads, and start the timer for the new running thread.
         pre_junmping();
         unblock_timer_signal();
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
     block_timer_signal();
    //  std::cout<<"uthread_block("<<tid<<")"<<std::endl;
     
     // print all ready threads:
    //  for(auto thread : unblocked_threads){
        // std::cout<<"uthread_block: unblocked_threads tid = "<<thread->tid<<std::endl;
    //  }
     int succses = 0;
 
     bool unvalid_tid = unused_tid.find(tid) != unused_tid.end() || tid > 99 || tid <= 0;
     if( unvalid_tid){
         print_error("uthread_block: unvalid tid", PrintType::THREAD_LIB_ERR);
         succses = -1;
     }
     
 
     else if(unblocked_threads.front()->tid == tid){
        //  std::cout << "uthread_block: tid = " << tid << " is the running thread" << std::endl; // TODO - remove this line
         Thread* thread_ptr = unblocked_threads.front();         
         thread_ptr->state = ThreadState::BLOCKED; 
         blocked_threads.push_back(thread_ptr);
         unblocked_threads.pop_front();
         unblocked_threads.front()->state = ThreadState::RUNNING;
         if(sigsetjmp(thread_ptr->env, 1) == 0){
             pre_junmping();
             unblock_timer_signal(); // TODO - check if when we unblock -unblocked signals its OK
            //  std::cout<< "jumping to next thread" << std::endl; // TODO - remove this line
             siglongjmp(unblocked_threads.front()->env, 1);
         }
     }
 
     else{ // meaning, if the wanted thread is valid and not the running one, need to find it in the unblocked list or do nothing
         auto thread_itr = find_thread_in_list(unblocked_threads, tid); 
         if(thread_itr != unblocked_threads.end()){  // if thread not block
             Thread* thread_ptr = *thread_itr;         
             thread_ptr->state = ThreadState::BLOCKED;
             unblocked_threads.erase(thread_itr);
             blocked_threads.push_back(thread_ptr);
         }
     }
 
     unblock_timer_signal();
     return succses;
     }
     
     
 
 int uthread_resume(int tid){
     block_timer_signal();
     //check for unvalid index
     bool unvalid_tid = unused_tid.find(tid) != unused_tid.end() || tid > 99 || tid < 0;
     if(unvalid_tid){
         print_error("uthread_resume: unvalid tid", PrintType::THREAD_LIB_ERR);
         return -1;
     }
     
     auto thread_itr = find_thread_in_list(blocked_threads, tid); 
     if(thread_itr != blocked_threads.end()){
         Thread* thread_ptr = *thread_itr;         // Pull the object
         thread_ptr->state = ThreadState::READY;
         blocked_threads.erase(thread_itr);
         unblocked_threads.push_back(thread_ptr);
     }
     unblock_timer_signal();
     return 0;
 }
 
 int uthread_sleep(int num_quantums){
     block_timer_signal();
     if(unblocked_threads.front()->tid == 0){
         print_error("uthread_sleep: trying to put main thread to sleep", PrintType::THREAD_LIB_ERR);
         return -1;
     }
     // set wake up quantum
     Thread *prev_running = unblocked_threads.front();
     prev_running-> wake_up_quantum = total_quantums + num_quantums - 1;
     //Blok running thread if its not the main one
     prev_running->state = ThreadState::BLOCKED;
     blocked_threads.push_back(prev_running);
     unblocked_threads.pop_front(); 
     unblocked_threads.front()->state = ThreadState::RUNNING;
     if(sigsetjmp(prev_running->env, 1) == 0){ 
         pre_junmping();
         unblock_timer_signal();
         siglongjmp(unblocked_threads.front()->env, 1); // jumping to the thread's context
     }
 
 
     unblock_timer_signal();
     return 0;
}
 
 int uthread_get_tid(){
         return unblocked_threads.front()->tid;
 }
     
 int uthread_get_total_quantums(){
         return total_quantums;
 }
      
 int uthread_get_quantums(int tid){
    block_timer_signal();
    bool unvalid_tid = unused_tid.find(tid) != unused_tid.end() || tid > 99 || tid < 0;
     int ret_val;
     if(unvalid_tid){
         print_error("uthread_get_quantums: unvalid tid " + std::to_string(tid), PrintType::THREAD_LIB_ERR);
         ret_val = -1;
     }
     
     auto unblocked_itr = find_thread_in_list(unblocked_threads, tid);
     if(unblocked_itr != unblocked_threads.end()){
         ret_val = (*unblocked_itr) -> quantom_count;
     }
     else{
         auto blocked_itr = find_thread_in_list(blocked_threads, tid);
         ret_val = (*blocked_itr) -> quantom_count;
     }
     unblock_timer_signal();
     return ret_val;
     
 }
 