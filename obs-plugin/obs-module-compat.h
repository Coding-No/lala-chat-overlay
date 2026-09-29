#pragma once
#include <windows.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LIBOBS_API_MAJOR_VER 32
#define LIBOBS_API_MINOR_VER 0
#define LIBOBS_API_PATCH_VER 4

#define MAKE_SEMANTIC_VERSION(maj, min, patch) (((maj) << 24) | ((min) << 16) | (patch))
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
