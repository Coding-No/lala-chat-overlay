#pragma once
#include <windows.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// OBS API version encoding: (major << 24) | (minor << 16) | patch
#define MAKE_SEMANTIC_VERSION(maj, min, patch) (((maj) << 24) | ((min) << 16) | (patch))

// ============================================================================
// OBS Version Compatibility Table:
// - OBS 30.x+  : libobs API 30.x, Qt6 (Qt6Gui.dll, Qt6Widgets.dll)
// - OBS 28-29.x: libobs API 28-29.x, Qt6
// - OBS 27.x   : libobs API 27.x, Qt5 (Qt5Gui.dll, Qt5Widgets.dll)
// - OBS 25-26.x: libobs API 25-26.x, Qt5
//
// We report version 28.0.0 as our compiled API version.
// This is high enough for modern features but won't be rejected by newer OBS.
// OBS uses only the major version for compatibility checks.
// ============================================================================
#define LIBOBS_API_MAJOR_VER 28
#define LIBOBS_API_MINOR_VER 0
#define LIBOBS_API_PATCH_VER 0
#define LIBOBS_API_VER MAKE_SEMANTIC_VERSION(LIBOBS_API_MAJOR_VER, LIBOBS_API_MINOR_VER, LIBOBS_API_PATCH_VER)

#ifndef MODULE_EXPORT
#define MODULE_EXPORT __declspec(dllexport)
#endif

typedef struct obs_module obs_module_t;
typedef void (*obs_frontend_cb)(void *private_data);

typedef void (*proc_obs_frontend_add_tools_menu_item)(const char *name, obs_frontend_cb callback, void *private_data);
typedef void* (*proc_obs_frontend_get_main_window)(void);

#ifdef __cplusplus
}
#endif
