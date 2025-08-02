#include "plugin_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Global plugin context (each plugin will have its own .so file)
static plugin_context_t g_plugin_context = {0};

void log_error(plugin_context_t* context, const char* message) {
    if (context && context->name && message) {
        fprintf(stderr, "[ERROR][%s] - %s\n", context->name, message);
    }
}

void log_info(plugin_context_t* context, const char* message) {
    if (context && context->name && message) {
        printf("[INFO][%s] - %s\n", context->name, message);
    }
}

void* plugin_consumer_thread(void* arg) {
    plugin_context_t* context = (plugin_context_t*)arg;
    
    if (!context) {
        return NULL;
    }
    
    while (1) {
        // Get work from the queue (this will block if queue is empty)
        char* work_item = consumer_producer_get(context->queue);
        
        if (!work_item) {
            log_error(context, "Failed to get work item from queue");
            break;
        }
        
        // Check for termination signal
        if (strcmp(work_item, "<END>") == 0) {
            // Pass <END> to next plugin if there is one
            if (context->next_place_work) {
                const char* result = context->next_place_work("<END>");
                if (result != NULL) {
                    log_error(context, "Failed to pass <END> to next plugin");
                }
            }
            
            // Clean up and signal we're finished
            free(work_item);
            context->finished = 1;
            consumer_producer_signal_finished(context->queue);
            break;
        }
        
        // Process the work item using the plugin-specific function
        const char* processed_item = context->process_function(work_item);
        
        // Free the original work item since we consumed it
        free(work_item);
        
        if (!processed_item) {
            log_error(context, "Plugin processing function returned NULL");
            continue;
        }
        
        // Pass the processed item to the next plugin if there is one
        if (context->next_place_work) {
            const char* result = context->next_place_work(processed_item);
            if (result != NULL) {
                log_error(context, "Failed to pass work to next plugin");
            }
        }
        
        // If this is the last plugin in the chain, we need to free the processed item
        // since no one else will take ownership of it
        if (!context->next_place_work) {
            free((void*)processed_item);
        }
    }
    
    return NULL;
}

const char* common_plugin_init(const char* (*process_function)(const char*), 
                               const char* name, int queue_size) {
    if (!process_function || !name || queue_size <= 0) {
        return "Invalid parameters for plugin initialization";
    }
    
    // Initialize the global context
    memset(&g_plugin_context, 0, sizeof(plugin_context_t));
    
    g_plugin_context.name = name;
    g_plugin_context.process_function = process_function;
    g_plugin_context.next_place_work = NULL;
    g_plugin_context.initialized = 0;
    g_plugin_context.finished = 0;
    
    // Allocate and initialize the queue
    g_plugin_context.queue = malloc(sizeof(consumer_producer_t));
    if (!g_plugin_context.queue) {
        return "Failed to allocate memory for plugin queue";
    }
    
    const char* queue_init_result = consumer_producer_init(g_plugin_context.queue, queue_size);
    if (queue_init_result != NULL) {
        free(g_plugin_context.queue);
        g_plugin_context.queue = NULL;
        return queue_init_result;
    }
    
    // Create the consumer thread
    int thread_result = pthread_create(&g_plugin_context.consumer_thread, NULL, 
                                       plugin_consumer_thread, &g_plugin_context);
    if (thread_result != 0) {
        consumer_producer_destroy(g_plugin_context.queue);
        free(g_plugin_context.queue);
        g_plugin_context.queue = NULL;
        return "Failed to create consumer thread";
    }
    
    g_plugin_context.initialized = 1;
    return NULL; // Success
}

const char* plugin_place_work(const char* str) {
    if (!str) {
        return "Cannot place NULL work item";
    }
    
    if (!g_plugin_context.initialized || !g_plugin_context.queue) {
        return "Plugin not initialized";
    }
    
    return consumer_producer_put(g_plugin_context.queue, str);
}

void plugin_attach(const char* (*next_place_work)(const char*)) {
    g_plugin_context.next_place_work = next_place_work;
}

const char* plugin_wait_finished(void) {
    if (!g_plugin_context.initialized || !g_plugin_context.queue) {
        return "Plugin not initialized";
    }
    
    // Wait for the finished signal
    if (consumer_producer_wait_finished(g_plugin_context.queue) != 0) {
        return "Failed to wait for plugin to finish";
    }
    
    return NULL; // Success
}

const char* plugin_fini(void) {
    if (!g_plugin_context.initialized) {
        return "Plugin not initialized";
    }
    
    // Wait for the consumer thread to finish
    void* thread_result;
    int join_result = pthread_join(g_plugin_context.consumer_thread, &thread_result);
    if (join_result != 0) {
        log_error(&g_plugin_context, "Failed to join consumer thread");
    }
    
    // Clean up the queue
    if (g_plugin_context.queue) {
        consumer_producer_destroy(g_plugin_context.queue);
        free(g_plugin_context.queue);
        g_plugin_context.queue = NULL;
    }
    
    // Reset the context
    memset(&g_plugin_context, 0, sizeof(plugin_context_t));
    
    return NULL; // Success
}