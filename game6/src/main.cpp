#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define GLEW_STATIC
#include "glew/include/GL/glew.h"
#include "glfw/GLFW/glfw3.h"

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"

// Total: 5.5

#define u8  uint8_t
#define u16 uint16_t
#define u32 uint32_t
#define u64 uint64_t

#define i8  int8_t
#define i16 int16_t
#define i32 int32_t
#define i64 int64_t

#define f32 float

#define MAX_QUADS 100

enum class InputState {
    up,
    down,
};

struct Vertex {
    f32 position[3]; 
    f32 colour[3]; 
};

struct Quad {
    Vertex vertices[4];
};

struct Renderer {
    Quad quads[MAX_QUADS];

    u32 vertex_array_id;
    u32 vertex_buffer_id;
    u32 index_buffer_id;
    u32 shader_program_id;
};

struct State {
    const char *title;
    i32 width;
    i32 height;
    GLFWwindow *window;
    InputState keys[348];

    Renderer renderer;
} state = {};

template <typename T>
struct Slice {
    T *data;
    i64 len;
};

bool create_window();
void glfw_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
void glfw_error_callback(int error_code, const char* description);

bool init_opengl();
void init_imgui();
bool init_renderer();

Slice<char> read_file(const char *path);

int main() {
    state = {
        .title = "game6",
        .width = 1080,
        .height = 720,
    };

    {
        bool ok;
    
        ok = create_window();
        if (!ok) {
            printf("failed to create window");
            return -1;
        }
    
        ok = init_opengl();
        if (!ok) {
            printf("failed to opengl functions");
            return -1;
        }
    
        init_imgui();

        ok = init_renderer();
        if (!ok) {
            printf("failed to init the renderer");
            return -1;
        }
    }

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

bool create_window() {
    if (glfwInit() == 0) {
        return false;
    }

    GLFWwindow *window = glfwCreateWindow(state.width, state.height, state.title, 0, 0);
    if (window == nullptr) {
        return false;
    }

    glfwMakeContextCurrent(window);

    glfwSetErrorCallback(glfw_error_callback);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);

    glfwSwapInterval(1);
    glfwSetKeyCallback(window, glfw_key_callback);

    state.window = window;

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

bool init_renderer() {
    { // load and compile shaders
        const i64 buffer_size = 640;
        i32 compile_status = 0;
        i32 link_status = 0;
        char error_buffer[buffer_size];
    
        Slice<char> vertex_shader_source = read_file("./src/shaders/vertex.shader");
        if (vertex_shader_source.len == 0) {
            return false;
        }

        Slice<char> fragment_shader_source = read_file("./src/shaders/fragment.shader");
        if (fragment_shader_source.len == 0) {
            return false;
        }

        u32 vertex_shader = glCreateShader(GL_VERTEX_SHADER);

        glShaderSource(vertex_shader, 1, &vertex_shader_source.data, NULL);
        glCompileShader(vertex_shader);

        glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &compile_status);
        if (compile_status == 0) {
            glGetShaderInfoLog(vertex_shader, buffer_size, nullptr, &error_buffer[0]);
            printf("failed to compile vertex shader: %s", error_buffer);
            return false;
        }

        u32 fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

        glShaderSource(fragment_shader, 1, &fragment_shader_source.data, NULL);
        glCompileShader(fragment_shader);

        glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &compile_status);
        if (compile_status == 0) {
            glGetShaderInfoLog(fragment_shader, buffer_size, nullptr, &error_buffer[0]);
            printf("failed to compile fragment shader: %s", error_buffer);
            return false;
        }

        u32 shader_program = glCreateProgram();
        glAttachShader(shader_program, vertex_shader);
        glAttachShader(shader_program, fragment_shader);
        glLinkProgram(shader_program);

        glGetProgramiv(shader_program, GL_LINK_STATUS, &link_status);
        if (link_status == 0) {
            glGetProgramInfoLog(shader_program, buffer_size, nullptr, &error_buffer[0]);
            printf("failed to link shader program: %s", error_buffer);
            return false;
        }

        state.renderer.shader_program_id = shader_program;

        glUseProgram(shader_program);

        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
    }

    { // vertex array
        u32 vertex_array;
        glGenVertexArrays(1, &vertex_array);
        glBindVertexArray(vertex_array);

        state.renderer.vertex_array_id = vertex_array;
    }

    { // vertex buffer
        u32 vertex_buffer;
        glGenBuffers(1, &vertex_buffer);
        glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(Quad) * MAX_QUADS, state.renderer.quads, GL_DYNAMIC_DRAW);

        state.renderer.vertex_buffer_id = vertex_buffer;
    }

    { // index buffer
        const i64 index_buffer_length = MAX_QUADS * 6;
        u32 indices[index_buffer_length];

        i64 i = 0;
        while (i < index_buffer_length) {
            // vertex offset pattern to draw a quad
            // { 0, 1, 2,  0, 2, 3 }
            indices[i + 0] = ((i/6)*4 + 0);
            indices[i + 1] = ((i/6)*4 + 1);
            indices[i + 2] = ((i/6)*4 + 2);
            indices[i + 3] = ((i/6)*4 + 0);
            indices[i + 4] = ((i/6)*4 + 2);
            indices[i + 5] = ((i/6)*4 + 3);
            i += 6;
        }

        u32 index_buffer;
        glGenBuffers(1, &index_buffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(u32) * index_buffer_length, indices, GL_STATIC_DRAW);

        state.renderer.index_buffer_id = index_buffer;
    }

    { // vertex attributes
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), 0);                             // position
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *) (sizeof(f32) * 3));    // colour
        
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
    }

    return true;
}

Slice<char> read_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (file == nullptr) {
        return {};
    }

    fseek(file, 0, SEEK_END);
    i64 file_size = ftell(file);
    fseek(file, 0, SEEK_SET);  /* same as rewind(f); */
    
    char *data = (char *) malloc(file_size + 1);
    fread(data, file_size, 1, file);
    fclose(file);
    
    data[file_size] = 0; // null terminate

    return Slice<char> {.data = data, .len = file_size};
}
