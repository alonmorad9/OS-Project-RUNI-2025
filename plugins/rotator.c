#include "plugin_common.h"
#include <stdlib.h>
#include <string.h>

// Rotator plugin transformation function

static const char* plugin_transform(const char* input) { // transform the input string
    if (!input) return NULL; // handle NULL input
    size_t len = strlen(input);
    char* out = (char*)malloc(len + 1); // allocate memory for output string
    if (!out) return NULL; // handle memory allocation failure
    if (len == 0) {
        out[0] = '\0';
        return out;
    }
    // rotate right by 1, last char to front
    out[0] = input[len - 1];
    if (len > 1) {
        memcpy(out + 1, input, len - 1);
    }
    out[len] = '\0';
    return out;
}

const char* plugin_get_name(void) { return "rotator"; } // get plugin name

const char* plugin_init(int queue_size) { return common_plugin_init(plugin_transform, "rotator", queue_size); } // initialize plugin
