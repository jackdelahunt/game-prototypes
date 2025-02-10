#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

#define GLEW_STATIC
#include "glew/include/GL/glew.h"
#include "glfw/GLFW/glfw3.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#include "HandmadeMath.h"

// Total: 9
// started: 4:30

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

#define MAX_QUADS 100

enum class InputState {
    up,
    down,
};

struct Vertex {
    HMM_Vec3 position;
    HMM_Vec4 colour;
};

struct Quad {
    Vertex vertices[4];
};

struct DrawCommand {
    HMM_Vec3 position;
    HMM_Vec2 size;
};

struct Renderer {
    DrawCommand commands[MAX_QUADS];
    i64 command_count;

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
        HMM_Vec3 position;
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

HMM_Vec4 RED      = {1, 0, 0, 1};
HMM_Vec4 GREEN    = {0, 1, 0, 1};
HMM_Vec4 BLUE     = {0, 0, 1, 1};

bool create_window();
void glfw_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
void glfw_error_callback(int error_code, const char* description);

// renderer - public
bool init_renderer();
void draw_quad(HMM_Vec3 position, HMM_Vec2 size);
void new_frame();
void draw_frame();

// renderer - internal
bool init_opengl();
void init_imgui();
void update_quad_buffer();
HMM_Mat4 get_view_matrix();
HMM_Mat4 get_projection_matrix();

Slice<char> read_file(const char *path);

int main() {
    state = {
        .title = "game6",
        .width = 1080,
        .height = 720,
        .camera = {
            .position = {0, 0, 0},
            .orthographic_size = 250,
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

        new_frame();

        static HMM_Vec3 position = {0, 0, 1};
        static HMM_Vec2 size = {100, 100};

        draw_quad(position, size);

        if (ImGui::Begin("Inspector", 0, ImGuiChildFlags_AlwaysAutoResize)) {
            ImGui::PushID("camera");
            ImGui::SeparatorText("Camera");
            ImGui::SliderFloat2("position", (f32 *) &state.camera.position, -5, 5);
            ImGui::SliderFloat("ortho size", &state.camera.orthographic_size, 1, 500);
            ImGui::PopID();
            ImGui::PushID("quad");
            ImGui::SeparatorText("Quad");
            ImGui::SliderFloat3("position", (f32 *) &position, -10, 10);
            ImGui::SliderFloat2("size", (f32 *) &size, 1, 300);
            ImGui::PopID();
            HMM_Vec3 ndc = state.renderer.quads[0].vertices[0].position;
            ImGui::Text("%f %f %f", ndc.X, ndc.Y, ndc.Z);
            ImGui::End();
        }

        draw_frame();
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
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *) offsetof(Vertex, position));   // position
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *) offsetof(Vertex, colour));     // colour
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
    }

    return true;
}

void new_frame() {
    state.renderer.quad_count = 0;
    state.renderer.command_count = 0;

    { // new frame for imgui
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame(); 
    }

    glClear(GL_COLOR_BUFFER_BIT);
}

void draw_frame() {
    update_quad_buffer();

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

void update_quad_buffer() {
    const HMM_Vec4 top_left      = {-0.5,   0.5, 0, 1};
    const HMM_Vec4 top_right     = { 0.5,   0.5, 0, 1};
    const HMM_Vec4 bottom_right  = { 0.5,  -0.5, 0, 1};
    const HMM_Vec4 bottom_left   = {-0.5,  -0.5, 0, 1};

    HMM_Mat4 view_projection = HMM_MulM4(get_projection_matrix(), get_view_matrix());

    state.renderer.quad_count = state.renderer.command_count;
    
    for (i64 i = 0; i < state.renderer.command_count; i++) {
        DrawCommand *command    = &state.renderer.commands[i];
        Quad *quad              = &state.renderer.quads[i];

        HMM_Mat4 model_matrix = HMM_M4D(1.0f);
        model_matrix = HMM_MulM4(model_matrix, HMM_Scale({command->size.X, command->size.Y, 1}));
        model_matrix = HMM_MulM4(model_matrix, HMM_Translate(command->position));
    
        HMM_Mat4 mvp_matrix = HMM_MulM4(view_projection, model_matrix);
   
        quad->vertices[0].position = HMM_MulM4V4(mvp_matrix, top_left).XYZ;
        quad->vertices[1].position = HMM_MulM4V4(mvp_matrix, top_right).XYZ;
        quad->vertices[2].position = HMM_MulM4V4(mvp_matrix, bottom_right).XYZ;
        quad->vertices[3].position = HMM_MulM4V4(mvp_matrix, bottom_left).XYZ;
    
        quad->vertices[0].colour = RED;
        quad->vertices[1].colour = GREEN;
        quad->vertices[2].colour = BLUE;
        quad->vertices[3].colour = RED;
    }
}

void draw_quad(HMM_Vec3 position, HMM_Vec2 size) {
    DrawCommand *command = &state.renderer.commands[state.renderer.command_count];
    state.renderer.command_count++;

    command->position = position;
    command->size = size;
}

HMM_Mat4 get_view_matrix() {
    return HMM_LookAt_LH(
        state.camera.position, 
        {state.camera.position.X, state.camera.position.Y, state.camera.position.Z + 1}, 
        {0, 1, 0}
    );
}

HMM_Mat4 get_projection_matrix() {
    f32 aspect_ratio = (f32)state.width / (f32)state.height;
    f32 size = state.camera.orthographic_size;

    return HMM_Orthographic_LH_NO(
        -size * aspect_ratio, 
        size * aspect_ratio, 
        -size, 
        size, 
        state.camera.near_plane, 
        state.camera.far_plane 
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
