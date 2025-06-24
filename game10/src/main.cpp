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

#include <string>
#include <iostream>

// Total: 01:00
// Started: 1:30

#define ALLOW_EDITOR 1
#define MAX_ENTITIES 2000

Model *cube_model;
Model *iso_model;

struct Entity {
    // meta
    u64 flags;

    // entity
    v3 position;
    v3 size;
    v3 rotation;
    v3 velocity;

    // rendering
    Model *model;
};

struct Editor {
    bool visable;

    struct {
        bool enabled;
        i32 range;
        i32 radius;
        f32 cooldown;
    } sculptor;

    struct {
        bool enabled;
        v3 colour;
        i32 range;
        i32 radius;
    } paint_brush;
};

enum EntityFlags {
    EF_PLAYER           = 1 << 0,
    EF_LIGHT            = 1 << 1,
    EF_ANIMATED_SPRITE  = 1 << 2,
    EF_GRAVITY_AFFECTED = 1 << 3,
    EF_DELETE           = 1 << 16,
};

struct State {
    Camera camera;
    Window window;
    Renderer renderer;
    SoundEngine sound_engine;
    Editor editor;

    f64 time;

    StackArray<Entity, MAX_ENTITIES> entities;
} state = {};

void update_and_draw(f32 delta_time);
void physics(f32 delta_time);
void update_and_draw_editor(f32 delta_time);

Entity *spawn_entity(Entity entity);

enum class TokenType {
    EQUAL
};

struct Token {
    TokenType type;
};

bool is_delim(char c) {
    switch (c) {
        case ' ':
        case '\n':
            return true;
        default:
            return false;
    }
}

Slice<Token> tokenise_config_file(Allocator *allocator, str path) {
    File file = new_file(path);
    Slice<u8> bytes = read_entire_file(&file);

    DynamicArray<Token> tokens = new_dynamic_array<Token>(allocator, bytes.len);

    i64 character = 0;

    while (character < bytes.len) {
        char c = (char) bytes[character];

        switch (c) {
            case ' ':
            case '\n':
            case '\r':
            case '\t':
                character++;
                continue;
        }

        printf("%c\n", c);

        character++;
    }

    return {};
}

int main() {
    Allocator allocator = new_allocator(1024);
    i64 *a = alloc<i64>(&allocator);
    i64 *b = alloc<i64>(&allocator);
    i64 *c = alloc<i64>(&allocator);

    *a = 10;
    *b = 20;
    *c = 30;

    Slice<Token> tokens = tokenise_config_file(&allocator, "start.config");

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
            .ssao_radius = 0.8,
            .ssao_bias = 0.025,
            .ssao_noise_scale = {480, 270},
        },
        .editor = {
            .visable = false,
            .sculptor = {
                .enabled = false,
                .range = 50,
                .radius = 4,
                .cooldown = 0.075,
            },
            .paint_brush = {
                .enabled = false,
                .colour = {0.2, 0.8, 0.2},
                .range = 50,
                .radius = 4,
            },
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

        cube_model = load_model(&state.renderer, "resources/models/cuber/cube.obj", "resources/models/cuber/Texture.png");
        iso_model = load_model(&state.renderer, "resources/models/ico/ico.obj", "resources/models/ico/Texture.png");

        srand(time(NULL));
    }

    spawn_entity(Entity {
        .position = {0, 0, 0},
        .model = cube_model,
    });

    spawn_entity(Entity {
        .position = {10, 0, 0},
        .model = iso_model,
    });

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

        draw_frame(&state.renderer, &state.window); 
        swap_buffers(&state.window);
    }

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

    for (Entity &entity : state.entities) {
        draw_model(&state.renderer, entity.position, entity.model);
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
                ImGui::SliderFloat3("Sun colour", &state.renderer.sun_colour[0], 0, 1);
                ImGui::SliderFloat3("Sun position", &state.renderer.sun_position[0], -100, 100);
                ImGui::SliderFloat("SSAO radius", &state.renderer.ssao_radius, 0, 2);
                ImGui::SliderFloat("SSAO bias", &state.renderer.ssao_bias, 0, 0.2);
                ImGui::SliderFloat2("SSAO noise", &state.renderer.ssao_noise_scale[0], 0, 1000);

                if(ImGui::CollapsingHeader("Render outputs")) {
                    ImVec2 image_size(360 * 1.777, 360);
    
                    FrameBuffer *fb = &state.renderer.g_buffer;
    
                    ImGui::Text("g_buffer position");
                    ImGui::Image(state.renderer.g_buffer.position_attachment, image_size, ImVec2(0, 1), ImVec2(1, 0));
    
                    ImGui::Text("g_buffer normal");
                    ImGui::Image(state.renderer.g_buffer.normals_attachment, image_size, ImVec2(0, 1), ImVec2(1, 0));
    
                    ImGui::Text("g_buffer view normal");
                    ImGui::Image(state.renderer.g_buffer.view_normals_attachment, image_size, ImVec2(0, 1), ImVec2(1, 0));
    
                    ImGui::Text("g_buffer albedo");
                    ImGui::Image(state.renderer.g_buffer.albedo_attachment, image_size, ImVec2(0, 1), ImVec2(1, 0));
    
                    ImGui::Text("g_buffer sun position");
                    ImGui::Image(state.renderer.g_buffer.sun_position_attachment, image_size, ImVec2(0, 1), ImVec2(1, 0));
    
                    ImGui::Text("g_buffer depth");
                    ImGui::Image(state.renderer.g_buffer.depth_attachment, image_size, ImVec2(0, 1), ImVec2(1, 0));
    
                    ImGui::Text("lighting buffer position");
                    ImGui::Image(state.renderer.lighting_frame_buffer.position_attachment, image_size, ImVec2(0, 1), ImVec2(1, 0));
    
                    ImGui::Text("sun buffer depth");
                    ImGui::Image(state.renderer.sun_frame_buffer.depth_attachment, image_size, ImVec2(0, 1), ImVec2(1, 0));
    
                    ImGui::Text("SSAO buffer position");
                    ImGui::Image(state.renderer.ssao_frame_buffer.position_attachment, image_size, ImVec2(0, 1), ImVec2(1, 0));
    
                    ImGui::Text("SSAO blur position");
                    ImGui::Image(state.renderer.ssao_blur_frame_buffer.position_attachment, image_size, ImVec2(0, 1), ImVec2(1, 0));
                }
            }
       
            ImGui::End();
        }
    }
}

Entity *spawn_entity(Entity entity) {
    Entity *ptr = push(&state.entities);
    *ptr = entity;

    return ptr;
}
