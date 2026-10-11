// gui_player.h
// One libmpv picture pane. The library is loaded with dlopen, not linked.

#ifndef GUI_PLAYER_H
#define GUI_PLAYER_H

#include <string>

struct GuiPlayer;
typedef struct _GtkWidget GtkWidget;

bool gui_player_backend_ok();
const char *gui_player_unavailable_reason();

GuiPlayer *gui_player_new();
GtkWidget *gui_player_widget(GuiPlayer *player);

void gui_player_load(GuiPlayer *player, const std::string &path);
void gui_player_clear(GuiPlayer *player);
void gui_player_play(GuiPlayer *player);
void gui_player_pause(GuiPlayer *player);
void gui_player_stop(GuiPlayer *player);
void gui_player_seek(GuiPlayer *player, double seconds);
void gui_player_pause_mute(GuiPlayer *player);
void gui_player_pause_all();
void gui_player_set_duration_hint(GuiPlayer *player, double seconds);

double gui_player_position(GuiPlayer *player);
double gui_player_duration(GuiPlayer *player);
bool gui_player_has_file(GuiPlayer *player);

typedef void (*GuiPlayerTick)(GuiPlayer *player, double position, double duration, void *user);
void gui_player_set_tick(GuiPlayer *player, GuiPlayerTick tick, void *user);

void gui_player_shutdown();

#endif
