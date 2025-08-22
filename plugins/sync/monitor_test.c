#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <sched.h>
#include "sync/monitor.h"

// test data structure
typedef struct { // thread data structure
    monitor_t* monitor;
    int* shared_data;
    int thread_id;
} test_data_t;

void* test_thread_wait_then_increment(void* arg) { // thread waiting and incrementing
    test_data_t* data = (test_data_t*)arg;
    
    printf("Thread %d: Waiting for signal...\n", data->thread_id);
    
    // wait for signal
    if (monitor_wait(data->monitor) == 0) {
        printf("Thread %d: Received signal, incrementing counter\n", data->thread_id);
        (*data->shared_data)++;
    }
    
    return NULL;
}

void* test_thread_signal_after_delay(void* arg) { // thread signaling after delay
    test_data_t* data = (test_data_t*)arg;
    
    printf("Signal thread: Sleeping for 1 second...\n");
    sleep(1);
    
    printf("Signal thread: Sending signal\n");
    monitor_signal(data->monitor);
    
    return NULL;
}

void test_basic_functionality() { // test basic functionality 
    printf("\n=== Test 1: Basic Monitor Functionality ===\n");
    
    monitor_t monitor;
    assert(monitor_init(&monitor) == 0);
    
    int data = 0;
    test_data_t thread_data = {&monitor, &data, 1}; // thread data structure

    pthread_t waiter_thread, signaler_thread; // thread identifiers

    // create thread that waits
    pthread_create(&waiter_thread, NULL, test_thread_wait_then_increment, &thread_data);

    // create thread that signals after delay
    pthread_create(&signaler_thread, NULL, test_thread_signal_after_delay, &thread_data);

    // wait for both threads
    pthread_join(waiter_thread, NULL);
    pthread_join(signaler_thread, NULL);
    
    assert(data == 1);
    printf("Basic functionality test passed\n");

    monitor_destroy(&monitor); // destroy monitor
}

void test_signal_before_wait() { // test signal before wait
    printf("\n=== Test 2: Signal Before Wait (Race Condition Test) ===\n");
    
    monitor_t monitor;
    assert(monitor_init(&monitor) == 0);
    
    // signal first
    printf("Sending signal before any thread waits...\n");
    monitor_signal(&monitor);

    // now wait - should return immediately because signal was remembered
    printf("Now waiting for signal...\n");
    int result = monitor_wait(&monitor);
    
    assert(result == 0);
    printf("Signal before wait test passed\n");
    
    monitor_destroy(&monitor);
}

void test_multiple_waiters() { // test multiple waiters
    printf("\n=== Test 3: Multiple Waiters ===\n");
    
    monitor_t monitor;
    assert(monitor_init(&monitor) == 0);
    
    int counter = 0;
    const int num_threads = 3;
    pthread_t threads[num_threads];
    test_data_t thread_data[num_threads];
    
    // create multiple waiting threads
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].monitor = &monitor;
        thread_data[i].shared_data = &counter;
        thread_data[i].thread_id = i + 1;
        
        pthread_create(&threads[i], NULL, test_thread_wait_then_increment, &thread_data[i]);
    }
    
    // give threads time to start waiting
    usleep(100000); // 100ms

    // signal once - should wake all threads (using broadcast)
    printf("Sending signal to wake all threads...\n");
    monitor_signal(&monitor);

    // wait for all threads
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    assert(counter == num_threads); // all threads should have incremented the counter
    printf("Multiple waiters test passed (counter = %d)\n", counter);

    monitor_destroy(&monitor);
}

void test_reset_functionality() { // test reset functionality
    printf("\n=== Test 4: Reset Functionality ===\n");
    
    monitor_t monitor;
    assert(monitor_init(&monitor) == 0);
    
    // signal the monitor
    monitor_signal(&monitor);
    
    // reset it
    monitor_reset(&monitor);
    
    // now create a thread that waits - it should block because we reset
    int data = 0;
    test_data_t thread_data = {&monitor, &data, 1};
    
    pthread_t waiter_thread, signaler_thread;
    
    pthread_create(&waiter_thread, NULL, test_thread_wait_then_increment, &thread_data);
    
    // give the waiter time to start waiting
    usleep(100000);

    // now signal again
    pthread_create(&signaler_thread, NULL, test_thread_signal_after_delay, &thread_data);
    
    pthread_join(waiter_thread, NULL);
    pthread_join(signaler_thread, NULL);
    
    assert(data == 1);
    printf("Reset functionality test passed\n");
    
    monitor_destroy(&monitor);
}

int main() { // main function
    printf("Starting Monitor Unit Tests...\n");
    
    test_basic_functionality();
    test_signal_before_wait();
    test_multiple_waiters();
    test_reset_functionality();
    
    printf("\n All monitor tests passed!\n");
    return 0;
}