// =========================================================================
// login_detector_app.cpp
// CIAO DEFENSIVE CODING STYLE - TTS KEEPER ONLY (AFTER LUA MIGRATION)
// =========================================================================

#include "login_detector_app.h"
#include "output.h"
#include <webkit2/webkit2.h>

// TTS Keeper remains (Lua userscripts now handle login flows)
static void tts_javascript_finished(GObject *object, GAsyncResult *result, gpointer user_data)
{
    (void)user_data;
    GError *error = NULL;
    webkit_web_view_evaluate_javascript_finish(WEBKIT_WEB_VIEW(object), result, &error);

    if (error) {
        ciao_debug("[TTS-KEEPER] JS Error: %s", error->message);
        g_error_free(error);
    } else {
        ciao_debug("[TTS-KEEPER] Script injected successfully");
    }
}

void inject_tts_keeper(WebKitWebView *web_view)
{
    if (web_view == NULL || !WEBKIT_IS_WEB_VIEW(web_view)) {
        ciao_warn("[TTS-KEEPER] Invalid web_view, skipping injection");
        return;
    }

    static const char *TTS_KEEPER_SCRIPT =
    "(function(){"
    "  try {"
    "    if (!('speechSynthesis' in window)) return;"
    "    if (window.__ciao_tts_keeper_installed) return;"
    "    window.__ciao_tts_keeper_installed = true;"
    "    var timer = null;"
    "    var keepAlive = function() {"
    "      try { if (window.speechSynthesis.speaking || window.speechSynthesis.pending) window.speechSynthesis.resume(); } catch(e){}"
    "    };"
    "    window.speechSynthesis.addEventListener('start', function(){ if(timer)clearInterval(timer); timer=setInterval(keepAlive,12000); });"
    "    window.speechSynthesis.addEventListener('end', function(){ if(timer){clearInterval(timer);timer=null;} });"
    "    console.log('[TTS-KEEPER] Installed (interval=12s)');"
    "  } catch (e) {}"
    "})();";

    webkit_web_view_evaluate_javascript(
        web_view,
        TTS_KEEPER_SCRIPT,
        -1,
        NULL,
        NULL,
        NULL,
        tts_javascript_finished,
        NULL
    );
}