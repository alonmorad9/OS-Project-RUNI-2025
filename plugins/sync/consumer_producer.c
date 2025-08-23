#include "consumer_producer.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

const char* consumer_producer_init(consumer_producer_t* queue, int capacity) { // Initialize the queue
    if (!queue || capacity <= 0) {
        return "Invalid queue or capacity";
    }
    
    // allocate memory for items array and initialize to NULLs
    queue->items = (char**)calloc((size_t)capacity, sizeof(char*));
    if (!queue->items) {
        return "Failed to allocate memory for queue items";
    }
    
    // initialize queue properties
    queue->capacity = capacity;
    queue->count = 0;
    queue->head = 0;
    queue->tail = 0;
    if (pthread_mutex_init(&queue->mutex, NULL) != 0) {
        free(queue->items);
        return "Failed to initialize queue mutex";
    }

    // initialize monitors
    if (monitor_init(&queue->not_full_monitor) != 0) {
        pthread_mutex_destroy(&queue->mutex);
        free(queue->items);
        return "Failed to initialize not_full_monitor";
    }

    // initialize not_empty_monitor
    if (monitor_init(&queue->not_empty_monitor) != 0) {
        monitor_destroy(&queue->not_full_monitor);
        pthread_mutex_destroy(&queue->mutex);
        free(queue->items);
        return "Failed to initialize not_empty_monitor";
    }

    // initialize finished_monitor
    if (monitor_init(&queue->finished_monitor) != 0) {
        monitor_destroy(&queue->not_full_monitor);
        monitor_destroy(&queue->not_empty_monitor);
        pthread_mutex_destroy(&queue->mutex);
        free(queue->items);
        return "Failed to initialize finished_monitor";
    }
    
    // queue starts as not full (since it's empty), not empty is false initially
    monitor_signal(&queue->not_full_monitor);

    return NULL; // success
}

void consumer_producer_destroy(consumer_producer_t* queue) { // destroy the queue
    if (!queue) {
        return;
    }
    
    // free any remaining strings in the queue
    if (queue->items) {
        // drain and free all items
        for (int i = 0; i < queue->capacity; i++) {
            if (queue->items[i]) {
                free(queue->items[i]);
                queue->items[i] = NULL;
            }
        }
        free(queue->items);
        queue->items = NULL;
    }
    
    // destroy monitors
    monitor_destroy(&queue->not_full_monitor);
    monitor_destroy(&queue->not_empty_monitor);
    monitor_destroy(&queue->finished_monitor);
    pthread_mutex_destroy(&queue->mutex);
    
    // reset queue properties
    queue->capacity = 0;
    queue->count = 0;
    queue->head = 0;
    queue->tail = 0;
}

const char* consumer_producer_put(consumer_producer_t* queue, const char* item) { // put item into the queue
    if (!queue || !item) {
        return "Invalid queue or item";
    }
    
    while (1) { // loop until item is added
        pthread_mutex_lock(&queue->mutex);
        
        if (queue->count < queue->capacity) {
            // create a copy of the string
            char* item_copy = strdup(item);
            if (!item_copy) {
                pthread_mutex_unlock(&queue->mutex);
                return "Failed to allocate memory for item copy";
            }
            
            // add item to the queue
            queue->items[queue->tail] = item_copy;
            queue->tail = (queue->tail + 1) % queue->capacity;
            queue->count++;

            // signal that queue is not empty (now has items)
            monitor_signal(&queue->not_empty_monitor);
            
            // check if queue is now full after adding this item
            int is_now_full = (queue->count == queue->capacity);
            
            pthread_mutex_unlock(&queue->mutex);
            
            // if queue became full, reset the not_full_monitor so producers will wait
            if (is_now_full) {
                monitor_reset(&queue->not_full_monitor);
            }
            
            return NULL; // success
        }
        
        pthread_mutex_unlock(&queue->mutex);

        // wait until queue is not full
        if (monitor_wait(&queue->not_full_monitor) != 0) {
            return "Failed to wait for not_full condition";
        }
        // loop and recheck
    }
}

char* consumer_producer_get(consumer_producer_t* queue) { // get item from the queue
    if (!queue) {
        return NULL;
    }
    
    while (1) { // loop until item is retrieved
        pthread_mutex_lock(&queue->mutex);
        
        if (queue->count > 0) {
            // get item from the queue
            char* item = queue->items[queue->head];
            queue->items[queue->head] = NULL; // clear the slot
            queue->head = (queue->head + 1) % queue->capacity;
            queue->count--;

            // signal that queue is not full (now has space)
            monitor_signal(&queue->not_full_monitor);
            
            // check if queue is now empty after removing this item
            int is_now_empty = (queue->count == 0);
            
            pthread_mutex_unlock(&queue->mutex);
            
            // if queue became empty, reset the not_empty_monitor so consumers will wait
            if (is_now_empty) {
                monitor_reset(&queue->not_empty_monitor);
            }
            
            return item;
        }
        
        pthread_mutex_unlock(&queue->mutex);

        // wait until queue is not empty
        if (monitor_wait(&queue->not_empty_monitor) != 0) {
            return NULL;
        }
        // loop and recheck
    }
}

void consumer_producer_signal_finished(consumer_producer_t* queue) { // signal finished
    if (!queue) {
        return;
    }

    monitor_signal(&queue->finished_monitor); // signal that processing is finished
}

int consumer_producer_wait_finished(consumer_producer_t* queue) { // wait for finished
    if (!queue) {
        return -1;
    }

    return monitor_wait(&queue->finished_monitor); // wait for processing to finish
}