#include "plugin_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// global plugin context, shared across all plugins
static plugin_context_t g_plugin_context = {0};

void log_error(plugin_context_t* context, const char* message) { // log error messages
    if (context && context->name && message) {
        fprintf(stderr, "[ERROR][%s] - %s\n", context->name, message); // log the error message
    }
}

void log_info(plugin_context_t* context, const char* message) { // log info messages
    if (context && context->name && message) {
        fprintf(stderr, "[INFO][%s] - %s\n", context->name, message); // log the info message
    }
}

void* plugin_consumer_thread(void* arg) { // consumer thread for plugin
    plugin_context_t* context = (plugin_context_t*)arg;
    
    if (!context) {
        return NULL;
    }

    while (1) {  
        char* work_item = consumer_producer_get(context->queue); // get work item from queue

        if (!work_item) { // check if work item is NULL
            log_error(context, "Failed to get work item from queue");
            break;
        }

        if (strcmp(work_item, "<END>") == 0) { // check for termination signal
            if (context->next_place_work) { // check if there is a next plugin
                const char* result = context->next_place_work("<END>"); // pass <END> to next plugin
                if (result != NULL) {
                    log_error(context, "Failed to pass <END> to next plugin");
                }
            }

            free(work_item); // free the work item
            context->finished = 1; // mark the context as finished
            consumer_producer_signal_finished(context->queue); // signal that processing is finished
            break;
        }

        const char* processed_item = context->process_function(work_item); // process the work item

        free(work_item); // free the original work item

        if (!processed_item) { // check if processed item is NULL
            log_error(context, "Plugin processing function returned NULL");
            continue;
        }

        if (context->next_place_work) { // check if there is a next plugin
            const char* result = context->next_place_work(processed_item);
            if (result != NULL) {
                log_error(context, "Failed to pass work to next plugin");
            }
        }
        
        // If this is the last plugin in the chain, we need to free the processed item
        if (!context->next_place_work) {
            free((void*)processed_item);
        }
    }
    
    return NULL;
}

const char* common_plugin_init(const char* (*process_function)(const char*), 
                               const char* name, int queue_size) { // Initialize the plugin
    if (!process_function || !name || queue_size <= 0) { // Check for valid parameters
        return "Invalid parameters for plugin initialization";
    }

    memset(&g_plugin_context, 0, sizeof(plugin_context_t)); // initialize the global context

    g_plugin_context.name = name;
    g_plugin_context.process_function = process_function;
    g_plugin_context.next_place_work = NULL;
    g_plugin_context.initialized = 0;
    g_plugin_context.finished = 0;

    // allocate and initialize the queue
    g_plugin_context.queue = malloc(sizeof(consumer_producer_t));
    if (!g_plugin_context.queue) {
        return "Failed to allocate memory for plugin queue";
    }
    const char* queue_init_result = consumer_producer_init(g_plugin_context.queue, queue_size);
    if (queue_init_result != NULL) { // Check for queue initialization errors
        free(g_plugin_context.queue);
        g_plugin_context.queue = NULL;
        return queue_init_result; // return the error message
    }
    
    // create the consumer thread
    int thread_result = pthread_create(&g_plugin_context.consumer_thread, NULL, 
                                       plugin_consumer_thread, &g_plugin_context);
    if (thread_result != 0) {
        consumer_producer_destroy(g_plugin_context.queue);
        free(g_plugin_context.queue);
        g_plugin_context.queue = NULL;
        return "Failed to create consumer thread";
    }

    g_plugin_context.initialized = 1; // mark the plugin as initialized
    return NULL; // success
}

const char* plugin_place_work(const char* str) { // place work item in the queue
    if (!str) {
        return "Cannot place NULL work item";
    }

    if (!g_plugin_context.initialized || !g_plugin_context.queue) { // check if plugin is initialized
        return "Plugin not initialized";
    }
    
    return consumer_producer_put(g_plugin_context.queue, str);
}

void plugin_attach(const char* (*next_place_work)(const char*)) { // attach next plugin
    g_plugin_context.next_place_work = next_place_work;
}

const char* plugin_wait_finished(void) { // wait for plugin to finish
    if (!g_plugin_context.initialized || !g_plugin_context.queue) {
        return "Plugin not initialized";
    }
    
    // wait for the finished signal
    if (consumer_producer_wait_finished(g_plugin_context.queue) != 0) {
        return "Failed to wait for plugin to finish";
    }

    return NULL; // success
}

const char* plugin_fini(void) { // finalize the plugin
    if (!g_plugin_context.initialized) {
        return "Plugin not initialized";
    }
    
    // wait for the consumer thread to finish
    void* thread_result;
    int join_result = pthread_join(g_plugin_context.consumer_thread, &thread_result);
    if (join_result != 0) {
        log_error(&g_plugin_context, "Failed to join consumer thread");
    }
    
    // clean up the queue
    if (g_plugin_context.queue) {
        consumer_producer_destroy(g_plugin_context.queue);
        free(g_plugin_context.queue);
        g_plugin_context.queue = NULL;
    }
    
    // reset the context
    memset(&g_plugin_context, 0, sizeof(plugin_context_t));

    return NULL; // success
}
