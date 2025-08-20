#include "plugin_common.h"
#include <stdlib.h>
#include <string.h>

static const char* plugin_transform(const char* input) {
    if (!input) return NULL;
    size_t len = strlen(input);
    char* out = (char*)malloc(len + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < len; i++) {
        out[i] = input[len - 1 - i];
    }
    out[len] = '\0';
    return out;
}

const char* plugin_get_name(void) { return "flipper"; }

const char* plugin_init(int queue_size) { return common_plugin_init(plugin_transform, "flipper", queue_size); }

