// =========================================================================
// login_detector_app.h
// CIAO DEFENSIVE CODING STYLE - TTS KEEPER ONLY (AFTER LUA MIGRATION)
// =========================================================================
//
// Login detection / auto-sign-in / continue-with-Google logic
// has been migrated to Lua userscripts (see requirement-cpp-window.md).
// This file now only retains the TTS keeper.
//
// Protection Rule (Sacred):
//   Do NOT re-introduce old JS login code here.
// =========================================================================

#ifndef LOGIN_DETECTOR_APP_H
#define LOGIN_DETECTOR_APP_H

#include <webkit2/webkit2.h>

// TTS keeper for the Gemini window.
void inject_tts_keeper(WebKitWebView *web_view);

#endif // LOGIN_DETECTOR_APP_H