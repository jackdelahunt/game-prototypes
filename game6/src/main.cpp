#include <stdint.h>
#include <stdio.h>

#define GLEW_STATIC
#include "glew/include/GL/glew.h"
#include "glfw/GLFW/glfw3.h"

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"

// Total: 3
// Started: 8:30

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
    i32 width;
    i32 height;
    GLFWwindow *window;
    InputState keys[348];
};

State state;

bool init_opengl();
void init_imgui();
GLFWwindow *create_window(const char *title, i32 width, i32 height);
void glfw_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
void glfw_error_callback(int error_code, const char* description);

int main() {
    state = {
        .width = 1080,
        .height = 720,
    };

    state.window = create_window("game6", state.width, state.height);
    if (state.window == nullptr) {
        printf("failed to create window");
        return -1;
    }

    bool ok = init_opengl();
    if (!ok) {
        printf("failed to opengl functions");
        return -1;
    }

    init_imgui();

    while (!glfwWindowShouldClose(state.window)) {
        glfwPollEvents();

        if (state.keys[GLFW_KEY_ESCAPE] == InputState::down) {
            glfwSetWindowShouldClose(state.window, GLFW_TRUE);
        }

        { // imgui draw commands
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
    
            static bool demo = true;
            ImGui::ShowDemoWindow(&demo);
    
        }

        glClear(GL_COLOR_BUFFER_BIT);

        { // our rendering code goes here

        }

        { // imgui rendering
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            GLFWwindow *current = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(current);
        }

        glfwSwapBuffers(state.window);
    }

    glfwTerminate();

    return 0;
}

bool init_opengl() {
    GLenum result = glewInit();
    if (result != GLEW_OK) {
        return false;
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float f = 0.8f;
    glClearColor(f, f, f, 1.0f);

    glViewport(0, 0, state.width, state.height);

    return true;
}

void init_imgui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGui::StyleColorsDark();

    ImGuiIO& io = ImGui::GetIO();

    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    ImGui_ImplGlfw_InitForOpenGL(state.window, true);
    ImGui_ImplOpenGL3_Init("#version 460");
}

GLFWwindow *create_window(const char *title, i32 width, i32 height) {
    if (glfwInit() == 0) {
        return nullptr;
    }

    GLFWwindow *window = glfwCreateWindow(width, height, title, 0, 0);
    if (window == nullptr) {
        return nullptr;
    }

    glfwMakeContextCurrent(window);

    glfwSetErrorCallback(glfw_error_callback);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);

    glfwSwapInterval(1);
    glfwSetKeyCallback(window, glfw_key_callback);

    return window;
}

void glfw_error_callback(int error_code, const char* description) {
    printf("glfw error: [%d]: %s", error_code, description);
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
