#ifndef WINDOW_CPP
#define WINDOW_CPP

#include "libs/libs.h"
#include "game.h"

struct Window {
    i32 width;
    i32 height;
    string title;
    GLFWwindow *glfw_window;
};

enum class InputState {
    up,
    down,
    pressed
};

Array<InputState, 348> KEYS = {};

bool init_window(i32 width, i32 height, string title);
void glfw_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
void glfw_error_callback(int error_code, const char* description);

bool init_window(Window *window, i32 width, i32 height, string title) {
    *window = Window {
        .width = width,
        .height = height,
        .title = title
    };

    if (glfwInit() == 0) {
        printf("failed to init glfw\n");
        return false;
    }

    window->glfw_window = glfwCreateWindow(width, height, title.c(), 0, 0);
    if (window->glfw_window == nullptr) {
        printf("failed to create window\n");
        return false;
    }

    glfwMakeContextCurrent(window->glfw_window);

    glfwSetErrorCallback(glfw_error_callback);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);

    glfwSwapInterval(1);
    glfwSetKeyCallback(window->glfw_window, glfw_key_callback);

    return true;
}

void glfw_error_callback(int error_code, const char* description) {
    printf("glfw error: [%d]: %s", error_code, description);
}

void glfw_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    switch (action) {
         case GLFW_RELEASE:	{
            KEYS[key] = InputState::up;
            break;
        }
        case GLFW_PRESS: {
            KEYS[key] = InputState::down;
            break;
        }
        case GLFW_REPEAT: break;
    }
}

#endif 
