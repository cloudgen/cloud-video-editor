// log_format.h
// =========================================================================
// CIAO DEFENSIVE CODING STYLE - LOG FILE FORMAT CHECKER
// =========================================================================
//
// Law: requirement-cpp-log-format.md
// The checker reads bytes. It does not write a log file and it does not
// call ciao_*. The logcheck verb is the test-purpose command.
// Last aligned: 2026-10-06
// =========================================================================

#ifndef LOG_FORMAT_H
#define LOG_FORMAT_H

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* TRUE when text is a local file request whose name ends in .log.
   file: URIs are decoded. An absolute path is accepted as given.
   http and https stay FALSE. Caller frees *path_out. */
gboolean log_format_request_path(const char *text, gchar **path_out);

/* TRUE when every non-empty line is a stored line and at least one
   stored line is present. A trailing newline is not a line.
   On FALSE, reason_out receives one sentence the caller frees.
   reason_out may be NULL. */
gboolean log_format_content_follows(const char *raw, gchar **reason_out);

gboolean ciao_argv_has_logcheck_verb(int argc, char **argv);

/* Prints the result and exits. Call only when the verb is present,
   after --json / JSON=1 have been applied, and before gtk_init(). */
void ciao_logcheck_command_dispatch(int argc, char **argv);

#ifdef __cplusplus
}
#endif

#endif /* LOG_FORMAT_H */
