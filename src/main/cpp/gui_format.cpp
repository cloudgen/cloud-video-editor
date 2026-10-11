#include "gui_format.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

std::string gui_format_elapsed(double seconds)
{
    if (seconds < 0.0 || seconds != seconds) {
        seconds = 0.0;
    }
    long long total = std::llround(seconds * 100.0);
    if (total < 0) {
        total = 0;
    }
    long long minutes = total / 6000;
    long long rest = total % 6000;
    long long secs = rest / 100;
    long long cs = rest % 100;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%02lld:%02lld.%02lld", minutes, secs, cs);
    return buf;
}

bool gui_parse_elapsed(const std::string &text, double &seconds)
{
    if (text.size() < 7) {
        return false;
    }
    size_t colon = text.find(':');
    size_t dot = text.find('.', colon == std::string::npos ? 0 : colon);
    if (colon == std::string::npos || dot == std::string::npos) {
        return false;
    }
    if (colon == 0 || dot != colon + 3 || text.size() != dot + 3) {
        return false;
    }
    for (size_t i = 0; i < text.size(); ++i) {
        if (i == colon || i == dot) {
            continue;
        }
        if (text[i] < '0' || text[i] > '9') {
            return false;
        }
    }
    int minutes = std::atoi(text.substr(0, colon).c_str());
    int secs = std::atoi(text.substr(colon + 1, 2).c_str());
    int cs = std::atoi(text.substr(dot + 1, 2).c_str());
    if (secs > 59) {
        return false;
    }
    seconds = static_cast<double>(minutes) * 60.0 + static_cast<double>(secs) +
              static_cast<double>(cs) / 100.0;
    return true;
}
