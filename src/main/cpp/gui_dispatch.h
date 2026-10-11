// gui_dispatch.h
// Pure route table for the video window. No GTK calls live here.

#ifndef GUI_DISPATCH_H
#define GUI_DISPATCH_H

#include <string>

enum GuiRoute {
    GUI_ROUTE_WINDOW = 0,
    GUI_ROUTE_MENU,
    GUI_ROUTE_HELP,
    GUI_ROUTE_HEADLESS,
    GUI_ROUTE_ERROR,
    GUI_ROUTE_GEMINI
};

struct GuiRouteInput {
    bool display_env;
    bool stdin_tty;
    bool stdout_tty;
    bool gemini;
    bool window_flag;
    bool video_domain_flags;
    bool complete_edit;
    bool bad;
    std::string verb;

    GuiRouteInput()
        : display_env(false), stdin_tty(false), stdout_tty(false), gemini(false),
          window_flag(false), video_domain_flags(false), complete_edit(false), bad(false) {}
};

GuiRoute gui_route(const GuiRouteInput &in);

// One stderr sentence for GUI_ROUTE_ERROR. Empty for every other route.
std::string gui_route_error_text(const GuiRouteInput &in);

bool gui_display_env_set();

GuiRouteInput gui_route_from_argv(int argc, char **argv, bool stdin_tty, bool stdout_tty);

// First positional word, after the same flag and value skip as gui_route_from_argv.
// Unknown words such as log are returned. Empty when argv has no positional.
std::string gui_first_positional(int argc, char **argv);

#endif
