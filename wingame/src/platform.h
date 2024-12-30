#ifndef H_PLATFORM
#define H_PLATFORM

#include "common.h"
#include "game.h"

#include "sokol/sokol_gfx.h"

void platform_init_window(i32 width, i32 height, Slice<char> title);
void platform_process_events();
sg_swapchain platform_swapchain();
sg_environment platform_enviroment();
void platform_present();

#endif
