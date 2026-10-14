// gui_window.cpp
// VideoTools picture window. Edit is the page that opens. Join shares the
// same stack. FFmpeg stays on a worker thread. GTK is touched on this thread.

#include "gui_window.h"

#include "gui_format.h"
#include "gui_player.h"
#include "output.h"
#include "project.h"
#include "video_host.h"
#include "video_job.h"

#include <gtk/gtk.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <direct.h>
#include <sys/stat.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {

const char *WAIT_PHRASE = "please wait for time consuming process";

struct Transport {
    GuiPlayer *player;
    GtkWidget *elapsed;
    GtkWidget *total;
    GtkWidget *scale;
    bool suppress;

    Transport() : player(NULL), elapsed(NULL), total(NULL), scale(NULL), suppress(false) {}
};

struct App {
    GtkWidget *window;
    GtkWidget *stack;
    GtkWidget *join_tab;
    GtkWidget *edit_tab;
    GtkWidget *path_label;
    GtkWidget *group_frame;
    bool tab_guard;
    bool busy;
    bool close_requested;
    GtkWidget *active_result;
    std::string page;

    std::string edit_path;
    double edit_duration;
    GtkWidget *edit_file;
    GtkWidget *edit_start;
    GtkWidget *edit_end;
    GtkWidget *edit_jump;
    GtkWidget *edit_percent;
    GtkWidget *edit_tail_none;
    GtkWidget *edit_tail_boom;
    GtkWidget *edit_tail_repeat;
    GtkWidget *edit_output;
    GtkWidget *edit_result;
    GtkWidget *edit_run;
    Transport edit_source;
    Transport edit_generated;

    std::string join_first;
    std::string join_second;
    bool join_name_custom;
    bool join_name_write;
    GtkWidget *join_file_1;
    GtkWidget *join_file_2;
    GtkWidget *join_name;
    GtkWidget *join_write;
    GtkWidget *join_result;
    GtkWidget *join_run;
    Transport join_a;
    Transport join_b;
    Transport join_out;

    GtkWidget *log_folder;
    GtkWidget *log_list;
    GtkWidget *log_status;
    GtkTextBuffer *log_buffer;
    std::string log_dir;

    std::vector<GtkWidget *> playback_buttons;

    App()
        : window(NULL), stack(NULL), join_tab(NULL), edit_tab(NULL), path_label(NULL),
          group_frame(NULL), tab_guard(false), busy(false), close_requested(false),
          active_result(NULL), edit_duration(0.0), edit_file(NULL), edit_start(NULL),
          edit_end(NULL), edit_jump(NULL), edit_percent(NULL), edit_tail_none(NULL),
          edit_tail_boom(NULL), edit_tail_repeat(NULL), edit_output(NULL), edit_result(NULL),
          edit_run(NULL), join_name_custom(false), join_name_write(false), join_file_1(NULL),
          join_file_2(NULL), join_name(NULL), join_write(NULL), join_result(NULL), join_run(NULL),
          log_folder(NULL), log_list(NULL), log_status(NULL), log_buffer(NULL) {}
};

struct EncodeRequest {
    App *app;
    bool edit;
    std::string first;
    std::string second;
    std::string output_name;
    std::string tail;
    double start;
    double end;
    double percent;
    GtkWidget *result;
    GuiPlayer *generated;
    bool ok;
    std::string user_text;
    std::string output_path;

    EncodeRequest()
        : app(NULL), edit(false), start(0.0), end(0.0), percent(100.0), result(NULL),
          generated(NULL), ok(false) {}
};

std::string current_dir()
{
#if defined(_WIN32)
    char buf[MAX_PATH];
    if (_getcwd(buf, MAX_PATH) == NULL) {
        return ".";
    }
    return buf;
#else
    char buf[4096];
    if (getcwd(buf, sizeof(buf)) == NULL) {
        return ".";
    }
    return buf;
#endif
}

std::string absolute_cwd()
{
    std::string real = host_real_path(current_dir());
    if (!real.empty()) {
        return real;
    }
    return current_dir();
}

std::string executable_path()
{
#if defined(_WIN32)
    wchar_t wide[MAX_PATH];
    DWORD n = GetModuleFileNameW(NULL, wide, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) {
        return "";
    }
    int bytes = WideCharToMultiByte(CP_UTF8, 0, wide, -1, NULL, 0, NULL, NULL);
    if (bytes <= 0) {
        return "";
    }
    std::string out(static_cast<size_t>(bytes), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, &out[0], bytes, NULL, NULL);
    if (!out.empty() && out[out.size() - 1] == '\0') {
        out.erase(out.size() - 1);
    }
    return out;
#else
    char buf[4096];
    ssize_t n = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n < 0) {
        return "";
    }
    buf[n] = '\0';
    return buf;
#endif
}

std::string lower_copy(const std::string &text)
{
    std::string out = text;
    for (size_t i = 0; i < out.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(out[i]);
        if (c >= 'A' && c <= 'Z') {
            out[i] = static_cast<char>(c - 'A' + 'a');
        }
    }
    return out;
}

bool regular_file(const std::string &path)
{
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        return false;
    }
    return S_ISREG(st.st_mode) != 0;
}

bool suffix_is(const std::string &path, const char *const *allowed)
{
    std::string base = host_basename(path);
    size_t dot = base.find_last_of('.');
    if (dot == std::string::npos) {
        return false;
    }
    std::string suffix = lower_copy(base.substr(dot));
    for (size_t i = 0; allowed[i] != NULL; ++i) {
        if (suffix == allowed[i]) {
            return true;
        }
    }
    return false;
}

bool looks_remote(const std::string &path)
{
    return path.find("://") != std::string::npos;
}

bool path_inside(const std::string &file, const std::string &dir)
{
    std::string real_file = host_real_path(file);
    std::string real_dir = host_real_path(dir);
    if (real_file.empty() || real_dir.empty() || real_file == real_dir) {
        return false;
    }
    std::string prefix = real_dir;
    char last = prefix[prefix.size() - 1];
    if (last != '/' && last != '\\') {
#if defined(_WIN32)
        prefix.push_back('\\');
#else
        prefix.push_back('/');
#endif
    }
    return real_file.compare(0, prefix.size(), prefix) == 0;
}

void set_label(GtkWidget *label, const std::string &text)
{
    if (label != NULL) {
        gtk_label_set_text(GTK_LABEL(label), text.c_str());
    }
}

GtkWidget *hbox(int spacing)
{
    return gtk_box_new(GTK_ORIENTATION_HORIZONTAL, spacing);
}

GtkWidget *vbox(int spacing)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, spacing);
    gtk_widget_set_hexpand(box, TRUE);
    gtk_widget_set_vexpand(box, TRUE);
    return box;
}

void pack(GtkWidget *box, GtkWidget *child, gboolean expand)
{
    gtk_box_pack_start(GTK_BOX(box), child, expand, expand, 0);
}

GtkWidget *button(const char *text, GCallback callback, gpointer data)
{
    GtkWidget *widget = gtk_button_new_with_label(text);
    g_signal_connect(widget, "clicked", callback, data);
    return widget;
}

void on_player_tick(GuiPlayer *player, double position, double duration, void *user)
{
    (void)player;
    Transport *transport = static_cast<Transport *>(user);
    set_label(transport->elapsed, gui_format_elapsed(position));
    set_label(transport->total, gui_format_elapsed(duration));
    if (duration > 0.0 && transport->scale != NULL) {
        transport->suppress = true;
        gtk_range_set_range(GTK_RANGE(transport->scale), 0.0, duration);
        double shown = position;
        if (shown < 0.0) {
            shown = 0.0;
        }
        if (shown > duration) {
            shown = duration;
        }
        gtk_range_set_value(GTK_RANGE(transport->scale), shown);
        transport->suppress = false;
    }
}

void on_scale(GtkRange *range, gpointer user)
{
    Transport *transport = static_cast<Transport *>(user);
    if (transport->suppress || transport->player == NULL) {
        return;
    }
    gui_player_seek(transport->player, gtk_range_get_value(range));
}

void wire_transport(Transport *transport)
{
    transport->elapsed = gtk_label_new("00:00.00");
    transport->total = gtk_label_new("00:00.00");
    transport->scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 1.0, 0.01);
    gtk_scale_set_draw_value(GTK_SCALE(transport->scale), FALSE);
    gtk_widget_set_hexpand(transport->scale, TRUE);
    gui_player_set_tick(transport->player, on_player_tick, transport);
    g_signal_connect(transport->scale, "value-changed", G_CALLBACK(on_scale), transport);
}

GtkWidget *transport_row(Transport *transport)
{
    GtkWidget *row = hbox(8);
    pack(row, transport->elapsed, FALSE);
    pack(row, transport->scale, TRUE);
    pack(row, transport->total, FALSE);
    return row;
}

std::string edit_tail(App *app)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->edit_tail_boom))) {
        return "boomerang";
    }
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->edit_tail_repeat))) {
        return "repeat";
    }
    return "none";
}

void refresh_path(App *app)
{
    set_label(app->path_label, "Path: " + absolute_cwd());
}

void refresh_edit_output(App *app)
{
    double start = 0.0;
    double end = 0.0;
    const char *start_text = gtk_entry_get_text(GTK_ENTRY(app->edit_start));
    const char *end_text = gtk_entry_get_text(GTK_ENTRY(app->edit_end));
    bool times = gui_parse_elapsed(start_text, start) && gui_parse_elapsed(end_text, end);
    if (app->edit_path.empty() || !times) {
        set_label(app->edit_output, "Output");
    } else {
        double percent = gtk_spin_button_get_value(GTK_SPIN_BUTTON(app->edit_percent));
        std::string path = video_edit_output_name(app->edit_path, start, end, percent, edit_tail(app));
        set_label(app->edit_output, "Output  " + path);
    }
    gboolean ready = !app->busy && !app->edit_path.empty() && times && start < end;
    gtk_widget_set_sensitive(app->edit_run, ready);
}

void refresh_join_write(App *app)
{
    if (app->join_first.empty() || app->join_second.empty()) {
        set_label(app->join_write, "Write to  the current folder");
        gtk_widget_set_sensitive(app->join_run, FALSE);
        return;
    }
    std::string raw;
    if (app->join_name_custom) {
        raw = gtk_entry_get_text(GTK_ENTRY(app->join_name));
    }
    std::string full = video_join_output_name(app->join_first, app->join_second, raw);
    if (!app->join_name_custom) {
        app->join_name_write = true;
        gtk_entry_set_text(GTK_ENTRY(app->join_name), host_basename(full).c_str());
        app->join_name_write = false;
    }
    set_label(app->join_write, "Write to  " + full);
    gtk_widget_set_sensitive(app->join_run, !app->busy);
}

void pause_edit(App *app)
{
    gui_player_pause_mute(app->edit_source.player);
    gui_player_pause_mute(app->edit_generated.player);
}

void pause_join(App *app)
{
    gui_player_pause_mute(app->join_a.player);
    gui_player_pause_mute(app->join_b.player);
    gui_player_pause_mute(app->join_out.player);
}

void mark_tabs(App *app)
{
    app->tab_guard = true;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->edit_tab), app->page == "edit");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->join_tab), app->page == "join");
    app->tab_guard = false;
    GtkStyleContext *context = gtk_widget_get_style_context(app->group_frame);
    if (app->page == "edit" || app->page == "join") {
        gtk_style_context_add_class(context, "group-open");
    } else {
        gtk_style_context_remove_class(context, "group-open");
    }
}

void show_page(App *app, const std::string &page);

void on_group_tab(GtkToggleButton *button, gpointer user)
{
    App *app = static_cast<App *>(user);
    if (app->tab_guard) {
        return;
    }
    const char *page = button == GTK_TOGGLE_BUTTON(app->join_tab) ? "join" : "edit";
    if (gtk_toggle_button_get_active(button)) {
        show_page(app, page);
        return;
    }
    if (app->page == page) {
        app->tab_guard = true;
        gtk_toggle_button_set_active(button, TRUE);
        app->tab_guard = false;
    }
}

void show_page(App *app, const std::string &page)
{
    if (page != "edit") {
        pause_edit(app);
    }
    if (page != "join") {
        pause_join(app);
    }
    app->page = page;
    gtk_stack_set_visible_child_name(GTK_STACK(app->stack), page.c_str());
    mark_tabs(app);
    refresh_path(app);
    if (page == "log") {
        // Filled by the caller after the list exists. show_page is used before
        // the log widgets exist only for the first edit page.
    }
}

void on_page_clicked(GtkButton *button, gpointer user)
{
    (void)button;
    const char *page = static_cast<const char *>(g_object_get_data(G_OBJECT(button), "page"));
    show_page(static_cast<App *>(user), page);
}

gboolean encode_done(gpointer data)
{
    EncodeRequest *request = static_cast<EncodeRequest *>(data);
    App *app = request->app;
    set_label(request->result, request->user_text);
    app->busy = false;
    if (request->ok && !request->output_path.empty() && request->generated != NULL) {
        gui_player_set_duration_hint(request->generated, 0.0);
        gui_player_load(request->generated, request->output_path);
    }
    refresh_edit_output(app);
    refresh_join_write(app);
    if (app->close_requested && app->window != NULL) {
        gtk_widget_destroy(app->window);
    }
    delete request;
    return G_SOURCE_REMOVE;
}

void encode_worker(EncodeRequest *request)
{
    VideoJobResult result;
    if (request->edit) {
        result = video_edit_encode(request->first, request->start, request->end, request->percent, request->tail);
    } else {
        result = video_join_encode(request->first, request->second, request->output_name);
    }
    request->ok = result.ok;
    request->user_text = result.user_text;
    request->output_path = result.output_path;
    g_idle_add(encode_done, request);
}

void start_encode(App *app, EncodeRequest *request)
{
    gui_player_pause_all();
    if (request->generated != NULL) {
        gui_player_clear(request->generated);
    }
    app->busy = true;
    app->active_result = request->result;
    set_label(request->result, WAIT_PHRASE);
    gtk_widget_set_sensitive(app->edit_run, FALSE);
    gtk_widget_set_sensitive(app->join_run, FALSE);
    std::thread(encode_worker, request).detach();
}

void on_edit_changed(GtkWidget *widget, gpointer user)
{
    (void)widget;
    refresh_edit_output(static_cast<App *>(user));
}

void on_edit_select(GtkButton *button, gpointer user)
{
    (void)button;
    App *app = static_cast<App *>(user);
    GtkWidget *dialog = gtk_file_chooser_dialog_new(
        "Select MP4", GTK_WINDOW(app->window), GTK_FILE_CHOOSER_ACTION_OPEN,
        "Cancel", GTK_RESPONSE_CANCEL, "Open", GTK_RESPONSE_ACCEPT, NULL);
    gtk_file_chooser_set_local_only(GTK_FILE_CHOOSER(dialog), TRUE);
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), current_dir().c_str());
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "MP4");
    gtk_file_filter_add_pattern(filter, "*.mp4");
    gtk_file_filter_add_pattern(filter, "*.MP4");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
    std::string path;
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *name = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        if (name != NULL) {
            path = name;
            g_free(name);
        }
    }
    gtk_widget_destroy(dialog);
    if (path.empty()) {
        return;
    }
    const char *mp4[] = {".mp4", NULL};
    if (looks_remote(path) || !regular_file(path) || !suffix_is(path, mp4)) {
        set_label(app->edit_result, "Edit accepts an MP4 file.");
        return;
    }
    double seconds = 0.0;
    std::string error;
    if (!video_probe_duration(path, seconds, error)) {
        set_label(app->edit_result, "Could not read duration for " + host_basename(path) + ".");
        return;
    }
    app->edit_path = path;
    app->edit_duration = seconds;
    set_label(app->edit_file, path);
    gtk_entry_set_text(GTK_ENTRY(app->edit_start), "00:00.00");
    gtk_entry_set_text(GTK_ENTRY(app->edit_end), gui_format_elapsed(seconds).c_str());
    gui_player_set_duration_hint(app->edit_source.player, seconds);
    gui_player_load(app->edit_source.player, path);
    set_label(app->edit_result, "");
    refresh_edit_output(app);
}

void on_edit_transport(GtkButton *button, gpointer user)
{
    App *app = static_cast<App *>(user);
    const char *action = static_cast<const char *>(g_object_get_data(G_OBJECT(button), "action"));
    GuiPlayer *player = app->edit_source.player;
    if (std::strcmp(action, "start") == 0) {
        gui_player_play(player);
    } else if (std::strcmp(action, "pause") == 0) {
        gui_player_pause(player);
    } else if (std::strcmp(action, "stop") == 0) {
        gui_player_stop(player);
    }
}

void on_generated_transport(GtkButton *button, gpointer user)
{
    App *app = static_cast<App *>(user);
    const char *action = static_cast<const char *>(g_object_get_data(G_OBJECT(button), "action"));
    if (std::strcmp(action, "play") == 0) {
        gui_player_play(app->edit_generated.player);
    } else {
        gui_player_stop(app->edit_generated.player);
    }
}

void say_edit(App *app, const std::string &text)
{
    set_label(app->edit_result, text);
}

void on_edit_go(GtkButton *button, gpointer user)
{
    (void)button;
    App *app = static_cast<App *>(user);
    double seconds = 0.0;
    if (!gui_parse_elapsed(gtk_entry_get_text(GTK_ENTRY(app->edit_jump)), seconds)) {
        say_edit(app, "Jump uses 00:00.00.");
        return;
    }
    double duration = gui_player_duration(app->edit_source.player);
    if (duration <= 0.0) {
        duration = app->edit_duration;
    }
    if (duration > 0.0 && seconds > duration + 0.001) {
        say_edit(app, "Jump is past the end of this file.");
        return;
    }
    gui_player_seek(app->edit_source.player, seconds);
}

void on_mark(GtkButton *button, gpointer user)
{
    App *app = static_cast<App *>(user);
    const char *which = static_cast<const char *>(g_object_get_data(G_OBJECT(button), "mark"));
    std::string text = gui_format_elapsed(gui_player_position(app->edit_source.player));
    if (std::strcmp(which, "start") == 0) {
        gtk_entry_set_text(GTK_ENTRY(app->edit_start), text.c_str());
    } else {
        gtk_entry_set_text(GTK_ENTRY(app->edit_end), text.c_str());
    }
    refresh_edit_output(app);
}

void on_edit_run(GtkButton *button, gpointer user)
{
    (void)button;
    App *app = static_cast<App *>(user);
    if (app->busy) {
        return;
    }
    const char *mp4[] = {".mp4", NULL};
    if (app->edit_path.empty() || looks_remote(app->edit_path) || !regular_file(app->edit_path) ||
        !suffix_is(app->edit_path, mp4)) {
        say_edit(app, "Edit accepts an MP4 file.");
        return;
    }
    double start = 0.0;
    double end = 0.0;
    if (!gui_parse_elapsed(gtk_entry_get_text(GTK_ENTRY(app->edit_start)), start) ||
        !gui_parse_elapsed(gtk_entry_get_text(GTK_ENTRY(app->edit_end)), end) || !(start < end)) {
        say_edit(app, "Set a start and an end. The end is after the start.");
        return;
    }
    double percent = gtk_spin_button_get_value(GTK_SPIN_BUTTON(app->edit_percent));
    if (percent < 20.0 || percent > 200.0) {
        say_edit(app, "Percent must be 20–200.");
        return;
    }
    double duration = 0.0;
    std::string error;
    if (!video_probe_duration(app->edit_path, duration, error)) {
        say_edit(app, "Could not read duration for " + host_basename(app->edit_path) + ".");
        return;
    }
    app->edit_duration = duration;
    if (end > duration + 0.001) {
        say_edit(app, "End is past the duration " + gui_format_elapsed(duration) + ".");
        return;
    }
    EncodeRequest *request = new EncodeRequest();
    request->app = app;
    request->edit = true;
    request->first = app->edit_path;
    request->start = start;
    request->end = end;
    request->percent = percent;
    request->tail = edit_tail(app);
    request->result = app->edit_result;
    request->generated = app->edit_generated.player;
    start_encode(app, request);
}

const char *JOIN_SUFFIXES[] = {".mp4", ".mov", ".mkv", ".avi", ".m4v", NULL};

void on_join_select(GtkButton *button, gpointer user)
{
    App *app = static_cast<App *>(user);
    int slot = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "slot"));
    GtkWidget *dialog = gtk_file_chooser_dialog_new(
        "Select video", GTK_WINDOW(app->window), GTK_FILE_CHOOSER_ACTION_OPEN,
        "Cancel", GTK_RESPONSE_CANCEL, "Open", GTK_RESPONSE_ACCEPT, NULL);
    gtk_file_chooser_set_local_only(GTK_FILE_CHOOSER(dialog), TRUE);
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), current_dir().c_str());
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Video");
    gtk_file_filter_add_pattern(filter, "*.mp4");
    gtk_file_filter_add_pattern(filter, "*.mov");
    gtk_file_filter_add_pattern(filter, "*.mkv");
    gtk_file_filter_add_pattern(filter, "*.avi");
    gtk_file_filter_add_pattern(filter, "*.m4v");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
    std::string path;
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *name = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        if (name != NULL) {
            path = name;
            g_free(name);
        }
    }
    gtk_widget_destroy(dialog);
    if (path.empty()) {
        return;
    }
    if (looks_remote(path) || !regular_file(path) || !suffix_is(path, JOIN_SUFFIXES)) {
        set_label(app->join_result, "That file is not a join video.");
        return;
    }
    const std::string &other = slot == 1 ? app->join_second : app->join_first;
    if (!other.empty() && video_same_path(path, other)) {
        set_label(app->join_result, "Choose two different files.");
        return;
    }
    double seconds = 0.0;
    std::string error;
    video_probe_duration(path, seconds, error);
    if (slot == 1) {
        app->join_first = path;
        set_label(app->join_file_1, path);
        gui_player_set_duration_hint(app->join_a.player, seconds);
        gui_player_load(app->join_a.player, path);
    } else {
        app->join_second = path;
        set_label(app->join_file_2, path);
        gui_player_set_duration_hint(app->join_b.player, seconds);
        gui_player_load(app->join_b.player, path);
    }
    set_label(app->join_result, "");
    refresh_join_write(app);
}

void on_join_name(GtkEditable *editable, gpointer user)
{
    (void)editable;
    App *app = static_cast<App *>(user);
    if (app->join_name_write) {
        return;
    }
    app->join_name_custom = true;
    refresh_join_write(app);
}

void on_join_transport(GtkButton *button, gpointer user)
{
    App *app = static_cast<App *>(user);
    int slot = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "slot"));
    const char *action = static_cast<const char *>(g_object_get_data(G_OBJECT(button), "action"));
    GuiPlayer *player = app->join_a.player;
    if (slot == 2) {
        player = app->join_b.player;
    } else if (slot == 3) {
        player = app->join_out.player;
    }
    if (std::strcmp(action, "play") == 0) {
        gui_player_play(player);
    } else {
        gui_player_stop(player);
    }
}

void on_join_run(GtkButton *button, gpointer user)
{
    (void)button;
    App *app = static_cast<App *>(user);
    if (app->busy) {
        return;
    }
    if (app->join_first.empty() || app->join_second.empty() || looks_remote(app->join_first) ||
        looks_remote(app->join_second) || !regular_file(app->join_first) || !regular_file(app->join_second) ||
        !suffix_is(app->join_first, JOIN_SUFFIXES) || !suffix_is(app->join_second, JOIN_SUFFIXES)) {
        set_label(app->join_result, "That file is not a join video.");
        return;
    }
    if (video_same_path(app->join_first, app->join_second)) {
        set_label(app->join_result, "Choose two different files.");
        return;
    }
    EncodeRequest *request = new EncodeRequest();
    request->app = app;
    request->edit = false;
    request->first = app->join_first;
    request->second = app->join_second;
    request->output_name = gtk_entry_get_text(GTK_ENTRY(app->join_name));
    request->result = app->join_result;
    request->generated = app->join_out.player;
    start_encode(app, request);
}

void clear_list(GtkWidget *list)
{
    GList *children = gtk_container_get_children(GTK_CONTAINER(list));
    for (GList *item = children; item != NULL; item = item->next) {
        gtk_widget_destroy(GTK_WIDGET(item->data));
    }
    g_list_free(children);
}

void fill_logs(App *app)
{
    gchar *dir = ciao_log_directory();
    if (dir == NULL || dir[0] == '\0') {
        app->log_dir.clear();
        set_label(app->log_folder, "No log folder.");
    } else {
        app->log_dir = dir;
        set_label(app->log_folder, std::string("Log folder: ") + dir);
    }
    g_free(dir);
    clear_list(app->log_list);
    if (app->log_dir.empty()) {
        return;
    }
    GDir *folder = g_dir_open(app->log_dir.c_str(), 0, NULL);
    if (folder == NULL) {
        return;
    }
    const gchar *name = NULL;
    while ((name = g_dir_read_name(folder)) != NULL) {
        if (!g_str_has_suffix(name, ".log") && !g_str_has_suffix(name, ".LOG")) {
            continue;
        }
        std::string full = host_join_path(app->log_dir, name);
        if (!regular_file(full)) {
            continue;
        }
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *label = gtk_label_new(name);
        gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
        gtk_container_add(GTK_CONTAINER(row), label);
        g_object_set_data_full(G_OBJECT(row), "path", g_strdup(full.c_str()), g_free);
        gtk_container_add(GTK_CONTAINER(app->log_list), row);
    }
    g_dir_close(folder);
    gtk_widget_show_all(app->log_list);
}

const char *selected_log(App *app)
{
    GtkListBoxRow *row = gtk_list_box_get_selected_row(GTK_LIST_BOX(app->log_list));
    if (row == NULL) {
        return NULL;
    }
    return static_cast<const char *>(g_object_get_data(G_OBJECT(row), "path"));
}

void on_log_view(GtkButton *button, gpointer user)
{
    (void)button;
    App *app = static_cast<App *>(user);
    const char *path = selected_log(app);
    if (path == NULL) {
        set_label(app->log_status, "Select a log file.");
        return;
    }
    if (app->log_dir.empty() || !path_inside(path, app->log_dir)) {
        set_label(app->log_status, "That file is outside the log folder.");
        return;
    }
    gchar *contents = NULL;
    gsize length = 0;
    if (!g_file_get_contents(path, &contents, &length, NULL)) {
        set_label(app->log_status, "That file is outside the log folder.");
        return;
    }
    if (length == 0) {
        gtk_text_buffer_set_text(app->log_buffer, "(empty)", -1);
    } else {
        gtk_text_buffer_set_text(app->log_buffer, contents, static_cast<gint>(length));
    }
    g_free(contents);
    set_label(app->log_status, "");
}

void on_log_clear(GtkButton *button, gpointer user)
{
    (void)button;
    App *app = static_cast<App *>(user);
    const char *path = selected_log(app);
    if (path == NULL) {
        set_label(app->log_status, "Select a log file.");
        return;
    }
    if (app->log_dir.empty() || !path_inside(path, app->log_dir) || !regular_file(path)) {
        set_label(app->log_status, "That file is outside the log folder.");
        return;
    }
    std::string name = host_basename(path);
    GtkWidget *dialog = gtk_message_dialog_new(
        GTK_WINDOW(app->window), GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
        "Clear %s?", name.c_str());
    gtk_dialog_add_button(GTK_DIALOG(dialog), "Yes", GTK_RESPONSE_YES);
    gtk_dialog_add_button(GTK_DIALOG(dialog), "No", GTK_RESPONSE_NO);
    gint response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    if (response != GTK_RESPONSE_YES) {
        return;
    }
#if defined(_WIN32)
    if (_unlink(path) != 0) {
#else
    if (unlink(path) != 0) {
#endif
        set_label(app->log_status, "Could not clear that log file.");
        return;
    }
    gtk_text_buffer_set_text(app->log_buffer, "", -1);
    fill_logs(app);
    set_label(app->log_status, "Cleared " + name + ".");
}

void on_show_log(GtkButton *button, gpointer user)
{
    on_page_clicked(button, user);
    fill_logs(static_cast<App *>(user));
}

gboolean on_delete(GtkWidget *widget, GdkEvent *event, gpointer user)
{
    (void)widget;
    (void)event;
    App *app = static_cast<App *>(user);
    if (app->busy) {
        app->close_requested = true;
        set_label(app->active_result, WAIT_PHRASE);
        return TRUE;
    }
    gui_player_pause_all();
    return FALSE;
}

void on_destroy(GtkWidget *widget, gpointer user)
{
    (void)widget;
    (void)user;
    gtk_main_quit();
}

void on_exit_clicked(GtkButton *button, gpointer user)
{
    (void)button;
    App *app = static_cast<App *>(user);
    if (app->busy) {
        app->close_requested = true;
        set_label(app->active_result, WAIT_PHRASE);
        return;
    }
    gui_player_pause_all();
    gtk_widget_destroy(app->window);
}

GtkWidget *action_button(App *app, const char *text, GCallback callback, const char *action)
{
    GtkWidget *widget = button(text, callback, app);
    g_object_set_data_full(G_OBJECT(widget), "action", g_strdup(action), g_free);
    app->playback_buttons.push_back(widget);
    return widget;
}

GtkWidget *source_block(App *app, const char *title, GtkWidget *file_label, GCallback select_cb, int slot,
                        Transport *transport, bool edit_source)
{
    GtkWidget *box = vbox(6);
    gtk_widget_set_vexpand(box, FALSE);
    GtkWidget *row = hbox(8);
    pack(row, gtk_label_new(title), FALSE);
    GtkWidget *select = button("Select file", select_cb, app);
    if (slot != 0) {
        g_object_set_data(G_OBJECT(select), "slot", GINT_TO_POINTER(slot));
    }
    pack(row, select, FALSE);
    gtk_label_set_xalign(GTK_LABEL(file_label), 0.0f);
    gtk_label_set_ellipsize(GTK_LABEL(file_label), PANGO_ELLIPSIZE_MIDDLE);
    gtk_widget_set_hexpand(file_label, TRUE);
    pack(row, file_label, TRUE);
    pack(box, row, FALSE);
    pack(box, gui_player_widget(transport->player), TRUE);
    pack(box, transport_row(transport), FALSE);
    GtkWidget *controls = hbox(6);
    if (edit_source) {
        pack(controls, action_button(app, "Start", G_CALLBACK(on_edit_transport), "start"), FALSE);
        pack(controls, action_button(app, "Pause", G_CALLBACK(on_edit_transport), "pause"), FALSE);
        pack(controls, action_button(app, "Stop", G_CALLBACK(on_edit_transport), "stop"), FALSE);
    } else {
        GtkWidget *play = button("Play", G_CALLBACK(on_join_transport), app);
        GtkWidget *stop = button("Stop", G_CALLBACK(on_join_transport), app);
        g_object_set_data(G_OBJECT(play), "slot", GINT_TO_POINTER(slot));
        g_object_set_data(G_OBJECT(stop), "slot", GINT_TO_POINTER(slot));
        g_object_set_data_full(G_OBJECT(play), "action", g_strdup("play"), g_free);
        g_object_set_data_full(G_OBJECT(stop), "action", g_strdup("stop"), g_free);
        app->playback_buttons.push_back(play);
        app->playback_buttons.push_back(stop);
        pack(controls, play, FALSE);
        pack(controls, stop, FALSE);
    }
    pack(box, controls, FALSE);
    return box;
}

GtkWidget *build_edit(App *app)
{
    app->edit_source.player = gui_player_new();
    app->edit_generated.player = gui_player_new();
    wire_transport(&app->edit_source);
    wire_transport(&app->edit_generated);
    app->edit_file = gtk_label_new("");
    app->edit_start = gtk_entry_new();
    app->edit_end = gtk_entry_new();
    app->edit_jump = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(app->edit_start), "00:00.00");
    gtk_entry_set_text(GTK_ENTRY(app->edit_end), "00:00.00");
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->edit_jump), "00:00.00");
    gtk_entry_set_width_chars(GTK_ENTRY(app->edit_start), 10);
    gtk_entry_set_width_chars(GTK_ENTRY(app->edit_end), 10);
    gtk_entry_set_width_chars(GTK_ENTRY(app->edit_jump), 10);
    app->edit_percent = gtk_spin_button_new_with_range(20.0, 200.0, 1.0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->edit_percent), 100.0);
    app->edit_tail_none = gtk_radio_button_new_with_label(NULL, "None");
    GSList *group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(app->edit_tail_none));
    app->edit_tail_boom = gtk_radio_button_new_with_label(group, "Boomerang");
    group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(app->edit_tail_boom));
    app->edit_tail_repeat = gtk_radio_button_new_with_label(group, "Repeat");

    GtkWidget *left = source_block(app, "MP4", app->edit_file, G_CALLBACK(on_edit_select), 0, &app->edit_source, true);
    GtkWidget *jump = hbox(6);
    pack(jump, app->edit_jump, FALSE);
    pack(jump, button("Go", G_CALLBACK(on_edit_go), app), FALSE);
    GtkWidget *mark_start = button("Mark start", G_CALLBACK(on_mark), app);
    GtkWidget *mark_end = button("Mark end", G_CALLBACK(on_mark), app);
    g_object_set_data_full(G_OBJECT(mark_start), "mark", g_strdup("start"), g_free);
    g_object_set_data_full(G_OBJECT(mark_end), "mark", g_strdup("end"), g_free);
    pack(jump, mark_start, FALSE);
    pack(jump, mark_end, FALSE);
    pack(left, jump, FALSE);
    GtkWidget *times = hbox(6);
    pack(times, gtk_label_new("Start"), FALSE);
    pack(times, app->edit_start, FALSE);
    pack(times, gtk_label_new("End"), FALSE);
    pack(times, app->edit_end, FALSE);
    pack(left, times, FALSE);
    GtkWidget *percent = hbox(6);
    pack(percent, gtk_label_new("Percent"), FALSE);
    pack(percent, app->edit_percent, FALSE);
    pack(left, percent, FALSE);
    GtkWidget *tail = hbox(6);
    pack(tail, gtk_label_new("Tail"), FALSE);
    pack(tail, app->edit_tail_none, FALSE);
    pack(tail, app->edit_tail_boom, FALSE);
    pack(tail, app->edit_tail_repeat, FALSE);
    pack(left, tail, FALSE);

    GtkWidget *right = vbox(6);
    app->edit_output = gtk_label_new("Output");
    gtk_label_set_xalign(GTK_LABEL(app->edit_output), 0.0f);
    gtk_label_set_ellipsize(GTK_LABEL(app->edit_output), PANGO_ELLIPSIZE_MIDDLE);
    app->edit_run = button("Run", G_CALLBACK(on_edit_run), app);
    app->edit_result = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(app->edit_result), 0.0f);
    gtk_label_set_line_wrap(GTK_LABEL(app->edit_result), TRUE);
    gtk_label_set_selectable(GTK_LABEL(app->edit_result), TRUE);
    pack(right, app->edit_output, FALSE);
    pack(right, app->edit_run, FALSE);
    pack(right, app->edit_result, FALSE);
    pack(right, gtk_label_new("Generated"), FALSE);
    pack(right, gui_player_widget(app->edit_generated.player), TRUE);
    pack(right, transport_row(&app->edit_generated), FALSE);
    GtkWidget *generated = hbox(6);
    pack(generated, action_button(app, "Play", G_CALLBACK(on_generated_transport), "play"), FALSE);
    pack(generated, action_button(app, "Stop", G_CALLBACK(on_generated_transport), "stop"), FALSE);
    pack(right, generated, FALSE);

    g_signal_connect(app->edit_start, "changed", G_CALLBACK(on_edit_changed), app);
    g_signal_connect(app->edit_end, "changed", G_CALLBACK(on_edit_changed), app);
    g_signal_connect(app->edit_percent, "value-changed", G_CALLBACK(on_edit_changed), app);
    g_signal_connect(app->edit_tail_none, "toggled", G_CALLBACK(on_edit_changed), app);
    g_signal_connect(app->edit_tail_boom, "toggled", G_CALLBACK(on_edit_changed), app);
    g_signal_connect(app->edit_tail_repeat, "toggled", G_CALLBACK(on_edit_changed), app);

    GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_pack1(GTK_PANED(paned), left, TRUE, FALSE);
    gtk_paned_pack2(GTK_PANED(paned), right, TRUE, FALSE);
    gtk_paned_set_position(GTK_PANED(paned), 640);
    refresh_edit_output(app);
    return paned;
}

GtkWidget *build_join(App *app)
{
    app->join_a.player = gui_player_new();
    app->join_b.player = gui_player_new();
    app->join_out.player = gui_player_new();
    wire_transport(&app->join_a);
    wire_transport(&app->join_b);
    wire_transport(&app->join_out);
    app->join_file_1 = gtk_label_new("");
    app->join_file_2 = gtk_label_new("");
    GtkWidget *left = vbox(8);
    pack(left, source_block(app, "Video 1", app->join_file_1, G_CALLBACK(on_join_select), 1, &app->join_a, false), TRUE);
    pack(left, source_block(app, "Video 2", app->join_file_2, G_CALLBACK(on_join_select), 2, &app->join_b, false), TRUE);

    GtkWidget *right = vbox(6);
    GtkWidget *name_row = hbox(6);
    pack(name_row, gtk_label_new("Output name"), FALSE);
    app->join_name = gtk_entry_new();
    gtk_widget_set_hexpand(app->join_name, TRUE);
    pack(name_row, app->join_name, TRUE);
    app->join_write = gtk_label_new("Write to  the current folder");
    gtk_label_set_xalign(GTK_LABEL(app->join_write), 0.0f);
    gtk_label_set_ellipsize(GTK_LABEL(app->join_write), PANGO_ELLIPSIZE_MIDDLE);
    app->join_run = button("Run", G_CALLBACK(on_join_run), app);
    app->join_result = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(app->join_result), 0.0f);
    gtk_label_set_line_wrap(GTK_LABEL(app->join_result), TRUE);
    gtk_label_set_selectable(GTK_LABEL(app->join_result), TRUE);
    pack(right, name_row, FALSE);
    pack(right, app->join_write, FALSE);
    pack(right, app->join_run, FALSE);
    pack(right, app->join_result, FALSE);
    pack(right, gtk_label_new("Generated"), FALSE);
    pack(right, gui_player_widget(app->join_out.player), TRUE);
    pack(right, transport_row(&app->join_out), FALSE);
    GtkWidget *generated = hbox(6);
    GtkWidget *play = button("Play", G_CALLBACK(on_join_transport), app);
    GtkWidget *stop = button("Stop", G_CALLBACK(on_join_transport), app);
    g_object_set_data(G_OBJECT(play), "slot", GINT_TO_POINTER(3));
    g_object_set_data(G_OBJECT(stop), "slot", GINT_TO_POINTER(3));
    g_object_set_data_full(G_OBJECT(play), "action", g_strdup("play"), g_free);
    g_object_set_data_full(G_OBJECT(stop), "action", g_strdup("stop"), g_free);
    app->playback_buttons.push_back(play);
    app->playback_buttons.push_back(stop);
    pack(generated, play, FALSE);
    pack(generated, stop, FALSE);
    pack(right, generated, FALSE);
    g_signal_connect(app->join_name, "changed", G_CALLBACK(on_join_name), app);

    GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_pack1(GTK_PANED(paned), left, TRUE, FALSE);
    gtk_paned_pack2(GTK_PANED(paned), right, TRUE, FALSE);
    gtk_paned_set_position(GTK_PANED(paned), 640);
    gtk_widget_set_sensitive(app->join_run, FALSE);
    return paned;
}

GtkWidget *build_log(App *app)
{
    GtkWidget *box = vbox(6);
    app->log_folder = gtk_label_new("No log folder.");
    gtk_label_set_xalign(GTK_LABEL(app->log_folder), 0.0f);
    app->log_list = gtk_list_box_new();
    gtk_widget_set_vexpand(app->log_list, TRUE);
    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scrolled), app->log_list);
    gtk_widget_set_size_request(scrolled, -1, 140);
    GtkWidget *actions = hbox(6);
    pack(actions, button("View", G_CALLBACK(on_log_view), app), FALSE);
    pack(actions, button("Clear", G_CALLBACK(on_log_clear), app), FALSE);
    app->log_status = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(app->log_status), 0.0f);
    GtkWidget *text = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text), GTK_WRAP_WORD_CHAR);
    app->log_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text));
    GtkWidget *text_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(text_scroll), text);
    gtk_widget_set_vexpand(text_scroll, TRUE);
    pack(box, app->log_folder, FALSE);
    pack(box, scrolled, TRUE);
    pack(box, actions, FALSE);
    pack(box, app->log_status, FALSE);
    pack(box, text_scroll, TRUE);
    return box;
}

GtkWidget *build_language()
{
    GtkWidget *box = vbox(8);
    GtkWidget *label = gtk_label_new("This tree has no language catalog. Labels stay English.");
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
    pack(box, label, FALSE);
    return box;
}

GtkWidget *build_self()
{
    GtkWidget *box = vbox(8);
    char version[32];
    std::snprintf(version, sizeof(version), "%d.%d.%d", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
    GtkWidget *name = gtk_label_new(PROJECT_NAME);
    GtkWidget *ver = gtk_label_new(version);
    GtkWidget *about = gtk_label_new(PROJECT_DESCRIPTION);
    GtkWidget *binary = gtk_label_new(executable_path().c_str());
    gtk_label_set_xalign(GTK_LABEL(name), 0.0f);
    gtk_label_set_xalign(GTK_LABEL(ver), 0.0f);
    gtk_label_set_xalign(GTK_LABEL(about), 0.0f);
    gtk_label_set_xalign(GTK_LABEL(binary), 0.0f);
    gtk_label_set_line_wrap(GTK_LABEL(about), TRUE);
    gtk_label_set_selectable(GTK_LABEL(binary), TRUE);
    pack(box, name, FALSE);
    pack(box, ver, FALSE);
    pack(box, about, FALSE);
    pack(box, binary, FALSE);
    return box;
}

void install_style()
{
    const char *css =
        "button.group-tab { border-radius: 8px 8px 0 0; padding: 6px 18px; margin-top: 4px; }"
        "button.group-tab:checked { background-color: @theme_selected_bg_color; color: @theme_selected_fg_color; }"
        "box.video-group.group-open { border-top: 3px solid @theme_selected_bg_color; }";
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider, css, -1, NULL);
    GdkScreen *screen = gdk_screen_get_default();
    if (screen != NULL) {
        gtk_style_context_add_provider_for_screen(
            screen, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }
    g_object_unref(provider);
}

void tag_tab(GtkWidget *widget)
{
    gtk_style_context_add_class(gtk_widget_get_style_context(widget), "group-tab");
}

}  // namespace

int gui_window_open(int argc, char **argv)
{
    std::vector<std::string> stored;
    stored.reserve(static_cast<size_t>(argc));
    for (int i = 0; i < argc; ++i) {
        stored.push_back(argv[i] == NULL ? "" : argv[i]);
    }
    std::vector<char *> args;
    args.reserve(stored.size() + 1);
    for (size_t i = 0; i < stored.size(); ++i) {
        args.push_back(&stored[i][0]);
    }
    args.push_back(NULL);
    int local_argc = argc;
    char **local_argv = args.data();

    gtk_disable_setlocale();
    if (!gtk_init_check(&local_argc, &local_argv)) {
        std::fprintf(stderr, "ERROR: the display was refused. The next step is %s tui.\n", PROJECT_NAME);
        return 1;
    }
    ciao_debug_from_env();

    App *app = new App();
    install_style();
    char title[64];
    std::snprintf(title, sizeof(title), "%s %d.%d.%d", PROJECT_NAME, VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
    app->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(app->window), title);
    gtk_window_set_default_size(GTK_WINDOW(app->window), 1280, 800);
    g_signal_connect(app->window, "delete-event", G_CALLBACK(on_delete), app);
    g_signal_connect(app->window, "destroy", G_CALLBACK(on_destroy), app);

    GtkWidget *header = gtk_header_bar_new();
    gtk_header_bar_set_title(GTK_HEADER_BAR(header), title);
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header), TRUE);
    gtk_window_set_titlebar(GTK_WINDOW(app->window), header);

    GtkWidget *tabs = hbox(4);
    app->join_tab = gtk_toggle_button_new_with_label("join");
    app->edit_tab = gtk_toggle_button_new_with_label("edit");
    tag_tab(app->join_tab);
    tag_tab(app->edit_tab);
    g_signal_connect(app->join_tab, "toggled", G_CALLBACK(on_group_tab), app);
    g_signal_connect(app->edit_tab, "toggled", G_CALLBACK(on_group_tab), app);
    pack(tabs, app->join_tab, FALSE);
    pack(tabs, app->edit_tab, FALSE);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(header), tabs);

    GtkWidget *exit_button = button("Exit", G_CALLBACK(on_exit_clicked), app);
    GtkWidget *self_button = button("self-management", G_CALLBACK(on_page_clicked), app);
    GtkWidget *language_button = button("language", G_CALLBACK(on_page_clicked), app);
    GtkWidget *log_button = button("system-log", G_CALLBACK(on_show_log), app);
    g_object_set_data(G_OBJECT(self_button), "page", const_cast<char *>("self"));
    g_object_set_data(G_OBJECT(language_button), "page", const_cast<char *>("lang"));
    g_object_set_data(G_OBJECT(log_button), "page", const_cast<char *>("log"));
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), exit_button);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), self_button);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), language_button);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), log_button);

    GtkWidget *root = vbox(8);
    gtk_container_set_border_width(GTK_CONTAINER(root), 8);
    app->path_label = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(app->path_label), 0.0f);
    pack(root, app->path_label, FALSE);
    app->group_frame = vbox(0);
    gtk_style_context_add_class(gtk_widget_get_style_context(app->group_frame), "video-group");
    app->stack = gtk_stack_new();
    gtk_stack_add_named(GTK_STACK(app->stack), build_edit(app), "edit");
    gtk_stack_add_named(GTK_STACK(app->stack), build_join(app), "join");
    gtk_stack_add_named(GTK_STACK(app->stack), build_log(app), "log");
    gtk_stack_add_named(GTK_STACK(app->stack), build_language(), "lang");
    gtk_stack_add_named(GTK_STACK(app->stack), build_self(), "self");
    pack(app->group_frame, app->stack, TRUE);
    pack(root, app->group_frame, TRUE);
    gtk_container_add(GTK_CONTAINER(app->window), root);

    if (!gui_player_backend_ok()) {
        for (size_t i = 0; i < app->playback_buttons.size(); ++i) {
            gtk_widget_set_sensitive(app->playback_buttons[i], FALSE);
        }
    }
    app->page = "edit";
    gtk_stack_set_visible_child_name(GTK_STACK(app->stack), "edit");
    mark_tabs(app);
    refresh_path(app);
    gtk_widget_show_all(app->window);
    gtk_main();
    gui_player_shutdown();
    delete app;
    return 0;
}
