#include "plugin_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Uppercaser plugin - converts lowercase to uppercase

static const char* plugin_transform(const char* input) {
    if (!input) {
        return NULL;
    }
    
    char* result = strdup(input);
    if (!result) {
        return NULL;
    }
    
    // Convert to uppercase
    for (int i = 0; result[i] != '\0'; i++) {
        if (isalpha(result[i])) {
            result[i] = toupper(result[i]);
        }
    }
    
    return result;
}

const char* plugin_get_name(void) { 
    return "uppercaser"; 
}

const char* plugin_init(int queue_size) { 
    return common_plugin_init(plugin_transform, "uppercaser", queue_size); 
}