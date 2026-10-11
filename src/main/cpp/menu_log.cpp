// menu_log.cpp
// =========================================================================
// CIAO DEFENSIVE CODING STYLE - LOG MENU (GNOME + GTK)
// =========================================================================
//
// !!! THIS IS A GNOME/GTK APPLICATION !!!
// Law: requirement-cpp-log-commands.md
// These handlers do not call ciao_*. A ciao_* line would create the log
// directory, and listing or opening the folder must not create it.
// Log folder opens the desktop file manager at that path.
// Choosing a log file loads the HTML table into the main web view.
// The address bar uses log_view_load_in_web_view() for the same table.
// Last aligned: 2026-10-06
// =========================================================================

#include "menu_log.h"
#include "log_commands.h"
#include "log_view_html.h"
#include "output.h"

#include <gtk/gtk.h>

static void message_dialog(GtkWindow *parent, GtkMessageType type,
                           const char *title, const char *body)
{
    GtkWidget *dialog = gtk_message_dialog_new(parent,
                                               static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
                                               type,
                                               GTK_BUTTONS_CLOSE,
                                               "%s",
                                               body != NULL ? body : "");
    gtk_window_set_title(GTK_WINDOW(dialog), title != NULL ? title : "Log");
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

void log_view_load_in_web_view(WebKitWebView *web_view,
                               const char *path,
                               const char *title,
                               const char *content)
{
    if (!WEBKIT_IS_WEB_VIEW(web_view)) {
        return;
    }
    const char *heading = (title != NULL && title[0] != '\0') ? title : "Log";
    gchar *html = log_view_html_document(heading, content != NULL ? content : "");
    gchar *base_uri = NULL;
    if (path != NULL && path[0] != '\0') {
        base_uri = g_filename_to_uri(path, NULL, NULL);
    }
    webkit_web_view_load_html(web_view, html != NULL ? html : "", base_uri);
    g_free(base_uri);
    g_free(html);
}

static void on_log_file_activate(GtkMenuItem *item, gpointer user_data)
{
    GtkWindow *parent = GTK_WINDOW(user_data);
    const char *path = (const char *)g_object_get_data(G_OBJECT(item), "log-path");
    const char *name = (const char *)g_object_get_data(G_OBJECT(item), "log-name");
    const char *title = (name != NULL && name[0] != '\0') ? name : "Log";

    int status = CIAO_LOG_OK;
    gchar *content = ciao_log_read_file(path, &status);
    if (content == NULL || status != CIAO_LOG_OK) {
        g_free(content);
        gchar *msg = g_strdup_printf("cannot read log file %s", title);
        message_dialog(parent, GTK_MESSAGE_ERROR, "Log", msg);
        g_free(msg);
        return;
    }

    WebKitWebView *web_view = (WebKitWebView *)g_object_get_data(G_OBJECT(item), "log-webview");
    if (!WEBKIT_IS_WEB_VIEW(web_view)) {
        g_free(content);
        message_dialog(parent, GTK_MESSAGE_ERROR, "Log", "Cannot show log");
        return;
    }

    log_view_load_in_web_view(web_view, path, title, content);
    g_free(content);
}

static void rebuild_list_submenu(GtkWidget *menu, GtkWindow *parent)
{
    GList *children = gtk_container_get_children(GTK_CONTAINER(menu));
    for (GList *node = children; node != NULL; node = node->next) {
        gtk_widget_destroy(GTK_WIDGET(node->data));
    }
    g_list_free(children);

    int status = CIAO_LOG_OK;
    GPtrArray *files = ciao_log_list_files(&status);
    const char *empty_label = "No log files";
    if (status == CIAO_LOG_UNRESOLVED) {
        empty_label = "Log folder is unresolved";
    } else if (status == CIAO_LOG_UNREADABLE) {
        empty_label = "Cannot read log folder";
    }

    if (status != CIAO_LOG_OK || files == NULL || files->len == 0) {
        GtkWidget *item = gtk_menu_item_new_with_label(empty_label);
        gtk_widget_set_sensitive(item, FALSE);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
        gtk_widget_show(item);
        if (files != NULL) {
            g_ptr_array_free(files, TRUE);
        }
        return;
    }

    for (guint i = 0; i < files->len; i++) {
        CiaoLogFile *file = (CiaoLogFile *)g_ptr_array_index(files, i);
        gchar *label = g_strdup_printf("%d. %s", file->number,
                                       file->name != NULL ? file->name : "");
        GtkWidget *item = gtk_menu_item_new_with_label(label);
        g_free(label);
        g_object_set_data_full(G_OBJECT(item), "log-path", g_strdup(file->path), g_free);
        g_object_set_data_full(G_OBJECT(item), "log-name", g_strdup(file->name), g_free);
        g_object_set_data(G_OBJECT(item), "log-webview",
                          g_object_get_data(G_OBJECT(menu), "log-webview"));
        g_signal_connect(item, "activate", G_CALLBACK(on_log_file_activate), parent);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
        gtk_widget_show(item);
    }
    g_ptr_array_free(files, TRUE);
}

static void on_log_list_select(GtkMenuItem *item, gpointer user_data)
{
    (void)item;
    GtkWidget *menu = GTK_WIDGET(user_data);
    GtkWindow *parent = GTK_WINDOW(g_object_get_data(G_OBJECT(menu), "log-parent"));
    rebuild_list_submenu(menu, parent);
}

// The default application for inode/directory is the file manager on
// GNOME, KDE, XFCE, and the other freedesktop desktops. xdg-open is the
// fallback when that association is missing. The directory is not created.
static gboolean launch_file_manager(GtkWindow *parent, const char *dir, GError **error)
{
    gboolean launched = FALSE;
    GFile *file = g_file_new_for_path(dir);
    GList *files = g_list_append(NULL, file);
    GAppInfo *app = g_app_info_get_default_for_type("inode/directory", FALSE);

    if (app != NULL) {
        GAppLaunchContext *context = NULL;
        if (parent != NULL) {
            GdkAppLaunchContext *gdk_context = gdk_display_get_app_launch_context(
                gtk_widget_get_display(GTK_WIDGET(parent)));
            if (gdk_context != NULL) {
                gdk_app_launch_context_set_timestamp(gdk_context, gtk_get_current_event_time());
                context = G_APP_LAUNCH_CONTEXT(gdk_context);
            }
        }
        launched = g_app_info_launch(app, files, context, error);
        if (context != NULL) {
            g_object_unref(context);
        }
        g_object_unref(app);
    }

    g_list_free(files);
    g_object_unref(file);

    if (launched) {
        return TRUE;
    }

    if (error != NULL && *error != NULL) {
        g_clear_error(error);
    }

    gchar *argv[3];
    argv[0] = g_strdup("xdg-open");
    argv[1] = g_strdup(dir);
    argv[2] = NULL;
    launched = g_spawn_async(NULL,
                             argv,
                             NULL,
                             G_SPAWN_SEARCH_PATH,
                             NULL,
                             NULL,
                             NULL,
                             error);
    g_free(argv[0]);
    g_free(argv[1]);
    return launched;
}

static void on_log_folder_activate(GtkMenuItem *item, gpointer user_data)
{
    (void)item;
    GtkWindow *parent = GTK_WINDOW(user_data);
    gchar *dir = ciao_log_directory();
    if (dir == NULL || dir[0] == '\0') {
        g_free(dir);
        message_dialog(parent, GTK_MESSAGE_INFO, "Log folder", "Log folder is unresolved");
        return;
    }

    GError *error = NULL;
    if (!launch_file_manager(parent, dir, &error)) {
        message_dialog(parent, GTK_MESSAGE_ERROR, "Log folder", "Cannot open log folder");
        g_clear_error(&error);
    }
    g_free(dir);
}

static void on_log_clear_activate(GtkMenuItem *item, gpointer user_data)
{
    (void)item;
    GtkWindow *parent = GTK_WINDOW(user_data);

    int status = CIAO_LOG_OK;
    GPtrArray *files = ciao_log_list_files(&status);
    if (status == CIAO_LOG_UNRESOLVED) {
        if (files != NULL) {
            g_ptr_array_free(files, TRUE);
        }
        message_dialog(parent, GTK_MESSAGE_INFO, "Clear log", "Log folder is unresolved");
        return;
    }
    if (status == CIAO_LOG_UNREADABLE) {
        if (files != NULL) {
            g_ptr_array_free(files, TRUE);
        }
        message_dialog(parent, GTK_MESSAGE_ERROR, "Clear log", "Cannot read log folder");
        return;
    }

    guint count = (files != NULL) ? files->len : 0;
    if (files != NULL) {
        g_ptr_array_free(files, TRUE);
    }

    gchar *dir = ciao_log_directory();
    if (dir == NULL) {
        message_dialog(parent, GTK_MESSAGE_INFO, "Clear log", "Log folder is unresolved");
        return;
    }
    if (count == 0) {
        message_dialog(parent, GTK_MESSAGE_INFO, "Clear log", "No log files to clear");
        g_free(dir);
        return;
    }

    gchar *question = g_strdup_printf("Delete %u log file(s) from %s?", count, dir);
    GtkWidget *dialog = gtk_message_dialog_new(parent,
                                               static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
                                               GTK_MESSAGE_QUESTION,
                                               GTK_BUTTONS_YES_NO,
                                               "%s",
                                               question);
    g_free(question);
    gtk_window_set_title(GTK_WINDOW(dialog), "Clear log");
    gint response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    if (response != GTK_RESPONSE_YES) {
        g_free(dir);
        return;
    }

    gchar *error = NULL;
    int clear_status = CIAO_LOG_OK;
    int removed = ciao_log_clear_files(&clear_status, &error);
    if (clear_status != CIAO_LOG_OK) {
        message_dialog(parent, GTK_MESSAGE_ERROR, "Clear log",
                       error != NULL ? error : "cannot delete log file");
        g_free(error);
        g_free(dir);
        return;
    }
    g_free(error);

    gchar *done = g_strdup_printf("Cleared %d log file(s) from %s", removed, dir);
    message_dialog(parent, GTK_MESSAGE_INFO, "Clear log", done);
    g_free(done);
    g_free(dir);
}

GtkWidget *add_log_menu(GtkWindow *parent_window, WebKitWebView *web_view)
{
    if (parent_window == NULL || !WEBKIT_IS_WEB_VIEW(web_view)) {
        return NULL;
    }

    GtkWidget *log_item = gtk_menu_item_new_with_label("Log");
    GtkWidget *log_menu = gtk_menu_new();

    GtkWidget *list_item = gtk_menu_item_new_with_label("List log files");
    GtkWidget *list_menu = gtk_menu_new();
    g_object_set_data(G_OBJECT(list_menu), "log-parent", parent_window);
    g_object_set_data(G_OBJECT(list_menu), "log-webview", web_view);
    GtkWidget *placeholder = gtk_menu_item_new_with_label("No log files");
    gtk_widget_set_sensitive(placeholder, FALSE);
    gtk_menu_shell_append(GTK_MENU_SHELL(list_menu), placeholder);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(list_item), list_menu);
    g_signal_connect(list_item, "select", G_CALLBACK(on_log_list_select), list_menu);

    GtkWidget *folder_item = gtk_menu_item_new_with_label("Log folder");
    g_signal_connect(folder_item, "activate", G_CALLBACK(on_log_folder_activate), parent_window);

    GtkWidget *clear_item = gtk_menu_item_new_with_label("Clear log");
    g_signal_connect(clear_item, "activate", G_CALLBACK(on_log_clear_activate), parent_window);

    gtk_menu_shell_append(GTK_MENU_SHELL(log_menu), list_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(log_menu), folder_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(log_menu), clear_item);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(log_item), log_menu);
    return log_item;
}
