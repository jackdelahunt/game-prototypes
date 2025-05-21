#ifndef ENGINE_CPP
#define ENGINE_CPP

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libs/libs.h"

/////////////////////////////////////////////////////////////////////////////
//////////////////////////////// @core //////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
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

template <typename T>
struct Slice { // TODO: do safety checks in slices
    T *ptr;
    i64 len;

    Slice() {}

    Slice(T *data, i64 len) { // C++ sucks
        this->ptr = data;
        this->len = len;
    }

    Slice(const char *c_string) {
        this->ptr = (T *) c_string;
        this->len = strlen(c_string);
    }

    T& operator[](i64 index) {
        return this->ptr[index];
    }

    Slice<T> slice(i64 start, i64 end) {
        return Slice<T>(this->ptr + start, end - start);
    }

    const char *c() {
        return (const char *) this->ptr;
    }
};

typedef Slice<u8> string;

template <typename T>
Slice<T> make_slice(T *data, i64 len) {
    return Slice<T>(data, len);
}

template <typename T>
Slice<T> mem_alloc(i64 len) {
    T *ptr = (T *) malloc(len * sizeof(T));
    return make_slice(ptr, len);
}

template <typename T>
void mem_free(Slice<T> slice) {
    free(slice.ptr);
}

template <typename T, i64 N>
struct Array {
    T data[N];
    i64 size = N;
    i64 len;

    T& operator[](i64 index) {
        return this->data[index];
    }
};

template <typename T, i64 N>
void append(Array<T, N> *array, T value) {
    assert(array->len < N);

    array->data[array->len] = value;
    array->len += 1;
}

template <typename T, i64 N>
T* push(Array<T, N> *array) {
    assert(array->len < N);

    T *ptr = &array->data[array->len];
    array->len++;
    return ptr;
}

template <typename T, i64 N>
void reset(Array<T, N> *array) {
    array->len = 0;
}

template <typename T, i64 N>
void swap_remove(Array<T, N> *array, i64 index) {
    assert(index < array->len);

    array->data[index] = array->data[array->len - 1];
    array->len -= 1;
}

Slice<u8> read_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (file == nullptr) {
        return {};
    }

    fseek(file, 0, SEEK_END);
    i64 file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    u8 *data = (u8 *) malloc(file_size + 1);
    fread(data, file_size, 1, file);
    fclose(file);
    
    data[file_size] = 0; // null terminate

    return make_slice(data, file_size);
}

// 0 -> 1
f32 rand_f32() {
    return (f32) rand() / (f32) RAND_MAX;
}

// -1 -> 1
f32 rand_f32_negative() {
    return (rand_f32() * 2.0f) - 1.0f;
}

/////////////////////////////////////////////////////////////////////////////
//////////////////////////////// @window ////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
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

struct {
    v2 position;
} MOUSE;

bool init_window(i32 width, i32 height, string title);
void swap_buffers(Window *window);
void glfw_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
void glfw_mouse_move_callback(GLFWwindow* window, f64 x, f64 y);
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

    glfwSetWindowUserPointer(window->glfw_window, window);

    glfwSwapInterval(1);

    glfwSetKeyCallback(window->glfw_window, glfw_key_callback);
    glfwSetCursorPosCallback(window->glfw_window, glfw_mouse_move_callback);

    return true;
}

void swap_buffers(Window *window) {
    glfwSwapBuffers(window->glfw_window);
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

void glfw_mouse_move_callback(GLFWwindow* window, f64 x, f64 y) {
    // changing y position from glfw so the bottem left is the origin
    // by default glfw is top left as origin - 18/03/25
    Window *win_ptr = (Window *) glfwGetWindowUserPointer(window);

    f32 x_32 = (f32) x;
    f32 y_32 = (f32) y;

    MOUSE.position = v2{
        x_32,
        (-y_32) + win_ptr->height,
    };
}

/////////////////////////////////////////////////////////////////////////////
//////////////////////////////// @renderer //////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
#define MAX_QUADS 2000
#define MAX_LIGHTS 20

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
};

struct Camera {
    v3 position;
    f32 orthographic_size;
    f32 near_plane;
    f32 far_plane;
};

enum TextureHandle {
    TH_FENCE,
    TH_LAMP,
    TH_ROCK_1,
    TH_ROCK_2,
    TH_SHOP,
    TH_BACKGROUND_LAYER_1,
    TH_BACKGROUND_LAYER_2,
    TH_BACKGROUND_LAYER_3,
    TH_FLOOR,
    TH_COUNT__
};

struct Texture {
    TextureHandle handle;
    i64 width;
    i64 height;
    v2 uvs[4];
    u8 *data;
};

struct Atlas {
    i64 width;
    i64 height;
    u8 *data;
};

struct Font {
    i64 width;
    i64 height;
    Array<stbtt_bakedchar, 96> characters;
    u8 *bitmap_data;
};

struct Renderer {
    v4 global_light;
    v4 light_colour;

    Array<Quad, MAX_QUADS> quads;
    Array<Light, MAX_LIGHTS> lights;

    m4 view_projection_matrix;

    Array<Texture, TH_COUNT__> textures;
    Atlas atlas;

    Font font;

    u32 vertex_array_id;
    u32 vertex_buffer_id;
    u32 index_buffer_id;

    u32 shader_program_id;
    u32 light_shader_program_id;
    u32 blur_shader_program_id;

    u32 atlas_texture_id;
    u32 font_texture_id;
};

struct FrameBuffer {
    u32 id;
    u32 width;
    u32 height;

    u32 colour_attachment;
    u32 depth_attachment;
};

v4 WHITE      = {1, 1, 1, 1};
v4 BLACK      = {0, 0, 0, 1};
v4 RED      = {1, 0, 0, 1};
v4 GREEN    = {0, 1, 0, 1};
v4 BLUE     = {0, 0, 1, 1};

bool init_renderer(Renderer *renderer, Window *window);
bool load_textures(Renderer *renderer);
u32 upload_texture_to_gpu(Renderer *renderer, i32 width, i32 height, u8 *data);
u32 upload_font_to_gpu(Renderer *renderer, i32 width, i32 height, u8 *data);
bool load_font(Renderer *renderer, string path, i64 width, i64 height, f32 pixel_height);

bool init_frame_buffer(FrameBuffer *frame_buffer);

void draw_rectangle(Renderer *renderer, v3 position, v2 size, v4 color);
void draw_circle(Renderer *renderer, v3 position, f32 radius, v4 color);
void draw_texture(Renderer *renderer, TextureHandle handle, v3 position, v2 size, f32 rotation, v4 color);
void draw_text(Renderer *renderer, string text, v3 position, f32 font_size, v4 color);
void draw_light(Renderer *renderer, v3 position);
void new_frame(Renderer *renderer, Window *window, Camera camera);
void draw_frame(Renderer *renderer, Window *window);
Quad *push_quad(Renderer *renderer, v3 position, v2 size, f32 rotation, v4 color, v2 uvs[4], i32 draw_type);

v2 screen_position_to_world_position(v2 screen_position, Camera camera, Window *window);
v2 screen_position_to_ndc(v2 screen_position, Window *window);

m4 get_view_matrix(Camera camera);
m4 get_projection_matrix(Camera camera, f32 aspect);

f32 texture_aspect_ratio(Renderer *renderer, TextureHandle handle);
const char *texture_path(TextureHandle handle);

void opengl_error_callback(GLenum source, GLenum type, u32 id, GLenum severity, i32 length, const char *message, const void *user_param);

v4 alpha(v4 base, f32 alpha);

bool init_renderer(Renderer *renderer, Window *window) {
    { // init opengl
        GLenum result = glewInit();
        if (result != GLEW_OK) {
            return false;
        }

        glDebugMessageCallback(opengl_error_callback, NULL);

        // alpha blend settings
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // depth buffer settings
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);


        float f = 0.1f;
        glClearColor(f, f, f, 1.0f);
    }

    { // init imgui
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
    
        ImGui::StyleColorsLight();
    
        ImGuiIO& io = ImGui::GetIO();
    
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    
        ImGui_ImplGlfw_InitForOpenGL(window->glfw_window, true);
        ImGui_ImplOpenGL3_Init("#version 460");
    }

    { // load and compile shaders
        const i64 buffer_size = 640;
        i32 compile_status = 0;
        i32 link_status = 0;
        char error_buffer[buffer_size];
    
        Slice<u8> vertex_shader_source = read_file("./resources/shaders/default_vertex.shader");
        if (vertex_shader_source.len == 0) {
            printf("failed to load vertex shader");
            return false;
        }

        Slice<u8> fragment_shader_source = read_file("./resources/shaders/default_fragment.shader");
        if (fragment_shader_source.len == 0) {
            printf("failed to load default fragment shader");
            return false;
        }

        Slice<u8> light_fragment_shader_source = read_file("./resources/shaders/lighting_fragment.shader");
        if (light_fragment_shader_source.len == 0) {
            printf("failed to load light shader");
            return false;
        }

        Slice<u8> blur_fragment_shader_source = read_file("./resources/shaders/blur_fragment.shader");
        if (blur_fragment_shader_source.len == 0) {
            printf("failed to load blur shader");
            return false;
        }

        u32 vertex_shader = glCreateShader(GL_VERTEX_SHADER);

        glShaderSource(vertex_shader, 1, (char **) &vertex_shader_source.ptr, NULL);
        glCompileShader(vertex_shader);

        glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &compile_status);
        if (compile_status == 0) {
            glGetShaderInfoLog(vertex_shader, buffer_size, nullptr, &error_buffer[0]);
            printf("failed to compile vertex shader: %s", error_buffer);
            return false;
        }

        u32 fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

        glShaderSource(fragment_shader, 1, (char**) &fragment_shader_source.ptr, NULL);
        glCompileShader(fragment_shader);

        glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &compile_status);
        if (compile_status == 0) {
            glGetShaderInfoLog(fragment_shader, buffer_size, nullptr, &error_buffer[0]);
            printf("failed to compile fragment shader: %s", error_buffer);
            return false;
        }

        u32 light_fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

        glShaderSource(light_fragment_shader, 1, (char**) &light_fragment_shader_source.ptr, NULL);
        glCompileShader(light_fragment_shader);

        glGetShaderiv(light_fragment_shader, GL_COMPILE_STATUS, &compile_status);
        if (compile_status == 0) {
            glGetShaderInfoLog(light_fragment_shader, buffer_size, nullptr, &error_buffer[0]);
            printf("failed to compile light shader: %s", error_buffer);
            return false;
        }

        u32 blur_fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

        glShaderSource(blur_fragment_shader, 1, (char**) &blur_fragment_shader_source.ptr, NULL);
        glCompileShader(blur_fragment_shader);

        glGetShaderiv(blur_fragment_shader, GL_COMPILE_STATUS, &compile_status);
        if (compile_status == 0) {
            glGetShaderInfoLog(blur_fragment_shader, buffer_size, nullptr, &error_buffer[0]);
            printf("failed to compile blur shader: %s", error_buffer);
            return false;
        }

        { // default shader program
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
    
            renderer->shader_program_id = shader_program;
    
            glUseProgram(shader_program);
            glUniform1i(glGetUniformLocation(shader_program, "atlas_texture"), 0);
            glUniform1i(glGetUniformLocation(shader_program, "font_texture"), 1);
        }

        { // light shader program
            u32 light_shader_program = glCreateProgram();
            glAttachShader(light_shader_program, vertex_shader);
            glAttachShader(light_shader_program, light_fragment_shader);
            glLinkProgram(light_shader_program);
    
            glGetProgramiv(light_shader_program, GL_LINK_STATUS, &link_status);
            if (link_status == 0) {
                glGetProgramInfoLog(light_shader_program, buffer_size, nullptr, &error_buffer[0]);
                printf("failed to link light shader program: %s", error_buffer);
                return false;
            }
    
            renderer->light_shader_program_id = light_shader_program;
    
            glUseProgram(light_shader_program);
            glUniform1i(glGetUniformLocation(light_shader_program, "scene_texture"), 0);
        }

        { // blur shader program
            u32 blur_shader_program = glCreateProgram();
            glAttachShader(blur_shader_program, vertex_shader);
            glAttachShader(blur_shader_program, blur_fragment_shader);
            glLinkProgram(blur_shader_program);
    
            glGetProgramiv(blur_shader_program, GL_LINK_STATUS, &link_status);
            if (link_status == 0) {
                glGetProgramInfoLog(blur_shader_program, buffer_size, nullptr, &error_buffer[0]);
                printf("failed to link blur shader program: %s", error_buffer);
                return false;
            }
    
            renderer->blur_shader_program_id = blur_shader_program;
    
            glUseProgram(blur_shader_program);
            glUniform1i(glGetUniformLocation(blur_shader_program, "scene_texture"), 0);
            glUniform1i(glGetUniformLocation(blur_shader_program, "depth_texture"), 1);
        }

        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
        glDeleteShader(light_fragment_shader);
        glDeleteShader(blur_fragment_shader);
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
        glBufferData(GL_ARRAY_BUFFER, sizeof(Quad) * MAX_QUADS, renderer->quads.data, GL_DYNAMIC_DRAW);

        renderer->vertex_buffer_id = vertex_buffer;
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

        renderer->index_buffer_id = index_buffer;
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
    }

    return true;
}

bool load_textures(Renderer *renderer) {
    stbi_set_flip_vertically_on_load(true);

    for(i64 i = 0; i < renderer->textures.size; i++) {
        TextureHandle handle = (TextureHandle) i;

        const char *path = texture_path(handle);

        i32 width       = 0;
        i32 height      = 0;
        i32 channels    = 0;
        u8 *image_data  = nullptr;
    
        image_data = stbi_load(path, &width, &height, &channels, 4);
        if (!image_data) {
            printf("Failed to load texture: %s\n", path);
            return false;
        }

        renderer->textures[i] = {
            .handle = handle,
            .width = width,
            .height = height,
            .data = image_data,   
        };
    }

    const i64 ATLAS_WIDTH     = 640;
    const i64 ATLAS_HEIGHT    = 480;
    const i64 BYTES_PER_PIXEL = 4;
    const i64 CHANNELS        = 4;
    const i64 ATLAS_BYTE_SIZE = ATLAS_WIDTH * ATLAS_HEIGHT * BYTES_PER_PIXEL;

    u8 *atlas_data = (u8 *) malloc(ATLAS_BYTE_SIZE);

    { // fill in atlas default data
        i64 i = 0;
        while (i < ATLAS_BYTE_SIZE) {
            atlas_data[i]       = 255;  // r
            atlas_data[i + 1]   = 0;    // g
            atlas_data[i + 2]   = 255;  // b
            atlas_data[i + 3]   = 255;  // a
     
            i += 4;
        }
    }

    { // copy textures into atlas with rect pack and send to gpu
        const i64 RECT_COUNT = TH_COUNT__;

        stbrp_context rp_context;
        stbrp_node nodes[ATLAS_WIDTH];
        stbrp_rect rects[RECT_COUNT];

        stbrp_init_target(&rp_context, ATLAS_WIDTH, ATLAS_HEIGHT, nodes, ATLAS_WIDTH);
        for(i64 i = 0; i < renderer->textures.size; i++) {
            TextureHandle texture_handle = (TextureHandle) i; 
            Texture *texture = &renderer->textures[texture_handle];

            rects[i] = stbrp_rect {
                .id = texture_handle,
                .w = (i32) texture->width,
                .h = (i32) texture->height,
            };
        }

        i64 status = stbrp_pack_rects(&rp_context, rects, RECT_COUNT);
        if (status == 0) {
            printf("error packing textures into atlas\n");
            return false;
        }

        for(int i = 0; i < RECT_COUNT; i++) {
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
        assert(texture_id != 0);

        renderer->atlas_texture_id = texture_id;
    }

#ifdef DEBUG
    { // write atlas to build folder
        stbi_flip_vertically_on_write(true);
        i64 status = stbi_write_png("build/atlas.png", ATLAS_WIDTH, ATLAS_HEIGHT, 4, atlas_data, ATLAS_WIDTH * BYTES_PER_PIXEL);
        if (status == 0) {
            printf("error writing atlas to build folder\n");
            return false;
        }
    }
#endif

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

bool init_frame_buffer(FrameBuffer *frame_buffer) {
    glCreateFramebuffers(1, &frame_buffer->id);
    glBindFramebuffer(GL_FRAMEBUFFER, frame_buffer->id);

    // create texture that frame buffer will render into as the colour attachment
    glCreateTextures(GL_TEXTURE_2D, 1, &frame_buffer->colour_attachment);
    glBindTexture(GL_TEXTURE_2D, frame_buffer->colour_attachment);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, frame_buffer->width, frame_buffer->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // create texture that frame buffer will use as depth buffer
    glCreateTextures(GL_TEXTURE_2D, 1, &frame_buffer->depth_attachment);
    glBindTexture(GL_TEXTURE_2D, frame_buffer->depth_attachment);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH24_STENCIL8, frame_buffer->width, frame_buffer->height);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // assign attachments to frame buffer
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, frame_buffer->colour_attachment, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, frame_buffer->depth_attachment, 0);

    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        printf("error when createing frame buffer, was not complete\n");
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

void draw_rectangle(Renderer *renderer, v3 position, v2 size, v4 color) {
    v2 uvs[4] = {
        {0, 1},
        {1, 1},
        {1, 0},
        {0, 0},
    };

    push_quad(renderer, position, size, 0, color, uvs, 0);
}

void draw_circle(Renderer *renderer, v3 position, f32 radius, v4 color) {
    v2 size = {radius * 2, radius * 2};

    v2 uvs[4] = {
        {0, 1},
        {1, 1},
        {1, 0},
        {0, 0},
    };

    push_quad(renderer, position, size, 0, color, uvs, 1);
}

void draw_texture(Renderer *renderer, TextureHandle handle, v3 position, v2 size, f32 rotation, v4 color) {
    Texture *texture = &renderer->textures[handle];
    push_quad(renderer, position, size, rotation, color, texture->uvs, 2);
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
        v2 translated_position = scaled_position + pivot_point_translation + position.XY;

        // quad needs position to be centre of quad so just convert that here
        v2 quad_centered_position = translated_position + (scaled_size * 0.5f);

        push_quad(renderer, v3{quad_centered_position.X, quad_centered_position.Y, 0}, scaled_size, 0, color, glyph->uvs, 3);
   }

    mem_free(glyphs);
}

void draw_light(Renderer *renderer, v3 position) {
    m4 model_matrix = HMM_M4D(1.0f);
    model_matrix = HMM_MulM4(model_matrix, HMM_Translate(v3{position.X, position.Y, 0}));
                
    m4 mvp_matrix = HMM_MulM4(renderer->view_projection_matrix, model_matrix);

    Light *light = push(&renderer->lights);
    light->position = HMM_MulM4V4(mvp_matrix, {0, 0, 0, 1}).XY;
}

void new_frame(Renderer *renderer, Window *window, Camera camera) {
    reset(&renderer->quads);

    renderer->view_projection_matrix = HMM_MulM4(get_projection_matrix(camera, (f32) window->width / (f32) window->height), get_view_matrix(camera)); 
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void draw_frame(Renderer *renderer, Window *window) {
    { // update the quad buffer and draw
        glViewport(0, 0, window->width, window->height);

        glBindBuffer(GL_ARRAY_BUFFER, renderer->vertex_buffer_id);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(Quad) * renderer->quads.len, renderer->quads.data);
        glBindVertexArray(renderer->vertex_array_id);

        glUseProgram(renderer->shader_program_id);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, renderer->atlas_texture_id);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, renderer->font_texture_id);

        glDrawElements(GL_TRIANGLES, 6 * renderer->quads.len, GL_UNSIGNED_INT, 0);
    } 
}

Quad *push_quad(Renderer *renderer, v3 position, v2 size, f32 rotation, v4 color, v2 uvs[4], i32 draw_type) {
    const v4 top_left      = {-0.5,   0.5, 0, 1};
    const v4 top_right     = { 0.5,   0.5, 0, 1};
    const v4 bottom_right  = { 0.5,  -0.5, 0, 1};
    const v4 bottom_left   = {-0.5,  -0.5, 0, 1};

    m4 model_matrix = HMM_M4D(1.0f);
    model_matrix = HMM_MulM4(model_matrix, HMM_Translate(position));
    model_matrix = HMM_MulM4(model_matrix, HMM_Scale({size.X, size.Y, 1}));
    model_matrix = HMM_MulM4(model_matrix, HMM_Rotate_LH(rotation * HMM_DegToRad, {0, 0, 1}));
                
    m4 mvp_matrix = HMM_MulM4(renderer->view_projection_matrix, model_matrix);

    Quad *quad = push(&renderer->quads);
               
    quad->vertices[0].position = HMM_MulM4V4(mvp_matrix, top_left).XYZ;
    quad->vertices[1].position = HMM_MulM4V4(mvp_matrix, top_right).XYZ;
    quad->vertices[2].position = HMM_MulM4V4(mvp_matrix, bottom_right).XYZ;
    quad->vertices[3].position = HMM_MulM4V4(mvp_matrix, bottom_left).XYZ;
                
    quad->vertices[0].colour = color;
    quad->vertices[1].colour = color;
    quad->vertices[2].colour = color;
    quad->vertices[3].colour = color;

    quad->vertices[0].uv = uvs[0];
    quad->vertices[1].uv = uvs[1];
    quad->vertices[2].uv = uvs[2];
    quad->vertices[3].uv = uvs[3];

    quad->vertices[0].draw_type = draw_type;
    quad->vertices[1].draw_type = draw_type;
    quad->vertices[2].draw_type = draw_type;
    quad->vertices[3].draw_type = draw_type;

    return quad;
}

v2 screen_position_to_world_position(v2 screen_position, Camera camera, Window *window) {
    // TODO: finsh this when needed
    v2 ndc = screen_position_to_ndc(screen_position, window);
    return ndc;
}

v2 screen_position_to_ndc(v2 screen_position, Window *window) {
    return {
        (screen_position.X / window->width) * 2 - 1,
        (screen_position.Y / window->height) * 2 - 1,
    };
}


m4 get_view_matrix(Camera camera) {
    return HMM_LookAt_LH(
        camera.position, 
        {camera.position.X, camera.position.Y, camera.position.Z + 1}, 
        {0, 1, 0}
    );
}

m4 get_projection_matrix(Camera camera, f32 aspect) {
    return HMM_Orthographic_LH_NO(
        -camera.orthographic_size * aspect,  // left
         camera.orthographic_size * aspect,  // right
        -camera.orthographic_size,           // bottom
         camera.orthographic_size,           // top
         camera.near_plane, 
         camera.far_plane 
    );
}

f32 texture_aspect_ratio(Renderer *renderer, TextureHandle handle) {
    Texture *texture = &renderer->textures[(i64) handle];
    return (f32) texture->width / (f32) texture->height;
}

const char *texture_path(TextureHandle handle) {
    switch (handle) {
        case TH_FENCE: 
            return "resources/textures/decorations/fence_1.png";
        case TH_LAMP: 
            return "resources/textures/decorations/lamp.png";
        case TH_ROCK_1: 
            return "resources/textures/decorations/rock_1.png";
        case TH_ROCK_2: 
            return "resources/textures/decorations/rock_2.png";
        case TH_SHOP: 
            return "resources/textures/decorations/shop.png";
        case TH_BACKGROUND_LAYER_1: 
            return "resources/textures/background/background_layer_1.png";
        case TH_BACKGROUND_LAYER_2: 
            return "resources/textures/background/background_layer_2.png";
        case TH_BACKGROUND_LAYER_3: 
            return "resources/textures/background/background_layer_3_big.png";
        case TH_FLOOR: 
            return "resources/textures/background/floor.png";
        case TH_COUNT__: 
            assert(0);
    }

    return nullptr;
}

v4 alpha(v4 base, f32 alpha) {
    return {base.R, base.G, base.B, alpha};
}

void opengl_error_callback(GLenum source, GLenum type, u32 id, GLenum severity, i32 length, const char *message, const void *user_param) {
    printf("OpenGL error: %s", message);
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

    Array<ma_sound, SH_COUNT__> sounds;
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
