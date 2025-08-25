#include "plugin_common.h"
#include <stdlib.h>
#include <string.h>

// Rotator plugin - shifts characters right

static const char* plugin_transform(const char* input) {
    if (!input) return NULL;
    
    size_t len = strlen(input);
    char* out = malloc(len + 1);
    if (!out) return NULL;
    
    if (len == 0) {
        out[0] = '\0';
        return out;
    }
    
    // Move last char to front, shift others right
    out[0] = input[len - 1];
    if (len > 1) {
        memcpy(out + 1, input, len - 1);
    }
    out[len] = '\0';
    
    return out;
}

const char* plugin_get_name(void) { 
    return "rotator"; 
}

const char* plugin_init(int queue_size) { 
    return common_plugin_init(plugin_transform, "rotator", queue_size); 
}
