#ifndef CPP_GAME
#define CPP_GAME

#include "common.h"
#include "containers.cpp"
#include "platform.h"
#include "shaders/basic_shader.h"

// what a guy has to do to get an angle
// calculated for him around here
#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/vector_angle.hpp"
#undef GLM_ENABLE_EXPERIMENTAL
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/geometric.hpp"

#include "stb/stb.cpp"
#include "sokol/sokol.cpp"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef WINDOWS
#include <windows.h>
#endif

#define DEFAULT_WIDTH 1500
#define DEFAULT_HEIGHT 900

#define GRID_STEP_SIZE 10.0f

// 10 Mb
#define MAIN_ALLOCATOR_SIZE 1024 * 1024 * 10
// 10 Kb
#define FRAME_ALLOCATOR_SIZE 1024 * 10

#define MAX_ENTITIES 128
#define MAX_QUADS 512

#define PLAYER_SPEED 100.0f

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

enum RenderLayer {
    RL_FLOOR,
    RL_FORGROUND
};

enum DrawType {
    DT_RECTANGLE,
    DT_CIRCLE,
    DT_TEXT,
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

enum TextureId {
    TX_NONE,
    TX_PLAYER,
    TX_CRAWLER,
    _TX_LAST_
};

struct Texture {
    TextureId id;
    i64 width;
    i64 height;
    u8 *data;
    glm::vec2 atlas_uvs[4];
};

struct TextureAtlas {
    i32 width;
    i32 height;
    sg_image image;
    Slice<u8> data;
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
    RenderLayer layer;

    // grid
    i64 grid_position[2];
};

struct State {
    Allocator allocator;
    Allocator frame_allocator;

    // application state
    bool running;
    i32 width;
    i32 height;

    // level state
    Slice<u8> level_data;

    // input
    glm::vec2 mouse_screen_position;
    InputState mouse_buttons[_MOUSE_LAST_];
    InputState keys[_KEY_LAST_];

    // camera
    glm::vec2 camera;
    f32 camera_view_width; // world units

    // entities
    Slice<Entity> entities;
    i64 entity_count;

    // render stuff
    Font font;
    TextureAtlas atlas;
    Texture textures[(i64)_TX_LAST_];
    Slice<Quad> quads;
    i64 quad_count;
    sg_bindings bindings;
    sg_pipeline render_pipeline;
    sg_pass_action pass_action;
};

internal void init_sokol();
internal void frame();
internal void event(const sapp_event *event);
internal void cleanup();

internal void update();
internal void physics(f32 delta_time);
internal void draw();

internal Entity *create_entity(Entity entity);
internal Entity *create_player(i64 grid_x, i64 grid_y);
internal Entity *create_floor(i64 grid_x, i64 grid_y);

internal void generate_level();

internal void renderer_init();
internal void renderer_draw();
internal void draw_rectangle(glm::vec2 position, glm::vec2 size, Colour colour, RenderLayer layer, f32 rotation = 0.0f);
internal void draw_circle(glm::vec2 position, f32 radius, Colour colour, RenderLayer layer);
internal void draw_line(glm::vec2 start, glm::vec2 end, f32 thickness, Colour colour, RenderLayer layer);
internal void draw_text(Slice<char> text, glm::vec2 position, f32 font_size, Colour colour, RenderLayer layer);
internal void draw_quad(glm::vec2 position, glm::vec2 size, f32 rotation, Colour colour, RenderLayer layer, DrawType type);
internal void draw_quad(glm::vec2 position, glm::vec2 size, f32 rotation, Colour colour, RenderLayer layer, DrawType type, 
                        glm::vec2 top_left_uv, glm::vec2 top_right_uv, glm::vec2 bottom_right_uv, glm::vec2 bottom_left_uv);

internal glm::mat4x4 get_view_matrix(glm::vec2 camera);
internal glm::mat4x4 get_projection_matrix(f32 aspect_ratio, f32 orthographic_size);

internal void load_levels();
internal void load_fonts();
internal void load_textures();
internal Slice<u8> read_file(Slice<char> path);
internal void write_file(Slice<char> path, Slice<u8> buffer);

internal Slice<char> texture_file_name(TextureId id);

internal glm::vec2 screen_to_ndc(glm::vec2 screen_position);
internal glm::vec2 screen_to_world(glm::vec2 screen_position);

internal Colour with_alpha(Colour c, f32 alpha);
internal Key convert_key(sapp_keycode keycode);
internal void print(Slice<u8> string);

internal State state;

internal // @main
sapp_desc sokol_main(i32 argc, char **argv) {
    state = {
        .running = true,
        .width = DEFAULT_WIDTH,
        .height = DEFAULT_HEIGHT,
        .camera_view_width = 100.0f,
    };

#ifdef WINDOWS
    assert(AllocConsole());
#endif

    init(&state.allocator, MAIN_ALLOCATOR_SIZE);
    init(&state.frame_allocator, FRAME_ALLOCATOR_SIZE);

    state.entities = alloc<Entity>(&state.allocator, MAX_ENTITIES);
    state.quads = alloc<Quad>(&state.allocator, MAX_QUADS);

    srand(120);

    load_levels();
    load_fonts();
    load_textures();

    generate_level(); 

    return sapp_desc {
        .init_cb = init_sokol,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = event,
        .width = DEFAULT_WIDTH,
        .height = DEFAULT_HEIGHT,
        .window_title = "Clear (sokol app)",
        .icon = {
            .sokol_default = true
        },
        .logger = {
            .func = slog_func
        },
    }; 
}

internal
void init_sokol() {
    sg_setup({
        .logger = {
            .func = slog_func
        },
        .environment = sglue_environment(),
    });

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

    sg_image_desc image_description = {
        .width = state.font.bitmap_width,
        .height = state.font.bitmap_height,
        .pixel_format = SG_PIXELFORMAT_R8,
        .label = "font_texture", 
    };
    image_description.data.subimage[0][0] = {.ptr = state.font.bitmap, .size = (u32)(state.font.bitmap_width * state.font.bitmap_height)};

    state.bindings.images[IMG_font_texture] = sg_make_image(image_description);

    state.bindings.samplers[SMP_default_sampler] = sg_make_sampler({
        .label = "default_sampler"
    });

    sg_shader shader = sg_make_shader(basic_shader_desc(sg_query_backend()));

    sg_pipeline_desc pipeline_desc = {
        .shader = shader,
        .depth = {
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true,
        },
        .index_type = SG_INDEXTYPE_UINT16,
        .cull_mode = SG_CULLMODE_BACK,
        .label = "basic-pipeline",
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
        }, 
    };

    state.render_pipeline = sg_make_pipeline(pipeline_desc);

    state.pass_action = {
        .colors = {
            {.load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.6f, 0.75f, 0.8f, 1.0f }}
        }
    }; 
}

internal
void frame() {
    update();
    physics(1.0f / 60.0f);
    draw();

    if (state.quad_count == 0) return;

    sg_update_buffer(
        state.bindings.vertex_buffers[0],
        { .ptr = state.quads.data, .size = sizeof(Quad) * state.quad_count }
    );

    sg_begin_pass({
        .action = state.pass_action,
        .swapchain = sglue_swapchain()
    });


    sg_apply_pipeline(state.render_pipeline);
    sg_apply_bindings(state.bindings);

    sg_draw(0, (i32) state.quad_count * 6, 1);
    

    sg_end_pass();

    sg_commit();

    state.quad_count = 0;
}

internal 
void event(const sapp_event *event) {
    switch (event->type) {
        case SAPP_EVENTTYPE_KEY_DOWN: {
            Key key = convert_key(event->key_code);
            
            if (key != _KEY_LAST_) {
                state.keys[(i64) key] = InputState::DOWN;
            }
            break;
        }
        case SAPP_EVENTTYPE_KEY_UP: {
            Key key = convert_key(event->key_code);
            
            if (key != _KEY_LAST_) {
                state.keys[(i64) key] = InputState::UP;
            }
            break;
        }
        case SAPP_EVENTTYPE_RESIZED: {
            state.width = event->window_width;
            state.height = event->window_height;

            break;
        }
        default: break;
    }
}

internal
void cleanup() {
    deinit(&state.allocator);
    deinit(&state.frame_allocator);
    sg_shutdown();
}

// @update
internal void update() {
    if (state.keys[KEY_ESCAPE] == InputState::DOWN) {
        sapp_quit();
    }

    for (i64 entity_index = 0; entity_index < state.entity_count; ++entity_index) {
        Entity *entity = &state.entities[entity_index];

        if (entity->flags & EF_PLAYER) {

            { // movement input
                glm::vec2 input = {};

                if (state.keys[KEY_W] == InputState::DOWN) {
                    input.y += 1.0f;
                }

                if (state.keys[KEY_S] == InputState::DOWN) {
                    input.y -= 1.0f;
                }

                if (state.keys[KEY_A] == InputState::DOWN) {
                    input.x -= 1.0f;
                }

                if (state.keys[KEY_D] == InputState::DOWN) {
                    input.x += 1.0f;
                }

                if (glm::length(input) > 0) {
                    // if there is no input (len == 0) then it becomes NaN
                    input = glm::normalize(input);
                }
                
                entity->velocity = input * PLAYER_SPEED;
            }
        }
    }
}

internal void physics(f32 delta_time) {
    for (i64 entity_index = 0; entity_index < state.entity_count; ++entity_index) {
        Entity *entity = &state.entities[entity_index];

        entity->position += entity->velocity * delta_time;
    }
}

// @draw
internal void draw() {
    for (i64 entity_index = 0; entity_index < state.entity_count; ++entity_index) {
        Entity *entity = &state.entities[entity_index];

        draw_rectangle(entity->position, entity->size, entity->colour, entity->layer);
    }
}

internal 
Entity *create_entity(Entity entity) {
    Entity *entity_ptr = &state.entities[state.entity_count];
    state.entity_count++;

    *entity_ptr = entity;

    return entity_ptr;
}

internal 
Entity *create_player(i64 grid_x, i64 grid_y) {
    return create_entity({
        .flags = EF_PLAYER,
        .position = {(f32) grid_x * GRID_STEP_SIZE, (f32) grid_y * GRID_STEP_SIZE},
        .size = {GRID_STEP_SIZE * 0.8f, GRID_STEP_SIZE * 0.8f},
        .colour = RED,
        .layer = RL_FORGROUND,
        .grid_position = {grid_x, grid_y},
    });
}

internal 
Entity *create_floor(i64 grid_x, i64 grid_y) {
    i32 n = rand();
    f32 f = (f32) n / (f32) RAND_MAX;

    return create_entity({
        .flags = EF_NONE,
        .position = {(f32) grid_x * GRID_STEP_SIZE, (f32) grid_y * GRID_STEP_SIZE},
        .size = {GRID_STEP_SIZE, GRID_STEP_SIZE},
        .colour = {f, f, f, 1.0f},
        .layer = RL_FLOOR,
        .grid_position = {grid_x, grid_y},
    });
}

// does not include line endings just the text from that line
internal 
Slice<u8> read_line(Slice<u8> text) {
    if (text.len == 0) {
        return {};
    }

    i64 index = 0;
    while (text[index] != '\n') {
        index += 1;
    }

    // if there is no line and just line ending, then just return
    // that as the length, 0. But if there was some text then reduce
    // the index so the returned slice is just the line, no endings
    i64 line_length = index;

    if (line_length > 0) {
        line_length -= 1;
    }

    return {.data = text.data, .len = line_length};
}

// line numbers starts at 1
internal 
Slice<u8> read_line_n(Slice<u8> text, i64 line) {
    if (text.len == 0) {
        return {};
    }

    i64 current_line = 0;

    while (true) {
        current_line += 1;

        Slice<u8> current = read_line(text);

        if (current_line == line) {
            return current; 
        }

        // TODO: assuming CRLF line endings, need to
        // check for UNIX system
        text.data = text.data + current.len + 2;
        text.len -= current.len + 2;

        if (text.len <= 2) {
            // only line endings left
            return {};
        }
    }
}

internal 
void generate_level() {
    i64 line_number = 1;
    
    while (true) {
        Slice<u8> line = read_line_n(state.level_data, line_number);
        if (line.len == 0) {
            break;
        }

        line_number += 1;

        for (i64 i = 0; i < line.len; i++) {
            glm::vec2 position = {GRID_STEP_SIZE * (f32) i, GRID_STEP_SIZE * (f32) -(line_number - 1)};
            u8 byte = line[i];

            switch (byte) {
                case ' ': break;
                case 'P': {
                    create_floor(i, line_number);
                    Entity *player = create_player(i, line_number);
                    state.camera = player->position;

                    break;
                }
                case '0': {
                    create_floor(i, line_number);
                    break;
                }
                default: {
                    // unknown character in level file
                    assert(0);
                }
            }
        }
    }
}

internal
void renderer_init() {
    
}

internal
void renderer_draw() {
    
}

internal 
void draw_rectangle(glm::vec2 position, glm::vec2 size, Colour colour, RenderLayer layer, f32 rotation) {
    draw_quad(position, size, rotation, colour, layer, DT_RECTANGLE);
}

internal 
void draw_circle(glm::vec2 position, f32 radius, Colour colour, RenderLayer layer) {
    draw_quad(position, {radius * 2, radius * 2}, 0, colour, layer, DT_CIRCLE);
}

internal
void draw_line(glm::vec2 start, glm::vec2 end, f32 thickness, Colour colour, RenderLayer layer) {
    glm::vec2 direction = end - start;
    f32 radians = glm::angle({0, 1.0f}, glm::normalize(direction));
    f32 degrees = glm::degrees(radians);

    glm::vec2 line_centre = start + (direction * 0.5f);
    f32 length = glm::length(direction);

    if (start.x > end.x) {
        degrees *= -1;
    }

    draw_rectangle(line_centre, {thickness, length}, colour, layer, degrees);
}

internal 
void draw_text(Slice<char> text, glm::vec2 position, f32 font_size, Colour colour, RenderLayer layer) {
    // Font size is the final height of the text drawn in world units
    // 10 font size == size{#chars * avg_char_width, font_size}

    if (text.len == 0) return;

    Slice<GlyphRenderInfo> infos = alloc<GlyphRenderInfo>(&state.frame_allocator, text.len);

    f32 total_x = 0;
    f32 total_y = 0;
    f32 max_height = 0;

    for (i64 i = 0; i < text.len; i++) {
        char c = text[i];

        f32 advanced_x = 0;
        f32 advanced_y = 0;

        stbtt_aligned_quad alligned_quad = {};


        // this is the the data for the aligned_quad we're given, with y+ going down
        //	   x0, y0       x1, y0
        //     s0, t0       s1, t0
        //	    o tl        o tr
        // 
        //
        //     x0, y1      x1, y1
        //     s0, t1      s1, t1
        //	    o bl        o br
        // 
        // x, and y and expected vertex positions
        // s and t are texture uv position
        stbtt_GetBakedQuad(state.font.characters, state.font.bitmap_width, state.font.bitmap_height, c - 32, &advanced_x, &advanced_y, &alligned_quad, false);

        f32 width = alligned_quad.x1 - alligned_quad.x0;
        f32 height = alligned_quad.y1 - alligned_quad.y0;

        if (height > max_height) {
            max_height = height;
        }

        glm::vec2 top_left_uv       = {alligned_quad.s0, alligned_quad.t0};
        glm::vec2 top_right_uv      = { alligned_quad.s1, alligned_quad.t0 };
        glm::vec2 bottom_right_uv   = {alligned_quad.s1, alligned_quad.t1};
        glm::vec2 bottom_left_uv    = { alligned_quad.s0, alligned_quad.t1 };

        infos[i] = {
            .relative_x = total_x,
            .relative_y = total_y,
            .width = width,
            .height = height,
            .uvs = {
                top_left_uv,
                top_right_uv,
                bottom_right_uv,
                bottom_left_uv
            }
        };

        total_x += advanced_x;
        total_y += advanced_y;
    }

    f32 scale_factor = (1 / max_height);
    f32 height_scale_factor = scale_factor * font_size; // font size of 1 means tallest glyph is 1 world unit tall
    f32 total_scaled_width = total_x * scale_factor * font_size;

    for (i64 i = 0; i < infos.len; i++) {
        GlyphRenderInfo *info = &infos[i];

        f32 width_proportion = info->width / total_x;
        f32 x_proportion = info->relative_x / total_x;

        f32 width = width_proportion * total_scaled_width;
        f32 x = x_proportion * total_scaled_width;

        // TODO: right now the text is just centred, could add an origin
        // vec2 which dictates where the centre of the text should be
        glm::vec2 size = {width, info->height * height_scale_factor};
        glm::vec2 offset_position = glm::vec2{x - (total_scaled_width * 0.5), info->relative_y};

        draw_quad(
            position + offset_position,
            size,
            0,
            colour,
            layer,
            DT_TEXT,
            info->uvs[0],
            info->uvs[1],
            info->uvs[2],
            info->uvs[3]
        );
    }
}

internal 
void draw_quad(glm::vec2 position, glm::vec2 size, f32 rotation, Colour colour, RenderLayer layer, DrawType type) {
    draw_quad(position, size, rotation, colour, layer, type, {0, 1}, {1, 1}, {1, 0}, {0, 0});
}

internal void draw_quad(glm::vec2 position, glm::vec2 size, f32 rotation, Colour colour, RenderLayer layer, DrawType type, glm::vec2 top_left_uv, glm::vec2 top_right_uv, glm::vec2 bottom_right_uv, glm::vec2 bottom_left_uv) {
    // Quad *quad = &state.quads[state.quad_count];
    Quad *quad = &state.quads.data[state.quad_count];
    state.quad_count += 1;

    glm::mat4 model_matrix = glm::translate(glm::mat4(1.0f), {position.x, position.y, 0});
    model_matrix = glm::rotate(model_matrix, glm::radians(-rotation), glm::vec3{0.0f, 0.0f, 1.0f}); // -rotation so we rotate right
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
        case DT_RECTANGLE: {
            texture_index = 0;
            break;
        }
        case DT_CIRCLE: {
            texture_index = 1;
            break;
        }
        case DT_TEXT: {
            texture_index = 2;
            break;
        }
        default: {
            assert(0);
        }
    }

    f32 z;
    switch (layer) {
        case RL_FLOOR: z = 1;
            break;
        case RL_FORGROUND: z = 0; 
            break;
        default: assert(0);
    }

    quad->vertices[0] = {
        .position = {top_left.x, top_left.y, z},
        .colour = colour,
        .texture_uv = {top_left_uv.x, top_left_uv.y},
        .texture_index = texture_index,
    };

    quad->vertices[1] = {
        .position = {top_right.x, top_right.y, z},
        .colour = colour,
        .texture_uv = {top_right_uv.x, top_right_uv.y},
        .texture_index = texture_index,
    };

    quad->vertices[2] = {
        .position = {bottom_right.x, bottom_right.y, z},
        .colour = colour,
        .texture_uv = {bottom_right_uv.x, bottom_right_uv.y},
        .texture_index = texture_index,
    };

    quad->vertices[3] = {
        .position = {bottom_left.x, bottom_left.y, z},
        .colour = colour,
        .texture_uv = {bottom_left_uv.x, bottom_left_uv.y},
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

internal 
void load_levels() {
    Slice<u8> level_data = read_file(STR("resources/levels/start.level"));
    state.level_data = level_data;
}

internal 
void load_fonts() {
    state.font = {
        .bitmap_width = 256,
        .bitmap_height = 256,
        .character_count = 96,
        .font_height = 15.0f,
    };

    state.font.bitmap = (u8 *) malloc(state.font.bitmap_width * state.font.bitmap_height);
    assert(state.font.bitmap);

    state.font.characters = (stbtt_bakedchar *) malloc(sizeof(stbtt_bakedchar) * state.font.character_count);
    assert(state.font.characters);

    Slice<u8> bytes = read_file(STR("resources/fonts/alagard.ttf"));

    i64 bake_result = stbtt_BakeFontBitmap(
        bytes.data, 
        0, 
        state.font.font_height, 
        state.font.bitmap, 
        state.font.bitmap_width, 
        state.font.bitmap_height, 
        32, 
        state.font.character_count,
        state.font.characters
    );

    assert(bake_result > 0);

    i64 write_result = stbi_write_png("build/font.png", state.font.bitmap_width, state.font.bitmap_height, 1, state.font.bitmap, state.font.bitmap_width);
    assert(write_result != 0);
}

internal 
void load_textures() {
    return;
    // 1 to skip none texture
    for (i64 i = 1; i < _TX_LAST_; i++)  {
        TextureId id = (TextureId) i;

        Slice<char> file_name = texture_file_name(id);
        Slice<char> path = fmt_string(&state.frame_allocator, STR("resources/textures/%s"), file_name.data);

        Slice<u8> bytes =  read_file(path);

        i32 width;
        i32 height;
        i32 channels;

        u8 *image_data = stbi_load(path.data, &width, &height, &channels, 4);
        assert(image_data);

        state.textures[i] = {
            .id = id,
            .width = width,
            .height = height,
            .data = image_data,   
        }; 

        Slice<char> out = fmt_string(&state.frame_allocator, STR("build/%d_%s"), channels, file_name.data);
        // write_file(out, bytes);
        stbi_write_png(out.data, width, height, 4, image_data, 4);
    }


    const i32 atlas_width = 16;
    const i32 atlas_height = 35;
    const i32 bytes_per_pixel = 4;
    i32 image_size = atlas_width * atlas_height * bytes_per_pixel;

    Slice<u8> atlas_data = alloc<u8>(&state.allocator, image_size);

    // setting it to be all purple
    for (i32 i = 0; i < image_size; i += bytes_per_pixel) {
        u8 *pixel = &atlas_data.data[i];

        pixel[0] = 255; // r
        pixel[1] = 10;  // g
        pixel[2] = 255; // b
        pixel[3] = 255; // a
    }

    // do rect packing for atlas
    const i32 rect_count = 1; // texture count - 1 because of none
    stbrp_context context;
    stbrp_node nodes[atlas_width];
    stbrp_rect rects[rect_count];

    stbrp_init_target(&context, atlas_width, atlas_height, nodes, atlas_width);

    for (i64 i = 0; i < rect_count; i++) {
        TextureId id = (TextureId) (i + 1); // plus 1 to skip none texture
        Texture *texture = &state.textures[id];
        rects[i] = {.id = id, .w = (i32) texture->width, .h = (i32) texture->height};
    }

    bool ok = stbrp_pack_rects(&context, rects, rect_count);
    assert(ok);

    for (i64 i = 0; i < rect_count; i++) {
        stbrp_rect *rect = &rects[i];
        Texture *texture = &state.textures[rect->id];

        // copy row by row into atlas
        for (i64 row = 0; row < rect->h; row++) {
            u8 *source_row = texture->data + (row * rect->w * bytes_per_pixel);
            u8 *dest_row = atlas_data.data + (row * rect->w * bytes_per_pixel);
            memcpy(dest_row, source_row, rect->w * 4);

            // src_row := mem.ptr_offset(&img.data[0], row * rect.w * 4)
            // dest_row := mem.ptr_offset(cast(^u8)raw_data, ((rect.y + row) * auto_cast atlas.w + rect.x) * 4)
            // mem.copy(dest_row, src_row, auto_cast rect.w * 4)
        }
    }

    Texture *player = &state.textures[TX_PLAYER];
    stbi_write_png("build/alas.png", atlas_width, atlas_height, 4, atlas_data.data, 4);
}

internal 
Slice<char> texture_file_name(TextureId id) {
    switch (id) {
        case TX_PLAYER: {
            return STR("player.png");
        }
        case TX_CRAWLER: {
            return STR("crawler.png");
        }
        case TX_NONE:
        case _TX_LAST_:
        default:
            assert(0);
    }

    return {};
}

internal 
Slice<u8> read_file(Slice<char> path) {
    // TODO: using malloc for this and not freeing LUL
    FILE *file = fopen(path.data, "rb");
    assert(file);

    fseek(file, 0, SEEK_END);
    i64 file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    Slice<u8> bytes = alloc<u8>(&state.allocator, file_size);

    fread(bytes.data, file_size, 1, file);
    fclose(file);

    return bytes;
}

internal 
void write_file(Slice<char> path, Slice<u8> buffer) {
    // TODO: using malloc for this and not freeing LUL
    FILE *file = fopen(path.data, "wb");
    assert(file);

    fwrite(buffer.data, buffer.len, 1, file);
    fclose(file);
}

internal 
glm::vec2 screen_to_ndc(glm::vec2 screen_position) {
    return {
        (screen_position.x / state.width) * 2 - 1,
        (screen_position.y / state.height) * 2 - 1,
    };
}

internal 
glm::vec2 screen_to_world(glm::vec2 screen_position) {
    /*
    ndc_vec_2 := screen_position_to_ndc(position)
    ndc_position := Vector4{ndc_vec_2.x, ndc_vec_2.y, -1, 1} // -1 here for near plane

    inverse_vp := linalg.inverse(get_projection_matrix() * (scale_matrix(state.zoom) * get_view_matrix()))
    world_position := inverse_vp * ndc_position
    world_position /= world_position.w
    */

    glm::vec2 ndc = screen_to_ndc(screen_position);

    glm::mat4 view_matrix = get_view_matrix(state.camera);
    glm::mat4 projection_matrix = get_projection_matrix((f32)state.width / (f32)state.height, state.camera_view_width * 0.5f);

    glm::mat4 inverse_transformmation_matrix = glm::inverse(projection_matrix * view_matrix);
    
    glm::vec4 world_position = inverse_transformmation_matrix * glm::vec4{ndc.x, ndc.y, -1, 1};
    world_position /= world_position.w;
    
    return world_position;
}

internal 
Colour with_alpha(Colour colour, f32 alpha) {
    return Colour {colour.r, colour.g, colour.b, alpha};
}

internal
Key convert_key(sapp_keycode keycode) {
    if (keycode >= SAPP_KEYCODE_0 && keycode <= SAPP_KEYCODE_9) {
        return (Key)(keycode - SAPP_KEYCODE_0);
    }

    if (keycode >= SAPP_KEYCODE_A && keycode <= SAPP_KEYCODE_Z) {
        i64 key = KEY_A + (keycode - SAPP_KEYCODE_A);
        return (Key) key;
    }

    if (keycode == SAPP_KEYCODE_ESCAPE) {
        return KEY_ESCAPE;
    }

    return _KEY_LAST_;
}

internal 
void print(Slice<u8> string) {
    if(string.len == 0) return;
    assert(string.data != nullptr);

#ifdef WINDOWS
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    WriteConsoleA(out, string.data, (DWORD)string.len, NULL, NULL);
    WriteConsoleA(out, "\n\r", 2, NULL, NULL);
#else
#error Need to add a print implementation for this platform
#endif
}

#endif
