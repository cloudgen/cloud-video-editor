// gui_format.h
// Elapsed clock for the video window: MM:SS.CC

#ifndef GUI_FORMAT_H
#define GUI_FORMAT_H

#include <string>

std::string gui_format_elapsed(double seconds);
bool gui_parse_elapsed(const std::string &text, double &seconds);

#endif
