#include "glad/glad.cpp"
#include "glfw/glfw3.h"

#include <stdint.h>
#include <stdio.h>

#define u8  uint8_t
#define u16 uint16_t
#define u32 uint32_t
#define u64 uint64_t

#define i8  int8_t
#define i16 int16_t
#define i32 int32_t
#define i64 int64_t

enum class InputState {
    up,
    down,
};

struct State {
    GLFWwindow *window;
    InputState keys[348];
};

State state;

bool init_opengl();
GLFWwindow *create_window(const char *title, i32 width, i32 height);
void glfw_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);

int main() {
    state = {};

    state.window = create_window("my glfw window", 640, 420);
    if (state.window == nullptr) {
        printf("failed to create window");
        return -1;
    }

    bool ok = init_opengl();
    if (!ok) {
        printf("failed to opengl functions");
        return -1;
    }

    while (!glfwWindowShouldClose(state.window)) {
        glfwPollEvents();

        if (state.keys[GLFW_KEY_ESCAPE] == InputState::down) {
            glfwSetWindowShouldClose(state.window, GLFW_TRUE);
        }

        glad_glClear(GL_COLOR_BUFFER_BIT);
        glfwSwapBuffers(state.window);
    }

    glfwTerminate();

    return 0;
}

bool init_opengl() {
    i32 loaded = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    if (loaded == 0) {
        return false;
    }

    glad_glEnable(GL_BLEND);
    glad_glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float f = 0.8f;
    glad_glClearColor(f, f, f, 1.0f);

    return true;
}

GLFWwindow *create_window(const char *title, i32 width, i32 height) {
    if (glfwInit() == 0) {
        return nullptr;
    }

    GLFWwindow *window = glfwCreateWindow(width, height, title, 0, 0);
    if (window == nullptr) {
        return nullptr;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, GL_MAJOR_VERSION);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, GL_MINOR_VERSION);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);

    glfwSetKeyCallback(window, glfw_key_callback);

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    return window;
}

void glfw_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    switch (action) {
         case GLFW_RELEASE:	{
            state.keys[key] = InputState::up;
            break;
        }
        case GLFW_PRESS: {
            state.keys[key] = InputState::down;
            break;
        }
        case GLFW_REPEAT: break;
    }
}
