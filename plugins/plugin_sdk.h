#ifndef PLUGIN_SDK_H
#define PLUGIN_SDK_H

/**
 * Plugin SDK - Interface that all plugins must implement
 * This defines the contract between the main application and plugins
 */

// Function pointer types for plugin interface
typedef const char* (*plugin_init_func_t)(int queue_size);
typedef const char* (*plugin_fini_func_t)(void);
typedef const char* (*plugin_place_work_func_t)(const char* str);
typedef void (*plugin_attach_func_t)(const char* (*next_place_work)(const char*));
typedef const char* (*plugin_wait_finished_func_t)(void);
typedef const char* (*plugin_get_name_func_t)(void);

/**
 * Get the plugin's name
 * @return The plugin's name (should not be modified or freed)
 */
const char* plugin_get_name(void);

/**
 * Initialize the plugin with the specified queue size
 * @param queue_size Maximum number of items that can be queued
 * @return NULL on success, error message on failure
 */
const char* plugin_init(int queue_size);

/**
 * Finalize the plugin - terminate thread gracefully
 * @return NULL on success, error message on failure
 */
const char* plugin_fini(void);

/**
 * Place work (a string) into the plugin's queue
 * @param str The string to process (plugin takes ownership if it allocates new memory)
 * @return NULL on success, error message on failure
 */
const char* plugin_place_work(const char* str);

/**
 * Attach this plugin to the next plugin in the chain
 * @param next_place_work Function pointer to the next plugin's place_work function
 */
void plugin_attach(const char* (*next_place_work)(const char*));

/**
 * Wait until the plugin has finished processing all work and is ready to shutdown
 * This is a blocking function used for graceful shutdown coordination
 * @return NULL on success, error message on failure
 */
const char* plugin_wait_finished(void);

#endif // PLUGIN_SDK_H