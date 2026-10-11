// menu_log.h
// =========================================================================
// CIAO DEFENSIVE CODING STYLE - LOG MENU HEADER (GNOME + GTK)
// =========================================================================
//
// !!! THIS IS A GNOME/GTK APPLICATION !!!
// Law: requirement-cpp-log-commands.md
// The Log item is added only from build_main_menu_bar().
// Choosing a file loads the HTML log view into the main web view.
// Last aligned: 2026-10-05
// =========================================================================

#ifndef MENU_LOG_H
#define MENU_LOG_H

#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

GtkWidget *add_log_menu(GtkWindow *parent_window, WebKitWebView *web_view);

/* Load the same HTML log table the Log menu uses.
   path is the file whose URI becomes the document base.
   title is the heading, usually the basename. content is the file bytes. */
void log_view_load_in_web_view(WebKitWebView *web_view,
                               const char *path,
                               const char *title,
                               const char *content);

#endif /* MENU_LOG_H */
