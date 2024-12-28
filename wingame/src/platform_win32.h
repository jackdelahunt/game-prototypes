#pragma once
/*
    app wrapper for D3D11 samples
*/
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include "sokol/sokol_gfx.h"

void start();

#ifdef __cplusplus
extern "C" {
#endif

typedef struct d3d11_desc_t {
    int width;
    int height;
    int sample_count;
    bool no_depth_buffer;
    const wchar_t* title;
} d3d11_desc_t;

// initialize (window, d3d11 device, swapchain, ...)
void d3d11_init(const d3d11_desc_t* desc);
// shutdown function
void d3d11_shutdown(void);
// present the current frame
void d3d11_present(void);
// handle messages, return true if app should quit
bool d3d11_process_events(void);

// get current framebuffer width
int d3d11_width(void);
// get current framebuffer height
int d3d11_height(void);
// get initialized sg_environment struct
sg_environment d3d11_environment(void);
// get initialized sg_swapchainb struct
sg_swapchain d3d11_swapchain(void);

// input callback typedefs
typedef void(*d3d11_key_func)(int key);
typedef void(*d3d11_char_func)(wchar_t c);
typedef void(*d3d11_mouse_btn_func)(int btn);
typedef void(*d3d11_mouse_pos_func)(float x, float y);
typedef void(*d3d11_mouse_wheel_func)(float v);

// register key-down callback
void d3d11_key_down(d3d11_key_func);
// register key-up callback
void d3d11_key_up(d3d11_key_func);
// register character entry callback
void d3d11_char(d3d11_char_func);
// register mouse-button-down callback
void d3d11_mouse_btn_down(d3d11_mouse_btn_func);
// register mouse-button-up callback
void d3d11_mouse_btn_up(d3d11_mouse_btn_func);
// register mouse position callback
void d3d11_mouse_pos(d3d11_mouse_pos_func);
// register mouse wheel callback
void d3d11_mouse_wheel(d3d11_mouse_wheel_func);

#ifdef __cplusplus
} // extern "C"
#endif
