#include "plugin_common.h"
#include <stdlib.h>
#include <string.h>

// Expander plugin transformation function

static const char* plugin_transform(const char* input) { // transform input string
    if (!input) return NULL; // handle NULL input
    size_t len = strlen(input);
    
    if (len == 0) { // handle empty input
        char* empty = (char*)malloc(1); // allocate memory for empty string
        if (empty) empty[0] = '\0';
        return empty;
    }
    
    if (len == 1) { 
        // Single character case: no spaces to insert between characters
        // Assignment: "Inserts a single white space between each character"
        // For one character, there are no "between" positions, so return as-is
        return strdup(input);
    }
    
    // For multiple characters: insert spaces between each pair
    // e.g., "abc" becomes "a b c" (len=3, spaces=2, total=5)
    size_t out_len = len + (len - 1);
    char* out = (char*)malloc(out_len + 1);
    if (!out) return NULL;
    
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        out[j++] = input[i];
        if (i + 1 < len) out[j++] = ' '; // insert space if not the last character
    }
    out[out_len] = '\0';
    return out;
}

const char* plugin_get_name(void) { return "expander"; } // get plugin name

const char* plugin_init(int queue_size) { return common_plugin_init(plugin_transform, "expander", queue_size); } // initialize plugin