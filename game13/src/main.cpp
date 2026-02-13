#include "libs/libs.h"
#include "ack.cpp"
#include "math.cpp"
#include "engine.cpp"

#include <cmath>
#include <cstring>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

// https://auburn.github.io/FastNoiseLite/

#define ALLOW_EDITOR 1
#define MAX_ENTITIES 1000

#define TEXT_INPUT_LEN 64

#define CHUNK_W 128
#define CHUNK_D 128
#define CHUNK_H 32

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
    v3 size;
    v3 rotation;
    v3 velocity;

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

    char input_buffer[TEXT_INPUT_LEN];

    struct {
        bool enabled;
        i32 range;
        i32 radius;
        f32 cooldown;
    } sculptor;
};

enum EntityFlags {
    EF_PLAYER           = 1 << 0,
    EF_LIGHT            = 1 << 1,
    EF_ANIMATED_SPRITE  = 1 << 2,
    EF_GRAVITY_AFFECTED = 1 << 3,
    EF_DELETE           = 1 << 16,
};

#define BLOCK_SIZE 1
#define V_BLOCK_SIZE v3{BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE}

enum class BlockType {
    AIR,
    STONE,
    DIRT,
    GRASS,
};

typedef v3i ChunkPosition;

struct Chunk {
    bool dirty;

    v3 position;
    v3 size;
    Mesh *mesh;
    slice<BlockType> blocks;
};

struct ChunkNoiseOptions {
    i32 seed;
    f32 frequency;
};

struct State {
    Camera camera;
    Window window;
    Renderer renderer;
    SoundEngine sound_engine;
    Editor editor;

    Chunk chunk;

    f64 time;

    StackArray<Entity, MAX_ENTITIES> entities;
} state = {};

void update_and_draw(f32 delta_time);
void physics(f32 delta_time);
void update_and_draw_editor(f32 delta_time);

v4 get_block_colour(BlockType type);

Chunk new_chunk(Renderer *renderer);
void generate_mesh(Chunk *chunk);

void generate_empty(Chunk *chunk);
void generate_terrain(Chunk *chunk, ChunkNoiseOptions options);

bool set_block(Chunk *chunk, ChunkPosition position, BlockType block);
void set_block_radius(Chunk *chunk, ChunkPosition centre, BlockType block, i32 radius);

ChunkPosition block_index_to_chunk_position(Chunk *chunk, i32 index);
i32 chunk_position_to_block_index(Chunk *chunk, ChunkPosition position);
BlockType get_block_neighbour(Chunk *chunk, ChunkPosition position, v3i offset);
ChunkPosition get_block_looking_at(Chunk *chunk, Camera camera, i32 range, bool *hit);
ChunkPosition world_to_chunk_position(v3 position);

bool save_chunk(Chunk *chunk, const char *name);
bool load_chunk(Chunk *chunk, const char *name);

Entity *spawn_entity(Entity entity);

Sprite *get_sprite(SpriteHandle handle);

int main() {
    state = State {
        .camera = {
            .mode = CameraMode::FIRST_PERSON,
            .fov = 110,
            .position = {CHUNK_W / 2, 30, CHUNK_D / 2},
            .near_plane = 0.1f,
            .far_plane = 500.0f,
            .orthographic_size = 75,
        },
        .window = {
            .mouse_captured = true, 
        },
        .renderer = {
            .clear_colour = v4{0.8, 1, 1, 1},
            .ambient_light = v3{0.6, 0.6, 0.6},
        },
        .editor = {
            .visable = false,
            .input_buffer = {},
            .sculptor = {
                .enabled = false,
                .range = 50,
                .radius = 4,
                .cooldown = 0.075,
            },
        },
    };

    { // init engine stuff
        bool ok = false;

        ok = init_window(&state.window, "game13");
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

    state.chunk = new_chunk(&state.renderer);
    generate_terrain(&state.chunk, ChunkNoiseOptions{.seed = 69420, .frequency = 0.05f});

#if 0
    bool loaded = load_chunk(&state.chunk, "main.chunk");
    if (!loaded) {
        Err("error loading chunk from file\n");
    }
#endif

    while (!glfwWindowShouldClose(state.window.glfw_window)) {
        f64 current_time    = state.time;
        f64 new_time        = glfwGetTime();
        f32 delta_time      = (f32) (new_time - current_time);
        state.time          = new_time;

        if (KEYS[GLFW_KEY_ESCAPE] == InputState::DOWN) {
            glfwSetWindowShouldClose(state.window.glfw_window, GLFW_TRUE);
        }

        new_frame(&state.renderer, &state.window, state.camera);

        poll_inputs(); 
        update_and_draw(delta_time);

#if ALLOW_EDITOR
        update_and_draw_editor(delta_time); 
#endif
        physics(delta_time);  

        if (state.chunk.dirty) {
            reset_mesh(state.chunk.mesh);
            generate_mesh(&state.chunk);
            upload_mesh(state.chunk.mesh);
            state.chunk.dirty = false;
        }

        draw_frame(&state.renderer, &state.window); 
        swap_buffers(&state.window);
    }

#if 0
    bool ok = save_chunk(&state.chunk, "main.chunk");
    if (!ok) {
        Err("error saving chunk to file\n");
    }
#endif

    glfwTerminate();

    return 0;
}

void update_and_draw(f32 delta_time) {
    if (!state.editor.visable) { // update editor camera
        state.camera.mode = CameraMode::FIRST_PERSON;

        v3 input = {};
    
        if (KEYS[GLFW_KEY_A] == InputState::PRESSED) {
            input.x -= 1;
        }
            
        if (KEYS[GLFW_KEY_D] == InputState::PRESSED) {
            input.x += 1;
        }
                
        if (KEYS[GLFW_KEY_SPACE] == InputState::PRESSED) {
            input.y += 1;
        }
                
        if (KEYS[GLFW_KEY_LEFT_SHIFT] == InputState::PRESSED) {
            input.y -= 1;
        }
            
        if (KEYS[GLFW_KEY_W] == InputState::PRESSED) {
            input.z += 1;
        }
         
        if (KEYS[GLFW_KEY_S] == InputState::PRESSED) {
            input.z -= 1;
        }
         
        const f32 FLY_SPEED = 15;
 
        v3 forward = get_forward_direction(state.camera);
        v3 up = {0, 1, 0};
        v3 right = get_right_direction(state.camera);
 
        state.camera.position += right * (input.x * FLY_SPEED * delta_time);
        state.camera.position += up * (input.y * FLY_SPEED * delta_time);
        state.camera.position += forward * (input.z * FLY_SPEED * delta_time);

        // update camera rotation (looking at)
        if (state.window.mouse_captured) {
            f32 sensitivity = 0.1;
            v2 mouse_input = MOUSE.delta;
    
            if(length(mouse_input) != 0) {
                state.camera.rotation += v3{mouse_input.y, mouse_input.x, 0} * sensitivity;
                state.camera.rotation.x = clamp(-90, state.camera.rotation.x, 90);
            }
        }
    }
}

// AABB detection for a point against a box where the position is centred on the box
bool point_collision(v3 point, v3 collider_position, v3 collider_size) {
    v3 delta_position = point - collider_position;
    v3 bounding_box = collider_size * 0.5;

    return (
        delta_position.x >= -bounding_box.x && delta_position.x < bounding_box.x &&
        delta_position.y >= -bounding_box.y && delta_position.y < bounding_box.y &&
        delta_position.z >= -bounding_box.z && delta_position.z < bounding_box.z
    );
}

struct CubeCollision {
    bool collision;
    v3 overlap;
    v3 distance;
};

CubeCollision cube_collision(v3 a_position, v3 a_size, v3 b_position, v3 b_size) {
    v3 distance = b_position - a_position;
    v3 distance_abs = v3{ABS(distance.x), ABS(distance.y), ABS(distance.z)};
    v3 distance_for_collision = (a_size + b_size) * 0.5; 

    bool collision = distance_for_collision.x >= distance_abs.x && distance_for_collision.y >= distance_abs.y && distance_for_collision.z >= distance_abs.z;
    v3 overlap = distance_for_collision - distance_abs;

    return CubeCollision {
        .collision = collision,
        .overlap = overlap,
        .distance = distance
    };
}

void physics(f32 delta_time) {
    for (int i = 0; i < state.entities.len; i++) {
        Entity* entity = &state.entities[i];

        const f32 MAX_SPEED = 50;
        if (length(entity->velocity) > MAX_SPEED) {
            entity->velocity = norm(entity->velocity) * MAX_SPEED;
        }

        entity->position += entity->velocity * delta_time;
    }
}

void update_and_draw_editor(f32 delta_time) {
    if (KEYS[GLFW_KEY_F1] == InputState::DOWN) {
        state.editor.visable = !state.editor.visable;
        set_mouse_captured(&state.window, !state.editor.visable);
    }

    if (state.editor.sculptor.enabled) {{ Scope
        bool hit = false;
        bool too_close = false;
        i32 radius = state.editor.sculptor.radius;
 
        ChunkPosition target_position = get_block_looking_at(&state.chunk, state.camera, state.editor.sculptor.range, &hit);
        if (!hit) {
            break;
        }
    
        v3 centre = state.chunk.position + as_floats(target_position);
    
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
 
                        draw_cube(&state.renderer, offset_position, {BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE}, {}, alpha(cube_colour, 0.4));
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
    
        if(MOUSE.buttons[GLFW_MOUSE_BUTTON_1] == InputState::PRESSED) {
            set_block_radius(&state.chunk, target_position, BlockType::AIR, radius);
            cooldown_timer = state.editor.sculptor.cooldown;
        }
        else if(MOUSE.buttons[GLFW_MOUSE_BUTTON_2] == InputState::PRESSED) {
            set_block_radius(&state.chunk, target_position, BlockType::STONE, radius);
            cooldown_timer = state.editor.sculptor.cooldown;
        }
    }}

    if(state.editor.visable) {
        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_NoDockingOverCentralNode);

        // ImGui::ShowDemoWindow();

        {
            ImGui::Begin("Inspector");
        
            { 
                v3 direction = get_forward_direction(state.camera);
                ImGui::Text("Looking: {%.2f, %.2f, %.2f}", direction.x, direction.y ,direction.z);
                ImGui::Text("FPS: %f", 1.0f / delta_time);
            }

            { // frame time plot
                const i64 BUFFER_SIZE = 1024;
                static f32 frame_times[BUFFER_SIZE] = {};

                for (i64 i = BUFFER_SIZE - 1; i > 0; i--) {
                    frame_times[i - 1] = frame_times[i];
                }

                frame_times[BUFFER_SIZE - 1] = delta_time * 1000;

                f32 average = 0;
                for (i64 i = 0 - 1; i < BUFFER_SIZE; i++) {
                    average += frame_times[i];
                }

                average /= (f32) BUFFER_SIZE;

                char overlay[32] = {};
                sprintf(overlay, "avg %f", average);

                ImGui::PlotLines("Frame times", frame_times, BUFFER_SIZE, 0, overlay, -1, 1, ImVec2(300, 100));
            }

            {
                ImGui::SeparatorText("Camera");
                state.camera.mode == CameraMode::FIRST_PERSON ? ImGui::Text("Mode: FP") : ImGui::Text("Mode: TP");
                ImGui::SliderFloat("FOV", &state.camera.fov, 1, 360);
                ImGui::SliderFloat3("Camera position", &state.camera.position[0], -50, 50);
                ImGui::SliderFloat3("Camera rotation", &state.camera.rotation[0], -360, 360);
                ImGui::SliderFloat("Ortho Size", &state.camera.orthographic_size, 1, 100);
            }

            {
                ImGui::SeparatorText("Renderer");

                if(ImGui::Button("Reload Shaders")) {
                    delete_shaders(&state.renderer);
                    load_shaders(&state.renderer);
                }

                ImGui::SameLine();
            
                if(ImGui::Button("Toggle V-sync")) {
                    toggle_vsync(&state.window);
                } 

                ImGui::SliderFloat4("Clear colour", &state.renderer.clear_colour[0], 0, 1);
                ImGui::SliderFloat3("Ambient light", &state.renderer.ambient_light[0], 0, 1);

                if(ImGui::CollapsingHeader("Render outputs")) {
                    ImVec2 image_size(360 * 1.777, 360);
    
                    ImGui::Text("g_buffer albedo");
                    ImGui::Image(state.renderer.g_buffer.albedo_attachment, image_size, ImVec2(0, 1), ImVec2(1, 0));
    
                    ImGui::Text("g_buffer depth");
                    ImGui::Image(state.renderer.g_buffer.depth_attachment, image_size, ImVec2(0, 1), ImVec2(1, 0));
                }
            }
       
            ImGui::End();
        }
    
        {
            ImGui::Begin("Tools");

            ImGui::SeparatorText("Chunk editor");

            if(ImGui::Button("Clear to empty")) {
                generate_empty(&state.chunk);
            }

            ImGui::SameLine();

            if (ImGui::CollapsingHeader("Save and Load")) {
                ImGui::TextDisabled("Name of chunk file in resources/chunks/");
                ImGui::InputTextWithHint("##", "creation.chunk", state.editor.input_buffer, TEXT_INPUT_LEN - 1);
    
                if(ImGui::Button("Save to file")) {
                    bool ok = save_chunk(&state.chunk, state.editor.input_buffer);
                    if (!ok) {
                        printf("error saving chunk to file\n");
                    }
                }
    
                ImGui::SameLine();
    
                if(ImGui::Button("Load from file")) {
                    bool ok = load_chunk(&state.chunk, state.editor.input_buffer);
                    if (!ok) {
                        printf("Error loading chunk from file\n");
                    }
                }
            }

            if (ImGui::CollapsingHeader("Generate blocks")) {
                static ChunkNoiseOptions options = {
                    .seed = 420,
                    .frequency = 0.1,
                };

                ImGui::SliderInt("Seed", &options.seed, 0, 5000);
                ImGui::SliderFloat("Frequency", &options.frequency, 0, 0.5);

                if(ImGui::Button("Generate")) {
                    generate_terrain(&state.chunk, options);
                }
            }

            {
                ImGui::SeparatorText("Sculptor");
                ImGui::PushID("Sculptor");

                ImGui::Checkbox("Enabled", &state.editor.sculptor.enabled);
                ImGui::SliderInt("Range", &state.editor.sculptor.range, 10, 200);
                ImGui::SliderInt("Radius", &state.editor.sculptor.radius, 1, 10);
                ImGui::SliderFloat("Cooldown", &state.editor.sculptor.cooldown, 0, 0.5);

                ImGui::PopID();
            }

            ImGui::End();
        }
    }
}

v4 get_block_colour(BlockType type) {
    switch (type) {
        case BlockType::AIR: return v4{0, 0, 0, 1}; break;
        case BlockType::STONE: return v4{0.5, 0.5, 0.5, 1}; break;
        case BlockType::DIRT: return v4{0.55, 0.25, 0.09, 1}; break;
        case BlockType::GRASS: return v4{0.2, 0.8, 0.1, 1}; break;
    }
}

Chunk new_chunk(Renderer *renderer) {
    v3 position = v3{0, 0, 0};
    v3 size = v3{CHUNK_W, CHUNK_H, CHUNK_D};

    i64 block_count = i64(size.x) * i64(size.y) * i64(size.z);

    return Chunk {
        .dirty = false,
        .position = position,
        .size = size,
        .mesh = new_mesh(renderer, position, block_count * 6),
        .blocks = slice_create_malloc<BlockType>(block_count),
    };
}

void generate_mesh(Chunk *chunk) {
    for(i64 i = 0; i < chunk->blocks.len; i++) {
        if(chunk->blocks[i] == BlockType::AIR) {
            continue;
        }

        ChunkPosition chunk_position = block_index_to_chunk_position(chunk, i);
        v3 position = as_floats(chunk_position);

        v4 colour = get_block_colour(chunk->blocks[i]);

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

            push_quad(chunk->mesh, positions, normals, colour);
        }

        if (down == BlockType::AIR) {
            v3 positions[4] = {
                position + front_bottom_left,
                position + front_bottom_right,
                position + back_bottom_right,
                position + back_bottom_left
            };

            v3 normals[4] = {down_normal, down_normal, down_normal, down_normal};

            push_quad(chunk->mesh, positions, normals, colour);
        }

        if (left == BlockType::AIR) {
            v3 positions[4] = {
                position + back_top_left,
                position + front_top_left,
                position + front_bottom_left,
                position + back_bottom_left
            };

            v3 normals[4] = {left_normal, left_normal, left_normal, left_normal};

            push_quad(chunk->mesh, positions, normals, colour);
        }


        if (right == BlockType::AIR) {
            v3 positions[4] = {
                position + front_top_right,
                position + back_top_right,
                position + back_bottom_right,
                position + front_bottom_right
            };

            v3 normals[4] = {right_normal, right_normal, right_normal, right_normal};

            push_quad(chunk->mesh, positions, normals, colour);
        }


        if (front == BlockType::AIR) {
            v3 positions[4] = {
                position + front_top_left,
                position + front_top_right,
                position + front_bottom_right,
                position + front_bottom_left
            };

            v3 normals[4] = {front_normal, front_normal, front_normal, front_normal};

            push_quad(chunk->mesh, positions, normals, colour);
        }


        if (back == BlockType::AIR) {
            v3 positions[4] = {
                position + back_top_right,
                position + back_top_left,
                position + back_bottom_left,
                position + back_bottom_right
            };

            v3 normals[4] = {back_normal, back_normal, back_normal, back_normal};

            push_quad(chunk->mesh, positions, normals, colour);
        }
    }
}

void generate_empty(Chunk *chunk) {
    memset(chunk->blocks.ptr, 0, sizeof(BlockType) * chunk->blocks.len);
    chunk->dirty = true;
}

void generate_terrain(Chunk *chunk, ChunkNoiseOptions options) {
    memset(chunk->blocks.ptr, 0, sizeof(BlockType) * chunk->blocks.len);
    chunk->dirty = true;

    FastNoiseLite noise;
    noise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    noise.SetSeed(options.seed);
    noise.SetFrequency(options.frequency);

    const i32 stone_height = 4;
    const i32 dirt_height = 2;
    const i32 max_hill_height = 20;
    const f32 min_noise_value = 0.2;

    // bottom stone layer
    for (i32 index = 0; index < chunk->blocks.len; index++) {
        ChunkPosition position = block_index_to_chunk_position(chunk, index);
        if (position.y <= stone_height) {
            chunk->blocks[index] = BlockType::STONE;
        }
        else if (position.y <= stone_height + dirt_height) {
            chunk->blocks[index] = BlockType::DIRT;
        }
    }

    for (i32 x = 0; x < chunk->size.x; x++) {
        for (i32 z = 0; z < chunk->size.z; z++) {
            f32 noise_value = noise.GetNoise(f32(x), f32(z));
            if (noise_value < min_noise_value) {
                noise_value = min_noise_value;
            }

            i32 terrain_height = i32(max_hill_height * noise_value);

            for (i32 y = 1; y < terrain_height; y++) {
                i32 index = chunk_position_to_block_index(chunk, ChunkPosition{x, stone_height + dirt_height + y, z});

                if (y == terrain_height - 1) {
                    chunk->blocks[index] = BlockType::GRASS;
                }
                else {
                    chunk->blocks[index] = BlockType::DIRT;
                }
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

ChunkPosition block_index_to_chunk_position(Chunk *chunk, i32 index) {
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

bool save_chunk(Chunk *chunk, const char *name) {
    u8 path_buffer[64] = {};
    i64 len = sprintf((char *) path_buffer, "resources/chunks/%s", name);
    slice<u8> path = slice_create(path_buffer, len);

    File file = new_file(path);

    bool ok = create_file(&file);
    if (!ok) {
        return false;
    }

    ok = write_file(&file, bytes_from_ptr(&chunk->size));
    if (!ok) {
        return false;
    }

    ok = write_file(&file, slice_to_bytes(chunk->blocks));
    if (!ok) {
        return false;
    }

    close_file(&file);
}

bool load_chunk(Chunk *chunk, const char *name) {
    const i64 SIZE_START    = 0;
    const i64 SIZE_END      = SIZE_START + sizeof(v3);

    const i64 BLOCKS_START  = SIZE_END;
    const i64 BLOCKS_END    = BLOCKS_START + (chunk->blocks.len * sizeof(BlockType));

    u8 path_buffer[64] = {};
    i64 len = sprintf((char *) path_buffer, "resources/chunks/%s", name);
    slice<u8> path = slice_create(path_buffer, len);

    File file = new_file(path);
    slice<u8> bytes = read_entire_file(&file);

    if (bytes.len == 0) {
        printf("Failed reading file: \"%s\"\n", path.c());
        return false;
    }

    slice<u8> size_bytes    = slice_range(bytes, SIZE_START, SIZE_END);
    slice<u8> block_bytes   = slice_range(bytes, BLOCKS_START, BLOCKS_END);

    v3 *size                = bytes_to_ptr<v3>(size_bytes);
    slice<BlockType> blocks = slice_from_bytes<BlockType>(block_bytes);

    chunk->size = *size;

    for (i64 i = 0; i < chunk->blocks.len; i++) {
        chunk->blocks[i] = blocks[i];
    }

    chunk->dirty = true;

    return true;
}

Entity *spawn_entity(Entity entity) {
    Entity *ptr = push(&state.entities);
    *ptr = entity;

    return ptr;
}

Sprite *get_sprite(SpriteHandle handle) {
    if(handle == SH_NONE) {
        return NULL;
    }

    return sprites[handle];
}
