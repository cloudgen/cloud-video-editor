// lua_script_commands.cpp
// =========================================================================
// CIAO DEFENSIVE CODING STYLE - LUA SCRIPT COMMAND (GNOME APPLICATION)
// =========================================================================
//
// !!! THIS IS A GNOME/GTK APPLICATION !!!
// !!! RESULT TEXT USES g_print() AND g_printerr() ONLY !!!
// !!! DO NOT CALL ciao_log OR ciao_error. THAT STORES A LINE AND CREATES THE LOG DIRECTORY !!!
// !!! DO NOT CALL config_init(). THAT CREATES config.conf !!!
//
// Law: requirement-cpp-lua-script-commands.md
// The directory join matches resolve_full_storage_path() without calling it.
// That function reports through ciao_error.
// Last aligned: 2026-10-04
// =========================================================================

#include "lua_script_commands.h"
#include "output.h"
#include "project.h"
#include "config.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char LUA_SCRIPT_TEMPLATE[] =
    "-- @match        https://gemini.google.com/*\n"
    "-- @priority     50\n"
    "\n"
    "function on_url_changed(url)\n"
    "end\n"
    "\n"
    "register_on_url_changed(on_url_changed)\n";

static void lua_file_free(gpointer data)
{
    CiaoLuaScript *file = (CiaoLuaScript *)data;
    if (file == NULL) {
        return;
    }
    g_free(file->name);
    g_free(file->path);
    g_free(file);
}

static gint lua_file_cmp(gconstpointer a, gconstpointer b)
{
    const CiaoLuaScript *fa = (const CiaoLuaScript *)a;
    const CiaoLuaScript *fb = (const CiaoLuaScript *)b;
    if (fa == NULL || fa->name == NULL) {
        return (fb == NULL || fb->name == NULL) ? 0 : -1;
    }
    if (fb == NULL || fb->name == NULL) {
        return 1;
    }
    return strcmp(fa->name, fb->name);
}

static gboolean has_lua_extension(const char *name)
{
    const char *dot;
    if (name == NULL || name[0] == '\0') {
        return FALSE;
    }
    dot = strrchr(name, '.');
    if (dot == NULL || dot == name) {
        return FALSE;
    }
    return strcmp(dot, ".lua") == 0;
}

static gchar *json_escape(const char *text, gsize length)
{
    GString *out = g_string_new("");
    if (text == NULL) {
        return g_string_free(out, FALSE);
    }
    for (gsize i = 0; i < length; i++) {
        unsigned char c = (unsigned char)text[i];
        if (c == '\\' || c == '"') {
            g_string_append_c(out, '\\');
            g_string_append_c(out, (char)c);
        } else if (c == '\n') {
            g_string_append(out, "\\n");
        } else if (c == '\r') {
            g_string_append(out, "\\r");
        } else if (c == '\t') {
            g_string_append(out, "\\t");
        } else if (c < 0x20) {
            g_string_append_printf(out, "\\u%04x", c);
        } else {
            g_string_append_c(out, (char)c);
        }
    }
    return g_string_free(out, FALSE);
}

static gchar *json_escape_str(const char *text)
{
    if (text == NULL) {
        return json_escape("", 0);
    }
    return json_escape(text, strlen(text));
}

static void lua_fail(const char *action, const char *message)
{
    if (ciao_json_mode) {
        gchar *action_json = json_escape_str(action != NULL ? action : "");
        gchar *message_json = json_escape_str(message != NULL ? message : "");
        g_print("{\"command\":\"luascript\",\"action\":\"%s\",\"error\":\"%s\"}\n",
                action_json, message_json);
        g_free(action_json);
        g_free(message_json);
    } else if (message != NULL) {
        g_printerr("%s\n", message);
    }
    exit(1);
}

static void print_raw(const char *data, gsize length)
{
    if (data == NULL || length == 0) {
        return;
    }
    if (memchr(data, '\0', length) == NULL) {
        g_print("%s", data);
        return;
    }
    for (gsize i = 0; i < length; i++) {
        g_print("%c", data[i]);
    }
}

GPtrArray *ciao_lua_script_list(const char *dir, int *status)
{
    if (status != NULL) {
        *status = CIAO_LUA_OK;
    }

    GPtrArray *files = g_ptr_array_new_with_free_func(lua_file_free);
    if (dir == NULL || dir[0] == '\0') {
        if (status != NULL) {
            *status = CIAO_LUA_UNRESOLVED;
        }
        return files;
    }

    if (!g_file_test(dir, G_FILE_TEST_EXISTS)) {
        return files;
    }
    if (!g_file_test(dir, G_FILE_TEST_IS_DIR)) {
        if (status != NULL) {
            *status = CIAO_LUA_UNREADABLE;
        }
        return files;
    }

    GError *error = NULL;
    GDir *handle = g_dir_open(dir, 0, &error);
    if (handle == NULL) {
        g_clear_error(&error);
        if (status != NULL) {
            *status = CIAO_LUA_UNREADABLE;
        }
        return files;
    }

    GList *sorted = NULL;
    const gchar *name;
    while ((name = g_dir_read_name(handle)) != NULL) {
        if (name[0] == '\0' || strchr(name, '/') != NULL) {
            continue;
        }
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
            continue;
        }
        if (!has_lua_extension(name)) {
            continue;
        }

        gchar *path = g_build_filename(dir, name, NULL);
        struct stat st;
        if (path == NULL || lstat(path, &st) != 0 || !S_ISREG(st.st_mode)) {
            g_free(path);
            continue;
        }

        CiaoLuaScript *file = g_new0(CiaoLuaScript, 1);
        file->name = g_strdup(name);
        file->path = path;
        sorted = g_list_insert_sorted(sorted, file, lua_file_cmp);
    }
    g_dir_close(handle);

    int number = 1;
    for (GList *node = sorted; node != NULL; node = node->next) {
        CiaoLuaScript *file = (CiaoLuaScript *)node->data;
        file->number = number;
        number++;
        g_ptr_array_add(files, file);
    }
    g_list_free(sorted);
    return files;
}

gchar *ciao_lua_script_read(const char *path, gsize *length_out, int *status)
{
    if (length_out != NULL) {
        *length_out = 0;
    }
    if (status != NULL) {
        *status = CIAO_LUA_OK;
    }
    if (path == NULL || path[0] == '\0') {
        if (status != NULL) {
            *status = CIAO_LUA_IO;
        }
        return NULL;
    }

    int fd = open(path, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) {
        if (status != NULL) {
            *status = CIAO_LUA_IO;
        }
        return NULL;
    }

    struct stat st;
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode)) {
        close(fd);
        if (status != NULL) {
            *status = CIAO_LUA_IO;
        }
        return NULL;
    }

    GString *buf = g_string_new("");
    char tmp[4096];
    for (;;) {
        ssize_t got = read(fd, tmp, sizeof tmp);
        if (got < 0) {
            if (errno == EINTR) {
                continue;
            }
            close(fd);
            g_string_free(buf, TRUE);
            if (status != NULL) {
                *status = CIAO_LUA_IO;
            }
            return NULL;
        }
        if (got == 0) {
            break;
        }
        g_string_append_len(buf, tmp, (gssize)got);
    }

    if (close(fd) != 0) {
        g_string_free(buf, TRUE);
        if (status != NULL) {
            *status = CIAO_LUA_IO;
        }
        return NULL;
    }

    if (length_out != NULL) {
        *length_out = buf->len;
    }
    return g_string_free(buf, FALSE);
}

static gboolean write_all(int fd, const char *data, gsize length)
{
    gsize off = 0;
    if (data == NULL && length > 0) {
        return FALSE;
    }
    while (off < length) {
        ssize_t wrote = write(fd, data + off, length - off);
        if (wrote < 0) {
            if (errno == EINTR) {
                continue;
            }
            return FALSE;
        }
        if (wrote == 0) {
            return FALSE;
        }
        off += (gsize)wrote;
    }
    return TRUE;
}

gboolean ciao_lua_script_replace(const char *path, const char *data, gsize length, int *status)
{
    if (status != NULL) {
        *status = CIAO_LUA_OK;
    }
    if (path == NULL || path[0] == '\0') {
        if (status != NULL) {
            *status = CIAO_LUA_IO;
        }
        return FALSE;
    }

    int fd = open(path, O_WRONLY | O_TRUNC | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) {
        if (status != NULL) {
            *status = CIAO_LUA_IO;
        }
        return FALSE;
    }

    if (!write_all(fd, data != NULL ? data : "", length)) {
        close(fd);
        if (status != NULL) {
            *status = CIAO_LUA_IO;
        }
        return FALSE;
    }
    if (close(fd) != 0) {
        if (status != NULL) {
            *status = CIAO_LUA_IO;
        }
        return FALSE;
    }
    return TRUE;
}

static gboolean is_ascii_alnum(unsigned char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
}

static gboolean filename_matches(const char *name)
{
    size_t n;
    if (name == NULL || name[0] == '\0') {
        return FALSE;
    }
    if (!is_ascii_alnum((unsigned char)name[0])) {
        return FALSE;
    }
    n = strlen(name);
    if (n < 4 || strcmp(name + (n - 4), ".lua") != 0) {
        return FALSE;
    }
    for (size_t i = 1; i < n - 4; i++) {
        unsigned char c = (unsigned char)name[i];
        if (!(is_ascii_alnum(c) || c == '.' || c == '_' || c == '-')) {
            return FALSE;
        }
    }
    return TRUE;
}

static gchar *normalize_filename(const char *token, gchar **error_out)
{
    if (token == NULL) {
        if (error_out != NULL) {
            *error_out = g_strdup("missing lua script filename");
        }
        return NULL;
    }
    if (token[0] == '\0' || strcmp(token, ".") == 0 || strcmp(token, "..") == 0 ||
        strchr(token, '/') != NULL) {
        if (error_out != NULL) {
            *error_out = g_strdup("lua script filename must be a single name");
        }
        return NULL;
    }

    gchar *stored;
    if (strchr(token, '.') == NULL) {
        stored = g_strconcat(token, ".lua", NULL);
    } else {
        stored = g_strdup(token);
    }

    if (!g_str_has_suffix(stored, ".lua")) {
        g_free(stored);
        if (error_out != NULL) {
            *error_out = g_strdup("lua script filename must end in .lua");
        }
        return NULL;
    }
    if (!filename_matches(stored) || strchr(stored, '/') != NULL) {
        g_free(stored);
        if (error_out != NULL) {
            *error_out = g_strdup("lua script filename must be a single name");
        }
        return NULL;
    }
    return stored;
}

gboolean ciao_lua_script_create(const char *dir, const char *raw_name,
                                gchar **created_path, gchar **error_out)
{
    if (created_path != NULL) {
        *created_path = NULL;
    }
    if (error_out != NULL) {
        *error_out = NULL;
    }

    gchar *name = normalize_filename(raw_name, error_out);
    if (name == NULL) {
        return FALSE;
    }
    if (dir == NULL || dir[0] == '\0') {
        g_free(name);
        if (error_out != NULL) {
            *error_out = g_strdup("lua script folder is unresolved");
        }
        return FALSE;
    }

    gboolean dir_exists = g_file_test(dir, G_FILE_TEST_EXISTS);
    if (dir_exists && !g_file_test(dir, G_FILE_TEST_IS_DIR)) {
        g_free(name);
        if (error_out != NULL) {
            *error_out = g_strdup("cannot create lua script folder");
        }
        return FALSE;
    }

    gchar *path = g_build_filename(dir, name, NULL);
    if (path == NULL) {
        g_free(name);
        if (error_out != NULL) {
            *error_out = g_strdup("cannot create lua script folder");
        }
        return FALSE;
    }

    if (dir_exists) {
        struct stat st;
        if (lstat(path, &st) == 0) {
            if (error_out != NULL) {
                *error_out = g_strdup_printf("lua script already exists %s", name);
            }
            g_free(path);
            g_free(name);
            return FALSE;
        }
        GError *open_error = NULL;
        GDir *handle = g_dir_open(dir, 0, &open_error);
        if (handle == NULL) {
            g_clear_error(&open_error);
            if (error_out != NULL) {
                *error_out = g_strdup("cannot read lua script folder");
            }
            g_free(path);
            g_free(name);
            return FALSE;
        }
        g_dir_close(handle);
    } else if (g_mkdir_with_parents(dir, 0700) != 0 || chmod(dir, 0700) != 0) {
        if (error_out != NULL) {
            *error_out = g_strdup("cannot create lua script folder");
        }
        g_free(path);
        g_free(name);
        return FALSE;
    }

    int fd = open(path, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (fd < 0) {
        if (error_out != NULL) {
            if (errno == EEXIST) {
                *error_out = g_strdup_printf("lua script already exists %s", name);
            } else {
                *error_out = g_strdup_printf("cannot write lua script %s", name);
            }
        }
        g_free(path);
        g_free(name);
        return FALSE;
    }

    gboolean wrote = fchmod(fd, 0600) == 0 &&
                     write_all(fd, LUA_SCRIPT_TEMPLATE, sizeof LUA_SCRIPT_TEMPLATE - 1);
    if (close(fd) != 0) {
        wrote = FALSE;
    }
    if (!wrote) {
        unlink(path);
        if (error_out != NULL) {
            *error_out = g_strdup_printf("cannot write lua script %s", name);
        }
        g_free(path);
        g_free(name);
        return FALSE;
    }

    if (created_path != NULL) {
        *created_path = path;
    } else {
        g_free(path);
    }
    g_free(name);
    return TRUE;
}

static gboolean is_flag(const char *arg)
{
    return arg != NULL && strncmp(arg, "--", 2) == 0;
}

gboolean ciao_argv_has_lua_script_verb(int argc, char **argv)
{
    if (argv == NULL) {
        return FALSE;
    }
    for (int i = 1; i < argc; i++) {
        if (argv[i] == NULL) {
            continue;
        }
        if (strcmp(argv[i], "--config") == 0) {
            i++;
            continue;
        }
        if (is_flag(argv[i])) {
            continue;
        }
        if (strcmp(argv[i], "luascript") == 0) {
            return TRUE;
        }
    }
    return FALSE;
}

static void strip_config_value(char *value)
{
    gchar *trimmed;
    size_t len;
    if (value == NULL) {
        return;
    }
    trimmed = g_strstrip(value);
    len = strlen(trimmed);
    if (len >= 2 &&
        ((trimmed[0] == '"' && trimmed[len - 1] == '"') ||
         (trimmed[0] == '\'' && trimmed[len - 1] == '\''))) {
        trimmed[len - 1] = '\0';
        memmove(value, trimmed + 1, strlen(trimmed + 1) + 1);
    } else if (trimmed != value) {
        memmove(value, trimmed, strlen(trimmed) + 1);
    }
}

static gboolean read_scripts_path_file(const char *filepath, gchar **raw_out, gchar **error_out)
{
    FILE *file;
    char line[4096];
    char section[64];
    gchar *found = NULL;

    file = fopen(filepath, "r");
    if (file == NULL) {
        if (error_out != NULL) {
            *error_out = g_strdup_printf("cannot read config file %s",
                                         filepath != NULL ? filepath : "");
        }
        return FALSE;
    }

    g_strlcpy(section, "core", sizeof section);
    while (fgets(line, sizeof line, file) != NULL) {
        char *cursor = line;
        char *end;
        char *eq;
        while (*cursor == ' ' || *cursor == '\t') {
            cursor++;
        }
        if (*cursor == ';' || *cursor == '#' || *cursor == '\n' || *cursor == '\0' || *cursor == '\r') {
            continue;
        }
        if (*cursor == '[') {
            end = strchr(cursor, ']');
            if (end != NULL) {
                *end = '\0';
                g_strlcpy(section, cursor + 1, sizeof section);
                g_strstrip(section);
            }
            continue;
        }
        eq = strchr(cursor, '=');
        if (eq == NULL) {
            continue;
        }
        *eq = '\0';
        g_strstrip(cursor);
        strip_config_value(eq + 1);
        if (strcmp(section, "scripts") == 0 && strcmp(cursor, "path") == 0) {
            g_free(found);
            found = g_strdup(eq + 1);
        }
    }
    fclose(file);

    if (found == NULL || found[0] == '\0') {
        g_free(found);
        *raw_out = g_strdup(CONFIG_DEFAULT_SCRIPTS_PATH);
    } else {
        *raw_out = found;
    }
    return TRUE;
}

/* --config selects a file. With no --config, $HOME/.app/<id>/config.conf is
   read when it exists and is not created when it does not. */
static gboolean load_raw_scripts_path(int argc, char **argv, gchar **raw_out, gchar **error_out)
{
    const char *selected = NULL;
    gboolean saw_config = FALSE;

    if (raw_out != NULL) {
        *raw_out = NULL;
    }
    if (error_out != NULL) {
        *error_out = NULL;
    }

    for (int i = 1; argv != NULL && i < argc; i++) {
        if (argv[i] == NULL) {
            continue;
        }
        if (strcmp(argv[i], "--config") == 0) {
            if (i + 1 >= argc || argv[i + 1] == NULL) {
                if (error_out != NULL) {
                    *error_out = g_strdup("missing config path");
                }
                return FALSE;
            }
            selected = argv[++i];
            saw_config = TRUE;
            continue;
        }
    }

    if (saw_config) {
        return read_scripts_path_file(selected, raw_out, error_out);
    }

    const char *home = getenv("HOME");
    if (home != NULL && home[0] != '\0') {
        gchar *default_config = g_build_filename(home, ".app", PROJECT_ID, "config.conf", NULL);
        if (default_config != NULL && g_file_test(default_config, G_FILE_TEST_EXISTS)) {
            gboolean ok = read_scripts_path_file(default_config, raw_out, error_out);
            g_free(default_config);
            return ok;
        }
        g_free(default_config);
    }

    if (raw_out != NULL) {
        *raw_out = g_strdup(CONFIG_DEFAULT_SCRIPTS_PATH);
    }
    return TRUE;
}

static gchar *expand_scripts_path(const char *base_path)
{
    const char *home = getenv("HOME");
    char cleaned[1024];
    gchar *trimmed;
    size_t len;
    gboolean absolute;
    gboolean tilde;
    gchar *resolved = NULL;

    if (base_path == NULL || base_path[0] == '\0') {
        base_path = CONFIG_DEFAULT_SCRIPTS_PATH;
    }
    g_strlcpy(cleaned, base_path, sizeof cleaned);
    trimmed = g_strstrip(cleaned);
    len = strlen(trimmed);
    if (len >= 2 &&
        ((trimmed[0] == '"' && trimmed[len - 1] == '"') ||
         (trimmed[0] == '\'' && trimmed[len - 1] == '\''))) {
        trimmed[len - 1] = '\0';
        trimmed++;
    }

    absolute = g_path_is_absolute(trimmed);
    tilde = g_str_has_prefix(trimmed, "~/");
    if (tilde) {
        if (home == NULL || home[0] == '\0') {
            return NULL;
        }
        resolved = g_build_filename(home, trimmed + 2, NULL);
    } else if (absolute) {
        resolved = g_strdup(trimmed);
    } else {
        if (home == NULL || home[0] == '\0') {
            return NULL;
        }
        resolved = g_build_filename(home, trimmed, NULL);
    }
    if (resolved == NULL || resolved[0] == '\0') {
        g_free(resolved);
        return NULL;
    }

    char canonical[PATH_MAX];
    if (realpath(resolved, canonical) != NULL) {
        g_free(resolved);
        return g_strdup(canonical);
    }
    return resolved;
}

static void print_usage(void)
{
    if (ciao_json_mode) {
        g_print("{\"command\":\"luascript\",\"actions\":[\"folder\",\"list\",\"view\",\"edit\",\"new\"]}\n");
        return;
    }
    g_print("Usage: %s luascript <action>\n\n", PROJECT_ID);
    g_print("Actions:\n");
    g_print("  folder            Display the lua script folder\n");
    g_print("  list              Number the lua scripts\n");
    g_print("  view <token>      Display one lua script\n");
    g_print("  edit <token>      Replace one lua script from stdin\n");
    g_print("  new <filename>    Create a lua script\n");
}

static void fail_list_status(const char *action, int status)
{
    if (status == CIAO_LUA_UNRESOLVED) {
        lua_fail(action, "lua script folder is unresolved");
    }
    if (status == CIAO_LUA_UNREADABLE) {
        lua_fail(action, "cannot read lua script folder");
    }
}

static gboolean token_is_number(const char *text, int *out)
{
    if (text == NULL || text[0] == '\0' || out == NULL) {
        return FALSE;
    }
    for (const char *cursor = text; *cursor != '\0'; cursor++) {
        if (*cursor < '0' || *cursor > '9') {
            return FALSE;
        }
    }
    if (text[0] == '0' && text[1] != '\0') {
        return FALSE;
    }
    errno = 0;
    char *end = NULL;
    long value = strtol(text, &end, 10);
    if (end == text || *end != '\0' || errno == ERANGE || value < 0 || value > INT_MAX) {
        *out = -1;
        return TRUE;
    }
    *out = (int)value;
    return TRUE;
}

static CiaoLuaScript *find_script(GPtrArray *files, const char *token)
{
    int number = 0;
    gboolean numeric = token_is_number(token, &number);
    if (files == NULL) {
        return NULL;
    }
    for (guint i = 0; i < files->len; i++) {
        CiaoLuaScript *file = (CiaoLuaScript *)g_ptr_array_index(files, i);
        if (file == NULL) {
            continue;
        }
        if (numeric) {
            if (file->number == number) {
                return file;
            }
        } else if (file->name != NULL && strcmp(file->name, token) == 0) {
            return file;
        }
    }
    return NULL;
}

static gchar *require_directory(const char *raw, const char *action)
{
    gchar *dir = expand_scripts_path(raw);
    if (dir == NULL) {
        lua_fail(action, "lua script folder is unresolved");
    }
    return dir;
}

static void cmd_folder(const char *raw)
{
    gchar *folder = require_directory(raw, "folder");
    if (ciao_json_mode) {
        gchar *folder_json = json_escape_str(folder);
        g_print("{\"command\":\"luascript\",\"action\":\"folder\",\"folder\":\"%s\"}\n", folder_json);
        g_free(folder_json);
    } else {
        g_print("%s\n", folder);
    }
    g_free(folder);
    exit(0);
}

static void cmd_list(const char *raw)
{
    gchar *folder = require_directory(raw, "list");
    int status = CIAO_LUA_OK;
    GPtrArray *files = ciao_lua_script_list(folder, &status);
    fail_list_status("list", status);

    if (ciao_json_mode) {
        gchar *folder_json = json_escape_str(folder);
        GString *out = g_string_new("{\"command\":\"luascript\",\"action\":\"list\",\"folder\":\"");
        g_string_append(out, folder_json);
        g_string_append(out, "\",\"files\":[");
        g_free(folder_json);
        if (files != NULL) {
            for (guint i = 0; i < files->len; i++) {
                CiaoLuaScript *file = (CiaoLuaScript *)g_ptr_array_index(files, i);
                gchar *name_json = json_escape_str(file->name);
                if (i > 0) {
                    g_string_append_c(out, ',');
                }
                g_string_append_printf(out, "{\"number\":%d,\"name\":\"%s\"}",
                                       file->number, name_json);
                g_free(name_json);
            }
        }
        g_string_append(out, "]}");
        g_print("%s\n", out->str);
        g_string_free(out, TRUE);
    } else if (files != NULL) {
        for (guint i = 0; i < files->len; i++) {
            CiaoLuaScript *file = (CiaoLuaScript *)g_ptr_array_index(files, i);
            g_print("%d. %s\n", file->number, file->name != NULL ? file->name : "");
        }
    }

    g_free(folder);
    if (files != NULL) {
        g_ptr_array_free(files, TRUE);
    }
    exit(0);
}

static void cmd_view(const char *raw, const char *token)
{
    if (token == NULL) {
        lua_fail("view", "missing lua script name");
    }
    gchar *folder = require_directory(raw, "view");
    int status = CIAO_LUA_OK;
    GPtrArray *files = ciao_lua_script_list(folder, &status);
    g_free(folder);
    fail_list_status("view", status);

    CiaoLuaScript *file = find_script(files, token);
    if (file == NULL || file->path == NULL) {
        if (files != NULL) {
            g_ptr_array_free(files, TRUE);
        }
        gchar *msg = g_strdup_printf("no lua script %s", token);
        lua_fail("view", msg);
    }

    gsize length = 0;
    int read_status = CIAO_LUA_OK;
    gchar *content = ciao_lua_script_read(file->path, &length, &read_status);
    if (content == NULL || read_status != CIAO_LUA_OK) {
        g_free(content);
        gchar *msg = g_strdup_printf("cannot read lua script %s",
                                     file->name != NULL ? file->name : "");
        g_ptr_array_free(files, TRUE);
        lua_fail("view", msg);
    }

    if (ciao_json_mode) {
        gchar *name_json = json_escape_str(file->name);
        gchar *content_json = json_escape(content, length);
        g_print("{\"command\":\"luascript\",\"action\":\"view\",\"number\":%d,\"name\":\"%s\",\"content\":\"%s\"}\n",
                file->number, name_json, content_json);
        g_free(name_json);
        g_free(content_json);
    } else {
        print_raw(content, length);
    }

    g_free(content);
    g_ptr_array_free(files, TRUE);
    exit(0);
}

static gchar *read_stdin_all(gsize *length_out)
{
    GString *buf = g_string_new("");
    char tmp[4096];
    for (;;) {
        ssize_t got = read(STDIN_FILENO, tmp, sizeof tmp);
        if (got < 0) {
            if (errno == EINTR) {
                continue;
            }
            g_string_free(buf, TRUE);
            return NULL;
        }
        if (got == 0) {
            break;
        }
        g_string_append_len(buf, tmp, (gssize)got);
    }
    if (length_out != NULL) {
        *length_out = buf->len;
    }
    return g_string_free(buf, FALSE);
}

static void cmd_edit(const char *raw, const char *token)
{
    if (token == NULL) {
        lua_fail("edit", "missing lua script name");
    }
    if (isatty(STDIN_FILENO)) {
        lua_fail("edit", "lua script edit needs the new text on stdin");
    }

    gchar *folder = require_directory(raw, "edit");
    int status = CIAO_LUA_OK;
    GPtrArray *files = ciao_lua_script_list(folder, &status);
    g_free(folder);
    fail_list_status("edit", status);

    CiaoLuaScript *file = find_script(files, token);
    if (file == NULL || file->path == NULL || file->name == NULL) {
        if (files != NULL) {
            g_ptr_array_free(files, TRUE);
        }
        gchar *msg = g_strdup_printf("no lua script %s", token);
        lua_fail("edit", msg);
    }

    gsize length = 0;
    gchar *body = read_stdin_all(&length);
    if (body == NULL) {
        gchar *msg = g_strdup_printf("cannot write lua script %s", file->name);
        g_ptr_array_free(files, TRUE);
        lua_fail("edit", msg);
    }

    int write_status = CIAO_LUA_OK;
    if (!ciao_lua_script_replace(file->path, body, length, &write_status)) {
        g_free(body);
        gchar *msg = g_strdup_printf("cannot write lua script %s", file->name);
        g_ptr_array_free(files, TRUE);
        lua_fail("edit", msg);
    }

    if (ciao_json_mode) {
        gchar *name_json = json_escape_str(file->name);
        g_print("{\"command\":\"luascript\",\"action\":\"edit\",\"number\":%d,\"name\":\"%s\",\"bytes\":%llu}\n",
                file->number, name_json, (unsigned long long)length);
        g_free(name_json);
    } else {
        g_print("Updated %s\n", file->name);
    }

    g_free(body);
    g_ptr_array_free(files, TRUE);
    exit(0);
}

/* prompt_allowed is decided by the caller. This function does not test the terminal. */
static gchar *prompt_for_filename(gboolean prompt_allowed)
{
    if (!prompt_allowed) {
        return NULL;
    }
    g_printerr("Lua script filename: ");
    GString *line = g_string_new("");
    for (;;) {
        char byte;
        ssize_t got = read(STDIN_FILENO, &byte, 1);
        if (got < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        if (got == 0 || byte == '\n') {
            break;
        }
        g_string_append_c(line, byte);
    }
    return g_string_free(line, FALSE);
}

static void cmd_new(const char *raw, const char *token)
{
    gboolean prompt_allowed = isatty(STDIN_FILENO) && isatty(STDOUT_FILENO) && !ciao_json_mode;
    gchar *prompted = NULL;
    const char *name = token;
    if (name == NULL) {
        prompted = prompt_for_filename(prompt_allowed);
        if (prompted == NULL) {
            lua_fail("new", "missing lua script filename");
        }
        name = prompted;
    }

    gchar *folder = require_directory(raw, "new");
    gchar *created = NULL;
    gchar *error = NULL;
    if (!ciao_lua_script_create(folder, name, &created, &error)) {
        g_free(prompted);
        g_free(folder);
        lua_fail("new", error != NULL ? error : "cannot write lua script");
    }
    g_free(prompted);

    gchar *base = g_path_get_basename(created);
    if (ciao_json_mode) {
        gchar *folder_json = json_escape_str(folder);
        gchar *name_json = json_escape_str(base);
        g_print("{\"command\":\"luascript\",\"action\":\"new\",\"folder\":\"%s\",\"name\":\"%s\"}\n",
                folder_json, name_json);
        g_free(folder_json);
        g_free(name_json);
    } else {
        g_print("Created %s\n", base != NULL ? base : "");
    }
    g_free(base);
    g_free(created);
    g_free(folder);
    exit(0);
}

void ciao_lua_script_command_dispatch(int argc, char **argv)
{
    const char *action = NULL;
    const char *token = NULL;
    const char *extra = NULL;
    gboolean seen_verb = FALSE;
    gchar *raw = NULL;
    gchar *config_error = NULL;

    for (int i = 1; argv != NULL && i < argc; i++) {
        if (argv[i] == NULL) {
            continue;
        }
        if (strcmp(argv[i], "--config") == 0) {
            i++;
            continue;
        }
        if (is_flag(argv[i])) {
            continue;
        }
        if (!seen_verb) {
            if (strcmp(argv[i], "luascript") == 0) {
                seen_verb = TRUE;
            }
            continue;
        }
        if (action == NULL) {
            action = argv[i];
            continue;
        }
        if (token == NULL) {
            token = argv[i];
            continue;
        }
        extra = argv[i];
        break;
    }

    if (!seen_verb) {
        lua_fail("", "missing lua script command");
    }
    if (!load_raw_scripts_path(argc, argv, &raw, &config_error)) {
        lua_fail(action != NULL ? action : "", config_error);
    }
    if (extra != NULL) {
        gchar *msg = g_strdup_printf("unexpected argument %s", extra);
        g_free(raw);
        lua_fail(action != NULL ? action : "", msg);
    }
    if (action == NULL) {
        g_free(raw);
        print_usage();
        exit(0);
    }
    if (strcmp(action, "folder") == 0) {
        if (token != NULL) {
            gchar *msg = g_strdup_printf("unexpected argument %s", token);
            g_free(raw);
            lua_fail(action, msg);
        }
        cmd_folder(raw);
    }
    if (strcmp(action, "list") == 0) {
        if (token != NULL) {
            gchar *msg = g_strdup_printf("unexpected argument %s", token);
            g_free(raw);
            lua_fail(action, msg);
        }
        cmd_list(raw);
    }
    if (strcmp(action, "view") == 0) {
        cmd_view(raw, token);
    }
    if (strcmp(action, "edit") == 0) {
        cmd_edit(raw, token);
    }
    if (strcmp(action, "new") == 0) {
        cmd_new(raw, token);
    }

    gchar *msg = g_strdup_printf("unknown lua script action %s", action);
    g_free(raw);
    lua_fail(action, msg);
}
