#ifndef ENGINE_CPP
#define ENGINE_CPP

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libs/libs.h"
#include "ack.cpp"
#include "math.cpp"

#define REPORT_GL_ERRORS 0

#define GL_CALL(x) \
    clear_gl_errors(); \
    x;\
    Assert(check_gl_errors());

#define GL_VERIFY() Assert(check_gl_errors());

/////////////////////////////////////////////////////////////////////////////
//////////////////////////////// @window ////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
struct Window {
    string title;
    v2i logical_size;
    v2i frame_buffer_size;

    GLFWwindow *glfw_window;

    bool vsync;
    bool mouse_captured;
};

Window g_window = {};

bool window_init(string title, i32 width, i32 height);
void window_set_mouse_captured(bool captured);
void window_poll_inputs();
void window_swap_buffers();
void window_toggle_vsync();
bool window_wants_to_close();
void window_close();

void glfw_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
void glfw_mouse_move_callback(GLFWwindow* window, f64 x, f64 y);
void glfw_mouse_button_callback(GLFWwindow* window, i32 button, i32 action, i32 mods);
void glfw_error_callback(int error_code, const char* description);
void opengl_error_callback(GLenum source, GLenum type, u32 id, GLenum severity, i32 length, const char *message, const void *user_param);

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

bool window_init(string title, i32 width, i32 height) {
    if (glfwInit() == 0) {
        Err("Failed to init glfw");
        return false;
    }

    g_window = Window {
        .title = title,
        .logical_size = v2i{width, height},
        .vsync = true,
        .mouse_captured = false,
    };

    // glfwWindowHint(GLFW_MAXIMIZED, GL_TRUE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#if REPORT_GL_ERRORS
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
#endif

    g_window.glfw_window = glfwCreateWindow(g_window.logical_size.x, g_window.logical_size.y, g_window.title.c(), NULL, NULL);
    if (g_window.glfw_window == NULL) {
        Err("Failed to create window");
        return false;
    }

    glfwGetFramebufferSize(g_window.glfw_window, &g_window.frame_buffer_size.x, &g_window.frame_buffer_size.y);
    glfwGetWindowSize(g_window.glfw_window, &g_window.logical_size.x, &g_window.logical_size.y);

    glfwMakeContextCurrent(g_window.glfw_window);
    glfwSetWindowUserPointer(g_window.glfw_window, &g_window);

    glfwSwapInterval(g_window.vsync);
    window_set_mouse_captured(g_window.mouse_captured);

    glfwSetErrorCallback(glfw_error_callback);
    glfwSetKeyCallback(g_window.glfw_window, glfw_key_callback);
    glfwSetCursorPosCallback(g_window.glfw_window, glfw_mouse_move_callback);
    glfwSetMouseButtonCallback(g_window.glfw_window, glfw_mouse_button_callback);

    return true;
}

void window_set_mouse_captured(bool captured) {
    g_window.mouse_captured = captured;

    i32 glfw_mode = captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL;
    glfwSetInputMode(g_window.glfw_window, GLFW_CURSOR, glfw_mode);
}

void window_poll_inputs() {
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
    
    for (int i = 0; i < KEYS.items.size(); i++) {
        if (KEYS[i] == InputState::DOWN) {
            KEYS[i] = InputState::PRESSED;
        }
    }

    for (int i = 0; i < MOUSE.buttons.items.size(); i++) {
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

void window_swap_buffers() {
    glfwSwapBuffers(g_window.glfw_window);
}

void window_toggle_vsync() {
    g_window.vsync = !g_window.vsync;

    if (g_window.vsync) {
        glfwSwapInterval(1);
    } else {
        glfwSwapInterval(0); 
    }
}

bool window_wants_to_close() {
    return glfwWindowShouldClose(g_window.glfw_window) == 1;
}

void window_close() {
    glfwSetWindowShouldClose(g_window.glfw_window, GLFW_TRUE);
    glfwTerminate();
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
        ((f32) -y) + win_ptr->logical_size.y,
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

void glfw_error_callback(int error_code, const char* description) {
    printf("glfw error: [%d]: %s\n", error_code, description);
}

void opengl_error_callback(GLenum source, GLenum type, u32 id, GLenum severity, i32 length, const char *message, const void *user_param) {
    printf("OpenGL error: %s\n", message);
}

/////////////////////////////////////////////////////////////////////////////
//////////////////////////////// @renderer //////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
#define MAX_QUADS 200000
#define MAX_TEXTURES 256

struct Vertex {
    v4 position;
    v4 colour;
    v2 uv;
    i32 draw_type;
};

struct Quad {
    Vertex vertices[4];
};

// @camera
struct Camera {
    v3 position;
    v3 rotation;
    v3 target;
    f32 near_plane;
    f32 far_plane;
    f32 orthographic_size;
};

m4 camera_view_matrix(Camera camera);
m4 camera_projection_matrix(Camera camera, f32 aspect);
v3 camera_forward_direction(Camera camera);
v3 camera_right_direction(Camera camera);
v3 camera_up_direction(Camera camera);
v3 camera_forward_direction(v3 rotation);
v3 camera_right_direction(v3 rotation);
v3 camera_up_direction(v3 rotation);

enum class DrawType {
    RECTANGLE   = 0,
    CIRCLE      = 1,
    TEXTURE     = 2,
    TEXT        = 3,
};

// @texture
struct Texture {
    i64 width;
    i64 height;
    v2 uvs[4]; // [top_left, top_right, bottom_right, bottom_left]
    u8 *data;
};

f32 texture_aspect_ratio(Texture *texture);

struct Atlas {
    i64 width;
    i64 height;
    u8 *data;
};

struct Font {
    i64 width;
    i64 height;
    StackArray<stbtt_bakedchar, 96> characters;
    u8 *bitmap_data;
};

// @framebuffer
struct FrameBuffer {
    u32 id;
    i64 width;
    i64 height;

    u32 albedo_attachment;
    u32 depth_attachment;
};

bool frame_buffer_init(FrameBuffer *frame_buffer);

// @shader
struct Shader {
    string debug_name;
    u32 id;
};

bool shader_init(Shader *shader, string debug_name, string vertex_shader_path, string fragment_shader_path);
void shader_use(Shader shader);
void shader_assign_texture_slot(Shader shader, string texture_name, i32 slot);
void shader_set_uniform_f32(Shader shader, string name, f32 value);
void shader_set_uniform_m4(Shader shader, string name, m4 *matrix);
void shader_set_uniform_v2(Shader shader, string name, v2 vector);
void shader_set_uniform_v3(Shader shader, string name, v3 vector);
void shader_set_uniform_v4(Shader shader, string name, v4 vector);

// @renderer
struct Renderer {
    v4 clear_colour;

    FixedArray<Quad> quads;

    m4 view_matrix;
    m4 projection_matrix;

    StackArray<Texture, MAX_TEXTURES> textures;

    Atlas atlas;

    Font font;

    FrameBuffer gbuffer;

    u32 vertex_array_id;
    u32 vertex_buffer_id;
    u32 index_buffer_id;

    Shader default_shader;
    Shader post_processing_shader;

    u32 atlas_texture_id;
    u32 font_texture_id;
};

Renderer g_renderer = {};

bool renderer_init(Window *window);
bool renderer_load_shaders();
void renderer_delete_shaders();
Texture *renderer_load_texture(string path);
bool renderer_build_atlas();
u32 renderer_upload_texture_to_gpu(i32 width, i32 height, u8 *data);
u32 renderer_upload_font_to_gpu(i32 width, i32 height, u8 *data);
bool renderer_load_font(string path, i64 width, i64 height, f32 pixel_height);
void renderer_new_frame(Window *window, Camera camera);
void renderer_draw_frame(Window *window);
void renderer_new_imgui_frame();
void renderer_draw_imgui_frame();
void renderer_draw_rectangle(v3 position, v2 size, v4 color);
void renderer_draw_circle(v3 position, f32 radius, v4 color);
void renderer_draw_texture(Texture *texture, v3 position, v2 size, f32 rotation, v4 color);
void renderer_draw_text(string text, v3 position, f32 font_size, v4 color);
Quad *renderer_push_quad(v3 position, v2 size, v3 rotation, v4 color, v2 uvs[4], DrawType draw_type);
Quad *renderer_push_screen_quad(v4 color);

v2 screen_position_to_world_position(Window *window, v2 screen_position, Camera camera);
v2 screen_position_to_ndc(Window *window, v2 screen_position);

v4 alpha(v4 base, f32 alpha);
v4 brightness(v4 base, f32 brightness);

void clear_gl_errors();
bool check_gl_errors();

v4 WHITE            = {1, 1, 1, 1};
v4 BLACK            = {0, 0, 0, 1};

v4 RED              = {1, 0, 0, 1};
v4 GREEN            = {0, 1, 0, 1};
v4 BLUE             = {0, 0, 1, 1};

v4 ORANGE           = {1, 0.64, 0.1, 1};
v4 CORNFLOUR_BLUE   = {0.35, 0.80, 0.80, 1};
v4 SUN_YELLOW       = {1, 0.95, 0.5, 1};

// @camera
m4 camera_view_matrix(Camera camera) {
    v3 target = camera.position + camera_forward_direction(camera);

    // FIXME: having the up always be y = 1 is probably wrong - 04/06/25
    m4 view_matrix = HMM_LookAt_LH(
        camera.position, 
        target, 
        {0, 1, 0}
    );

    return view_matrix;
}

m4 camera_projection_matrix(Camera camera, f32 aspect) {
    return HMM_Orthographic_LH_NO(
        -camera.orthographic_size * aspect,  // left
         camera.orthographic_size * aspect,  // right
        -camera.orthographic_size,           // bottom
         camera.orthographic_size,           // top
         camera.near_plane, 
         camera.far_plane 
    );
}

v3 camera_forward_direction(Camera camera) {
    // pitch    - x
    // yaw      - y
    // roll     - z
    v3 direction {
        .x = sin(camera.rotation.y * HMM_DegToRad) * cos(camera.rotation.x * HMM_DegToRad),
        .y = sin(camera.rotation.x * HMM_DegToRad),
        .z = cos(camera.rotation.y * HMM_DegToRad) * cos(camera.rotation.x * HMM_DegToRad)
    };

    return norm(direction);
}

v3 camera_right_direction(Camera camera) {
    return HMM_Cross(camera_up_direction(camera), camera_forward_direction(camera));
}

v3 camera_up_direction(Camera camera) {
    return {0, 1, 0};
}

v3 camera_forward_direction(v3 rotation) {
    // pitch    - x
    // yaw      - y
    // roll     - z
    v3 direction {
        .x = sin(rotation.y * HMM_DegToRad) * cos(rotation.x * HMM_DegToRad),
        .y = sin(rotation.x * HMM_DegToRad),
        .z = cos(rotation.y * HMM_DegToRad) * cos(rotation.x * HMM_DegToRad)
    };

    return norm(direction);
}

v3 camera_right_direction(v3 rotation) {
    return HMM_Cross(camera_up_direction(rotation), camera_forward_direction(rotation));
}

v3 camera_up_direction(v3 rotation) {
    return {0, 1, 0};
}

// @texture
f32 texture_aspect_ratio(Texture *texture) {
    return (f32) texture->width / (f32) texture->height;
}

// @framebuffer
bool frame_buffer_init(FrameBuffer *frame_buffer) {
    glCreateFramebuffers(1, &frame_buffer->id);
    glBindFramebuffer(GL_FRAMEBUFFER, frame_buffer->id);

    StackArray<GLenum, 5> draw_buffers = {};

    {
        glCreateTextures(GL_TEXTURE_2D, 1, &frame_buffer->albedo_attachment);
        glBindTexture(GL_TEXTURE_2D, frame_buffer->albedo_attachment);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, frame_buffer->width, frame_buffer->height, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); 

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, frame_buffer->albedo_attachment, 0);
        append(&draw_buffers, (GLenum) GL_COLOR_ATTACHMENT0);
    }

    {
        glCreateTextures(GL_TEXTURE_2D, 1, &frame_buffer->depth_attachment);
        glBindTexture(GL_TEXTURE_2D, frame_buffer->depth_attachment);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, frame_buffer->width, frame_buffer->height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, frame_buffer->depth_attachment, 0);
    }

    // when using more then one colour attachment, need to set all colour buffers the
    // frame buffer can write too, if not the normal buffer will not be write too
    glDrawBuffers(draw_buffers.len, draw_buffers.items.items);

    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        Err("error when creating frame buffer, was not complete");
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

// @shader
bool shader_init(Shader *shader, string debug_name, string vertex_shader_path, string fragment_shader_path) {
    const i64 buffer_size = 640;
    i32 compile_status = 0;
    i32 link_status = 0;
    char error_buffer[buffer_size];
    
    string vertex_shader_source = read_entire_file(vertex_shader_path);
    if (vertex_shader_source.len == 0) {
        Errf("{}: failed to load vertex shader file", debug_name);
        return false;
    }

    string fragment_shader_source = read_entire_file(fragment_shader_path);
    if (fragment_shader_source.len == 0) {
        Errf("{}: failed to load default fragment shader file", debug_name);
        return false;
    }

    u32 vertex_shader = glCreateShader(GL_VERTEX_SHADER);

    glShaderSource(vertex_shader, 1, (char **) &vertex_shader_source.ptr, NULL);
    glCompileShader(vertex_shader);

    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &compile_status);
    if (compile_status == 0) {
        glGetShaderInfoLog(vertex_shader, buffer_size, nullptr, &error_buffer[0]);
        Errf("{}: failed to compile vertex shader: {}", debug_name, error_buffer);
        return false;
    }

    u32 fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

    glShaderSource(fragment_shader, 1, (char**) &fragment_shader_source.ptr, NULL);
    glCompileShader(fragment_shader);

    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &compile_status);
    if (compile_status == 0) {
        glGetShaderInfoLog(fragment_shader, buffer_size, nullptr, &error_buffer[0]);
        Errf("{}: failed to compile fragment shader: {}", debug_name, error_buffer);
        return false;
    }

    u32 shader_program = glCreateProgram();

    glAttachShader(shader_program, vertex_shader);
    glAttachShader(shader_program, fragment_shader);
    glLinkProgram(shader_program); 

    glGetProgramiv(shader_program, GL_LINK_STATUS, &link_status);

    if (link_status == 0) {
        glGetProgramInfoLog(shader_program, buffer_size, nullptr, &error_buffer[0]);
        Errf("{}: failed to link shader program: {}", debug_name, error_buffer);
        return false;
    }
 
    shader->id = shader_program; 
    shader->debug_name = debug_name;

    Logf("Compiled and linked {}", debug_name);

    return true;
}

void shader_use(Shader shader) {
    glUseProgram(shader.id);
}

void shader_assign_texture_slot(Shader shader, string texture_name, i32 slot) {
    glUseProgram(shader.id);
    glUniform1i(glGetUniformLocation(shader.id, texture_name.c()), slot);
    glUseProgram(0);
}

void shader_set_uniform_f32(Shader shader, string name, f32 value) {
    glUniform1f(glGetUniformLocation(shader.id, name.c()), value);
}

void shader_set_uniform_m4(Shader shader, string name, m4 *matrix) {
    glUniformMatrix4fv(
        glGetUniformLocation(shader.id, name.c()),
        1,
        false,
        (f32 *) &matrix->Columns[0]
    );
}

void shader_set_uniform_v2(Shader shader, string name, v2 vector) {
    glUniform2f(
        glGetUniformLocation(shader.id, name.c()),
        vector.x, vector.y
    );
}

void shader_set_uniform_v3(Shader shader, string name, v3 vector) {
    glUniform3f(
        glGetUniformLocation(shader.id, name.c()),
        vector.x, vector.y, vector.z 
    );
}

void shader_set_uniform_v4(Shader shader, string name, v4 vector) {
    glUniform4f(
        glGetUniformLocation(shader.id, name.c()),
        vector.x, vector.y, vector.z, vector.w
    );
}

// @renderer
bool renderer_init(Window *window) {
    g_renderer.clear_colour = v4{0.2, 0.35, 0.45, 1};
    g_renderer.quads = fixed_array_create<Quad>(MAX_QUADS);

    { // init opengl
        GLenum result = glewInit();
        if (result != GLEW_OK) {
            return false;
        }

#if REPORT_GL_ERRORS
        glDebugMessageCallback(opengl_error_callback, NULL);
#endif

        // alpha blend settings
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // depth buffer settings
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        // enable back face culling
        glEnable(GL_CULL_FACE);

        glClearColor(
            g_renderer.clear_colour.r,
            g_renderer.clear_colour.g,
            g_renderer.clear_colour.b,
            g_renderer.clear_colour.a
        );
    }

    bool ok = renderer_load_shaders();
    if (!ok) {
        printf("Error when loading and compiling shaders\n");
        return false;
    }

    { // init imgui
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
    
        // ImGui::StyleColorsLight();
        ImGui::StyleColorsDark();
    
        ImGuiIO& io = ImGui::GetIO();
    
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    
        ImGui_ImplGlfw_InitForOpenGL(window->glfw_window, true);
        ImGui_ImplOpenGL3_Init("#version 460");
    } 

    { // vertex array
        u32 vertex_array;
        glGenVertexArrays(1, &vertex_array);
        glBindVertexArray(vertex_array);

        g_renderer.vertex_array_id = vertex_array;
    }

    { // vertex buffer
        u32 vertex_buffer;
        glGenBuffers(1, &vertex_buffer);
        glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(Quad) * MAX_QUADS, g_renderer.quads.slice.ptr, GL_DYNAMIC_DRAW);

        g_renderer.vertex_buffer_id = vertex_buffer;
    }

    { // index buffer
        const i64 index_buffer_length = MAX_QUADS * 6;
        slice<u32> indices = slice_create_malloc<u32>(index_buffer_length);

        i64 i = 0;
        while (i < index_buffer_length) {
            // updated order of indices to be CCW as that is the default
            // for opengl and we want to use back face culling now that
            // we are rendering in 3d
            // 31/05/25

            // vertex offset pattern to draw a quad
            // { 0, 1, 2,  0, 2, 3 } -> CW winding 
            // { 0, 2, 1,  0, 3, 2 } -> CCW winding
            indices[i + 0] = ((i/6)*4 + 0);
            indices[i + 1] = ((i/6)*4 + 2);
            indices[i + 2] = ((i/6)*4 + 1);
            indices[i + 3] = ((i/6)*4 + 0);
            indices[i + 4] = ((i/6)*4 + 3);
            indices[i + 5] = ((i/6)*4 + 2);
            i += 6;
        }

        u32 index_buffer;
        glGenBuffers(1, &index_buffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(u32) * index_buffer_length, indices.ptr, GL_STATIC_DRAW);

        g_renderer.index_buffer_id = index_buffer;

        slice_free(indices);
    }

    { // vertex attributes
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *) offsetof(Vertex, position));   // position
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *) offsetof(Vertex, colour));     // colour
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *) offsetof(Vertex, uv));         // uv
        glVertexAttribIPointer(3, 1, GL_INT, sizeof(Vertex), (void *) offsetof(Vertex, draw_type));             // draw_type

        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glEnableVertexAttribArray(2);
        glEnableVertexAttribArray(3);

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

    { // init frame buffers
         g_renderer.gbuffer = FrameBuffer {
            .width =  window->frame_buffer_size.x,
            .height =  window->frame_buffer_size.y
        };
    
        ok = frame_buffer_init(&g_renderer.gbuffer);
        if (!ok) {
            Err("failed to init gbuffer frame buffer");
            return false;
        }
    }

    return true;
}

bool renderer_load_shaders() {
    bool ok = shader_init(&g_renderer.default_shader, "Default shader", "resources/shaders/default_vertex.shader", "resources/shaders/default_fragment.shader");
    if (!ok) {
        Err("Error when creating default shader program");
        return false;
    }

    shader_assign_texture_slot(g_renderer.default_shader, "atlas_texture", 0);
    shader_assign_texture_slot(g_renderer.default_shader, "font_texture", 1);

    ok = shader_init(&g_renderer.post_processing_shader, "Post processing shader", "resources/shaders/default_vertex.shader", "resources/shaders/post_processing_fragment.shader");
    if (!ok) {
        Err("Error when creating post processing shader program");
        return false;
    }

    shader_assign_texture_slot(g_renderer.post_processing_shader, "scene_texture", 0);

    return true;
}

void renderer_delete_shaders() {
    glDeleteProgram(g_renderer.default_shader.id);
    glDeleteProgram(g_renderer.post_processing_shader.id);
}

Texture *renderer_load_texture(string path) {
    i32 width       = 0;
    i32 height      = 0;
    i32 channels    = 0;
    u8 *image_data  = nullptr;

    stbi_set_flip_vertically_on_load(true);

    image_data = stbi_load(path.c(), &width, &height, &channels, 4);
    if (!image_data) {
        Errf("Failed to load texture: {}", path);
        return NULL;
    }

    Logf("Loaded texture with path {} [{}x{}] {} bytes", path, width, height, width * height * channels);

    Texture *texture = push(&g_renderer.textures);

    *texture = Texture {
        .width = width,
        .height = height,
        .data = image_data,
    };

    return texture;
}

bool renderer_build_atlas() {
    const i64 ATLAS_WIDTH     = 480;
    const i64 ATLAS_HEIGHT    = 480;
    const i64 BYTES_PER_PIXEL = 4;
    const i64 CHANNELS        = 4;
    const i64 ATLAS_BYTE_SIZE = ATLAS_WIDTH * ATLAS_HEIGHT * BYTES_PER_PIXEL;

    stbrp_context rp_context;
    i64 rect_count    = g_renderer.textures.len;
    u8 *atlas_data    = (u8 *) malloc(ATLAS_BYTE_SIZE);
    stbrp_rect *rects = (stbrp_rect *) malloc(sizeof(stbrp_rect) * rect_count);
    stbrp_node *nodes = (stbrp_node *) malloc(sizeof(stbrp_node) * ATLAS_WIDTH);

    // fill in default atlas data
    i64 i = 0;
    while (i < ATLAS_BYTE_SIZE) {
        atlas_data[i]       = 255;  // r
        atlas_data[i + 1]   = 0;    // g
        atlas_data[i + 2]   = 255;  // b
        atlas_data[i + 3]   = 255;  // a
 
        i += 4;
    }

    stbrp_init_target(&rp_context, ATLAS_WIDTH, ATLAS_HEIGHT, nodes, ATLAS_WIDTH);

    for(i64 i = 0; i < g_renderer.textures.len; i++) {
        Texture *texture = &g_renderer.textures[i];
        rects[i] = stbrp_rect {
            .id = (i32) i,
            .w  = (i32) texture->width,
            .h  = (i32) texture->height,
        };
    }

    i64 status = stbrp_pack_rects(&rp_context, rects, rect_count);
    if (status == 0) {
        printf("error packing textures into atlas\n");
        return false;
    }

    for(int i = 0; i < rect_count; i++) {
        stbrp_rect *rect = &rects[i];
        Texture *texture = &g_renderer.textures[rect->id];

        f32 bottom_y_uv = (f32) rect->y             / (f32) ATLAS_HEIGHT;
        f32 top_y_uv    = (f32) (rect->y + rect->h) / (f32) ATLAS_HEIGHT;
        f32 left_x_uv   = (f32) rect->x             / (f32) ATLAS_WIDTH;
        f32 right_x_uv  = (f32) (rect->x + rect->w) / (f32) ATLAS_WIDTH;

        texture->uvs[0] = {left_x_uv, top_y_uv};
        texture->uvs[1] = {right_x_uv, top_y_uv};
        texture->uvs[2] = {right_x_uv, bottom_y_uv};
        texture->uvs[3] = {left_x_uv, bottom_y_uv};

        for (i64 row = 0; row < rect->h; row++) {
            u8 *source_row = texture->data + (row * rect->w * BYTES_PER_PIXEL);
            u8 *dest_row = atlas_data + (((rect->y + row) * ATLAS_WIDTH + rect->x) * BYTES_PER_PIXEL);
            memcpy(dest_row, source_row, rect->w * BYTES_PER_PIXEL);
        }
    }

    u32 texture_id = renderer_upload_texture_to_gpu(ATLAS_WIDTH, ATLAS_HEIGHT, atlas_data);
    if (texture_id == 0) {
        printf("error sending atlas texture GPU\n");
        return false;
    }

    g_renderer.atlas_texture_id = texture_id;

#ifdef DEBUG
    stbi_flip_vertically_on_write(true);
    i64 atlas_write_status = stbi_write_png("build/atlas.png", ATLAS_WIDTH, ATLAS_HEIGHT, 4, atlas_data, ATLAS_WIDTH * BYTES_PER_PIXEL);
    if (atlas_write_status == 0) {
        printf("error writing atlas to build folder\n");
        return false;
    }
#endif

    free(atlas_data);
    free(rects);
    free(nodes);

    return true; 
}

u32 renderer_upload_texture_to_gpu(i32 width, i32 height, u8 *data) {
    u32 texture_id = 0;
    glGenTextures(1, &texture_id);

    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // border param might fix texture bleeding
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

    return texture_id;
}

u32 renderer_upload_font_to_gpu(i32 width, i32 height, u8 *data) {
    u32 texture_id = 0;
    glGenTextures(1, &texture_id);

    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // border param might fix texture bleeding
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, data);

    return texture_id;
}

bool renderer_load_font(string path, i64 width, i64 height, f32 pixel_height) {
    Font font = Font{
        .width = width,
        .height = height,
        .characters = {},
        .bitmap_data = (u8 *) malloc(width * height),
    };

    string font_data = read_entire_file(path);
    if (font_data.len == 0) {
        printf("failed to load font \"%s\"\n", path.c());
        return false;
    }

    i64 bake_result = stbtt_BakeFontBitmap((const u8*)font_data.c(), 0, pixel_height, font.bitmap_data, font.width, font.height, 32, font.characters.items.size(), font.characters.items.items);
    if (bake_result <= 0) {
        printf("failed to bake font \"%s\"\n", path.c());
        return false;
    }


#ifdef DEBUG
    { // write debug image out
        stbi_flip_vertically_on_write(false);

        i64 write_result = stbi_write_png("build/font.png", font.width, font.height, 1, font.bitmap_data, font.width);
        if (write_result == 0) {
            printf("error writing font to build folder\n");
            return false;
        }
    }
#endif

    g_renderer.font_texture_id = renderer_upload_font_to_gpu(font.width, font.height, font.bitmap_data);
    assert(g_renderer.font_texture_id != 0);

    g_renderer.font = font;

    return true;
}

void renderer_new_frame(Window *window, Camera camera) {
    reset(&g_renderer.quads);

    g_renderer.view_matrix = camera_view_matrix(camera);
    g_renderer.projection_matrix = camera_projection_matrix(camera, (f32) window->logical_size.x / (f32) window->logical_size.y);

    glClearColor(
        g_renderer.clear_colour.r,
        g_renderer.clear_colour.g,
        g_renderer.clear_colour.b,
        g_renderer.clear_colour.a
    );

    renderer_new_imgui_frame();
}

void renderer_draw_frame(Window *window) {
    { // imediete mode quads
        glBindFramebuffer(GL_FRAMEBUFFER, g_renderer.gbuffer.id);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glBindBuffer(GL_ARRAY_BUFFER, g_renderer.vertex_buffer_id);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(Quad) * g_renderer.quads.len, g_renderer.quads.slice.ptr);
    
        glBindVertexArray(g_renderer.vertex_array_id);

        shader_use(g_renderer.default_shader);
    
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, g_renderer.atlas_texture_id);
    
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, g_renderer.font_texture_id);
    
        glDrawElements(GL_TRIANGLES, 6 * g_renderer.quads.len, GL_UNSIGNED_INT, 0);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    { // post processing
        reset(&g_renderer.quads);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glViewport(0, 0, window->frame_buffer_size.x, window->frame_buffer_size.y);

        Quad *quad = renderer_push_screen_quad(WHITE);
    
        glBindBuffer(GL_ARRAY_BUFFER, g_renderer.vertex_buffer_id);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(Quad) * g_renderer.quads.len, g_renderer.quads.slice.ptr);
        glBindVertexArray(g_renderer.vertex_array_id);
 
        shader_use(g_renderer.post_processing_shader);
 
        // set the input texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, g_renderer.gbuffer.albedo_attachment);

        glDrawElements(GL_TRIANGLES, 6 * g_renderer.quads.len, GL_UNSIGNED_INT, 0);
    }

    renderer_draw_imgui_frame();
}

void renderer_new_imgui_frame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame(); 
}

void renderer_draw_imgui_frame() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    GLFWwindow *current = glfwGetCurrentContext();
    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault();
    glfwMakeContextCurrent(current);
}

void renderer_draw_rectangle(v3 position, v2 size, v4 color) {
    v2 uvs[4] = {
        {0, 1},
        {1, 1},
        {1, 0},
        {0, 0},
    };

    renderer_push_quad(position, size, {}, color, uvs, DrawType::RECTANGLE);
}

void renderer_draw_circle(v3 position, f32 radius, v4 color) {
    v2 size = {radius * 2, radius * 2};

    v2 uvs[4] = {
        {0, 1},
        {1, 1},
        {1, 0},
        {0, 0},
    };

    renderer_push_quad(position, size, {}, color, uvs, DrawType::CIRCLE);
}

void renderer_draw_texture(Texture *texture, v3 position, v2 size, f32 rotation, v4 color) {
    renderer_push_quad(position, size, {0, 0, rotation}, color, texture->uvs, DrawType::TEXTURE);
}

void renderer_draw_text(string text, v3 position, f32 font_size, v4 color) {
    if (text.len == 0) {
        return;
    }

    struct Glyph {
        v2 position;
        v2 size;
        v2 uvs[4];
    };

    slice<Glyph> glyphs = slice_create_malloc<Glyph>(text.len);

    f32 total_text_width = 0;
    f32 text_height = 0;

    for (i64 i = 0; i < text.len; i++) {
        char c = text[i];

        f32 advanced_x = 0;
        f32 advanced_y = 0;
        stbtt_aligned_quad aligned_quad = {};

        // this is the the data for the aligned_quad we're given, with y+ going down
        //	   x0, y0       x1, y0
        //     s0, t0       s1, t0
        //	    o tl        o tr
                        
                        
        //     x0, y1      x1, y1
        //     s0, t1      s1, t1
        //	    o bl        o br
        // 
        // x, and y and expected vertex positions
        // s and t are texture uv position
 
        stbtt_GetBakedQuad(g_renderer.font.characters.items.items, g_renderer.font.width, g_renderer.font.height, c - 32, &advanced_x, &advanced_y, &aligned_quad, false);

        f32 bottom_y = -aligned_quad.y1;
        f32 top_y = -aligned_quad.y0;

        f32 height = top_y - bottom_y;
        f32 width = aligned_quad.x1 - aligned_quad.x0;

        if (height > text_height) {
            text_height = height;
        }

        v2 top_left_uv     = v2{aligned_quad.s0, aligned_quad.t0};
        v2 top_right_uv    = v2{aligned_quad.s1, aligned_quad.t0};
        v2 bottom_right_uv = v2{aligned_quad.s1, aligned_quad.t1};
        v2 bottom_left_uv  = v2{aligned_quad.s0, aligned_quad.t1};

        glyphs[i] = {
            .position = {total_text_width, bottom_y},
            .size = {width, height},
            .uvs = {top_left_uv, top_right_uv, bottom_right_uv, bottom_left_uv}
        };

        // if the character is not the last then add the advanced x to the total width
        // because this includes the with of the character and also the kerning gap added
        // for the next character, if it is the last one then just take the width and have
        // no extra gap at the end - 20/01/25
        if (i < text.len - 1) {
            total_text_width += advanced_x;
        } else {
            total_text_width += width;
        }
    }

    v2 pivot_point_translation = {};
    f32 scale = font_size / text_height;

    for (i64 i = 0; i < glyphs.len; i++) {
        Glyph *glyph = &glyphs[i];

        v2 scaled_position = glyph->position * scale;
        v2 scaled_size = glyph->size * scale;
        v2 translated_position = scaled_position + pivot_point_translation + position.xy;

        // quad needs position to be centre of quad so just convert that here
        v2 quad_centered_position = translated_position + (scaled_size * 0.5f);

        renderer_push_quad(v3{quad_centered_position.x, quad_centered_position.y, position.z}, scaled_size, {}, color, glyph->uvs, DrawType::TEXT);
   }

    slice_free(glyphs);
}

Quad *renderer_push_quad(v3 position, v2 size, v3 rotation, v4 color, v2 uvs[4], DrawType draw_type) {
    const v4 top_left      = {-0.5,   0.5, 0, 1};
    const v4 top_right     = { 0.5,   0.5, 0, 1};
    const v4 bottom_right  = { 0.5,  -0.5, 0, 1};
    const v4 bottom_left   = {-0.5,  -0.5, 0, 1};

    // After looking at how unity does their rotations I am doing the oppisite.
    // In unity, when looking down the negative of an axis towards origin, 
    // increasing the rotation of that axis means it rotates to the right. 
    // For me it was when looking in the positive of that axis.
    //
    // After trying it I think I rather my approach so I am keeping it, maybe
    // this will change. If in the future I am confused, always remember that
    // when looking from the origin, down an axis, a positive rotation means
    // it rotates to the right
    // - 31/05/25

    m4 model_matrix = HMM_M4D(1.0f);
    model_matrix = HMM_MulM4(model_matrix, HMM_Translate(position));
    model_matrix = HMM_MulM4(model_matrix, HMM_Rotate_LH(rotation.x * HMM_DegToRad, {1, 0, 0}));
    model_matrix = HMM_MulM4(model_matrix, HMM_Rotate_LH(rotation.y * HMM_DegToRad, {0, 1, 0}));
    model_matrix = HMM_MulM4(model_matrix, HMM_Rotate_LH(rotation.z * HMM_DegToRad, {0, 0, 1}));
    model_matrix = HMM_MulM4(model_matrix, HMM_Scale({size.x, size.y, 1}));
              
    m4 mvp_matrix = HMM_MulM4(HMM_MulM4(g_renderer.projection_matrix, g_renderer.view_matrix), model_matrix);

    Quad *quad = push(&g_renderer.quads);

    quad->vertices[0].position = HMM_MulM4V4(mvp_matrix, top_left);
    quad->vertices[1].position = HMM_MulM4V4(mvp_matrix, top_right);
    quad->vertices[2].position = HMM_MulM4V4(mvp_matrix, bottom_right);
    quad->vertices[3].position = HMM_MulM4V4(mvp_matrix, bottom_left);
                
    quad->vertices[0].colour = color;
    quad->vertices[1].colour = color;
    quad->vertices[2].colour = color;
    quad->vertices[3].colour = color;

    quad->vertices[0].uv = uvs[0];
    quad->vertices[1].uv = uvs[1];
    quad->vertices[2].uv = uvs[2];
    quad->vertices[3].uv = uvs[3];

    quad->vertices[0].draw_type = (i32) draw_type;
    quad->vertices[1].draw_type = (i32) draw_type;
    quad->vertices[2].draw_type = (i32) draw_type;
    quad->vertices[3].draw_type = (i32) draw_type;

    return quad;
}

Quad *renderer_push_screen_quad(v4 color) {
    const v4 top_left      = {-1,   1, 0, 1};
    const v4 top_right     = { 1,   1, 0, 1};
    const v4 bottom_right  = { 1,  -1, 0, 1};
    const v4 bottom_left   = {-1,  -1, 0, 1};

    v2 uvs[4] = {
        {0, 1},
        {1, 1},
        {1, 0},
        {0, 0},
    };

    Quad *quad = push(&g_renderer.quads);

    quad->vertices[0].position = top_left;
    quad->vertices[1].position = top_right;
    quad->vertices[2].position = bottom_right;
    quad->vertices[3].position = bottom_left;
                
    quad->vertices[0].colour = color;
    quad->vertices[1].colour = color;
    quad->vertices[2].colour = color;
    quad->vertices[3].colour = color;

    quad->vertices[0].uv = uvs[0];
    quad->vertices[1].uv = uvs[1];
    quad->vertices[2].uv = uvs[2];
    quad->vertices[3].uv = uvs[3];

    quad->vertices[0].draw_type = 0;
    quad->vertices[1].draw_type = 0;
    quad->vertices[2].draw_type = 0;
    quad->vertices[3].draw_type = 0;

    return quad;
}

v2 screen_position_to_world_position(Window *window, v2 screen_position, Camera camera) {
    // pretty muched copied from odin engine,
    // haven't tweaked it because I am just happy 
    // it works. Maybe can find a way to not pass
    // in window or camera
    // - 28/05/25
    
    v2 ndc = screen_position_to_ndc(window, screen_position);
    f32 aspect_ratio = (f32) window->logical_size.x / (f32) window->logical_size.y;

    m4 inverse_vp = HMM_InvGeneralM4(camera_projection_matrix(camera, aspect_ratio) * camera_view_matrix(camera));

    v4 world_position = inverse_vp * v4{ndc.x, ndc.y, 0, 1};
    world_position /= world_position.w;

    return v2{world_position.x, world_position.y};
}

v2 screen_position_to_ndc(Window *window, v2 screen_position) {
    return {
        (screen_position.x / window->logical_size.x) * 2 - 1,
        (screen_position.y / window->logical_size.y) * 2 - 1,
    };
}

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

void clear_gl_errors() {
    while (glGetError() != GL_NO_ERROR);
}

bool check_gl_errors() {
    GLenum error = glGetError();
    bool no_errors = true;

    while (error != GL_NO_ERROR) {
        printf("[OpenGL ERROR]: %d\n", error);
        no_errors = false;
    }

    return no_errors;
}

#endif
