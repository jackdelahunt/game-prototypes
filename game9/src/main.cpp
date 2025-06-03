#include "libs/libs.h"
#include "ack.cpp"
#include "math.cpp"
#include "engine.cpp"

#include <cmath>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

// cursed c++ headers to get saving working
#include <vector>
#include <fstream>
#include <string>

// Total: 30:00
// Started: 01:30

// Performance (20 * 20 * 20):
// start                            == ~21 fps  - 48,000 quads
// no block unless touching air     == ~78 fps  - 13,008 quads
// no face unless touching air      == ~350 fps - 2,400 quads

// Profilling (150 * 150 * 150)
// .cutoff = 0.31, .frequency = 0.05
// profile_1 = ~17 fps
// profile_2 = ~35 fps

#define ALLOW_EDITOR 1
#define MAX_ENTITIES 2000
#define DEFAULT_SAVE_FILE "resources/saves/scene.json"

enum SpriteHandle {
    SH_NONE,
    SH_BRICK,
    SH_COUNT_
};

Sprite *sprites[SH_COUNT_];

struct Entity {
    // meta
    u64 flags;

    // entity
    v3 position;
    v2 size;
    f32 rotation;
    v2 velocity;

    // rendering
    v4 color;
    SpriteHandle sprite;

    // animated texture
    f32 animation_cycle;

    // light
    v4 light_colour;
    f32 light_intensity;
    f32 light_radius;
};

struct Editor {
    bool visable;

    struct {
        i32 range;
        i32 radius;
        f32 cooldown;
    } sculptor;
};

enum EntityFlags {
    EF_PLAYER           = 1 << 0,
    EF_LIGHT            = 1 << 1,
    EF_ANIMATED_SPRITE  = 1 << 2,
    EF_DELETE           = 1 << 16,
};

#define BLOCK_SIZE 1

enum class BlockType {
    AIR,
    BRICK
};

typedef v3i ChunkPosition;

struct Chunk {
    bool dirty;

    v3 position;
    v3 size;
    Mesh mesh;
    Slice<BlockType> blocks;
};

struct State {
    Camera camera;
    Window window;
    Renderer renderer;
    SoundEngine sound_engine;
    Editor editor;

    i64 chunk_quads_last_frame;
    i64 im_quads_last_frame;

    StackArray<Chunk, 10> chunks;

    struct {
        f32 cutoff;
        f32 frequency;
    } noise;

    f64 time;

    StackArray<Entity, MAX_ENTITIES> entities;
} state = {};

struct CollisionIterator {
    Entity* entity;
    i64 index;
};

void update_and_draw(f32 delta_time);
void physics(f32 delta_time);
void update_and_draw_editor(f32 delta_time);

Chunk new_chunk(v3i position, v3i size);
void generate_mesh(Chunk *chunk);
void generate_blocks(Chunk *chunk);
bool set_block(Chunk *chunk, ChunkPosition position, BlockType block);
void set_block_radius(Chunk *chunk, ChunkPosition centre, BlockType block, i32 radius);
ChunkPosition block_index_to_chunk_position(Chunk *chunk, i64 index);
i32 chunk_position_to_block_index(Chunk *chunk, ChunkPosition position);
BlockType get_block_neighbour(Chunk *chunk, ChunkPosition position, v3i offset);
ChunkPosition get_block_looking_at(Chunk *chunk, Camera camera, i32 range, bool *hit);
ChunkPosition world_to_chunk_position(v3 position);

Entity *spawn_entity(Entity entity);

void to_json(json& j, const Entity& entity);
void from_json(const json& j, Entity& entity);

void backup_scene();
void save_scene(State *state);
void load_scene(State *state);

CollisionIterator new_collision_iterator(Entity *entity);
Entity *next(CollisionIterator *iterator);

Sprite *get_sprite(SpriteHandle handle);

int main() {
    state = State {
        .camera = {
            .fov = 110,
            .position = {10, 10, -20},
            .rotation = {0, 0, 0},
            .orthographic_size = 5,
            .near_plane = 0.1f,
            .far_plane = 1000.0f,
        },
        .window = {
            .mouse_captured = true, 
        },
        .renderer = {
            .clear_colour = {0.8, 1, 1, 1},
            .ambient_light = {0.5, 0.5, 0.5, 1},
            .sun_colour = {0.8, 0.8, 0.5, 1},
            .sun_direction = {-0.25, 0.6, -0.5},
        },
        .editor = {
            .visable = false,
            .sculptor = {
                .range = 50,
                .radius = 4,
                .cooldown = 0.075,
            },
        },
        .noise = {
            .cutoff = 0.1,
            .frequency = 0.05
        },
    };

    { // init engine stuff
        bool ok = false;

        ok = init_window(&state.window, "game9");
        if (!ok) {
            printf("failed to init window\n");
            return 1;
        }

        ok = init_renderer(&state.renderer, &state.window);
        if (!ok) {
            printf("failed to init the renderer\n");
            return 1;
        }

        { // load and build all sprites
            Sprite *sprite = NULL;

            sprite = load_sprite(&state.renderer, "resources/textures/brick.png", "");
            if (sprite == NULL) {
                return 1;
            }

            sprites[SH_BRICK] = sprite;
        }

        ok = build_atlas(&state.renderer);
        if (!ok) {
            printf("failed to build texture atlas\n");
            return 1;
        }

        ok = load_font(&state.renderer, "resources/fonts/LibreBaskerville.ttf", 1000, 1000, 160);
        if (!ok) {
            printf("failed to load font\n");
            return 1;
        }

        ok = init_sound_engine(&state.sound_engine);
        if (!ok) {
            printf("failed to init sound engine\n");
            return 1;
        }

        ok = load_sounds(&state.sound_engine);
        if (!ok) {
            printf("failed to load sounds\n");
            return 1;
        } 

        srand(time(NULL));
    }

    while (!glfwWindowShouldClose(state.window.glfw_window)) {
        f64 current_time    = state.time;
        f64 new_time        = glfwGetTime();
        f32 delta_time      = (f32) (new_time - current_time);
        state.time          = new_time;

        if (KEYS[GLFW_KEY_ESCAPE] == InputState::down) {
            glfwSetWindowShouldClose(state.window.glfw_window, GLFW_TRUE);
        }

        state.im_quads_last_frame = state.renderer.quads.len;
        new_frame(&state.renderer, &state.window, state.camera);

        poll_inputs(); 
        update_and_draw(delta_time);
#if ALLOW_EDITOR
        update_and_draw_editor(delta_time); 
#endif
        physics(delta_time);  

        for (Chunk &chunk : state.chunks) {
            if (chunk.dirty) {
                reset_mesh(&chunk.mesh);
                generate_mesh(&chunk);
                upload_mesh(&chunk.mesh);
                chunk.dirty = false;
            }

            draw_mesh(&state.renderer, &chunk.mesh, chunk.position); 
        }

        draw_frame(&state.renderer, &state.window, state.camera); 

        swap_buffers(&state.window);
    }

    glfwTerminate();

    return 0;
}

void update_and_draw(f32 delta_time) {
    draw_cube(&state.renderer, {}, {1, 1}, RED);
    draw_cube(&state.renderer, {10, 0, 0}, {1, 1}, BLUE);

    { // update camera position x and z
        v2 input = {};
    
        if (KEYS[GLFW_KEY_W] == InputState::pressed) {
            input.y += 1;
        }
    
        if (KEYS[GLFW_KEY_S] == InputState::pressed) {
            input.y -= 1;
        }
    
        if (KEYS[GLFW_KEY_A] == InputState::pressed) {
            input.x -= 1;
        }
    
        if (KEYS[GLFW_KEY_D] == InputState::pressed) {
            input.x += 1;
        }
    
        const f32 move_speed = 10;
        v3 forward = get_forward_direction(state.camera);
        v3 right = get_right_direction(state.camera);
    
        if(input.y != 0) {
            state.camera.position += forward * (input.y * move_speed * delta_time);
        }
    
        if(input.x != 0) {
            state.camera.position += right * (input.x * move_speed * delta_time);
        }
    }

    { // update camera position y 
        f32 input = {};
    
        if (KEYS[GLFW_KEY_SPACE] == InputState::pressed) {
            input += 1;
        }
    
        if (KEYS[GLFW_KEY_LEFT_SHIFT] == InputState::pressed) {
            input -= 1;
        }
    
        const f32 move_speed = 10;

        if(input != 0) {
            v3 up = get_up_direction(state.camera);
            state.camera.position += up * (input * move_speed * delta_time);
        }
    }

    // update camera rotation (looking at)
    if (state.window.mouse_captured) {
        f32 sensitivity = 0.1;
        v2 mouse_input = MOUSE.delta;

        if(length(mouse_input) != 0) {
            // max the mouse delta vector can be, stops huge spikes mouse input 
            // when mouse changes capture like at start of game
            // - 02/06/25
            f32 max_delta = 75;

            if (length(mouse_input) > max_delta) {
                mouse_input = norm(mouse_input) * max_delta;
            }
    
            state.camera.rotation += v3{mouse_input.y, mouse_input.x, 0} * sensitivity;
            state.camera.rotation.x = clamp(-90, state.camera.rotation.x, 90);
        }
    }
}

void physics(f32 delta_time) {
    for (int i = 0; i < state.entities.len; i++) {
        Entity* entity = &state.entities[i];

        entity->position.x += entity->velocity.x * delta_time;
        entity->position.y += entity->velocity.y * delta_time;
    }
}

void update_and_draw_editor(f32 delta_time) {
    if (KEYS[GLFW_KEY_F1] == InputState::down) {
        state.editor.visable = !state.editor.visable;
        set_mouse_captured(&state.window, !state.editor.visable);
    }

    for (Chunk &chunk : state.chunks) {
        bool hit = false;
        bool too_close = false;
        i32 radius = state.editor.sculptor.radius;

        ChunkPosition target_position = get_block_looking_at(&chunk, state.camera, state.editor.sculptor.range, &hit);
        if (!hit) {
            continue;
        }

        v3 centre = chunk.position + as_floats(target_position);

        if (length(state.camera.position - centre) < 1.5 * (f32) radius) {
            too_close = true;
        }

        i32 start_offset = -state.editor.sculptor.radius;
        i32 end_offset = -start_offset;

        for (i32 z = start_offset; z <= end_offset; z++) {
            for (i32 y = start_offset; y <= end_offset; y++) {
                for (i32 x = start_offset; x <= end_offset; x++) {
                    v3 offset_position = centre + as_floats(v3i{x, y, z});
                    f32 distance = length(offset_position - centre);
                   
                    // draw if in radius but also draw if close to the edge of the radius
                    // this reduces the amount of cubes we are drawing for no reason - 02/06/25
                    if (distance <= (f32) radius && distance > f32(radius - BLOCK_SIZE)) {
                        v4 cube_colour = GREEN;
                        if (too_close) {
                            cube_colour = RED;
                        }

                        draw_cube(&state.renderer, offset_position, {BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE}, alpha(cube_colour, 0.4));
                    }
                }
            }
        }


        static f32 cooldown_timer = 0;
        
        cooldown_timer -= delta_time;
        if (cooldown_timer < 0) {
            cooldown_timer = 0;
        }

        // check all these here because we still want to see the brush cubes
        // even though it is disabled for whatever reason
        if(state.editor.visable || too_close || cooldown_timer > 0) {
            break;
        }

        if(MOUSE.buttons[GLFW_MOUSE_BUTTON_1] == InputState::pressed) {
            set_block_radius(&chunk, target_position, BlockType::AIR, radius);
            cooldown_timer = state.editor.sculptor.cooldown;
        }
        else if(MOUSE.buttons[GLFW_MOUSE_BUTTON_2] == InputState::pressed) {
            set_block_radius(&chunk, target_position, BlockType::BRICK, radius);
            cooldown_timer = state.editor.sculptor.cooldown;
        }
    }

    if(state.editor.visable) {
        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_NoDockingOverCentralNode);

        {
            ImGui::Begin("Inspector");
        
            { 
                v3 direction = get_forward_direction(state.camera);
                ImGui::Text("Looking: {%.2f, %.2f, %.2f}", direction.x, direction.y ,direction.z);
                ImGui::Text("Chunk quads: %llu", state.chunk_quads_last_frame);
                ImGui::Text("IM quads: %llu", state.im_quads_last_frame);
                ImGui::Text("Total triangles: %llu", (state.im_quads_last_frame + state.chunk_quads_last_frame) * 2);
                ImGui::Text("FPS: %f", 1.0f / delta_time);
            }

            {
                ImGui::SeparatorText("Camera");
                ImGui::SliderFloat("FOV", &state.camera.fov, 1, 360);
                ImGui::SliderFloat3("Camera position", &state.camera.position[0], -50, 50);
                ImGui::SliderFloat3("Camera rotation", &state.camera.rotation[0], -360, 360);
                ImGui::SliderFloat("Orthographic size", &state.camera.orthographic_size, 10, 2000);
                ImGui::SliderFloat4("Clear colour", &state.renderer.clear_colour[0], 0, 1);
                ImGui::SliderFloat4("Ambient light", &state.renderer.ambient_light[0], 0, 1);
                ImGui::SliderFloat4("Sun colour", &state.renderer.sun_colour[0], 0, 1);
                ImGui::SliderFloat3("Sun direction", &state.renderer.sun_direction[0], -1, 1);
            }
       
            {
                ImGui::SeparatorText("Chunk generation");
                ImGui::SliderFloat("Cutoff", &state.noise.cutoff, 0, 1);
                ImGui::SliderFloat("Frequency", &state.noise.frequency, 0, 0.4);
            }

            {
                ImGui::SeparatorText("Chunk list");
                for (i64 i = 0; i < state.chunks.len; i++) {
                    Chunk *chunk = &state.chunks[i];

                    ImGui::PushID(i);

                    if(ImGui::Button("TP")) {
                        chunk->position = as_floats(world_to_chunk_position(state.camera.position));
                    }

                    ImGui::InputFloat3("Position", &chunk->position[0]);
                    ImGui::InputFloat3("Size", &chunk->size[0]);
                    ImGui::PopID();
                }
            }
        
            ImGui::End();
        }
    
        {
            ImGui::Begin("Tools");

            ImGui::SeparatorText("Chunk builder");

            static struct {
                v3i position = {0, 0, 0};
                v3i size = {50, 50, 50};
            } builder;  

            ImGui::InputInt3("Position", &builder.position[0]);
            ImGui::InputInt3("Size", &builder.size[0]);

            if(ImGui::Button("Build")) {
                Chunk *chunk = push(&state.chunks);
                *chunk = new_chunk(builder.position, builder.size);

                generate_blocks(chunk);
                generate_mesh(chunk);
                upload_mesh(&chunk->mesh);
            }

            ImGui::SeparatorText("Sculptor");
            ImGui::SliderInt("Range", &state.editor.sculptor.range, 10, 200);
            ImGui::SliderInt("Radius", &state.editor.sculptor.radius, 1, 10);
            ImGui::SliderFloat("Cooldown", &state.editor.sculptor.cooldown, 0, 0.5);

            ImGui::End();
        }

        {
            ImGui::Begin("Options");
            if(ImGui::Button("Reload Shaders")) {
                delete_shaders(&state.renderer);
                load_shaders(&state.renderer);
            }
        
            if(ImGui::Button("Save scene")) {
                save_scene(&state);
            }
        
            ImGui::SameLine();
            if(ImGui::Button("Load scene")) {
                load_scene(&state);
            }
    
            if(ImGui::Button("Toggle wireframe")) {
                toggle_wireframe(&state.renderer);
            }
    
            if(ImGui::Button("Toggle V-sync")) {
                toggle_vsync(&state.window);
            }
    
            ImGui::Separator();
    
            if(ImGui::CollapsingHeader("Render outputs")) {
                ImVec2 image_size(360 * 1.777, 360);
    
                ImGui::Text("Depth buffer");
                ImGui::Image(state.renderer.unlit_frame_buffer.depth_attachment, image_size, ImVec2(0, 1), ImVec2(1, 0));

                ImGui::Text("Normal buffer");
                ImGui::Image(state.renderer.unlit_frame_buffer.normals_attachment, image_size, ImVec2(0, 1), ImVec2(1, 0));

                ImGui::Text("Colour buffer");
                ImGui::Image(state.renderer.unlit_frame_buffer.colour_attachment, image_size, ImVec2(0, 1), ImVec2(1, 0));
            }

            ImGui::End();
        }
    }
}

Chunk new_chunk(v3i position, v3i size) {
    i64 blocks = size.x * size.y * size.z;

    return Chunk {
        .dirty = false,
        .position = as_floats(position),
        .size = as_floats(size),
        .mesh = new_mesh(as_floats(position), blocks * 6),
        .blocks = mem_alloc<BlockType>(blocks)
    };
}

void generate_mesh(Chunk *chunk) {
    Sprite *sprite = get_sprite(SH_BRICK);

    for(i64 i = 0; i < chunk->blocks.len; i++) {
        if(chunk->blocks[i] == BlockType::AIR) {
            continue;
        }

        ChunkPosition chunk_position = block_index_to_chunk_position(chunk, i);
        v3 position = as_floats(chunk_position);

        v4 colour = {0.8, position.y / (f32) chunk->size.y, 0.3, 1};
        // v4 colour = WHITE;

        BlockType up    = get_block_neighbour(chunk, chunk_position, {0, 1, 0});
        BlockType down  = get_block_neighbour(chunk, chunk_position, {0, -1, 0});
        BlockType left  = get_block_neighbour(chunk, chunk_position, {-1, 0, 0});
        BlockType right = get_block_neighbour(chunk, chunk_position, {1, 0, 0});
        BlockType front = get_block_neighbour(chunk, chunk_position, {0, 0, -1});
        BlockType back  = get_block_neighbour(chunk, chunk_position, {0, 0, 1});

        // order of vertices in a quad is:
        // top_left
        // top_right
        // bottom_rigth
        // bottom_left

        // TODO: not actually taking into account the size of the cubes here so
        // they are always drawn as 1x1x1

        v3 front_top_left       = {-0.5, 0.5, -0.5};
        v3 front_top_right      = {0.5, 0.5, -0.5};
        v3 front_bottom_right   = {0.5, -0.5, -0.5};
        v3 front_bottom_left    = {-0.5, -0.5, -0.5};

        v3 back_top_left       = {-0.5, 0.5, 0.5};
        v3 back_top_right      = {0.5, 0.5, 0.5};
        v3 back_bottom_right   = {0.5, -0.5, 0.5};
        v3 back_bottom_left    = {-0.5, -0.5, 0.5};

        v3 up_normal = {0, 1, 0};
        v3 down_normal = {0, -1, 0};
        v3 left_normal = {-1, 0, 0};
        v3 right_normal = {1, 0, 0};
        v3 front_normal = {0, 0, -1};
        v3 back_normal = {0, 0, 1};

        if (up == BlockType::AIR) {
            v3 positions[4] = {
                position + back_top_left,
                position + back_top_right,
                position + front_top_right,
                position + front_top_left
            };

            v3 normals[4] = {up_normal, up_normal, up_normal, up_normal};

            push_quad(&chunk->mesh, positions, normals, colour, sprite->albedo->uvs, state.renderer.default_normal->uvs);
        }

        if (down == BlockType::AIR) {
            v3 positions[4] = {
                position + front_bottom_left,
                position + front_bottom_right,
                position + back_bottom_right,
                position + back_bottom_left
            };

            v3 normals[4] = {down_normal, down_normal, down_normal, down_normal};

            push_quad(&chunk->mesh, positions, normals, colour, sprite->albedo->uvs, state.renderer.default_normal->uvs);
        }

        if (left == BlockType::AIR) {
            v3 positions[4] = {
                position + back_top_left,
                position + front_top_left,
                position + front_bottom_left,
                position + back_bottom_left
            };

            v3 normals[4] = {left_normal, left_normal, left_normal, left_normal};

            push_quad(&chunk->mesh, positions, normals, colour, sprite->albedo->uvs, state.renderer.default_normal->uvs);
        }


        if (right == BlockType::AIR) {
            v3 positions[4] = {
                position + front_top_right,
                position + back_top_right,
                position + back_bottom_right,
                position + front_bottom_right
            };

            v3 normals[4] = {right_normal, right_normal, right_normal, right_normal};

            push_quad(&chunk->mesh, positions, normals, colour, sprite->albedo->uvs, state.renderer.default_normal->uvs);
        }


        if (front == BlockType::AIR) {
            v3 positions[4] = {
                position + front_top_left,
                position + front_top_right,
                position + front_bottom_right,
                position + front_bottom_left
            };

            v3 normals[4] = {front_normal, front_normal, front_normal, front_normal};

            push_quad(&chunk->mesh, positions, normals, colour, sprite->albedo->uvs, state.renderer.default_normal->uvs);
        }


        if (back == BlockType::AIR) {
            v3 positions[4] = {
                position + back_top_right,
                position + back_top_left,
                position + back_bottom_left,
                position + back_bottom_right
            };

            v3 normals[4] = {back_normal, back_normal, back_normal, back_normal};

            push_quad(&chunk->mesh, positions, normals, colour, sprite->albedo->uvs, state.renderer.default_normal->uvs);
        }
    }
}

void generate_blocks(Chunk *chunk) {
    memset(chunk->blocks.ptr, 0, sizeof(BlockType) * chunk->blocks.len);

    FastNoiseLite noise;
    noise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    noise.SetFrequency(state.noise.frequency);

    i64 index = 0;

    for (i64 z = 0; z < chunk->size.z; z++) {
        for (i64 y = 0; y < chunk->size.y; y++) {
            for (i64 x = 0; x < chunk->size.x; x++) {
                f32 n = noise.GetNoise((f32) x, (f32) y, (f32) z);
                    
                if (n > state.noise.cutoff) {
                    chunk->blocks[index] = BlockType::BRICK;
                }

                index++;
            }
        }
    }
}

bool set_block(Chunk *chunk, ChunkPosition position, BlockType block) {
    i32 index = chunk_position_to_block_index(chunk, position);

    if (index < 0 || index >= chunk->blocks.len) {
        return false;
    }

    chunk->blocks[index] = block;
    chunk->dirty = true;
}

void set_block_radius(Chunk *chunk, ChunkPosition centre, BlockType block, i32 radius) {
    for (i32 z = -radius; z <= radius; z++) {
        for (i32 y = -radius; y <= radius; y++) {
            for (i32 x = -radius; x <= radius; x++) {
                ChunkPosition block_position = centre + v3i{x, y, z};
                 
                if (length(as_floats(block_position) - as_floats(centre)) <= (f32) radius) {
                    set_block(chunk, block_position, block);
                }
            }
        }
    }
}

ChunkPosition block_index_to_chunk_position(Chunk *chunk, i64 index) {
    v3i size = {
        (i32) chunk->size.x,
        (i32) chunk->size.y,
        (i32) chunk->size.z,
    };

    i32 x = index % size.x;
    i32 y = (index / size.z) % size.y;
    i32 z = index / (size.y * size.z);

    return {x, y, z};
}

i32 chunk_position_to_block_index(Chunk *chunk, ChunkPosition position) {
    return position.x + (chunk->size.x * position.y) + (chunk->size.x * chunk->size.y * position.z);
}

BlockType get_block_neighbour(Chunk *chunk, ChunkPosition position, v3i offset) {
    ChunkPosition neighbour = position + offset;

    if(neighbour.x < 0 || neighbour.y < 0 || neighbour.z < 0) {
        return BlockType::AIR;
    }

    if(neighbour.x >= chunk->size.x || neighbour.y >= chunk->size.y || neighbour.z >= chunk->size.z) {
        return BlockType::AIR;
    }

    i64 neighbour_index = chunk_position_to_block_index(chunk, neighbour);
    return chunk->blocks[neighbour_index];
}


ChunkPosition get_block_looking_at(Chunk *chunk, Camera camera, i32 range, bool *hit) {
    v3 forward = get_forward_direction(state.camera);

    for (i64 i = 0; i < range; i++) {
        v3 check_position = state.camera.position + (forward * f32(i * BLOCK_SIZE));
        v3 delta_position = check_position - chunk->position;

        if (delta_position.x >= 0 && delta_position.x < chunk->size.x &&
            delta_position.y >= 0 && delta_position.y < chunk->size.y &&
            delta_position.z >= 0 && delta_position.z < chunk->size.z) {

            ChunkPosition chunk_position = world_to_chunk_position(delta_position);
            i32 index = chunk_position_to_block_index(chunk, chunk_position);

            if (chunk->blocks[index] != BlockType::AIR) {
                *hit = true;
                return chunk_position;
            }
        }
    }

    *hit = false;
    return {};
}

ChunkPosition world_to_chunk_position(v3 position) {
    return ChunkPosition {
        (i32) floor(position.x),
        (i32) floor(position.y),
        (i32) floor(position.z),
    };
}

Entity *spawn_entity(Entity entity) {
    Entity *ptr = push(&state.entities);
    *ptr = entity;

    return ptr;
}

void backup_scene() {
    if (!file_exists(DEFAULT_SAVE_FILE)) {
        return;
    }

    char backup_path[128];
    sprintf(backup_path, "resources/saves/backups/scene_%llu.json", rand_i64());

    bool saved_backup = copy_file(DEFAULT_SAVE_FILE, backup_path);
    if (saved_backup) {
        printf("Created backup save to \"%s\"\n", backup_path);
    }
}

void save_scene(State *state) {
    backup_scene(); 

    std::vector<Entity> entities_copy(state->entities.len);

    for(i64 i = 0; i < state->entities.len; i++) {
        entities_copy[i] = state->entities[i];
    }

    json j;
    j["entities"] = entities_copy;

    { // save to file
        std::ofstream file(DEFAULT_SAVE_FILE);

        std::string output = j.dump(2);
        i64 bytes = output.size();

        file << output;
        file.close();

        printf("Created save to \"%s\" [%llu bytes]\n", DEFAULT_SAVE_FILE, bytes);
    }
}


void load_scene(State *state) {
    std::string saved_data = read_entire_file(DEFAULT_SAVE_FILE);
    if (saved_data.size() == 0) {
        printf("Could not load scene file at \"%s\"\n", DEFAULT_SAVE_FILE);
        return;
    }

    json j = json::parse(saved_data, nullptr, false);
    if (j.is_discarded() || !j.contains("entities")) {
        printf("Failed to parse scene JSON or 'entities' not found.\n");
        return;
    }

    std::vector<Entity> loaded_entities = j["entities"].get<std::vector<Entity>>();

    // Assuming state->entities is a resizable container or has assign function
    state->entities.len = loaded_entities.size();
    for (size_t i = 0; i < loaded_entities.size(); ++i) {
        state->entities[i] = loaded_entities[i];
    }

    printf("Loaded scene from \"%s\" with %llu entities\n", DEFAULT_SAVE_FILE, (u64)loaded_entities.size());
}

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(v2, x, y)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(v3, x, y, z)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(v4, x, y, z, w)

void to_json(json& j, const Entity& entity) {
    j = json{
        {"flags",           entity.flags},
        {"position",        entity.position},
        {"size",            entity.size},
        {"rotation",        entity.rotation},
        {"velocity",        entity.velocity},
        {"color",           entity.color},
        {"sprite",          entity.sprite},
        {"animation_cycle", entity.animation_cycle},
        {"light_colour",    entity.light_colour},
        {"light_intensity", entity.light_intensity},
        {"light_radius",    entity.light_radius},
    };
}


void from_json(const json& j, Entity& entity) {
    j.at("flags").get_to(entity.flags);
    j.at("position").get_to(entity.position);
    j.at("size").get_to(entity.size);
    j.at("rotation").get_to(entity.rotation);
    j.at("velocity").get_to(entity.velocity);
    j.at("color").get_to(entity.color);
    j.at("sprite").get_to(entity.sprite);
    j.at("animation_cycle").get_to(entity.animation_cycle);
    j.at("light_colour").get_to(entity.light_colour);
    j.at("light_intensity").get_to(entity.light_intensity);
    j.at("light_radius").get_to(entity.light_radius);
}

CollisionIterator new_collision_iterator(Entity *entity) {
    return CollisionIterator {
        .entity = entity,
        .index = 0,
    };
}

Entity *next(CollisionIterator *iterator) {
    while (iterator->index < state.entities.len) {
        Entity *entity = iterator->entity;
        Entity *other = &state.entities[iterator->index];

        iterator->index++;

        { // basic aabb collision
            v2 distance = other->position.xy - entity->position.xy;
            v2 distance_abs = v2{ABS(distance.x), ABS(distance.y)};
            v2 distance_for_collision = (entity->size + other->size) * v2{0.5, 0.5};

            bool collision = distance_for_collision[0] >= distance_abs[0] && distance_for_collision[1] >= distance_abs[1];
            if (collision) {
                return other;
            }
        }
    }

    return nullptr;
}


Sprite *get_sprite(SpriteHandle handle) {
    if(handle == SH_NONE) {
        return NULL;
    }

    return sprites[handle];
}
