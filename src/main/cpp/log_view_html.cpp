// log_view_html.cpp
// =========================================================================
// CIAO DEFENSIVE CODING STYLE - UI VIEW FOR AN APP LOG
// =========================================================================
//
// Law: requirement-cpp-log-commands.md
// Term: ui-view-for-app-log
// This file builds the HTML table. It does not read or write the log file
// and it does not call ciao_*.
// Last aligned: 2026-10-06
// =========================================================================

#include "log_view_html.h"

#include <ctype.h>
#include <string.h>

// ──────────────────────────────────────────────
// log_view_html_document() - HTML table for one log file
// ──────────────────────────────────────────────
//
// Purpose:
//   Turn the raw bytes of one log file into the ui-view-for-app-log sheet.
//   A stored line fills Time, Pid, Level, Source, and Message. Any other
//   non-empty line fills Message only. Enclosure characters are omitted
//   from every displayed cell. Source also drops a trailing .cpp or .c.
//   The bytes themselves are not modified here because this function only
//   returns a new string.
//
// Parameters:
//   Input:
//     - title : heading text, or NULL
//     - raw   : file bytes, or NULL. A trailing newline does not add a row.
//   Output/Return:
//     - gchar* - a complete HTML document. The caller frees it.
//
// Dependencies:
//   - Functions called: g_utf8_make_valid(), g_markup_escape_text(),
//     g_strsplit(), g_string_new()
//   - Headers required: glib.h, ctype.h, string.h
//
// Why This Design:
//   The command-line show path prints the file. This function is the sheet
//   the main web view loads. Keeping it free of GTK means the sheet rules
//   can be checked without opening the menu.
//
// CIAO Principles Applied:
//   - Caution: invalid UTF-8 is replaced before it is parsed
//   - Intentional: five columns, two stripe colors, one hover color
//   - Anti-fragile: a line that is not the stored shape still becomes a row
//   - Over-protect: enclosure characters and a trailing .cpp or .c are
//     removed only from displayed text
//
// Protection Rule:
//   Do not write the log file from this function.
//   Do not call this function from the log show command.
// ──────────────────────────────────────────────

static gboolean is_enclosure(char c)
{
    return c == '[' || c == ']' || c == '<' || c == '>' ||
           c == '(' || c == ')' || c == '{' || c == '}';
}

static gchar *source_display_name(const char *source)
{
    gsize len;

    if (source == NULL || source[0] == '\0') {
        return g_strdup("");
    }
    len = strlen(source);
    if (len > 4 && g_str_has_suffix(source, ".cpp")) {
        return g_strndup(source, len - 4);
    }
    if (len > 2 && g_str_has_suffix(source, ".c")) {
        return g_strndup(source, len - 2);
    }
    return g_strdup(source);
}

static gchar *cell_text(const char *text)
{
    GString *stripped;
    gchar *escaped;
    const char *cursor;

    stripped = g_string_new("");
    if (text != NULL) {
        for (cursor = text; *cursor != '\0'; cursor++) {
            if (!is_enclosure(*cursor)) {
                g_string_append_c(stripped, *cursor);
            }
        }
    }
    escaped = g_markup_escape_text(stripped->str, (gssize)stripped->len);
    g_string_free(stripped, TRUE);
    if (escaped == NULL) {
        return g_strdup("");
    }
    return escaped;
}

static gboolean parse_stored_line(const char *line,
                                  gchar **time_out,
                                  gchar **pid_out,
                                  gchar **level_out,
                                  gchar **source_out,
                                  gchar **message_out)
{
    const char *cursor;
    const char *end;
    gchar *time_text = NULL;
    gchar *pid_text = NULL;
    gchar *level_text = NULL;
    gchar *source_text = NULL;
    gchar *message_text = NULL;

    if (line == NULL || line[0] != '[') {
        return FALSE;
    }
    cursor = line + 1;
    end = strchr(cursor, ']');
    if (end == NULL) {
        return FALSE;
    }
    time_text = g_strndup(cursor, (gsize)(end - cursor));
    cursor = end + 1;
    if (!g_str_has_prefix(cursor, " pid:")) {
        g_free(time_text);
        return FALSE;
    }
    cursor += 5;
    end = cursor;
    if (!isdigit((unsigned char)*end)) {
        g_free(time_text);
        return FALSE;
    }
    while (isdigit((unsigned char)*end)) {
        end++;
    }
    pid_text = g_strndup(cursor, (gsize)(end - cursor));
    cursor = end;
    if (!g_str_has_prefix(cursor, " [")) {
        g_free(time_text);
        g_free(pid_text);
        return FALSE;
    }
    cursor += 2;
    end = strchr(cursor, ']');
    if (end == NULL) {
        g_free(time_text);
        g_free(pid_text);
        return FALSE;
    }
    level_text = g_strndup(cursor, (gsize)(end - cursor));
    cursor = end + 1;
    if (g_str_has_prefix(cursor, " @")) {
        cursor += 2;
        end = cursor;
        while (*end != '\0' && *end != ' ') {
            end++;
        }
        source_text = g_strndup(cursor, (gsize)(end - cursor));
        cursor = end;
    } else {
        source_text = g_strdup("");
    }
    if (!g_str_has_prefix(cursor, " :] ")) {
        g_free(time_text);
        g_free(pid_text);
        g_free(level_text);
        g_free(source_text);
        return FALSE;
    }
    message_text = g_strdup(cursor + 4);

    *time_out = time_text;
    *pid_out = pid_text;
    *level_out = level_text;
    *source_out = source_text;
    *message_out = message_text;
    return TRUE;
}

static void append_row(GString *html,
                       const char *time_text,
                       const char *pid_text,
                       const char *level_text,
                       const char *source_text,
                       const char *message_text)
{
    gchar *source_name = source_display_name(source_text);
    gchar *time_cell = cell_text(time_text);
    gchar *pid_cell = cell_text(pid_text);
    gchar *level_cell = cell_text(level_text);
    gchar *source_cell = cell_text(source_name);
    gchar *message_cell = cell_text(message_text);
    g_free(source_name);

    g_string_append_printf(html,
                           "<tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>\n",
                           time_cell,
                           pid_cell,
                           level_cell,
                           source_cell,
                           message_cell);
    g_free(time_cell);
    g_free(pid_cell);
    g_free(level_cell);
    g_free(source_cell);
    g_free(message_cell);
}

gchar *log_view_html_document(const char *title, const char *raw)
{
    gchar *valid;
    gchar **lines;
    GString *html;
    gchar *heading;
    guint i;

    valid = g_utf8_make_valid(raw != NULL ? raw : "", -1);
    heading = cell_text(title != NULL ? title : "");
    html = g_string_new("");
    g_string_append(html,
                    "<!DOCTYPE html>\n"
                    "<html>\n"
                    "<head>\n"
                    "<meta charset=\"utf-8\">\n"
                    "<title>");
    g_string_append(html, heading);
    g_string_append(html,
                    "</title>\n"
                    "<style>\n"
                    "body { margin: 16px; font-family: sans-serif; color: #1a1a1a; background: #f4f6f8; }\n"
                    "h1 { font-size: 1.15rem; margin: 0 0 12px; }\n"
                    "table { border-collapse: collapse; width: 100%; background: #ffffff; }\n"
                    "th { background: #1f4e79; color: #ffffff; text-align: left; font-weight: 700;\n"
                    "     padding: 8px 10px; border-bottom: 2px solid #163a5a; }\n"
                    "td { padding: 6px 10px; vertical-align: top; border-bottom: 1px solid #d5dde6; }\n"
                    "tbody tr:nth-child(odd) { background: #ffffff; }\n"
                    "tbody tr:nth-child(even) { background: #e7eef6; }\n"
                    "tbody tr:hover { background: #ffe08a; }\n"
                    "</style>\n"
                    "</head>\n"
                    "<body>\n"
                    "<h1>");
    g_string_append(html, heading);
    g_string_append(html,
                    "</h1>\n"
                    "<table>\n"
                    "<thead><tr>"
                    "<th scope=\"col\">Time</th>"
                    "<th scope=\"col\">Pid</th>"
                    "<th scope=\"col\">Level</th>"
                    "<th scope=\"col\">Source</th>"
                    "<th scope=\"col\">Message</th>"
                    "</tr></thead>\n"
                    "<tbody>\n");
    g_free(heading);

    lines = g_strsplit(valid != NULL ? valid : "", "\n", -1);
    g_free(valid);
    for (i = 0; lines != NULL && lines[i] != NULL; i++) {
        gchar *line = lines[i];
        gsize len;
        gchar *time_text = NULL;
        gchar *pid_text = NULL;
        gchar *level_text = NULL;
        gchar *source_text = NULL;
        gchar *message_text = NULL;

        len = strlen(line);
        if (len > 0 && line[len - 1] == '\r') {
            line[len - 1] = '\0';
        }
        if (line[0] == '\0') {
            continue;
        }
        if (parse_stored_line(line, &time_text, &pid_text, &level_text,
                              &source_text, &message_text)) {
            append_row(html, time_text, pid_text, level_text, source_text, message_text);
        } else {
            append_row(html, "", "", "", "", line);
        }
        g_free(time_text);
        g_free(pid_text);
        g_free(level_text);
        g_free(source_text);
        g_free(message_text);
    }
    g_strfreev(lines);

    g_string_append(html, "</tbody>\n</table>\n</body>\n</html>\n");
    return g_string_free(html, FALSE);
}
