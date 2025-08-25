#define _POSIX_C_SOURCE 200809L
#include "plugin_common.h"
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Typewriter plugin - prints characters with delay

static const char* plugin_transform(const char* input) {
    if (!input) return NULL;
    
    size_t len = strlen(input);
    printf("[typewriter] ");
    fflush(stdout); 
    
    for (size_t i = 0; i < len; i++) {
        putchar(input[i]);
        fflush(stdout);
        usleep(100000); // 100ms delay
    }
    putchar('\n');
    fflush(stdout);
    
    return strdup(input);
}

const char* plugin_get_name(void) { 
    return "typewriter"; 
}

const char* plugin_init(int queue_size) { 
    return common_plugin_init(plugin_transform, "typewriter", queue_size); 
}