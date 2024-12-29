#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/vector_float2.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/ext/vector_float4.hpp"
#include "glm/trigonometric.hpp"
#include "platform.h"
#include "common.h"

#include "sokol/sokol_log.h"
#include "shaders/basic_shader.h"

#include "glm/vec2.hpp"
#include "glm/mat4x4.hpp"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_transform.hpp"

#define DEFAULT_WIDTH 1000
#define DEFAULT_HEIGHT 750

#define MAX_QUADS 1000

internal void renderer_init();
internal void renderer_draw();

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

// @state
struct State {
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

internal State state;
internal Platform platform;

void set_platform(Platform p) {
    platform = p;    
}

void start() {
    state = {
        .width = DEFAULT_WIDTH,
        .height = DEFAULT_HEIGHT,
        .zoom = 0.1,
    };

    platform.init(state.width, state.height, L"Cool game");
    renderer_init();
    
    // draw loop
    while (platform.process_events()) {
        renderer_draw();
    }
}

internal
glm::mat4x4 view_matrix(glm::vec2 camera) {
    return glm::lookAt({camera.x, camera.y, 1}, glm::vec3{}, {0, 1, 0});
}

internal
glm::mat4x4 projection_matrix() {
    return glm::perspective(glm::radians(90.0f), (f32)state.width / (f32)state.height, 0.1f, 100.0f);
}

internal 
void draw_quad(glm::vec2 position, glm::vec2 size, Colour colour) {
    Quad *quad = &state.quads[state.quad_count];
    state.quad_count += 1;

    glm::mat4x4 transformation_matrix = glm::mat4(1.0f);
    transformation_matrix = glm::translate(transformation_matrix, {position.x, position.y, 0});
    transformation_matrix = glm::scale(transformation_matrix, {size.x, size.y, 1});

#if 1
    transformation_matrix *= view_matrix(state.camera);
    transformation_matrix = glm::scale(transformation_matrix, {state.zoom, state.zoom, 1});

    transformation_matrix *= projection_matrix();
#endif

    glm::vec4 top_left = transformation_matrix * glm::vec4{-0.5, 0.5, 0, 1};
    glm::vec4 top_right = transformation_matrix * glm::vec4{0.5, 0.5, 0, 1};
    glm::vec4 bottom_right = transformation_matrix * glm::vec4{0.5, -0.5, 0, 1};
    glm::vec4 bottom_left = transformation_matrix * glm::vec4{-0.5, -0.5, 0, 1};

    quad->vertices[0] = {
        .position = {top_left.x, top_left.y, 0},
        .colour = colour,
        .texture_uv = {0, 1},
    };

    quad->vertices[1] = {
        .position = {top_right.x, top_right.y, 0},
        .colour = colour,
        .texture_uv = {1, 1},
    };

    quad->vertices[2] = {
        .position = {bottom_right.x, bottom_right.y, 0},
        .colour = colour,
        .texture_uv = {1, 0},
    };

    quad->vertices[3] = {
        .position = {bottom_left.x, bottom_left.y, 0},
        .colour = colour,
        .texture_uv = {0, 0},
    };
}

internal
void renderer_draw() {
    state.quad_count = 0;

    draw_quad({-2, 0}, {1, 1},  RED);
    draw_quad({5, -3}, {0.5, 5},  GREEN);


    if (state.quad_count <= 0) {
        return;
    }

    sg_update_buffer(
        state.bindings.vertex_buffers[0],
        { .ptr = state.quads, .size = sizeof(Quad) * state.quad_count }
    );

    sg_begin_pass({
        .action = state.pass_action,
        .swapchain = platform.swapchain()
    });


    sg_apply_pipeline(state.render_pipeline);
    sg_apply_bindings(state.bindings);

    sg_draw(0, 6 * state.quad_count, 1);
    

    sg_end_pass();

    sg_commit();
    platform.present();
}

internal
void renderer_init() {
    sg_desc sg_description {
        .logger = {
            .func = slog_func
        },
        .environment = platform.enviroment(),
    };

    sg_setup(&sg_description);

    state.bindings.vertex_buffers[0] = sg_make_buffer({
        .size = sizeof(Quad) * MAX_QUADS,
        .usage = SG_USAGE_DYNAMIC,
        .label = "quad-vertices"
    });

    u16 index_buffer[MAX_QUADS * 6];
    i32 i = 0;
    while (i < MAX_QUADS * 6) {
        // vertex offset pattern to draw a quad
        // { 0, 1, 2,  0, 2, 3 }
        index_buffer[i + 0] = ((i/6)*4 + 0);
        index_buffer[i + 1] = ((i/6)*4 + 1);
        index_buffer[i + 2] = ((i/6)*4 + 2);
        index_buffer[i + 3] = ((i/6)*4 + 0);
        index_buffer[i + 4] = ((i/6)*4 + 2);
        index_buffer[i + 5] = ((i/6)*4 + 3);
        i += 6;
    }

    state.bindings.index_buffer = sg_make_buffer({
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .data = {.ptr = index_buffer, .size = sizeof(index_buffer)},
        .label = "quad-indices"
    });


    sg_shader shader = sg_make_shader(basic_shader_desc(sg_query_backend()));

    sg_pipeline_desc pipeline_desc = {
        .shader = shader,
        .index_type = SG_INDEXTYPE_UINT16,
        .label = "basic-pipeline"
    };

    pipeline_desc.layout.attrs[ATTR_basic_position] = {.format = SG_VERTEXFORMAT_FLOAT3};
    pipeline_desc.layout.attrs[ATTR_basic_color0] = {.format = SG_VERTEXFORMAT_FLOAT4};
    pipeline_desc.layout.attrs[ATTR_basic_texture_uv0] = {.format = SG_VERTEXFORMAT_FLOAT2};
    pipeline_desc.layout.attrs[ATTR_basic_texture_index0] = {.format = SG_VERTEXFORMAT_FLOAT};

    pipeline_desc.colors[0] = {
        .blend = {
            .enabled = true,
            .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
            .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
            .op_rgb = SG_BLENDOP_ADD,
            .src_factor_alpha = SG_BLENDFACTOR_ONE,
            .dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
            .op_alpha = SG_BLENDOP_ADD,
        }
    };

    state.render_pipeline = sg_make_pipeline(pipeline_desc);

    state.pass_action = {
        .colors = {
            {.load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.3f, 0.3f, 0.7f, 1.0f }}
        }
    };
}
