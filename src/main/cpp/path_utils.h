// =========================================================================
// CIAO DEFENSIVE CODING STYLE - PATH UTILITIES MODULE
// =========================================================================
//
// !!! THIS IS A GNOME/GTK APPLICATION !!!
// !!! ONLY g_print() / g_printerr() ARE ALLOWED THROUGH ciao_* FUNCTIONS !!!
// 
// Purpose:
//   Single Point of Truth for safe path construction, tilde expansion,
//   and storage directory resolution. Prevents the critical bug where
//   "~/..." produced paths like "<home>/~/.app/...".
//
// Protection Rule (Sacred):
//   Future AI or developers MUST NOT:
//     - Remove tilde expansion logic
//     - Hardcode any paths or use literal "~/"
//     - Bypass this module for storage path construction
//   All cookie/settings/storage paths must go through here.
//
// Last updated: 2026-05-17 (introduced to fix tilde bug)
// =========================================================================

#ifndef PATH_UTILS_H
#define PATH_UTILS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void resolve_full_storage_path(const char *base_path,
                               char *full_path,
                               size_t max_len);

#ifdef __cplusplus
}
#endif

#endif // PATH_UTILS_H