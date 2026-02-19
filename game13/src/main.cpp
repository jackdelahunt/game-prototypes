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

#define ENABLE_ASSERTS
#define MAX_ENTITIES 1000

f32 g_player_speed = 6;
f32 g_drone_speed = 8;

enum TextureHandle {
    TH_NONE,
    TH_PLAYER,
    TH_GRASS,
    TH_STONE,
    TH_WOOD,
    TH_DRONE,
    TH_COUNT_
};

Texture *textures[TH_COUNT_];

typedef u64 EntityId;

enum class EntityType : u32 {
    None,
    Player,
    Drone,
    Wood,
};

enum EntityFlags : u64 {
    EF_NONE   = 0,
    EF_DELETE = 1 << 0,
};

enum class DroneMission {
    Recall,
    Destroy
};

// @entity
struct Entity {
    // meta
    EntityId id;
    EntityType type;
    u64 flags;

    // entity
    v2 position;
    v2 size;

    // rendering
    v4 color;
    TextureHandle texture;

    // drone
    DroneMission drone_mission;
    EntityId drone_target;
};

struct Editor {
    bool visable;
};

// @state
struct State {
    Camera camera;
    Editor editor;

    bool running;
    f64 time;

    StackArray<Entity, MAX_ENTITIES> entities;
};

bool state_load_textures(State *state);
void state_update_and_draw(State *state, f32 delta_time);
void state_update_and_draw_editor(State *state, f32 delta_time);

void entity_player_update(State *state, Entity *player, f32 delta_time);
void entity_drone_update(State *state, Entity *drone, f32 delta_time);

Entity *entity_spawn(State *state, Entity entity);
Entity *entity_player_spawn(State *state, v2 position);
Entity *entity_drone_spawn(State *state, v2 position, DroneMission mission, EntityId target);
Entity *entity_wood_spawn(State *state, v2 position);
Entity *entity_find_by_id(State *state, EntityId id);
Entity *entity_find_by_type(State *state, EntityType type);
Entity *entity_find_colliding(State *state, v2 point);
bool entity_is_colliding(State *state, Entity *a, Entity *b);

// @tileposition
typedef v2i TilePosition;

TilePosition world_position_to_tile_position(v2 world_position);
v2 tile_position_to_world_position(TilePosition tile_position);

bool point_collision(v2 point, v2 collider_position, v2 collider_size);
bool box_collision(v2 a_position, v2 a_size, v2 b_position, v2 b_size);

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
        .editor = {
            .visable = true,
        },
        .running = true,
    };

    { // init engine stuff
        bool ok = false;

        ok = window_init("game13", 1920, 1080);
        if (!ok) {
            Fatal("failed to init window");
            return 1;
        }

        ok = renderer_init(&g_window);
        if (!ok) {
            Fatal("failed to init the renderer");
            return 1;
        }

        ok = state_load_textures(&state);
        if (!ok) {
            Fatal("failed to load textures");
            return 1;
        }

        ok = renderer_build_atlas();
        if (!ok) {
            Fatal("failed to build texture atlas");
            return 1;
        }

        ok = renderer_load_font("resources/fonts/LibreBaskerville.ttf", 1000, 1000, 160);
        if (!ok) {
            Fatal("failed to load font");
            return 1;
        }

        srand(time(NULL));
    }

    { // init game stuff
        entity_player_spawn(&state, v2{0, 0});
    }

    while (state.running && !window_wants_to_close()) {
        f64 current_time    = state.time;
        f64 new_time        = glfwGetTime();
        f32 delta_time      = (f32) (new_time - current_time);
        state.time          = new_time;

        if (KEYS[GLFW_KEY_ESCAPE] == InputState::DOWN) {
            state.running = false;
        }

        renderer_new_frame(&g_window, state.camera);

        window_poll_inputs(); 
        state_update_and_draw(&state, delta_time);
        state_update_and_draw_editor(&state, delta_time); 

        renderer_draw_frame(&g_window); 

        window_swap_buffers();
    }

    window_close();

    return 0;
}

// @state
bool state_load_textures(State *state) {
    Texture *texture = NULL;

    texture = renderer_load_texture("resources/textures/player/player.png");
    if (texture == NULL) {
        return false;
    }

    textures[TH_PLAYER] = texture;

    texture = renderer_load_texture("resources/textures/grass/grass.png");
    if (texture == NULL) {
        return false;
    }

    textures[TH_GRASS] = texture;

    texture = renderer_load_texture("resources/textures/stone/stone.png");
    if (texture == NULL) {
        return false;
    }

    textures[TH_STONE] = texture;

    texture = renderer_load_texture("resources/textures/wood/wood.png");
    if (texture == NULL) {
        return false;
    }

    textures[TH_WOOD] = texture;

    texture = renderer_load_texture("resources/textures/drone/drone.png");
    if (texture == NULL) {
        return false;
    }

    textures[TH_DRONE] = texture;

    return true;
}


void state_update_and_draw(State *state, f32 delta_time) {
    if (KEYS[GLFW_KEY_F1] == InputState::DOWN) {
        state->editor.visable = !state->editor.visable;
    }

    for (Entity &entity : state->entities) {
        // if this entity's delete flag was set during this update then
        // don't update it and wait until updating is over to delete it
        if (BitSet(entity.flags, EF_DELETE)) {
            continue;
        }

        switch (entity.type) {
            case EntityType::None:
                Unreachable("entity with 'None' type given when trying to update and draw");
            break;
            case EntityType::Player: 
                entity_player_update(state, &entity, delta_time); 
            break;
            case EntityType::Drone: 
                entity_drone_update(state, &entity, delta_time); 
            break;
            case EntityType::Wood: break;
            default:
                Unreachable("unknown entity type given when trying to update and draw");
            break;
        }

        renderer_draw_texture(textures[entity.texture], v3{entity.position.x, entity.position.y, 0}, entity.size, 0, entity.color);
    }

    for (i64 i = 0; i < state->entities.len; i++) {
        Entity *entity = &state->entities[i];

        if (!BitSet(entity->flags, EF_DELETE)) {
            continue;
        }

        swap_remove(&state->entities, i);
    }

    const i32 terrain_width = 60;
    const i32 terrain_height = 60;

    for (i32 x = -(terrain_width / 2); x < (terrain_width / 2); x++) {
        for (i32 y = -(terrain_height / 2); y < (terrain_height / 2); y++) {
            v3 position = v3{f32(x), f32(y), 0};
            TextureHandle texture = TH_GRASS;

            if (x == 0 || y == 0) {
                texture = TH_STONE;
            }

            renderer_draw_texture(textures[texture], position, v2{1, 1}, 0, WHITE);
        }
    }
}

void state_update_and_draw_editor(State *state, f32 delta_time) {
    if(state->editor.visable) {
        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_NoDockingOverCentralNode);

        // ImGui::ShowDemoWindow();

        {
            ImGui::Begin("Inspector");
        
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

                ImGui::Text("FPS: %f", 1.0f / delta_time);
                ImGui::PlotLines("Frame times", frame_times, BUFFER_SIZE, 0, overlay, -1, 1, ImVec2(300, 100));
            }

            {
                ImGui::SeparatorText("Globals");
                ImGui::SliderFloat("Player speed", &g_player_speed, 0, 20);
                ImGui::SliderFloat("Drone speed", &g_drone_speed, 0, 20);
            }

            {
                ImGui::SeparatorText("Camera");
                ImGui::SliderFloat3("Camera position", &state->camera.position[0], -50, 50);
                ImGui::SliderFloat3("Camera rotation", &state->camera.rotation[0], -360, 360);
                ImGui::SliderFloat("Ortho Size", &state->camera.orthographic_size, 1, 100);
            }

            {
                v2 world_position = screen_position_to_world_position(&g_window, MOUSE.position, state->camera);
                v2i tile_position = world_position_to_tile_position(world_position);

                ImGui::SeparatorText("Mouse");
                ImGui::Text("Screen position: %f, %f", MOUSE.position.x, MOUSE.position.y);
                ImGui::Text("World position: %f, %f", world_position.x, world_position.y);
                ImGui::Text("Tile position: %d, %d", tile_position.x, tile_position.y);
            }

            {
                ImGui::SeparatorText("Window");
                ImGui::Text("Logical size: %d, %d", g_window.logical_size.x, g_window.logical_size.y);
                ImGui::Text("Frame buffer size: %d, %d", g_window.frame_buffer_size.x, g_window.frame_buffer_size.y);
            }

            {
                ImGui::SeparatorText("Renderer");

                if(ImGui::Button("Reload Shaders")) {
                    renderer_delete_shaders();
                    renderer_load_shaders();
                }

                ImGui::SameLine();
            
                if(ImGui::Button("Toggle V-sync")) {
                    window_toggle_vsync();
                } 

                ImGui::SliderFloat4("Clear colour", &g_renderer.clear_colour[0], 0, 1);

                if(ImGui::CollapsingHeader("Render outputs")) {
                    ImVec2 image_size(360 * 1.777, 360);
    
                    ImGui::Text("g_buffer albedo");
                    ImGui::Image(g_renderer.gbuffer.albedo_attachment, image_size, ImVec2(0, 1), ImVec2(1, 0));
    
                    ImGui::Text("g_buffer depth");
                    ImGui::Image(g_renderer.gbuffer.depth_attachment, image_size, ImVec2(0, 1), ImVec2(1, 0));
                }
            }
       
            ImGui::End();
        }
    }
}

void entity_player_update(State *state, Entity *player, f32 delta_time) {
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

        player->position.x += input.x * g_player_speed * delta_time;
        player->position.y += input.y * g_player_speed * delta_time;

        state->camera.position.x = player->position.x;
        state->camera.position.y = player->position.y;
    }

    { // mouse interaction
        if (MOUSE.buttons[GLFW_MOUSE_BUTTON_1] == InputState::DOWN) {
            v2 mouse_position = screen_position_to_world_position(&g_window, MOUSE.position, state->camera);

            Entity *other = entity_find_colliding(state, mouse_position);
            if (other) {
                other->color = RED;
                entity_drone_spawn(state, player->position, DroneMission::Destroy, other->id);
            }
        }

        if (MOUSE.buttons[GLFW_MOUSE_BUTTON_2] == InputState::DOWN) {
            v2 mouse_position = screen_position_to_world_position(&g_window, MOUSE.position, state->camera);
            v2i tile_position = world_position_to_tile_position(mouse_position);
            v2 spawn_position = tile_position_to_world_position(tile_position);

            entity_wood_spawn(state, spawn_position);
        }
    }
}

void entity_drone_update(State *state, Entity *drone, f32 delta_time) {
    if (drone->drone_target == 0) {
        Entity *player = entity_find_by_type(state, EntityType::Player);
        if (player != NULL) {
            drone->drone_mission = DroneMission::Recall;
            drone->drone_target = player->id;
        }

        return;
    }

    Entity *target_entity = entity_find_by_id(state, drone->drone_target);
    if (target_entity == NULL) {
        drone->drone_target = 0; 
        return;
    }

    v2 direction = target_entity->position - drone->position;
    direction = norm(direction);
    drone->position += direction * g_drone_speed * delta_time;

    if (!entity_is_colliding(state, drone, target_entity)) {
        return;
    }

    if (drone->drone_mission == DroneMission::Recall) {
        SetBit(drone->flags, EF_DELETE);
        return;
    }

    if (drone->drone_mission == DroneMission::Destroy) {
        SetBit(target_entity->flags, EF_DELETE);
        return;
    }
}

Entity *entity_spawn(State *state, Entity entity) {
    static EntityId id_counter = 1; 

    Entity *ptr = push(&state->entities);
    *ptr = entity;
    ptr->id = id_counter++;

    return ptr;
}

Entity *entity_player_spawn(State *state, v2 position) {
    Entity entity = Entity{
        .type = EntityType::Player,
        .flags = EF_NONE,
        .position = position,
        .size = v2{1, 1.5},
        .color = WHITE,
        .texture = TH_PLAYER
    };

    return entity_spawn(state, entity);
}

Entity *entity_drone_spawn(State *state, v2 position, DroneMission mission, EntityId target) {
    Entity entity = Entity{
        .type = EntityType::Drone,
        .flags = EF_NONE,
        .position = position,
        .size = v2{0.5, 0.5},
        .color = WHITE,
        .texture = TH_DRONE,
        .drone_mission = mission,
        .drone_target = target
    };

    return entity_spawn(state, entity);
}

Entity *entity_wood_spawn(State *state, v2 position) {
    Entity wood = Entity{
        .type = EntityType::Wood,
        .flags = EF_NONE,
        .position = position,
        .size = v2{1, 1},
        .color = WHITE,
        .texture = TH_WOOD,
    };

    return entity_spawn(state, wood);
}

Entity *entity_find_by_id(State *state, EntityId id) {
    for (Entity &entity : state->entities) {
        if (entity.id == id) {
            return &entity;
        }
    }

    return NULL;
}

Entity *entity_find_by_type(State *state, EntityType type) {
    for (Entity &entity : state->entities) {
        if (entity.type == type) {
            return &entity;
        }
    }

    return NULL;
}

Entity *entity_find_colliding(State *state, v2 point) {
    for (Entity &entity : state->entities) {
        if (point_collision(point, entity.position, entity.size)) {
            return &entity;
        }
    }
}

bool entity_is_colliding(State *state, Entity *a, Entity *b) {
    return box_collision(a->position, a->size, b->position, b->size);
}

// @tileposition
TilePosition world_position_to_tile_position(v2 world_position) {
    i32 x = i32(llroundf(world_position.x));
    i32 y = i32(llroundf(world_position.y));

    return TilePosition{x, y};
}

v2 tile_position_to_world_position(TilePosition tile_position) {
    return v2{f32(tile_position.x), f32(tile_position.y)};  
}

// AABB detection for a point against a box where the position is centred on the box
bool point_collision(v2 point, v2 collider_position, v2 collider_size) {
    v2 delta_position = point - collider_position;
    v2 bounding_box = collider_size * 0.5;

    return (
        delta_position.x >= -bounding_box.x && delta_position.x < bounding_box.x &&
        delta_position.y >= -bounding_box.y && delta_position.y < bounding_box.y
    );
}

bool box_collision(v2 a_position, v2 a_size, v2 b_position, v2 b_size) {
    v2 distance = b_position - a_position;
    v2 distance_abs = v2{ABS(distance.x), ABS(distance.y)};
    v2 distance_for_collision = (a_size + b_size) * 0.5; 

    return distance_for_collision.x >= distance_abs.x && distance_for_collision.y >= distance_abs.y;
}
