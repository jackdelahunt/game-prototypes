#ifndef H_GAME
#define H_GAME

#include "common.h"
#include "platform.h"
#include "containers.cpp"

#include "stb/stb_truetype.h"
#include "glm/glm.hpp"
#include "sokol/sokol_gfx.h"

struct Colour {
    f32 r;
    f32 g;
    f32 b;
    f32 a;
};

#define WHITE   Colour{1, 1, 1, 1}
#define BLACK   Colour{0, 0, 0, 1}
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
    CIRCLE,
    TEXT,
};

struct GlyphRenderInfo {
    f32 relative_x;
    f32 relative_y;
    f32 width;
    f32 height;
    glm::vec2 uvs[4];
};

struct Font {
    i32 bitmap_width;
    i32 bitmap_height;
    i32 character_count;
    f32 font_height;
    u8 *bitmap;
    stbtt_bakedchar *characters;
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
    Allocator allocator;
    Allocator frame_allocator;

    // application state
    bool running;
    i32 width;
    i32 height;

    Font font;

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
internal void draw_text(Slice<char> text, glm::vec2 position, f32 font_size, Colour colour);
internal void draw_quad(glm::vec2 position, glm::vec2 size, Colour colour, DrawType type);
internal void draw_quad(glm::vec2 position, glm::vec2 size, Colour colour, DrawType type, glm::vec2 top_left_uv, glm::vec2 top_right_uv, glm::vec2 bottom_right_uv, glm::vec2 bottom_left_uv);

internal bool load_fonts();
internal Slice<u8> read_file(const char *path);

internal glm::mat4x4 get_view_matrix(glm::vec2 camera);
internal glm::mat4x4 get_projection_matrix(f32 aspect_ratio, f32 orthographic_size);

internal void mouse_button_event(MouseButton button, InputState input_state);
internal void key_event(Key key, InputState input_state);

#endif
