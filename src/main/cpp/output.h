// output.h
// =========================================================================
// CIAO DEFENSIVE CODING STYLE - OUTPUT SYSTEM HEADER (GNOME APPLICATION)
// =========================================================================
//
// !!! THIS IS A GNOME/GTK APPLICATION !!!
// !!! DO NOT USE fprintf(), vfprintf(), printf(), perror() ANYWHERE !!!
// !!! ONLY USE g_print() and g_printerr() FOR ALL CONSOLE OUTPUT !!!
// 
// !!! DO NOT REMOVE, SHORTEN, COMMENT OUT, OR BYPASS ANY PART OF THIS HEADER !!!
// 
// Last aligned with SyncPrjs CIAO style + GNOME + JSON + DEBUG + "help": 2026-04-12
// =========================================================================

#ifndef OUTPUT_H
#define OUTPUT_H

#include <stdbool.h>
#include <stdarg.h>
#include <glib.h>

// =========================================================================
// Global configuration variables (Single Source of Truth)
// =========================================================================
extern int ciao_shutdown_seconds;
extern int ciao_bookmark_index;
extern bool ciao_quiet_mode;
extern bool ciao_json_mode;

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CIAO_LEVEL_INFO,
    CIAO_LEVEL_WARNING,
    CIAO_LEVEL_ERROR,
    CIAO_LEVEL_DEBUG
} CiaoOutputLevel;


// =========================================================================
// Core logging function - Single Source of Truth for ALL output
//
// Critical va_list Rule (Security & Correctness - CIAO enforced):
//   This function takes va_list BY VALUE (not "...").
//   The convenience wrappers (ciao_info, ciao_warn, etc.) are macros.
//   Each macro passes the caller's source file, then va_start() passes
//   the va_list to ciao_log. The component stored in the log file is the
//   base name of that source file.
//   Inside ciao_log we MUST use va_copy() before consuming it with
//   g_strdup_vprintf(), because the list is advanced.
//cd 
//   NEVER change this back to "..." without also updating all wrappers.
//   This prevents the "incorrectly passing a va_list" warning / UB.
//
// Protection Rule:
//   Do NOT bypass this function. Do NOT use stdio directly.
// =========================================================================
void ciao_log(CiaoOutputLevel level, const char *file, const char *format, va_list ap);

// =========================================================================
// Convenience functions (Single Point of Entry)
//
// Product code calls the macros. The macros pass __FILE__ so the dated
// log line can name the source file. Do not call the *_at functions.
// =========================================================================

void ciao_output_init(int argc, char **argv);

/* Read DEBUG into the debug gate. Does not scan argv and does not exit. */
void ciao_debug_from_env(void);

/* Resolved log directory. Caller frees. NULL when unresolved.
   Does not create the directory. Law: requirement-cpp-log-commands.md. */
gchar *ciao_log_directory(void);
void ciao_info_at(const char *file, const char *format, ...);
void ciao_warn_at(const char *file, const char *format, ...);
void ciao_error_at(const char *file, const char *format, ...);
void ciao_debug_at(const char *file, const char *format, ...);     // Controlled by DEBUG=1

#define ciao_info(...)  ciao_info_at(__FILE__, __VA_ARGS__)
#define ciao_warn(...)  ciao_warn_at(__FILE__, __VA_ARGS__)
#define ciao_error(...) ciao_error_at(__FILE__, __VA_ARGS__)
#define ciao_debug(...) ciao_debug_at(__FILE__, __VA_ARGS__)

void ciao_print_help(void);

#ifdef __cplusplus
}
#endif

#endif // OUTPUT_H