// log_commands.h
// =========================================================================
// CIAO DEFENSIVE CODING STYLE - LOG COMMAND HEADER (GNOME APPLICATION)
// =========================================================================
//
// !!! THIS IS A GNOME/GTK APPLICATION !!!
// !!! THE log VERB PRINTS WITH g_print / g_printerr, NOT ciao_log !!!
// !!! DO NOT CREATE THE LOG DIRECTORY FROM THESE COMMANDS !!!
//
// Law: requirement-cpp-log-commands.md
// Last aligned: 2026-10-04
// =========================================================================

#ifndef LOG_COMMANDS_H
#define LOG_COMMANDS_H

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    CIAO_LOG_OK = 0,
    CIAO_LOG_UNREADABLE = 1,
    CIAO_LOG_UNRESOLVED = 2,
    CIAO_LOG_IO = 3
};

typedef struct {
    int number;
    gchar *name;
    gchar *path;
} CiaoLogFile;

/* Regular *.log files, basename order, numbers starting at 1.
   Missing directory: empty array and CIAO_LOG_OK. Does not create it.
   Caller frees the array with g_ptr_array_free(array, TRUE). */
GPtrArray *ciao_log_list_files(int *status);

/* Raw bytes of one regular file. Does not follow a symlink.
   Caller frees the returned buffer. */
gchar *ciao_log_read_file(const char *path, int *status);

/* Delete regular *.log files only. Does not delete the directory.
   On failure, error_out receives one sentence the caller frees. */
int ciao_log_clear_files(int *status, gchar **error_out);

gboolean ciao_argv_has_log_verb(int argc, char **argv);

/* Prints the result and exits. Call only when the verb is present,
   after --json / JSON=1 have been applied, and before startup lines. */
void ciao_log_command_dispatch(int argc, char **argv);

#ifdef __cplusplus
}
#endif

#endif /* LOG_COMMANDS_H */
