// log_view_html.h
// =========================================================================
// CIAO DEFENSIVE CODING STYLE - UI VIEW FOR AN APP LOG
// =========================================================================
//
// Law: requirement-cpp-log-commands.md
// Term: ui-view-for-app-log
// The returned document is HTML for the main web view. It does not write
// the log file. The non-UI log show command does not call this function.
// Last aligned: 2026-10-06
// =========================================================================

#ifndef LOG_VIEW_HTML_H
#define LOG_VIEW_HTML_H

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* One HTML document for the raw bytes of one log file.
   title is the visible heading, usually the file basename.
   The Source cell omits a trailing .cpp or .c. The file bytes stay as stored.
   Caller frees the returned buffer. A NULL title or raw becomes empty text. */
gchar *log_view_html_document(const char *title, const char *raw);

#ifdef __cplusplus
}
#endif

#endif /* LOG_VIEW_HTML_H */
