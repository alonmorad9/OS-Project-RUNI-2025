#define _POSIX_C_SOURCE 200809L
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

void test_finished_signal() {
    printf("\n=== Test 3: Finished Signal ===\n");
    
    consumer_producer_t queue;
    const char* result = consumer_producer_init(&queue, 5);
    assert(result == NULL);
    
    // Test finished signal directly
    consumer_producer_signal_finished(&queue);
    int wait_result = consumer_producer_wait_finished(&queue);
    assert(wait_result == 0);
    
    consumer_producer_destroy(&queue);
    printf("✓ Finished signal test passed\n");
}

int main() {
    printf("Starting Consumer-Producer Queue Unit Tests...\n");
    
    test_basic_put_get();
    test_circular_buffer();
    test_finished_signal();
    
    printf("\n🎉 All consumer-producer queue tests passed!\n");
    return 0;
}