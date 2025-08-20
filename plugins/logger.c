#include "plugin_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Plugin-specific transformation function
static const char* plugin_transform(const char* input) {
    if (!input) {
        return NULL;
    }
    
    // Log the string to stdout
    printf("[logger] %s\n", input);
    fflush(stdout); // Ensure immediate output
    
    // Return a copy of the input string to pass to the next plugin
    return strdup(input);
}

// Plugin interface implementations
const char* plugin_get_name(void) { return "logger"; }
const char* plugin_init(int queue_size) { return common_plugin_init(plugin_transform, "logger", queue_size); }