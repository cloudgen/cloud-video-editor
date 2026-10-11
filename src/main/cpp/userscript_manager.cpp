// =========================================================================
// userscript_manager.cpp
// CIAO DEFENSIVE CODING STYLE - LUA USERSCRIPT MANAGER (gemini-wilgat)
// =========================================================================
//
// Purpose:
//   Single Point of Truth for the Lua userscript system.
//   Loads all *.lua files from ~/.app/gemini-wilgat/userscripts/
//   and provides a safe sandboxed API to WebKitWebView.
//
// Key Features:
//   - sol2 Lua binding (header-only). Law: requirement-cpp-lua-script.md
//   - Automatic loading of all .lua files (alphabetical order)
//   - Support for @match and @priority in script headers (basic)
//   - Safe API: inject_javascript(), log(), get_current_url(), register_on_url_changed()
//   - Full defensive error handling and ciao_* logging
//   - No dangerous Lua libraries (no os, io, etc.)
//   - Debug values: requirement-cpp-debug-mode.md (ciao_debug only)
//
// Protection Rule (Sacred):
//   Future AI or developers MUST NOT:
//     - Remove sandbox restrictions
//     - Bypass javascript_finished callback for JS injection
//     - Hard-code any paths (must use config_get_scripts_path + resolve_full_storage_path)
//     - Remove per-script error isolation
//
// Last updated: May 2026
// =========================================================================

#include "userscript_manager.h"
#include "config.h"
#include "output.h"
#include "path_utils.h"
#include <webkit2/webkit2.h>
#include <sol/sol.hpp>
#include <filesystem>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>

namespace fs = std::filesystem;

// =========================================================================
// Internal helper: Simple header parser (@match, @priority)
// =========================================================================
static std::string header_value(const std::string& line, const char* key)
{
    const std::string prefix = std::string("-- ") + key;
    if (line.compare(0, prefix.size(), prefix) != 0) return std::string();
    if (line.size() > prefix.size()) {
        const char next = line[prefix.size()];
        if (next != ' ' && next != '\t' && next != '\r') return std::string();
    }
    const std::string rest = line.substr(prefix.size());
    const size_t start = rest.find_first_not_of(" \t");
    if (start == std::string::npos) return std::string();
    const size_t end = rest.find_last_not_of(" \t\r");
    if (end == std::string::npos || end < start) return std::string();
    return rest.substr(start, end - start + 1);
}

static bool parse_userscript_header(const std::string& filepath, std::string& match_pattern, int& priority)
{
    std::ifstream file(filepath);
    if (!file.is_open()) return false;

    std::string line;
    priority = 50; // default priority
    match_pattern.clear();

    while (std::getline(file, line)) {
        const std::string match_text = header_value(line, "@match");
        if (!match_text.empty()) {
            match_pattern = match_text;
        }
        const std::string priority_text = header_value(line, "@priority");
        if (!priority_text.empty()) {
            try {
                priority = std::stoi(priority_text);
            } catch (...) {}
        }
        // Stop after header section
        if (line.find("-- ====") == 0 || line.find("function") == 0) break;
    }
    return true;
}

// =========================================================================
// LuaScriptManager class
// =========================================================================
class LuaScriptManager {
public:
    explicit LuaScriptManager(WebKitWebView* web_view)
        : web_view_(web_view), enabled_(false), script_load_state_(ScriptLoadState::IDLE)
    {
        if (!web_view || !WEBKIT_IS_WEB_VIEW(web_view)) {
            ciao_error("[USERSCRIPT] Invalid WebKitWebView passed to LuaScriptManager");
            debug_report("init", "invalid-web-view");
            return;
        }

        enabled_ = config_get_scripts_enabled();
        if (!enabled_) {
            debug_report("init", "disabled");
            return;
        }

        std::string raw_path = config_get_scripts_path();
        char resolved[1024] = {0};
        resolve_full_storage_path(raw_path.c_str(), resolved, sizeof(resolved));

        scripts_path_ = resolved;
        if (scripts_path_.empty()) {
            ciao_warn("[USERSCRIPT] Could not resolve scripts path");
            debug_report("init", "empty-path");
            return;
        }

        // ──────────────────────────────────────────────
        // AUTO-CREATE USERSCRIPTS DIRECTORY (NEW)
        // ──────────────────────────────────────────────
        if (!scripts_path_.empty()) {
            gint result = g_mkdir_with_parents(scripts_path_.c_str(), 0700);
            if (result == 0) {
                ciao_info("[USERSCRIPT] ✅ Created userscripts directory: %s", scripts_path_.c_str());
            } else if (result == -1 && errno != EEXIST) {
                ciao_warn("[USERSCRIPT] Failed to create userscripts directory: %s", scripts_path_.c_str());
            } else {
                ciao_debug("[USERSCRIPT] Userscripts directory already exists: %s", scripts_path_.c_str());
            }
        }

        ciao_info("[USERSCRIPT] Scripts directory resolved to: %s", scripts_path_.c_str());

        // Initialize Lua state (sandboxed)
        try {
            lua_.open_libraries(sol::lib::base, sol::lib::string, sol::lib::table, sol::lib::math);

            // Safe API exposed to Lua
            lua_.set_function("inject_javascript", [this](const std::string& js) {
                this->inject_javascript(js);
            });

            lua_.set_function("log", [this](const std::string& msg) {
                ciao_info("[LUA] %s", msg.c_str());
            });

            lua_.set_function("get_current_url", [this]() -> std::string {
                const gchar* uri = webkit_web_view_get_uri(web_view_);
                return uri ? std::string(uri) : std::string();
            });

            lua_.set_function("register_on_url_changed", [this](sol::function fn) {
                url_changed_hooks_.push_back(fn);
                ciao_debug("[USERSCRIPT] Registered on_url_changed hook");
            });

            ciao_info("[USERSCRIPT] Lua VM initialized successfully");
        } catch (const sol::error& e) {
            ciao_error("[USERSCRIPT] Failed to initialize Lua: %s", e.what());
            enabled_ = false;
            debug_report("init", "lua-init-error");
            return;
        }

        debug_report("init", nullptr);
        load_all_scripts();
    }

    ~LuaScriptManager() = default;

    // Called from on_uri_changed in url_bar_page.cpp
    void on_url_changed(const std::string& url)
    {
        if (!enabled_ || url_changed_hooks_.empty()) {
            debug_url("url-changed", url, !enabled_ ? "disabled" : "no-hooks");
            return;
        }

        debug_url("url-changed", url, nullptr);

        for (auto& hook : url_changed_hooks_) {
            try {
                hook(url);
            } catch (const sol::error& e) {
                ciao_warn("[USERSCRIPT] Hook error: %s", e.what());
            }
        }
    }

    void inject_javascript(const std::string& js_code)
    {
        if (!web_view_ || js_code.empty()) return;

        ciao_debug("[USERSCRIPT] inject bytes=%zu", js_code.length());

        webkit_web_view_evaluate_javascript(
            web_view_,
            js_code.c_str(),
            -1,
            NULL,
            NULL,
            NULL,
            javascript_finished,
            NULL
        );
    }

private:
    WebKitWebView* web_view_ = nullptr;
    sol::state lua_;
    std::vector<sol::function> url_changed_hooks_;
    bool enabled_ = false;
    std::string scripts_path_;

    enum class ScriptLoadState { IDLE, SCANNING, LOADING, LOADED, ERROR } script_load_state_ = ScriptLoadState::IDLE;

    // Callback for JS evaluation (reuses existing safe callback pattern)
    static void javascript_finished(GObject* object, GAsyncResult* result, gpointer user_data)
    {
        (void)user_data;
        GError* error = nullptr;
        webkit_web_view_evaluate_javascript_finish(WEBKIT_WEB_VIEW(object), result, &error);

        if (error) {
            ciao_warn("[USERSCRIPT] JS execution error: %s", error->message);
            g_error_free(error);
        } else {
            ciao_debug("[USERSCRIPT] JavaScript executed successfully");
        }
    }

    const char* load_state_label() const
    {
        switch (script_load_state_) {
            case ScriptLoadState::IDLE: return "IDLE";
            case ScriptLoadState::SCANNING: return "SCANNING";
            case ScriptLoadState::LOADING: return "LOADING";
            case ScriptLoadState::LOADED: return "LOADED";
            case ScriptLoadState::ERROR: return "ERROR";
        }
        return "UNKNOWN";
    }

    const char* path_or_empty() const
    {
        return scripts_path_.empty() ? "(empty)" : scripts_path_.c_str();
    }

public:
    void debug_report(const char* moment, const char* reason) const
    {
        if (reason && reason[0] != '\0') {
            ciao_debug("[USERSCRIPT] %s enabled=%d state=%s hooks=%zu path=%s reason=%s",
                       moment ? moment : "report",
                       enabled_ ? 1 : 0,
                       load_state_label(),
                       url_changed_hooks_.size(),
                       path_or_empty(),
                       reason);
        } else {
            ciao_debug("[USERSCRIPT] %s enabled=%d state=%s hooks=%zu path=%s",
                       moment ? moment : "report",
                       enabled_ ? 1 : 0,
                       load_state_label(),
                       url_changed_hooks_.size(),
                       path_or_empty());
        }
    }

    void debug_url(const char* moment, const std::string& url, const char* reason) const
    {
        const char* shown = url.empty() ? "(null)" : url.c_str();
        if (reason && reason[0] != '\0') {
            ciao_debug("[USERSCRIPT] %s enabled=%d state=%s hooks=%zu path=%s url=%s reason=%s",
                       moment ? moment : "url-changed",
                       enabled_ ? 1 : 0,
                       load_state_label(),
                       url_changed_hooks_.size(),
                       path_or_empty(),
                       shown,
                       reason);
        } else {
            ciao_debug("[USERSCRIPT] %s enabled=%d state=%s hooks=%zu path=%s url=%s",
                       moment ? moment : "url-changed",
                       enabled_ ? 1 : 0,
                       load_state_label(),
                       url_changed_hooks_.size(),
                       path_or_empty(),
                       shown);
        }
    }

private:
    void debug_script(int priority, const std::string& match_pattern, const std::string& filepath) const
    {
        ciao_debug("[USERSCRIPT] script priority=%d match=%s path=%s",
                   priority,
                   match_pattern.empty() ? "(none)" : match_pattern.c_str(),
                   filepath.c_str());
    }

    void load_all_scripts()
    {
        if (scripts_path_.empty()) {
            debug_report("loaded", "empty-path");
            return;
        }

        script_load_state_ = ScriptLoadState::SCANNING;
        ciao_info("[USERSCRIPT] Scanning directory: %s", scripts_path_.c_str());

        struct FoundScript {
            int priority;
            std::string match;
            std::string path;
        };
        std::vector<FoundScript> scripts;

        try {
            for (const auto& entry : fs::directory_iterator(scripts_path_)) {
                if (entry.is_regular_file() && entry.path().extension() == ".lua") {
                    FoundScript found;
                    found.path = entry.path().string();
                    found.priority = 50;
                    parse_userscript_header(found.path, found.match, found.priority);
                    scripts.push_back(std::move(found));
                }
            }
        } catch (const std::exception& e) {
            ciao_error("[USERSCRIPT] Failed to scan scripts directory: %s", e.what());
            script_load_state_ = ScriptLoadState::ERROR;
            debug_report("loaded", "scan-error");
            return;
        }

        std::sort(scripts.begin(), scripts.end(), [](const FoundScript& a, const FoundScript& b) {
            return a.priority > b.priority;
        });

        script_load_state_ = ScriptLoadState::LOADING;
        for (const auto& item : scripts) {
            debug_script(item.priority, item.match, item.path);
            ciao_info("[USERSCRIPT] Loading script (priority %d): %s", item.priority, item.path.c_str());
            try {
                lua_.script_file(item.path);
            } catch (const sol::error& e) {
                ciao_error("[USERSCRIPT] Failed to load %s: %s", item.path.c_str(), e.what());
            }
        }

        script_load_state_ = ScriptLoadState::LOADED;
        ciao_info("[USERSCRIPT] Loaded %zu Lua scripts successfully", scripts.size());
        debug_report("loaded", nullptr);
    }
};

// Global instance (created in webkit_setup.cpp)
static std::unique_ptr<LuaScriptManager> g_script_manager;

// Public API (called from webkit_setup.cpp)
void initialize_userscript_manager(WebKitWebView* web_view)
{
    if (g_script_manager) {
        g_script_manager->debug_report("init", "already-initialized");
        return;
    }
    g_script_manager = std::make_unique<LuaScriptManager>(web_view);
}

// Called from on_uri_changed in url_bar_page.cpp
void userscript_on_url_changed(const gchar* uri)
{
    if (!g_script_manager) {
        ciao_debug("[USERSCRIPT] url-changed enabled=0 state=IDLE hooks=0 path=(empty) url=%s reason=no-manager",
                   uri ? uri : "(null)");
        return;
    }
    if (!uri) {
        g_script_manager->debug_url("url-changed", std::string(), "null-uri");
        return;
    }
    g_script_manager->on_url_changed(std::string(uri));
}