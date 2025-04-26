/**
 * Implementation of the uthreads library.
 * Authors: Ido Yanay, Omri Baum.
 */
#include "uthreads.h"

#include <stdio.h>
#include <stdlib.h>


// --- enums, structs, and typedef for the internal use in the libraray --- //
typedef enum print_type {SYSTEM_ERR, THREAD_LIB_ERR} print_type;
typedef enum thread_state {RUNNING, READY, WAITING} thread_state;
// ------------------------------------------------------------------------- //

void print_error(const char* msg, print_type type) {
    if (type == SYSTEM_ERR) {
        fprintf(stderr, "system error: %s\n", msg);
        exit(1); // Immediately exit with failure
    } else if (type == THREAD_LIB_ERR) {
        fprintf(stderr, "thread library error: %s\n", msg);
        // Do NOT exit — the calling function should handle the return value
    }
}

/**
 * @brief initializes the thread library.
 *
 * Once this function returns, the main thread (tid == 0) will be set as RUNNING. There is no need to 
 * provide an entry_point or to create a stack for the main thread - it will be using the "regular" stack and PC.
 * You may assume that this function is called before any other thread library function, and that it is called
 * exactly once.
 * The input to the function is the length of a quantum in micro-seconds.
 * It is an error to call this function with non-positive quantum_usecs.
 *
 * @return On success, return 0. On failure, return -1.
*/

int uthread_init(int quantum_usecs){
    if(quantum_usecs <= 0){
        print_error("uthread_init: quantum_usecs must be positive", THREAD_LIB_ERR);
        return -1;
    }
    
}
