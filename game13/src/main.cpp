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

#define MAX_ENTITIES 1000

#define TEXT_INPUT_LEN 64

enum TextureHandle {
    TH_NONE,
    TH_PLAYER,
    TH_GRASS,
    TH_STONE,
    TH_WOOD,
    TH_COUNT_
};

Texture *textures[TH_COUNT_];

struct Entity {
    // meta
    u64 flags;

    // entity
    v2 position;
    v2 size;

    // rendering
    v4 color;
    TextureHandle texture;
};

enum EntityFlags {
    EF_PLAYER           = 1 << 0,
    EF_DELETE           = 1 << 16,
};

struct Editor {
    bool visable;
};

struct State {
    Camera camera;
    Window window;
    Renderer renderer;
    SoundEngine sound_engine;

    Editor editor;

    f64 time;

    StackArray<Entity, MAX_ENTITIES> entities;
};

void state_update_and_draw(State *state, f32 delta_time);
void state_update_and_draw_editor(State *state, f32 delta_time);
Entity *state_spawn_entity(State *state, Entity entity);
bool state_load_textures(State *state);

// @main
int main() {
    log_set_thread_name("main");

    State state = State {
        .camera = {
            .position = {0, 0, -1},
            .near_plane = 0.1f,
            .far_plane = 50.0f,
            .orthographic_size = 8,
        },
        .renderer = {
            .clear_colour = v4{0.45, 0.7, 0.9, 1},
        },
        .editor = {
            .visable = false,
        },
    };

    { // init engine stuff
        bool ok = false;

        ok = init_window(&state.window, "game13", 1920, 1080);
        if (!ok) {
            Fatal("failed to init window");
            return 1;
        }

        ok = init_renderer(&state.renderer, &state.window);
        if (!ok) {
            Fatal("failed to init the renderer");
            return 1;
        }

        ok = state_load_textures(&state);
        if (!ok) {
            Fatal("failed to load textures");
            return 1;
        }

        ok = build_atlas(&state.renderer);
        if (!ok) {
            Fatal("failed to build texture atlas");
            return 1;
        }

        ok = load_font(&state.renderer, "resources/fonts/LibreBaskerville.ttf", 1000, 1000, 160);
        if (!ok) {
            Fatal("failed to load font");
            return 1;
        }

        ok = init_sound_engine(&state.sound_engine);
        if (!ok) {
            Fatal("failed to init sound engine");
            return 1;
        }

        ok = load_sounds(&state.sound_engine);
        if (!ok) {
            Fatal("failed to load sounds");
            return 1;
        } 

        srand(time(NULL));
    }

    { // init game stuff
        Entity player = Entity{
            .flags = EF_PLAYER,
            .position = v2{0, 0},
            .size = v2{1, 1.5},
            .color = WHITE,
            .texture = TH_PLAYER
        };

        state_spawn_entity(&state, player);
    }

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
        state_update_and_draw(&state, delta_time);
        state_update_and_draw_editor(&state, delta_time); 

        draw_frame(&state.renderer, &state.window); 

        swap_buffers(&state.window);
    }

    glfwTerminate();

    return 0;
}

v2i world_position_to_tile_position(v2 world_position) {
    return {};
}

void state_update_and_draw(State *state, f32 delta_time) {
    if (KEYS[GLFW_KEY_F1] == InputState::DOWN) {
        state->editor.visable = !state->editor.visable;
    }

    for (Entity &entity : state->entities) {
        if (BitSet(entity.flags, EF_PLAYER)) {
            { // movement
                v2 input = {};
            
                if (KEYS[GLFW_KEY_W] == InputState::PRESSED) {
                    input.y += 1;
                }
            
                if (KEYS[GLFW_KEY_S] == InputState::PRESSED) {
                    input.y -= 1;
                }
            
                if (KEYS[GLFW_KEY_D] == InputState::PRESSED) {
                    input.x += 1;
                }
            
                if (KEYS[GLFW_KEY_A] == InputState::PRESSED) {
                    input.x -= 1;
                }

                f32 speed = 6;
                entity.position.x += input.x * speed * delta_time;
                entity.position.y += input.y * speed * delta_time;

                state->camera.position.x = entity.position.x;
                state->camera.position.y = entity.position.y;
            }

            { // placing
                if (MOUSE.buttons[GLFW_MOUSE_BUTTON_1] == InputState::DOWN) {
                    v2 spawn_position = screen_position_to_world_position(MOUSE.position, state->camera, &state->window);
                    Entity wood = Entity{
                        .flags = {},
                        .position = spawn_position,
                        .size = v2{1, 1},
                        .color = WHITE,
                        .texture = TH_WOOD,
                    };

                    state_spawn_entity(state, wood);
                }
            }
        }


        v2 mouse_position = screen_position_to_world_position(MOUSE.position, state->camera, &state->window);
        v2i tile_position = world_position_to_tile_position(mouse_position);

        draw_rectangle(&state->renderer, v3{mouse_position.x, mouse_position.y, 0}, v2{1, 1}, RED);
        draw_rectangle(&state->renderer, v3{(f32) tile_position.x, (f32) tile_position.y, 0}, v2{1, 1}, BLUE);

        draw_texture(&state->renderer, textures[entity.texture], v3{entity.position.x, entity.position.y, 0}, entity.size, 0, entity.color);
    }

    const i32 terrain_width = 100;
    const i32 terrain_height = 100;

    for (i32 x = -(terrain_width / 2); x < (terrain_width / 2); x++) {
        for (i32 y = -(terrain_height / 2); y < (terrain_height / 2); y++) {
            v3 position = v3{f32(x), f32(y), 0};
            TextureHandle texture = TH_GRASS;

            if (x == 0 || y == 0) {
                texture = TH_STONE;
            }

            draw_texture(&state->renderer, textures[texture], position, v2{1, 1}, 0, WHITE);
        }
    }
}


void state_update_and_draw_editor(State *state, f32 delta_time) {
    if(state->editor.visable) {
        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_NoDockingOverCentralNode);

        // ImGui::ShowDemoWindow();

        {
            ImGui::Begin("Inspector");
        
            { 
                v3 direction = get_forward_direction(state->camera);
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
                ImGui::SliderFloat3("Camera position", &state->camera.position[0], -50, 50);
                ImGui::SliderFloat3("Camera rotation", &state->camera.rotation[0], -360, 360);
                ImGui::SliderFloat("Ortho Size", &state->camera.orthographic_size, 1, 100);
            }

            {
                v2 mouse_world_position = screen_position_to_world_position(MOUSE.position, state->camera, &state->window);

                ImGui::SeparatorText("Mouse");
                ImGui::Text("Screen position: %f, %f", MOUSE.position.x, MOUSE.position.y);
                ImGui::Text("World position: %f, %f", mouse_world_position.x, mouse_world_position.y);
            }

            {
                ImGui::SeparatorText("Window");
                ImGui::Text("Logical size: %d, %d", state->window.logical_size.x, state->window.logical_size.y);
                ImGui::Text("Frame buffer size: %d, %d", state->window.frame_buffer_size.x, state->window.frame_buffer_size.y);
            }

            {
                ImGui::SeparatorText("Renderer");

                if(ImGui::Button("Reload Shaders")) {
                    delete_shaders(&state->renderer);
                    load_shaders(&state->renderer);
                }

                ImGui::SameLine();
            
                if(ImGui::Button("Toggle V-sync")) {
                    toggle_vsync(&state->window);
                } 

                ImGui::SliderFloat4("Clear colour", &state->renderer.clear_colour[0], 0, 1);

                if(ImGui::CollapsingHeader("Render outputs")) {
                    ImVec2 image_size(360 * 1.777, 360);
    
                    ImGui::Text("g_buffer albedo");
                    ImGui::Image(state->renderer.g_buffer.albedo_attachment, image_size, ImVec2(0, 1), ImVec2(1, 0));
    
                    ImGui::Text("g_buffer depth");
                    ImGui::Image(state->renderer.g_buffer.depth_attachment, image_size, ImVec2(0, 1), ImVec2(1, 0));
                }
            }
       
            ImGui::End();
        }
    }
}

Entity *state_spawn_entity(State *state, Entity entity) {
    Entity *ptr = push(&state->entities);
    *ptr = entity;

    return ptr;
}

bool state_load_textures(State *state) {
    Texture *texture = NULL;

    texture = load_texture(&state->renderer, "resources/textures/player/player.png");
    if (texture == NULL) {
        return false;
    }

    textures[TH_PLAYER] = texture;

    texture = load_texture(&state->renderer, "resources/textures/grass/grass.png");
    if (texture == NULL) {
        return false;
    }

    textures[TH_GRASS] = texture;

    texture = load_texture(&state->renderer, "resources/textures/stone/stone.png");
    if (texture == NULL) {
        return false;
    }

    textures[TH_STONE] = texture;

    texture = load_texture(&state->renderer, "resources/textures/wood/wood.png");
    if (texture == NULL) {
        return false;
    }

    textures[TH_WOOD] = texture;

    return true;
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
