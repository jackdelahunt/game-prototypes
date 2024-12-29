#pragma once

#include "common.h"

#include "sokol/sokol_gfx.h"

extern "C" {
    typedef struct {
        Func(void, init, i32 width, i32 height, const wchar_t *title);
        Func(bool, process_events);
        Func(void *, alloc_state, i32 size);
        Func(void, present);
        Func(sg_swapchain, swapchain);
        Func(sg_environment, enviroment);
        Func(void, write, const char *text, i32 length);
    } Platform;

    DLL_EXPORT
    void set_platform(Platform p);
    DLL_EXPORT
    void start();
}
