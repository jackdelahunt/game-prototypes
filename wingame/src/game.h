#ifndef H_GAME
#define H_GAME

#include "common.h"

#include "glm/vec2.hpp"
#include "glm/mat4x4.hpp"
#include "sokol/sokol_gfx.h"

#define MAX_QUADS 1000

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

struct State {
    bool running;

    i32 width;
    i32 height;

    glm::vec2 camera;
    f32 zoom;

    Quad quads[MAX_QUADS];
    i32 quad_count;

    // render stuff
    sg_bindings bindings;
    sg_pipeline render_pipeline;
    sg_pass_action pass_action;
};

template<typename T>
struct Slice {
	const T *data;
	i64 length;
};

internal void game_main();
internal void game_quit();
internal void renderer_init();
internal void renderer_draw();
internal void draw_quad(glm::vec2 position, glm::vec2 size, Colour colour);
internal glm::mat4x4 view_matrix(glm::vec2 camera);
internal glm::mat4x4 projection_matrix();

internal Slice<char> new_slice(const char *string);

#endif
