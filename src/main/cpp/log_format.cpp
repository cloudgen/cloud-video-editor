// log_format.cpp
// =========================================================================
// CIAO DEFENSIVE CODING STYLE - LOG FILE FORMAT CHECKER
// =========================================================================
//
// Law: requirement-cpp-log-format.md
// This file decides whether log bytes follow the stored line.
// logcheck is a test-purpose verb. It does not call ciao_*, it does not
// create a directory, and it does not read HOME, PREFIX, or the
// effective user id.
// The rehearsal verbs test, list, and show are not implemented here.
// Last aligned: 2026-10-06
// =========================================================================

#include "log_format.h"
#include "log_commands.h"
#include "output.h"
#include "project.h"

#include <stdlib.h>
#include <string.h>

static gchar *json_escape(const char *text)
{
    GString *out = g_string_new("");
    if (text == NULL) {
        return g_string_free(out, FALSE);
    }
    for (const char *cursor = text; *cursor != '\0'; cursor++) {
        unsigned char c = (unsigned char)*cursor;
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

static void usage_fail(const char *message)
{
    if (ciao_json_mode) {
        gchar *message_json = json_escape(message != NULL ? message : "");
        g_print("{\"command\":\"logcheck\",\"error\":\"%s\"}\n", message_json);
        g_free(message_json);
    } else if (message != NULL) {
        g_printerr("%s\n", message);
    }
    exit(1);
}

static gboolean is_flag(const char *arg)
{
    return arg != NULL && strncmp(arg, "--", 2) == 0;
}

static gboolean ends_with_log(const char *path)
{
    return path != NULL && g_str_has_suffix(path, LOGGER_FILE_EXT);
}

gboolean log_format_request_path(const char *text, gchar **path_out)
{
    if (path_out != NULL) {
        *path_out = NULL;
    }
    if (text == NULL || text[0] == '\0' || path_out == NULL) {
        return FALSE;
    }

    if (g_str_has_prefix(text, "file:")) {
        gchar *path = g_filename_from_uri(text, NULL, NULL);
        if (path == NULL || !ends_with_log(path)) {
            g_free(path);
            return FALSE;
        }
        *path_out = path;
        return TRUE;
    }

    if (text[0] == '/' && ends_with_log(text)) {
        *path_out = g_strdup(text);
        return TRUE;
    }
    return FALSE;
}

static gboolean take_digits(const char **cursor, int count, int *value)
{
    const char *text = *cursor;
    int number = 0;
    for (int i = 0; i < count; i++) {
        if (text[i] < '0' || text[i] > '9') {
            return FALSE;
        }
        number = number * 10 + (text[i] - '0');
    }
    *value = number;
    *cursor = text + count;
    return TRUE;
}

static gboolean level_token(const char *text, gsize length)
{
    if (length == 4 && strncmp(text, "INFO", 4) == 0) {
        return TRUE;
    }
    if (length == 5 && strncmp(text, "ERROR", 5) == 0) {
        return TRUE;
    }
    if (length == 5 && strncmp(text, "DEBUG", 5) == 0) {
        return TRUE;
    }
    if (length == 7 && strncmp(text, "WARNING", 7) == 0) {
        return TRUE;
    }
    return FALSE;
}

static gboolean stored_line(const char *line)
{
    const char *cursor;
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    const char *level;
    const char *level_end;

    if (line == NULL || line[0] != '[') {
        return FALSE;
    }
    cursor = line + 1;
    if (!take_digits(&cursor, 4, &year) || *cursor != '-') {
        return FALSE;
    }
    cursor++;
    if (!take_digits(&cursor, 2, &month) || month < 1 || month > 12 || *cursor != '-') {
        return FALSE;
    }
    cursor++;
    if (!take_digits(&cursor, 2, &day) || day < 1 || day > 31 || *cursor != ' ') {
        return FALSE;
    }
    cursor++;
    if (!take_digits(&cursor, 2, &hour) || hour > 23 || *cursor != ':') {
        return FALSE;
    }
    cursor++;
    if (!take_digits(&cursor, 2, &minute) || minute > 59 || *cursor != ':') {
        return FALSE;
    }
    cursor++;
    if (!take_digits(&cursor, 2, &second) || second > 60 || *cursor != ']') {
        return FALSE;
    }
    (void)year;
    cursor++;
    if (!g_str_has_prefix(cursor, " pid:")) {
        return FALSE;
    }
    cursor += 5;
    if (*cursor < '0' || *cursor > '9') {
        return FALSE;
    }
    while (*cursor >= '0' && *cursor <= '9') {
        cursor++;
    }
    if (!g_str_has_prefix(cursor, " [")) {
        return FALSE;
    }
    cursor += 2;
    level = cursor;
    level_end = strchr(cursor, ']');
    if (level_end == NULL || !level_token(level, (gsize)(level_end - level))) {
        return FALSE;
    }
    cursor = level_end + 1;
    if (g_str_has_prefix(cursor, " @")) {
        const char *name;
        gsize name_len;
        cursor += 2;
        name = cursor;
        if (*cursor == '\0' || *cursor == ' ' || *cursor == '/') {
            return FALSE;
        }
        while (*cursor != '\0' && *cursor != ' ') {
            if (*cursor == '/' || *cursor == '\r' || *cursor == '\n') {
                return FALSE;
            }
            cursor++;
        }
        name_len = (gsize)(cursor - name);
        if (name_len == 1 && name[0] == '.') {
            return FALSE;
        }
        if (name_len == 2 && name[0] == '.' && name[1] == '.') {
            return FALSE;
        }
    }
    return g_str_has_prefix(cursor, " :] ");
}

gboolean log_format_content_follows(const char *raw, gchar **reason_out)
{
    gchar **lines;
    gboolean any = FALSE;

    if (reason_out != NULL) {
        *reason_out = NULL;
    }
    lines = g_strsplit(raw != NULL ? raw : "", "\n", -1);
    for (guint i = 0; lines != NULL && lines[i] != NULL; i++) {
        gchar *line = lines[i];
        gsize len = strlen(line);
        if (len > 0 && line[len - 1] == '\r') {
            line[len - 1] = '\0';
        }
        if (line[0] == '\0') {
            if (lines[i + 1] == NULL) {
                continue;
            }
            if (reason_out != NULL) {
                *reason_out = g_strdup_printf("line %u does not follow the stored line", i + 1);
            }
            g_strfreev(lines);
            return FALSE;
        }
        if (!stored_line(line)) {
            if (reason_out != NULL) {
                *reason_out = g_strdup_printf("line %u does not follow the stored line", i + 1);
            }
            g_strfreev(lines);
            return FALSE;
        }
        any = TRUE;
    }
    g_strfreev(lines);
    if (!any) {
        if (reason_out != NULL) {
            *reason_out = g_strdup("no stored line");
        }
        return FALSE;
    }
    return TRUE;
}

gboolean ciao_argv_has_logcheck_verb(int argc, char **argv)
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
        if (strcmp(argv[i], "logcheck") == 0) {
            return TRUE;
        }
    }
    return FALSE;
}

static gboolean eight_digits(const char *text)
{
    if (text == NULL) {
        return FALSE;
    }
    for (int i = 0; i < 8; i++) {
        if (text[i] < '0' || text[i] > '9') {
            return FALSE;
        }
    }
    return text[8] == '\0';
}

static const char *take_value(int argc, char **argv, int *index, const char *flag)
{
    if (*index + 1 >= argc || argv[*index + 1] == NULL || argv[*index + 1][0] == '\0' ||
        is_flag(argv[*index + 1])) {
        gchar *msg = g_strdup_printf("missing value for %s", flag);
        usage_fail(msg);
        return NULL;
    }
    *index += 1;
    return argv[*index];
}

static gchar *rehearsal_path(const char *root, gboolean prefix_shape, const char *stamp)
{
    const char *root_segment = LOGGER_ROOT_PARENT;
    gchar *name;
    gchar *path;

    if (root_segment[0] == '/') {
        root_segment++;
    }
    name = g_strdup_printf("%s-%s%s", PROJECT_ID, stamp, LOGGER_FILE_EXT);
    if (prefix_shape) {
        path = g_build_filename(root, root_segment, PROJECT_ID, LOGGER_DIR_NAME, name, NULL);
    } else {
        path = g_build_filename(root, LOGGER_USER_PARENT, PROJECT_ID, LOGGER_DIR_NAME, name, NULL);
    }
    g_free(name);
    return path;
}

static void finish_pass(const char *path)
{
    if (ciao_json_mode) {
        gchar *path_json = json_escape(path != NULL ? path : "");
        g_print("{\"command\":\"logcheck\",\"result\":\"pass\",\"path\":\"%s\"}\n", path_json);
        g_free(path_json);
    } else {
        g_print("log format pass %s\n", path != NULL ? path : "");
    }
    exit(0);
}

static void finish_fail(const char *path, const char *reason)
{
    const char *why = (reason != NULL && reason[0] != '\0') ? reason : "no stored line";
    if (ciao_json_mode) {
        gchar *path_json = json_escape(path != NULL ? path : "");
        gchar *why_json = json_escape(why);
        g_print("{\"command\":\"logcheck\",\"result\":\"fail\",\"path\":\"%s\",\"error\":\"%s\"}\n",
                path_json, why_json);
        g_free(path_json);
        g_free(why_json);
    } else {
        g_print("log format fail %s: %s\n", path != NULL ? path : "", why);
    }
    exit(1);
}

static void check_path(const char *path)
{
    gchar *base;
    int status = CIAO_LOG_OK;
    gchar *content;
    gchar *reason = NULL;

    if (path == NULL || !ends_with_log(path)) {
        base = g_path_get_basename(path != NULL ? path : "");
        gchar *msg = g_strdup_printf("not a log file %s", base != NULL ? base : "");
        g_free(base);
        usage_fail(msg);
    }

    content = ciao_log_read_file(path, &status);
    if (content == NULL || status != CIAO_LOG_OK) {
        g_free(content);
        base = g_path_get_basename(path);
        gchar *msg = g_strdup_printf("cannot read log file %s", base != NULL ? base : "");
        g_free(base);
        usage_fail(msg);
    }
    if (!log_format_content_follows(content, &reason)) {
        g_free(content);
        finish_fail(path, reason);
    }
    g_free(reason);
    g_free(content);
    finish_pass(path);
}

void ciao_logcheck_command_dispatch(int argc, char **argv)
{
    const char *file_arg = NULL;
    const char *home_arg = NULL;
    const char *prefix_arg = NULL;
    const char *stamp_arg = NULL;
    gboolean seen = FALSE;
    const char *extra = NULL;

    for (int i = 1; argv != NULL && i < argc; i++) {
        const char *arg = argv[i];
        if (arg == NULL) {
            continue;
        }
        if (strcmp(arg, "--config") == 0) {
            i++;
            continue;
        }
        if (strcmp(arg, "--quiet") == 0 || strcmp(arg, "--json") == 0) {
            continue;
        }
        if (strncmp(arg, "--shutdown=", 11) == 0 || strncmp(arg, "--bookmark=", 11) == 0) {
            continue;
        }
        if (strcmp(arg, "--file") == 0) {
            file_arg = take_value(argc, argv, &i, "--file");
            continue;
        }
        if (strcmp(arg, "--home") == 0) {
            home_arg = take_value(argc, argv, &i, "--home");
            continue;
        }
        if (strcmp(arg, "--prefix") == 0) {
            prefix_arg = take_value(argc, argv, &i, "--prefix");
            continue;
        }
        if (strcmp(arg, "--log-datestamp") == 0) {
            stamp_arg = take_value(argc, argv, &i, "--log-datestamp");
            continue;
        }
        if (g_str_has_prefix(arg, "--file=")) {
            file_arg = arg + 7;
            continue;
        }
        if (g_str_has_prefix(arg, "--home=")) {
            home_arg = arg + 7;
            continue;
        }
        if (g_str_has_prefix(arg, "--prefix=")) {
            prefix_arg = arg + 9;
            continue;
        }
        if (g_str_has_prefix(arg, "--log-datestamp=")) {
            stamp_arg = arg + 16;
            continue;
        }
        if (is_flag(arg)) {
            gchar *msg = g_strdup_printf("unexpected argument %s", arg);
            usage_fail(msg);
        }
        if (!seen && strcmp(arg, "logcheck") == 0) {
            seen = TRUE;
            continue;
        }
        extra = arg;
        break;
    }

    if (!seen) {
        usage_fail("missing logcheck command");
    }
    if (extra != NULL) {
        gchar *msg = g_strdup_printf("unexpected argument %s", extra);
        usage_fail(msg);
    }
    if (file_arg != NULL && file_arg[0] == '\0') {
        usage_fail("missing value for --file");
    }
    if (home_arg != NULL && home_arg[0] == '\0') {
        usage_fail("missing value for --home");
    }
    if (prefix_arg != NULL && prefix_arg[0] == '\0') {
        usage_fail("missing value for --prefix");
    }
    if (file_arg != NULL && (home_arg != NULL || prefix_arg != NULL)) {
        usage_fail("logcheck accepts either --file or --home or --prefix");
    }
    if (file_arg != NULL && stamp_arg != NULL) {
        usage_fail("logcheck --file does not use --log-datestamp");
    }
    if (file_arg != NULL) {
        check_path(file_arg);
    }
    if (home_arg == NULL && prefix_arg == NULL) {
        usage_fail("logcheck requires --file or --home or --prefix");
    }
    if (stamp_arg == NULL || stamp_arg[0] == '\0') {
        usage_fail("test purpose command requires --log-datestamp YYYYMMDD");
    }
    if (!eight_digits(stamp_arg)) {
        usage_fail("--log-datestamp requires YYYYMMDD");
    }

    gboolean prefix_shape = prefix_arg != NULL;
    const char *root = prefix_shape ? prefix_arg : home_arg;
    gchar *path = rehearsal_path(root, prefix_shape, stamp_arg);
    check_path(path);
}
