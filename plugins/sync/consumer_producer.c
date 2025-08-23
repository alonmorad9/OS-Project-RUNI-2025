#include "consumer_producer.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

const char* consumer_producer_init(consumer_producer_t* queue, int capacity) {
    if (!queue || capacity <= 0) {
        return "Invalid queue or capacity";
    }
    
    queue->items = (char**)calloc((size_t)capacity, sizeof(char*));
    if (!queue->items) {
        return "Failed to allocate memory for queue items";
    }
    
    queue->capacity = capacity;
    queue->count = 0;
    queue->head = 0;
    queue->tail = 0;
    
    if (pthread_mutex_init(&queue->mutex, NULL) != 0) {
        free(queue->items);
        return "Failed to initialize queue mutex";
    }

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
    
    // start with queue not full
    monitor_signal(&queue->not_full_monitor);

    return NULL;
}

void consumer_producer_destroy(consumer_producer_t* queue) {
    if (!queue) {
        return;
    }
    
    if (queue->items) {
        for (int i = 0; i < queue->capacity; i++) {
            if (queue->items[i]) {
                free(queue->items[i]);
                queue->items[i] = NULL;
            }
        }
        free(queue->items);
        queue->items = NULL;
    }
    
    monitor_destroy(&queue->not_full_monitor);
    monitor_destroy(&queue->not_empty_monitor);
    monitor_destroy(&queue->finished_monitor);
    pthread_mutex_destroy(&queue->mutex);
    
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
            char* item_copy = strdup(item);
            if (!item_copy) {
                pthread_mutex_unlock(&queue->mutex);
                return "Failed to allocate memory for item copy";
            }
            
            queue->items[queue->tail] = item_copy;
            queue->tail = (queue->tail + 1) % queue->capacity;
            queue->count++;

            monitor_signal(&queue->not_empty_monitor);
            
            int was_full_before = (queue->count == queue->capacity);
            
            pthread_mutex_unlock(&queue->mutex);
            
            if (was_full_before) {
                monitor_reset(&queue->not_full_monitor);
            }
            
            return NULL;
        }
        
        pthread_mutex_unlock(&queue->mutex);

        if (monitor_wait(&queue->not_full_monitor) != 0) {
            return "Failed to wait for not_full condition";
        }
    }
}

char* consumer_producer_get(consumer_producer_t* queue) {
    if (!queue) {
        return NULL;
    }
    
    while (1) {
        pthread_mutex_lock(&queue->mutex);
        
        if (queue->count > 0) {
            char* item = queue->items[queue->head];
            queue->items[queue->head] = NULL;
            queue->head = (queue->head + 1) % queue->capacity;
            queue->count--;

            monitor_signal(&queue->not_full_monitor);
            
            int became_empty = (queue->count == 0);
            
            pthread_mutex_unlock(&queue->mutex);
            
            if (became_empty) {
                monitor_reset(&queue->not_empty_monitor);
            }
            
            return item;
        }
        
        pthread_mutex_unlock(&queue->mutex);

        if (monitor_wait(&queue->not_empty_monitor) != 0) {
            return NULL;
        }
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