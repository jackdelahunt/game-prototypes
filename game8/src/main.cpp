#include "libs/libs.h"
#include "engine.cpp"

#include <cmath>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

// cursed c++ headers to get saving working
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <filesystem>

// Total: 26:00
// Started: 19:00 
//
// Lighting TODO:
// - bloom

#define MAX_ENTITIES 2000
#define DEFAULT_SAVE_FILE "resources/saves/scene.json"
f32 CAMERA_START_X = 0;
f32 CAMERA_END_X = 820;

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
    SH_WALL_3,
    SH_WALL_4,
    SH_ROCK_1,
    SH_ROCK_2,
    SH_BACKGROUND_1,
    SH_BACKGROUND_2,
    SH_BACKGROUND_3,
    SH_CORNER_1,
    SH_CORNER_2,
    SH_SPIKE,
    SH_ORE_1,
    SH_ORE_2,
    SH_GEM_1,
    SH_GEM_2,
    SH_BAT,
    SH_COUNT_,
};

Sprite *sprites[SH_COUNT_];

enum Prefab {
    PF_NONE,
    PF_FLOOR_1,
    PF_FLOOR_2,
    PF_FLOOR_3,
    PF_WALL_1,
    PF_WALL_2,
    PF_WALL_3,
    PF_WALL_4,
    PF_ROCK_1,
    PF_ROCK_2,
    PF_BACKGROUND_1,
    PF_BACKGROUND_2,
    PF_BACKGROUND_3,
    PF_CORNER_1,
    PF_CORNER_2,
    PF_LIGHT,
    PF_SPIKE,
    PF_ORE_1,
    PF_ORE_2,
    PF_GEM_1,
    PF_GEM_2,
    PF_BAT,
    PF_COUNT_,
};

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
    Entity *selected_entity;
    bool snap_to_grid;
    v2 grid_size;
    v2 selection_range;
};

enum EntityFlags {
    EF_LIGHT            = 1 << 0,
    EF_PLAYER           = 1 << 1,
    EF_GREEN_ORE        = 1 << 2,
    EF_RED_ORE          = 1 << 3,
    EF_ANIMATED_SPRITE  = 1 << 4,
    EF_FLIPPED_SPRITE   = 1 << 5,
    EF_DELETE           = 1 << 16,
};

struct State {
    Camera camera;
    Window window;
    Renderer renderer;
    SoundEngine sound_engine;
    Editor editor;

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
void draw_editor(f32 delta_time);

Entity *spawn_entity(Entity entity);
Entity create_prefab(Prefab prefab);

void to_json(json& j, const Entity& entity);
void from_json(const json& j, Entity& entity);

bool file_exists(const char *path);
bool copy_file(const char *path, const char *new_path);
std::string read_entire_file(const char *path);
void backup_scene();
void save_scene(State *state);
void load_scene(State *state);

CollisionIterator new_collision_iterator(Entity *entity);
Entity *next(CollisionIterator *iterator);

Sprite *get_sprite(SpriteHandle handle);

int main() {
    state = State {
        .camera = {
            .position = {CAMERA_START_X, 150, -1},
            .orthographic_size = 300,
            .near_plane = 0.1f,
            .far_plane = 100.0f,
        },
        .renderer = {
            .global_light = {0.15, 0.15, 0.3, 1},
            .clear_colour = {0.2, 0.2, 0.2, 1},
        },
        .editor = {
            .snap_to_grid = true,
            .grid_size = {50, 50},
            .selection_range = {0, 20},
        }
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

            sprite = load_sprite(&state.renderer, "resources/textures/caves/tiles/wall_1.png", "resources/textures/caves/tiles/wall_1_normal.png");
            if (sprite == NULL) {
                return 1;
            }

            sprites[SH_WALL_1] = sprite;

            sprite = load_sprite(&state.renderer, "resources/textures/caves/tiles/wall_2.png", "resources/textures/caves/tiles/wall_2_normal.png");
            if (sprite == NULL) {
                return 1;
            }

            sprites[SH_WALL_2] = sprite;

            sprite = load_sprite(&state.renderer, "resources/textures/caves/tiles/wall_3.png", "resources/textures/caves/tiles/wall_3_normal.png");
            if (sprite == NULL) {
                return 1;
            }

            sprites[SH_WALL_3] = sprite;

            sprite = load_sprite(&state.renderer, "resources/textures/caves/tiles/wall_4.png", "");
            if (sprite == NULL) {
                return 1;
            }

            sprites[SH_WALL_4] = sprite;

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


            sprite = load_sprite(&state.renderer, "resources/textures/caves/backgrounds/background_1.png", "");
            if (sprite == NULL) {
                return 1;
            }

            sprites[SH_BACKGROUND_1] = sprite;


            sprite = load_sprite(&state.renderer, "resources/textures/caves/backgrounds/background_2.png", "");
            if (sprite == NULL) {
                return 1;
            }

            sprites[SH_BACKGROUND_2] = sprite;

            sprite = load_sprite(&state.renderer, "resources/textures/caves/backgrounds/background_3.png", "");
            if (sprite == NULL) {
                return 1;
            }

            sprites[SH_BACKGROUND_3] = sprite;

            sprite = load_sprite(&state.renderer, "resources/textures/caves/tiles/corner_1.png", "");
            if (sprite == NULL) {
                return 1;
            }

            sprites[SH_CORNER_1] = sprite;

            sprite = load_sprite(&state.renderer, "resources/textures/caves/tiles/corner_2.png", "");
            if (sprite == NULL) {
                return 1;
            }

            sprites[SH_CORNER_2] = sprite;


            sprite = load_sprite(&state.renderer, "resources/textures/caves/props/spike.png", "resources/textures/caves/props/spike_normal.png");
            if (sprite == NULL) {
                return 1;
            }

            sprites[SH_SPIKE] = sprite;

            sprite = load_sprite(&state.renderer, "resources/textures/caves/tiles/ore_1.png", "resources/textures/caves/tiles/ore_1_normal.png");
            if (sprite == NULL) {
                return 1;
            }

            sprites[SH_ORE_1] = sprite;

            sprite = load_sprite(&state.renderer, "resources/textures/caves/tiles/ore_2.png", "resources/textures/caves/tiles/ore_2_normal.png");
            if (sprite == NULL) {
                return 1;
            }

            sprites[SH_ORE_2] = sprite;

            sprite = load_sprite(&state.renderer, "resources/textures/caves/props/gem_1.png", "");
            if (sprite == NULL) {
                return 1;
            }

            sprites[SH_GEM_1] = sprite;


            sprite = load_sprite(&state.renderer, "resources/textures/caves/props/gem_2.png", "");
            if (sprite == NULL) {
                return 1;
            }

            sprites[SH_GEM_2] = sprite;


            sprite = load_animated_sprite(&state.renderer, "resources/textures/bat/bat.png", 4, 1);
            if (sprite == NULL) {
                return 1;
            }

            sprites[SH_BAT] = sprite;
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

    load_scene(&state);

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
        draw_editor(delta_time); 

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

    for (int i = 0; i < MOUSE.buttons.size; i++) {
        if (MOUSE.buttons[i] == InputState::down) {
            MOUSE.buttons[i] = InputState::pressed;
        }
    }

    glfwPollEvents();
}

void update_and_draw(f32 delta_time) {
    // check to see for a new selected entity
    if(MOUSE.buttons[GLFW_MOUSE_BUTTON_1] == InputState::down) {
        v2 world_position = screen_position_to_world_position(MOUSE.position, state.camera, &state.window);

        for (int i = 0; i < state.entities.len; i++) {
            Entity* entity = &state.entities[i];
            if (entity == state.editor.selected_entity) {
                continue; // means it gives a chance to select an entity that is overlapping
            }

            if (entity->position.z < state.editor.selection_range.x || entity->position.z > state.editor.selection_range.y) {
                continue;
            }

            f32 low_x = entity->position.x - (entity->size.x * 0.5);
            f32 high_x = entity->position.x + (entity->size.x * 0.5);
            f32 low_y = entity->position.y - (entity->size.y * 0.5);
            f32 high_y = entity->position.y + (entity->size.y * 0.5);

            if ((world_position.x >= low_x && world_position.x <= high_x) &&
                (world_position.y >= low_y && world_position.y <= high_y)) {
                state.editor.selected_entity = entity;
                break;
            }
        }
    }

    { // duplicate selected entity
        if(KEYS[GLFW_KEY_SPACE] == InputState::down) {
            printf("new entity\n");
            Entity copy = *state.editor.selected_entity;
            copy.position += {copy.size.x, 0, 0};
            state.editor.selected_entity = spawn_entity(copy);
        }
    }
        
    { // delete selected entity
        if(state.editor.selected_entity != NULL && KEYS[GLFW_KEY_DELETE] == InputState::down) {
            state.editor.selected_entity->flags |= EF_DELETE;
            state.editor.selected_entity = NULL;
        }
    }

    { // rotate selected entity
        if(state.editor.selected_entity != NULL && KEYS[GLFW_KEY_R] == InputState::down) {

            if(KEYS[GLFW_KEY_LEFT_SHIFT] == InputState::pressed) {
                state.editor.selected_entity->rotation -= 90;
            } else {
                state.editor.selected_entity->rotation += 90;
            }
        }
    }

    { // update position of selected entity, with or without grid
    
        // if we are using grid then you just want to press it once
        // to move but if not you can hold it down
        InputState input_type;
        if(state.editor.snap_to_grid) {
            input_type = InputState::down;
        } else {
            input_type = InputState::pressed;
        }
   
        v2 input = {};

        if (KEYS[GLFW_KEY_UP] == input_type) {
            input.y += 1;
        }
    
        if (KEYS[GLFW_KEY_DOWN] == input_type) {
            input.y -= 1;
        }
    
        if (KEYS[GLFW_KEY_LEFT] == input_type) {
            input.x -= 1;
        }
    
        if (KEYS[GLFW_KEY_RIGHT] == input_type) {
            input.x += 1;
        }

        Entity *entity = state.editor.selected_entity;
    
        if (length(input) != 0) {
            if(state.editor.snap_to_grid) {
                v2 grid_index = v2{entity->position.x, entity->position.y} / state.editor.grid_size;
                grid_index.x = truncf(grid_index.x);
                grid_index.y = truncf(grid_index.y); 

                grid_index += input;
                v2 new_position = grid_index * state.editor.grid_size;
                entity->position = v3{new_position.x, new_position.y, entity->position.z};
            } else {
                entity->position += v3{input.x, input.y, 0} * 10;
            }
        }
    }

    for (int i = 0; i < state.entities.len; i++) {
        Entity* entity = &state.entities[i];

        if (entity->flags & EF_GREEN_ORE || entity->flags & EF_RED_ORE) {
            f32 t; // 0 -> 1

            if(entity->flags & EF_GREEN_ORE) {
                t = (sin(state.time) + 1) * 0.5; 
            } else {
                t = (cos(state.time) + 1) * 0.5;
            }

            f32 a =  (0.6 + (0.4 * t));

            entity->light_intensity = a;
            entity->light_radius = a * 100;
        }

        if (entity->flags & EF_PLAYER) {

            { // update camera position
                state.camera.position.x = clamp(CAMERA_START_X, entity->position.x, CAMERA_END_X);
            }

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

            entity->velocity = input * 250;

            if (entity->velocity.x < 0) {
                entity->flags |= EF_FLIPPED_SPRITE;
            }
            else if (entity->velocity.x > 0) {
                entity->flags &= ~EF_FLIPPED_SPRITE;
            }
        }

        if (entity->flags & EF_ANIMATED_SPRITE) {
            entity->animation_cycle += delta_time;

            Sprite *sprite = get_sprite(entity->sprite);
            if(entity->animation_cycle >= sprite->albedo->animation_length) {
                entity->animation_cycle = 0;
            }
        }

        if (entity->flags & EF_LIGHT) {
            draw_light(&state.renderer, entity->position, entity->light_radius, entity->light_colour, entity->light_intensity);
        } 

        if (entity->sprite != SH_NONE) {
            Sprite *sprite = get_sprite(entity->sprite);

            // highlight green as selected entity
            v4 draw_colour = entity->color;
            if (entity == state.editor.selected_entity) {
                draw_colour = GREEN;
                draw_circle(&state.renderer, entity->position, 5, alpha(RED, 0.4));
            }

            if(sprite->albedo->type == TextureType::ANIMATED) {
                bool flipped = entity->flags & EF_FLIPPED_SPRITE;

                draw_animated_sprite(&state.renderer, sprite, entity->animation_cycle, entity->position, entity->size, entity->rotation, draw_colour, flipped);
            }
            else {
                draw_sprite(&state.renderer, sprite, entity->position, entity->size, entity->rotation, draw_colour);
            }
        }
    }

#if 0
    { // draw grid lines
        i64 grid_region_width = 2000;
        i64 grid_region_height = 2000;
        f32 line_thickness = 1;
        v4 grid_colour = BLACK;

        // horizontal lines
        for(i64 y = (-grid_region_height) / 2; y <= grid_region_height / 2; y += (i64) state.editor.grid_size.y) {
            draw_rectangle(&state.renderer, {0, (f32) y, 90}, {(f32) grid_region_width, line_thickness}, grid_colour);
        }

        // vertical lines
        for(i64 x = (-grid_region_width) / 2; x <= grid_region_width / 2; x += (i64) state.editor.grid_size.x) {
            draw_rectangle(&state.renderer, {(f32) x, 0, 90}, {line_thickness, (f32) grid_region_height}, grid_colour);
        }
    }
#endif

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

void draw_editor(f32 delta_time) {
    new_imgui_frame();
    ImGui::Begin("Editor");

    ImGui::Text("FPS: %f", 1.0f / delta_time);

    { // top level buttons
        if(ImGui::Button("Deselect Entity")) {
            state.editor.selected_entity = NULL;
        }

        if(ImGui::Button("Reload Shaders")) {
            delete_shaders(&state.renderer);
            load_shaders(&state.renderer);
        }
    
        ImGui::SameLine();
        if(ImGui::Button("Save scene")) {
            save_scene(&state);
        }
    
        ImGui::SameLine();
        if(ImGui::Button("Load scene")) {
            load_scene(&state);
        }
    }

    ImGui::Separator();

    { // grid settings 
        ImGui::Checkbox("Snap to grid", &state.editor.snap_to_grid);

        ImGui::SameLine();

        if(ImGui::Button("Use entity size")) {
            if (state.editor.selected_entity != NULL) {
                state.editor.grid_size = state.editor.selected_entity->size;
            }
        }

        ImGui::InputFloat2("Grid size", &state.editor.grid_size[0]);
        ImGui::InputFloat2("selection range", &state.editor.selection_range[0]);
    }

    if(ImGui::CollapsingHeader("Settings")) {
        ImGui::SliderFloat3("Camera position", &state.camera.position[0], -500, 2000);
        ImGui::SliderFloat("Orthographic size", &state.camera.orthographic_size, 10, 2000);
        ImGui::InputFloat4("Global light", &state.renderer.global_light[0]);
    }

    if(ImGui::CollapsingHeader("Prefabs")) {
        Prefab selected_prefab = PF_NONE;
                
        if(ImGui::Button("Floor 1")) {
            selected_prefab = PF_FLOOR_1;
        }

        ImGui::SameLine();
        if(ImGui::Button("Floor 2")) {
            selected_prefab = PF_FLOOR_2;
        }

        ImGui::SameLine();
        if(ImGui::Button("Floor 3")) {
            selected_prefab = PF_FLOOR_3;
        }

        if(ImGui::Button("Wall 1")) {
            selected_prefab = PF_WALL_1;
        }

        ImGui::SameLine();
        if(ImGui::Button("Wall 2")) {
            selected_prefab = PF_WALL_2;
        }

        ImGui::SameLine();
        if(ImGui::Button("Wall 3")) {
            selected_prefab = PF_WALL_3;
        }

        ImGui::SameLine();
        if(ImGui::Button("Wall 4")) {
            selected_prefab = PF_WALL_4;
        }

        if(ImGui::Button("Rock 1")) {
            selected_prefab = PF_ROCK_1;
        }

        ImGui::SameLine();
        if(ImGui::Button("Rock 2")) {
            selected_prefab = PF_ROCK_2;
        }

        if(ImGui::Button("Background 1")) {
            selected_prefab = PF_BACKGROUND_1;
        }

        ImGui::SameLine();

        if(ImGui::Button("Background 2")) {
            selected_prefab = PF_BACKGROUND_2;
        }

        ImGui::SameLine();

        if(ImGui::Button("Background 3")) {
            selected_prefab = PF_BACKGROUND_3;
        }

        if(ImGui::Button("Corner 1")) {
            selected_prefab = PF_CORNER_1;
        }

        ImGui::SameLine();

        if(ImGui::Button("Corner 2")) {
            selected_prefab = PF_CORNER_2;
        }

        if(ImGui::Button("Light")) {
            selected_prefab = PF_LIGHT;
        }

        if(ImGui::Button("Spike")) {
            selected_prefab = PF_SPIKE;
        }

        if(ImGui::Button("Ore 1")) {
            selected_prefab = PF_ORE_1;
        }

        ImGui::SameLine();
        if(ImGui::Button("Ore 2")) {
            selected_prefab = PF_ORE_2;
        }

        if(ImGui::Button("Gem 1")) {
            selected_prefab = PF_GEM_1;
        }

        ImGui::SameLine();
        if(ImGui::Button("Gem 2")) {
            selected_prefab = PF_GEM_2;
        }

        if(ImGui::Button("Bat")) {
            selected_prefab = PF_BAT;
        }

        if (selected_prefab != PF_NONE) {
            Entity new_entity = create_prefab(selected_prefab);
            new_entity.position = v3 {state.camera.position.x, state.camera.position.y, 10};
            state.editor.selected_entity = spawn_entity(new_entity);
        }
    }

    if(state.editor.selected_entity != NULL && ImGui::CollapsingHeader("Entity Editor")) {
        Entity *entity = state.editor.selected_entity;

        ImGui::InputFloat3("position", &entity->position[0]);
        ImGui::InputFloat2("size", &entity->size[0]);
        ImGui::InputFloat("rotation", &entity->rotation);
        ImGui::InputFloat4("colour", &entity->color[0]);
        ImGui::InputFloat4("light_colour", &entity->light_colour[0]);
        ImGui::InputFloat("light_radius", &entity->light_radius);
        ImGui::InputFloat("light_intensity", &entity->light_intensity);
        ImGui::InputInt("sprite", (i32 *) &entity->sprite);
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

    ImGui::End();
    draw_imgui_frame();
}

Entity *spawn_entity(Entity entity) {
    Entity *ptr = push(&state.entities);
    *ptr = entity;

    return ptr;
}

Entity create_prefab(Prefab prefab) {
    switch (prefab) {
        case PF_FLOOR_1: {
             return Entity {
                .size = {50, 50},
                .color = WHITE,
                .sprite = SH_FLOOR_1,
            };
        };
        case PF_FLOOR_2: {
             return Entity {
                .size = {50, 50},
                .color = WHITE,
                .sprite = SH_FLOOR_2,
            };
        };
        case PF_FLOOR_3: {
             return Entity {
                .size = {50, 50},
                .color = WHITE,
                .sprite = SH_FLOOR_3,
            };
        };
        case PF_WALL_1: {
             return Entity {
                .size = {50, 50},
                .color = WHITE,
                .sprite = SH_WALL_1,
            };
        };
        case PF_WALL_2: {
             return Entity {
                .size = {50, 50},
                .color = WHITE,
                .sprite = SH_WALL_2,
            };
        };
        case PF_WALL_3: {
             return Entity {
                .size = {50, 50},
                .color = WHITE,
                .sprite = SH_WALL_3,
            };
        };
        case PF_WALL_4: {
             return Entity {
                .size = {50, 50},
                .color = WHITE,
                .sprite = SH_WALL_4,
            };
        };
        case PF_ROCK_1: {
            f32 ratio = texture_aspect_ratio(&state.renderer, get_sprite(SH_ROCK_1)->albedo);
            f32 height = 120;
            f32 width = height * ratio;

             return Entity {
                .size = {width, height},
                .color = WHITE,
                .sprite = SH_ROCK_1,
            };
        };
        case PF_ROCK_2: {
            f32 ratio = texture_aspect_ratio(&state.renderer, get_sprite(SH_ROCK_2)->albedo);
            f32 height = 120;
            f32 width = height * ratio;

             return Entity {
                .size = {width, height},
                .color = WHITE,
                .sprite = SH_ROCK_2,
            };
        };
        case PF_BACKGROUND_1: {
            f32 ratio = texture_aspect_ratio(&state.renderer, get_sprite(SH_BACKGROUND_1)->albedo);
            f32 height = 600;
            f32 width = height * ratio;

             return Entity {
                .size = {width, height},
                .color = WHITE,
                .sprite = SH_BACKGROUND_1,
            };
        };
        case PF_BACKGROUND_2: {
            f32 ratio = texture_aspect_ratio(&state.renderer, get_sprite(SH_BACKGROUND_2)->albedo);
            f32 height = 600;
            f32 width = height * ratio;

             return Entity {
                .size = {width, height},
                .color = WHITE,
                .sprite = SH_BACKGROUND_2,
            };
        };
        case PF_BACKGROUND_3: {
            f32 ratio = texture_aspect_ratio(&state.renderer, get_sprite(SH_BACKGROUND_3)->albedo);
            f32 height = 600;
            f32 width = height * ratio;

             return Entity {
                .size = {width, height},
                .color = WHITE,
                .sprite = SH_BACKGROUND_3,
            };
        };
        case PF_CORNER_1: {
             return Entity {
                .size = {50, 50},
                .color = WHITE,
                .sprite = SH_CORNER_1,
            };
        };
        case PF_CORNER_2: {
             return Entity {
                .size = {50, 50},
                .color = WHITE,
                .sprite = SH_CORNER_2,
            };
        };
        case PF_LIGHT: {
             return Entity {
                .flags = EF_LIGHT | EF_PLAYER,
                .size = {20, 20},
                .light_colour = WHITE,
                .light_intensity = 1,
                .light_radius = 300,
            };
        };
        case PF_SPIKE: {
             return Entity {
                .size = {50, 33},
                .color = WHITE,
                .sprite = SH_SPIKE
            };
        };
        case PF_ORE_1: {
             return Entity {
                .flags = EF_LIGHT,
                .size = {50, 50},
                .color = WHITE,
                .sprite = SH_ORE_1,
                .light_colour = v4{0.15, 0.4, 1, 1},
                .light_intensity = 1,
                .light_radius = 90,
            };
        };
        case PF_ORE_2: {
             return Entity {
                .flags = EF_LIGHT,
                .size = {50, 50},
                .color = WHITE,
                .sprite = SH_ORE_2,
                .light_colour = RED,
                .light_intensity = 1,
                .light_radius = 90,
            };
        };
        case PF_GEM_1: {
             return Entity {
                .flags = EF_LIGHT,
                .size = {50, 50},
                .color = WHITE,
                .sprite = SH_GEM_1,
                .light_colour = v4{0.3, 0.5, 1, 1},
                .light_intensity = 1,
                .light_radius = 50,
            };
        };
        case PF_GEM_2: {
             return Entity {
                .flags = EF_LIGHT,
                .size = {50, 50},
                .color = WHITE,
                .sprite = SH_GEM_2,
                .light_colour = RED,
                .light_intensity = 1,
                .light_radius = 50,
            };
        };
        case PF_BAT: {
             return Entity {
                .flags = EF_LIGHT | EF_ANIMATED_SPRITE | EF_PLAYER,
                .size = {50, 50},
                .color = WHITE,
                .sprite = SH_BAT,
                .light_colour = WHITE,
                .light_intensity = 0.8,
                .light_radius = 200,
            };
        };
        default:
            return {};
    }
}

bool file_exists(const char *path) {
    return std::filesystem::exists(path) && std::filesystem::is_regular_file(path);
}

bool copy_file(const char *path, const char *new_path) {
    if (!file_exists(path)) {
        return false;
    }

    std::ifstream src(path, std::ios::binary);
    std::ofstream dst(new_path, std::ios::binary);

    if (!src || !dst) return false;

    dst << src.rdbuf();

    return src && dst;
}

std::string read_entire_file(const char *path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return ""; // Could also throw or handle error differently
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
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
