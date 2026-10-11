// menu_lua_script.cpp
// =========================================================================
// CIAO DEFENSIVE CODING STYLE - LUA SCRIPT MENU (GNOME + GTK)
// =========================================================================
//
// !!! THIS IS A GNOME/GTK APPLICATION !!!
// Law: requirement-cpp-lua-script-commands.md
// These handlers do not call ciao_*. The folder comes from
// config_get_scripts_path() and resolve_full_storage_path() after
// configuration has loaded. [scripts] enabled does not hide this item.
// Last aligned: 2026-10-04
// =========================================================================

#include "menu_lua_script.h"
#include "lua_script_commands.h"
#include "config.h"
#include "path_utils.h"

#include <gtk/gtk.h>
#include <limits.h>

enum {
    LUA_RESPONSE_EDIT = 1,
    LUA_RESPONSE_SAVE = 2,
    LUA_RESPONSE_CLOSE = 3
};

static void message_dialog(GtkWindow *parent, GtkMessageType type,
                           const char *title, const char *body)
{
    GtkWidget *dialog = gtk_message_dialog_new(parent,
                                               static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
                                               type,
                                               GTK_BUTTONS_CLOSE,
                                               "%s",
                                               body != NULL ? body : "");
    gtk_window_set_title(GTK_WINDOW(dialog), title != NULL ? title : "Lua script");
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

static gchar *menu_scripts_directory(void)
{
    const char *raw = config_get_scripts_path();
    char full[PATH_MAX];

    full[0] = '\0';
    resolve_full_storage_path(raw != NULL ? raw : "", full, sizeof full);
    if (full[0] == '\0') {
        return NULL;
    }
    return g_strdup(full);
}

static gchar *buffer_text(GtkTextView *view)
{
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(view);
    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    return gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
}

static void set_script_editable(GtkWidget *dialog, GtkWidget *view, gboolean editable)
{
    GtkWidget *save = gtk_dialog_get_widget_for_response(GTK_DIALOG(dialog), LUA_RESPONSE_SAVE);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(view), editable);
    if (save != NULL) {
        gtk_widget_set_sensitive(save, editable);
    }
}

static gboolean save_script_buffer(GtkWindow *parent, GtkWidget *view, GtkWidget *error_label,
                                   const char *path, const char *name, gchar **saved)
{
    gchar *text = buffer_text(GTK_TEXT_VIEW(view));
    int status = CIAO_LUA_OK;
    gsize length = text != NULL ? strlen(text) : 0;
    (void)parent;
    if (!ciao_lua_script_replace(path, text != NULL ? text : "", length, &status)) {
        gchar *msg = g_strdup_printf("cannot write lua script %s", name != NULL ? name : "");
        gtk_label_set_text(GTK_LABEL(error_label), msg);
        g_free(msg);
        g_free(text);
        return FALSE;
    }
    g_free(*saved);
    *saved = text;
    gtk_label_set_text(GTK_LABEL(error_label), "");
    return TRUE;
}

static void open_script_dialog(GtkWindow *parent, const char *path, const char *name, gboolean editable)
{
    const char *title = (name != NULL && name[0] != '\0') ? name : "Lua script";
    gsize length = 0;
    int status = CIAO_LUA_OK;
    gchar *content = ciao_lua_script_read(path, &length, &status);
    if (content == NULL || status != CIAO_LUA_OK) {
        g_free(content);
        gchar *msg = g_strdup_printf("cannot read lua script %s", title);
        message_dialog(parent, GTK_MESSAGE_ERROR, "Lua script", msg);
        g_free(msg);
        return;
    }

    gchar *shown = g_utf8_make_valid(content, (gssize)length);
    g_free(content);
    if (shown == NULL) {
        shown = g_strdup("");
    }

    GtkWidget *dialog = gtk_dialog_new_with_buttons(title,
                                                    parent,
                                                    static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
                                                    "Edit", LUA_RESPONSE_EDIT,
                                                    "Save", LUA_RESPONSE_SAVE,
                                                    "Close", LUA_RESPONSE_CLOSE,
                                                    NULL);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 720, 480);

    GtkWidget *content_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(content_box), 8);

    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_hexpand(scrolled, TRUE);
    gtk_widget_set_vexpand(scrolled, TRUE);

    GtkWidget *view = gtk_text_view_new();
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(view), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(view), GTK_WRAP_NONE);
    gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(view)), shown, -1);

    GtkWidget *error_label = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(error_label), 0.0f);
    gtk_label_set_line_wrap(GTK_LABEL(error_label), TRUE);
    gtk_widget_set_halign(error_label, GTK_ALIGN_START);

    gtk_container_add(GTK_CONTAINER(scrolled), view);
    gtk_box_pack_start(GTK_BOX(content_box), scrolled, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(content_box), error_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
                       content_box, TRUE, TRUE, 0);

    gchar *saved = shown;
    set_script_editable(dialog, view, editable);
    gtk_widget_show_all(dialog);

    for (;;) {
        gint response = gtk_dialog_run(GTK_DIALOG(dialog));
        if (response == LUA_RESPONSE_EDIT) {
            set_script_editable(dialog, view, TRUE);
            continue;
        }
        if (response == LUA_RESPONSE_SAVE) {
            save_script_buffer(parent, view, error_label, path, title, &saved);
            continue;
        }

        gchar *now = buffer_text(GTK_TEXT_VIEW(view));
        gboolean dirty = g_strcmp0(now, saved) != 0;
        g_free(now);
        if (!dirty) {
            break;
        }

        gchar *question = g_strdup_printf("Save changes to %s?", title);
        GtkWidget *ask = gtk_message_dialog_new(GTK_WINDOW(dialog),
                                                static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
                                                GTK_MESSAGE_QUESTION,
                                                GTK_BUTTONS_NONE,
                                                "%s",
                                                question);
        g_free(question);
        gtk_dialog_add_button(GTK_DIALOG(ask), "Yes", GTK_RESPONSE_YES);
        gtk_dialog_add_button(GTK_DIALOG(ask), "No", GTK_RESPONSE_NO);
        gtk_dialog_add_button(GTK_DIALOG(ask), "Cancel", GTK_RESPONSE_CANCEL);
        gtk_dialog_set_default_response(GTK_DIALOG(ask), GTK_RESPONSE_CANCEL);
        gint choice = gtk_dialog_run(GTK_DIALOG(ask));
        gtk_widget_destroy(ask);
        if (choice == GTK_RESPONSE_YES) {
            if (!save_script_buffer(parent, view, error_label, path, title, &saved)) {
                continue;
            }
            break;
        }
        if (choice == GTK_RESPONSE_NO) {
            break;
        }
    }

    g_free(saved);
    gtk_widget_destroy(dialog);
}

static void on_lua_file_activate(GtkMenuItem *item, gpointer user_data)
{
    GtkWindow *parent = GTK_WINDOW(user_data);
    const char *path = (const char *)g_object_get_data(G_OBJECT(item), "lua-path");
    const char *name = (const char *)g_object_get_data(G_OBJECT(item), "lua-name");
    open_script_dialog(parent, path, name, FALSE);
}

static void rebuild_list_submenu(GtkWidget *menu, GtkWindow *parent)
{
    GList *children = gtk_container_get_children(GTK_CONTAINER(menu));
    for (GList *node = children; node != NULL; node = node->next) {
        gtk_widget_destroy(GTK_WIDGET(node->data));
    }
    g_list_free(children);

    gchar *dir = menu_scripts_directory();
    int status = CIAO_LUA_OK;
    GPtrArray *files = ciao_lua_script_list(dir, &status);
    g_free(dir);

    const char *empty_label = "No lua scripts";
    if (status == CIAO_LUA_UNRESOLVED) {
        empty_label = "Lua script folder is unresolved";
    } else if (status == CIAO_LUA_UNREADABLE) {
        empty_label = "Cannot read lua script folder";
    }

    if (status != CIAO_LUA_OK || files == NULL || files->len == 0) {
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
        CiaoLuaScript *file = (CiaoLuaScript *)g_ptr_array_index(files, i);
        gchar *label = g_strdup_printf("%d. %s", file->number,
                                       file->name != NULL ? file->name : "");
        GtkWidget *item = gtk_menu_item_new_with_label(label);
        g_free(label);
        g_object_set_data_full(G_OBJECT(item), "lua-path", g_strdup(file->path), g_free);
        g_object_set_data_full(G_OBJECT(item), "lua-name", g_strdup(file->name), g_free);
        g_signal_connect(item, "activate", G_CALLBACK(on_lua_file_activate), parent);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
        gtk_widget_show(item);
    }
    g_ptr_array_free(files, TRUE);
}

static void on_lua_list_select(GtkMenuItem *item, gpointer user_data)
{
    (void)item;
    GtkWidget *menu = GTK_WIDGET(user_data);
    GtkWindow *parent = GTK_WINDOW(g_object_get_data(G_OBJECT(menu), "lua-parent"));
    rebuild_list_submenu(menu, parent);
}

static void on_lua_folder_activate(GtkMenuItem *item, gpointer user_data)
{
    (void)item;
    GtkWindow *parent = GTK_WINDOW(user_data);
    gchar *dir = menu_scripts_directory();
    if (dir == NULL) {
        message_dialog(parent, GTK_MESSAGE_INFO, "Lua script folder", "Lua script folder is unresolved");
        return;
    }
    message_dialog(parent, GTK_MESSAGE_INFO, "Lua script folder", dir);
    g_free(dir);
}

static void on_lua_new_activate(GtkMenuItem *item, gpointer user_data)
{
    (void)item;
    GtkWindow *parent = GTK_WINDOW(user_data);
    GtkWidget *dialog = gtk_dialog_new_with_buttons("New lua script",
                                                    parent,
                                                    static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
                                                    "Cancel", GTK_RESPONSE_CANCEL,
                                                    "Create", GTK_RESPONSE_ACCEPT,
                                                    NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(box), 12);

    GtkWidget *name_label = gtk_label_new("Lua script filename");
    gtk_label_set_xalign(GTK_LABEL(name_label), 0.0f);

    GtkWidget *hint = gtk_label_new("This file is created in the lua script folder. A name with no dot is saved with .lua added.");
    gtk_label_set_xalign(GTK_LABEL(hint), 0.0f);
    gtk_label_set_line_wrap(GTK_LABEL(hint), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(hint), 48);
    gtk_widget_set_halign(hint, GTK_ALIGN_START);

    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "name.lua");
    gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);

    GtkWidget *error_label = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(error_label), 0.0f);
    gtk_label_set_line_wrap(GTK_LABEL(error_label), TRUE);
    gtk_widget_set_halign(error_label, GTK_ALIGN_START);

    gtk_box_pack_start(GTK_BOX(box), name_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), hint, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), entry, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), error_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
                       box, TRUE, TRUE, 0);

    for (;;) {
        gtk_widget_show_all(dialog);
        gint response = gtk_dialog_run(GTK_DIALOG(dialog));
        if (response != GTK_RESPONSE_ACCEPT) {
            gtk_widget_destroy(dialog);
            return;
        }

        const char *typed = gtk_entry_get_text(GTK_ENTRY(entry));
        gchar *dir = menu_scripts_directory();
        gchar *created = NULL;
        gchar *error = NULL;
        gboolean ok = ciao_lua_script_create(dir, typed, &created, &error);
        g_free(dir);
        if (!ok) {
            gtk_label_set_text(GTK_LABEL(error_label), error != NULL ? error : "");
            g_free(error);
            continue;
        }
        g_free(error);

        gchar *created_name = g_path_get_basename(created);
        gchar *created_path = created;
        gtk_widget_destroy(dialog);
        open_script_dialog(parent, created_path, created_name, TRUE);
        g_free(created_path);
        g_free(created_name);
        return;
    }
}

GtkWidget *add_lua_script_menu(GtkWindow *parent_window)
{
    if (parent_window == NULL) {
        return NULL;
    }

    GtkWidget *lua_item = gtk_menu_item_new_with_label("Lua Script");
    GtkWidget *lua_menu = gtk_menu_new();

    GtkWidget *list_item = gtk_menu_item_new_with_label("List lua scripts");
    GtkWidget *list_menu = gtk_menu_new();
    g_object_set_data(G_OBJECT(list_menu), "lua-parent", parent_window);
    GtkWidget *placeholder = gtk_menu_item_new_with_label("No lua scripts");
    gtk_widget_set_sensitive(placeholder, FALSE);
    gtk_menu_shell_append(GTK_MENU_SHELL(list_menu), placeholder);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(list_item), list_menu);
    g_signal_connect(list_item, "select", G_CALLBACK(on_lua_list_select), list_menu);

    GtkWidget *folder_item = gtk_menu_item_new_with_label("Lua script folder");
    g_signal_connect(folder_item, "activate", G_CALLBACK(on_lua_folder_activate), parent_window);

    GtkWidget *new_item = gtk_menu_item_new_with_label("New lua script");
    g_signal_connect(new_item, "activate", G_CALLBACK(on_lua_new_activate), parent_window);

    gtk_menu_shell_append(GTK_MENU_SHELL(lua_menu), list_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(lua_menu), folder_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(lua_menu), new_item);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(lua_item), lua_menu);
    return lua_item;
}
