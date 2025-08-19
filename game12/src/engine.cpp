#ifndef ENGINE_CPP
#define ENGINE_CPP

#include "libs/libs.h"
#include "ack.cpp"
#include "math.cpp"

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <random>

////////////////////////////////////////////////
///             engine.cpp                   ///
////////////////////////////////////////////////
//
// IMPORTANT NOTES & ASSUMPTIONS
//
// Co-ordinate system is left handed:
// -X & +X = left & right
// -Y & +Y = down & up
// -Z & +Z = back & front
//
// Rotation should be defined as the following when looking forward along +Z:
// -X & +X = pitch down & pitch up
// -Y & +Y = yaw right  & yaw left (camera roation is oppisite)
// -Z & +Z = roll right & roll left
//
// All matrices are column major:
// - Also assumes column vectors so, matrix-vector multiplication is M * V 
//
// Model matrix transformation order:
// 1. Scale locally
// 2. Rotate locally 
// 3. Translate to world position
//     
//    Matrix order -> M = T * R * S
//
// Rotation is a 3 component euler angle and is applied to model matrix as:
// 1. Rotate about X
// 2. Rotate about Y
// 3. Rotate about Z
//
//    Matrix order -> R = Rz * Ry * Rx
//
// Screen positions are bottom left origin:
// tl = [0, h]
// tr = [w, h]
// br = [w, 0]
// bl = [0, 0]
//
// If a screen position requires a Z component, it is
// ignored or always assumed to be 0
//
// UV coords are bottom left origin:
// tl = [0, 1]
// tr = [1, 1]
// br = [1, 0]
// bl = [0, 0]
//
// Default quad vertex order is:
// 1. Top left
// 2. Top right
// 3. Bottom right
// 4. Bottom left
//
// Indices are in CCW winding order
//       b
//      /|
//     / |
//    /  |     indices = [a, c, b]
//   /   |
//  a----c
//

#define REPORT_GL_ERRORS 0

#define GL_CALL(x) \
    clear_gl_errors(); \
    x;\
    ASSERT(check_gl_errors());

#define GL_VERIFY() ASSERT(check_gl_errors());

/////////////////////////////////////////////////////////////////////////////
//////////////////////////////// @window ////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
struct Window {
    GLFWwindow *glfw_window;

    str title;
    v2i logical_size;
    v2i frame_buffer_size;
    bool vsync;
    bool mouse_captured;
};

// call window_init() and WIN()
Window *g_window = NULL;

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

Window *WIN() {
    ASSERT(g_window != NULL);
    return g_window;
}

bool window_init(str title, i32 width, i32 height);
void set_mouse_captured(Window *window, bool captured);
void set_window_title(Window *window, str title);
void poll_inputs();
void swap_buffers(Window *window);
void toggle_vsync(Window *window);
void glfw_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
void glfw_mouse_move_callback(GLFWwindow* window, f64 x, f64 y);
void glfw_mouse_button_callback(GLFWwindow* window, i32 button, i32 action, i32 mods);
void glfw_error_callback(int error_code, const char* description);

bool window_init(str title, i32 width, i32 height) {
    if (glfwInit() == 0) {
        log("Failed to init glfw");
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_MAXIMIZED, GL_FALSE); // start maximised and then inputed size is minimised size

    g_window = new Window {
        .glfw_window = NULL,
        .title = title,
        .logical_size = v2i{width, height},
        .vsync = true,
        .mouse_captured = false,
    };


#if REPORT_GL_ERRORS
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
#endif

    g_window->glfw_window = glfwCreateWindow(g_window->logical_size.x, g_window->logical_size.y, g_window->title.c(), NULL, NULL);
    if (g_window->glfw_window == NULL) {
        log("Failed to create window");
        return false;
    }

    glfwGetFramebufferSize(g_window->glfw_window, &g_window->frame_buffer_size.x, &g_window->frame_buffer_size.y);
    glfwGetWindowSize(g_window->glfw_window, &g_window->logical_size.x, &g_window->logical_size.y);

    glfwMakeContextCurrent(g_window->glfw_window);
    glfwSetWindowUserPointer(g_window->glfw_window, g_window);

    glfwSwapInterval(g_window->vsync);

    glfwSetErrorCallback(glfw_error_callback);
    glfwSetKeyCallback(g_window->glfw_window, glfw_key_callback);
    glfwSetCursorPosCallback(g_window->glfw_window, glfw_mouse_move_callback);
    glfwSetMouseButtonCallback(g_window->glfw_window, glfw_mouse_button_callback);

    return true;
}

void set_mouse_captured(Window *window, bool captured) {
    window->mouse_captured = captured;

    ImGuiIO& io = ImGui::GetIO();

    if (captured) {
        glfwSetInputMode(window->glfw_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        SET_BIT(io.ConfigFlags, ImGuiConfigFlags_NoMouse);
    } 
    else {
        glfwSetInputMode(window->glfw_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        UNSET_BIT(io.ConfigFlags, ImGuiConfigFlags_NoMouse);
    }
}

void set_window_title(Window *window, str title) {
    glfwSetWindowTitle(window->glfw_window, title.c());
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

/////////////////////////////////////////////////////////////////////////////
//////////////////////////////// @renderer //////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
#define MAX_UI_QUADS 100
#define MAX_QUADS 500
#define MAX_RENDER_COMMANDS 5000
#define MAX_MESHES 128
#define MAX_MODELS 128
#define MAX_LIGHTS 20
#define MAX_SPRITES 256
#define MAX_TEXTURES 256

v3 QUAD_POSITIONS[4] = {
    {-0.5,  0.5, 0}, // top left
    { 0.5,  0.5, 0}, // top right
    { 0.5, -0.5, 0}, // bottom right 
    {-0.5, -0.5, 0}, // bottom left
};

v2 QUAD_UVS[4] = {
    {0, 1},
    {1, 1},
    {1, 0},
    {0, 0},
};

v4 WHITE            = {1, 1, 1, 1};
v4 BLACK            = {0, 0, 0, 1};

v4 RED              = {1, 0, 0, 1};
v4 GREEN            = {0, 1, 0, 1};
v4 BLUE             = {0, 0, 1, 1};

v4 ORANGE           = {1, 0.64, 0.1, 1};
v4 CORNFLOUR_BLUE   = {0.35, 0.80, 0.80, 1};
v4 SUN_YELLOW       = {0.9, 0.9, 0.3, 1};
v4 HOT_PINK         = {1, 0, 0.8, 1};
v4 TURQUOISE        = {0.090f, 0.78, 0.78, 1};
v4 BEIGE            = {0.75, 0.58, 0.47, 1};

struct MeshVertex {
    v3 position;
    v3 normal;
    v2 uv;
};

struct Vertex {
    v3 position;
    v4 colour;
    v2 uv;
    i32 draw_type;
};

struct Quad {
    Vertex vertices[4];
};

struct Light {
    v2 position;
    f32 radius;
    v4 colour;
    f32 intensity;
};

enum class CameraMode {
    FIRST_PERSON,
    THIRD_PERSON
};

struct Camera {
    CameraMode mode;
    f32 fov;
    v3 position;
    v3 rotation;
    v3 target;
    f32 near_plane;
    f32 far_plane;
    f32 orthographic_size;
};

// call camera_init() and CAM()
Camera *g_camera = NULL;

enum class DrawType {
    RECTANGLE   = 0,
    CIRCLE      = 1,
    TEXTURE     = 2,
    TEXT        = 3,
};

enum class TextureType {
    SINGLE,
    ANIMATED,
};

// For Texture.uv:
// x and y of each corner of the texture in the atlas
// uv[0] == top left point
// uv[1] == top right point
// uv[2] == bottom right point
// uv[3] == bottom left point
struct Texture {
    TextureType type;
    i64 width;
    i64 height;
    v2 uvs[4];
    u8 *data;

    f32 animation_length;
    Slice<Texture> sub_textures;
};

struct RenderTexture {
    u32 id;
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


// @viewport
struct Viewport {
    bool focused;

    v2i size;
    v2 mouse;
};

struct FrameBuffer {
    u32 id;
    v2i size;

    u32 position_attachment;
    u32 normals_attachment;
    u32 albedo_attachment;
    u32 depth_attachment;
};

struct Shader {
    str debug_name;
    u32 id;
};

struct Mesh {
    Slice<MeshVertex> vertices;
    Slice<u32> indices;

    u32 vertex_array_id;
    u32 vertex_buffer_id;
    u32 index_buffer_id;
};

enum RenderCommandType {
    RC_MODEL,
    RC_QUAD,
    _RC_COUNT
};

struct MeshRenderCommand {
    v3 position;
    v3 scale;
    v3 rotation;
    Mesh *mesh;
    v4 colour;
};

struct RenderCommand {
    RenderCommandType type;

    union {
        MeshRenderCommand mesh;
    };
};

struct QuadBuffer {
    u32 vertex_array_id;
    u32 vertex_buffer_id;
    u32 index_buffer_id;

    FixedArray<Quad> quads;
};

struct Renderer {
    bool wireframe;

    v4 clear_colour;
    v3 ambient_light;
    v3 sun_colour;
    v3 sun_position;
    v3 shadow_colour;

    FixedArray<Mesh> meshes;
    FixedArray<RenderCommand> commands;
    StackArray<Texture, MAX_TEXTURES> textures;

    Mesh *cube_primitive;
    Mesh *sphere_primitive;
    Mesh *quad_primitive;

    m4 view_matrix;
    m4 projection_matrix;
    m4 projection_matrix_ortho;

    Texture *default_normal;

    Font font;

    QuadBuffer ui_quads;
    QuadBuffer screen_quad;

    FrameBuffer g_buffer;
    FrameBuffer lighting_buffer;
    FrameBuffer ui_buffer;

    u32 vertex_array_id;
    u32 vertex_buffer_id;
    u32 index_buffer_id;

    Shader default_shader;
    Shader lighting_shader;
    Shader mesh_shader;

    u32 atlas_texture_id;
    u32 font_texture_id;
    u32 noise_texture_id;
};

struct Ray {
    v3 origin;
    v3 direction;
};

// call renderer_init() and REN()
Renderer *g_renderer = NULL;

// Camera API
Camera camera_create(CameraMode mode, f32 fov, v3 position, f32 near_plane, f32 far_plane);

v3 get_forward_direction(Camera *camera);
v3 get_right_direction(Camera *camera);
v3 get_up_direction(Camera *camera);

v3 get_forward_direction(v3 rotation);
v3 get_right_direction(v3 rotation);
v3 get_up_direction(v3 rotation);

// Shader API
bool init_shader(Shader *shader, str debug_name, str vertex_shader_path, str fragment_shader_path);
void assign_texture_slot(Shader *shader, str texture_name, i32 slot);
void use_shader(Shader shader);
void set_uniform_i32(Shader shader, str name, i32 value);
void set_uniform_f32(Shader shader, str name, f32 value);
void set_uniform_m4(Shader shader, str name, m4 *matrix);
void set_uniform_v2(Shader shader, str name, v2 vector);
void set_uniform_v3(Shader shader, str name, v3 vector);
void set_uniform_v4(Shader shader, str name, v4 vector);

// Mesh API
Mesh *mesh_create(Renderer *renderer, Slice<MeshVertex> vertices, Slice<u32> indices);
Mesh *mesh_create_from_file(Renderer *renderer, str mesh_path);
void upload_mesh(Mesh *mesh);

// Render Texture API
RenderTexture load_render_texture(Renderer *renderer, str path);
void upload_texture_to_gpu(Renderer *renderer, RenderTexture *texture);

// Renderer init API
Renderer *REN();
bool renderer_init(Window *window, v4 clear_colour, v3 ambient_light, v3 sun_colour, v3 sun_position, v3 shadow_colour);

bool load_shaders(Renderer *renderer);
void delete_shaders(Renderer *renderer);
Texture *load_texture(Renderer *renderer, str path);
Texture *load_animated_texture(Renderer *renderer, str path, i64 cell_count, f32 animation_length);
bool build_atlas(Renderer *renderer);
u32 upload_texture_to_gpu(Renderer *renderer, i32 width, i32 height, u8 *data);
u32 upload_font_to_gpu(Renderer *renderer, i32 width, i32 height, u8 *data);
bool load_font(Renderer *renderer, str path, i64 width, i64 height, f32 pixel_height);

// Renderer frame API
void renderer_clear_frame(v4 colour);
void renderer_start_frame(Renderer *renderer);
void renderer_draw_frame(Renderer *renderer, Camera *camera, Viewport viewport, FrameBuffer *target, bool draw_ui);
void renderer_end_frame(Renderer *renderer);
void new_imgui_frame();
void draw_imgui_frame();

// Immediate rendering API
void draw_texture(Renderer *renderer, Texture *texture, Texture *normal_texture, v3 position, v2 size, f32 rotation, v4 color);
void draw_animated_texture(Renderer *renderer, Texture *texture, f32 time_in_animation, v3 position, v2 size, f32 rotation, v4 color);
void push_screen_quad(Renderer *renderer, v4 color);

// Drawing API
void draw_quad(Renderer *renderer, v3 position, v2 size, v3 rotation, v4 colour);
void draw_line(Renderer *renderer, v3 position, v3 direction, f32 radius, f32 step, v4 colour);
void draw_cube(Renderer *renderer, v3 position, v3 size, v3 rotation, v4 colour);
void draw_sphere(Renderer *renderer, v3 position, f32 radius, v4 colour);
void draw_mesh(Renderer *renderer, Mesh *mesh, v3 position, v3 scale, v3 rotation, v4 colour);

// Drawing UI API
void draw_quad_ui(Renderer *renderer, v3 position, v2 size, v3 rotation, v4 colour, v2 uvs[4], DrawType type);
void draw_rectangle_ui(Renderer *renderer, v3 position, v2 size, v3 rotation, v4 colour);
void draw_circle_ui(Renderer *renderer, v3 position, v2 size, v3 rotation, v4 colour);
void draw_text_ui(Renderer *renderer, str text, v3 position, f32 font_size, v4 color);

void toggle_wireframe(Renderer *renderer);
f32 texture_aspect_ratio(Renderer *renderer, Texture *texture);

bool frame_buffer_init(FrameBuffer *frame_buffer);
void frame_buffer_bind(FrameBuffer *frame_buffer);
void frame_buffer_unbind();
bool frame_buffer_maybe_resize(FrameBuffer *frame_buffer, v2i new_size);
bool frame_buffer_rebuild(FrameBuffer *frame_buffer);
void frame_buffer_copy_to(FrameBuffer *source_buffer, FrameBuffer *dest_buffer);

v3 screen_position_to_world_position(Renderer *renderer, Viewport viewport, v3 screen_position);
v3 screen_position_to_ndc(Viewport viewport, v3 screen_position);
v3 relative_to_screen_position(Viewport viewport, v2 relative_position);

m4 get_model_matrix(v3 position, v3 scale, v3 rotation);
m4 get_view_matrix(Camera *camera);
m4 get_projection_matrix(Camera *camera, f32 aspect);
m4 get_projection_matrix_ortho(Camera *camera, f32 aspect);

Ray ray_create(v3 origin, v3 direction);
Ray ray_from_screen_position(Viewport viewport, v3 screen_position);

QuadBuffer quad_buffer_create(i64 size);
void quad_buffer_bind_and_update(QuadBuffer *buffer);
void quad_buffer_unbind();
Quad *quad_buffer_push(QuadBuffer *buffer);
void quad_buffer_reset(QuadBuffer *buffer);

v4 rgb(i64 r, i64 g, i64 b);
v4 rgba(i64 r, i64 g, i64 b, i64 a);
v4 alpha(v4 base, f32 alpha);
v4 brightness(v4 base, f32 brightness);
v4 mix(v4 c1, v4 c2, f32 t);

void set_imgui_theme();

void clear_gl_errors();
bool check_gl_errors();

void opengl_error_callback(GLenum source, GLenum type, u32 id, GLenum severity, i32 length, const char *message, const void *user_param);

void print(v2 vector);
void print(v3 vector);
void print(v4 vector);

f32 accel_lerp(f32 a, f32 b, f32 f);

Camera camera_create(CameraMode mode, f32 fov, v3 position, f32 near_plane, f32 far_plane) {
    return Camera {
        .mode = mode,
        .fov = fov,
        .position = position,
        .near_plane = near_plane,
        .far_plane = far_plane
    };
}

v3 get_forward_direction(Camera *camera) {
    // pitch    - x
    // yaw      - y
    // roll     - z
    v3 direction {
        .x = sin(camera->rotation.y * HMM_DegToRad) * cos(camera->rotation.x * HMM_DegToRad),
        .y = sin(camera->rotation.x * HMM_DegToRad),
        .z = cos(camera->rotation.y * HMM_DegToRad) * cos(camera->rotation.x * HMM_DegToRad)
    };

    return norm(direction);
}

v3 get_right_direction(Camera *camera) {
    return norm(HMM_Cross({0, 1, 0}, get_forward_direction(camera)));
}

v3 get_up_direction(Camera *camera) {
    return norm(HMM_Cross(get_forward_direction(camera), get_right_direction(camera)));
}

v3 get_forward_direction(v3 rotation) {
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

v3 get_right_direction(v3 rotation) {
    return HMM_Cross(get_up_direction(rotation), get_forward_direction(rotation));
}

v3 get_up_direction(v3 rotation) {
    return {0, 1, 0};
}

bool init_shader(Shader *shader, str debug_name, str vertex_shader_path, str fragment_shader_path) {
    const i64 buffer_size = 640;
    i32 compile_status = 0;
    i32 link_status = 0;
    char error_buffer[buffer_size];
    
    str vertex_shader_source = read_entire_file(vertex_shader_path);
    if (vertex_shader_source.len == 0) {
        logf("{}: failed to load vertex shader file", debug_name.c());
        return false;
    }

    str fragment_shader_source = read_entire_file(fragment_shader_path);
    if (fragment_shader_source.len == 0) {
        logf("{}: failed to load default fragment shader file", debug_name.c());
        return false;
    }

    u32 vertex_shader = glCreateShader(GL_VERTEX_SHADER);

    glShaderSource(vertex_shader, 1, (char **) &vertex_shader_source.ptr, NULL);
    glCompileShader(vertex_shader);

    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &compile_status);
    if (compile_status == 0) {
        glGetShaderInfoLog(vertex_shader, buffer_size, nullptr, &error_buffer[0]);
        logf("{}: failed to compile vertex shader: {}", debug_name.c(), error_buffer);
        return false;
    }

    u32 fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

    glShaderSource(fragment_shader, 1, (char**) &fragment_shader_source.ptr, NULL);
    glCompileShader(fragment_shader);

    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &compile_status);
    if (compile_status == 0) {
        glGetShaderInfoLog(fragment_shader, buffer_size, nullptr, &error_buffer[0]);
        logf("{}: failed to compile fragment shader: {}", debug_name.c(), error_buffer);
        return false;
    }

    u32 shader_program = glCreateProgram();

    glAttachShader(shader_program, vertex_shader);
    glAttachShader(shader_program, fragment_shader);
    glLinkProgram(shader_program); 

    glGetProgramiv(shader_program, GL_LINK_STATUS, &link_status);

    if (link_status == 0) {
        glGetProgramInfoLog(shader_program, buffer_size, nullptr, &error_buffer[0]);
        logf("{}: failed to link shader program: {}", debug_name.c(), error_buffer);
        return false;
    }
 
    shader->id = shader_program; 
    shader->debug_name = debug_name;

    logf("Compiled and linked {}", debug_name.c());

    return true;
}

void assign_texture_slot(Shader *shader, str texture_name, i32 slot) {
    glUseProgram(shader->id);
    glUniform1i(glGetUniformLocation(shader->id, texture_name.c()), slot);
    glUseProgram(0);
}

void use_shader(Shader shader) {
    glUseProgram(shader.id);
}

void set_uniform_i32(Shader shader, str name, i32 value) {
    glUniform1i(glGetUniformLocation(shader.id, name.c()), value);
}

void set_uniform_f32(Shader shader, str name, f32 value) {
    glUniform1f(glGetUniformLocation(shader.id, name.c()), value);
}

void set_uniform_m4(Shader shader, str name, m4 *matrix) {
    glUniformMatrix4fv(
        glGetUniformLocation(shader.id, name.c()),
        1,
        false,
        (f32 *) &matrix->Columns[0]
    );
}

void set_uniform_v2(Shader shader, str name, v2 vector) {
    glUniform2f(
        glGetUniformLocation(shader.id, name.c()),
        vector.x, vector.y
    );
}

void set_uniform_v3(Shader shader, str name, v3 vector) {
    glUniform3f(
        glGetUniformLocation(shader.id, name.c()),
        vector.x, vector.y, vector.z 
    );
}

void set_uniform_v4(Shader shader, str name, v4 vector) {
    glUniform4f(
        glGetUniformLocation(shader.id, name.c()),
        vector.x, vector.y, vector.z, vector.w
    );
}

Mesh *mesh_create(Renderer *renderer, Slice<MeshVertex> vertices, Slice<u32> indices) {
    Mesh* mesh = push(&renderer->meshes);

    *mesh = Mesh {
        .vertices = vertices,
        .indices = indices
    };

    { // vertex array
        u32 vertex_array;
        glGenVertexArrays(1, &vertex_array);
        glBindVertexArray(vertex_array);

        mesh->vertex_array_id = vertex_array;
    }

    { // vertex buffer
        u32 vertex_buffer;
        glGenBuffers(1, &vertex_buffer);
        glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(MeshVertex) * mesh->vertices.len, mesh->vertices.ptr, GL_DYNAMIC_DRAW);

        mesh->vertex_buffer_id = vertex_buffer;
    }

    { // index buffer
        u32 index_buffer;
        glGenBuffers(1, &index_buffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(u32) * indices.len, indices.ptr, GL_STATIC_DRAW);

        mesh->index_buffer_id = index_buffer;
    }

    { // vertex attributes
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void *) offsetof(MeshVertex, position));
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void *) offsetof(MeshVertex, normal));
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void *) offsetof(MeshVertex, uv));

        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glEnableVertexAttribArray(2);
    }

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    return mesh;
}

Mesh *mesh_create_from_file(Renderer *renderer, str mesh_path) {
    // Create an instance of the Importer class
    Assimp::Importer importer;
    
    const aiScene *scene = importer.ReadFile(mesh_path.c(), aiProcess_Triangulate | aiProcess_MakeLeftHanded | aiProcess_GenSmoothNormals);	
    if(scene == NULL || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        logf("assimp error: {}", importer.GetErrorString());
        return NULL;
    }

    // only assuming one child and one mesh for now
    ASSERT(scene->mRootNode->mNumChildren == 1);
    aiNode *node = scene->mRootNode->mChildren[0];

    ASSERT(node->mNumMeshes == 1);
    aiMesh *loaded_mesh = scene->mMeshes[node->mMeshes[0]];

    Slice<MeshVertex> vertices = slice_create_malloc<MeshVertex>(loaded_mesh->mNumVertices);
    Slice<u32> indices = slice_create_malloc<u32>(loaded_mesh->mNumFaces * 3);

    for(i64 v = 0; v < loaded_mesh->mNumVertices; v++) {
        MeshVertex vertex = {};
        vertex.position.x = loaded_mesh->mVertices[v].x;
        vertex.position.y = loaded_mesh->mVertices[v].y;
        vertex.position.z = loaded_mesh->mVertices[v].z;

        vertex.normal.x = loaded_mesh->mNormals[v].x;
        vertex.normal.y = loaded_mesh->mNormals[v].y;
        vertex.normal.z = loaded_mesh->mNormals[v].z;

        // is this needed ?
        // vertex.normal = norm(vertex.normal);

        vertices[v] = vertex;
    }

    i64 index = 0;

    for(i64 f = 0; f < loaded_mesh->mNumFaces; f++) {
        aiFace face = loaded_mesh->mFaces[f];
        for(i64 j = 0; j < face.mNumIndices; j++) {
            indices[index] = face.mIndices[j];
            index++;
        }
    }

    logf("Loaded model with path \"{}\"", mesh_path.c());

    return mesh_create(renderer, vertices, indices);
}

void upload_mesh(Mesh *mesh) {
    glBindBuffer(GL_ARRAY_BUFFER, mesh->vertex_buffer_id);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(MeshVertex) * mesh->vertices.len, &mesh->vertices[0]);
}

RenderTexture load_render_texture(Renderer *renderer, str path) {
    i32 width       = 0;
    i32 height      = 0;
    i32 channels    = 0;
    u8 *image_data  = nullptr;

    stbi_set_flip_vertically_on_load(true);

    image_data = stbi_load(path.c(), &width, &height, &channels, 4);
    if (!image_data) {
        printf("Failed to load texture: %s\n", path.c());
        return {};
    }

    logf("loaded texture with path \"{}\" [{}x{}] {} bytes", path.c(), width, height, width * height * channels);

    return RenderTexture {
        .id = 0,
        .width = width,
        .height = height,
        .data = image_data,
    };
}

void upload_texture_to_gpu(Renderer *renderer, RenderTexture *texture) {
    glGenTextures(1, &texture->id);

    glBindTexture(GL_TEXTURE_2D, texture->id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture->width, texture->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, texture->data);

    glBindTexture(GL_TEXTURE_2D, 0);
}

Renderer *REN() {
    ASSERT(g_renderer != NULL);
    return g_renderer;
}

bool renderer_init(Window *window, v4 clear_colour, v3 ambient_light, v3 sun_colour, v3 sun_position, v3 shadow_colour) {
    g_renderer = new Renderer {
        .clear_colour = clear_colour,
        .ambient_light = ambient_light,
        .sun_colour = sun_colour,
        .sun_position = sun_position,
        .shadow_colour = shadow_colour,
        .meshes = fixed_array_create<Mesh>(MAX_MESHES),
        .commands = fixed_array_create<RenderCommand>(MAX_RENDER_COMMANDS),
        .textures = stack_array_create<Texture, MAX_TEXTURES>(),
        .g_buffer = FrameBuffer {.size = {100, 100}},
        .lighting_buffer = FrameBuffer {.size = {100, 100}},
        .ui_buffer = FrameBuffer {.size = {100, 100}},
    };

    Renderer *renderer = REN();

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
            renderer->clear_colour.r,
            renderer->clear_colour.g,
            renderer->clear_colour.b,
            renderer->clear_colour.a
        );
    }

    bool ok = load_shaders(renderer);
    if (!ok) {
        log("Error when loading and compiling shaders");
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

        io.Fonts->AddFontFromFileTTF("resources/fonts/OpenSans-Bold.ttf", 16);
        io.Fonts->Build();

        // set custom colours
        set_imgui_theme();
        ImGui::GetStyle().FrameRounding = 2;

        ImGui_ImplGlfw_InitForOpenGL(window->glfw_window, true);
        ImGui_ImplOpenGL3_Init("#version 460");
    } 

    { // load default normal texture
        Texture *texture = load_texture(renderer, "resources/textures/defaults/normal.png");
        if (texture == NULL) {
            log("failed to load default texture");
            return false;
        }

        renderer->default_normal = texture;
    }

    { // load default font
        bool ok = load_font(renderer, "resources/fonts/OpenSans-Bold.ttf", 1000, 1000, 160);
        if (!ok) {
            log("failed to load default font");
            return false;
        }
    }

    { // load primitive models 
        renderer->cube_primitive = mesh_create_from_file(REN(), "resources/models/primitives/cube.obj");
        ASSERT(renderer->cube_primitive);

        renderer->sphere_primitive = mesh_create_from_file(REN(), "resources/models/primitives/sphere.obj");
        ASSERT(renderer->sphere_primitive);

        Slice<MeshVertex> verts = slice_create_malloc<MeshVertex>(4);
        Slice<u32> indices = slice_create_malloc<u32>(6);

        verts[0] = MeshVertex {.position = {-0.5,  0.5, 0}, .normal = {0, 0, -1}, .uv = {0, 1}};
        verts[1] = MeshVertex {.position = { 0.5,  0.5, 0}, .normal = {0, 0, -1}, .uv = {1, 1}};
        verts[2] = MeshVertex {.position = { 0.5, -0.5, 0}, .normal = {0, 0, -1}, .uv = {1, 0}};
        verts[3] = MeshVertex {.position = {-0.5, -0.5, 0}, .normal = {0, 0, -1}, .uv = {0, 0}};

        indices[0] = 0;
        indices[1] = 2;
        indices[2] = 1;
        indices[3] = 0;
        indices[4] = 3;
        indices[5] = 2;

        renderer->quad_primitive = mesh_create(REN(), verts, indices);
        ASSERT(renderer->quad_primitive);
    }

    { // create ui quad buffer
        renderer->ui_quads = quad_buffer_create(MAX_UI_QUADS);
        renderer->screen_quad = quad_buffer_create(1);
    }

    { // create frame buffers 
        frame_buffer_init(&renderer->g_buffer);
        frame_buffer_init(&renderer->lighting_buffer);
        frame_buffer_init(&renderer->ui_buffer);
    }

    return true;
}

bool load_shaders(Renderer *renderer) {
    bool ok = init_shader(&renderer->default_shader, "Default shader", "resources/shaders/default_vertex.shader", "resources/shaders/default_fragment.shader");
    if (!ok) {
        log("Error when creating default shader program");
        return false;
    }

    assign_texture_slot(&renderer->default_shader, "atlas_texture", 0);
    assign_texture_slot(&renderer->default_shader, "font_texture", 1);

    ok = init_shader(&renderer->lighting_shader, "Lighting shader", "resources/shaders/lighting_vertex.shader", "resources/shaders/lighting_fragment.shader");
    if (!ok) {
        log("Error when creating lighting shader program");
        return false;
    }

    assign_texture_slot(&renderer->lighting_shader, "position_map", 0);
    assign_texture_slot(&renderer->lighting_shader, "normal_map", 1);
    assign_texture_slot(&renderer->lighting_shader, "albedo_map", 2);
    assign_texture_slot(&renderer->lighting_shader, "sun_position_map", 3);
    assign_texture_slot(&renderer->lighting_shader, "shadow_map", 4);
    assign_texture_slot(&renderer->lighting_shader, "ssao_map", 5);

    ok = init_shader(&renderer->mesh_shader, "Mesh shader", "resources/shaders/mesh_vertex.shader", "resources/shaders/mesh_fragment.shader");
    if (!ok) {
        log("Error when creating mesh shader program");
        return false;
    }

    return true;
}

void delete_shaders(Renderer *renderer) {
    glDeleteProgram(renderer->default_shader.id);
    glDeleteProgram(renderer->lighting_shader.id);
    glDeleteProgram(renderer->mesh_shader.id);
}

Texture *load_texture(Renderer *renderer, str path) {
    i32 width       = 0;
    i32 height      = 0;
    i32 channels    = 0;
    u8 *image_data  = nullptr;

    stbi_set_flip_vertically_on_load(true);

    image_data = stbi_load(path.c(), &width, &height, &channels, 4);
    if (!image_data) {
        printf("Failed to load texture: %s\n", path.c());
        return NULL;
    }

    logf("Loaded texture with path \"{}\" [{}x{}] {} bytes", path.c(), width, height, width * height * channels);

    i64 id = renderer->textures.len;

    Texture *texture = push(&renderer->textures);

    *texture = Texture {
        .type = TextureType::SINGLE,
        .width = width,
        .height = height,
        .data = image_data,
    };

    return texture;
}

Texture *load_animated_texture(Renderer *renderer, str path, i64 cell_count, f32 animation_length) {
    Texture *texture = load_texture(renderer, path);
    if(texture == NULL) {
        return NULL;
    }

    texture->type = TextureType::ANIMATED;
    texture->animation_length = animation_length;
    texture->sub_textures = slice_create_malloc<Texture>(cell_count);

    if(texture->width % cell_count != 0) {
        printf("Animated texture \"%s\" has a width of %llu, a cell count of %llu does not fit", path.c(), texture->width, cell_count);
        return NULL;
    }

    i64 sub_texture_width = texture->width / cell_count;
    i64 sub_texture_height = texture->height;

    for(i64 i = 0; i < texture->sub_textures.len; i++) {
        Texture *sub_texture    = &texture->sub_textures[i];
        sub_texture->type       = TextureType::SINGLE;
        sub_texture->width      = sub_texture_width;
        sub_texture->height     = sub_texture_height;
    }

    return texture;
}

bool build_atlas(Renderer *renderer) {
    const i64 ATLAS_WIDTH     = 480;
    const i64 ATLAS_HEIGHT    = 480;
    const i64 BYTES_PER_PIXEL = 4;
    const i64 CHANNELS        = 4;
    const i64 ATLAS_BYTE_SIZE = ATLAS_WIDTH * ATLAS_HEIGHT * BYTES_PER_PIXEL;

    stbrp_context rp_context;
    i64 rect_count    = renderer->textures.len;
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

    for(i64 i = 0; i < renderer->textures.len; i++) {
        Texture *texture = &renderer->textures[i];
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
        Texture *texture = &renderer->textures[rect->id];

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

    u32 texture_id = upload_texture_to_gpu(renderer, ATLAS_WIDTH, ATLAS_HEIGHT, atlas_data);
    if (texture_id == 0) {
        printf("error sending atlas texture GPU\n");
        return false;
    }

    renderer->atlas_texture_id = texture_id;

    // now that all of the texture uvs are set and the data is copied to the atlas
    // it is time to figure out the uvs for all sub textures in animated textures
    // it is enforced that the sub textures equally divide the texture's width as
    // it is a horizontal strip. The height of each sub texture is then assumed to be
    // the same as the parent texture
    for(Texture &texture : renderer->textures) {
        if(texture.type == TextureType::SINGLE) {
            continue;
        }

        i64 sub_texture_count   = texture.sub_textures.len;
        f32 starting_x          = texture.uvs[0].x;
        f32 ending_x            = texture.uvs[1].x;
        f32 step_x              = (ending_x - starting_x) / (f32) sub_texture_count;
        f32 top_y               = texture.uvs[0].y;
        f32 bottom_y            = texture.uvs[2].y;

        for(i64 i = 0; i < sub_texture_count; i++) {
            Texture *sub_texture = &texture.sub_textures[i];

            sub_texture->uvs[0] = v2{
                starting_x + (step_x * (f32) i),
                top_y
            };

            sub_texture->uvs[1] = v2{
                starting_x + (step_x * (f32) (i + 1)), // +1 because this is the right side
                top_y
            };

            sub_texture->uvs[2] = v2{
                starting_x + (step_x * (f32) (i + 1)), // +1 because this is the right side
                bottom_y
            };

            sub_texture->uvs[3] = v2{
                starting_x + (step_x * (f32) i),                 
                bottom_y
            };
        }
    }

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

u32 upload_texture_to_gpu(Renderer *renderer, i32 width, i32 height, u8 *data) {
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

u32 upload_font_to_gpu(Renderer *renderer, i32 width, i32 height, u8 *data) {
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

bool load_font(Renderer *renderer, str path, i64 width, i64 height, f32 pixel_height) {
    Font font = Font{
        .width = width,
        .height = height,
        .characters = {},
        .bitmap_data = (u8 *) malloc(width * height),
    };

    str font_data = read_entire_file(path);
    if (font_data.len == 0) {
        printf("failed to load font \"%s\"\n", path.c());
        return false;
    }

    i64 bake_result = stbtt_BakeFontBitmap((const u8*)font_data.c(), 0, pixel_height, font.bitmap_data, font.width, font.height, 32, font.characters.size, font.characters.data);
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

    renderer->font_texture_id = upload_font_to_gpu(renderer, font.width, font.height, font.bitmap_data);
    assert(renderer->font_texture_id != 0);

    renderer->font = font;

    return true;
}

void renderer_clear_frame(v4 colour) {
    glClearColor(
        colour.r,
        colour.g,
        colour.b,
        1 
    );

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void renderer_start_frame(Renderer *renderer) {
    renderer_clear_frame(renderer->clear_colour);
}

void renderer_draw_frame(Renderer *renderer, Camera *camera, Viewport viewport, FrameBuffer *target, bool draw_ui) {
    renderer->view_matrix = get_view_matrix(camera);
    renderer->projection_matrix = get_projection_matrix(camera, f32(viewport.size.x) / f32(viewport.size.y));
    renderer->projection_matrix_ortho = get_projection_matrix_ortho(camera,  f32(viewport.size.x) / f32(viewport.size.y));

    { // geometry pass
        frame_buffer_maybe_resize(&renderer->g_buffer, viewport.size);
        frame_buffer_bind(&renderer->g_buffer);
        renderer_clear_frame(BLACK);
    
        for (RenderCommand &command : renderer->commands) {
            if (command.type == RC_MODEL) {
                MeshRenderCommand *model_cmd = &command.mesh;
                 
                m4 model_matrix = get_model_matrix(model_cmd->position, model_cmd->scale, model_cmd->rotation);
         
                use_shader(renderer->mesh_shader);
         
                set_uniform_m4(renderer->mesh_shader, "model", &model_matrix);
                set_uniform_m4(renderer->mesh_shader, "view", &renderer->view_matrix);
                set_uniform_m4(renderer->mesh_shader, "projection", &renderer->projection_matrix);
                set_uniform_v4(renderer->mesh_shader, "colour", model_cmd->colour);
            
                GL_CALL(glBindVertexArray(model_cmd->mesh->vertex_array_id));
                GL_CALL(glDrawElements(GL_TRIANGLES, model_cmd->mesh->indices.len, GL_UNSIGNED_INT, 0));
            }
        }
    
        frame_buffer_unbind();
    }

    { // lighting pass
        frame_buffer_maybe_resize(&renderer->lighting_buffer, viewport.size);
        frame_buffer_bind(&renderer->lighting_buffer);
        renderer_clear_frame(renderer->clear_colour);
    
        push_screen_quad(renderer, WHITE);
        quad_buffer_bind_and_update(&renderer->screen_quad);
        
        use_shader(renderer->lighting_shader);
    
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, renderer->g_buffer.position_attachment);
    
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, renderer->g_buffer.normals_attachment);
    
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, renderer->g_buffer.albedo_attachment);
    
        set_uniform_v3(renderer->lighting_shader, "ambient_light", renderer->ambient_light);
        set_uniform_v3(renderer->lighting_shader, "sun_position", renderer->sun_position);
        set_uniform_v3(renderer->lighting_shader, "sun_colour", renderer->sun_colour);
        set_uniform_v3(renderer->lighting_shader, "shadow_colour", renderer->shadow_colour);
            
        glDrawElements(GL_TRIANGLES, 6 * renderer->screen_quad.quads.len, GL_UNSIGNED_INT, 0);
    
        quad_buffer_reset(&renderer->screen_quad);
        quad_buffer_unbind();
    
        frame_buffer_unbind();
    }

    if (draw_ui) { // ui pass
        m4 model_matrix = HMM_M4D(1.0f);
        m4 view_matrix = HMM_M4D(1.0f);
        m4 projection_matrix = HMM_Orthographic_LH_NO(0, viewport.size.x, 0, viewport.size.y,  -1, 1);

        // drawing on top of lighting buffer output
        frame_buffer_bind(&renderer->lighting_buffer);
    
        quad_buffer_bind_and_update(&renderer->ui_quads);
     
        use_shader(renderer->default_shader);
    
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, renderer->atlas_texture_id);
    
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, renderer->font_texture_id);
    
        set_uniform_m4(renderer->default_shader, "model", &model_matrix);
        set_uniform_m4(renderer->default_shader, "view", &view_matrix);
        set_uniform_m4(renderer->default_shader, "projection", &projection_matrix);
    
        glDrawElements(GL_TRIANGLES, 6 * renderer->ui_quads.quads.len, GL_UNSIGNED_INT, 0);
    
        quad_buffer_unbind();
        frame_buffer_unbind();
    }

    { // copy to target buffer
        frame_buffer_maybe_resize(target, viewport.size);
        frame_buffer_copy_to(&renderer->lighting_buffer, target);
    }
}

void renderer_end_frame(Renderer *renderer) {
    reset(&renderer->commands);
    quad_buffer_reset(&renderer->ui_quads);
    quad_buffer_reset(&renderer->screen_quad);
}

void new_imgui_frame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame(); 
}

void draw_imgui_frame() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    GLFWwindow *current = glfwGetCurrentContext();
    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault();
    glfwMakeContextCurrent(current);
}

void push_screen_quad(Renderer *renderer, v4 color) {
    Quad *quad = quad_buffer_push(&renderer->screen_quad);

    // 0.99 so it is barely in clip space and behind everything else
    v3 pos[4] = {
        {-1,  1, 0.99}, // top left
        { 1,  1, 0.99}, // top right
        { 1, -1, 0.99}, // bottom right 
        {-1, -1, 0.99}, // bottom left
    };

    quad->vertices[0].position = pos[0];
    quad->vertices[1].position = pos[1];
    quad->vertices[2].position = pos[2];
    quad->vertices[3].position = pos[3];
                
    quad->vertices[0].colour = color;
    quad->vertices[1].colour = color;
    quad->vertices[2].colour = color;
    quad->vertices[3].colour = color;

    quad->vertices[0].uv = QUAD_UVS[0];
    quad->vertices[1].uv = QUAD_UVS[1];
    quad->vertices[2].uv = QUAD_UVS[2];
    quad->vertices[3].uv = QUAD_UVS[3];

    quad->vertices[0].draw_type = 0;
    quad->vertices[1].draw_type = 0;
    quad->vertices[2].draw_type = 0;
    quad->vertices[3].draw_type = 0;
}

void draw_quad(Renderer *renderer, v3 position, v2 size, v3 rotation, v4 colour) {
    draw_mesh(renderer, renderer->quad_primitive, position, v3{size.x, size.y, 1}, rotation, colour);
}

void draw_line(Renderer *renderer, v3 position, v3 direction, f32 radius, f32 step, v4 colour) {
    v3 dir = norm(direction);

    for (i64 i = 0; i < 200; i++) {
        v3 draw_position = position + (dir * step * f32(i));
        draw_sphere(renderer, draw_position, radius, colour);
    }
}

void draw_cube(Renderer *renderer, v3 position, v3 size, v3 rotation, v4 colour) {
    draw_mesh(renderer, renderer->cube_primitive, position, size, rotation, colour);
}

void draw_sphere(Renderer *renderer, v3 position, f32 radius, v4 colour) {
    draw_mesh(renderer, renderer->sphere_primitive, position, v3{radius, radius, radius} * 2, v3{}, colour);
}

void draw_mesh(Renderer *renderer, Mesh *mesh, v3 position, v3 scale, v3 rotation, v4 colour) {
    RenderCommand *command = push(&renderer->commands);
    command->type = RC_MODEL;
    command->mesh.position = position;
    command->mesh.scale = scale;
    command->mesh.rotation = rotation;
    command->mesh.mesh = mesh;
    command->mesh.colour = colour;
}

void draw_quad_ui(Renderer *renderer, v3 position, v2 size, v3 rotation, v4 colour, v2 uvs[4], DrawType type) {
    Quad *quad = quad_buffer_push(&renderer->ui_quads);

    for (i64 i = 0; i < 4; i++) {
        quad->vertices[i].position  = (QUAD_POSITIONS[i] * v3{size.x, size.y, 1}) + position;
        quad->vertices[i].colour    = colour;
        quad->vertices[i].uv        = uvs[i];
        quad->vertices[i].draw_type = (i32) type;
    }
}

void draw_rectangle_ui(Renderer *renderer, v3 position, v2 size, v3 rotation, v4 colour) {
    draw_quad_ui(renderer, position, size, rotation, colour, QUAD_UVS, DrawType::RECTANGLE);
}

void draw_circle_ui(Renderer *renderer, v3 position, v2 size, v3 rotation, v4 colour) {
    draw_quad_ui(renderer, position, size, rotation, colour, QUAD_UVS, DrawType::CIRCLE);
}

void draw_text_ui(Renderer *renderer, str text, v3 position, f32 font_size, v4 color) {
    if (text.len == 0) {
        return;
    }

    struct Glyph {
        v2 position;
        v2 size;
        v2 uvs[4];
    };

    Slice<Glyph> glyphs = slice_create_malloc<Glyph>(text.len);

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
 
        stbtt_GetBakedQuad(renderer->font.characters.data, renderer->font.width, renderer->font.height, c - 32, &advanced_x, &advanced_y, &aligned_quad, false);

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
        v3 centered_v3 = v3{quad_centered_position.x, quad_centered_position.y, position.z};

        draw_quad_ui(renderer, centered_v3, scaled_size, {}, color, glyph->uvs, DrawType::TEXT);
   }

    slice_free(glyphs);
}

void toggle_wireframe(Renderer *renderer) {
    renderer->wireframe = !renderer->wireframe;

    if (renderer->wireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    } else {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
}

f32 texture_aspect_ratio(Renderer *renderer, Texture *texture) {
    return (f32) texture->width / (f32) texture->height;
}

bool frame_buffer_init(FrameBuffer *frame_buffer) {
    return frame_buffer_rebuild(frame_buffer);
}

void frame_buffer_bind(FrameBuffer *frame_buffer) {
    glBindFramebuffer(GL_FRAMEBUFFER, frame_buffer->id);

    // viewport settings
    glViewport(0, 0, frame_buffer->size.x, frame_buffer->size.y);
}

void frame_buffer_unbind() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

bool frame_buffer_maybe_resize(FrameBuffer *frame_buffer, v2i new_size) {
    v2i old_size = frame_buffer->size;
    frame_buffer->size = new_size;

    if (old_size.x != new_size.x || old_size.y != new_size.y) {
        return frame_buffer_rebuild(frame_buffer);
    }

    return true;
}

bool frame_buffer_rebuild(FrameBuffer *frame_buffer) {
    if (frame_buffer->id != 0) {
        glDeleteFramebuffers(1, &frame_buffer->id);
        glDeleteTextures(1, &frame_buffer->position_attachment);
        glDeleteTextures(1, &frame_buffer->normals_attachment);
        glDeleteTextures(1, &frame_buffer->albedo_attachment);
        glDeleteTextures(1, &frame_buffer->depth_attachment);
    }

    glCreateFramebuffers(1, &frame_buffer->id);
    glBindFramebuffer(GL_FRAMEBUFFER, frame_buffer->id);

    { // position attachment
        glCreateTextures(GL_TEXTURE_2D, 1, &frame_buffer->position_attachment);
        glBindTexture(GL_TEXTURE_2D, frame_buffer->position_attachment);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, frame_buffer->size.x, frame_buffer->size.y, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); 

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, frame_buffer->position_attachment, 0);
    }

    { // normal attachment
        glCreateTextures(GL_TEXTURE_2D, 1, &frame_buffer->normals_attachment);
        glBindTexture(GL_TEXTURE_2D, frame_buffer->normals_attachment);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, frame_buffer->size.x, frame_buffer->size.y, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, frame_buffer->normals_attachment, 0);
    }

    { // albedo attachment
        glCreateTextures(GL_TEXTURE_2D, 1, &frame_buffer->albedo_attachment);
        glBindTexture(GL_TEXTURE_2D, frame_buffer->albedo_attachment);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, frame_buffer->size.x, frame_buffer->size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, frame_buffer->albedo_attachment, 0);
    }

    { // depth attachment
        glCreateTextures(GL_TEXTURE_2D, 1, &frame_buffer->depth_attachment);
        glBindTexture(GL_TEXTURE_2D, frame_buffer->depth_attachment);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, frame_buffer->size.x, frame_buffer->size.y, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, frame_buffer->depth_attachment, 0);
    }

    GLenum draw_buffers[3] = {
        GL_COLOR_ATTACHMENT0,
        GL_COLOR_ATTACHMENT1,
        GL_COLOR_ATTACHMENT2,
    };

    // when using more then one colour attachment, need to set all colour buffers the
    // frame buffer can write too, if not the normal buffer will not be write too
    glDrawBuffers(3, draw_buffers);

    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        printf("error when createing frame buffer, was not complete\n");
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void frame_buffer_copy_to(FrameBuffer *source_buffer, FrameBuffer *dest_buffer) {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, source_buffer->id);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dest_buffer->id);

    glBlitFramebuffer(
        0, 0, source_buffer->size.x, source_buffer->size.y,
        0, 0, dest_buffer->size.x, dest_buffer->size.y,
        GL_COLOR_BUFFER_BIT,
        GL_NEAREST
    );

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

v3 screen_position_to_world_position(Renderer *renderer, Viewport viewport, v3 screen_position) {
    // while screen coords are just x and y, the z coord of screen
    // position determines the depth of the position in the view frustum
    // z=-1 -> near plane
    // z=1  -> far plane
    // - 13/08/25
    
    v3 ndc = screen_position_to_ndc(viewport, screen_position);

    m4 inverse_vp = HMM_InvGeneralM4(renderer->projection_matrix * renderer->view_matrix);

    v4 world_position = inverse_vp * v4{ndc.x, ndc.y, ndc.z, 1};
    world_position /= world_position.w;

    return v3{world_position.x, world_position.y, world_position.z};

#if 0
    v3 ndc = screen_position_to_ndc(window, screen_position);
    v4 clip = v4{ndc.x, ndc.y, ndc.z, 1};
    v4 eye = HMM_InvGeneralM4(renderer->projection_matrix) * clip;
    eye.z = -1; eye.w = 0;
    v4 world = HMM_InvGeneralM4(renderer->view_matrix) * eye;

    return v3{world.x, world.y, world.z};
#endif
}

v3 screen_position_to_ndc(Viewport viewport, v3 screen_position) {
    v3 v_ndc = screen_position;

    // set origin to center
    v_ndc.x -= f32(viewport.size.x) * 0.5f;
    v_ndc.y -= f32(viewport.size.y) * 0.5f;

    // scale to [-1, 1]
    v_ndc.x /= (f32(viewport.size.x) * 0.5f);
    v_ndc.y /= (f32(viewport.size.y) * 0.5f);

    return v_ndc;
}

v3 relative_to_screen_position(Viewport viewport, v2 relative_position) {
    return v3{f32(viewport.size.x), f32(viewport.size.y), 0} * v3{relative_position.x, relative_position.y, 0};
}

m4 get_model_matrix(v3 position, v3 scale, v3 rotation) {
    // IMPORTANT: if any of this stuff changes you need to update the docs 
    m4 model_matrix = HMM_M4D(1.0f);

    model_matrix = HMM_MulM4(model_matrix, HMM_Translate(position));

    // flip z axis to ensure -z = roll right & +z = roll left
    model_matrix = HMM_MulM4(model_matrix, HMM_Rotate_LH(rotation.z * HMM_DegToRad, {0, 0, -1}));
    model_matrix = HMM_MulM4(model_matrix, HMM_Rotate_LH(rotation.y * HMM_DegToRad, {0, 1, 0}));
    model_matrix = HMM_MulM4(model_matrix, HMM_Rotate_LH(rotation.x * HMM_DegToRad, {1, 0, 0}));

    model_matrix = HMM_MulM4(model_matrix, HMM_Scale({scale.x, scale.y, scale.z}));

    return model_matrix;
}

m4 get_view_matrix(Camera *camera) {
    v3 target = camera->position + get_forward_direction(camera);
    v3 up = get_up_direction(camera);

    // FIXME: having the up always be y = 1 is probably wrong - 04/06/25
    m4 view_matrix = HMM_LookAt_LH(
        camera->position, 
        target, 
        up 
    );

    return view_matrix;
}

m4 get_projection_matrix(Camera *camera, f32 aspect) {
    return HMM_Perspective_LH_NO(camera->fov * HMM_DegToRad, aspect, camera->near_plane, camera->far_plane);
}

m4 get_projection_matrix_ortho(Camera *camera, f32 aspect) {
    return HMM_Orthographic_LH_NO(
        -camera->orthographic_size * aspect,  // left
         camera->orthographic_size * aspect,  // right
        -camera->orthographic_size,           // bottom
         camera->orthographic_size,           // top
         camera->near_plane, 
         camera->far_plane 
    );
}

Ray ray_create(v3 origin, v3 direction) {
    return Ray {.origin = origin, .direction = direction};
}

Ray ray_from_screen_position(Viewport viewport, v3 screen_position) {
    // this sticks with my convention of having screen positions
    // all be v3, but with this case you probably always want the origin
    // to be on the near plane so I ignore the z of the give screen position
    // this is just to stop any annoying bugs by making an assumption 
    // - 13/08/25

    v3 start = screen_position_to_world_position(REN(), viewport, v3{screen_position.x, screen_position.y, -1});
    v3 end = screen_position_to_world_position(REN(), viewport, v3{screen_position.x, screen_position.y, 1});

    return ray_create(start, norm(end - start));
}

QuadBuffer quad_buffer_create(i64 size) {
    QuadBuffer buffer = QuadBuffer {
        .quads = fixed_array_create<Quad>(size),
    };

    { // vertex array
        u32 vertex_array;
        glGenVertexArrays(1, &vertex_array);
        glBindVertexArray(vertex_array);

        buffer.vertex_array_id = vertex_array;
    }

    { // vertex buffer
        u32 vertex_buffer;
        glGenBuffers(1, &vertex_buffer);
        glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(Quad) * size, buffer.quads.slice.ptr, GL_DYNAMIC_DRAW);

        buffer.vertex_buffer_id = vertex_buffer;
    }

    { // index buffer
        const i64 index_buffer_length = size * 6;
        Slice<u32> indices = slice_create_malloc<u32>(index_buffer_length);

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

        buffer.index_buffer_id = index_buffer;

        slice_free(indices);
    }

    { // vertex attributes
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *) offsetof(Vertex, position));   // position
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

    return buffer;
}

void quad_buffer_bind_and_update(QuadBuffer *buffer) {
    // update vertex buffer data
    glBindBuffer(GL_ARRAY_BUFFER, buffer->vertex_buffer_id);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(Quad) * buffer->quads.len, buffer->quads.slice.ptr);

    // bind vertex array
    glBindVertexArray(buffer->vertex_array_id);
}

void quad_buffer_unbind() {
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

Quad *quad_buffer_push(QuadBuffer *buffer) {
    return push(&buffer->quads);
}

void quad_buffer_reset(QuadBuffer *buffer) {
    reset(&buffer->quads);
}

v4 rgb(i64 r, i64 g, i64 b) {
    return rgba(r, g, b, 255);
}

v4 rgba(i64 r, i64 g, i64 b, i64 a) {
    return v4 {
        (f32) r / 255.0f,
        (f32) g / 255.0f,
        (f32) b / 255.0f,
        (f32) a / 255.0f,
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

v4 mix(v4 c1, v4 c2, f32 t) {
    v4 result;
    result.r = (1.0f - t) * c1.r + t * c2.r;
    result.g = (1.0f - t) * c1.g + t * c2.g;
    result.b = (1.0f - t) * c1.b + t * c2.b;
    result.a = (1.0f - t) * c1.a + t * c2.a;
    return result;
}

void set_imgui_theme() {
    ImVec4 window_background    = ImVec4(0.07f, 0.08f, 0.09f, 0.94f);
    ImVec4 border               = ImVec4(0.26f, 0.26f, 0.26f, 0.50f);
    ImVec4 widget               = ImVec4(0.21f, 0.21f, 0.21f, 0.54f);
    ImVec4 widget_hovered       = ImVec4(0.31f, 0.31f, 0.31f, 0.54f);
    ImVec4 widget_active        = ImVec4(0.31f, 0.31f, 0.31f, 0.54f);
    ImVec4 title_background     = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    ImVec4 title_active        = ImVec4(0.73f, 0.68f, 0.09f, 1.00f);

    ImVec4* colors = ImGui::GetStyle().Colors;
    colors[ImGuiCol_Text]                   = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg]               = window_background;
    colors[ImGuiCol_ChildBg]                = window_background;
    colors[ImGuiCol_PopupBg]                = window_background;
    colors[ImGuiCol_Border]                 = border;
    colors[ImGuiCol_BorderShadow]           = border;
    colors[ImGuiCol_FrameBg]                = widget;
    colors[ImGuiCol_FrameBgHovered]         = widget_hovered;
    colors[ImGuiCol_FrameBgActive]          = widget_active;
    colors[ImGuiCol_TitleBg]                = title_background;
    colors[ImGuiCol_TitleBgActive]          = title_active;
    colors[ImGuiCol_TitleBgCollapsed]       = title_background;
    colors[ImGuiCol_MenuBarBg]              = window_background;
    colors[ImGuiCol_ScrollbarBg]            = window_background;
    colors[ImGuiCol_ScrollbarGrab]          = widget;
    colors[ImGuiCol_ScrollbarGrabHovered]   = widget_hovered;
    colors[ImGuiCol_ScrollbarGrabActive]    = widget_active;
    colors[ImGuiCol_CheckMark]              = widget_active;
    colors[ImGuiCol_SliderGrab]             = widget;
    colors[ImGuiCol_SliderGrabActive]       = widget_active;
    colors[ImGuiCol_Button]                 = widget;
    colors[ImGuiCol_ButtonHovered]          = widget_hovered;
    colors[ImGuiCol_ButtonActive]           = widget_active;
    colors[ImGuiCol_Header]                 = widget;
    colors[ImGuiCol_HeaderHovered]          = widget_hovered;
    colors[ImGuiCol_HeaderActive]           = widget_active;
    colors[ImGuiCol_Separator]              = widget;
    colors[ImGuiCol_SeparatorHovered]       = widget_hovered;
    colors[ImGuiCol_SeparatorActive]        = widget_active;
    colors[ImGuiCol_ResizeGrip]             = widget;
    colors[ImGuiCol_ResizeGripHovered]      = widget_hovered;
    colors[ImGuiCol_ResizeGripActive]       = widget_active;
    colors[ImGuiCol_TabHovered]             = widget_hovered;
    colors[ImGuiCol_Tab]                    = widget;
    colors[ImGuiCol_TabSelected]            = widget_active;
    colors[ImGuiCol_TabSelectedOverline]    = widget;
    colors[ImGuiCol_TabDimmed]              = widget;
    colors[ImGuiCol_TabDimmedSelected]      = widget_active;
    colors[ImGuiCol_TabDimmedSelectedOverline]  = widget;
    colors[ImGuiCol_DockingPreview]         = ImVec4(0.26f, 0.59f, 0.98f, 0.70f);
    colors[ImGuiCol_DockingEmptyBg]         = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_PlotLines]              = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    colors[ImGuiCol_PlotHistogram]          = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    colors[ImGuiCol_TableHeaderBg]          = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
    colors[ImGuiCol_TableBorderStrong]      = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
    colors[ImGuiCol_TableBorderLight]       = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
    colors[ImGuiCol_TableRowBg]             = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]          = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
    colors[ImGuiCol_TextLink]               = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
    colors[ImGuiCol_DragDropTarget]         = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
    colors[ImGuiCol_NavCursor]              = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
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

void opengl_error_callback(GLenum source, GLenum type, u32 id, GLenum severity, i32 length, const char *message, const void *user_param) {
    printf("OpenGL error: %s\n", message);
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

////////////////////////////////////////////////////////////////////////////
//////////////////////////////// @sound ////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
#define MAX_SOUNDS 10

typedef ma_sound Sound;

struct SoundEngine {
    ma_engine engine;

    StackArray<Sound, MAX_SOUNDS> sounds;
};

// call sound_engine_init() and SE()
SoundEngine *g_sound_engine = NULL;

SoundEngine *SE();
bool sound_engine_init();
Sound *sound_engine_load(SoundEngine *sound_engine, str path);
void sound_engine_play(Sound *sound);

SoundEngine *SE() {
    ASSERT(g_sound_engine != NULL);
    return g_sound_engine;
}

bool sound_engine_init() {
    ASSERT(g_sound_engine == NULL);

    g_sound_engine = new SoundEngine {
        .sounds = stack_array_create<ma_sound, MAX_SOUNDS>() 
    };

    ma_result result = ma_engine_init(NULL, &g_sound_engine->engine);
    if (result != MA_SUCCESS) {
        log("failed to init sound engine");
        return false;
    }

    return true;
}

Sound *sound_engine_load(SoundEngine *sound_engine, str path) {
    Sound *sound = push(&sound_engine->sounds);

    ma_result result = ma_sound_init_from_file(&sound_engine->engine, path.c(), 0, NULL, NULL, sound);
    if (result != MA_SUCCESS) {
        logf("failed to load sound with path: \"{}\"", path.c());
        return NULL;
    }

    logf("Loaded sound with path \"{}\"", path.c());

    return sound; 
}

void sound_engine_play(Sound *sound) {
    ma_sound_start(sound);
}

#endif
