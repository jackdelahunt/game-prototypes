#ifndef ENGINE_CPP
#define ENGINE_CPP

#include <cassert>
#include <cstring>
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
    ASSERT(check_gl_errors());

#define GL_VERIFY() ASSERT(check_gl_errors());

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

/////////////////////////////////////////////////////////////////////////////
//////////////////////////////// @renderer //////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
#define MAX_QUADS 200000
#define MAX_MESHES 1000
#define MAX_LIGHTS 20
#define MAX_SPRITES 256
#define MAX_TEXTURES 256

struct MeshVertex {
    v3 position;
    v3 normal;
    v4 colour;
    v2 uv;
    v2 normal_uv;
};

struct MeshQuad {
    MeshVertex vertices[4];
};

struct Vertex {
    v4 position;
    v4 colour;
    v2 uv;
    v2 normal_uv;
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

struct Sprite {
    Texture *albedo;
    Texture *normal;
};

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

enum FrameBufferOptions {
    FB_POSITION_ATTACHMENT      = 1 << 0,
    FB_NORMAL_ATTACHMENT        = 1 << 1,
    FB_ALBEDO_ATTACHMENT        = 1 << 2,
    FB_DEPTH_ATTACHMENT         = 1 << 3,
    FB_DISABLE_READ_BUFFER      = 1 << 4,
    FB_DISABLE_DRAW_BUFFER      = 1 << 5,
};

struct FrameBuffer {
    u32 id;
    i64 width;
    i64 height;

    u32 position_attachment;
    u32 normals_attachment;
    u32 albedo_attachment;
    u32 depth_attachment;
};

struct Shader {
    string debug_name;
    u32 id;
};

struct Mesh {
    v3 position;
    FixedArray<MeshQuad> quads;

    u32 vertex_array_id;
    u32 vertex_buffer_id;
    u32 index_buffer_id;
};

enum class RenderMode {
    PERSPECTIVE,
    ORTHOGRAPHIC
};

struct Renderer {
    bool wireframe;

    v4 clear_colour;
    v3 ambient_light;
    v3 sun_colour;
    v3 sun_position;

    FixedArray<Mesh> meshes;
    FixedArray<Quad> quads;
    StackArray<Light, MAX_LIGHTS> lights;

    m4 view_matrix;
    m4 projection_matrix;
    m4 projection_matrix_ortho;

    StackArray<Sprite, MAX_SPRITES> sprites;
    StackArray<Texture, MAX_TEXTURES> textures;

    Texture *default_normal;

    Atlas atlas;

    Font font;

    FrameBuffer g_buffer;
    FrameBuffer sun_frame_buffer;

    u32 vertex_array_id;
    u32 vertex_buffer_id;
    u32 index_buffer_id;

    Shader default_shader;
    Shader geometry_shader;
    Shader post_processing_shader;
    Shader sun_shader;

    u32 atlas_texture_id;
    u32 font_texture_id;
};

v4 WHITE            = {1, 1, 1, 1};
v4 BLACK            = {0, 0, 0, 1};

v4 RED              = {1, 0, 0, 1};
v4 GREEN            = {0, 1, 0, 1};
v4 BLUE             = {0, 0, 1, 1};

v4 ORANGE           = {1, 0.64, 0.1, 1};
v4 CORNFLOUR_BLUE   = {0.35, 0.80, 0.80, 1};
v4 SUN_YELLOW       = {1, 0.95, 0.5, 1};

// Camera API
v3 get_forward_direction(Camera camera);
v3 get_right_direction(Camera camera);
v3 get_up_direction(Camera camera);
v3 get_forward_direction(v3 rotation);
v3 get_right_direction(v3 rotation);
v3 get_up_direction(v3 rotation);

// Shader API
void use_shader(Shader shader);
void set_uniform_m4(Shader shader, string name, m4 *matrix);
void set_uniform_v3(Shader shader, string name, v3 vector);
void set_uniform_v4(Shader shader, string name, v4 vector);

// Renderer init API
bool init_renderer(Renderer *renderer, Window *window);
bool load_shaders(Renderer *renderer);
void delete_shaders(Renderer *renderer);
Sprite *load_sprite(Renderer *renderer, string albedo_path, string normal_path);
Sprite *load_animated_sprite(Renderer *renderer, string albedo_path, i64 cell_count, f32 animation_length);
Texture *load_texture(Renderer *renderer, string path);
Texture *load_animated_texture(Renderer *renderer, string path, i64 cell_count, f32 animation_length);
bool build_atlas(Renderer *renderer);
u32 upload_texture_to_gpu(Renderer *renderer, i32 width, i32 height, u8 *data);
u32 upload_font_to_gpu(Renderer *renderer, i32 width, i32 height, u8 *data);
bool load_font(Renderer *renderer, string path, i64 width, i64 height, f32 pixel_height);

// Renderer frame API
void new_frame(Renderer *renderer, Window *window, Camera camera);
void draw_frame(Renderer *renderer, Window *window);
void new_imgui_frame();
void draw_imgui_frame();

// Immediate rendering API
void draw_rectangle(Renderer *renderer, v3 position, v2 size, v4 color);
void draw_circle(Renderer *renderer, v3 position, f32 radius, v4 color);
void draw_cube(Renderer *renderer, v3 position, v3 size, v3 rotation, v4 color);
void draw_sprite(Renderer *renderer, Sprite *sprite, v3 position, v2 size, f32 rotation, v4 color);
void draw_animated_sprite(Renderer *renderer, Sprite *sprite, f32 time_in_animation, v3 position, v2 size, f32 rotation, v4 color);
void draw_texture(Renderer *renderer, Texture *texture, Texture *normal_texture, v3 position, v2 size, f32 rotation, v4 color);
void draw_animated_texture(Renderer *renderer, Texture *texture, f32 time_in_animation, v3 position, v2 size, f32 rotation, v4 color);
void draw_text(Renderer *renderer, string text, v3 position, f32 font_size, v4 color);
void draw_light(Renderer *renderer, v3 position, f32 radius, v4 colour, f32 intensity);
Quad *push_quad(Renderer *renderer, v3 position, v2 size, v3 rotation, v4 color, v2 uvs[4], v2 normal_uvs[4], DrawType draw_type);
Quad *push_screen_quad(Renderer *renderer, v4 color);

void toggle_wireframe(Renderer *renderer);
f32 texture_aspect_ratio(Renderer *renderer, Texture *texture);

// Mesh API
Mesh *new_mesh(Renderer *renderer, v3 position, i64 quad_count);
MeshQuad *push_quad(Mesh *mesh, v3 positions[4], v3 normals[4], v4 color, v2 uvs[4], v2 normal_uvs[4]);
void reset_mesh(Mesh *mesh);
void upload_mesh(Mesh *mesh);

bool init_frame_buffer(FrameBuffer *frame_buffer, i64 options);

bool init_shader(Shader *shader, string debug_name, string vertex_shader_path, string fragment_shader_path);
void assign_texture_slot(Shader *shader, string texture_name, i32 slot);

v2 screen_position_to_world_position(v2 screen_position, Camera camera, Window *window);
v2 screen_position_to_ndc(v2 screen_position, Window *window);

m4 get_view_matrix(Camera camera);
m4 get_projection_matrix(Camera camera, f32 aspect);
m4 get_projection_matrix_ortho(Camera camera, f32 aspect);

v4 alpha(v4 base, f32 alpha);
v4 brightness(v4 base, f32 brightness);

void clear_gl_errors();
bool check_gl_errors();

void opengl_error_callback(GLenum source, GLenum type, u32 id, GLenum severity, i32 length, const char *message, const void *user_param);

void print(v2 vector);
void print(v3 vector);
void print(v4 vector);

v3 get_forward_direction(Camera camera) {
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

v3 get_right_direction(Camera camera) {
    return HMM_Cross(get_up_direction(camera), get_forward_direction(camera));
}

v3 get_up_direction(Camera camera) {
    return {0, 1, 0};
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

void use_shader(Shader shader) {
    glUseProgram(shader.id);
}

void set_uniform_m4(Shader shader, string name, m4 *matrix) {
    glUniformMatrix4fv(
        glGetUniformLocation(shader.id, name.c()),
        1,
        false,
        (f32 *) &matrix->Columns[0]
    );
}

void set_uniform_v3(Shader shader, string name, v3 vector) {
    glUniform3f(
        glGetUniformLocation(shader.id, name.c()),
        vector.x, vector.y, vector.z 
    );
}

void set_uniform_v4(Shader shader, string name, v4 vector) {
    glUniform4f(
        glGetUniformLocation(shader.id, name.c()),
        vector.x, vector.y, vector.z, vector.w
    );
}

bool init_renderer(Renderer *renderer, Window *window) {
    renderer->quads = new_fixed_array<Quad>(MAX_QUADS);
    renderer->meshes = new_fixed_array<Mesh>(MAX_MESHES);

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

        renderer->vertex_array_id = vertex_array;
    }

    { // vertex buffer
        u32 vertex_buffer;
        glGenBuffers(1, &vertex_buffer);
        glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(Quad) * MAX_QUADS, renderer->quads.slice.ptr, GL_DYNAMIC_DRAW);

        renderer->vertex_buffer_id = vertex_buffer;
    }

    { // index buffer
        const i64 index_buffer_length = MAX_QUADS * 6;
        Slice<u32> indices = mem_alloc<u32>(index_buffer_length);

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

        renderer->index_buffer_id = index_buffer;

        mem_free(indices);
    }

    { // vertex attributes
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *) offsetof(Vertex, position));   // position
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *) offsetof(Vertex, colour));     // colour
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *) offsetof(Vertex, uv));         // uv
        glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *) offsetof(Vertex, normal_uv));  // normal_uv
        glVertexAttribIPointer(4, 1, GL_INT, sizeof(Vertex), (void *) offsetof(Vertex, draw_type));             // draw_type

        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glEnableVertexAttribArray(2);
        glEnableVertexAttribArray(3);
        glEnableVertexAttribArray(4);

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

    { // init frame buffers
         renderer->g_buffer = FrameBuffer {
            .width =  window->width,
            .height =  window->height
        };
    
        ok = init_frame_buffer(&renderer->g_buffer, FB_POSITION_ATTACHMENT | FB_NORMAL_ATTACHMENT | FB_ALBEDO_ATTACHMENT | FB_DEPTH_ATTACHMENT);
        if (!ok) {
            printf("failed to init default frame buffer\n");
            return false;
        }

         renderer->sun_frame_buffer = FrameBuffer {
            .width =  window->width,
            .height =  window->height
        };
    
        ok = init_frame_buffer(&renderer->sun_frame_buffer, FB_DEPTH_ATTACHMENT | FB_DISABLE_DRAW_BUFFER | FB_DISABLE_READ_BUFFER);
        if (!ok) {
            printf("failed to init sun frame buffer\n");
            return false;
        }
    }

    { // load default normal texture
        Texture *texture = load_texture(renderer, "resources/textures/defaults/normal.png");
        if (texture == NULL) {
            printf("failed to load default texture\n");
            return false;
        }

        renderer->default_normal = texture;
    }

    return true;
}

bool load_shaders(Renderer *renderer) {
    bool ok = init_shader(&renderer->default_shader, "Default shader", "resources/shaders/default_vertex.shader", "resources/shaders/default_fragment.shader");
    if (!ok) {
        printf("Error when creating default shader program\n");
        return false;
    }

    assign_texture_slot(&renderer->default_shader, "atlas_texture", 0);
    assign_texture_slot(&renderer->default_shader, "font_texture", 1);

    ok = init_shader(&renderer->geometry_shader, "Geometry shader", "resources/shaders/geometry_vertex.shader", "resources/shaders/geometry_fragment.shader");
    if (!ok) {
        printf("Error when creating mesh shader program\n");
        return false;
    }

    assign_texture_slot(&renderer->geometry_shader, "atlas_texture", 0);
    assign_texture_slot(&renderer->geometry_shader, "shadow_map", 1);

    ok = init_shader(&renderer->post_processing_shader, "Post processing shader", "resources/shaders/default_vertex.shader", "resources/shaders/post_processing_fragment.shader");
    if (!ok) {
        printf("Error when creating post processing shader program\n");
        return false;
    }

    assign_texture_slot(&renderer->post_processing_shader, "scene_texture", 0);

    ok = init_shader(&renderer->sun_shader, "Sun shader", "resources/shaders/sun_vertex.shader", "resources/shaders/sun_fragment.shader");
    if (!ok) {
        printf("Error when creating sun shader program\n");
        return false;
    }

    return true;
}

void delete_shaders(Renderer *renderer) {
    glDeleteProgram(renderer->default_shader.id);
    glDeleteProgram(renderer->geometry_shader.id);
    glDeleteProgram(renderer->post_processing_shader.id);
    glDeleteProgram(renderer->sun_shader.id);
}

Sprite *load_sprite(Renderer *renderer, string albedo_path, string normal_path) {
    Texture *albedo = load_texture(renderer, albedo_path);
    Texture *normal = NULL;

    if (normal_path.len != 0) {
        normal = load_texture(renderer, normal_path);
    }

    Sprite *sprite = push(&renderer->sprites);
    *sprite = Sprite {
        .albedo = albedo,
        .normal = normal
    };

    return sprite;
}


Sprite *load_animated_sprite(Renderer *renderer, string albedo_path, i64 cell_count, f32 animation_length) {
    Texture *albedo = load_animated_texture(renderer, albedo_path, cell_count, animation_length);

    Sprite *sprite = push(&renderer->sprites);
    *sprite = Sprite {
        .albedo = albedo,
        .normal = NULL
    };

    return sprite;
}

Texture *load_texture(Renderer *renderer, string path) {
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

    printf("Loaded texture with path \"%s\" [%dx%d] %d bytes\n", path.c(), width, height, width * height * channels);

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

Texture *load_animated_texture(Renderer *renderer, string path, i64 cell_count, f32 animation_length) {
    Texture *texture = load_texture(renderer, path);
    if(texture == NULL) {
        return NULL;
    }

    texture->type = TextureType::ANIMATED;
    texture->animation_length = animation_length;
    texture->sub_textures = mem_alloc<Texture>(cell_count);

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

bool load_font(Renderer *renderer, string path, i64 width, i64 height, f32 pixel_height) {
    Font font = Font{
        .width = width,
        .height = height,
        .characters = {},
        .bitmap_data = (u8 *) malloc(width * height),
    };

    Slice<u8> font_data = read_file(path.c());
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

void new_frame(Renderer *renderer, Window *window, Camera camera) {
    reset(&renderer->quads);

    renderer->view_matrix = get_view_matrix(camera);
    renderer->projection_matrix = get_projection_matrix(camera, (f32) window->width / (f32) window->height);
    renderer->projection_matrix_ortho = get_projection_matrix_ortho(camera, (f32) window->width / (f32) window->height);

    glClearColor(
        renderer->clear_colour.r,
        renderer->clear_colour.g,
        renderer->clear_colour.b,
        renderer->clear_colour.a
    );

    new_imgui_frame();
}

void draw_frame(Renderer *renderer, Window *window) {
    m4 sun_space = {};

    // scene render from sun POV
    for (Mesh &mesh : renderer->meshes) {
        glBindFramebuffer(GL_FRAMEBUFFER, renderer->sun_frame_buffer.id);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glViewport(0, 0, window->width, window->height);

        m4 model_matrix = HMM_M4D(1.0f);
        model_matrix = HMM_MulM4(model_matrix, HMM_Translate(mesh.position));

        m4 view = HMM_LookAt_LH(
            renderer->sun_position, 
            {0, 0, 0}, 
            {0, 1, 0}
        );

        f32 aspect = (f32) window->width / (f32) window->height;
        f32 orthographic_size = 100;

        m4 projection = HMM_Orthographic_LH_NO(
            -orthographic_size * aspect,  // left
             orthographic_size * aspect,  // right
            -orthographic_size,           // bottom
             orthographic_size,           // top
             0.1, 
             500 
        );

        sun_space = HMM_MulM4(projection, view);

        use_shader(renderer->sun_shader);
        set_uniform_m4(renderer->sun_shader, "model", &model_matrix);
        set_uniform_m4(renderer->sun_shader, "sun_space", &sun_space);

        GL_CALL(glBindVertexArray(mesh.vertex_array_id));
        GL_CALL(glDrawElements(GL_TRIANGLES, 6 * mesh.quads.len, GL_UNSIGNED_INT, 0));

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    // scene render
    for (Mesh &mesh : renderer->meshes) {
        glBindFramebuffer(GL_FRAMEBUFFER, renderer->g_buffer.id);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glViewport(0, 0, window->width, window->height);

        m4 model_matrix = HMM_M4D(1.0f);
        model_matrix = HMM_MulM4(model_matrix, HMM_Translate(mesh.position));

        use_shader(renderer->geometry_shader);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, renderer->atlas_texture_id);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, renderer->sun_frame_buffer.depth_attachment);

        set_uniform_m4(renderer->geometry_shader, "model", &model_matrix);
        set_uniform_m4(renderer->geometry_shader, "view", &renderer->view_matrix);
        set_uniform_m4(renderer->geometry_shader, "projection", &renderer->projection_matrix);
        set_uniform_m4(renderer->geometry_shader, "sun_space", &sun_space);

        set_uniform_v3(renderer->geometry_shader, "ambient_light", renderer->ambient_light);
        set_uniform_v3(renderer->geometry_shader, "sun_position", renderer->sun_position);
        set_uniform_v3(renderer->geometry_shader, "sun_colour", renderer->sun_colour);

        GL_CALL(glBindVertexArray(mesh.vertex_array_id));
        GL_CALL(glDrawElements(GL_TRIANGLES, 6 * mesh.quads.len, GL_UNSIGNED_INT, 0));
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    { // imediete mode quads
        glBindFramebuffer(GL_FRAMEBUFFER, renderer->g_buffer.id);

        glBindBuffer(GL_ARRAY_BUFFER, renderer->vertex_buffer_id);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(Quad) * renderer->quads.len, renderer->quads.slice.ptr);
    
        glBindVertexArray(renderer->vertex_array_id);

        use_shader(renderer->default_shader);
    
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, renderer->atlas_texture_id);
    
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, renderer->font_texture_id);
    
        glDrawElements(GL_TRIANGLES, 6 * renderer->quads.len, GL_UNSIGNED_INT, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    { // post processing
        reset(&renderer->quads);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glViewport(0, 0, window->width, window->height);

        Quad *quad = push_screen_quad(renderer, WHITE);
    
        glBindBuffer(GL_ARRAY_BUFFER, renderer->vertex_buffer_id);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(Quad) * renderer->quads.len, renderer->quads.slice.ptr);
        glBindVertexArray(renderer->vertex_array_id);
 
        use_shader(renderer->post_processing_shader);
 
        // set the input texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, renderer->g_buffer.albedo_attachment);

        glDrawElements(GL_TRIANGLES, 6 * renderer->quads.len, GL_UNSIGNED_INT, 0);
    }

    draw_imgui_frame();
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

void draw_rectangle(Renderer *renderer, v3 position, v2 size, v4 color) {
    v2 uvs[4] = {
        {0, 1},
        {1, 1},
        {1, 0},
        {0, 0},
    };

    push_quad(renderer, position, size, {}, color, uvs, {}, DrawType::RECTANGLE);
}

void draw_circle(Renderer *renderer, v3 position, f32 radius, v4 color) {
    v2 size = {radius * 2, radius * 2};

    v2 uvs[4] = {
        {0, 1},
        {1, 1},
        {1, 0},
        {0, 0},
    };

    push_quad(renderer, position, size, {}, color, uvs, {}, DrawType::CIRCLE);
}

void draw_cube(Renderer *renderer, v3 position, v3 size, v3 rotation, v4 color) {
    v2 uvs[4] = {
        {0, 1},
        {1, 1},
        {1, 0},
        {0, 0},
    };

    push_quad(renderer, position + (v3{-0.5,    0,    0} * size), {size.z, size.y}, v3{  0, -90, 0}, color, uvs, renderer->default_normal->uvs, DrawType::RECTANGLE); // left
    push_quad(renderer, position + (v3{   0,    0, -0.5} * size), {size.x, size.y}, v3{  0,   0, 0}, color, uvs, renderer->default_normal->uvs, DrawType::RECTANGLE); // front
    push_quad(renderer, position + (v3{ 0.5,    0,    0} * size), {size.z, size.y}, v3{  0,  90, 0}, color, uvs, renderer->default_normal->uvs, DrawType::RECTANGLE); // right
    push_quad(renderer, position + (v3{   0,  0.5,    0} * size), {size.x, size.z}, v3{-90,   0, 0}, color, uvs, renderer->default_normal->uvs, DrawType::RECTANGLE); // top
    push_quad(renderer, position + (v3{   0, -0.5,    0} * size), {size.x, size.z}, v3{ 90,   0, 0}, color, uvs, renderer->default_normal->uvs, DrawType::RECTANGLE); // bottom
    push_quad(renderer, position + (v3{   0,    0,  0.5} * size), {size.x, size.y}, v3{  0, 180, 0}, color, uvs, renderer->default_normal->uvs, DrawType::RECTANGLE); // back
}

void draw_sprite(Renderer *renderer, Sprite *sprite, v3 position, v2 size, f32 rotation, v4 color) {
    v2 *normal_uvs = NULL; // need to use pointer here because stupid C reasons

    if (sprite->normal == NULL) {
        normal_uvs = renderer->default_normal->uvs;
    } else  {
        normal_uvs = sprite->normal->uvs;
    }

    push_quad(renderer, position, size, {0, 0, rotation}, color, sprite->albedo->uvs, normal_uvs, DrawType::TEXTURE);
}

void draw_animated_sprite(Renderer *renderer, Sprite *sprite, f32 time_in_animation, v3 position, v2 size, f32 rotation, v4 color) {
    f32 animation_progress = time_in_animation / sprite->albedo->animation_length;
    animation_progress = clamp(0, animation_progress, 1);

    i64 sub_texture_count = sprite->albedo->sub_textures.len;
    i64 sub_texture_index = (i64)(animation_progress * sub_texture_count);
    
    if (sub_texture_index >= sub_texture_count) {
        sub_texture_index = sub_texture_count - 1;
    }

    Texture *sub_texture = &sprite->albedo->sub_textures[sub_texture_index];

    push_quad(renderer, position, size, {0, 0, rotation}, color, sub_texture->uvs, renderer->default_normal->uvs, DrawType::TEXTURE);
}

void draw_texture(Renderer *renderer, Texture *texture, Texture *normal_texture, v3 position, v2 size, f32 rotation, v4 color) {
    assert(texture->type == TextureType::SINGLE);

    push_quad(renderer, position, size, {0, 0, rotation}, color, texture->uvs, normal_texture->uvs, DrawType::TEXTURE);
}

void draw_animated_texture(Renderer *renderer, Texture *texture, f32 time_in_animation, v3 position, v2 size, f32 rotation, v4 color) {
    f32 animation_progress = time_in_animation / texture->animation_length;
    animation_progress = clamp(0, animation_progress, 1);

    i64 sub_texture_count = texture->sub_textures.len;
    i64 sub_texture_index = (i64)(animation_progress * sub_texture_count);
    
    if (sub_texture_index >= sub_texture_count) {
        sub_texture_index = sub_texture_count - 1;
    }

    Texture *sub_texture = &texture->sub_textures[sub_texture_index];
    push_quad(renderer, position, size, {0, 0, rotation}, color, sub_texture->uvs, {}, DrawType::TEXTURE);
}

void draw_text(Renderer *renderer, string text, v3 position, f32 font_size, v4 color) {
    if (text.len == 0) {
        return;
    }

    struct Glyph {
        v2 position;
        v2 size;
        v2 uvs[4];
    };

    Slice<Glyph> glyphs = mem_alloc<Glyph>(text.len);

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

        push_quad(renderer, v3{quad_centered_position.x, quad_centered_position.y, position.z}, scaled_size, {}, color, glyph->uvs, {}, DrawType::TEXT);
   }

    mem_free(glyphs);
}

void draw_light(Renderer *renderer, v3 position, f32 radius, v4 colour, f32 intensity) {
    Light *light = push(&renderer->lights);

    m4 view_projection_matrix = HMM_MulM4(renderer->projection_matrix, renderer->view_matrix);

    // light data is sent to the GPU in NDC so using view projection 
    // matrix for the transformation
    *light = Light {
        .position = HMM_MulM4V4(view_projection_matrix, v4{position.x, position.y, position.z, 1}).xy,
        .radius = length(HMM_MulM4V4(view_projection_matrix, {radius, 0, 0, 0}).xy) * 2,
        .colour = colour,
        .intensity = intensity
    };
}

Quad *push_quad(Renderer *renderer, v3 position, v2 size, v3 rotation, v4 color, v2 uvs[4], v2 normal_uvs[4], DrawType draw_type) {
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
              
    m4 mvp_matrix = HMM_MulM4(HMM_MulM4(renderer->projection_matrix, renderer->view_matrix), model_matrix);

    Quad *quad = push(&renderer->quads);

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

    if (normal_uvs != NULL) {
        quad->vertices[0].normal_uv = normal_uvs[0];
        quad->vertices[1].normal_uv = normal_uvs[1];
        quad->vertices[2].normal_uv = normal_uvs[2];
        quad->vertices[3].normal_uv = normal_uvs[3];
    }

    quad->vertices[0].draw_type = (i32) draw_type;
    quad->vertices[1].draw_type = (i32) draw_type;
    quad->vertices[2].draw_type = (i32) draw_type;
    quad->vertices[3].draw_type = (i32) draw_type;

    return quad;
}

Quad *push_screen_quad(Renderer *renderer, v4 color) {
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

    Quad *quad = push(&renderer->quads);

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

    quad->vertices[0].normal_uv = uvs[0];
    quad->vertices[1].normal_uv = uvs[1];
    quad->vertices[2].normal_uv = uvs[2];
    quad->vertices[3].normal_uv = uvs[3];

    quad->vertices[0].draw_type = 0;
    quad->vertices[1].draw_type = 0;
    quad->vertices[2].draw_type = 0;
    quad->vertices[3].draw_type = 0;

    return quad;
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

Mesh *new_mesh(Renderer *renderer, v3 position, i64 quad_count) {
    Mesh *mesh = push(&renderer->meshes);
    *mesh = Mesh {
        .position = position,
        .quads = new_fixed_array<MeshQuad>(quad_count),
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
        glBufferData(GL_ARRAY_BUFFER, sizeof(MeshQuad) * mesh->quads.slice.len, mesh->quads.slice.ptr, GL_DYNAMIC_DRAW);

        mesh->vertex_buffer_id = vertex_buffer;
    }

    { // index buffer
        const i64 index_buffer_length = mesh->quads.slice.len * 6;
        Slice<u32> indices = mem_alloc<u32>(index_buffer_length);

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

        mesh->index_buffer_id = index_buffer;

        mem_free(indices);
    }

    { // vertex attributes
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void *) offsetof(MeshVertex, position));   // position
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void *) offsetof(MeshVertex, normal));     // normal
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void *) offsetof(MeshVertex, colour));     // colour
        glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void *) offsetof(MeshVertex, uv));         // uv
        glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void *) offsetof(MeshVertex, normal_uv));  // normal_uv

        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glEnableVertexAttribArray(2);
        glEnableVertexAttribArray(3);
        glEnableVertexAttribArray(4);
    }

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    return mesh;
}

MeshQuad *push_quad(Mesh *mesh, v3 positions[4], v3 normals[4], v4 color, v2 uvs[4], v2 normal_uvs[4]) {
    MeshQuad *quad = push(&mesh->quads);

    quad->vertices[0].position = positions[0];
    quad->vertices[1].position = positions[1];
    quad->vertices[2].position = positions[2];
    quad->vertices[3].position = positions[3];

    quad->vertices[0].normal = normals[0];
    quad->vertices[1].normal = normals[1];
    quad->vertices[2].normal = normals[2];
    quad->vertices[3].normal = normals[3];

    quad->vertices[0].colour = color;
    quad->vertices[1].colour = color;
    quad->vertices[2].colour = color;
    quad->vertices[3].colour = color;

    quad->vertices[0].uv = uvs[0];
    quad->vertices[1].uv = uvs[1];
    quad->vertices[2].uv = uvs[2];
    quad->vertices[3].uv = uvs[3];

    quad->vertices[0].normal_uv = normal_uvs[0];
    quad->vertices[1].normal_uv = normal_uvs[1];
    quad->vertices[2].normal_uv = normal_uvs[2];
    quad->vertices[3].normal_uv = normal_uvs[3];

#if 0
    for (i64 i = 0; i < 4; i++) {
        quad->vertices[i].position = v4{positions[i], 1};
        quad->vertices[i].colour = color;
        quad->vertices[i].uv = uvs[i];
        quad->vertices[i].normal_uv = normal_uvs[i];
        quad->vertices[i].draw_type = (i32) draw_type;
    }
#endif

    return quad;
}

void reset_mesh(Mesh *mesh) {
    reset(&mesh->quads);
}

void upload_mesh(Mesh *mesh) {
    glBindBuffer(GL_ARRAY_BUFFER, mesh->vertex_buffer_id);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(Quad) * mesh->quads.len, mesh->quads.slice.ptr);
}

bool init_frame_buffer(FrameBuffer *frame_buffer, i64 options) {
    glCreateFramebuffers(1, &frame_buffer->id);
    glBindFramebuffer(GL_FRAMEBUFFER, frame_buffer->id);

    StackArray<GLenum, 3> draw_buffers = {};

    if (BIT_SET(options, FB_POSITION_ATTACHMENT)) {
        glCreateTextures(GL_TEXTURE_2D, 1, &frame_buffer->position_attachment);
        glBindTexture(GL_TEXTURE_2D, frame_buffer->position_attachment);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, frame_buffer->width, frame_buffer->height, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); 

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, frame_buffer->position_attachment, 0);
        append(&draw_buffers, (GLenum) GL_COLOR_ATTACHMENT0);
    }

    if (BIT_SET(options, FB_NORMAL_ATTACHMENT)) {
        glCreateTextures(GL_TEXTURE_2D, 1, &frame_buffer->normals_attachment);
        glBindTexture(GL_TEXTURE_2D, frame_buffer->normals_attachment);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, frame_buffer->width, frame_buffer->height, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, frame_buffer->normals_attachment, 0);
        append(&draw_buffers, (GLenum) GL_COLOR_ATTACHMENT1);
    }

    if (BIT_SET(options, FB_ALBEDO_ATTACHMENT)) {
        glCreateTextures(GL_TEXTURE_2D, 1, &frame_buffer->albedo_attachment);
        glBindTexture(GL_TEXTURE_2D, frame_buffer->albedo_attachment);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, frame_buffer->width, frame_buffer->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, frame_buffer->albedo_attachment, 0);
        append(&draw_buffers, (GLenum) GL_COLOR_ATTACHMENT2);
    }

    if (BIT_SET(options, FB_DEPTH_ATTACHMENT)) {
        glCreateTextures(GL_TEXTURE_2D, 1, &frame_buffer->depth_attachment);
        glBindTexture(GL_TEXTURE_2D, frame_buffer->depth_attachment);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, frame_buffer->width, frame_buffer->height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, frame_buffer->depth_attachment, 0);
    }

    if (BIT_SET(options, FB_DISABLE_READ_BUFFER)) {
        glReadBuffer(GL_NONE);
    }

    if (BIT_SET(options, FB_DISABLE_DRAW_BUFFER)) {
        glDrawBuffer(GL_NONE);
    }

    if (draw_buffers.len > 0) {
        ASSERT(BIT_SET(options, FB_DISABLE_DRAW_BUFFER) == false);

        // when using more then one colour attachment, need to set all colour buffers the
        // frame buffer can write too, if not the normal buffer will not be write too
        glDrawBuffers(draw_buffers.len, draw_buffers.data);
    }

    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        printf("error when createing frame buffer, was not complete\n");
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

bool init_shader(Shader *shader, string debug_name, string vertex_shader_path, string fragment_shader_path) {
    const i64 buffer_size = 640;
    i32 compile_status = 0;
    i32 link_status = 0;
    char error_buffer[buffer_size];
    
    string vertex_shader_source = read_file(vertex_shader_path);
    if (vertex_shader_source.len == 0) {
        printf("%s: failed to load vertex shader file\n", debug_name.c());
        return false;
    }

    string fragment_shader_source = read_file(fragment_shader_path);
    if (fragment_shader_source.len == 0) {
        printf("%s: failed to load default fragment shader file\n", debug_name.c());
        return false;
    }

    u32 vertex_shader = glCreateShader(GL_VERTEX_SHADER);

    glShaderSource(vertex_shader, 1, (char **) &vertex_shader_source.ptr, NULL);
    glCompileShader(vertex_shader);

    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &compile_status);
    if (compile_status == 0) {
        glGetShaderInfoLog(vertex_shader, buffer_size, nullptr, &error_buffer[0]);
        printf("%s: failed to compile vertex shader: %s\n", debug_name.c(), error_buffer);
        return false;
    }

    u32 fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

    glShaderSource(fragment_shader, 1, (char**) &fragment_shader_source.ptr, NULL);
    glCompileShader(fragment_shader);

    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &compile_status);
    if (compile_status == 0) {
        glGetShaderInfoLog(fragment_shader, buffer_size, nullptr, &error_buffer[0]);
        printf("%s: failed to compile fragment shader: %s\n", debug_name.c(), error_buffer);
        return false;
    }

    u32 shader_program = glCreateProgram();

    glAttachShader(shader_program, vertex_shader);
    glAttachShader(shader_program, fragment_shader);
    glLinkProgram(shader_program); 

    glGetProgramiv(shader_program, GL_LINK_STATUS, &link_status);

    if (link_status == 0) {
        glGetProgramInfoLog(shader_program, buffer_size, nullptr, &error_buffer[0]);
        printf("%s: failed to link shader program: %s\n", debug_name.c(), error_buffer);
        return false;
    }
 
    shader->id = shader_program; 
    shader->debug_name = debug_name;

    printf("Compiled and linked %s\n", debug_name.c());

    return true;
}

void assign_texture_slot(Shader *shader, string texture_name, i32 slot) {
    glUseProgram(shader->id);
    glUniform1i(glGetUniformLocation(shader->id, texture_name.c()), slot);
    glUseProgram(0);
}

v2 screen_position_to_world_position(v2 screen_position, Camera camera, Window *window) {
    // pretty muched copied from odin engine,
    // haven't tweaked it because I am just happy 
    // it works. Maybe can find a way to not pass
    // in window or camera
    // - 28/05/25
    
    v2 ndc = screen_position_to_ndc(screen_position, window);
    f32 aspect_ratio = (f32) window->width / (f32) window->height;

    m4 inverse_vp = HMM_InvGeneralM4(get_projection_matrix(camera, aspect_ratio) * get_view_matrix(camera));

    v4 world_position = inverse_vp * v4{ndc.x, ndc.y, 0, 1};
    world_position /= world_position.w;

    return v2{world_position.x, world_position.y};
}

v2 screen_position_to_ndc(v2 screen_position, Window *window) {
    return {
        (screen_position.x / window->width) * 2 - 1,
        (screen_position.y / window->height) * 2 - 1,
    };
}

m4 get_view_matrix(Camera camera) {
    v3 target = {};

    if (camera.mode == CameraMode::FIRST_PERSON) {
        target = camera.position + get_forward_direction(camera);
    } else {
        target = camera.target;
    }

    // FIXME: having the up always be y = 1 is probably wrong - 04/06/25
    m4 view_matrix = HMM_LookAt_LH(
        camera.position, 
        target, 
        {0, 1, 0}
    );

    return view_matrix;
}

m4 get_projection_matrix(Camera camera, f32 aspect) {
    return HMM_Perspective_LH_NO(camera.fov * HMM_DegToRad, aspect, camera.near_plane, camera.far_plane);
}

m4 get_projection_matrix_ortho(Camera camera, f32 aspect) {
    return HMM_Orthographic_LH_NO(
        -camera.orthographic_size * aspect,  // left
         camera.orthographic_size * aspect,  // right
        -camera.orthographic_size,           // bottom
         camera.orthographic_size,           // top
         camera.near_plane, 
         camera.far_plane 
    );
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

////////////////////////////////////////////////////////////////////////////
//////////////////////////////// @sound ////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
enum SoundHandle {
    SH_DASH,
    SH_COUNT__
};

struct SoundEngine {
    ma_engine engine;

    StackArray<ma_sound, SH_COUNT__> sounds;
};

bool init_sound_engine(SoundEngine *sound_engine);
bool load_sounds(SoundEngine *sound_engine);
void play_sound(SoundEngine *sound_engine, SoundHandle handle);

string sound_path(SoundHandle handle);

bool init_sound_engine(SoundEngine *sound_engine) {
    ma_result result = ma_engine_init(NULL, &sound_engine->engine);
    if (result != MA_SUCCESS) {
        printf("failed to init sound engine\n");
        return false;
    }

    return true;
}

bool load_sounds(SoundEngine *sound_engine) {
    for (i64 i = 0; i < sound_engine->sounds.size; i++) {
        SoundHandle handle = (SoundHandle) i;

        string path = sound_path(handle);
        ma_sound *sound = &sound_engine->sounds[i];

        ma_result result = ma_sound_init_from_file(&sound_engine->engine, path.c(), 0, NULL, NULL, sound);
        if (result != MA_SUCCESS) {
            printf("failed to load sound: %s\n", path.c());
            return false;
        }

        printf("Loaded sound with path \"%s\"\n", path.c());
    }

    return true; 
}

void play_sound(SoundEngine *sound_engine, SoundHandle handle) {
    ma_sound *sound = &sound_engine->sounds[handle];
    ma_sound_start(sound);
}

string sound_path(SoundHandle handle) {
    switch (handle) {
        case SH_DASH: 
            return "resources/sounds/dash.wav";
        default: 
            assert(0);
    }

    return {};
}

#endif
