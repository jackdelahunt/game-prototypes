#include "libs/libs.h"
#include "engine.cpp"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

// Total: 19:30
// Started: 22:30
//
// Lighting TODO:
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

enum SpriteHandle {
    SH_NONE,
    SH_FLOOR_1,
    SH_FLOOR_2,
    SH_FLOOR_3,
    SH_WALL_1,
    SH_WALL_2,
    SH_ROCK_1,
    SH_ROCK_2,
    SH_COUNT_,
};

Sprite *sprites[SH_COUNT_];

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
    SpriteHandle sprite;

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

Sprite *get_sprite(SpriteHandle handle);

int main() {
    state = {
        .camera = {
            .position = {0, 110, -1},
            .orthographic_size = 180,
            .near_plane = 0.1f,
            .far_plane = 100.0f,
        },
        .renderer = {
            .global_light = {0.3, 0.3, 0.6, 1},
            .clear_colour = {0.2, 0.2, 0.2, 1},
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

        { // load and build all sprites
            Sprite *sprite = NULL;

            sprite = load_sprite(&state.renderer, "resources/textures/caves/tiles/floor_1.png", "resources/textures/caves/tiles/floor_1_normal.png");
            if (sprite == NULL) {
                return 1;
            }

            sprites[SH_FLOOR_1] = sprite;

            sprite = load_sprite(&state.renderer, "resources/textures/caves/tiles/floor_2.png", "resources/textures/caves/tiles/floor_2_normal.png");
            if (sprite == NULL) {
                return 1;
            }

            sprites[SH_FLOOR_2] = sprite;

            sprite = load_sprite(&state.renderer, "resources/textures/caves/tiles/floor_3.png", "resources/textures/caves/tiles/floor_3_normal.png");
            if (sprite == NULL) {
                return 1;
            }

            sprites[SH_FLOOR_3] = sprite;

            sprite = load_sprite(&state.renderer, "resources/textures/caves/tiles/wall_1.png", "");
            if (sprite == NULL) {
                return 1;
            }

            sprites[SH_WALL_1] = sprite;

            sprite = load_sprite(&state.renderer, "resources/textures/caves/tiles/wall_2.png", "");
            if (sprite == NULL) {
                return 1;
            }

            sprites[SH_WALL_2] = sprite;

            sprite = load_sprite(&state.renderer, "resources/textures/caves/props/rock_1.png", "resources/textures/caves/props/rock_1_normal.png");
            if (sprite == NULL) {
                return 1;
            }

            sprites[SH_ROCK_1] = sprite;


            sprite = load_sprite(&state.renderer, "resources/textures/caves/props/rock_2.png", "");
            if (sprite == NULL) {
                return 1;
            }

            sprites[SH_ROCK_2] = sprite;
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

    while (!glfwWindowShouldClose(state.window.glfw_window)) {
        f64 current_time    = state.time;
        f64 new_time        = glfwGetTime();
        f32 delta_time      = (f32) (new_time - current_time);
        state.time          = new_time;

        if (KEYS[GLFW_KEY_ESCAPE] == InputState::down) {
            glfwSetWindowShouldClose(state.window.glfw_window, GLFW_TRUE);
        }

        new_frame(&state.renderer, &state.window, state.camera);

        input(); 
        update_and_draw(delta_time);
        physics(delta_time); 


        draw_frame(&state.renderer, &state.window, state.camera); 

        { // imgui render 
            new_imgui_frame();

            ImGui::Begin("Inspector");

            ImGui::Text("FPS: %f", 1.0f / delta_time);

            if(ImGui::Button("Reload Shaders")) {
                delete_shaders(&state.renderer);
                load_shaders(&state.renderer);
            }

            if(ImGui::CollapsingHeader("Camera")) {
                ImGui::SliderFloat3("position", &state.camera.position[0], -500, 500);
                ImGui::SliderFloat("Orthographic size", &state.camera.orthographic_size, 10, 1000);
            }

            if(ImGui::CollapsingHeader("Rendering")) {
                ImGui::InputFloat4("Global light", &state.renderer.global_light[0]);
            }

            if(ImGui::CollapsingHeader("Render outputs")) {
                ImVec2 image_size(360 * 1.77, 360);

                ImGui::Text("Depth buffer");
                ImGui::Image(state.renderer.unlit_frame_buffer.depth_attachment, image_size, ImVec2(0, 1), ImVec2(1, 0));

                ImGui::Text("Normal buffer");
                ImGui::Image(state.renderer.unlit_frame_buffer.normals_attachment, image_size, ImVec2(0, 1), ImVec2(1, 0));

                ImGui::Text("Colour buffer");
                ImGui::Image(state.renderer.unlit_frame_buffer.colour_attachment, image_size, ImVec2(0, 1), ImVec2(1, 0));
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

            draw_imgui_frame();
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

        if (entity->sprite != SH_NONE) {
            Sprite *sprite = get_sprite(entity->sprite);
            draw_sprite(&state.renderer, sprite, entity->position, entity->size, entity->rotation, entity->color);
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
    f32 ratio = texture_aspect_ratio(&state.renderer, get_sprite(SH_ROCK_1)->albedo);
    f32 height = 150;
    f32 width = height * ratio;
    
    spawn_entity(Entity{
        .position = {0, 0, 10},
        .size = {width, height},
        .color = WHITE,
        .sprite = SH_ROCK_1,
    });

    spawn_entity(Entity{
        .position = {-100, 0, 10},
        .size = {40, 40},
        .color = WHITE,
        .sprite = SH_FLOOR_1,
    });

    spawn_entity(Entity{
        .position = {-140, 0, 10},
        .size = {40, 40},
        .color = WHITE,
        .sprite = SH_FLOOR_2,
    });

    spawn_entity(Entity{
        .position = {-180, 0, 10},
        .size = {40, 40},
        .color = WHITE,
        .sprite = SH_FLOOR_3,
    });

    spawn_entity(Entity{
        .flags = EF_LIGHT | EF_PLAYER,
        .position = {-150, 0, 0},
        .light_colour = ORANGE,
        .light_intensity = 1,
        .light_radius = 400
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
