// log_commands.cpp
// =========================================================================
// CIAO DEFENSIVE CODING STYLE - LOG COMMAND (GNOME APPLICATION)
// =========================================================================
//
// !!! THIS IS A GNOME/GTK APPLICATION !!!
// !!! RESULT TEXT USES g_print() AND g_printerr() ONLY !!!
// !!! DO NOT CALL ciao_log. THAT WOULD STORE A LINE AND CREATE THE DIRECTORY !!!
//
// Law: requirement-cpp-log-commands.md
// The rehearsal verbs test, list, and show are not implemented here.
// Last aligned: 2026-10-04
// =========================================================================

#include "log_commands.h"
#include "output.h"
#include "project.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void log_file_free(gpointer data)
{
    CiaoLogFile *file = (CiaoLogFile *)data;
    if (file == NULL) {
        return;
    }
    g_free(file->name);
    g_free(file->path);
    g_free(file);
}

static gint log_file_cmp(gconstpointer a, gconstpointer b)
{
    const CiaoLogFile *fa = (const CiaoLogFile *)a;
    const CiaoLogFile *fb = (const CiaoLogFile *)b;
    if (fa == NULL || fa->name == NULL) {
        return (fb == NULL || fb->name == NULL) ? 0 : -1;
    }
    if (fb == NULL || fb->name == NULL) {
        return 1;
    }
    return strcmp(fa->name, fb->name);
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

static void log_fail(const char *action, const char *message)
{
    if (ciao_json_mode) {
        gchar *action_json = json_escape_str(action != NULL ? action : "");
        gchar *message_json = json_escape_str(message != NULL ? message : "");
        g_print("{\"command\":\"log\",\"action\":\"%s\",\"error\":\"%s\"}\n",
                action_json, message_json);
        g_free(action_json);
        g_free(message_json);
    } else if (message != NULL) {
        g_printerr("%s\n", message);
    }
    exit(1);
}

GPtrArray *ciao_log_list_files(int *status)
{
    if (status != NULL) {
        *status = CIAO_LOG_OK;
    }

    GPtrArray *files = g_ptr_array_new_with_free_func(log_file_free);
    gchar *dir = ciao_log_directory();
    if (dir == NULL) {
        if (status != NULL) {
            *status = CIAO_LOG_UNRESOLVED;
        }
        return files;
    }

    if (!g_file_test(dir, G_FILE_TEST_EXISTS)) {
        g_free(dir);
        return files;
    }
    if (!g_file_test(dir, G_FILE_TEST_IS_DIR)) {
        g_free(dir);
        if (status != NULL) {
            *status = CIAO_LOG_UNREADABLE;
        }
        return files;
    }

    GError *error = NULL;
    GDir *handle = g_dir_open(dir, 0, &error);
    if (handle == NULL) {
        g_clear_error(&error);
        g_free(dir);
        if (status != NULL) {
            *status = CIAO_LOG_UNREADABLE;
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
        if (!g_str_has_suffix(name, LOGGER_FILE_EXT)) {
            continue;
        }

        gchar *path = g_build_filename(dir, name, NULL);
        struct stat st;
        if (path == NULL || lstat(path, &st) != 0 || !S_ISREG(st.st_mode)) {
            g_free(path);
            continue;
        }

        CiaoLogFile *file = g_new0(CiaoLogFile, 1);
        file->name = g_strdup(name);
        file->path = path;
        sorted = g_list_insert_sorted(sorted, file, log_file_cmp);
    }
    g_dir_close(handle);
    g_free(dir);

    int number = 1;
    for (GList *node = sorted; node != NULL; node = node->next) {
        CiaoLogFile *file = (CiaoLogFile *)node->data;
        file->number = number;
        number++;
        g_ptr_array_add(files, file);
    }
    g_list_free(sorted);
    return files;
}

gchar *ciao_log_read_file(const char *path, int *status)
{
    if (status != NULL) {
        *status = CIAO_LOG_OK;
    }
    if (path == NULL || path[0] == '\0') {
        if (status != NULL) {
            *status = CIAO_LOG_IO;
        }
        return NULL;
    }

    int fd = open(path, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) {
        if (status != NULL) {
            *status = CIAO_LOG_IO;
        }
        return NULL;
    }

    struct stat st;
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode)) {
        close(fd);
        if (status != NULL) {
            *status = CIAO_LOG_IO;
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
                *status = CIAO_LOG_IO;
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
            *status = CIAO_LOG_IO;
        }
        return NULL;
    }
    return g_string_free(buf, FALSE);
}

int ciao_log_clear_files(int *status, gchar **error_out)
{
    if (status != NULL) {
        *status = CIAO_LOG_OK;
    }
    if (error_out != NULL) {
        *error_out = NULL;
    }

    int list_status = CIAO_LOG_OK;
    GPtrArray *files = ciao_log_list_files(&list_status);
    if (list_status != CIAO_LOG_OK) {
        if (status != NULL) {
            *status = list_status;
        }
        if (files != NULL) {
            g_ptr_array_free(files, TRUE);
        }
        return 0;
    }

    int removed = 0;
    for (guint i = 0; i < files->len; i++) {
        CiaoLogFile *file = (CiaoLogFile *)g_ptr_array_index(files, i);
        if (file == NULL || file->path == NULL) {
            continue;
        }
        if (unlink(file->path) == 0) {
            removed++;
            continue;
        }
        if (errno == ENOENT) {
            continue;
        }
        if (status != NULL) {
            *status = CIAO_LOG_IO;
        }
        if (error_out != NULL && *error_out == NULL) {
            *error_out = g_strdup_printf("cannot delete log file %s",
                                         file->name != NULL ? file->name : "");
        }
        break;
    }

    g_ptr_array_free(files, TRUE);
    return removed;
}

static gboolean is_flag(const char *arg)
{
    return arg != NULL && strncmp(arg, "--", 2) == 0;
}

gboolean ciao_argv_has_log_verb(int argc, char **argv)
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
        if (strcmp(argv[i], "log") == 0) {
            return TRUE;
        }
    }
    return FALSE;
}

static void print_usage(void)
{
    if (ciao_json_mode) {
        g_print("{\"command\":\"log\",\"actions\":[\"list\",\"show\",\"folder\",\"clear\"]}\n");
        return;
    }
    g_print("Usage: %s log <action>\n\n", PROJECT_ID);
    g_print("Actions:\n");
    g_print("  list       List log files as a numbered list\n");
    g_print("  show N     Display log file number N\n");
    g_print("  folder     Display the log folder\n");
    g_print("  clear      Delete log files in the log folder\n");
}

static void fail_list_status(const char *action, int status)
{
    if (status == CIAO_LOG_UNRESOLVED) {
        log_fail(action, "log folder is unresolved");
    }
    if (status == CIAO_LOG_UNREADABLE) {
        log_fail(action, "cannot read log folder");
    }
}

static void cmd_list(void)
{
    int status = CIAO_LOG_OK;
    GPtrArray *files = ciao_log_list_files(&status);
    fail_list_status("list", status);

    gchar *folder = ciao_log_directory();
    if (folder == NULL) {
        if (files != NULL) {
            g_ptr_array_free(files, TRUE);
        }
        log_fail("list", "log folder is unresolved");
    }

    if (ciao_json_mode) {
        gchar *folder_json = json_escape_str(folder);
        GString *out = g_string_new("{\"command\":\"log\",\"action\":\"list\",\"folder\":\"");
        g_string_append(out, folder_json);
        g_string_append(out, "\",\"files\":[");
        g_free(folder_json);
        if (files != NULL) {
            for (guint i = 0; i < files->len; i++) {
                CiaoLogFile *file = (CiaoLogFile *)g_ptr_array_index(files, i);
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
            CiaoLogFile *file = (CiaoLogFile *)g_ptr_array_index(files, i);
            g_print("%d. %s\n", file->number, file->name != NULL ? file->name : "");
        }
    }

    g_free(folder);
    if (files != NULL) {
        g_ptr_array_free(files, TRUE);
    }
    exit(0);
}

static gboolean parse_log_number(const char *text, int *out)
{
    if (text == NULL || text[0] == '\0' || out == NULL) {
        return FALSE;
    }
    for (const char *p = text; *p != '\0'; p++) {
        if (*p < '0' || *p > '9') {
            return FALSE;
        }
    }
    errno = 0;
    char *end = NULL;
    long value = strtol(text, &end, 10);
    if (end == text || *end != '\0' || errno == ERANGE) {
        return FALSE;
    }
    if (value < 1 || value > INT_MAX) {
        return FALSE;
    }
    *out = (int)value;
    return TRUE;
}

static void cmd_show(const char *token)
{
    if (token == NULL) {
        log_fail("show", "missing log file number");
    }
    int number = 0;
    if (!parse_log_number(token, &number)) {
        gchar *msg = g_strdup_printf("no log file %s", token);
        log_fail("show", msg);
    }

    int status = CIAO_LOG_OK;
    GPtrArray *files = ciao_log_list_files(&status);
    fail_list_status("show", status);

    CiaoLogFile *file = NULL;
    if (files != NULL) {
        for (guint i = 0; i < files->len; i++) {
            CiaoLogFile *candidate = (CiaoLogFile *)g_ptr_array_index(files, i);
            if (candidate != NULL && candidate->number == number) {
                file = candidate;
                break;
            }
        }
    }
    if (file == NULL || file->path == NULL) {
        if (files != NULL) {
            g_ptr_array_free(files, TRUE);
        }
        gchar *msg = g_strdup_printf("no log file %s", token);
        log_fail("show", msg);
    }

    int read_status = CIAO_LOG_OK;
    gchar *content = ciao_log_read_file(file->path, &read_status);
    if (content == NULL || read_status != CIAO_LOG_OK) {
        g_free(content);
        gchar *msg = g_strdup_printf("cannot read log file %s",
                                     file->name != NULL ? file->name : "");
        g_ptr_array_free(files, TRUE);
        log_fail("show", msg);
    }

    if (ciao_json_mode) {
        gchar *name_json = json_escape_str(file->name);
        gchar *content_json = json_escape(content, strlen(content));
        g_print("{\"command\":\"log\",\"action\":\"show\",\"number\":%d,\"name\":\"%s\",\"content\":\"%s\"}\n",
                file->number, name_json, content_json);
        g_free(name_json);
        g_free(content_json);
    } else {
        g_print("%s", content);
    }

    g_free(content);
    g_ptr_array_free(files, TRUE);
    exit(0);
}

static void cmd_folder(void)
{
    gchar *folder = ciao_log_directory();
    if (folder == NULL) {
        log_fail("folder", "log folder is unresolved");
    }
    if (ciao_json_mode) {
        gchar *folder_json = json_escape_str(folder);
        g_print("{\"command\":\"log\",\"action\":\"folder\",\"folder\":\"%s\"}\n", folder_json);
        g_free(folder_json);
    } else {
        g_print("%s\n", folder);
    }
    g_free(folder);
    exit(0);
}

static void cmd_clear(void)
{
    int status = CIAO_LOG_OK;
    gchar *error = NULL;
    int removed = ciao_log_clear_files(&status, &error);
    if (status == CIAO_LOG_UNRESOLVED) {
        g_free(error);
        log_fail("clear", "log folder is unresolved");
    }
    if (status == CIAO_LOG_UNREADABLE) {
        g_free(error);
        log_fail("clear", "cannot read log folder");
    }
    if (status != CIAO_LOG_OK) {
        log_fail("clear", error != NULL ? error : "cannot delete log file");
    }
    g_free(error);

    gchar *folder = ciao_log_directory();
    if (folder == NULL) {
        log_fail("clear", "log folder is unresolved");
    }
    if (ciao_json_mode) {
        gchar *folder_json = json_escape_str(folder);
        g_print("{\"command\":\"log\",\"action\":\"clear\",\"folder\":\"%s\",\"removed\":%d}\n",
                folder_json, removed);
        g_free(folder_json);
    } else {
        g_print("Cleared %d log file(s) from %s\n", removed, folder);
    }
    g_free(folder);
    exit(0);
}

void ciao_log_command_dispatch(int argc, char **argv)
{
    const char *action = NULL;
    const char *show_token = NULL;
    const char *extra = NULL;
    gboolean seen_log = FALSE;

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
        if (!seen_log) {
            if (strcmp(argv[i], "log") == 0) {
                seen_log = TRUE;
            }
            continue;
        }
        if (action == NULL) {
            action = argv[i];
            continue;
        }
        if (show_token == NULL && strcmp(action, "show") == 0) {
            show_token = argv[i];
            continue;
        }
        extra = argv[i];
        break;
    }

    if (!seen_log) {
        log_fail("", "missing log command");
    }
    if (extra != NULL) {
        gchar *msg = g_strdup_printf("unexpected argument %s", extra);
        log_fail(action != NULL ? action : "", msg);
    }
    if (action == NULL) {
        print_usage();
        exit(0);
    }
    if (strcmp(action, "list") == 0) {
        cmd_list();
    }
    if (strcmp(action, "show") == 0) {
        cmd_show(show_token);
    }
    if (strcmp(action, "folder") == 0) {
        cmd_folder();
    }
    if (strcmp(action, "clear") == 0) {
        cmd_clear();
    }

    gchar *msg = g_strdup_printf("unknown log action %s", action);
    log_fail(action, msg);
}
