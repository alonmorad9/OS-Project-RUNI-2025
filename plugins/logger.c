#include "plugin_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Logger plugin transform function

static const char* plugin_transform(const char* input) { // transform input string
    if (!input) { // handle NULL input
        return NULL;
    }

    printf("[logger] %s\n", input); // log the input string
    fflush(stdout); // ensure immediate output

    return strdup(input); // return a copy of the input string
}

const char* plugin_get_name(void) { return "logger"; } // get plugin name
const char* plugin_init(int queue_size) { return common_plugin_init(plugin_transform, "logger", queue_size); } // initialize plugin