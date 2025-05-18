#include "libs/libs.h"
#include "engine.cpp"

#include <time.h>
#include <stdlib.h>

// Total: 0
// Started: 15:00

#define MAX_ENTITIES 2000

#define PLAYER_SPEED 400

#define BULLET_SPEED 1200

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
    EF_PLAYER   = 1 << 0,
    EF_BULLET   = 1 << 1,
    EF_DELETE   = 1 << 2,
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
void spawn_player();
void spawn_bullet(v3 position, v2 velocity);

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
    };

    { // init engine stuff
        bool ok = false;

        ok = init_window(&state.window, 1440, 1080, "game7");
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

    { // init game stuff
        spawn_player(); 
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

        new_frame(&state.renderer, &state.window, state.camera);

        update_and_draw(delta_time);
        physics(delta_time);

        draw_frame(&state.renderer, &state.window);
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

        if (entity->flags & EF_PLAYER) {
            v2 movement_input = {};

            if (KEYS[GLFW_KEY_W] == InputState::pressed) {
                movement_input.Y += 1;
            } 

            if (KEYS[GLFW_KEY_S] == InputState::pressed) {
                movement_input.Y += -1;
            }

            if (KEYS[GLFW_KEY_A] == InputState::pressed) {
                movement_input.X += -1;
            } 

            if (KEYS[GLFW_KEY_D] == InputState::pressed) {
                movement_input.X += 1;
            }

            if (movement_input != v2{0, 0}) {
                movement_input = norm(movement_input);
                entity->velocity = movement_input * PLAYER_SPEED;
            } else {
                entity->velocity = v2{0, 0};
            }

            if (KEYS[GLFW_KEY_SPACE] == InputState::down) {
                spawn_bullet(entity->position, v2{0, BULLET_SPEED});
            }
        }

        draw_rectangle(&state.renderer, entity->position, entity->size, entity->color);
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

void spawn_player() {
    spawn_entity(Entity {
        .flags = EF_PLAYER,
        .size = {50, 50},
        .color = RED,
    });
}

void spawn_bullet(v3 position, v2 velocity) {
    spawn_entity(Entity {
        .flags = EF_BULLET,
        .position = position,
        .size = {10, 10},
        .velocity = velocity,
        .color = GREEN,
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
