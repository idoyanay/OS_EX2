#include "uthreads.h"
#include <iostream>

void dummy_function() {
    std::cout << "Dummy function running." << std::endl;
}

void another_dummy_function() {
    std::cout << "Another dummy function running." << std::endl;
}

int main() {
    // Initialize thread library
    if (uthread_init(1000000) == -1) {
        std::cerr << "uthread_init failed!" << std::endl;
        return 1;
    }
    std::cout << "uthread_init succeeded." << std::endl;

    // Spawn first thread
    int tid1 = uthread_spawn(dummy_function);
    if (tid1 == -1) {
        std::cerr << "uthread_spawn 1 failed!" << std::endl;
        return 1;
    }
    std::cout << "uthread_spawn 1 succeeded. TID: " << tid1 << std::endl;

    // Spawn second thread
    int tid2 = uthread_spawn(another_dummy_function);
    if (tid2 == -1) {
        std::cerr << "uthread_spawn 2 failed!" << std::endl;
        return 1;
    }
    std::cout << "uthread_spawn 2 succeeded. TID: " << tid2 << std::endl;

    // Try to spawn with null pointer - should fail
    if (uthread_spawn(nullptr) != -1) {
        std::cerr << "uthread_spawn with nullptr did NOT fail as expected!" << std::endl;
        return 1;
    } else {
        std::cout << "uthread_spawn with nullptr correctly failed." << std::endl;
    }

    return 0;
}
