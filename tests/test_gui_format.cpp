#include "gui_format.h"

#include <cmath>
#include <iostream>
#include <string>

static int g_failed = 0;

static void expect(bool ok, const std::string &name)
{
    if (!ok) {
        std::cerr << "FAIL " << name << "\n";
        g_failed += 1;
    }
}

int main()
{
    expect(gui_format_elapsed(0) == "00:00.00", "zero");
    expect(gui_format_elapsed(65.5) == "01:05.50", "minute");
    expect(gui_format_elapsed(3723.456) == "62:03.46", "long minutes");
    double seconds = -1;
    expect(gui_parse_elapsed("01:05.50", seconds) && std::fabs(seconds - 65.5) < 0.001, "parse");
    expect(!gui_parse_elapsed("1:5.5", seconds), "short");
    expect(!gui_parse_elapsed("00:60.00", seconds), "seconds");
    expect(gui_parse_elapsed("100:00.00", seconds) && std::fabs(seconds - 6000.0) < 0.001, "hundred");
    if (g_failed != 0) {
        return 1;
    }
    std::cout << "gui_format ok\n";
    return 0;
}
