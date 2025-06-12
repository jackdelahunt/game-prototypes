#ifndef ENGINE_CPP
#define ENGINE_CPP

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libs/libs.h"
#include "ack.cpp"
#include "math.cpp"

/////////////////////////////////////////////////////////////////////////////
//////////////////////////////// @window ////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
struct Window {
    i32 width;
    i32 height;
    string title;

    GLFWwindow *glfw_window;

    bool vsync;
    bool mouse_captured;
};

enum class InputState {
    UP,
    DOWN,
    PRESSED
};

StackArray<InputState, 348> KEYS = {};

struct {
    v2 position = {};
    v2 delta = {};
    StackArray<InputState, 8> buttons;
} MOUSE;

bool init_window(Window *window, string title);
void set_mouse_captured(Window *window, bool captured);
void poll_inputs();
void swap_buffers(Window *window);
void toggle_vsync(Window *window);
void glfw_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
void glfw_mouse_move_callback(GLFWwindow* window, f64 x, f64 y);
void glfw_mouse_button_callback(GLFWwindow* window, i32 button, i32 action, i32 mods);
void glfw_error_callback(int error_code, const char* description);

bool init_window(Window *window, string title) {
    if (glfwInit() == 0) {
        printf("Failed to init glfw\n");
        return false;
    }

    GLFWmonitor *monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);

    *window = Window {
        .width = mode->width,
        .height = mode->height,
        .title = title,
        .vsync = true,
        .mouse_captured = true,
    };
 
    glfwWindowHint(GLFW_MAXIMIZED, GL_TRUE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#if REPORT_GL_ERRORS
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
#endif

    window->glfw_window = glfwCreateWindow(window->width, window->height, window->title.c(), NULL, NULL);
    if (window->glfw_window == nullptr) {
        printf("Failed to create window\n");
        return false;
    }

    glfwMakeContextCurrent(window->glfw_window);
    glfwSetWindowUserPointer(window->glfw_window, window);

    glfwSwapInterval(window->vsync);
    set_mouse_captured(window, window->mouse_captured);

    glfwSetErrorCallback(glfw_error_callback);
    glfwSetKeyCallback(window->glfw_window, glfw_key_callback);
    glfwSetCursorPosCallback(window->glfw_window, glfw_mouse_move_callback);
    glfwSetMouseButtonCallback(window->glfw_window, glfw_mouse_button_callback);

    return true;
}

void set_mouse_captured(Window *window, bool captured) {
    window->mouse_captured = captured;

    i32 glfw_mode = captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL;
    glfwSetInputMode(window->glfw_window, GLFW_CURSOR, glfw_mode);
}

void poll_inputs() {
    // this will set the state of things to up or down
    // to keep track of what is already down, we can go through
    // every key before this and set it to pressed, if is still
    // down we dont get and event and it stays pressed, if we get
    // an event for that key it will be to set it to up so the
    // pressed we accidentlly set is changed, this is not the best
    // - 24/01/25
    //
    // copied from odin engine so maybe need to look into this more
    // - 03/03/25
    
    for (int i = 0; i < KEYS.size; i++) {
        if (KEYS[i] == InputState::DOWN) {
            KEYS[i] = InputState::PRESSED;
        }
    }

    for (int i = 0; i < MOUSE.buttons.size; i++) {
        if (MOUSE.buttons[i] == InputState::DOWN) {
            MOUSE.buttons[i] = InputState::PRESSED;
        }
    }

    // update mouse delta position, cant do this in the callback because
    // if there is no movement then the delta is stuck with a non zero
    // vector, so doing this before we check for events we know if there is
    // a change
    // - 31/05/25
    v2 last_mouse_position = MOUSE.position;

    glfwPollEvents();

    MOUSE.delta = MOUSE.position - last_mouse_position;

    // max the mouse delta vector can be, stops huge spikes mouse input 
    // when mouse changes capture like at start of game
    // - 02/06/25
    f32 max_delta = 75;
         
    if (length(MOUSE.delta) > max_delta) {
        MOUSE.delta = norm(MOUSE.delta) * max_delta;
    }
}

void swap_buffers(Window *window) {
    glfwSwapBuffers(window->glfw_window);
}

void toggle_vsync(Window *window) {
    window->vsync = !window->vsync;

    if (window->vsync) {
        glfwSwapInterval(1);
    } else {
        glfwSwapInterval(0); 
    }
}

void glfw_error_callback(int error_code, const char* description) {
    printf("glfw error: [%d]: %s\n", error_code, description);
}

void glfw_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    switch (action) {
         case GLFW_RELEASE:	{
            KEYS[key] = InputState::UP;
            break;
        }
        case GLFW_PRESS: {
            KEYS[key] = InputState::DOWN;
            break;
        }
        case GLFW_REPEAT: break;
    }
}

void glfw_mouse_move_callback(GLFWwindow* window, f64 x, f64 y) {
    // changing y position from glfw so the bottem left is the origin
    // by default glfw is top left as origin - 18/03/25
    Window *win_ptr = (Window *) glfwGetWindowUserPointer(window);

    MOUSE.position = v2{
        (f32) x,
        ((f32) -y) + win_ptr->height,
    };
}

void glfw_mouse_button_callback(GLFWwindow* window, i32 button, i32 action, i32 mods) {
     switch (action) {
         case GLFW_RELEASE:	{
            MOUSE.buttons[button] = InputState::UP;
            break;
        }
        case GLFW_PRESS: {
            MOUSE.buttons[button] = InputState::DOWN;
            break;
        }
        case GLFW_REPEAT: break;
    } 
}

v4 WHITE            = {1, 1, 1, 1};
v4 BLACK            = {0, 0, 0, 1};

v4 RED              = {1, 0, 0, 1};
v4 GREEN            = {0, 1, 0, 1};
v4 BLUE             = {0, 0, 1, 1};

v4 ORANGE           = {1, 0.64, 0.1, 1};
v4 CORNFLOUR_BLUE   = {0.35, 0.80, 0.80, 1};
v4 SUN_YELLOW       = {1, 0.95, 0.5, 1};

v4 alpha(v4 base, f32 alpha);
v4 brightness(v4 base, f32 brightness);

void print(v2 vector);
void print(v3 vector);
void print(v4 vector);

f32 accel_lerp(f32 a, f32 b, f32 f);

v4 alpha(v4 base, f32 alpha) {
    return {base.r, base.g, base.b, alpha};
}

v4 brightness(v4 base, f32 brightness) {
    v4 result;

    result.r = clamp(base.r * brightness, 0.0f, 1.0f);
    result.g = clamp(base.g * brightness, 0.0f, 1.0f);
    result.b = clamp(base.b * brightness, 0.0f, 1.0f);
    result.a = base.a;

    return result;
}

void print(v2 vector) {
    printf("{x: %f, y: %f}\n", vector.x, vector.y);
}

void print(v3 vector) {
    printf("{x: %f, y: %f, z: %f}\n", vector.x, vector.y, vector.z);
}

void print(v4 vector) {
    printf("{x: %f, y: %f, z: %f, w: %f}\n", vector.x, vector.y, vector.z, vector.w);
}

f32 accel_lerp(f32 a, f32 b, f32 f) {
    return a + f * (b - a);
}

#endif
