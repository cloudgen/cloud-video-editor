// lua_script_commands.h
// =========================================================================
// CIAO DEFENSIVE CODING STYLE - LUA SCRIPT COMMAND HEADER (GNOME APPLICATION)
// =========================================================================
//
// !!! THIS IS A GNOME/GTK APPLICATION !!!
// !!! THE luascript VERB PRINTS WITH g_print / g_printerr, NOT ciao_log !!!
// !!! DO NOT CALL config_init() FROM THIS VERB !!!
// !!! DO NOT CREATE THE SCRIPT DIRECTORY FROM folder, list, view, OR edit !!!
//
// Law: requirement-cpp-lua-script-commands.md
// Last aligned: 2026-10-04
// =========================================================================

#ifndef LUA_SCRIPT_COMMANDS_H
#define LUA_SCRIPT_COMMANDS_H

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    CIAO_LUA_OK = 0,
    CIAO_LUA_UNREADABLE = 1,
    CIAO_LUA_UNRESOLVED = 2,
    CIAO_LUA_IO = 3
};

typedef struct {
    int number;
    gchar *name;
    gchar *path;
} CiaoLuaScript;

/* Regular *.lua files in dir, basename order, numbers starting at 1.
   A missing directory is an empty array and CIAO_LUA_OK. Does not create it.
   A NULL dir is CIAO_LUA_UNRESOLVED. Caller frees with g_ptr_array_free(array, TRUE). */
GPtrArray *ciao_lua_script_list(const char *dir, int *status);

/* Raw bytes of one regular file. Does not follow a symlink.
   *length_out is the byte count. Caller frees the returned buffer. */
gchar *ciao_lua_script_read(const char *path, gsize *length_out, int *status);

/* Replace one file in place. The mode is left as it was. Does not create the file. */
gboolean ciao_lua_script_replace(const char *path, const char *data, gsize length, int *status);

/* Create one script. raw_name is the dialog or argv token, before .lua is added.
   The directory is created mode 0700 only after the name is accepted and only
   when it is missing. On success *created_path is the new file. On failure
   *error_out is one sentence. Caller frees both. */
gboolean ciao_lua_script_create(const char *dir, const char *raw_name,
                                gchar **created_path, gchar **error_out);

gboolean ciao_argv_has_lua_script_verb(int argc, char **argv);

/* Prints the result and exits. Call only when the verb is present,
   after --json / JSON=1 have been applied, and before startup lines. */
void ciao_lua_script_command_dispatch(int argc, char **argv);

#ifdef __cplusplus
}
#endif

#endif /* LUA_SCRIPT_COMMANDS_H */
