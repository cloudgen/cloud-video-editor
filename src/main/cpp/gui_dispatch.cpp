#include "gui_dispatch.h"

#include <cstdlib>
#include <cstring>

namespace {

bool equals(const char *text, const char *lit)
{
    return std::strcmp(text, lit) == 0;
}

bool starts_with(const char *text, const char *prefix)
{
    size_t n = std::strlen(prefix);
    return std::strncmp(text, prefix, n) == 0;
}

bool is_known_verb(const std::string &text)
{
    return text == "help" || text == "version" || text == "about" || text == "join" ||
           text == "edit" || text == "list-videos" || text == "list-mp4" || text == "tui";
}

bool is_value_flag(const char *text)
{
    return equals(text, "--file") || equals(text, "--folder") || equals(text, "--start") ||
           equals(text, "--end") || equals(text, "--percent") || equals(text, "--first") ||
           equals(text, "--second") || equals(text, "--output");
}

bool bare_start(const GuiRouteInput &in)
{
    return !in.gemini && !in.window_flag && in.verb.empty() && !in.video_domain_flags &&
           !in.complete_edit && !in.bad;
}

}  // namespace

GuiRoute gui_route(const GuiRouteInput &in)
{
    if (in.window_flag) {
        return GUI_ROUTE_ERROR;
    }
    if (in.gemini) {
        if (in.verb.empty() && !in.video_domain_flags && !in.complete_edit && !in.bad) {
            return GUI_ROUTE_GEMINI;
        }
        return GUI_ROUTE_ERROR;
    }
    if (in.verb == "tui") {
        if (!in.stdin_tty) {
            return GUI_ROUTE_ERROR;
        }
        return GUI_ROUTE_MENU;
    }
    if (!in.verb.empty() || in.video_domain_flags || in.complete_edit || in.bad) {
        return GUI_ROUTE_HEADLESS;
    }
    if (!bare_start(in)) {
        return GUI_ROUTE_HEADLESS;
    }
    if (!in.stdin_tty) {
        return GUI_ROUTE_HELP;
    }
    if (!in.stdout_tty || !in.display_env) {
        return GUI_ROUTE_MENU;
    }
    return GUI_ROUTE_WINDOW;
}

std::string gui_route_error_text(const GuiRouteInput &in)
{
    if (gui_route(in) != GUI_ROUTE_ERROR) {
        return "";
    }
    if (in.verb == "tui" && !in.stdin_tty) {
        return "ERROR: tui needs a terminal.";
    }
    if (in.window_flag) {
        return "ERROR: --window is not the video window. A bare start opens the video window. --gemini opens the inherited shell.";
    }
    if (in.gemini) {
        return "ERROR: --gemini cannot be combined with another mode.";
    }
    return "ERROR: the video window route is not valid.";
}

bool gui_display_env_set()
{
    const char *wayland = std::getenv("WAYLAND_DISPLAY");
    if (wayland != NULL && wayland[0] != '\0') {
        return true;
    }
    const char *display = std::getenv("DISPLAY");
    return display != NULL && display[0] != '\0';
}

GuiRouteInput gui_route_from_argv(int argc, char **argv, bool stdin_tty, bool stdout_tty)
{
    GuiRouteInput in;
    in.stdin_tty = stdin_tty;
    in.stdout_tty = stdout_tty;
    in.display_env = gui_display_env_set();
    bool saw_file = false;
    bool saw_start = false;
    bool saw_end = false;
    std::string positional;
    bool saw_positional = false;
    bool extra_positional = false;

    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (equals(arg, "--quiet")) {
            continue;
        }
        if (starts_with(arg, "--shutdown=") || starts_with(arg, "--bookmark=")) {
            continue;
        }
        if (equals(arg, "--gemini")) {
            in.gemini = true;
            continue;
        }
        if (equals(arg, "--window")) {
            in.window_flag = true;
            continue;
        }
        if (equals(arg, "--help") || equals(arg, "-h")) {
            in.verb = "help";
            continue;
        }
        if (equals(arg, "--json") || equals(arg, "--boomerang") || equals(arg, "--repeat")) {
            in.video_domain_flags = true;
            continue;
        }
        if (is_value_flag(arg)) {
            in.video_domain_flags = true;
            if (i + 1 >= argc) {
                in.bad = true;
                break;
            }
            ++i;
            if (equals(arg, "--file")) {
                saw_file = true;
            } else if (equals(arg, "--start")) {
                saw_start = true;
            } else if (equals(arg, "--end")) {
                saw_end = true;
            }
            continue;
        }
        if (arg[0] == '-') {
            in.bad = true;
            continue;
        }
        if (!saw_positional) {
            positional = arg;
            saw_positional = true;
        } else {
            extra_positional = true;
        }
    }
    in.complete_edit = saw_file && saw_start && saw_end;
    if (saw_positional) {
        if (is_known_verb(positional)) {
            in.verb = positional;
            if (extra_positional && positional != "join") {
                in.bad = true;
            }
        } else {
            in.bad = true;
        }
    }
    return in;
}

std::string gui_first_positional(int argc, char **argv)
{
    if (argv == NULL) {
        return "";
    }
    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (arg == NULL) {
            continue;
        }
        if (equals(arg, "--quiet") || equals(arg, "--gemini") || equals(arg, "--window") ||
            equals(arg, "--help") || equals(arg, "-h") || equals(arg, "--json") ||
            equals(arg, "--boomerang") || equals(arg, "--repeat")) {
            continue;
        }
        if (starts_with(arg, "--shutdown=") || starts_with(arg, "--bookmark=")) {
            continue;
        }
        if (is_value_flag(arg)) {
            if (i + 1 < argc) {
                ++i;
            }
            continue;
        }
        if (arg[0] == '-') {
            continue;
        }
        return arg;
    }
    return "";
}
