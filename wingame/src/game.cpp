#include "game.h"

#include "common.h"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "platform.h"
#include "shaders/basic_shader.h"

#include "sokol/sokol_log.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define DEFAULT_WIDTH 1280
#define DEFAULT_HEIGHT 720

internal State state;

internal // @main
void game_main() {
    state = {
        .running = true,
        .width = DEFAULT_WIDTH,
        .height = DEFAULT_HEIGHT,
        .camera_view_width = 10.0f,
    };

    platform_init_window(state.width, state.height, "Cool game");
    renderer_init();

    while (state.running) {
        for (i64 i = 0; i < _KEY_LAST_; i++) {
            if (state.keys[i] == InputState::DOWN) {
                char buffer[120];
                Slice<char> s = fmt_string(buffer, 120, "%llu is down\n", i); 
                platform_stdout(s.data, s.length);
            }
        }

        platform_process_events();
        renderer_draw();
    }

    sg_shutdown();
}

void game_quit() {
    state.running = false;
}

internal
void renderer_init() {

    sg_desc sg_description {
        .logger = {
            .func = slog_func
        },
        .environment = platform_enviroment(),
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

internal
void renderer_draw() {
    state.quad_count = 0;

    for(i32 i = 0; i < 10; i++) {
        Colour c = RED;

        if (i % 2 == 0) {
            c = BLUE;
        }

        draw_rectangle({(f32)i + 0.5f, 0}, {1, 1}, c);
    }

    draw_circle({0, 0}, 0.2f, WHITE);
    draw_circle({8, 0}, 0.2f, WHITE);

    if (state.quad_count <= 0) {
        return;
    }

    sg_update_buffer(
        state.bindings.vertex_buffers[0],
        { .ptr = state.quads, .size = sizeof(Quad) * state.quad_count }
    );

    sg_begin_pass({
        .action = state.pass_action,
        .swapchain = platform_swapchain()
    });


    sg_apply_pipeline(state.render_pipeline);
    sg_apply_bindings(state.bindings);

    sg_draw(0, 6 * state.quad_count, 1);
    

    sg_end_pass();

    sg_commit();
    platform_present();
}

internal 
void draw_rectangle(glm::vec2 position, glm::vec2 size, Colour colour) {
    draw_quad(position, size, colour, DrawType::RECTANGLE);
}

internal 
void draw_circle(glm::vec2 position, f32 radius, Colour colour) {
    draw_quad(position, {radius * 2, radius * 2}, colour, DrawType::CIRCLE);
}

internal 
void draw_quad(glm::vec2 position, glm::vec2 size, Colour colour, DrawType type) {
    Quad *quad = &state.quads[state.quad_count];
    state.quad_count += 1;

    glm::mat4 model_matrix = glm::translate(glm::mat4(1.0f), {position.x, position.y, 0});
    model_matrix = glm::scale(model_matrix, {size.x, size.y, 1.0f});

    glm::mat4 view_matrix = get_view_matrix(state.camera);
    glm::mat4 projection_matrix = get_projection_matrix((f32)state.width / (f32)state.height, state.camera_view_width * 0.5f);

    glm::mat4x4 transformation_matrix = projection_matrix * view_matrix * model_matrix;

    glm::vec4 top_left = transformation_matrix * glm::vec4{-0.5, 0.5, 0, 1};
    glm::vec4 top_right = transformation_matrix * glm::vec4{0.5, 0.5, 0, 1};
    glm::vec4 bottom_right = transformation_matrix * glm::vec4{0.5, -0.5, 0, 1};
    glm::vec4 bottom_left = transformation_matrix * glm::vec4{-0.5, -0.5, 0, 1};

    f32 texture_index = 0;
    switch (type) {
        case DrawType::RECTANGLE: {
            texture_index = 0;
            break;
        }
        case DrawType::CIRCLE: {
            texture_index = 1;
            break;
        }
        default: {
            assert(0);
        }
    }

    quad->vertices[0] = {
        .position = {top_left.x, top_left.y, 0},
        .colour = colour,
        .texture_uv = {0, 1},
        .texture_index = texture_index,
    };

    quad->vertices[1] = {
        .position = {top_right.x, top_right.y, 0},
        .colour = colour,
        .texture_uv = {1, 1},
        .texture_index = texture_index,
    };

    quad->vertices[2] = {
        .position = {bottom_right.x, bottom_right.y, 0},
        .colour = colour,
        .texture_uv = {1, 0},
        .texture_index = texture_index,
    };

    quad->vertices[3] = {
        .position = {bottom_left.x, bottom_left.y, 0},
        .colour = colour,
        .texture_uv = {0, 0},
        .texture_index = texture_index,
    };
}

internal
glm::mat4x4 get_view_matrix(glm::vec2 camera) {
    return glm::lookAt({camera.x, camera.y, 1}, glm::vec3{camera.x, camera.y, 0}, {0, 1, 0});
}

internal
glm::mat4x4 get_projection_matrix(f32 aspect_ratio, f32 orthographic_size) {
    return glm::ortho(-orthographic_size * aspect_ratio, orthographic_size * aspect_ratio, -orthographic_size, orthographic_size, 0.1f, 100.0f);
}

internal void mouse_button_event(MouseButton button, InputState input_state) {
    state.mouse_buttons[(i64) button] = input_state;
}

internal void key_event(Key key, InputState input_state) {
    state.keys[(i64) key] = input_state;
}
