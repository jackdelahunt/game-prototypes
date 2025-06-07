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

// cursed c++ headers to get saving working
#include <vector>
#include <fstream>
#include <string>

// Total: 50:00
// Started: 15:00

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
    BRICK
};

typedef v3i ChunkPosition;

struct Chunk {
    bool dirty;

    v3 position;
    v3 size;
    Mesh *mesh;
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

    f32 gravity;
    bool game_running;

    struct {
        f32 cutoff;
        f32 frequency;
    } noise;

    f64 time;

    StackArray<Entity, MAX_ENTITIES> entities;
} state = {};

void update_and_draw(f32 delta_time);
void physics(f32 delta_time);
void update_and_draw_editor(f32 delta_time);

Chunk new_chunk(Renderer *renderer, v3i position, v3i size);
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

Sprite *get_sprite(SpriteHandle handle);

int main() {
    state = State {
        .camera = {
            .mode = CameraMode::FIRST_PERSON,
            .fov = 110,
            .position = {10, 10, -20},
            .near_plane = 0.1f,
            .far_plane = 500.0f,
            .orthographic_size = 75,
        },
        .window = {
            .mouse_captured = true, 
        },
        .renderer = {
            .clear_colour = {0.8, 1, 1, 1},
            .ambient_light = v3{0.6, 0.6, 0.6},
            .sun_colour = v3{1, 1, 1},
            .sun_position = {100, 100, -100},
        },
        .editor = {
            .visable = false,
            .sculptor = {
                .enabled = false,
                .range = 50,
                .radius = 4,
                .cooldown = 0.075,
            },
        },
        .gravity = 1,
        .game_running = false,
        .noise = {
            .cutoff = 0.1,
            .frequency = 0.1
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

    spawn_entity(Entity {
        .flags = EF_PLAYER | EF_GRAVITY_AFFECTED,
        .position = {25, 150, 0},
        .size = {1.5, 0.5, 2}
    });


    Chunk *chunk = push(&state.chunks);
    *chunk = new_chunk(&state.renderer, {}, {30, 100, 30});

    generate_blocks(chunk);
    generate_mesh(chunk);
    upload_mesh(chunk->mesh);

    while (!glfwWindowShouldClose(state.window.glfw_window)) {
        f64 current_time    = state.time;
        f64 new_time        = glfwGetTime();
        f32 delta_time      = (f32) (new_time - current_time);
        state.time          = new_time;

        if (KEYS[GLFW_KEY_ESCAPE] == InputState::DOWN) {
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
                reset_mesh(chunk.mesh);
                generate_mesh(&chunk);
                upload_mesh(chunk.mesh);
                chunk.dirty = false;
            }

        }

        draw_frame(&state.renderer, &state.window); 
        swap_buffers(&state.window);
    }

    glfwTerminate();

    return 0;
}

void update_and_draw(f32 delta_time) {
    draw_cube(&state.renderer, {}, {1, 1, 1}, {}, alpha(RED, 0.5));

    if (KEYS[GLFW_KEY_F5] == InputState::DOWN) {
        state.game_running = !state.game_running; 
    }

    if (!state.game_running) { // update editor camera
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

    for (Entity &entity : state.entities) {
        if (state.game_running) {
            if (BIT_SET(entity.flags, EF_PLAYER)) {
                v3 input = {};
    
                if (KEYS[GLFW_KEY_A] == InputState::PRESSED) {
                    input.x -= 1;
                }
            
                if (KEYS[GLFW_KEY_D] == InputState::PRESSED) {
                    input.x += 1;
                }
                
                if (KEYS[GLFW_KEY_W] == InputState::PRESSED) {
                    input.z += 1;
                }
            
                if (KEYS[GLFW_KEY_S] == InputState::PRESSED) {
                    input.z -= 1;
                }
            
                const f32 SPEED = 1;
                const f32 JUMP = 10;
                const f32 LIFT = 0.03;
                const f32 PUSH = 0.4;
    
                v3 up = {0, 1, 0};
                v3 forward = get_forward_direction(entity.rotation);
                v3 right = get_right_direction(entity.rotation);

                f32 air_speed = length(v3{entity.velocity.x, clamp(-10000, entity.velocity.y, 0), entity.velocity.z});
                f32 pitch = forward.y;

                f32 lift_influence = 0;
                f32 push_influence = 0;

                if (pitch < 0) {
                    lift_influence = 0;
                } else if (pitch < 0.5) {
                    lift_influence = pitch * 2;
                } else if (pitch < 1) {
                    lift_influence = 1 - pitch;
                }

                if (pitch < -0.5) {
                    push_influence = 0;
                } else if (pitch < 0) {
                    push_influence = 1 + (pitch * 2);
                } else if (pitch < 1) {
                    push_influence = 1 - pitch;
                }

                // upward force applied
                entity.velocity.y += lift_influence * air_speed  * LIFT;

                // forward force applied
                entity.velocity += v3{forward.x, 0, forward.z} * push_influence * PUSH;

                u8 buffer[128] = {};

                memset(buffer, 0, 128);
                i64 len = sprintf((char *) buffer, "%.2f %.2f %.2f", entity.velocity.x, entity.velocity.y, entity.velocity.z);
                string s = make_slice(buffer, len);
                draw_text(&state.renderer, s, entity.position + v3{-8, 2, 0}, 1, BLACK);

                memset(buffer, 0, 128);
                len = sprintf((char *) buffer, "%.2f %.2f %.2f", pitch, lift_influence, air_speed);
                s = make_slice(buffer, len);
                draw_text(&state.renderer, s, entity.position + v3{-8, -2, 0}, 1, BLACK);

                memset(buffer, 0, 128);
                len = sprintf((char *) buffer, "%.2f", push_influence);
                s = make_slice(buffer, len);
                draw_text(&state.renderer, s, entity.position + v3{8, -3, 0}, 1, RED);

                if (state.window.mouse_captured) {
                    f32 sensitivity = 0.1;
                    v2 mouse_input = MOUSE.delta;
            
                    if(length(mouse_input) != 0) { 
                        entity.rotation += v3{mouse_input.y, mouse_input.x, 0} * sensitivity;
                    }
                }

                state.camera.mode = CameraMode::THIRD_PERSON;
                state.camera.position = entity.position - (forward * 5) + v3{0, 2, 0};
                state.camera.target = entity.position + v3{0, 1, 0};
            }
        }

        draw_cube(&state.renderer, entity.position, entity.size, entity.rotation, WHITE);
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
    if (!state.game_running) {
        return;
    }

    for (int i = 0; i < state.entities.len; i++) {
        Entity* entity = &state.entities[i];

        if (BIT_SET(entity->flags, EF_GRAVITY_AFFECTED)) {
            entity->velocity.y -= state.gravity;
        }

        const f32 MAX_SPEED = 50;
        if (length(entity->velocity) > MAX_SPEED) {
            entity->velocity = norm(entity->velocity) * MAX_SPEED;
        }

        // v3 drag = -(entity->velocity * entity->velocity * 0.0007);
        // entity->velocity += drag;

        entity->position += entity->velocity * delta_time;

        for (Chunk &chunk : state.chunks) {
            // adjusting position as this collision detection's position is centred 
            // and the chunk position is the bottom left corner
            bool in_chunk = point_collision(entity->position, chunk.position + (chunk.size * 0.5), chunk.size);
            if (!in_chunk) {
                continue;
            }

            for (i64 i = 0; i < chunk.blocks.len; i++) {
                if (chunk.blocks[i] == BlockType::AIR) {
                    continue;
                }

                ChunkPosition chunk_position = block_index_to_chunk_position(&chunk, i);
                v3 block_world_position = chunk.position + (as_floats(chunk_position) * V_BLOCK_SIZE);

                CubeCollision info = cube_collision(entity->position, entity->size, block_world_position, V_BLOCK_SIZE);
                if (!info.collision) {
                    continue;
                }

                if (info.overlap.x < info.overlap.y && info.overlap.x < info.overlap.z) {
                    entity->position.x -= sign(info.distance.x) * info.overlap.x;
                    entity->velocity.x = 0;
                }
                else if (info.overlap.y < info.overlap.x && info.overlap.y < info.overlap.z) {
                    entity->position.y -= sign(info.distance.y) * info.overlap.y;
                    entity->velocity.y = 0;
                }
                else if (info.overlap.z < info.overlap.x && info.overlap.z < info.overlap.y) {
                    entity->position.z -= sign(info.distance.z) * info.overlap.z;
                    entity->velocity.z = 0;
                }
            }
        }
    }
}

void update_and_draw_editor(f32 delta_time) {
    if (KEYS[GLFW_KEY_F1] == InputState::DOWN) {
        state.editor.visable = !state.editor.visable;
        set_mouse_captured(&state.window, !state.editor.visable);
    }

    if (state.editor.sculptor.enabled) {
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
                set_block_radius(&chunk, target_position, BlockType::AIR, radius);
                cooldown_timer = state.editor.sculptor.cooldown;
            }
            else if(MOUSE.buttons[GLFW_MOUSE_BUTTON_2] == InputState::PRESSED) {
                set_block_radius(&chunk, target_position, BlockType::BRICK, radius);
                cooldown_timer = state.editor.sculptor.cooldown;
            }
        }
    }

    if(state.editor.visable) {
        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_NoDockingOverCentralNode);

        // ImGui::ShowDemoWindow();

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

            { // frame time plot
                const i64 BUFFER_SIZE = 128;
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
                ImGui::SeparatorText("World");
                ImGui::Checkbox("Running", &state.game_running);
                ImGui::SliderFloat("Gravity", &state.gravity, 0, 20);
            }

            {
                ImGui::SeparatorText("Camera");
                state.camera.mode == CameraMode::FIRST_PERSON ? ImGui::Text("Mode: FP") : ImGui::Text("Mode: TP");
                ImGui::SliderFloat("FOV", &state.camera.fov, 1, 360);
                ImGui::SliderFloat3("Camera position", &state.camera.position[0], -50, 50);
                ImGui::SliderFloat3("Camera rotation", &state.camera.rotation[0], -360, 360);
                ImGui::SliderFloat("Ortho Size", &state.camera.orthographic_size, 1, 100);
                ImGui::SliderFloat4("Clear colour", &state.renderer.clear_colour[0], 0, 1);
                ImGui::SliderFloat3("Ambient light", &state.renderer.ambient_light[0], 0, 1);
                ImGui::SliderFloat3("Sun colour", &state.renderer.sun_colour[0], 0, 1);
                ImGui::SliderFloat3("Sun position", &state.renderer.sun_position[0], -1, 1);
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
                *chunk = new_chunk(&state.renderer, builder.position, builder.size);

                generate_blocks(chunk);
                generate_mesh(chunk);
                upload_mesh(chunk->mesh);
            }

            ImGui::SeparatorText("Sculptor");
            ImGui::Checkbox("Enabled", &state.editor.sculptor.enabled);
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

                FrameBuffer *fb = &state.renderer.g_buffer;

                ImGui::Text("SSAO buffer");
                ImGui::Image(state.renderer.ssao_frame_buffer.position_attachment, image_size, ImVec2(0, 1), ImVec2(1, 0));

                ImGui::Text("Position buffer");
                ImGui::Image(fb->position_attachment, image_size, ImVec2(0, 1), ImVec2(1, 0));

                ImGui::Text("Normal buffer");
                ImGui::Image(fb->normals_attachment, image_size, ImVec2(0, 1), ImVec2(1, 0));

                ImGui::Text("Albedo buffer");
                ImGui::Image(fb->albedo_attachment, image_size, ImVec2(0, 1), ImVec2(1, 0));

                ImGui::Text("Depth buffer");
                ImGui::Image(fb->depth_attachment, image_size, ImVec2(0, 1), ImVec2(1, 0));

                ImGui::Text("Sun depth buffer");
                ImGui::Image(state.renderer.sun_frame_buffer.depth_attachment, image_size, ImVec2(0, 1), ImVec2(1, 0));
            }

            ImGui::End();
        }
    }
}

Chunk new_chunk(Renderer *renderer, v3i position, v3i size) {
    i64 blocks = size.x * size.y * size.z;

    return Chunk {
        .dirty = false,
        .position = as_floats(position),
        .size = as_floats(size),
        .mesh = new_mesh(renderer, as_floats(position), blocks * 6),
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

            push_quad(chunk->mesh, positions, normals, colour, sprite->albedo->uvs, state.renderer.default_normal->uvs);
        }

        if (down == BlockType::AIR) {
            v3 positions[4] = {
                position + front_bottom_left,
                position + front_bottom_right,
                position + back_bottom_right,
                position + back_bottom_left
            };

            v3 normals[4] = {down_normal, down_normal, down_normal, down_normal};

            push_quad(chunk->mesh, positions, normals, colour, sprite->albedo->uvs, state.renderer.default_normal->uvs);
        }

        if (left == BlockType::AIR) {
            v3 positions[4] = {
                position + back_top_left,
                position + front_top_left,
                position + front_bottom_left,
                position + back_bottom_left
            };

            v3 normals[4] = {left_normal, left_normal, left_normal, left_normal};

            push_quad(chunk->mesh, positions, normals, colour, sprite->albedo->uvs, state.renderer.default_normal->uvs);
        }


        if (right == BlockType::AIR) {
            v3 positions[4] = {
                position + front_top_right,
                position + back_top_right,
                position + back_bottom_right,
                position + front_bottom_right
            };

            v3 normals[4] = {right_normal, right_normal, right_normal, right_normal};

            push_quad(chunk->mesh, positions, normals, colour, sprite->albedo->uvs, state.renderer.default_normal->uvs);
        }


        if (front == BlockType::AIR) {
            v3 positions[4] = {
                position + front_top_left,
                position + front_top_right,
                position + front_bottom_right,
                position + front_bottom_left
            };

            v3 normals[4] = {front_normal, front_normal, front_normal, front_normal};

            push_quad(chunk->mesh, positions, normals, colour, sprite->albedo->uvs, state.renderer.default_normal->uvs);
        }


        if (back == BlockType::AIR) {
            v3 positions[4] = {
                position + back_top_right,
                position + back_top_left,
                position + back_bottom_left,
                position + back_bottom_right
            };

            v3 normals[4] = {back_normal, back_normal, back_normal, back_normal};

            push_quad(chunk->mesh, positions, normals, colour, sprite->albedo->uvs, state.renderer.default_normal->uvs);
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

Sprite *get_sprite(SpriteHandle handle) {
    if(handle == SH_NONE) {
        return NULL;
    }

    return sprites[handle];
}
