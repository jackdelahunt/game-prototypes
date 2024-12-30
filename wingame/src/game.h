#ifndef H_GAME
#define H_GAME

#include "common.h"
#include "glm/ext/vector_float2.hpp"
#include "platform.h"

#include "glm/glm.hpp"
#include "sokol/sokol_gfx.h"

#include <assert.h>

template<typename T>
struct Slice {
    T *data;
    i64 length;
};

template <typename T>
T *at_ptr(Slice<T> *slice, i64 index) {
    assert(index >= 0);
    assert(index < slice->length);

    return &slice->data[index];
}

#define fmt_string(buffer, size, fmt, ...) \
Slice<char> { .data = buffer, .length = snprintf(buffer, size, fmt, __VA_ARGS__) }

// -1 to not include null byte
#define STR(s) \
Slice<char> {.data = s, .length = sizeof(s) - 1 } 

struct Colour {
    f32 r;
    f32 g;
    f32 b;
    f32 a;
};

#define WHITE   Colour{1, 1, 1, 1}
#define RED     Colour{1, 0, 0, 1}
#define GREEN   Colour{0, 1, 0, 1}
#define BLUE    Colour{0, 0, 1, 1}

struct Vertex {
    f32 position[3];
    Colour colour;
    f32 texture_uv[2];
    f32 texture_index;
};

struct Quad {
    Vertex vertices[4];
};

enum class DrawType {
    RECTANGLE,
    CIRCLE
};

enum EntityFlag {
    EF_NONE = 0,
    EF_PLAYER
};

struct Entity {
    // meta
    u64 flags;
    glm::vec2 position;
    glm::vec2 velocity;
    glm::vec2 size;
    Colour colour;
};

struct State {
    // application state
    bool running;
    i32 width;
    i32 height;

    // input
    InputState mouse_buttons[_MOUSE_LAST_];
    InputState keys[_KEY_LAST_];

    // camera
    glm::vec2 camera;
    f32 camera_view_width; // world units

    // entities
    Slice<Entity> entities;
    i64 entity_count;

    // render stuff
    Slice<Quad> quads;
    i64 quad_count;
    sg_bindings bindings;
    sg_pipeline render_pipeline;
    sg_pass_action pass_action;
};

internal void game_main();
internal void game_quit();

internal void update();
internal void physics(f32 delta_time);
internal void draw();

internal Entity *create_entity(Entity entity);

internal void renderer_init();
internal void renderer_draw();
internal void draw_rectangle(glm::vec2 position, glm::vec2 size, Colour colour);
internal void draw_circle(glm::vec2 position, f32 radius, Colour colour);
internal void draw_quad(glm::vec2 position, glm::vec2 size, Colour colour, DrawType type);

internal glm::mat4x4 get_view_matrix(glm::vec2 camera);
internal glm::mat4x4 get_projection_matrix(f32 aspect_ratio, f32 orthographic_size);

internal void mouse_button_event(MouseButton button, InputState input_state);
internal void key_event(Key key, InputState input_state);

#endif
