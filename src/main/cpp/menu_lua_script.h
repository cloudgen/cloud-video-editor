// menu_lua_script.h
// =========================================================================
// CIAO DEFENSIVE CODING STYLE - LUA SCRIPT MENU HEADER (GNOME + GTK)
// =========================================================================
//
// !!! THIS IS A GNOME/GTK APPLICATION !!!
// Law: requirement-cpp-lua-script-commands.md
// The Lua Script item is added only from build_main_menu_bar().
// Last aligned: 2026-10-04
// =========================================================================

#ifndef MENU_LUA_SCRIPT_H
#define MENU_LUA_SCRIPT_H

#include <gtk/gtk.h>

GtkWidget *add_lua_script_menu(GtkWindow *parent_window);

#endif /* MENU_LUA_SCRIPT_H */
