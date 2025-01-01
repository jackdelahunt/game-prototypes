#ifndef CPP_GAME
#define CPP_GAME

#include "game.h"

#include "common.h"
#include "platform.h"
#include "containers.cpp"
#include "shaders/basic_shader.h"

// what a guy has to do to get an angle
// calculated for him around here
#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/vector_angle.hpp"
#undef GLM_ENABLE_EXPERIMENTAL
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/geometric.hpp"

#include "sokol/sokol_log.h"
#include "stb/stb.cpp"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define DEFAULT_WIDTH 1280
#define DEFAULT_HEIGHT 720

// 10 Mb
#define MAIN_ALLOCATOR_SIZE 1024 * 1024 * 10
// 10 Kb
#define FRAME_ALLOCATOR_SIZE 1024 * 10

#define MAX_ENTITIES 128
#define MAX_QUADS 512

#define PLAYER_SPEED 100.0f

internal State state;

internal // @main
void game_main() {
    state = {
        .running = true,
        .width = DEFAULT_WIDTH,
        .height = DEFAULT_HEIGHT,
        .camera_view_width = 100.0f,
    };

    init(&state.allocator, MAIN_ALLOCATOR_SIZE);
    init(&state.frame_allocator, FRAME_ALLOCATOR_SIZE);

    state.entities = alloc<Entity>(&state.allocator, MAX_ENTITIES);
    state.quads = alloc<Quad>(&state.allocator, MAX_QUADS);

    platform_init_window(state.width, state.height, "Cool game");

    load_fonts();
    load_textures();

    renderer_init();

    create_entity({
        .flags = EF_NONE,
        .position = {0, 0},
        .size = {125, 75},
        .colour = WHITE,
    });

    create_entity({
        .flags = EF_NONE,
        .position = {0, 0},
        .size = {15, 15},
        .colour = with_alpha(BLUE, 0.5),
    });

    create_entity({
        .flags = EF_NONE,
        .position = {15, 15},
        .size = {10, 10},
        .colour = with_alpha(GREEN, 0.5),
    });

    create_entity({
        .flags = EF_PLAYER,
        .position = {0, 0},
        .size = {10, 10},
        .colour = with_alpha(RED, 0.5f),
    });

    f32 delta_time = 1.0 / 60.0f;

    while (state.running) {
        platform_process_events();
        update();
        physics(delta_time);
        draw();

        renderer_draw();

        reset(&state.frame_allocator);
    }

    // dont need to do this but who cares
    deinit(&state.allocator);
    deinit(&state.frame_allocator);

    sg_shutdown();
}

internal
void game_quit() {
    state.running = false;
}

// @update
internal void update() {
    if (state.keys[KEY_ESCAPE] == InputState::DOWN) {
        game_quit();
    }

    for (i64 entity_index = 0; entity_index < state.entity_count; ++entity_index) {
        Entity *entity = at_ptr(&state.entities, entity_index);

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
        Entity *entity = at_ptr(&state.entities, entity_index);
        entity->position += entity->velocity * delta_time;
    }
}

// @draw
internal void draw() {
    for (i64 entity_index = 0; entity_index < state.entity_count; ++entity_index) {
        Entity *entity = at_ptr(&state.entities, entity_index);

        draw_rectangle(entity->position, entity->size, entity->colour);

        if (entity->flags & EF_PLAYER) {
            draw_line(entity->position, {0, 0}, 1, RED);
        }
    }
}

internal 
Entity *create_entity(Entity entity) {
    Entity *entity_ptr = at_ptr(&state.entities, state.entity_count);

    *entity_ptr = entity;
    state.entity_count++;

    return entity_ptr;
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
            {.load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.6f, 0.75f, 0.8f, 1.0f }}
        }
    };
}

internal
void renderer_draw() {
    if (state.quad_count == 0) return;

    sg_update_buffer(
        state.bindings.vertex_buffers[0],
        { .ptr = state.quads.data, .size = sizeof(Quad) * state.quad_count }
    );

    sg_begin_pass({
        .action = state.pass_action,
        .swapchain = platform_swapchain()
    });


    sg_apply_pipeline(state.render_pipeline);
    sg_apply_bindings(state.bindings);

    sg_draw(0, (i32) state.quad_count * 6, 1);
    

    sg_end_pass();

    sg_commit();
    platform_present();

    state.quad_count = 0;
}

internal 
void draw_rectangle(glm::vec2 position, glm::vec2 size, Colour colour, f32 rotation) {
    draw_quad(position, size, rotation, colour, DrawType::RECTANGLE);
}

internal 
void draw_circle(glm::vec2 position, f32 radius, Colour colour) {
    draw_quad(position, {radius * 2, radius * 2}, 0, colour, DrawType::CIRCLE);
}

internal
void draw_line(glm::vec2 start, glm::vec2 end, f32 thickness, Colour colour) {
    glm::vec2 direction = end - start;
    f32 radians = glm::angle({0, 1.0f}, glm::normalize(direction));
    f32 degrees = glm::degrees(radians);

    glm::vec2 line_centre = start + (direction * 0.5f);
    f32 length = glm::length(direction);

    if (start.x > end.x) {
        degrees *= -1;
    }

    draw_rectangle(line_centre, {thickness, length}, colour, degrees);
}

internal 
void draw_text(Slice<char> text, glm::vec2 position, f32 font_size, Colour colour) {
    // Font size is the final height of the text drawn in world units
    // 10 font size == size{#chars * avg_char_width, font_size}

    if (text.length == 0) return;

    Slice<GlyphRenderInfo> infos = alloc<GlyphRenderInfo>(&state.frame_allocator, text.length);

    f32 total_x = 0;
    f32 total_y = 0;
    f32 max_height = 0;

    for (i64 i = 0; i < text.length; i++) {
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

    for (i64 i = 0; i < infos.length; i++) {
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
            DrawType::TEXT,
            info->uvs[0],
            info->uvs[1],
            info->uvs[2],
            info->uvs[3]
        );
    }
}

internal 
void draw_quad(glm::vec2 position, glm::vec2 size, f32 rotation, Colour colour, DrawType type) {
    draw_quad(position, size, rotation, colour, type, {0, 1}, {1, 1}, {1, 0}, {0, 0});
}

internal void draw_quad(glm::vec2 position, glm::vec2 size, f32 rotation, Colour colour, DrawType type, glm::vec2 top_left_uv, glm::vec2 top_right_uv, glm::vec2 bottom_right_uv, glm::vec2 bottom_left_uv) {
    Quad *quad = at_ptr(&state.quads, state.quad_count);
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
        case DrawType::RECTANGLE: {
            texture_index = 0;
            break;
        }
        case DrawType::CIRCLE: {
            texture_index = 1;
            break;
        }
        case DrawType::TEXT: {
            texture_index = 2;
            break;
        }
        default: {
            assert(0);
        }
    }

    quad->vertices[0] = {
        .position = {top_left.x, top_left.y, 0},
        .colour = colour,
        .texture_uv = {top_left_uv.x, top_left_uv.y},
        .texture_index = texture_index,
    };

    quad->vertices[1] = {
        .position = {top_right.x, top_right.y, 0},
        .colour = colour,
        .texture_uv = {top_right_uv.x, top_right_uv.y},
        .texture_index = texture_index,
    };

    quad->vertices[2] = {
        .position = {bottom_right.x, bottom_right.y, 0},
        .colour = colour,
        .texture_uv = {bottom_right_uv.x, bottom_right_uv.y},
        .texture_index = texture_index,
    };

    quad->vertices[3] = {
        .position = {bottom_left.x, bottom_left.y, 0},
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

    u8 *data = (u8 *)malloc(file_size + 1);
    assert(data);

    fread(data, file_size, 1, file);
    fclose(file);

    return Slice<u8> {.data = data, .length = file_size + 1};
}

internal 
void write_file(Slice<char> path, Slice<u8> buffer) {
    // TODO: using malloc for this and not freeing LUL
    FILE *file = fopen(path.data, "wb");
    assert(file);

    fwrite(buffer.data, buffer.length, 1, file);
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
void mouse_move_event(f32 x, f32 y) {
    state.mouse_screen_position = {x, y};
}

internal 
void mouse_button_event(MouseButton button, InputState input_state) {
    state.mouse_buttons[(i64) button] = input_state;
}

internal 
void key_event(Key key, InputState input_state) {
    state.keys[(i64) key] = input_state;
}

internal 
void window_resize(i32 width, i32 height) {
    state.width = width;
    state.height = height;
}

#endif
