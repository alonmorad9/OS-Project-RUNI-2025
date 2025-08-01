#include "plugin_common.h"

const char* plugin_transform(const char* input) {
    printf("[logger] %s\n", input);
    return strdup(input); // Pass to next plugin
}

const char* plugin_init(int queue_size) {
    return common_plugin_init(plugin_transform, "logger", queue_size);
}