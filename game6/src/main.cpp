#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define GLEW_STATIC
#include "glew/include/GL/glew.h"
#include "glfw/GLFW/glfw3.h"
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"
#include "glm/glm.hpp"
#include "glm/ext.hpp"

// Total: 7

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef float f32;
typedef double f64;

typedef glm::vec2 v2;
typedef glm::vec3 v3;
typedef glm::vec4 v4;

typedef glm::mat4 mat4;

#define MAX_QUADS 100

enum class InputState {
    up,
    down,
};

struct Vertex {
    v3 position;
    v4 colour;
};

struct Quad {
    Vertex vertices[4];
};

struct Renderer {
    Quad quads[MAX_QUADS];
    i64 quad_count;

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

    struct {
        v2 position;
        f32 orthographic_size;
        f32 near_plane;
        f32 far_plane;
    } camera;

    Renderer renderer;
} state = {};

template <typename T>
struct Slice {
    T *data;
    i64 len;
};

v4 RED      = {1, 0, 0, 1};
v4 GREEN    = {0, 1, 0, 1};
v4 BLUE     = {0, 0, 1, 1};

bool create_window();
void glfw_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
void glfw_error_callback(int error_code, const char* description);

bool init_opengl();
void init_imgui();
bool init_renderer();
void draw_quad(v3 position, v2 size);
mat4 get_view_matrix();
mat4 get_projection_matrix();

Slice<char> read_file(const char *path);

int main() {
    state = {
        .title = "game6",
        .width = 1080,
        .height = 720,
        .camera = {
            .position = {},
            .orthographic_size = 10,
            .near_plane = 0.1f,
            .far_plane = 100.0f,
        }
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

        static v3 position = {0, 0, 1};
        static v2 size = {1, 1};

        { // our rendering code goes here
            state.renderer.quad_count = 0;
            draw_quad(position, size);
        }

        { // imgui draw commands
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            if (ImGui::Begin("Inspector", 0, ImGuiChildFlags_AlwaysAutoResize)) {

                ImGui::PushID("camera");
                ImGui::SeparatorText("Camera");
                ImGui::SliderFloat2("position", &state.camera.position.x, -100, 100);
                ImGui::SliderFloat("ortho size", &state.camera.orthographic_size, 1, 100);
                ImGui::PopID();

                ImGui::PushID("quad");
                ImGui::SeparatorText("Quad");
                ImGui::SliderFloat3("position", &position.x, -100, 100);
                ImGui::SliderFloat2("size", &size.x, 1, 100);
                ImGui::PopID();

                v3 ndc = state.renderer.quads[0].vertices[0].position;
                ImGui::Text("%f %f %f", ndc.x, ndc.y, ndc.z);


                ImGui::End();
            }
    
            // bool demo = false;
            // ImGui::ShowDemoWindow(&demo);
        }

        glClear(GL_COLOR_BUFFER_BIT);

        { // draw the things we have sent to the renderer
            glBindBuffer(GL_ARRAY_BUFFER, state.renderer.vertex_buffer_id);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(Quad) * state.renderer.quad_count, state.renderer.quads);

            glUseProgram(state.renderer.shader_program_id);
            glBindVertexArray(state.renderer.vertex_array_id);
            glDrawElements(GL_TRIANGLES, 6 * state.renderer.quad_count, GL_UNSIGNED_INT, 0);
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

void draw_quad(v3 position, v2 size) {
    mat4 transformation_matrix = glm::mat4(1);

    // model
    transformation_matrix = glm::translate(transformation_matrix, position); 
    transformation_matrix = glm::scale(transformation_matrix, {size.x, size.y, 1});    

    // view
    transformation_matrix = get_view_matrix() * transformation_matrix;

    // projection
    transformation_matrix = get_projection_matrix() * transformation_matrix;

    Quad *quad = &state.renderer.quads[state.renderer.quad_count];
    state.renderer.quad_count++;

    v4 top_left      = {-0.5,   0.5, 0, 1};
    v4 top_right     = { 0.5,   0.5, 0, 1};
    v4 bottom_right  = { 0.5,  -0.5, 0, 1};
    v4 bottom_left   = {-0.5,  -0.5, 0, 1};

    quad->vertices[0].position = transformation_matrix * top_left;
    quad->vertices[1].position = transformation_matrix * top_right;
    quad->vertices[2].position = transformation_matrix * bottom_right;
    quad->vertices[3].position = transformation_matrix * bottom_left;

    quad->vertices[0].colour = RED;
    quad->vertices[1].colour = GREEN;
    quad->vertices[2].colour = BLUE;
    quad->vertices[3].colour = RED;
}

mat4 get_view_matrix() {
    return glm::lookAt(
        v3{state.camera.position.x, state.camera.position.y, 0},
        v3{state.camera.position.x, state.camera.position.y, 1},
        v3{0, 1, 0}
    );
}

mat4 get_projection_matrix() {
    f32 aspect_ratio = (f32)state.width / (f32)state.height;
    f32 size = state.camera.orthographic_size;

    return glm::ortho(
        -size * aspect_ratio, 
        size * aspect_ratio, 
        -size, size,
        state.camera.near_plane, state.camera.far_plane
    );
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
