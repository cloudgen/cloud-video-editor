// gui_window.h
// The VideoTools picture window. Encode stays in video_domain.

#ifndef GUI_WINDOW_H
#define GUI_WINDOW_H

// Open the video window. Returns 0 when the window closes, or 1 when the
// display is refused. Does not call ciao_output_init.
int gui_window_open(int argc, char **argv);

#endif
