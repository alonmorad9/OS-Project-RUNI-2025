#include "consumer_producer.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

const char* consumer_producer_init(consumer_producer_t* queue, int capacity) {
    if (!queue || capacity <= 0) {
        return "Invalid queue or capacity";
    }
    
    // Allocate memory for items array and initialize to NULLs
    queue->items = (char**)calloc((size_t)capacity, sizeof(char*));
    if (!queue->items) {
        return "Failed to allocate memory for queue items";
    }
    
    // Initialize queue properties
    queue->capacity = capacity;
    queue->count = 0;
    queue->head = 0;
    queue->tail = 0;
    if (pthread_mutex_init(&queue->mutex, NULL) != 0) {
        free(queue->items);
        return "Failed to initialize queue mutex";
    }
    
    // Initialize monitors
    if (monitor_init(&queue->not_full_monitor) != 0) {
        pthread_mutex_destroy(&queue->mutex);
        free(queue->items);
        return "Failed to initialize not_full_monitor";
    }
    
    if (monitor_init(&queue->not_empty_monitor) != 0) {
        monitor_destroy(&queue->not_full_monitor);
        pthread_mutex_destroy(&queue->mutex);
        free(queue->items);
        return "Failed to initialize not_empty_monitor";
    }
    
    if (monitor_init(&queue->finished_monitor) != 0) {
        monitor_destroy(&queue->not_full_monitor);
        monitor_destroy(&queue->not_empty_monitor);
        pthread_mutex_destroy(&queue->mutex);
        free(queue->items);
        return "Failed to initialize finished_monitor";
    }
    
    // Queue starts as not full (since it's empty)
    monitor_signal(&queue->not_full_monitor);
    
    return NULL; // Success
}

void consumer_producer_destroy(consumer_producer_t* queue) {
    if (!queue) {
        return;
    }
    
    // Free any remaining strings in the queue
    if (queue->items) {
        // Drain and free all items
        for (int i = 0; i < queue->capacity; i++) {
            if (queue->items[i]) {
                free(queue->items[i]);
                queue->items[i] = NULL;
            }
        }
        free(queue->items);
        queue->items = NULL;
    }
    
    // Destroy monitors
    monitor_destroy(&queue->not_full_monitor);
    monitor_destroy(&queue->not_empty_monitor);
    monitor_destroy(&queue->finished_monitor);
    pthread_mutex_destroy(&queue->mutex);
    
    // Reset queue properties
    queue->capacity = 0;
    queue->count = 0;
    queue->head = 0;
    queue->tail = 0;
}

const char* consumer_producer_put(consumer_producer_t* queue, const char* item) {
    if (!queue || !item) {
        return "Invalid queue or item";
    }
    while (1) {
        pthread_mutex_lock(&queue->mutex);
        if (queue->count < queue->capacity) {
            // Create a copy of the string
            char* item_copy = strdup(item);
            if (!item_copy) {
                pthread_mutex_unlock(&queue->mutex);
                return "Failed to allocate memory for item copy";
            }
            // Add item to the queue
            queue->items[queue->tail] = item_copy;
            queue->tail = (queue->tail + 1) % queue->capacity;
            queue->count++;

            // If queue is now full, reset the not_full monitor
            if (queue->count == queue->capacity) {
                monitor_reset(&queue->not_full_monitor);
            }

            // Signal that queue is not empty
            monitor_signal(&queue->not_empty_monitor);
            pthread_mutex_unlock(&queue->mutex);
            return NULL; // Success
        }
        pthread_mutex_unlock(&queue->mutex);

        // Wait until queue is not full
        if (monitor_wait(&queue->not_full_monitor) != 0) {
            return "Failed to wait for not_full condition";
        }
        // loop and recheck
    }
}

char* consumer_producer_get(consumer_producer_t* queue) {
    if (!queue) {
        return NULL;
    }
    while (1) {
        pthread_mutex_lock(&queue->mutex);
        if (queue->count > 0) {
            // Get item from the queue
            char* item = queue->items[queue->head];
            queue->items[queue->head] = NULL; // Clear the slot
            queue->head = (queue->head + 1) % queue->capacity;
            queue->count--;

            // If queue is now empty, reset the not_empty monitor
            if (queue->count == 0) {
                monitor_reset(&queue->not_empty_monitor);
            }

            // Signal that queue is not full
            monitor_signal(&queue->not_full_monitor);
            pthread_mutex_unlock(&queue->mutex);
            return item;
        }
        pthread_mutex_unlock(&queue->mutex);

        // Wait until queue is not empty
        if (monitor_wait(&queue->not_empty_monitor) != 0) {
            return NULL;
        }
        // loop and recheck
    }
}

void consumer_producer_signal_finished(consumer_producer_t* queue) {
    if (!queue) {
        return;
    }
    
    monitor_signal(&queue->finished_monitor);
}

int consumer_producer_wait_finished(consumer_producer_t* queue) {
    if (!queue) {
        return -1;
    }
    
    return monitor_wait(&queue->finished_monitor);
}