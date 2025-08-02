#include "monitor.h"
#include <errno.h>

int monitor_init(monitor_t* monitor) {
    if (!monitor) {
        return -1;
    }
    
    // Initialize mutex
    if (pthread_mutex_init(&monitor->mutex, NULL) != 0) {
        return -1;
    }
    
    // Initialize condition variable
    if (pthread_cond_init(&monitor->condition, NULL) != 0) {
        pthread_mutex_destroy(&monitor->mutex);
        return -1;
    }
    
    // Initialize signaled state to false
    monitor->signaled = 0;
    
    return 0;
}

void monitor_destroy(monitor_t* monitor) {
    if (!monitor) {
        return;
    }
    
    pthread_mutex_destroy(&monitor->mutex);
    pthread_cond_destroy(&monitor->condition);
    monitor->signaled = 0;
}

void monitor_signal(monitor_t* monitor) {
    if (!monitor) {
        return;
    }
    
    pthread_mutex_lock(&monitor->mutex);
    
    // Set the signaled flag and wake up waiting threads
    monitor->signaled = 1;
    pthread_cond_broadcast(&monitor->condition);
    
    pthread_mutex_unlock(&monitor->mutex);
}

void monitor_reset(monitor_t* monitor) {
    if (!monitor) {
        return;
    }
    
    pthread_mutex_lock(&monitor->mutex);
    monitor->signaled = 0;
    pthread_mutex_unlock(&monitor->mutex);
}

int monitor_wait(monitor_t* monitor) {
    if (!monitor) {
        return -1;
    }
    
    pthread_mutex_lock(&monitor->mutex);
    
    // Wait until the monitor is signaled
    while (!monitor->signaled) {
        int result = pthread_cond_wait(&monitor->condition, &monitor->mutex);
        if (result != 0) {
            pthread_mutex_unlock(&monitor->mutex);
            return -1;
        }
    }
    
    pthread_mutex_unlock(&monitor->mutex);
    return 0;
}