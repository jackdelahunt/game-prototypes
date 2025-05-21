#include "libs/libs.h"
#include "engine.cpp"

#include <cstdio>
#include <cstring>
#include <time.h>
#include <stdlib.h>

// Total: 12
// Started: 23:30
//
// TODO:
// - forground blurring
// - leaf particals
// - maybe animated textures
// - paralax when moving

#define MAX_ENTITIES 2000

struct Entity {
    // meta
    u64 flags;

    // entity
    v3 position;
    v2 size;
    f32 rotation;
    v2 velocity;
    v4 color;

    // rendering
    TextureHandle texture;
};

enum EntityFlags {
    EF_LIGHT    = 1 << 0,
    EF_DELETE   = 1 << 1,
};

struct State {
    Camera camera;
    Window window;
    Renderer renderer;
    SoundEngine sound_engine;

    f64 time;

    Array<Entity, MAX_ENTITIES> entities;
} state = {};

struct CollisionIterator {
    Entity* entity;
    i64 index;
};

void input();
void update_and_draw(f32 delta_time);
void physics(f32 delta_time);

void spawn_entity(Entity entity);

void create_scene();

CollisionIterator new_collision_iterator(Entity *entity);
Entity *next(CollisionIterator *iterator);

int main() {
    state = {
        .camera = {
            .position = {0, 0, -1},
            .orthographic_size = 450,
            .near_plane = 0.1f,
            .far_plane = 100.0f,
        },
        .renderer = {
            .global_light = {0.25, 0.25, 0.5, 1},
            .light_colour = {1, 0.8, 0.6, 1},
        },
    };

    { // init engine stuff
        bool ok = false;

        ok = init_window(&state.window, 1920, 1080, "game7");
        if (!ok) {
            printf("failed to init window\n");
            return 1;
        }

        ok = init_renderer(&state.renderer, &state.window);
        if (!ok) {
            printf("failed to init the renderer\n");
            return 1;
        }

        ok = load_textures(&state.renderer);
        if (!ok) {
            printf("failed to load textures\n");
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

    if (true) {
        create_scene();
    } else {
        state.renderer.global_light = WHITE;

        spawn_entity(Entity {
            .position = {0, 0, 0},
            .size = {300, 300},
            .color = WHITE,
            .texture = TH_SHOP
        });

        spawn_entity(Entity {
            .position = {200,  0},
            .size = {300, 300},
            .color = RED,
            .texture = TH_SHOP
        });
 
    }

    FrameBuffer unlit_frame_buffer {
        .width = (u32) state.window.width,
        .height = (u32) state.window.height
    };

    bool ok = init_frame_buffer(&unlit_frame_buffer);
    if (!ok) {
        printf("failed to init unlit frame buffer\n");
        return 1;
    }

    FrameBuffer lighting_frame_buffer {
        .width = (u32) state.window.width,
        .height = (u32) state.window.height
    };

    ok = init_frame_buffer(&lighting_frame_buffer);
    if (!ok) {
        printf("failed to init lighting frame buffer\n");
        return 1;
    }

    while (!glfwWindowShouldClose(state.window.glfw_window)) {
        f64 current_time    = state.time;
        f64 new_time        = glfwGetTime();
        f32 delta_time      = (f32) (new_time - current_time);
        state.time          = new_time;

        input();

        if (KEYS[GLFW_KEY_ESCAPE] == InputState::down) {
            glfwSetWindowShouldClose(state.window.glfw_window, GLFW_TRUE);
        }

        { // first render pass - unlit scene
            glBindFramebuffer(GL_FRAMEBUFFER, unlit_frame_buffer.id);
            new_frame(&state.renderer, &state.window, state.camera);
    
            update_and_draw(delta_time);
            physics(delta_time); 
    
            draw_frame(&state.renderer, &state.window);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        { // second render pass - lighting 
            glBindFramebuffer(GL_FRAMEBUFFER, lighting_frame_buffer.id);

            new_frame(&state.renderer, &state.window, state.camera);

            v2 uvs[4] = {
                {0, 1},
                {1, 1},
                {1, 0},
                {0, 0},
            };

            Quad *quad = push_quad(&state.renderer, {}, {50, 50}, 0, WHITE, uvs, 2);
            f32 z = 0;
            quad->vertices[0].position = {-1,  1, z};
            quad->vertices[1].position = { 1,  1, z};
            quad->vertices[2].position = { 1, -1, z};
            quad->vertices[3].position = {-1, -1, z};

            glViewport(0, 0, state.window.width, state.window.height);
    
            glBindBuffer(GL_ARRAY_BUFFER, state.renderer.vertex_buffer_id);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(Quad) * state.renderer.quads.len, state.renderer.quads.data);
            glBindVertexArray(state.renderer.vertex_array_id);
    
            glUseProgram(state.renderer.light_shader_program_id);
   
            // set the scene texture
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, unlit_frame_buffer.colour_attachment);

            // set all uniforms used in the lights shader, using sprintf to get
            // the location of each value in the lights array that is why it looks
            // really weird and long winded
            glUniform4f(
                glGetUniformLocation(state.renderer.light_shader_program_id, "global_light"),
                state.renderer.global_light[0],
                state.renderer.global_light[1],
                state.renderer.global_light[2],
                state.renderer.global_light[3]
            );

            glUniform4f(
                glGetUniformLocation(state.renderer.light_shader_program_id, "light_colour"),
                state.renderer.light_colour[0],
                state.renderer.light_colour[1],
                state.renderer.light_colour[2],
                state.renderer.light_colour[3]
            );

            glUniform1i(
                glGetUniformLocation(state.renderer.light_shader_program_id, "light_count"),
                (i32) state.renderer.lights.len
            );

            for(i64 i = 0; i < state.renderer.lights.len; i++) {
                const i64 buffer_size = 32;
                char buffer[buffer_size] = {};

                sprintf(buffer, "lights[%llu].position", i);

                glUniform2f(
                    glGetUniformLocation(state.renderer.light_shader_program_id, buffer), 
                    state.renderer.lights[i].position.X,
                    state.renderer.lights[i].position.Y
                );
            }
    
            glDrawElements(GL_TRIANGLES, 6 * state.renderer.quads.len, GL_UNSIGNED_INT, 0);

            reset(&state.renderer.lights);

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        { // third render pass- blur
            new_frame(&state.renderer, &state.window, state.camera);

            v2 uvs[4] = {
                {0, 1},
                {1, 1},
                {1, 0},
                {0, 0},
            };

            Quad *quad = push_quad(&state.renderer, {}, {50, 50}, 0, WHITE, uvs, 2);
            f32 z = 0;
            quad->vertices[0].position = {-1,  1, z};
            quad->vertices[1].position = { 1,  1, z};
            quad->vertices[2].position = { 1, -1, z};
            quad->vertices[3].position = {-1, -1, z};

            glViewport(0, 0, state.window.width, state.window.height);
    
            glBindBuffer(GL_ARRAY_BUFFER, state.renderer.vertex_buffer_id);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(Quad) * state.renderer.quads.len, state.renderer.quads.data);
            glBindVertexArray(state.renderer.vertex_array_id);
    
            glUseProgram(state.renderer.blur_shader_program_id);
   
            // set the scene texture
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, lighting_frame_buffer.colour_attachment);

            glDrawElements(GL_TRIANGLES, 6 * state.renderer.quads.len, GL_UNSIGNED_INT, 0);
        }

        { // imgui render 
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame(); 

            ImGui::Begin("Inspector");

            if(ImGui::CollapsingHeader("Camera")) {
                ImGui::SliderFloat("Orthographic size", &state.camera.orthographic_size, 10, 1000);
            }

            if(ImGui::CollapsingHeader("Rendering")) {
                ImGui::InputFloat4("Global light", &state.renderer.global_light[0]);
                ImGui::InputFloat4("Lights", &state.renderer.light_colour[0]);
            }

            if(ImGui::CollapsingHeader("Render passes")) {
                ImGui::Image(unlit_frame_buffer.depth_attachment, ImVec2(360, 240), ImVec2(0, 1), ImVec2(1, 0));
                ImGui::Image(unlit_frame_buffer.colour_attachment, ImVec2(360, 240), ImVec2(0, 1), ImVec2(1, 0));
                ImGui::Image(lighting_frame_buffer.colour_attachment, ImVec2(360, 240), ImVec2(0, 1), ImVec2(1, 0));
            }

            if(ImGui::CollapsingHeader("Entities")) {
                for(i64 i = 0; i < state.entities.len; i++) {
                    ImGui::PushID(i);
                    Entity *entity = &state.entities.data[i];

                    char name_buffer[32] = {};
                    sprintf(name_buffer, "Entity: %llu", i);

                    if (ImGui::CollapsingHeader(name_buffer)) {
                        ImGui::SliderFloat3("position", &entity->position[0], -500, 500);
                        ImGui::InputFloat2("size", &entity->size[0]);
                        ImGui::InputFloat4("colour", &entity->color[0]);
                    }
                    ImGui::PopID();
                }
            }

            ImGui::End();

            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            GLFWwindow *current = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(current);
        }

        swap_buffers(&state.window);
    }

    glfwTerminate();

    return 0;
}

void input() {
    // this will set the state of things to up or down
    // to keep track of what is already down, we can go through
    // every key before this and set it to pressed, if is still
    // down we dont get and event and it stays pressed, if we get
    // an event for that key it will be to set it to up so the
    // pressed we accidentlly set is changed, this is not the best
    // - 24/01/25
    //
    // copied from odin engine so maybe need to look into this more
    // - 03/03/25
    
    for (int i = 0; i < KEYS.size; i++) {
        if (KEYS[i] == InputState::down) {
            KEYS[i] = InputState::pressed;
        }
    }

    glfwPollEvents();
}

void update_and_draw(f32 delta_time) {
    for (int i = 0; i < state.entities.len; i++) {
        Entity* entity = &state.entities[i];

        if (entity->flags & EF_LIGHT) {
            draw_light(&state.renderer, entity->position);
        } else {
            draw_texture(&state.renderer, entity->texture, entity->position, entity->size, entity->rotation, entity->color);
        }
    }

    for (int i = 0; i < state.entities.len; i++) {
        Entity* entity = &state.entities[i];

        if (entity->flags & EF_DELETE) {
            swap_remove(&state.entities, i);
            i--;

            printf("entity deleted\n");
        }
    }
}

void physics(f32 delta_time) {
    for (int i = 0; i < state.entities.len; i++) {
        Entity* entity = &state.entities[i];

        entity->position.X += entity->velocity.X * delta_time;
        entity->position.Y += entity->velocity.Y * delta_time;
    }
}

void spawn_entity(Entity entity) {
    append(&state.entities, entity);
}

void create_scene() {
    f32 forest_background_z = 80;
    f32 forest_foreground_z = 60;

    f32 decorations_background_z = 55;
    f32 decorations_foreground_z = 50;

    f32 floor_top_z = 40;
    f32 floor_middle_z = 20;
    f32 floor_bottom_z = 1;

    { // forest background
        f32 ratio = texture_aspect_ratio(&state.renderer, TH_BACKGROUND_LAYER_1);
        f32 height = 900;
        f32 width = height * ratio;
        f32 y = 0;

        spawn_entity(Entity{
            .position = {0, y, forest_background_z},
            .size = {width, height},
            .color = WHITE,
            .texture = TH_BACKGROUND_LAYER_1,
        });
    }

    { // forest foreground
        f32 ratio = texture_aspect_ratio(&state.renderer, TH_BACKGROUND_LAYER_3);
        f32 height = 900;
        f32 width = height * ratio;
        f32 y = 0;

       spawn_entity(Entity{
            .position = {-width, y, forest_foreground_z},
            .size = {width, height},
            .color = WHITE,
            .texture = TH_BACKGROUND_LAYER_3,
        });

        spawn_entity(Entity{
            .position = {0, y, forest_foreground_z},
            .size = {width, height},
            .color = WHITE,
            .texture = TH_BACKGROUND_LAYER_3,
        });

        spawn_entity(Entity{
            .position = {width, y, forest_foreground_z},
            .size = {width, height},
            .color = WHITE,
            .texture = TH_BACKGROUND_LAYER_3,
        });
    }

    { // fence posts
        f32 ratio = texture_aspect_ratio(&state.renderer, TH_FENCE);
        f32 height = 75;
        f32 width = height * ratio;
        f32 x = 170;
        f32 y = -200;

        spawn_entity(Entity{
            .position = {x - width, y, decorations_background_z},
            .size = {width, height},
            .color = WHITE,
            .texture = TH_FENCE,
        });

        spawn_entity(Entity{
            .position = {x, y, decorations_background_z},
            .size = {width, height},
            .color = WHITE,
            .texture = TH_FENCE,
        });

        spawn_entity(Entity{
            .position = {x + width, y, decorations_background_z},
            .size = {width, height},
            .color = WHITE,
            .texture = TH_FENCE,
        });

        spawn_entity(Entity{
            .position = {x + width + width, y, decorations_background_z},
            .size = {width, height},
            .color = WHITE,
            .texture = TH_FENCE,
        });
    }

    { // shop
        f32 ratio = texture_aspect_ratio(&state.renderer, TH_SHOP);
        f32 height = 325;
        f32 width = height * ratio;

        spawn_entity(Entity{
            .position = {-400, -100, decorations_foreground_z},
            .size = {width, height},
            .color = WHITE,
            .texture = TH_SHOP,
        });

        spawn_entity(Entity{
            .flags = EF_LIGHT,
            .position = {-560, -175},
        });

        spawn_entity(Entity{
            .flags = EF_LIGHT,
            .position = {-240, -175},
        });
    
    } 
    
    { // lamp posts
        f32 ratio = texture_aspect_ratio(&state.renderer, TH_LAMP);
        f32 height = 150;
        f32 width = height * ratio;

        spawn_entity(Entity{
            .position = {35, -160, decorations_foreground_z},
            .size = {width, height},
            .color = WHITE,
            .texture = TH_LAMP,
        });

        spawn_entity(Entity{
            .position = {325, -160, decorations_foreground_z},
            .size = {width, height},
            .color = WHITE,
            .texture = TH_LAMP,
        });

        spawn_entity(Entity{
            .position = {615, -160, decorations_foreground_z},
            .size = {width, height},
            .color = WHITE,
            .texture = TH_LAMP,
        });

        spawn_entity(Entity{
            .flags = EF_LIGHT,
            .position = {20, -160},
        });

        spawn_entity(Entity{
            .flags = EF_LIGHT,
            .position = {310, -160},
        });

        spawn_entity(Entity{
            .flags = EF_LIGHT,
            .position = {600, -160},
        });
    }

    { // top floor layer
        f32 ratio = texture_aspect_ratio(&state.renderer, TH_FLOOR);
        f32 height = 300;
        f32 width = height * ratio;
        f32 y = -325;

        spawn_entity(Entity{
            .position = {-width, y, floor_top_z},
            .size = {width, height},
            .color = WHITE,
            .texture = TH_FLOOR,
        });

        spawn_entity(Entity{
            .position = {0, y, floor_top_z},
            .size = {width, height},
            .color = WHITE,
            .texture = TH_FLOOR,
        });


        spawn_entity(Entity{
            .position = {width, y, floor_top_z},
            .size = {width, height},
            .color = WHITE,
            .texture = TH_FLOOR,
        });
    }

    { // middle floor layer
        f32 ratio = texture_aspect_ratio(&state.renderer, TH_FLOOR);
        f32 height = 375;
        f32 width = height * ratio;
        f32 y = -425;
        f32 x = -250;

        spawn_entity(Entity{
            .position = {x - width, y, floor_middle_z},
            .size = {x + width, height},
            .color = WHITE,
            .texture = TH_FLOOR,
        });

        spawn_entity(Entity{
            .position = {x, y, floor_middle_z},
            .size = {width, height},
            .color = WHITE,
            .texture = TH_FLOOR,
        });


        spawn_entity(Entity{
            .position = {x + width, y, floor_middle_z},
            .size = {width, height},
            .color = WHITE,
            .texture = TH_FLOOR,
        });
    }

    { // bottom floor layer
        f32 ratio = texture_aspect_ratio(&state.renderer, TH_FLOOR);
        f32 height = 450;
        f32 width = height * ratio;
        f32 y = -525;
        f32 x = 250;

        spawn_entity(Entity{
            .position = {x - width, y, floor_bottom_z},
            .size = {width, height},
            .color = WHITE,
            .texture = TH_FLOOR,
        });

        spawn_entity(Entity{
            .position = {x, y, floor_bottom_z},
            .size = {width, height},
            .color = WHITE,
            .texture = TH_FLOOR,
        });


        spawn_entity(Entity{
            .position = {x + width, y, floor_bottom_z},
            .size = {width, height},
            .color = WHITE,
            .texture = TH_FLOOR,
        });
    } 
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
            v2 distance = other->position.XY - entity->position.XY;
            v2 distance_abs = v2{abs(distance.X), abs(distance.Y)};
            v2 distance_for_collision = (entity->size + other->size) * v2{0.5, 0.5};

            bool collision = distance_for_collision[0] >= distance_abs[0] && distance_for_collision[1] >= distance_abs[1];
            if (collision) {
                return other;
            }
        }
    }

    return nullptr;
}
