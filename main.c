#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <errno.h>
#include "plugins/plugin_sdk.h"

typedef struct {
    plugin_init_func_t init;
    plugin_fini_func_t fini;
    plugin_place_work_func_t place_work;
    plugin_attach_func_t attach;
    plugin_wait_finished_func_t wait_finished;
    plugin_get_name_func_t get_name;
    char* name;
    void* handle;
} plugin_handle_t;

static void print_usage(void) {
    printf("Usage: ./analyzer <queue_size> <plugin1> <plugin2> ... <pluginN>\n\n");
    printf("Arguments:\n");
    printf("queue_size Maximum number of items in each plugin's queue\n");
    printf("plugin1..N Names of plugins to load (without .so extension)\n\n");
    printf("Available plugins:\n");
    printf("logger - Logs all strings that pass through\n");
    printf("typewriter - Simulates typewriter effect with delays\n");
    printf("uppercaser - Converts strings to uppercase\n");
    printf("rotator - Move every character to the right. Last character moves to the beginning.\n");
    printf("flipper - Reverses the order of characters\n");
    printf("expander - Expands each character with spaces\n\n");
    printf("Example:\n");
    printf("./analyzer 20 uppercaser rotator logger\n\n");
    printf("echo 'hello' | ./analyzer 20 uppercaser rotator logger\n");
    printf("echo '<END>' | ./analyzer 20 uppercaser rotator logger\n");
}

static void cleanup_plugins(plugin_handle_t* plugins, int count) {
    if (!plugins) return;
    for (int i = 0; i < count; i++) {
        if (plugins[i].fini) {
            plugins[i].fini();
        }
    }
    for (int i = 0; i < count; i++) {
        if (plugins[i].handle) {
            dlclose(plugins[i].handle);
            plugins[i].handle = NULL;
        }
        if (plugins[i].name) {
            free(plugins[i].name);
            plugins[i].name = NULL;
        }
    }
}

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "Invalid arguments\n");
        print_usage();
        return 1;
    }

    char* endptr = NULL;
    long queue_size_long = strtol(argv[1], &endptr, 10);
    if (endptr == argv[1] || *endptr != '\0' || queue_size_long <= 0 || queue_size_long > 1000000) {
        fprintf(stderr, "Invalid queue size\n");
        print_usage();
        return 1;
    }
    int queue_size = (int)queue_size_long;

    int num_plugins = argc - 2;
    plugin_handle_t* plugins = (plugin_handle_t*)calloc((size_t)num_plugins, sizeof(plugin_handle_t));
    if (!plugins) {
        fprintf(stderr, "Memory allocation failure\n");
        return 1;
    }

    // Load plugins
    for (int i = 0; i < num_plugins; i++) {
        const char* plugin_name = argv[2 + i];
        char so_path[512];
        snprintf(so_path, sizeof(so_path), "./output/%s.so", plugin_name);

        void* handle = dlopen(so_path, RTLD_NOW | RTLD_LOCAL);
        if (!handle) {
            fprintf(stderr, "Failed to load plugin '%s': %s\n", plugin_name, dlerror());
            print_usage();
            cleanup_plugins(plugins, i);
            free(plugins);
            return 1;
        }

        plugins[i].handle = handle;
        plugins[i].name = strdup(plugin_name);
        if (!plugins[i].name) {
            fprintf(stderr, "Memory allocation failure\n");
            print_usage();
            cleanup_plugins(plugins, i + 1);
            free(plugins);
            return 1;
        }

        // Resolve symbols
        plugins[i].init = (plugin_init_func_t)dlsym(handle, "plugin_init");
        plugins[i].fini = (plugin_fini_func_t)dlsym(handle, "plugin_fini");
        plugins[i].place_work = (plugin_place_work_func_t)dlsym(handle, "plugin_place_work");
        plugins[i].attach = (plugin_attach_func_t)dlsym(handle, "plugin_attach");
        plugins[i].wait_finished = (plugin_wait_finished_func_t)dlsym(handle, "plugin_wait_finished");
        plugins[i].get_name = (plugin_get_name_func_t)dlsym(handle, "plugin_get_name");

        if (!plugins[i].init || !plugins[i].fini || !plugins[i].place_work || !plugins[i].attach || !plugins[i].wait_finished || !plugins[i].get_name) {
            fprintf(stderr, "Missing required symbol(s) in plugin '%s'\n", plugin_name);
            print_usage();
            cleanup_plugins(plugins, i + 1);
            free(plugins);
            return 1;
        }
    }

    // Initialize plugins
    for (int i = 0; i < num_plugins; i++) {
        const char* err = plugins[i].init(queue_size);
        if (err != NULL) {
            fprintf(stderr, "Failed to init plugin '%s': %s\n", plugins[i].name, err);
            // cleanup any already-initialized
            for (int j = 0; j < i; j++) {
                plugins[j].fini();
            }
            cleanup_plugins(plugins, num_plugins);
            free(plugins);
            return 2;
        }
    }

    // Attach pipeline
    for (int i = 0; i < num_plugins - 1; i++) {
        plugins[i].attach(plugins[i + 1].place_work);
    }

    // Read input lines and feed into pipeline
    char buffer[1026];
    while (fgets(buffer, sizeof(buffer), stdin) != NULL) {
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len - 1] == '\n') {
            buffer[len - 1] = '\0';
            len--;
        }

        // Send to first plugin
        const char* place_err = plugins[0].place_work(buffer);
        if (place_err != NULL) {
            fprintf(stderr, "Failed to place work in first plugin: %s\n", place_err);
            break;
        }

        if (strcmp(buffer, "<END>") == 0) {
            break;
        }
    }

    // Wait for plugins to finish (from first to last)
    for (int i = 0; i < num_plugins; i++) {
        const char* err = plugins[i].wait_finished();
        if (err != NULL) {
            fprintf(stderr, "Error waiting for plugin '%s': %s\n", plugins[i].name, err);
        }
    }

    // Cleanup
    for (int i = 0; i < num_plugins; i++) {
        const char* err = plugins[i].fini();
        if (err != NULL) {
            fprintf(stderr, "Error finalizing plugin '%s': %s\n", plugins[i].name, err);
        }
    }

    cleanup_plugins(plugins, num_plugins);
    free(plugins);

    printf("Pipeline shutdown complete\n");
    return 0;
}

