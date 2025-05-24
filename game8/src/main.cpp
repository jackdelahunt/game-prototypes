#include "libs/libs.h"
#include "engine.cpp"

#include <cstdio>
#include <cstring>
#include <time.h>
#include <stdlib.h>

// Total: 05:15
// Started: 23:00
//
#define MAX_ENTITIES 2000

enum TextureHandle {
    TH_NONE,
    TH_FACE,
    TH_FACES,
    TH_BACKGROUND,
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

    // animated
    f32 animation_cycle;
};

enum EntityFlags {
    EF_LIGHT            = 1 << 0,
    EF_SHOP             = 1 << 1,
    EF_DELETE           = 1 << 16,
};

struct State {
    Camera camera;
    Window window;
    Renderer renderer;
    SoundEngine sound_engine;

    f32 light_radius;
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
            .orthographic_size = 200,
            .near_plane = 0.1f,
            .far_plane = 100.0f,
        },
        .renderer = {
            .global_light = {0.2, 0.2, 0.2, 1},
            .light_colour = {1, 1, 1, 1},
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

            texture = load_texture(&state.renderer, "resources/textures/background.png");
            if (texture == NULL) {
                return 1;
            }

            textures[TH_BACKGROUND] = texture;
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
                        state.renderer.lights[i].position.X,
                        state.renderer.lights[i].position.Y
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
            }
    
            glDrawElements(GL_TRIANGLES, 6 * state.renderer.quads.len, GL_UNSIGNED_INT, 0);

            reset(&state.renderer.lights);
        }

#ifdef DEBUG
        { // imgui render 
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame(); 

            ImGui::Begin("Inspector");

            if(ImGui::Button("Reload Shaders")) {
                delete_shaders(&state.renderer);
                load_shaders(&state.renderer);
            }

            ImGui::SliderFloat("Light radius", &state.light_radius, 1, 1000);

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
#endif

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
            draw_light(&state.renderer, entity->position, state.light_radius);
        } 

        if (entity->texture != TH_NONE) {
            Texture *texture = get_texture(entity->texture);

            if(texture->type == TextureType::SINGLE) {
                draw_texture(&state.renderer, texture, entity->position, entity->size, entity->rotation, entity->color);
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

        entity->position.X += entity->velocity.X * delta_time;
        entity->position.Y += entity->velocity.Y * delta_time;
    }
}

void spawn_entity(Entity entity) {
    entity.time_created = state.time;

    append(&state.entities, entity);
}

void create_scene() {
    spawn_entity(Entity{
        .position = {0, 0, 80},
        .size = {1920, 1080},
        .color = WHITE,
        .texture = TH_BACKGROUND,
    });

    f32 ratio = texture_aspect_ratio(&state.renderer, get_texture(TH_FACE));
    f32 height = 50;
    f32 width = height * ratio;

    spawn_entity(Entity{
        .position = {0, 0, 30},
        .size = {width, height},
        .color = WHITE,
        .texture = TH_FACE,
    });

    spawn_entity(Entity{
        .position = {-width, height, 30},
        .size = {width, height},
        .color = WHITE,
        .texture = TH_FACES,
    });

    spawn_entity(Entity{
        .flags = EF_LIGHT,
        .position = {0, 0, 0},
        .color = WHITE,
    });

#if 0
    { // shop
        f32 ratio = texture_aspect_ratio(&state.renderer, TH_SHOP_1);
        f32 height = 420;
        f32 width = height * ratio;

        spawn_entity(Entity{
            .flags = EF_ANIMATED_TEXTURE | EF_SHOP,
            .position = {-400, -40, decorations_foreground_z},
            .size = {width, height},
            .color = WHITE,
            .texture = TH_SHOP_1,
            .animation_cycle_amount = 1.5,
            .animation_cycle = 0,
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
#endif
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


Texture *get_texture(TextureHandle handle) {
    if(handle == TH_NONE) {
        return NULL;
    }

    return textures[handle];
}
