#ifndef H_PLATFORM
#define H_PLATFORM

#include "common.h"

#include "sokol/sokol_gfx.h"

enum class InputState {
    UP,
    DOWN,
};

enum MouseButton {
    MOUSE_LEFT,
    MOUSE_RIGHT,
    MOUSE_MIDDLE,
    _MOUSE_LAST_
};
enum Key {
    KEY_ZERO, KEY_ONE, KEY_TWO, KEY_THREE, KEY_FOUR,
    KEY_FIVE, KEY_SIX, KEY_SEVEN, KEY_EIGHT, KEY_NINE,

    KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I,
    KEY_J, KEY_K, KEY_L, KEY_M, KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R,
    KEY_S, KEY_T, KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z,

    _KEY_LAST_
};

internal void platform_init_window(i32 width, i32 height, const char *title);
internal void platform_process_events();
internal sg_swapchain platform_swapchain();
internal sg_environment platform_enviroment();
internal void platform_present();
internal void platform_stdout(const char *text, i64 length);

#endif
