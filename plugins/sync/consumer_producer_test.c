#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include "sync/consumer_producer.h"

typedef struct {
    consumer_producer_t* queue;
    int thread_id;
    int num_items;
    char** items_produced;
    char** items_consumed;
    int items_produced_count;
    int items_consumed_count;
} thread_data_t;

void* producer_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    
    for (int i = 0; i < data->num_items; i++) {
        char item[100];
        snprintf(item, sizeof(item), "Producer-%d-Item-%d", data->thread_id, i);
        
        printf("Producer %d: Putting item: %s\n", data->thread_id, item);
        
        const char* result = consumer_producer_put(data->queue, item);
        if (result != NULL) {
            printf("Producer %d: Error putting item: %s\n", data->thread_id, result);
            break;
        }
        
        // Store what we produced
        data->items_produced[i] = strdup(item);
        data->items_produced_count++;
        
        usleep(50000); // 50ms delay
    }
    
    printf("Producer %d: Finished\n", data->thread_id);
    return NULL;
}

void* consumer_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    
    for (int i = 0; i < data->num_items; i++) {
        printf("Consumer %d: Getting item...\n", data->thread_id);
        
        char* item = consumer_producer_get(data->queue);
        if (item) {
            printf("Consumer %d: Got item: %s\n", data->thread_id, item);
            
            // Store what we consumed
            data->items_consumed[i] = item; // Take ownership
            data->items_consumed_count++;
        } else {
            printf("Consumer %d: Failed to get item\n", data->thread_id);
            break;
        }
        
        usleep(100000); // 100ms delay (slower than producer)
    }
    
    printf("Consumer %d: Finished\n", data->thread_id);
    return NULL;
}

void test_basic_put_get() {
    printf("\n=== Test 1: Basic Put/Get ===\n");
    
    consumer_producer_t queue;
    const char* result = consumer_producer_init(&queue, 5);
    assert(result == NULL);
    
    // Put some items
    result = consumer_producer_put(&queue, "item1");
    assert(result == NULL);
    
    result = consumer_producer_put(&queue, "item2");
    assert(result == NULL);
    
    // Get items back
    char* item1 = consumer_producer_get(&queue);
    assert(item1 != NULL);
    assert(strcmp(item1, "item1") == 0);
    
    char* item2 = consumer_producer_get(&queue);
    assert(item2 != NULL);
    assert(strcmp(item2, "item2") == 0);
    
    free(item1);
    free(item2);
    
    consumer_producer_destroy(&queue);
    printf("✓ Basic put/get test passed\n");
}

void test_circular_buffer() {
    printf("\n=== Test 2: Circular Buffer ===\n");
    
    consumer_producer_t queue;
    const char* result = consumer_producer_init(&queue, 3);
    assert(result == NULL);
    
    // Fill the queue
    result = consumer_producer_put(&queue, "A");
    assert(result == NULL);
    result = consumer_producer_put(&queue, "B");
    assert(result == NULL);
    result = consumer_producer_put(&queue, "C");
    assert(result == NULL);
    
    // Remove one item
    char* item = consumer_producer_get(&queue);
    assert(strcmp(item, "A") == 0);
    free(item);
    
    // Add another (this should wrap around)
    result = consumer_producer_put(&queue, "D");
    assert(result == NULL);
    
    // Check order
    item = consumer_producer_get(&queue);
    assert(strcmp(item, "B") == 0);
    free(item);
    
    item = consumer_producer_get(&queue);
    assert(strcmp(item, "C") == 0);
    free(item);
    
    item = consumer_producer_get(&queue);
    assert(strcmp(item, "D") == 0);
    free(item);
    
    consumer_producer_destroy(&queue);
    printf("✓ Circular buffer test passed\n");
}

void test_producer_consumer_threads() {
    printf("\n=== Test 3: Producer-Consumer Threads ===\n");
    
    consumer_producer_t queue;
    const char* result = consumer_producer_init(&queue, 5);
    assert(result == NULL);
    
    const int num_items = 10;
    
    // Set up thread data
    thread_data_t producer_data = {0};
    thread_data_t consumer_data = {0};
    
    producer_data.queue = &queue;
    producer_data.thread_id = 1;
    producer_data.num_items = num_items;
    producer_data.items_produced = malloc(num_items * sizeof(char*));
    producer_data.items_produced_count = 0;
    
    consumer_data.queue = &queue;
    consumer_data.thread_id = 1;
    consumer_data.num_items = num_items;
    consumer_data.items_consumed = malloc(num_items * sizeof(char*));
    consumer_data.items_consumed_count = 0;
    
    pthread_t producer_thread_id, consumer_thread_id;
    
    // Start threads
    pthread_create(&producer_thread_id, NULL, producer_thread, &producer_data);
    pthread_create(&consumer_thread_id, NULL, consumer_thread, &consumer_data);
    
    // Wait for completion
    pthread_join(producer_thread_id, NULL);
    pthread_join(consumer_thread_id, NULL);
    
    // Verify all items were produced and consumed
    assert(producer_data.items_produced_count == num_items);
    assert(consumer_data.items_consumed_count == num_items);
    
    // Clean up
    for (int i = 0; i < producer_data.items_produced_count; i++) {
        free(producer_data.items_produced[i]);
    }
    for (int i = 0; i < consumer_data.items_consumed_count; i++) {
        free(consumer_data.items_consumed[i]);
    }
    
    free(producer_data.items_produced);
    free(consumer_data.items_consumed);
    
    consumer_producer_destroy(&queue);
    printf("✓ Producer-consumer threads test passed\n");
}

void* signal_finished_thread(void* arg) {
    consumer_producer_t* q = (consumer_producer_t*)arg;
    sleep(1); // Wait 1 second
    printf("Signaling finished...\n");
    consumer_producer_signal_finished(q);
    return NULL;
}

void test_finished_signal() {
    printf("\n=== Test 4: Finished Signal ===\n");
    
    consumer_producer_t queue;
    const char* result = consumer_producer_init(&queue, 5);
    assert(result == NULL);
    
    // Signal finished from another thread
    pthread_t signal_thread;
    
    pthread_create(&signal_thread, NULL, signal_finished_thread, &queue);
    
    printf("Waiting for finished signal...\n");
    int wait_result = consumer_producer_wait_finished(&queue);
    assert(wait_result == 0);
    
    pthread_join(signal_thread, NULL);
    
    consumer_producer_destroy(&queue);
    printf("✓ Finished signal test passed\n");
}

int main() {
    printf("Starting Consumer-Producer Queue Unit Tests...\n");
    
    test_basic_put_get();
    test_circular_buffer();
    test_producer_consumer_threads();
    test_finished_signal();
    
    printf("\n🎉 All consumer-producer queue tests passed!\n");
    return 0;
}