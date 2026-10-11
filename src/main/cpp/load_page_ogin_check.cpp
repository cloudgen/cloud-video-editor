// load_page_ogin_check.cpp
// =========================================================================
// CIAO DEFENSIVE CODING STYLE - PAGE LOADING MODULE
// =========================================================================

#include "load_page_login_check.h"
#include "output.h"
#include "project.h"
#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

void load_page(WebKitWebView *web_view, const gchar *uri)
{
    if (web_view == NULL) {
        ciao_error("load_page: web_view is NULL");
        return;
    }
    if (uri == NULL || *uri == '\0') {
        ciao_error("load_page: uri is NULL or empty");
        return;
    }

    ciao_debug("Loading page: %s", uri);
    webkit_web_view_load_uri(web_view, uri);
    ciao_info("Page load requested: %s", uri);
}

// load_page_with_login_check is kept for backward compatibility
// Login flows are now handled by Lua userscripts via on_url_changed
void load_page_with_login_check(WebKitWebView *web_view, const gchar *uri)
{
    load_page(web_view, uri);
}