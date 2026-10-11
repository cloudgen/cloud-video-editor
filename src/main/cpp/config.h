// config.h
// =========================================================================
// CIAO DEFENSIVE CODING STYLE - CONFIGURATION SYSTEM
// =========================================================================

#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// =========================================================================
// Official Default Values - Single Source of Truth
// =========================================================================

#define CONFIG_DEFAULT_WINDOW_WIDTH                 1200
#define CONFIG_DEFAULT_WINDOW_HEIGHT                800
#define CONFIG_DEFAULT_SHOW_NAVIGATION_BUTTONS      true
#define CONFIG_DEFAULT_SHOW_USER_AGENT_LABEL        true
#define CONFIG_DEFAULT_NAVIGATION_BUTTONS_ENABLED   true
#define CONFIG_DEFAULT_COOKIES_ENABLED              true
#define CONFIG_DEFAULT_BOOKMARKS_ENABLED            true
#define CONFIG_DEFAULT_OUTPUT_QUIET                 false
#define CONFIG_DEFAULT_OUTPUT_JSON                  false
#define CONFIG_DEFAULT_OUTPUT_DEBUG                 false

// New: Lua Userscript System
#define CONFIG_DEFAULT_SCRIPTS_ENABLED              true
#define CONFIG_DEFAULT_SCRIPTS_PATH                 "~/.app/" PROJECT_ID "/userscripts"

// =========================================================================
// AppConfig structure
// =========================================================================
typedef struct {
    char window_title[256];
    int  default_width;
    int  default_height;
    char project_name[128];

    bool show_navigation_buttons;
    bool show_user_agent_label;
    bool navigation_buttons_enabled;

    char base_path[512];
    bool cookies_enabled;
    char cookies_filename[128];
    char settings_filename[128];

    bool bookmarks_enabled;

    bool output_quiet;
    bool output_json;
    bool output_debug;

    // [scripts]
    bool scripts_enabled;
    char scripts_path[512];

} AppConfig;

// Initialize and load config
void config_init(const char *custom_config_path);

// Getters
const char* config_get_window_title(void);
int         config_get_default_width(void);
int         config_get_default_height(void);
const char* config_get_project_name(void);

bool config_get_show_navigation_buttons(void);
bool config_get_show_user_agent_label(void);
bool config_get_navigation_buttons_enabled(void);

const char* config_get_base_path(void);
const char* config_get_cookies_filename(void);
const char* config_get_settings_filename(void);
bool        config_get_cookies_enabled(void);

bool config_get_bookmarks_enabled(void);

bool config_get_output_quiet(void);
bool config_get_output_json(void);
bool config_get_output_debug(void);

// Scripts getters
bool        config_get_scripts_enabled(void);
const char* config_get_scripts_path(void);

#ifdef __cplusplus
}
#endif

#endif // CONFIG_H