#include "plugin_common.h"
#include <stdlib.h>
#include <string.h>

// Flipper plugin transformation function

static const char* plugin_transform(const char* input) { // transform input string
    if (!input) return NULL; // handle NULL input
    size_t len = strlen(input);
    char* out = (char*)malloc(len + 1); // allocate memory for output string
    if (!out) return NULL; // handle memory allocation failure
    for (size_t i = 0; i < len; i++) { // reverse input string
        out[i] = input[len - 1 - i];
    }
    out[len] = '\0';
    return out;
}

const char* plugin_get_name(void) { return "flipper"; } // get plugin name

const char* plugin_init(int queue_size) { return common_plugin_init(plugin_transform, "flipper", queue_size); } // initialize plugin
