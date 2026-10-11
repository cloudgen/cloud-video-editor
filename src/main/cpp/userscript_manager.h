// =========================================================================
// userscript_manager.h
// CIAO DEFENSIVE CODING STYLE - LUA USERSCRIPT MANAGER HEADER
// =========================================================================
//
// Purpose:
//   Public C-compatible API for the Lua userscript system.
//
// Protection Rule (Sacred):
//   Future AI or developers MUST NOT change function signatures or remove
//   the extern "C" guards.
//   Debug field list: requirement-cpp-debug-mode.md.
//   Lua host (sol2): requirement-cpp-lua-script.md.
//
// Last updated: May 2026
// =========================================================================

#ifndef USERSCRIPT_MANAGER_H
#define USERSCRIPT_MANAGER_H

#include <webkit2/webkit2.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * initialize_userscript_manager
 *
 * Purpose:
 *   Creates and initializes the LuaScriptManager with the given WebKitWebView.
 *   Must be called once after the web_view is fully created.
 */
void initialize_userscript_manager(WebKitWebView *web_view);

/**
 * userscript_on_url_changed
 *
 * Purpose:
 *   Notifies the Lua userscript system that the current URL has changed.
 */
void userscript_on_url_changed(const gchar *uri);

#ifdef __cplusplus
}
#endif

#endif // USERSCRIPT_MANAGER_H