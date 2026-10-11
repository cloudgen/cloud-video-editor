// config.cpp
// =========================================================================
// CIAO DEFENSIVE CODING STYLE - CONFIGURATION LOADER
// =========================================================================
//
// !!! THIS IS A GNOME/GTK APPLICATION !!!
// !!! CRITICAL: The INI parser used here is extremely naive and preserves
//     surrounding quotes and whitespace from config.conf !!!
//
// Purpose:
//   Loads config.conf and overrides defaults from project.h (Single Source of Truth).
// New Feature (2026-05-17):
//   When config.conf is missing, the application now automatically:
//     1. Creates the project directory ~/.app/${PROJECT_ID}/
//     2. Creates a new config.conf with sensible defaults + helpful comments
//     3. Logs the creation clearly
//
// Root Cause of Previous Week-Long Bug (2026-05-16 ~ 2026-05-17):
//   The simple INI parser did NOT strip surrounding quotes.
//   Values like cookies_filename = "cookies.sqlite" were stored as "\"cookies.sqlite\"".
//   This caused cookie storage paths to become:
//     ~/.app/<PROJECT_ID>/cookies/"cookies.sqlite"
//   → libsoup/WebKit could not open the file → cookies never persisted.
//
// Lessons Learned (Critical - must never repeat):
//   - Even "small" config parsing bugs can silently break core features like cookies.
//   - All string values from config must be cleaned before use in paths.
//   - Path construction must be centralized in path_utils.cpp (Single Source of Truth).
//   - Never assume config values are clean — always sanitize quotes, whitespace, etc.
//   - This bug wasted almost a full week because the error only appeared deep in the
//     final path passed to WebKit (very hard to spot without DEBUG=1).
//
// CIAO-Lite Principles Applied (Strengthened):
//   - Caution: Full defensive cleaning of all config strings (quotes, whitespace)
//   - Intentional: Clear documentation of past failure mode
//   - Anti-fragile: Centralized cleaning in path_utils + robust getters
//   - Over-protect: Strong warning so future AI never reintroduces this bug
//
// Protection Rule (Sacred - Zero Tolerance):
//   Future AI or developers MUST NOT:
//     - Remove quote/whitespace stripping logic
//     - Assume config values are clean
//     - Bypass path_utils.cpp for any storage path construction
//   This config system has already caused too much pain.
//
// Last updated: 2026-05-17 (after week-long quote pollution debugging nightmare)
// =========================================================================

#include "config.h"
#include "project.h"
#include "output.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <glib.h>       // for g_mkdir_with_parents

static AppConfig current_config = {0};
static bool config_loaded = false;

static void load_config_file(const char *filepath);
static void create_default_config_file(const char *filepath);

// ──────────────────────────────────────────────
// create_default_config_file() - Auto-create config.conf when missing
// ──────────────────────────────────────────────
//
// Purpose:
//   When config.conf does not exist (first run or after forking to a new project),
//   this function automatically creates the project directory and a new,
//   well-commented config.conf file populated with sensible defaults.
//
//   This provides an excellent self-configuring first-run experience and
//   fulfills the clarified requirement in configuration.md.
//
// Parameters:
//   Input:
//     - filepath : const char* - Full path where config.conf should be created
//   Output/Return: None (void function)
//
// Dependencies:
//   - Functions called: g_path_get_dirname(), g_mkdir_with_parents(), fopen(), fprintf(), ciao_info()
//   - Headers required: glib.h, stdio.h, output.h, config.h
//
// Why This Design:
//   Centralizes the creation of the default configuration file so that the
//   application is always ready to run without manual setup. The generated
//   file includes helpful comments to guide the user.
//
// CIAO-Lite Principles Applied:
//   - Caution: Safe directory creation and file writing with proper error handling
//   - Intentional: Creates a self-documenting config file with clear comments
//   - Anti-fragile: Graceful behavior on first run and after project forking
//   - Over-protect: Ensures the user always has a valid, well-commented config
//
// Protection Rule (Sacred):
//   Future AI or developers MUST NOT:
//     - Remove or simplify the automatic config file creation logic
//     - Remove the helpful comment header in the generated config.conf
//     - Change the default values without updating configuration.md and config.h macros
//   This function is critical for user experience and first-run reliability.
//
// Last updated: 2026-05-17 (uses centralized CONFIG_DEFAULT_* macros from config.h)
// ──────────────────────────────────────────────
static void create_default_config_file(const char *filepath)
{
    if (filepath == NULL || *filepath == '\0') return;

    gchar *dir = g_path_get_dirname(filepath);
    if (dir) {
        g_mkdir_with_parents(dir, 0700);
        g_free(dir);
    }

    FILE *f = fopen(filepath, "w");
    if (!f) {
        ciao_error("Failed to create default config file: %s", filepath);
        return;
    }

    fprintf(f, "# =============================================================================\n");
    fprintf(f, "# config.conf - Single Source of Truth\n");
    fprintf(f, "# Project: %s\n", PROJECT_ID);
    fprintf(f, "# Automatically created on first run\n");
    fprintf(f, "# =============================================================================\n\n");

    fprintf(f, "[core]\n");
    fprintf(f, "window_title = %s\n", PROJECT_NAME);
    fprintf(f, "default_width = %d\n", CONFIG_DEFAULT_WINDOW_WIDTH);
    fprintf(f, "default_height = %d\n", CONFIG_DEFAULT_WINDOW_HEIGHT);
    fprintf(f, "project_name = %s\n\n", PROJECT_ID);

    fprintf(f, "[ui]\n");
    fprintf(f, "show_navigation_buttons = %s\n",   CONFIG_DEFAULT_SHOW_NAVIGATION_BUTTONS ? "true" : "false");
    fprintf(f, "show_user_agent_label = %s\n",     CONFIG_DEFAULT_SHOW_USER_AGENT_LABEL ? "true" : "false");
    fprintf(f, "navigation_buttons_enabled = %s\n",CONFIG_DEFAULT_NAVIGATION_BUTTONS_ENABLED ? "true" : "false");
    fprintf(f, "\n");

    fprintf(f, "[storage]\n");
    fprintf(f, "base_path = ~/.app/%s\n", PROJECT_ID);
    fprintf(f, "cookies_enabled = %s\n", CONFIG_DEFAULT_COOKIES_ENABLED ? "true" : "false");
    fprintf(f, "cookies_filename = cookies.sqlite\n");
    fprintf(f, "settings_filename = settings.sqlite\n\n");

    fprintf(f, "[output]\n");
    fprintf(f, "quiet = %s\n", CONFIG_DEFAULT_OUTPUT_QUIET ? "true" : "false");
    fprintf(f, "json = %s\n",  CONFIG_DEFAULT_OUTPUT_JSON  ? "true" : "false");
    fprintf(f, "debug = %s\n\n", CONFIG_DEFAULT_OUTPUT_DEBUG ? "true" : "false");

    fprintf(f, "[bookmarks]\n");
    fprintf(f, "enabled = %s\n\n", CONFIG_DEFAULT_BOOKMARKS_ENABLED ? "true" : "false");

    // New: Scripts section for Lua userscript system
    fprintf(f, "[scripts]\n");
    fprintf(f, "enabled = %s\n", CONFIG_DEFAULT_SCRIPTS_ENABLED ? "true" : "false");
    fprintf(f, "path = %s\n\n", CONFIG_DEFAULT_SCRIPTS_PATH);

    fprintf(f, "[parameters]\n");
    fprintf(f, "shutdown_seconds = 0\n");
    fprintf(f, "bookmark_index = 0\n");

    fclose(f);
    ciao_info("✅ Created new default configuration file: %s", filepath);
}

// ──────────────────────────────────────────────
// config_set_value() - Core INI Value Setter (Single Source of Truth)
// ──────────────────────────────────────────────
//
// Purpose:
//   Parses and stores a single key=value pair from config.conf into the
//   global current_config structure.
//
// CRITICAL LESSON LEARNED (2026-05-16 ~ 2026-05-17 - wasted almost a full week):
//   This function was previously careless and did NOT strip surrounding quotes.
//   When config.conf contained lines like:
//       cookies_filename = "cookies.sqlite"
//       base_path = "~/.app/<PROJECT_ID>"
//
//   The values were stored with literal quotes, causing paths like:
//       ~/.app/<PROJECT_ID>/cookies/"cookies.sqlite"
//
//   → WebKit/libsoup could not open the cookie file → cookies never worked.
//   The bug was extremely hard to spot because it only manifested deep in
//   path construction (initialize_cookie_storage / initialize_settings_storage).
//
// Root Cause:
//   Naive string copy without any sanitization of quotes or whitespace.
//
// CIAO-Lite Principles Applied (now strengthened):
//   - Caution: Aggressive quote and whitespace stripping for all string values
//   - Intentional: This is the ONLY place where raw config values are processed
//   - Anti-fragile: Prevents the same quote pollution bug from ever recurring
//   - Over-protect: Strong defensive cleaning + detailed lesson learned
//
// Protection Rule (Sacred - Zero Tolerance):
//   Future AI or developers MUST NOT:
//     - Remove or weaken the quote/whitespace stripping logic
//     - Add new string fields without cleaning them here
//     - Assume config values are clean — they are NOT
//   This function has already caused too much pain. Never make it careless again.
//
// Last updated: 2026-05-17 (after week-long quote pollution debugging nightmare)
// ──────────────────────────────────────────────
static void config_set_value(const char *section, const char *key, const char *value)
{
    if (section == NULL || key == NULL || value == NULL) return;

    char cleaned[512] = {0};
    g_strlcpy(cleaned, value, sizeof(cleaned));
    gchar *trimmed = g_strstrip(cleaned);

    // Remove surrounding quotes (quote pollution defense)
    if (trimmed[0] == '"' && trimmed[strlen(trimmed)-1] == '"') {
        trimmed[strlen(trimmed)-1] = '\0';
        trimmed++;
    } else if (trimmed[0] == '\'' && trimmed[strlen(trimmed)-1] == '\'') {
        trimmed[strlen(trimmed)-1] = '\0';
        trimmed++;
    }

    if (strcmp(section, "core") == 0) {
        if (strcmp(key, "window_title") == 0) strncpy(current_config.window_title, trimmed, sizeof(current_config.window_title)-1);
        else if (strcmp(key, "default_width") == 0) current_config.default_width = atoi(trimmed);
        else if (strcmp(key, "default_height") == 0) current_config.default_height = atoi(trimmed);
        else if (strcmp(key, "project_name") == 0) strncpy(current_config.project_name, trimmed, sizeof(current_config.project_name)-1);
    }
    else if (strcmp(section, "ui") == 0) {
        if (strcmp(key, "show_navigation_buttons") == 0) current_config.show_navigation_buttons = (atoi(trimmed)==1 || strcasecmp(trimmed,"true")==0);
        else if (strcmp(key, "show_user_agent_label") == 0) current_config.show_user_agent_label = (atoi(trimmed)==1 || strcasecmp(trimmed,"true")==0);
        else if (strcmp(key, "navigation_buttons_enabled") == 0) current_config.navigation_buttons_enabled = (atoi(trimmed)==1 || strcasecmp(trimmed,"true")==0);
    }
    else if (strcmp(section, "storage") == 0) {
        if (strcmp(key, "base_path") == 0) strncpy(current_config.base_path, trimmed, sizeof(current_config.base_path)-1);
        else if (strcmp(key, "cookies_enabled") == 0) current_config.cookies_enabled = (atoi(trimmed)==1 || strcasecmp(trimmed,"true")==0);
        else if (strcmp(key, "cookies_filename") == 0) strncpy(current_config.cookies_filename, trimmed, sizeof(current_config.cookies_filename)-1);
        else if (strcmp(key, "settings_filename") == 0) strncpy(current_config.settings_filename, trimmed, sizeof(current_config.settings_filename)-1);
    }
    else if (strcmp(section, "bookmarks") == 0) {
        if (strcmp(key, "enabled") == 0) current_config.bookmarks_enabled = (atoi(trimmed)==1 || strcasecmp(trimmed,"true")==0);
    }
    else if (strcmp(section, "output") == 0) {
        if (strcmp(key, "quiet") == 0) current_config.output_quiet = (atoi(trimmed)==1 || strcasecmp(trimmed,"true")==0);
        else if (strcmp(key, "json") == 0) current_config.output_json = (atoi(trimmed)==1 || strcasecmp(trimmed,"true")==0);
        else if (strcmp(key, "debug") == 0) current_config.output_debug = (atoi(trimmed)==1 || strcasecmp(trimmed,"true")==0);
    }
    // NEW: Scripts section for Lua userscript system
    else if (strcmp(section, "scripts") == 0) {
        if (strcmp(key, "enabled") == 0) current_config.scripts_enabled = (atoi(trimmed)==1 || strcasecmp(trimmed,"true")==0);
        else if (strcmp(key, "path") == 0) strncpy(current_config.scripts_path, trimmed, sizeof(current_config.scripts_path)-1);
    }
}

// ──────────────────────────────────────────────
// load_config_file() - Raw Config File Parser
// ──────────────────────────────────────────────
//
// Purpose:
//   Reads config.conf line by line and dispatches each key=value pair to
//   config_set_value(). This is the entry point where raw text from the
//   config file is turned into configuration values.
//
// CRITICAL LESSON LEARNED (2026-05-16 ~ 2026-05-17 — wasted almost a full week):
//   The previous naive parser blindly accepted values with surrounding quotes.
//   Lines like:
//       cookies_filename = "cookies.sqlite"
//       base_path = "~/.app/<PROJECT_ID>"
//
//   caused values to be stored as "\"cookies.sqlite\"" and "\"~/.app/<PROJECT_ID>\"".
//   This led to broken paths such as:
//       ~/.app/<PROJECT_ID>/cookies/"cookies.sqlite"
//
//   → Persistent cookies failed silently for days. The bug was extremely hard to
//   spot because it only appeared deep in path construction (WebKit/libsoup error).
//
// Root Cause:
//   No sanitization of quotes/whitespace at the parsing stage.
//
// CIAO-Lite Principles Applied (now heavily strengthened):
//   - Caution: Aggressive trimming of quotes and whitespace on every value
//   - Intentional: Clear documentation of past failure mode and why it hurt
//   - Anti-fragile: Prevents the same quote pollution bug from ever recurring
//   - Over-protect: This is now the ONLY place where raw config strings are processed
//
// Protection Rule (Sacred - Zero Tolerance):
//   Future AI or developers MUST NOT:
//     - Remove or weaken the quote/whitespace cleaning logic
//     - Assume config values are clean — they are NOT
//     - Add new string parsing without calling the same cleaning step
//   This function has already caused too much pain. Never make it careless again.
//
// Last updated: 2026-05-17 (after week-long quote pollution debugging nightmare)
// ──────────────────────────────────────────────
static void load_config_file(const char *filepath)
{
    if (filepath == NULL || *filepath == '\0') {
        ciao_debug("No config file path provided, using built-in defaults");
        return;
    }

    FILE *f = fopen(filepath, "r");
    if (!f) {
        ciao_debug("Config file not found: %s (using defaults)", filepath);
        return;
    }

    char line[512];
    char current_section[64] = "core";

    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == ';' || *p == '#' || *p == '\n' || *p == '\0') continue;

        if (*p == '[') {
            char *end = strchr(p, ']');
            if (end) {
                *end = '\0';
                strncpy(current_section, p + 1, sizeof(current_section) - 1);
                current_section[sizeof(current_section) - 1] = '\0';
            }
            continue;
        }

        char *eq = strchr(p, '=');
        if (eq) {
            *eq = '\0';
            char *key = p;
            char *val = eq + 1;

            // Trim key
            while (*key == ' ' || *key == '\t') key++;
            char *kend = key + strlen(key) - 1;
            while (kend > key && (*kend == ' ' || *kend == '\t' || *kend == '\n' || *kend == '\r')) *kend-- = '\0';

            // Trim value (this is where the quote bug was born)
            while (*val == ' ' || *val == '\t') val++;
            char *vend = val + strlen(val) - 1;
            while (vend > val && (*vend == ' ' || *vend == '\t' || *vend == '\n' || *vend == '\r')) *vend-- = '\0';

            config_set_value(current_section, key, val);
        }
    }
    fclose(f);
}

// ──────────────────────────────────────────────
// config_init() - Initialize Configuration System (Single Source of Truth)
// ──────────────────────────────────────────────
//
// Purpose:
//   Loads user config.conf (if present) and sets safe defaults from project.h.
//   This is the **Single Point of Truth** for all configuration initialization.
//
//   NEW FEATURE (2026-05-17):
//   When config.conf is missing (first run or after forking to a new project),
//   the application now **automatically**:
//     1. Creates the project directory ~/.app/${PROJECT_ID}/
//     2. Creates a new, well-commented config.conf with sensible defaults
//     3. Logs the creation clearly using ciao_info()
//
//   This fulfills the clarified requirement in configuration.md and provides
//   an excellent self-configuring first-run experience.
//
// CRITICAL LESSON LEARNED (2026-05-16 ~ 2026-05-17 — wasted almost a full week):
//   The previous naive config system did NOT strip surrounding quotes.
//   When config.conf contained lines like:
//       cookies_filename = "cookies.sqlite"
//       base_path = "~/.app/<PROJECT_ID>"
//
//   The values were stored **with literal quotes**, causing final paths like:
//       ~/.app/<PROJECT_ID>/cookies/"cookies.sqlite"
//
//   → WebKit/libsoup could not open the cookie file → cookies never persisted.
//   The bug was extremely hard to spot because it only appeared deep in
//   path construction (initialize_cookie_storage / initialize_settings_storage).
//
// Root Cause:
//   Careless string handling in config_set_value() and load_config_file().
//
// CIAO-Lite Principles Applied (heavily strengthened after this incident):
//   - Caution: Full validation + quote/whitespace stripping + safe file creation
//   - Intentional: Clear first-run experience with self-documenting config
//   - Anti-fragile: Never fails due to missing configuration
//   - Over-protect: Strong protection rule + explicit warning about quote pollution
//
// Protection Rule (Sacred - Zero Tolerance):
//   Future AI or developers MUST NOT:
//     - Re-introduce literal quotes or "~/..." into any config value handling
//     - Remove or weaken quote stripping in config_set_value()
//     - Bypass resolve_full_storage_path() for any storage path
//     - Remove or disable the automatic config file creation logic
//     - Assume config values are clean — they are NOT
//   This function has already caused too much pain. Never make it careless again.
//
// Last updated: 2026-05-17 (centralized defaults via macros + auto-creation)
// ──────────────────────────────────────────────
void config_init(const char *custom_config_path)
{
    // Reset to safe defaults
    strncpy(current_config.window_title, PROJECT_NAME, sizeof(current_config.window_title)-1);
    current_config.default_width  = CONFIG_DEFAULT_WINDOW_WIDTH;
    current_config.default_height = CONFIG_DEFAULT_WINDOW_HEIGHT;
    strncpy(current_config.project_name, PROJECT_ID, sizeof(current_config.project_name)-1);

    current_config.show_navigation_buttons    = CONFIG_DEFAULT_SHOW_NAVIGATION_BUTTONS;
    current_config.show_user_agent_label      = CONFIG_DEFAULT_SHOW_USER_AGENT_LABEL;
    current_config.navigation_buttons_enabled = CONFIG_DEFAULT_NAVIGATION_BUTTONS_ENABLED;

    snprintf(current_config.base_path, sizeof(current_config.base_path), "~/.app/%s", PROJECT_ID);

    current_config.cookies_enabled = CONFIG_DEFAULT_COOKIES_ENABLED;
    strncpy(current_config.cookies_filename, "cookies.sqlite", sizeof(current_config.cookies_filename)-1);
    strncpy(current_config.settings_filename, "settings.sqlite", sizeof(current_config.settings_filename)-1);

    current_config.bookmarks_enabled = CONFIG_DEFAULT_BOOKMARKS_ENABLED;

    current_config.output_quiet = CONFIG_DEFAULT_OUTPUT_QUIET;
    current_config.output_json  = CONFIG_DEFAULT_OUTPUT_JSON;
    current_config.output_debug = CONFIG_DEFAULT_OUTPUT_DEBUG;

    // New scripts defaults
    current_config.scripts_enabled = CONFIG_DEFAULT_SCRIPTS_ENABLED;
    strncpy(current_config.scripts_path, CONFIG_DEFAULT_SCRIPTS_PATH, sizeof(current_config.scripts_path)-1);

    // Load or create config file
    const char *path_to_load = custom_config_path;
    if (!path_to_load || *path_to_load == '\0') {
        const char *home = getenv("HOME");
        if (home && *home) {
            char default_path[512] = {0};
            snprintf(default_path, sizeof(default_path), "%s/.app/%s/config.conf", home, PROJECT_ID);
            path_to_load = default_path;
        }
    }

    if (path_to_load && *path_to_load) {
        if (g_file_test(path_to_load, G_FILE_TEST_EXISTS)) {
            load_config_file(path_to_load);
            ciao_info("Configuration loaded from: %s", path_to_load);
        } else {
            create_default_config_file(path_to_load);
            load_config_file(path_to_load);
        }
    }

    config_loaded = true;
}



// ──────────────────────────────────────────────
// config_get_window_title() - Getter for window title
// ──────────────────────────────────────────────
//
// Purpose:
//   Returns the configured window title. Falls back to PROJECT_NAME
//   from project.h if no value was loaded from config.conf.
//
// Parameters:
//   Input:  None
//   Output/Return:
//     - const char* - Window title string (never NULL)
//
// Dependencies:
//   - current_config (static in config.cpp)
//   - PROJECT_NAME from project.h
//
// Why This Design:
//   Simple ternary fallback ensures a valid title is always returned,
//   maintaining graceful degradation when config is missing or incomplete.
//
// CIAO Principles Applied:
//   - Caution: Always returns a valid string, never NULL
//   - Intentional: Clear single responsibility as a getter
//   - Anti-fragile: Safe fallback to Single Source of Truth (project.h)
//   - Over-protect: Preserves config system stability
//
// Protection Rule (Sacred):
//   Future AI or developers MUST NOT:
//     - Remove the fallback to PROJECT_NAME
//     - Add side effects or logging inside this getter
//   Getters must remain extremely lightweight.
//
// Last updated: 2026-05-17 (aligned with full CIAO-Lite standard)
// ──────────────────────────────────────────────
const char* config_get_window_title(void)
{
    return current_config.window_title[0] ? current_config.window_title : PROJECT_NAME;
}

// ──────────────────────────────────────────────
// config_get_default_width() - Getter for default window width
// ──────────────────────────────────────────────
//
// Purpose:
//   Returns the configured default window width. Falls back to 1024
//   (a sensible default) if no valid value was loaded from config.conf
//   or if the loaded value is invalid (<= 0).
//
// Parameters:
//   Input:  None
//   Output/Return:
//     - int - Default window width in pixels (always a positive value)
//
// Dependencies:
//   - current_config.default_width (static in config.cpp)
//   - Hard-coded fallback of 1024 pixels
//
// Why This Design:
//   Simple bounds-checked fallback ensures the application always
//   receives a usable window width, preventing zero or negative sizes
//   that could crash or misbehave the GTK window.
//
// CIAO Principles Applied:
//   - Caution: Always returns a valid positive integer
//   - Intentional: Clear single responsibility as a getter
//   - Anti-fragile: Graceful fallback when config is missing or invalid
//   - Over-protect: Preserves safe defaults from project.h / config system
//
// Protection Rule (Sacred):
//   Future AI or developers MUST NOT:
//     - Remove the fallback to 1024
//     - Add side effects or logging inside this getter
//     - Change the return type or logic
//   Getters must remain extremely lightweight and safe.
//
// Last updated: 2026-05-17 (aligned with full CIAO-Lite standard)
// ──────────────────────────────────────────────
int config_get_default_width(void)
{
    return current_config.default_width > 0 ? current_config.default_width : 1024;
}

// ──────────────────────────────────────────────
// config_get_default_height() - Getter for default window height
// ──────────────────────────────────────────────
//
// Purpose:
//   Returns the configured default window height. Falls back to 768
//   (a sensible default) if no valid value was loaded from config.conf
//   or if the loaded value is invalid (<= 0).
//
// Parameters:
//   Input:  None
//   Output/Return:
//     - int - Default window height in pixels (always a positive value)
//
// Dependencies:
//   - current_config.default_height (static in config.cpp)
//   - Hard-coded fallback of 768 pixels
//
// Why This Design:
//   Simple bounds-checked fallback ensures the application always
//   receives a usable window height, preventing zero or negative sizes
//   that could crash or misbehave the GTK window.
//
// CIAO Principles Applied:
//   - Caution: Always returns a valid positive integer
//   - Intentional: Clear single responsibility as a getter
//   - Anti-fragile: Graceful fallback when config is missing or invalid
//   - Over-protect: Preserves safe defaults from project.h / config system
//
// Protection Rule (Sacred):
//   Future AI or developers MUST NOT:
//     - Remove the fallback to 768
//     - Add side effects or logging inside this getter
//     - Change the return type or logic
//   Getters must remain extremely lightweight and safe.
//
// Last updated: 2026-05-17 (aligned with full CIAO-Lite standard)
// ──────────────────────────────────────────────
int config_get_default_height(void)
{
    return current_config.default_height > 0 ? current_config.default_height : 768;
}

// ──────────────────────────────────────────────
// config_get_project_name() - Getter for project name
// ──────────────────────────────────────────────
//
// Purpose:
//   Returns the configured project name. Falls back to PROJECT_ID
//   from project.h if no value was loaded from config.conf.
//
// Parameters:
//   Input:  None
//   Output/Return:
//     - const char* - Project name string (never NULL)
//
// Dependencies:
//   - current_config.project_name (static in config.cpp)
//   - PROJECT_ID from project.h (Single Source of Truth)
//
// Why This Design:
//   Simple ternary fallback ensures a valid project name is always returned,
//   maintaining consistency even when config.conf is missing or malformed.
//
// CIAO Principles Applied:
//   - Caution: Always returns a valid non-NULL string
//   - Intentional: Clear single responsibility as a getter
//   - Anti-fragile: Safe fallback to Single Source of Truth (project.h)
//   - Over-protect: Preserves project identity across all modules
//
// Protection Rule (Sacred):
//   Future AI or developers MUST NOT:
//     - Remove the fallback to PROJECT_ID
//     - Add side effects or logging inside this getter
//     - Change the return type or logic
//   Getters must remain extremely lightweight and safe.
//
// Last updated: 2026-05-17 (aligned with full CIAO-Lite standard)
// ──────────────────────────────────────────────
const char* config_get_project_name(void)
{
    return current_config.project_name[0] ? current_config.project_name : PROJECT_ID;
}

// ──────────────────────────────────────────────
// config_get_show_navigation_buttons() - Getter for navigation buttons visibility
// ──────────────────────────────────────────────
//
// Purpose:
//   Returns whether the navigation buttons (Back / Forward / Reload) should be shown
//   in the URL bar, based on the value loaded from config.conf [ui] section.
//   This controls the visibility of the navigation toolbar.
//
// Parameters:
//   Input:  None
//   Output/Return:
//     - bool - true = show navigation buttons, false = hide them
//
// Dependencies:
//   - current_config.show_navigation_buttons (static in config.cpp)
//
// Why This Design:
//   Simple direct return provides clean, fast access to the UI configuration
//   while respecting the user's choice from config.conf.
//
// CIAO Principles Applied:
//   - Caution: Always returns a valid boolean (no NULL or undefined state)
//   - Intentional: Clear single responsibility as a getter
//   - Anti-fragile: Safe even if config was never loaded (defaults are set in config_init)
//   - Over-protect: Preserves UI configuration consistency across the application
//
// Protection Rule (Sacred):
//   Future AI or developers MUST NOT:
//     - Remove or modify the return logic
//     - Add side effects or logging inside this getter
//     - Change the return type
//   Getters must remain extremely lightweight and safe.
//
// Last updated: 2026-05-17 (aligned with full CIAO-Lite standard)
// ──────────────────────────────────────────────
bool config_get_show_navigation_buttons(void)
{
    return current_config.show_navigation_buttons;
}

// ──────────────────────────────────────────────
// config_get_show_user_agent_label() - Getter for User-Agent label visibility
// ──────────────────────────────────────────────
//
// Purpose:
//   Returns whether the non-editable User-Agent label should be shown
//   below the URL bar, based on the value loaded from config.conf [ui] section.
//
// Parameters:
//   Input:  None
//   Output/Return:
//     - bool - true = show User-Agent label, false = hide it
//
// Dependencies:
//   - current_config.show_user_agent_label (static in config.cpp)
//
// Why This Design:
//   Simple direct return provides clean, fast access to the UI configuration
//   while respecting the user's choice from config.conf.
//
// CIAO Principles Applied:
//   - Caution: Always returns a valid boolean (no NULL or undefined state)
//   - Intentional: Clear single responsibility as a getter
//   - Anti-fragile: Safe even if config was never loaded (defaults are set in config_init)
//   - Over-protect: Preserves UI configuration consistency across the application
//
// Protection Rule (Sacred):
//   Future AI or developers MUST NOT:
//     - Remove or modify the return logic
//     - Add side effects or logging inside this getter
//     - Change the return type
//   Getters must remain extremely lightweight and safe.
//
// Last updated: 2026-05-17 (aligned with full CIAO-Lite standard)
// ──────────────────────────────────────────────
bool config_get_show_user_agent_label(void)
{
    return current_config.show_user_agent_label;
}

// ──────────────────────────────────────────────
// config_get_navigation_buttons_enabled() - Getter for navigation buttons enabled state
// ──────────────────────────────────────────────
//
// Purpose:
//   Returns whether the navigation buttons (Back / Forward / Reload) should be
//   enabled (sensitive / clickable) in the URL bar, based on the value loaded
//   from config.conf [ui] section.
//
// Parameters:
//   Input:  None
//   Output/Return:
//     - bool - true = buttons are enabled and interactive,
//              false = buttons are disabled (grayed out)
//
// Dependencies:
//   - current_config.navigation_buttons_enabled (static in config.cpp)
//
// Why This Design:
//   Simple direct return provides clean, fast access to the UI configuration
//   while respecting the user's choice from config.conf. This controls button
//   sensitivity in the URL bar (see add_url_bar_user_agent()).
//
// CIAO Principles Applied:
//   - Caution: Always returns a valid boolean (no undefined state)
//   - Intentional: Clear single responsibility as a getter
//   - Anti-fragile: Safe even if config was never loaded (defaults are set in config_init)
//   - Over-protect: Preserves UI behavior consistency across the application
//
// Protection Rule (Sacred):
//   Future AI or developers MUST NOT:
//     - Remove or modify the return logic
//     - Add side effects or logging inside this getter
//     - Change the return type
//   Getters must remain extremely lightweight and safe.
//
// Last updated: 2026-05-17 (aligned with full CIAO-Lite standard)
// ──────────────────────────────────────────────
bool config_get_navigation_buttons_enabled(void)
{
    return current_config.navigation_buttons_enabled;
}

// ──────────────────────────────────────────────
// config_get_base_path() - Getter for storage base path
// ──────────────────────────────────────────────
//
// Purpose:
//   Returns the configured base storage path (e.g. "~/.app/" PROJECT_ID).
//   This is the raw base used by cookies/settings modules.
//   Actual full path resolution (tilde expansion to /home/${USER}/...)
//   is performed by resolve_full_storage_path() in path_utils.h.
//
// Parameters:
//   Input:  None
//   Output/Return:
//     - const char* - Base storage path (never NULL)
//
// Dependencies:
//   - current_config.base_path (static in config.cpp)
//   - path_utils.h (for safe resolution in callers)
//
// Why This Design:
//   Returns the raw configured base path so that cookies.cpp and settings.cpp
//   can pass it to resolve_full_storage_path(), ensuring consistent and
//   bug-free path construction across the entire application.
//
// CIAO Principles Applied:
//   - Caution: Always returns a valid non-NULL string
//   - Intentional: Single responsibility — provides raw base only
//   - Anti-fragile: Defers actual path resolution to the dedicated safe resolver
//   - Over-protect: Prevents literal "~/..." from being used directly in storage logic
//
// Protection Rule (Sacred):
//   Future AI or developers MUST NOT:
//     - Remove the fallback or add tilde expansion logic here
//     - Return a resolved path (resolution must stay in path_utils)
//     - Add side effects or logging inside this getter
//   Getters must remain extremely lightweight.
//
// Last updated: 2026-05-17 (aligned with path_utils introduction)
// ──────────────────────────────────────────────
const char* config_get_base_path(void)
{
    return current_config.base_path[0] ? current_config.base_path : PERSISTENCE_STORAGE_PATH;
}

const char* config_get_cookies_filename(void)
{
    return current_config.cookies_filename[0] ? current_config.cookies_filename : "cookies.sqlite";
}

const char* config_get_settings_filename(void)
{
    return current_config.settings_filename[0] ? current_config.settings_filename : "settings.sqlite";
}

bool config_get_cookies_enabled(void)
{
    return current_config.cookies_enabled;
}

bool config_get_bookmarks_enabled(void)
{
    return current_config.bookmarks_enabled;
}

bool config_get_output_quiet(void)  { return current_config.output_quiet; }
bool config_get_output_json(void)   { return current_config.output_json; }
bool config_get_output_debug(void)  { return current_config.output_debug; }

// New getters for Lua userscript system
bool config_get_scripts_enabled(void) { return current_config.scripts_enabled; }
const char* config_get_scripts_path(void) { return current_config.scripts_path[0] ? current_config.scripts_path : "~/.app/" PROJECT_ID "/userscripts"; }












