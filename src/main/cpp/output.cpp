// output.cpp
// =========================================================================
// CIAO DEFENSIVE CODING STYLE - OUTPUT SYSTEM IMPLEMENTATION (GNOME + JSON + DEBUG)
// =========================================================================
//
// !!! THIS IS A GNOME/GTK APPLICATION !!!
// !!! ONLY g_print() / g_printerr() ARE ALLOWED !!!
// !!! DO NOT REMOVE OR MODIFY ANY HEADER OR FUNCTION COMMENT BLOCK !!! 
//
// Last aligned with SyncPrjs CIAO style + GNOME + JSON + "help" + DEBUG: 2026-04-12
// =========================================================================

#include "output.h"
#include "project.h"
#include "config.h"
#include "log_commands.h"
#include "log_format.h"
#include "lua_script_commands.h"
#include <glib.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

bool ciao_quiet_mode = false;
bool ciao_json_mode = false;
static bool debug_env_enabled = false;

// =========================================================================
// Global for auto-shutdown (Single Source of Truth)
// =========================================================================
int ciao_shutdown_seconds = 0;
int ciao_bookmark_index = 0;        // New: default = first bookmark (0)

// Dated log file. Resolved once. NULL means the directory is unresolved.
static gchar *logger_dir = NULL;
static gboolean logger_dir_ready = FALSE;
static gboolean logger_in_append = FALSE;
static gboolean logger_dir_error_reported = FALSE;
static gboolean logger_file_error_reported = FALSE;

// ──────────────────────────────────────────────
// logger_format_time() - Local civil time for the stored line and file name
// ──────────────────────────────────────────────
//
// Purpose:
//   Fill the stored-line clock and the YYYYMMDD file stamp from local time.
//
// Parameters:
//   Input: buffer lengths for the civil clock and the datestamp
//   Output: civil "YYYY-MM-DD HH:MM:SS", stamp "YYYYMMDD"
//   Clock failure writes 1970-01-01 00:00:00 and 19700101.
//
// Dependencies:
//   - Functions called: time(), localtime_r(), strftime(), g_strlcpy()
//   - Headers required: time.h, glib.h
//
// Why This Design:
//   One clock feeds both the line and the file name, so they cannot disagree.
//
// CIAO Principles Applied:
//   - Caution: fixed buffers, failure falls back to the epoch stamp
//   - Anti-fragile: localtime_r failure does not skip the log line
//
// Protection Rule:
//   Do not invent another timestamp shape for the stored line.
// ──────────────────────────────────────────────
static void logger_format_time(char *civil, size_t civil_len, char *stamp, size_t stamp_len)
{
    time_t now = time(NULL);
    struct tm tm_buf;
    struct tm *tm_ptr = NULL;

    if (now != (time_t)-1) {
        tm_ptr = localtime_r(&now, &tm_buf);
    }

    if (tm_ptr == NULL ||
        strftime(civil, civil_len, "%Y-%m-%d %H:%M:%S", tm_ptr) == 0) {
        g_strlcpy(civil, "1970-01-01 00:00:00", civil_len);
    }
    if (tm_ptr == NULL ||
        strftime(stamp, stamp_len, "%Y%m%d", tm_ptr) == 0) {
        g_strlcpy(stamp, "19700101", stamp_len);
    }
}

// ──────────────────────────────────────────────
// logger_resolve_dir() - Choose the one log directory for this process
// ──────────────────────────────────────────────
//
// Purpose:
//   Root or sudo (effective uid 0) uses /var/<PROJECT_ID>/log.
//   Every other user uses $HOME/.app/<PROJECT_ID>/log.
//   An empty HOME leaves the directory unresolved. No path is invented.
//
// Parameters:
//   Input: none (effective uid and HOME)
//   Output/Return: TRUE when logger_dir is set
//
// Dependencies:
//   - Functions called: geteuid(), getenv(), g_build_filename()
//   - Headers required: unistd.h, stdlib.h, glib.h, project.h
//
// Why This Design:
//   The directory is chosen once. Printing it does not create it.
//
// CIAO Principles Applied:
//   - Caution: HOME is taken only from the environment
//   - Intentional: effective uid 0 is the only root/sudo test
//   - Over-protect: no getpwuid fallback and no PREFIX/--home on a normal run
//
// Protection Rule:
//   Do not point this directory at an install bin or at src/main/cpp.
// ──────────────────────────────────────────────
static gboolean logger_resolve_dir(void)
{
    if (logger_dir_ready) {
        return logger_dir != NULL;
    }
    logger_dir_ready = TRUE;

    if (geteuid() == 0) {
        logger_dir = g_build_filename(LOGGER_ROOT_PARENT, PROJECT_ID, LOGGER_DIR_NAME, NULL);
        return logger_dir != NULL;
    }

    const char *home = getenv("HOME");
    if (home == NULL || home[0] == '\0') {
        return FALSE;
    }

    logger_dir = g_build_filename(home, LOGGER_USER_PARENT, PROJECT_ID, LOGGER_DIR_NAME, NULL);
    return logger_dir != NULL;
}

gchar *ciao_log_directory(void)
{
    if (!logger_resolve_dir() || logger_dir == NULL) {
        return NULL;
    }
    return g_strdup(logger_dir);
}

// ──────────────────────────────────────────────
// logger_append_line() - Append one stored line to the dated log file
// ──────────────────────────────────────────────
//
// Purpose:
//   Write one line for every ciao_log event. Quiet and JSON do not drop it.
//   The log directory is created here, on the first append, and not before.
//
// Parameters:
//   Input:
//     - level   : CiaoOutputLevel
//     - file    : source path from the call site, or NULL
//     - message : formatted text, or NULL
//   Output/Return: none
//
// Dependencies:
//   - Functions called: logger_component_token(), logger_resolve_dir(),
//     logger_format_time(), g_mkdir_with_parents(), open(), write(), close(),
//     g_printerr()
//   - Headers required: fcntl.h, unistd.h, errno.h, glib.h, project.h
//
// Why This Design:
//   Terminal text stays on g_print / g_printerr. This function is the file
//   sink only. A write failure is reported once and does not call ciao_*,
//   so a full disk cannot recurse back into this function.
//
// CIAO Principles Applied:
//   - Caution: short writes and EINTR are retried; mkstemp is not used
//   - Anti-fragile: unresolved directory skips the file and continues
//   - Over-protect: re-entry flag stops a nested append
//
// Protection Rule:
//   Do not add a second logging API. Product code keeps calling ciao_*.
//   The component is the caller's source file name, not a fixed token.
// ──────────────────────────────────────────────
static gchar *logger_component_token(const char *file)
{
    if (file == NULL || file[0] == '\0') {
        return g_strdup("");
    }

    gchar *base = g_path_get_basename(file);
    if (base == NULL || base[0] == '\0' ||
        strcmp(base, ".") == 0 || strcmp(base, "..") == 0 ||
        strchr(base, '\n') != NULL || strchr(base, '\r') != NULL) {
        g_free(base);
        return g_strdup("");
    }

    gchar *token = g_strdup_printf(" @%s", base);
    g_free(base);
    return token;
}

static void logger_append_line(CiaoOutputLevel level, const char *file, const char *message)
{
    if (logger_in_append) {
        return;
    }
    logger_in_append = TRUE;

    if (!logger_resolve_dir()) {
        logger_in_append = FALSE;
        return;
    }

    char civil[32];
    char stamp[16];
    logger_format_time(civil, sizeof civil, stamp, sizeof stamp);

    if (g_mkdir_with_parents(logger_dir, 0700) != 0) {
        if (!logger_dir_error_reported) {
            g_printerr("[ERROR] cannot create log directory %s\n", logger_dir);
            logger_dir_error_reported = TRUE;
        }
        logger_in_append = FALSE;
        return;
    }

    gchar *name = g_strdup_printf("%s-%s%s", PROJECT_ID, stamp, LOGGER_FILE_EXT);
    gchar *path = g_build_filename(logger_dir, name, NULL);
    g_free(name);
    if (path == NULL) {
        logger_in_append = FALSE;
        return;
    }

    const char *level_name =
        (level == CIAO_LEVEL_WARNING) ? "WARNING" :
        (level == CIAO_LEVEL_ERROR) ? "ERROR" :
        (level == CIAO_LEVEL_DEBUG) ? "DEBUG" : "INFO";

    gchar *component = logger_component_token(file);
    gchar *line = g_strdup_printf("[%s] pid:%ld [%s]%s :] %s\n",
                                  civil,
                                  (long)getpid(),
                                  level_name,
                                  (component != NULL) ? component : "",
                                  (message != NULL) ? message : "(null)");
    g_free(component);

    if (line != NULL) {
        int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0600);
        if (fd < 0) {
            if (!logger_file_error_reported) {
                g_printerr("[ERROR] cannot open log file %s\n", path);
                logger_file_error_reported = TRUE;
            }
        } else {
            const char *cursor = line;
            size_t left = strlen(line);
            while (left > 0) {
                ssize_t wrote = write(fd, cursor, left);
                if (wrote < 0) {
                    if (errno == EINTR) {
                        continue;
                    }
                    if (!logger_file_error_reported) {
                        g_printerr("[ERROR] cannot write log file %s\n", path);
                        logger_file_error_reported = TRUE;
                    }
                    break;
                }
                cursor += (size_t)wrote;
                left -= (size_t)wrote;
            }
            if (close(fd) != 0 && !logger_file_error_reported) {
                g_printerr("[ERROR] cannot close log file %s\n", path);
                logger_file_error_reported = TRUE;
            }
        }
        g_free(line);
    }

    g_free(path);
    logger_in_append = FALSE;
}

// ──────────────────────────────────────────────
// logger_announce() - DEBUG lines for the log directory and dated file
// ──────────────────────────────────────────────
//
// Purpose:
//   When DEBUG is set, print the resolved directory and the dated file.
//   An unresolved directory prints "logger folder unresolved".
//   This call does not create the directory by itself.
//
// Parameters:
//   Input: none
//   Output/Return: none (ciao_debug)
//
// Dependencies:
//   - Functions called: logger_resolve_dir(), logger_format_time(), ciao_debug()
//   - Headers required: project.h, output.h
//
// Why This Design:
//   The announcement goes through ciao_debug, so quiet hides the terminal
//   line and the same event is still stored when DEBUG is on.
//
// CIAO Principles Applied:
//   - Intentional: the two lines are the folder and the file, in that order
//
// Protection Rule:
//   Do not create the directory from this function.
// ──────────────────────────────────────────────
static void logger_announce(void)
{
    if (!debug_env_enabled) {
        return;
    }
    if (!logger_resolve_dir()) {
        ciao_debug("logger folder unresolved");
        return;
    }

    char civil[32];
    char stamp[16];
    logger_format_time(civil, sizeof civil, stamp, sizeof stamp);
    (void)civil;

    gchar *name = g_strdup_printf("%s-%s%s", PROJECT_ID, stamp, LOGGER_FILE_EXT);
    gchar *path = g_build_filename(logger_dir, name, NULL);
    g_free(name);

    ciao_debug("logger folder %s", logger_dir);
    if (path != NULL) {
        ciao_debug("log file %s", path);
        g_free(path);
    }
}

// ──────────────────────────────────────────────
// ciao_output_init() - General Purpose Requirement: Initialize Output System
// ──────────────────────────────────────────────
//
// Purpose:
//   Must be called very early in main() to parse command line and environment.
//   This is the Single Point of Truth for application output configuration.
//
// Parameters:
//   Input:
//     - argc : int - Number of command line arguments
//     - argv : char** - Command line argument array
//   Output/Return: None (configures global output flags)
//
// Dependencies:
//   - Functions called: getenv(), atoi(), strncmp(), strcmp(), ciao_info(), ciao_debug(), ciao_print_help()
//   - Headers required: output.h, project.h, stdlib.h, string.h
//   - External: Parses --quiet, --json, --shutdown, --bookmark and environment variables
//
// New Features (2026-04-30):
//   - JSON=1 / QUIET=1 environment variables
//   - --bookmark=N / BOOKMARK=N  (0 = Start Page, 1 = Contacts, etc.)
//
// New Feature (2026-04-30):
//   --shutdown=N  or  SHUTDOWN=N  → auto exit after N seconds
//   Value 0 (or omitted) explicitly means "never shutdown".
//
// CIAO Principles Applied:
//   - Caution: Careful parsing with fallbacks and bounds checking
//   - Intentional: Early initialization and clear feature documentation
//   - Anti-fragile: Graceful handling of missing/invalid flags
//   - Over-protect: All output configuration centralized here
//
// Protection Rule (Sacred):
//   Future AI must NOT remove any existing flag/env logic.
//
// Last updated: Full CIAO expansion with Parameters & Dependencies (2026-05-08)
// ──────────────────────────────────────────────
void ciao_debug_from_env(void)
{
    const char *debug_env = getenv("DEBUG");
    debug_env_enabled = (debug_env != NULL && *debug_env != '\0');
}

void ciao_output_init(int argc, char **argv)
{
    ciao_quiet_mode = false;
    ciao_json_mode = false;
    ciao_shutdown_seconds = 0;
    ciao_bookmark_index = 0;

    // Check DEBUG environment first
    const char *debug_env = getenv("DEBUG");
    debug_env_enabled = (debug_env != NULL && *debug_env != '\0');

    // Parse command line (highest priority)
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--quiet") == 0) {
            ciao_quiet_mode = true;
        } 
        else if (strcmp(argv[i], "--json") == 0) {
            ciao_json_mode = true;
        } 
        else if (strncmp(argv[i], "--shutdown=", 11) == 0) {
            int val = atoi(argv[i] + 11);
            if (val > 0) ciao_shutdown_seconds = val;
        }
        else if (strncmp(argv[i], "--bookmark=", 11) == 0) {
            ciao_bookmark_index = atoi(argv[i] + 11);
            if (ciao_bookmark_index < 0) ciao_bookmark_index = 0;
        }
        else if (strcmp(argv[i], "help") == 0) {
            ciao_print_help();
            exit(0);
        }
    }

    // Environment variables
    if (!ciao_quiet_mode) {
        const char *q = getenv("QUIET");
        if (q && (atoi(q) == 1 || strcmp(q, "1") == 0))
            ciao_quiet_mode = true;
    }
    if (!ciao_json_mode) {
        const char *j = getenv("JSON");
        if (j && (atoi(j) == 1 || strcmp(j, "1") == 0))
            ciao_json_mode = true;
    }

    // Respect config.conf [output] section
    if (!ciao_quiet_mode) ciao_quiet_mode = config_get_output_quiet();
    if (!ciao_json_mode)  ciao_json_mode  = config_get_output_json();
    if (!debug_env_enabled) debug_env_enabled = config_get_output_debug();

    // === NEW ENFORCEMENT RULE ===
    // When JSON mode is active, force quiet = true to prevent text pollution.
    // The log verb must not emit this debug line: ciao_debug stores a line
    // and would create the log directory before the verb runs.
    if (ciao_json_mode) {
        ciao_quiet_mode = true;
        if (!ciao_argv_has_log_verb(argc, argv) &&
            !ciao_argv_has_lua_script_verb(argc, argv) &&
            !ciao_argv_has_logcheck_verb(argc, argv)) {
            ciao_debug("JSON mode active → quiet mode forced ON");
        }
    }

    if (ciao_shutdown_seconds == 0) {
        const char *s = getenv("SHUTDOWN");
        if (s) ciao_shutdown_seconds = atoi(s);
        if (ciao_shutdown_seconds < 0) ciao_shutdown_seconds = 0;
    }

    if (ciao_bookmark_index == 0) {
        const char *b = getenv("BOOKMARK");
        if (b) {
            ciao_bookmark_index = atoi(b);
            if (ciao_bookmark_index < 0) ciao_bookmark_index = 0;
        }
    }

    // log and luascript print their own payload and stop before startup
    // lines and before gtk_init(). requirement-cpp-log-commands.md
    // requirement-cpp-lua-script-commands.md
    if (ciao_argv_has_logcheck_verb(argc, argv)) {
        ciao_logcheck_command_dispatch(argc, argv);
    }
    if (ciao_argv_has_log_verb(argc, argv)) {
        ciao_log_command_dispatch(argc, argv);
    }
    if (ciao_argv_has_lua_script_verb(argc, argv)) {
        ciao_lua_script_command_dispatch(argc, argv);
    }

    if (ciao_shutdown_seconds > 0)
        ciao_info("Auto-shutdown enabled: will exit after %d seconds", ciao_shutdown_seconds);

    if (ciao_bookmark_index > 0)
        ciao_info("Will open bookmark index %d on startup", ciao_bookmark_index);

    if (!ciao_quiet_mode && !ciao_json_mode) {
        ciao_info("%s Output System initialized (GNOME compliant)", PROJECT_NAME);
        if (debug_env_enabled) {
            ciao_info("Debug mode enabled via DEBUG=1 environment variable");
        }
    } else if (ciao_json_mode) {
        ciao_info("Output system initialized in JSON mode");
    }

    logger_announce();
}

// ──────────────────────────────────────────────
// ciao_log() - Core Single Source of Output Function
// ──────────────────────────────────────────────
//
// Purpose:
//   ALL output in the project must pass through this function.
//   This is the Single Source of Truth for all console output.
//
// Parameters:
//   Input:
//     - level  : CiaoOutputLevel - Message severity
//     - file   : const char* - source path of the ciao_* call
//     - format : const char* - printf-style format string
//     - ap     : va_list (by value) - Variable arguments
//   Output/Return: None (prints to stdout/stderr or JSON)
//
// Dependencies:
//   - Functions called: g_strdup_vprintf(), g_print(), g_printerr(), time(), va_copy(), va_end()
//   - Headers required: glib.h, stdarg.h, time.h, output.h
//
// Critical va_list Rule (Security & Correctness):
//   This function receives a va_list BY VALUE (not by pointer, not via ...).
//   Inside this function we MUST use va_copy() before consuming the list
//   because g_strdup_vprintf() (and similar GLib functions) will advance
//   the va_list, leaving the original caller’s list in an indeterminate state.
//
//   NEVER change the signature to accept "..." again and call va_start here.
//   NEVER pass a va_list from a convenience wrapper without va_copy().
//
//   Violating this causes:
//     - "incorrectly passing a va_list" static analysis warnings
//     - Undefined behavior on many ABIs (especially x86_64 and aarch64)
//     - Potential format-string related security issues
//
// Why This Must Stay Strong:
//   Prevents fragmentation of output logic (common failure in previous versions).
//   Supports both human-readable and JSON structured output.
//   Maintains strict GNOME compliance (only g_print / g_printerr allowed).
//
// Protection Rule (Sacred):
//   Do NOT bypass this function.
//   Do NOT replace g_print/g_printerr with stdio functions.
//   Do NOT modify the va_list handling logic without updating this comment.
//   This is the heart of the CIAO Single Source of Output principle.
//
// Last updated: Full CIAO expansion with Parameters & Dependencies (2026-05-08)
// ──────────────────────────────────────────────
void ciao_log(CiaoOutputLevel level, const char *file, const char *format, va_list ap)   // Changed: va_list (by value), no ...
{
    // Make a copy because g_strdup_vprintf / g_vasprintf may consume the list
    va_list ap_copy;
    va_copy(ap_copy, ap);

    gchar *message = g_strdup_vprintf(format, ap_copy);
    va_end(ap_copy);

    // Quiet and JSON change the terminal only. The dated file still gets the line.
    logger_append_line(level, file, message);

    if (ciao_quiet_mode && level != CIAO_LEVEL_ERROR) {
        g_free(message);
        return;
    }

    time_t now = time(NULL);

    if (ciao_json_mode) {
        g_print("{\"timestamp\":%ld,\"level\":\"%s\",\"message\":\"%s\"}\n",
                (long)now,
                (level == CIAO_LEVEL_INFO) ? "INFO" :
                (level == CIAO_LEVEL_WARNING) ? "WARN" :
                (level == CIAO_LEVEL_ERROR) ? "ERROR" : "DEBUG",
                message ? message : "(null)");
    } else {
        switch (level) {
            case CIAO_LEVEL_INFO:
                g_print("[INFO] %s\n", message ? message : "(null)");
                break;
            case CIAO_LEVEL_WARNING:
                g_printerr("[WARN] %s\n", message ? message : "(null)");
                break;
            case CIAO_LEVEL_ERROR:
                g_printerr("[ERROR] %s\n", message ? message : "(null)");
                break;
            case CIAO_LEVEL_DEBUG:
                if (debug_env_enabled) {
                    g_print("[DEBUG] %s\n", message ? message : "(null)");
                }
                break;
        }
    }

    g_free(message);
}

// ──────────────────────────────────────────────
// ciao_info() - Convenience Function: ciao_info
// ──────────────────────────────────────────────
//
// Purpose:
//   Standard information messages
//
// Parameters:
//   Input:
//     - format : const char* - printf-style format string
//     - ...    : Variable arguments
//   Output/Return: None
//
// Dependencies:
//   - Functions called: ciao_log()
//   - Headers required: output.h
//
// Protection:
//   Routes through ciao_log() to maintain Single Source of Output
//
// Last updated: Full CIAO expansion (2026-05-08)
// ──────────────────────────────────────────────
void ciao_info_at(const char *file, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    ciao_log(CIAO_LEVEL_INFO, file, format, args);
    va_end(args);
}

// ──────────────────────────────────────────────
// ciao_warn() - Convenience Function: ciao_warn
// ──────────────────────────────────────────────
//
// Purpose:
//   Warning messages (stderr)
//
// Parameters:
//   Input:
//     - format : const char* - printf-style format string
//     - ...    : Variable arguments
//   Output/Return: None
//
// Dependencies:
//   - Functions called: ciao_log()
//   - Headers required: output.h
//
// Protection:
//   Routes through ciao_log()
//
// Last updated: Full CIAO expansion (2026-05-08)
// ──────────────────────────────────────────────
void ciao_warn_at(const char *file, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    ciao_log(CIAO_LEVEL_WARNING, file, format, args);
    va_end(args);
}

// ──────────────────────────────────────────────
// ciao_error() - Convenience Function: ciao_error
// ──────────────────────────────────────────────
//
// Purpose:
//   Error messages (always shown, even in --quiet mode)
//
// Parameters:
//   Input:
//     - format : const char* - printf-style format string
//     - ...    : Variable arguments
//   Output/Return: None
//
// Dependencies:
//   - Functions called: ciao_log()
//   - Headers required: output.h
//
// Protection:
//   Critical path - must never be suppressed
//
// Last updated: Full CIAO expansion (2026-05-08)
// ──────────────────────────────────────────────
void ciao_error_at(const char *file, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    ciao_log(CIAO_LEVEL_ERROR, file, format, args);
    va_end(args);
}

// ──────────────────────────────────────────────
// ciao_debug() - Debug Function: ciao_debug
// ──────────────────────────────────────────────
//
// Purpose:
//   Developer-only debug output.
//
// Parameters:
//   Input:
//     - format : const char* - printf-style format string
//     - ...    : Variable arguments
//   Output/Return: None
//
// Dependencies:
//   - Functions called: ciao_log()
//   - Headers required: output.h
//
// Rules:
//   - Only active when DEBUG=1 (or any non-empty value) in environment.
//   - Still respects --quiet mode.
//   - Does NOT appear in JSON mode unless level allows.
//
// Why This Exists:
//   Prevents debug spam in normal runs while allowing deep inspection when needed.
//
// Last updated: Full CIAO expansion (2026-05-08)
// ──────────────────────────────────────────────
void ciao_debug_at(const char *file, const char *format, ...)
{
    if (!debug_env_enabled) return;

    // Quiet still hides the terminal line inside ciao_log. The file keeps the line.
    va_list args;
    va_start(args, format);
    ciao_log(CIAO_LEVEL_DEBUG, file, format, args);
    va_end(args);
}

// ──────────────────────────────────────────────
// ciao_print_help() - Help Function: ciao_print_help
// ──────────────────────────────────────────────
//
// Purpose:
//   Show usage when user types "help"
//
// Parameters:
//   Input: None
//   Output/Return: None
//
// Dependencies:
//   - Functions called: g_print()
//   - Headers required: glib.h
//
// Protection:
//   Uses g_print only (GNOME compliant)
//
// Last updated: Full CIAO expansion (2026-05-08)
// ──────────────────────────────────────────────
void ciao_print_help(void)
{
    g_print("Usage: %s [help] [options]\n\n", PROJECT_NAME);
    g_print("Commands:\n");
    g_print("  help                 Show this help message\n");
    g_print("  log list             Number the log files\n");
    g_print("  log show N           Display log file number N\n");
    g_print("  log folder           Display the log folder\n");
    g_print("  log clear            Delete the log files\n");
    g_print("  luascript folder     Display the lua script folder\n");
    g_print("  luascript list       Number the lua scripts\n");
    g_print("  luascript view N     Display lua script number N\n");
    g_print("  luascript edit N     Replace lua script number N from stdin\n");
    g_print("  luascript new NAME   Create a lua script\n\n");
    g_print("Tests:\n");
    g_print("  logcheck --file PATH Check one log file\n");
    g_print("  logcheck --home DIR --log-datestamp YYYYMMDD\n");
    g_print("                       Check that dated file\n");
    g_print("  logcheck --prefix DIR --log-datestamp YYYYMMDD\n");
    g_print("                       Check the root-shaped dated file\n\n");
    g_print("Options:\n");
    g_print("  --config <path>      Load this config file\n");
    g_print("  --quiet              Suppress non-error messages\n");
    g_print("  --json               Output in structured JSON format (forces quiet)\n");
    g_print("  --shutdown=N         Exit after N seconds (0 disables)\n");
    g_print("  --bookmark=N         Open bookmark index N on startup\n\n");
    g_print("Log file:\n");
    g_print("  $HOME/%s/%s/%s/%s-YYYYMMDD%s\n",
            LOGGER_USER_PARENT, PROJECT_ID, LOGGER_DIR_NAME, PROJECT_ID, LOGGER_FILE_EXT);
    g_print("  %s/%s/%s/%s-YYYYMMDD%s when the effective user is root\n\n",
            LOGGER_ROOT_PARENT, PROJECT_ID, LOGGER_DIR_NAME, PROJECT_ID, LOGGER_FILE_EXT);
    g_print("Environment Variables:\n");
    g_print("  DEBUG=1              Enable ciao_debug() output\n");
    g_print("  QUIET=1              Same as --quiet\n");
    g_print("  JSON=1               Same as --json\n");
    g_print("  SHUTDOWN=N           Same as --shutdown=N\n");
    g_print("  BOOKMARK=N           Same as --bookmark=N\n\n");
    g_print("Examples:\n");
    g_print("  DEBUG=1 %s\n", PROJECT_NAME);
    g_print("  %s --config config.conf\n", PROJECT_NAME);
    g_print("  %s --quiet\n", PROJECT_NAME);
    g_print("  %s --json\n", PROJECT_NAME);
    g_print("  %s --shutdown=30\n", PROJECT_NAME);
    g_print("  %s --bookmark=0\n", PROJECT_NAME);
    g_print("  %s log list\n", PROJECT_NAME);
    g_print("  %s log show 1 --json\n", PROJECT_NAME);
    g_print("  %s logcheck --home /tmp --log-datestamp 20261003\n", PROJECT_NAME);
    g_print("  %s luascript folder\n", PROJECT_ID);
    g_print("  %s luascript new gemini-note.lua\n", PROJECT_ID);
}

