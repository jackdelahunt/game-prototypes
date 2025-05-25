#include "libs/libs.h"
#include "engine.cpp"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <time.h>
#include <stdlib.h>

// Total: 11:30
// Started: 19:00
//
// Lighting TODO:
// - normal mapping
// - bloom
#define MAX_ENTITIES 2000

enum TextureHandle {
    TH_NONE,
    TH_FACE,
    TH_FACES,
    TH_SWORD,
    TH_SWORD_NORMAL,
    TH_GOLD,
    TH_GOLD_NORMAL,
    TH_COUNT_,
};

Texture *textures[TH_COUNT_];

struct Entity {
    // meta
    u64 flags;
    f64 time_created;

    // entity
    v3 position;
    v2 size;
    f32 rotation;
    v2 velocity;

    // rendering
    v4 color;
    TextureHandle texture;

    // animated texture
    f32 animation_cycle;

    // light
    v4 light_colour;
    f32 light_intensity;
    f32 light_radius;
};

enum EntityFlags {
    EF_LIGHT            = 1 << 0,
    EF_PLAYER           = 1 << 1,
    EF_ALT_PLAYER       = 1 << 2,
    EF_DELETE           = 1 << 16,
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

Texture *get_texture(TextureHandle handle);

int main() {
    state = {
        .camera = {
            .position = {0, 0, -1},
            .orthographic_size = 150,
            .near_plane = 0.1f,
            .far_plane = 100.0f,
        },
        .renderer = {
            .global_light = {0.6, 0.6, 0.6, 1},
            .clear_colour = {1, 1, 1, 1},
        },
    };

    { // init engine stuff
        bool ok = false;

        ok = init_window(&state.window, 1920, 1080, "game8");
        if (!ok) {
            printf("failed to init window\n");
            return 1;
        }

        ok = init_renderer(&state.renderer, &state.window);
        if (!ok) {
            printf("failed to init the renderer\n");
            return 1;
        }

        { // load and build all textures
            Texture *texture = NULL;

            texture = load_texture(&state.renderer, "resources/textures/face.png");
            if (texture == NULL) {
                return 1;
            }

            textures[TH_FACE] = texture;

            texture = load_animated_texture(&state.renderer, "resources/textures/faces.png", 7, 3);
            if (texture == NULL) {
                return 1;
            }

            textures[TH_FACES] = texture;

            texture = load_texture(&state.renderer, "resources/textures/sword.png");
            if (texture == NULL) {
                return 1;
            }

            textures[TH_SWORD] = texture;

            texture = load_texture(&state.renderer, "resources/textures/sword_normal.png");
            if (texture == NULL) {
                return 1;
            }

            textures[TH_SWORD_NORMAL] = texture; 

            texture = load_texture(&state.renderer, "resources/textures/gold.png");
            if (texture == NULL) {
                return 1;
            }

            textures[TH_GOLD] = texture;
    
            texture = load_texture(&state.renderer, "resources/textures/gold_normal.png");
            if (texture == NULL) {
                return 1;
            }

            textures[TH_GOLD_NORMAL] = texture;
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

    create_scene();

    FrameBuffer unlit_frame_buffer {
        .width = (u32) state.window.width,
        .height = (u32) state.window.height
    };

    bool ok = init_frame_buffer(&unlit_frame_buffer);
    if (!ok) {
        printf("failed to init unlit frame buffer\n");
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
            new_frame(&state.renderer, &state.window, state.camera);

            v2 uvs[4] = {
                {0, 1},
                {1, 1},
                {1, 0},
                {0, 0},
            };

            Quad *quad = push_quad(&state.renderer, {}, {50, 50}, 0, WHITE, uvs, {}, 2);
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
   
            // set the input texture
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, unlit_frame_buffer.colour_attachment);

            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, unlit_frame_buffer.normals_attachment);

            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, unlit_frame_buffer.depth_attachment);

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

            glUniform1i(
                glGetUniformLocation(state.renderer.light_shader_program_id, "light_count"),
                (i32) state.renderer.lights.len
            );

            glUniform1f(
                glGetUniformLocation(state.renderer.light_shader_program_id, "aspect_ratio"),
                (f32) state.window.width / (f32) state.window.height
            );

            for(i64 i = 0; i < state.renderer.lights.len; i++) {
                const i64 buffer_size = 64;
                char buffer[buffer_size] = {};

                { // set light position
                    sprintf(buffer, "lights[%llu].position", i);
                    glUniform2f(
                        glGetUniformLocation(state.renderer.light_shader_program_id, buffer), 
                        state.renderer.lights[i].position.x,
                        state.renderer.lights[i].position.y
                    );
                    memset(buffer, 0, buffer_size);
                }

                { // set light radius
                    sprintf(buffer, "lights[%llu].radius", i);
                    glUniform1f(
                        glGetUniformLocation(state.renderer.light_shader_program_id, buffer), 
                        state.renderer.lights[i].radius
                    );
                    memset(buffer, 0, buffer_size);
                }

                { // set light colour
                    sprintf(buffer, "lights[%llu].colour", i);
                    glUniform4f(
                        glGetUniformLocation(state.renderer.light_shader_program_id, buffer), 
                        state.renderer.lights[i].colour.r,
                        state.renderer.lights[i].colour.g,
                        state.renderer.lights[i].colour.b,
                        state.renderer.lights[i].colour.a
                    );
                    memset(buffer, 0, buffer_size);
                }

                { // set light intensity
                    sprintf(buffer, "lights[%llu].intensity", i);
                    glUniform1f(
                        glGetUniformLocation(state.renderer.light_shader_program_id, buffer), 
                        state.renderer.lights[i].intensity
                    );
                    memset(buffer, 0, buffer_size);
                }
     
            }
    
            glDrawElements(GL_TRIANGLES, 6 * state.renderer.quads.len, GL_UNSIGNED_INT, 0);

            reset(&state.renderer.lights);
        }

        { // imgui render 
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame(); 

            ImGui::Begin("Inspector");

            if(ImGui::Button("Reload Shaders")) {
                delete_shaders(&state.renderer);
                load_shaders(&state.renderer);
            }

            if(ImGui::CollapsingHeader("Camera")) {
                ImGui::SliderFloat("Orthographic size", &state.camera.orthographic_size, 10, 1000);
            }

            if(ImGui::CollapsingHeader("Rendering")) {
                ImGui::InputFloat4("Global light", &state.renderer.global_light[0]);
            }

            if(ImGui::CollapsingHeader("Render outputs")) {
                ImVec2 image_size(360 * 1.77, 360);

                ImGui::Text("Depth buffer");
                ImGui::Image(unlit_frame_buffer.depth_attachment, image_size, ImVec2(0, 1), ImVec2(1, 0));

                ImGui::Text("Normal buffer");
                ImGui::Image(unlit_frame_buffer.normals_attachment, image_size, ImVec2(0, 1), ImVec2(1, 0));

                ImGui::Text("Colour buffer");
                ImGui::Image(unlit_frame_buffer.colour_attachment, image_size, ImVec2(0, 1), ImVec2(1, 0));
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

                        ImGui::InputFloat4("light_colour", &entity->light_colour[0]);
                        ImGui::InputFloat("light_radius", &entity->light_radius);
                        ImGui::InputFloat("light_intensity", &entity->light_intensity);
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

        f32 player_speed = 300;

        if (entity->flags & EF_PLAYER) {
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

            entity->velocity = input * player_speed;
        }

        if (entity->flags & EF_ALT_PLAYER) {
            v2 input = {};

            if (KEYS[GLFW_KEY_UP] == InputState::pressed) {
                input.y += 1;
            }

            if (KEYS[GLFW_KEY_DOWN] == InputState::pressed) {
                input.y -= 1;
            }

            if (KEYS[GLFW_KEY_LEFT] == InputState::pressed) {
                input.x -= 1;
            }

            if (KEYS[GLFW_KEY_RIGHT] == InputState::pressed) {
                input.x += 1;
            }

            entity->velocity = input * player_speed;
        }

        if (entity->flags & EF_LIGHT) {
            draw_light(&state.renderer, entity->position, entity->light_radius, entity->light_colour, entity->light_intensity);
            draw_circle(&state.renderer, entity->position, 5, WHITE);
        } 

        if (entity->texture != TH_NONE) {
            Texture *texture = get_texture(entity->texture);

            if(texture->type == TextureType::SINGLE) {
                Texture *normal_texture = NULL;

                if(entity->texture == TH_GOLD) {
                    normal_texture = get_texture(TH_GOLD_NORMAL);
                } else if(entity->texture == TH_SWORD) {
                    normal_texture = get_texture(TH_SWORD_NORMAL);
                } else {
                    assert(0);
                }

                draw_texture(&state.renderer, texture, normal_texture, entity->position, entity->size, entity->rotation, entity->color);
            }
            else if(texture->type == TextureType::ANIMATED) {
                // progress and maybe reset texture animations 
                entity->animation_cycle += delta_time;
                if (entity->animation_cycle > texture->animation_length) {
                    entity->animation_cycle = 0;
                }
    
                draw_animated_texture(&state.renderer, texture, entity->animation_cycle, entity->position, entity->size, entity->rotation, entity->color);
            }
        }
    }

    { // draw grid lines
        i64 grid_region_width = 2000;
        i64 grid_region_height = 2000;
        f32 line_step = 100;
        f32 line_thickness = 1;
        v4 grid_colour = BLACK;

        // horizontal lines
        for(i64 y = (-grid_region_height) / 2; y <= grid_region_height / 2; y += line_step) {
            draw_rectangle(&state.renderer, {0, (f32) y, 90}, {(f32) grid_region_width, line_thickness}, grid_colour);
        }

        // vertical lines
        for(i64 x = (-grid_region_width) / 2; x <= grid_region_width / 2; x += line_step) {
            draw_rectangle(&state.renderer, {(f32) x, 0, 90}, {line_thickness, (f32) grid_region_height}, grid_colour);
        }

        // draw_circle(&state.renderer, {0, 0, 1}, 3, alpha(BLUE, 0.5));
    }


    for (int i = 0; i < state.entities.len; i++) {
        Entity* entity = &state.entities[i];

        if (entity->flags & EF_DELETE) {
            swap_remove(&state.entities, i);
            i--;
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

void spawn_entity(Entity entity) {
    entity.time_created = state.time;

    append(&state.entities, entity);
}

void create_scene() {
    if(false) { // faces entity
        f32 ratio = texture_aspect_ratio(&state.renderer, get_texture(TH_FACE));
        f32 height = 50;
        f32 width = height * ratio;
    
        spawn_entity(Entity{
            .position = {-200, -200, 30},
            .size = {width, height},
            .color = WHITE,
            .texture = TH_FACES,
        });
    }

    { // sword entity
        f32 ratio = texture_aspect_ratio(&state.renderer, get_texture(TH_SWORD));
        f32 height = 100;
        f32 width = height * ratio;
    
        spawn_entity(Entity{
            .position = {100, 0, 10},
            .size = {width, height},
            .color = WHITE,
            .texture = TH_SWORD,
        });
    }

    { // gold entity
        f32 ratio = texture_aspect_ratio(&state.renderer, get_texture(TH_GOLD));
        f32 height = 100;
        f32 width = height * ratio;
    
        spawn_entity(Entity{
            .position = {-100, 0, 10},
            .size = {width, height},
            .color = WHITE,
            .texture = TH_GOLD,
        });
    }

    spawn_entity(Entity{
        .flags = EF_LIGHT | EF_PLAYER,
        .position = {-150, 0, 0},
        .light_colour = BLUE,
        .light_intensity = 1,
        .light_radius = 150
    });

    spawn_entity(Entity{
        .flags = EF_LIGHT | EF_ALT_PLAYER,
        .position = {150, 0, 0},
        .light_colour = RED,
        .light_intensity = 1,
        .light_radius = 150
    });
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
            v2 distance_abs = v2{abs(distance.x), abs(distance.y)};
            v2 distance_for_collision = (entity->size + other->size) * v2{0.5, 0.5};

            bool collision = distance_for_collision[0] >= distance_abs[0] && distance_for_collision[1] >= distance_abs[1];
            if (collision) {
                return other;
            }
        }
    }

    return nullptr;
}


Texture *get_texture(TextureHandle handle) {
    if(handle == TH_NONE) {
        return NULL;
    }

    return textures[handle];
}
