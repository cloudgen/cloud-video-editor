// gui_player.cpp
// GtkGLArea panes. One client thread owns every mpv core. The GTK thread
// renders into the framebuffer GTK already bound, and it does not wait on
// that client thread.

#include "gui_player.h"

#include <mpv/client.h>
#include <mpv/render_gl.h>

#include <gtk/gtk.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#include <gdk/gdkwayland.h>
#include <gdk/gdkx.h>
#include <epoxy/egl.h>
#include <epoxy/glx.h>
#endif
#include <epoxy/gl.h>

enum CmdKind {
    CMD_CREATE = 1,
    CMD_LOAD,
    CMD_PAUSE,
    CMD_PLAY,
    CMD_SEEK,
    CMD_STOP,
    CMD_MUTE,
    CMD_CLEAR,
    CMD_HWDEC,
    CMD_DESTROY
};

struct Command {
    CmdKind kind;
    struct GuiPlayer *player;
    std::string text;
    double seconds;
    uint64_t gen;

    Command() : kind(CMD_CREATE), player(NULL), seconds(0.0), gen(0) {}
};

struct GuiPlayer {
    GtkWidget *box;
    GtkWidget *area;
    GtkWidget *fallback;
    mpv_handle *mpv;
    mpv_render_context *render;
    bool alive;
    bool core_started;
    bool render_ready;
    bool cleared;
    bool want_stop;
    bool has_file;
    bool use_hwdec;
    bool orphan;
    std::string pending_load;
    double pub_pos;
    double pub_dur;
    double duration_hint;
    std::atomic<bool> redraw_queued;
    GuiPlayerTick tick;
    void *tick_user;

    GuiPlayer()
        : box(NULL), area(NULL), fallback(NULL), mpv(NULL), render(NULL), alive(true),
          core_started(false), render_ready(false), cleared(false), want_stop(false),
          has_file(false), use_hwdec(false), orphan(false), pub_pos(0.0), pub_dur(0.0), duration_hint(0.0),
          redraw_queued(false), tick(NULL), tick_user(NULL) {}
};

namespace {

struct Api {
    mpv_handle *(*create)(void);
    int (*initialize)(mpv_handle *);
    int (*command)(mpv_handle *, const char **);
    int (*set_property_string)(mpv_handle *, const char *, const char *);
    int (*get_property)(mpv_handle *, const char *, mpv_format, void *);
    int (*set_property)(mpv_handle *, const char *, mpv_format, void *);
    mpv_event *(*wait_event)(mpv_handle *, double);
    void (*terminate_destroy)(mpv_handle *);
    int (*render_create)(mpv_render_context **, mpv_handle *, mpv_render_param *);
    int (*render_draw)(mpv_render_context *, mpv_render_param *);
    void (*render_set_update)(mpv_render_context *, mpv_render_update_fn, void *);
    void (*render_free)(mpv_render_context *);
    uint64_t (*render_update)(mpv_render_context *);
};

Api g_api;
void *g_lib = NULL;
bool g_lib_tried = false;
std::string g_unavailable = "libmpv";

std::mutex g_mpv_mu;
std::mutex g_pub_mu;
std::mutex g_q_mu;
std::condition_variable g_q_cv;
std::deque<Command> g_q;
std::thread *g_thread = NULL;
bool g_stop = false;
std::atomic<uint64_t> g_play_gen(0);
std::vector<GuiPlayer *> g_players;
std::mutex g_list_mu;
guint g_tick_id = 0;

std::vector<GuiPlayer *> player_snapshot()
{
    std::lock_guard<std::mutex> lock(g_list_mu);
    return g_players;
}

void show_fallback(GuiPlayer *player, const char *reason)
{
    if (player == NULL || player->fallback == NULL) {
        return;
    }
    std::string text = std::string("Playback is unavailable (") + reason + ").";
    gtk_label_set_text(GTK_LABEL(player->fallback), text.c_str());
    gtk_widget_set_no_show_all(player->fallback, FALSE);
    gtk_widget_show(player->fallback);
    if (player->area != NULL) {
        gtk_widget_hide(player->area);
    }
}

bool load_symbol(const char *name, void **out)
{
#if defined(_WIN32)
    *out = reinterpret_cast<void *>(GetProcAddress(static_cast<HMODULE>(g_lib), name));
#else
    *out = dlsym(g_lib, name);
#endif
    return *out != NULL;
}

bool open_library()
{
    if (g_lib_tried) {
        return g_lib != NULL;
    }
    g_lib_tried = true;
#if defined(_WIN32)
    g_lib = reinterpret_cast<void *>(LoadLibraryW(L"libmpv-2.dll"));
    if (g_lib == NULL) {
        g_lib = reinterpret_cast<void *>(LoadLibraryW(L"mpv-2.dll"));
    }
#else
    g_lib = dlopen("libmpv.so.2", RTLD_NOW | RTLD_LOCAL);
    if (g_lib == NULL) {
        g_lib = dlopen("libmpv.so", RTLD_NOW | RTLD_LOCAL);
    }
#endif
    if (g_lib == NULL) {
        g_unavailable = "libmpv";
        return false;
    }
    bool ok = true;
    ok = ok && load_symbol("mpv_create", reinterpret_cast<void **>(&g_api.create));
    ok = ok && load_symbol("mpv_initialize", reinterpret_cast<void **>(&g_api.initialize));
    ok = ok && load_symbol("mpv_command", reinterpret_cast<void **>(&g_api.command));
    ok = ok && load_symbol("mpv_set_property_string", reinterpret_cast<void **>(&g_api.set_property_string));
    ok = ok && load_symbol("mpv_get_property", reinterpret_cast<void **>(&g_api.get_property));
    ok = ok && load_symbol("mpv_set_property", reinterpret_cast<void **>(&g_api.set_property));
    ok = ok && load_symbol("mpv_wait_event", reinterpret_cast<void **>(&g_api.wait_event));
    ok = ok && load_symbol("mpv_terminate_destroy", reinterpret_cast<void **>(&g_api.terminate_destroy));
    ok = ok && load_symbol("mpv_render_context_create", reinterpret_cast<void **>(&g_api.render_create));
    ok = ok && load_symbol("mpv_render_context_render", reinterpret_cast<void **>(&g_api.render_draw));
    ok = ok && load_symbol("mpv_render_context_set_update_callback", reinterpret_cast<void **>(&g_api.render_set_update));
    ok = ok && load_symbol("mpv_render_context_free", reinterpret_cast<void **>(&g_api.render_free));
    ok = ok && load_symbol("mpv_render_context_update", reinterpret_cast<void **>(&g_api.render_update));
    if (!ok) {
        g_unavailable = "libmpv";
#if defined(_WIN32)
        FreeLibrary(static_cast<HMODULE>(g_lib));
#else
        dlclose(g_lib);
#endif
        g_lib = NULL;
        return false;
    }
    g_unavailable.clear();
    return true;
}

void post(const Command &cmd)
{
    {
        std::lock_guard<std::mutex> lock(g_q_mu);
        if (cmd.kind == CMD_SEEK) {
            for (size_t i = 0; i < g_q.size(); ++i) {
                if (g_q[i].kind == CMD_SEEK && g_q[i].player == cmd.player) {
                    g_q[i].seconds = cmd.seconds;
                    return;
                }
            }
        }
        g_q.push_back(cmd);
    }
    g_q_cv.notify_one();
}

void post_kind(GuiPlayer *player, CmdKind kind)
{
    Command cmd;
    cmd.kind = kind;
    cmd.player = player;
    post(cmd);
}

bool glx_is_current()
{
#if defined(_WIN32)
    return false;
#else
    GdkDisplay *display = gdk_display_get_default();
    if (display == NULL || !GDK_IS_X11_DISPLAY(display)) {
        return false;
    }
    Display *x11 = gdk_x11_display_get_xdisplay(display);
    if (x11 == NULL || !epoxy_has_glx(x11)) {
        return false;
    }
    return glXGetCurrentContext() != NULL;
#endif
}

void *get_proc(void *ctx, const char *name)
{
    (void)ctx;
#if defined(_WIN32)
    PROC sym = wglGetProcAddress(name);
    if (sym != NULL) {
        return reinterpret_cast<void *>(sym);
    }
    HMODULE opengl = GetModuleHandleW(L"opengl32.dll");
    if (opengl == NULL) {
        opengl = LoadLibraryW(L"opengl32.dll");
    }
    if (opengl == NULL) {
        return NULL;
    }
    return reinterpret_cast<void *>(GetProcAddress(opengl, name));
#else
    if (epoxy_has_egl() && eglGetCurrentContext() != NULL) {
        return reinterpret_cast<void *>(eglGetProcAddress(name));
    }
    if (glx_is_current()) {
        return reinterpret_cast<void *>(glXGetProcAddressARB(reinterpret_cast<const GLubyte *>(name)));
    }
    return NULL;
#endif
}

gboolean redraw_idle(gpointer data)
{
    GuiPlayer *player = static_cast<GuiPlayer *>(data);
    player->redraw_queued.store(false);
    if (player->alive && player->area != NULL && gtk_widget_get_realized(player->area)) {
        gtk_gl_area_queue_render(GTK_GL_AREA(player->area));
    }
    return G_SOURCE_REMOVE;
}

gboolean retry_render(gpointer data)
{
    GuiPlayer *player = static_cast<GuiPlayer *>(data);
    if (player->alive && player->area != NULL && gtk_widget_get_realized(player->area)) {
        gtk_gl_area_queue_render(GTK_GL_AREA(player->area));
    }
    return G_SOURCE_REMOVE;
}

void on_mpv_update(void *ctx)
{
    GuiPlayer *player = static_cast<GuiPlayer *>(ctx);
    bool expected = false;
    if (!player->redraw_queued.compare_exchange_strong(expected, true)) {
        return;
    }
    g_idle_add(redraw_idle, player);
}

void issue_load(GuiPlayer *player, const std::string &path)
{
    if (player->mpv == NULL || path.empty()) {
        return;
    }
    player->cleared = false;
    player->want_stop = false;
    player->pending_load.clear();
    {
        std::lock_guard<std::mutex> lock(g_pub_mu);
        player->has_file = true;
    }
    const char *args[] = {"loadfile", path.c_str(), "replace", NULL};
    g_api.command(player->mpv, args);
    g_api.set_property_string(player->mpv, "pause", "yes");
}

gboolean create_render_idle(gpointer data)
{
    GuiPlayer *player = static_cast<GuiPlayer *>(data);
    if (!player->alive || player->area == NULL || player->mpv == NULL || player->render_ready) {
        return G_SOURCE_REMOVE;
    }
    gtk_gl_area_make_current(GTK_GL_AREA(player->area));
    GError *error = gtk_gl_area_get_error(GTK_GL_AREA(player->area));
    if (error != NULL) {
        show_fallback(player, error->message);
        return G_SOURCE_REMOVE;
    }
#if !defined(_WIN32)
    bool egl = epoxy_has_egl() && eglGetCurrentContext() != NULL;
    bool glx = !egl && glx_is_current();
    if (!egl && !glx) {
        show_fallback(player, "OpenGL");
        return G_SOURCE_REMOVE;
    }
#else
    bool egl = false;
#endif
    mpv_opengl_init_params gl_init;
    gl_init.get_proc_address = get_proc;
    gl_init.get_proc_address_ctx = NULL;
    void *wl = NULL;
    void *x11 = NULL;
    bool pass_display = false;
#if !defined(_WIN32)
    if (egl) {
        GdkDisplay *display = gdk_display_get_default();
        if (display != NULL && GDK_IS_WAYLAND_DISPLAY(display)) {
            wl = gdk_wayland_display_get_wl_display(display);
            pass_display = wl != NULL;
        } else if (display != NULL && GDK_IS_X11_DISPLAY(display)) {
            x11 = gdk_x11_display_get_xdisplay(display);
            pass_display = x11 != NULL;
        }
    }
#endif
    mpv_render_param params[6];
    int count = 0;
    const char *api_type = MPV_RENDER_API_TYPE_OPENGL;
    params[count].type = MPV_RENDER_PARAM_API_TYPE;
    params[count].data = const_cast<char *>(api_type);
    count++;
    params[count].type = MPV_RENDER_PARAM_OPENGL_INIT_PARAMS;
    params[count].data = &gl_init;
    count++;
    if (wl != NULL) {
        params[count].type = MPV_RENDER_PARAM_WL_DISPLAY;
        params[count].data = wl;
        count++;
    } else if (x11 != NULL) {
        params[count].type = MPV_RENDER_PARAM_X11_DISPLAY;
        params[count].data = x11;
        count++;
    }
    params[count].type = MPV_RENDER_PARAM_INVALID;
    params[count].data = NULL;
    count++;

    if (!g_mpv_mu.try_lock()) {
        g_timeout_add(16, create_render_idle, player);
        return G_SOURCE_REMOVE;
    }
    if (!player->alive || player->mpv == NULL || player->render_ready) {
        g_mpv_mu.unlock();
        return G_SOURCE_REMOVE;
    }
    mpv_render_context *ctx = NULL;
    int rc = g_api.render_create(&ctx, player->mpv, params);
    if (rc < 0 || ctx == NULL) {
        g_mpv_mu.unlock();
        show_fallback(player, "OpenGL");
        return G_SOURCE_REMOVE;
    }
    player->render = ctx;
    player->render_ready = true;
    player->use_hwdec = egl && pass_display;
    g_api.render_set_update(ctx, on_mpv_update, player);
    std::string pending = player->pending_load;
    bool hwdec = player->use_hwdec;
    g_mpv_mu.unlock();
    if (hwdec) {
        post_kind(player, CMD_HWDEC);
    }
    if (!pending.empty()) {
        Command cmd;
        cmd.kind = CMD_LOAD;
        cmd.player = player;
        cmd.text = pending;
        post(cmd);
    }
    gtk_gl_area_queue_render(GTK_GL_AREA(player->area));
    return G_SOURCE_REMOVE;
}

void set_option(mpv_handle *mpv, const char *name, const char *value)
{
    g_api.set_property_string(mpv, name, value);
}

void do_create(GuiPlayer *player)
{
    if (!player->alive || player->core_started || player->mpv != NULL) {
        return;
    }
    player->core_started = true;
    mpv_handle *mpv = g_api.create();
    if (mpv == NULL) {
        return;
    }
    set_option(mpv, "vo", "libmpv");
    set_option(mpv, "idle", "yes");
    set_option(mpv, "pause", "yes");
    set_option(mpv, "mute", "yes");
    set_option(mpv, "keep-open", "yes");
    set_option(mpv, "config", "no");
    set_option(mpv, "load-scripts", "no");
    set_option(mpv, "ytdl", "no");
    set_option(mpv, "terminal", "no");
    set_option(mpv, "input-default-bindings", "no");
    set_option(mpv, "osc", "no");
    set_option(mpv, "hwdec", "no");
    set_option(mpv, "force-window", "no");
    if (g_api.initialize(mpv) < 0) {
        g_api.terminate_destroy(mpv);
        return;
    }
    player->mpv = mpv;
    g_idle_add(create_render_idle, player);
}

void do_play(GuiPlayer *player, uint64_t gen)
{
    if (player->mpv == NULL || player->cleared || !player->has_file) {
        return;
    }
    if (gen != g_play_gen.load(std::memory_order_acquire)) {
        return;
    }
    std::vector<GuiPlayer *> others = player_snapshot();
    for (size_t i = 0; i < others.size(); ++i) {
        GuiPlayer *other = others[i];
        if (other == player || other->mpv == NULL) {
            continue;
        }
        g_api.set_property_string(other->mpv, "mute", "yes");
    }
    if (gen != g_play_gen.load(std::memory_order_acquire)) {
        return;
    }
    g_api.set_property_string(player->mpv, "mute", "no");
    g_api.set_property_string(player->mpv, "pause", "no");
}

void do_clear(GuiPlayer *player)
{
    player->cleared = true;
    player->pending_load.clear();
    player->want_stop = true;
    {
        std::lock_guard<std::mutex> lock(g_pub_mu);
        player->has_file = false;
        player->pub_pos = 0.0;
    }
    if (player->mpv == NULL) {
        player->want_stop = false;
        return;
    }
    int idle = 0;
    if (g_api.get_property(player->mpv, "idle-active", MPV_FORMAT_FLAG, &idle) >= 0 && idle) {
        const char *args[] = {"stop", NULL};
        g_api.command(player->mpv, args);
        player->want_stop = false;
    }
}

void poll_player(GuiPlayer *player)
{
    if (player->mpv == NULL) {
        return;
    }
    if (player->want_stop) {
        int idle = 0;
        if (g_api.get_property(player->mpv, "idle-active", MPV_FORMAT_FLAG, &idle) >= 0 && idle) {
            const char *args[] = {"stop", NULL};
            g_api.command(player->mpv, args);
            player->want_stop = false;
        }
    }
    double pos = 0.0;
    double dur = 0.0;
    if (g_api.get_property(player->mpv, "time-pos", MPV_FORMAT_DOUBLE, &pos) < 0) {
        pos = 0.0;
    }
    if (g_api.get_property(player->mpv, "duration", MPV_FORMAT_DOUBLE, &dur) < 0) {
        dur = 0.0;
    }
    std::lock_guard<std::mutex> lock(g_pub_mu);
    player->pub_pos = player->cleared ? 0.0 : pos;
    player->pub_dur = dur > 0.0 ? dur : player->duration_hint;
}

void dispatch(const Command &cmd)
{
    GuiPlayer *player = cmd.player;
    if (player == NULL) {
        return;
    }
    if (!player->alive && cmd.kind != CMD_DESTROY) {
        return;
    }
    if (cmd.kind == CMD_CREATE) {
        do_create(player);
        return;
    }
    if (cmd.kind == CMD_DESTROY) {
        if (player->render != NULL) {
            return;
        }
        if (player->mpv != NULL) {
            g_api.terminate_destroy(player->mpv);
            player->mpv = NULL;
        }
        player->render_ready = false;
        return;
    }
    if (player->mpv == NULL) {
        if (cmd.kind == CMD_LOAD) {
            player->pending_load = cmd.text;
            player->cleared = false;
            std::lock_guard<std::mutex> lock(g_pub_mu);
            player->has_file = !cmd.text.empty();
        }
        return;
    }
    if (cmd.kind == CMD_LOAD) {
        if (!player->render_ready) {
            player->pending_load = cmd.text;
            player->cleared = false;
            std::lock_guard<std::mutex> lock(g_pub_mu);
            player->has_file = !cmd.text.empty();
            return;
        }
        issue_load(player, cmd.text);
        return;
    }
    if (cmd.kind == CMD_PAUSE) {
        g_api.set_property_string(player->mpv, "pause", "yes");
        return;
    }
    if (cmd.kind == CMD_MUTE) {
        g_api.set_property_string(player->mpv, "mute", "yes");
        return;
    }
    if (cmd.kind == CMD_PLAY) {
        do_play(player, cmd.gen);
        return;
    }
    if (cmd.kind == CMD_SEEK) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.6f", cmd.seconds);
        const char *args[] = {"seek", buf, "absolute", NULL};
        g_api.command(player->mpv, args);
        return;
    }
    if (cmd.kind == CMD_STOP) {
        const char *args[] = {"seek", "0", "absolute", NULL};
        g_api.command(player->mpv, args);
        g_api.set_property_string(player->mpv, "pause", "yes");
        return;
    }
    if (cmd.kind == CMD_CLEAR) {
        do_clear(player);
        return;
    }
    if (cmd.kind == CMD_HWDEC) {
        if (player->use_hwdec) {
            g_api.set_property_string(player->mpv, "hwdec", "auto-safe");
        }
    }
}

void drain_events(GuiPlayer *player)
{
    if (player->mpv == NULL || g_api.wait_event == NULL) {
        return;
    }
    for (int n = 0; n < 32; ++n) {
        mpv_event *ev = g_api.wait_event(player->mpv, 0);
        if (ev == NULL || ev->event_id == MPV_EVENT_NONE) {
            break;
        }
    }
}

void client_main()
{
    while (true) {
        Command cmd;
        bool have = false;
        {
            std::unique_lock<std::mutex> lock(g_q_mu);
            if (g_q.empty() && !g_stop) {
                g_q_cv.wait_for(lock, std::chrono::milliseconds(50));
            }
            if (!g_q.empty()) {
                cmd = g_q.front();
                g_q.pop_front();
                have = true;
            } else if (g_stop) {
                break;
            }
        }
        std::lock_guard<std::mutex> mpv_lock(g_mpv_mu);
        if (have) {
            dispatch(cmd);
        }
        std::vector<GuiPlayer *> players = player_snapshot();
        for (size_t i = 0; i < players.size(); ++i) {
            if (!players[i]->alive || players[i]->mpv == NULL) {
                continue;
            }
            drain_events(players[i]);
            if (!have) {
                poll_player(players[i]);
            }
        }
    }
}

void ensure_thread()
{
    if (g_thread != NULL || !open_library()) {
        return;
    }
    g_stop = false;
    g_thread = new std::thread(client_main);
}

gboolean on_tick(gpointer)
{
    std::vector<GuiPlayer *> players = player_snapshot();
    std::vector<double> pos;
    std::vector<double> dur;
    {
        std::lock_guard<std::mutex> lock(g_pub_mu);
        std::vector<GuiPlayer *> live;
        for (size_t i = 0; i < players.size(); ++i) {
            GuiPlayer *player = players[i];
            if (!player->alive || player->tick == NULL) {
                continue;
            }
            live.push_back(player);
            pos.push_back(player->pub_pos);
            dur.push_back(player->pub_dur);
        }
        players.swap(live);
    }
    for (size_t i = 0; i < players.size(); ++i) {
        players[i]->tick(players[i], pos[i], dur[i], players[i]->tick_user);
    }
    return G_SOURCE_CONTINUE;
}

gboolean on_render(GtkGLArea *area, GdkGLContext *context, gpointer user)
{
    (void)context;
    GuiPlayer *player = static_cast<GuiPlayer *>(user);
    int scale = gtk_widget_get_scale_factor(GTK_WIDGET(area));
    int width = gtk_widget_get_allocated_width(GTK_WIDGET(area)) * scale;
    int height = gtk_widget_get_allocated_height(GTK_WIDGET(area)) * scale;
    if (width < 1) {
        width = 1;
    }
    if (height < 1) {
        height = 1;
    }
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    if (player->cleared || !player->render_ready || player->render == NULL) {
        return TRUE;
    }
    if (!g_mpv_mu.try_lock()) {
        g_timeout_add(16, retry_render, player);
        return TRUE;
    }
    GLint fbo = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &fbo);
    if (fbo == 0) {
        g_mpv_mu.unlock();
        return TRUE;
    }
    g_api.render_update(player->render);
    mpv_opengl_fbo target;
    target.fbo = static_cast<int>(fbo);
    target.w = width;
    target.h = height;
    target.internal_format = gtk_gl_area_get_has_alpha(area) ? GL_RGBA8 : GL_RGB8;
    int flip = 1;
    int block = 0;
    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_OPENGL_FBO, &target},
        {MPV_RENDER_PARAM_FLIP_Y, &flip},
        {MPV_RENDER_PARAM_BLOCK_FOR_TARGET_TIME, &block},
        {MPV_RENDER_PARAM_INVALID, NULL},
    };
    g_api.render_draw(player->render, params);
    g_mpv_mu.unlock();
    return TRUE;
}

void on_realize(GtkGLArea *area, gpointer user)
{
    (void)area;
    GuiPlayer *player = static_cast<GuiPlayer *>(user);
    if (!open_library()) {
        show_fallback(player, "libmpv");
        return;
    }
    post_kind(player, CMD_CREATE);
}

void on_unrealize(GtkGLArea *area, gpointer user)
{
    GuiPlayer *player = static_cast<GuiPlayer *>(user);
    player->alive = false;
    gtk_gl_area_make_current(area);
    if (!g_mpv_mu.try_lock()) {
        if (player->render != NULL) {
            player->orphan = true;
            return;
        }
        post_kind(player, CMD_DESTROY);
        return;
    }
    if (player->render != NULL) {
        g_api.render_free(player->render);
        player->render = NULL;
        player->render_ready = false;
    }
    g_mpv_mu.unlock();
    post_kind(player, CMD_DESTROY);
}

}  // namespace

bool gui_player_backend_ok()
{
    return open_library();
}

const char *gui_player_unavailable_reason()
{
    if (g_unavailable.empty()) {
        return "libmpv";
    }
    return g_unavailable.c_str();
}

GuiPlayer *gui_player_new()
{
    GuiPlayer *player = new GuiPlayer();
    player->box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(player->box, -1, 160);
    gtk_widget_set_hexpand(player->box, TRUE);
    gtk_widget_set_vexpand(player->box, TRUE);
    player->area = gtk_gl_area_new();
    gtk_gl_area_set_has_alpha(GTK_GL_AREA(player->area), FALSE);
    gtk_gl_area_set_required_version(GTK_GL_AREA(player->area), 3, 2);
    gtk_widget_set_hexpand(player->area, TRUE);
    gtk_widget_set_vexpand(player->area, TRUE);
    g_signal_connect(player->area, "realize", G_CALLBACK(on_realize), player);
    g_signal_connect(player->area, "unrealize", G_CALLBACK(on_unrealize), player);
    g_signal_connect(player->area, "render", G_CALLBACK(on_render), player);
    player->fallback = gtk_label_new("Playback is unavailable (libmpv).");
    gtk_label_set_line_wrap(GTK_LABEL(player->fallback), TRUE);
    gtk_widget_set_no_show_all(player->fallback, TRUE);
    gtk_box_pack_start(GTK_BOX(player->box), player->area, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(player->box), player->fallback, TRUE, TRUE, 0);
    if (!open_library()) {
        gtk_widget_set_no_show_all(player->area, TRUE);
        gtk_widget_set_no_show_all(player->fallback, FALSE);
        gtk_widget_show(player->fallback);
    } else {
        ensure_thread();
    }
    {
        std::lock_guard<std::mutex> lock(g_list_mu);
        g_players.push_back(player);
    }
    if (g_tick_id == 0) {
        g_tick_id = g_timeout_add(100, on_tick, NULL);
    }
    return player;
}

GtkWidget *gui_player_widget(GuiPlayer *player)
{
    return player == NULL ? NULL : player->box;
}

void gui_player_load(GuiPlayer *player, const std::string &path)
{
    if (player == NULL) {
        return;
    }
    Command cmd;
    cmd.kind = CMD_LOAD;
    cmd.player = player;
    cmd.text = path;
    post(cmd);
}

void gui_player_clear(GuiPlayer *player)
{
    if (player != NULL) {
        post_kind(player, CMD_CLEAR);
    }
}

void gui_player_play(GuiPlayer *player)
{
    if (player == NULL) {
        return;
    }
    Command cmd;
    cmd.kind = CMD_PLAY;
    cmd.player = player;
    {
        std::lock_guard<std::mutex> lock(g_q_mu);
        cmd.gen = g_play_gen.fetch_add(1, std::memory_order_acq_rel) + 1;
        g_q.push_back(cmd);
    }
    g_q_cv.notify_one();
}

void gui_player_pause(GuiPlayer *player)
{
    if (player != NULL) {
        post_kind(player, CMD_PAUSE);
    }
}

void gui_player_stop(GuiPlayer *player)
{
    if (player != NULL) {
        post_kind(player, CMD_STOP);
    }
}

void gui_player_seek(GuiPlayer *player, double seconds)
{
    if (player == NULL) {
        return;
    }
    Command cmd;
    cmd.kind = CMD_SEEK;
    cmd.player = player;
    cmd.seconds = seconds;
    post(cmd);
}

void gui_player_pause_mute(GuiPlayer *player)
{
    if (player == NULL) {
        return;
    }
    post_kind(player, CMD_PAUSE);
    post_kind(player, CMD_MUTE);
}

void gui_player_pause_all()
{
    std::vector<GuiPlayer *> players = player_snapshot();
    for (size_t i = 0; i < players.size(); ++i) {
        post_kind(players[i], CMD_PAUSE);
        post_kind(players[i], CMD_MUTE);
    }
}

void gui_player_set_duration_hint(GuiPlayer *player, double seconds)
{
    if (player == NULL) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_pub_mu);
    player->duration_hint = seconds;
    if (player->pub_dur <= 0.0) {
        player->pub_dur = seconds;
    }
}

double gui_player_position(GuiPlayer *player)
{
    if (player == NULL) {
        return 0.0;
    }
    std::lock_guard<std::mutex> lock(g_pub_mu);
    return player->pub_pos;
}

double gui_player_duration(GuiPlayer *player)
{
    if (player == NULL) {
        return 0.0;
    }
    std::lock_guard<std::mutex> lock(g_pub_mu);
    return player->pub_dur;
}

bool gui_player_has_file(GuiPlayer *player)
{
    if (player == NULL) {
        return false;
    }
    std::lock_guard<std::mutex> lock(g_pub_mu);
    return player->has_file;
}

void gui_player_set_tick(GuiPlayer *player, GuiPlayerTick tick, void *user)
{
    if (player == NULL) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_pub_mu);
    player->tick = tick;
    player->tick_user = user;
}

void gui_player_shutdown()
{
    if (g_tick_id != 0) {
        g_source_remove(g_tick_id);
        g_tick_id = 0;
    }
    {
        std::lock_guard<std::mutex> lock(g_q_mu);
        g_stop = true;
    }
    g_q_cv.notify_one();
    if (g_thread != NULL) {
        if (g_thread->joinable()) {
            g_thread->join();
        }
        delete g_thread;
        g_thread = NULL;
    }
    std::vector<GuiPlayer *> players = player_snapshot();
    {
        std::lock_guard<std::mutex> lock(g_list_mu);
        g_players.clear();
    }
    for (size_t i = 0; i < players.size(); ++i) {
        if (players[i]->orphan || players[i]->render != NULL) {
            continue;
        }
        delete players[i];
    }
}
