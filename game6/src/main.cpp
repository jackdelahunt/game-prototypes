#include "libs/libs.h"
#include "game.h"

// Total: 17.5
// started: 14:30

#define MAX_ENTITIES 2000

#define PLAYER_SPEED 0.02
#define PLAYER_MAX_SPEED 1
#define PLAYER_ROTATION_SPEED 1.2

#define MISSLE_SPEED 2
#define MISSLE_DESPAWN_DISTANCE 1000

struct Entity {
    // meta
    u64 flags;

    // entity
    v3 position;
    v2 size;
    f32 rotation;
    v2 velocity;

    // rendering
    TextureHandle texture;
};

enum EntityFlags {
    EF_PLAYER   = 1 << 0,
    EF_ASTEROID = 1 << 1,
    EF_MISSLE   = 1 << 2,
    EF_DELETE   = 1 << 3,
};

struct State {
    Camera camera;
    Window window;
    Renderer renderer;

    Array<Entity, MAX_ENTITIES> entities;
    i64 score;
} state = {};

struct CollisionIterator {
    Entity* entity;
    i64 index;
};

void input();
void update_and_draw();
void physics();

void spawn_entity(Entity entity);

CollisionIterator new_collision_iterator(Entity *entity);
Entity *next(CollisionIterator *iterator);

int main() {
    state = {
        .camera = {
            .position = {0, 0, -1},
            .orthographic_size = 450,
            .near_plane = 0.1f,
            .far_plane = 100.0f,
        }
    };

    { // init engine stuff
        state.window = create_window(1440, 1080, "game6");
    
        bool ok = init_renderer(&state.renderer, state.window);
        if (!ok) {
            printf("failed to init the renderer");
            return -1;
        }

        ok = load_textures(&state.renderer);
        if (!ok) {
            printf("failed to load textures");
            return -1;
        }

        ok = load_font(&state.renderer, "resources/fonts/LibreBaskerville.ttf", 1000, 1000, 160);
        if (!ok) {
            printf("failed to load textures");
            return -1;
        }
    }

    { // init game stuff
        spawn_entity(Entity {
            .flags = EF_PLAYER,
            .size = {50, 50},
            .texture = TH_PLAYER,
        });

        spawn_entity(Entity {
            .flags = EF_ASTEROID,
            .position = {100, 100, 0},
            .size = {60, 60},
            .texture = TH_MISSLE,
        });
    }

    while (!glfwWindowShouldClose(state.window.glfw_window)) {
        input();

        if (KEYS[GLFW_KEY_ESCAPE] == InputState::down) {
            glfwSetWindowShouldClose(state.window.glfw_window, GLFW_TRUE);
        }

        new_frame(&state.renderer);

        update_and_draw();
        physics();

        draw_frame(&state.renderer, state.window, state.camera);
        glfwSwapBuffers(state.window.glfw_window);
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

void update_and_draw() {
    for (int i = 0; i < state.entities.len; i++) {
        Entity* entity = &state.entities[i];

        { // player
            if (entity->flags & EF_PLAYER) {
                if (KEYS[GLFW_KEY_W] == InputState::pressed) {
                    v2 direction = vector_from_angle(entity->rotation);

                    entity->velocity.X += direction.X * PLAYER_SPEED;
                    entity->velocity.Y += direction.Y * PLAYER_SPEED;

                    if (length(entity->velocity) > PLAYER_MAX_SPEED) {
                        entity->velocity = norm(entity->velocity);

                        entity->velocity.X += direction.X * PLAYER_MAX_SPEED;
                        entity->velocity.Y += direction.Y * PLAYER_MAX_SPEED;
                    }
                }
    
                if (KEYS[GLFW_KEY_A] == InputState::pressed) {
                    entity->rotation -= PLAYER_ROTATION_SPEED;
                }

                if (KEYS[GLFW_KEY_D] == InputState::pressed) {
                    entity->rotation += PLAYER_ROTATION_SPEED;
                }

                if (KEYS[GLFW_KEY_SPACE] == InputState::down) {
                    v2 direction = vector_from_angle(entity->rotation);

                    spawn_entity(Entity {
                        .flags = EF_MISSLE,
                        .position = entity->position,
                        .size = {10, 10},
                        .velocity = direction * MISSLE_SPEED,
                        .texture = TH_MISSLE,
                    });
                }
            }
        }

        { // asteroid
            if (entity->flags & EF_ASTEROID) {
                entity->rotation += 0.15;

                CollisionIterator iter = new_collision_iterator(entity);
                while (true) {
                    Entity *other = next(&iter);
                    if (other == nullptr) {
                        break;
                    }

                    if (other->flags & EF_MISSLE) {
                        entity->flags |= EF_DELETE;
                        other->flags |= EF_DELETE;

                        state.score += 1;
                    }
                }
            }
        }

        { // missle
            if (entity->flags & EF_MISSLE) {
                if (length(entity->position.XY) >= MISSLE_DESPAWN_DISTANCE) {
                    entity->flags |= EF_DELETE;
                }
            }
        }

        draw_texture(&state.renderer, entity->texture, entity->position, entity->size, entity->rotation, WHITE);
    }

    { // score
        u8 buffer[100];
        i64 length = sprintf((char *) buffer, "score: %lld", state.score);

        string text = make_slice(buffer, length);

        draw_text(&state.renderer, text, {-580, 420, 0}, 20, WHITE);
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

void physics() {
    for (int i = 0; i < state.entities.len; i++) {
        Entity* entity = &state.entities[i];

        entity->position.X += entity->velocity.X;
        entity->position.Y += entity->velocity.Y;
    }
}

void spawn_entity(Entity entity) {
    append(&state.entities, entity);
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

/*
 aabb :: proc(position_a: v2, size_a: v2, position_b: v2, size_b: v2) -> (v2, v2, bool) {
    distance := position_b - position_a
    distance_abs := v2{abs(distance.x), abs(distance.y)}
    distance_for_collision := (size_a + size_b) * v2{0.5, 0.5}

    collision := distance_for_collision[0] >= distance_abs[0] && distance_for_collision[1] >= distance_abs[1]
    overlap_amount := distance_for_collision - distance_abs

    return overlap_amount, distance, collision
}
*/
