// Route table for the video window. No GTK.

#include "gui_dispatch.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

static int g_failed = 0;

static void expect_route(const char *name, GuiRoute actual, GuiRoute want)
{
    if (actual != want) {
        std::cerr << "FAIL " << name << " got " << static_cast<int>(actual)
                  << " want " << static_cast<int>(want) << "\n";
        g_failed += 1;
    }
}

static GuiRoute route_of(bool display, bool in_tty, bool out_tty, const std::vector<std::string> &words)
{
    if (display) {
        setenv("DISPLAY", ":9", 1);
    } else {
        unsetenv("DISPLAY");
    }
    unsetenv("WAYLAND_DISPLAY");
    std::vector<std::string> stored;
    stored.push_back("VideoTools");
    for (size_t i = 0; i < words.size(); ++i) {
        stored.push_back(words[i]);
    }
    std::vector<char *> argv;
    for (size_t i = 0; i < stored.size(); ++i) {
        argv.push_back(&stored[i][0]);
    }
    argv.push_back(NULL);
    GuiRouteInput input = gui_route_from_argv(static_cast<int>(stored.size()), argv.data(), in_tty, out_tty);
    return gui_route(input);
}

int main()
{
    expect_route("bare window", route_of(true, true, true, {}), GUI_ROUTE_WINDOW);
    expect_route("no display", route_of(false, true, true, {}), GUI_ROUTE_MENU);
    expect_route("stdout not tty", route_of(true, true, false, {}), GUI_ROUTE_MENU);
    expect_route("stdin not tty", route_of(true, false, true, {}), GUI_ROUTE_HELP);
    expect_route("tui", route_of(false, true, true, {"tui"}), GUI_ROUTE_MENU);
    expect_route("tui no tty", route_of(true, false, true, {"tui"}), GUI_ROUTE_ERROR);
    expect_route("join", route_of(true, true, true, {"join"}), GUI_ROUTE_HEADLESS);
    expect_route("first second", route_of(true, true, true, {"--first", "a.mp4", "--second", "b.mp4"}), GUI_ROUTE_HEADLESS);
    expect_route("json", route_of(true, true, true, {"--json"}), GUI_ROUTE_HEADLESS);
    expect_route("file only", route_of(true, true, true, {"--file", "a.mp4"}), GUI_ROUTE_HEADLESS);
    expect_route("percent", route_of(true, true, true, {"--percent", "50"}), GUI_ROUTE_HEADLESS);
    expect_route("quiet", route_of(true, true, true, {"--quiet"}), GUI_ROUTE_WINDOW);
    expect_route("window flag", route_of(true, true, true, {"--window"}), GUI_ROUTE_ERROR);
    expect_route("gemini", route_of(true, true, true, {"--gemini"}), GUI_ROUTE_GEMINI);
    expect_route("gemini join", route_of(true, true, true, {"--gemini", "join"}), GUI_ROUTE_ERROR);
    expect_route("unknown", route_of(true, true, true, {"nope"}), GUI_ROUTE_HEADLESS);
    expect_route("complete edit", route_of(true, true, true, {"--file", "a.mp4", "--start", "1", "--end", "2"}), GUI_ROUTE_HEADLESS);

    setenv("DISPLAY", ":9", 1);
    const char *window_argv[] = {"VideoTools", "--window", NULL};
    GuiRouteInput window_in = gui_route_from_argv(2, const_cast<char **>(window_argv), true, true);
    std::string text = gui_route_error_text(window_in);
    if (text.find("--window is not the video window") == std::string::npos) {
        std::cerr << "FAIL window sentence: " << text << "\n";
        g_failed += 1;
    }
    const char *mix_argv[] = {"VideoTools", "--gemini", "join", NULL};
    GuiRouteInput mix_in = gui_route_from_argv(3, const_cast<char **>(mix_argv), true, true);
    std::string mix = gui_route_error_text(mix_in);
    if (mix.find("cannot be combined") == std::string::npos) {
        std::cerr << "FAIL gemini sentence: " << mix << "\n";
        g_failed += 1;
    }

    if (g_failed != 0) {
        std::cerr << g_failed << " route checks failed\n";
        return 1;
    }

    auto positional = [](const std::vector<std::string> &words) {
        std::vector<std::string> stored;
        stored.push_back("VideoTools");
        for (size_t i = 0; i < words.size(); ++i) {
            stored.push_back(words[i]);
        }
        std::vector<char *> argv;
        for (size_t i = 0; i < stored.size(); ++i) {
            argv.push_back(&stored[i][0]);
        }
        return gui_first_positional(static_cast<int>(stored.size()), argv.data());
    };
    auto expect_pos = [&](const char *name, const std::vector<std::string> &words, const std::string &want) {
        std::string got = positional(words);
        if (got != want) {
            std::cerr << "FAIL " << name << " got '" << got << "' want '" << want << "'\n";
            g_failed += 1;
        }
    };
    expect_pos("edit file log", {"edit", "--file", "log", "--start", "0", "--end", "1"}, "edit");
    expect_pos("join second log", {"join", "--first", "a.mp4", "--second", "log"}, "join");
    expect_pos("flag value only", {"--file", "log"}, "");
    expect_pos("shell verb", {"log"}, "log");
    expect_pos("luascript", {"luascript", "folder"}, "luascript");
    expect_pos("window then log", {"--window", "log"}, "log");
    expect_pos("gemini alone", {"--gemini"}, "");

    if (g_failed != 0) {
        std::cerr << g_failed << " route checks failed\n";
        return 1;
    }
    std::cout << "gui_dispatch ok\n";
    return 0;
}
