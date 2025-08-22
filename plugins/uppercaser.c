#include "plugin_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Uppercaser plugin transformation function

static const char* plugin_transform(const char* input) { // transform the input string
    if (!input) {
        return NULL;
    }
    
    // Create a copy of the input string
    char* result = strdup(input);
    if (!result) {
        return NULL;
    }
    
    // Convert all alphabetic characters to uppercase
    for (int i = 0; result[i] != '\0'; i++) {
        if (isalpha(result[i])) {
            result[i] = toupper(result[i]);
        }
    }
    
    return result;
}

const char* plugin_get_name(void) { return "uppercaser"; } // get plugin name
const char* plugin_init(int queue_size) { return common_plugin_init(plugin_transform, "uppercaser", queue_size); } // initialize plugin