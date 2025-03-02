#include <stdio.h>

#include "libs/libs.h"
#include "game.h"

// Total: 14
// started: 15:30

#define MAX_ENTITIES 2000
#define PLAYER_SPEED 0.02
#define PLAYER_MAX_SPEED 1
#define PLAYER_ROTATION_SPEED 1.2

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
    EF_NONE     = 0,
    EF_PLAYER   = 1 << 0,
};

struct State {
    Camera camera;
    Window window;
    Renderer renderer;

    Array<Entity, MAX_ENTITIES> entities;
} state = {};

void update_and_draw();
void physics();

int main() {
    state = {
        .camera = {
            .position = {0, 0, -1},
            .orthographic_size = 250,
            .near_plane = 0.1f,
            .far_plane = 100.0f,
        }
    };

    { // init engine stuff
        state.window = create_window(1080, 720, "game6");
    
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
    }

    { // init game stuff
        Entity player = {
            .flags = EF_PLAYER,
            .size = {50, 50},
            .texture = TH_PLAYER,
        };

        append(&state.entities, player);
    }

    while (!glfwWindowShouldClose(state.window.glfw_window)) {
        glfwPollEvents();

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

v2 vector_from_angle(f32 angle) {
    f32 angle_radians = angle * HMM_DegToRad;

    return v2 {
        .X = HMM_SINF(angle_radians),
        .Y = HMM_COSF(angle_radians),
    };
}

void update_and_draw() {
    for (int i = 0; i < state.entities.len; i++) {
        Entity* entity = &state.entities[i];

        { // player
            if (entity->flags & EF_PLAYER) {
                if (KEYS[GLFW_KEY_W] == InputState::down) {
                    v2 direction = vector_from_angle(entity->rotation);

                    entity->velocity.X += direction.X * PLAYER_SPEED;
                    entity->velocity.Y += direction.Y * PLAYER_SPEED;

                    if (HMM_LenV2(entity->velocity) > PLAYER_MAX_SPEED) {
                        entity->velocity = HMM_NormV2(entity->velocity);

                        entity->velocity.X += direction.X * PLAYER_MAX_SPEED;
                        entity->velocity.Y += direction.Y * PLAYER_MAX_SPEED;
                    }
                }
    
                if (KEYS[GLFW_KEY_A] == InputState::down) {
                    entity->rotation -= PLAYER_ROTATION_SPEED;
                }

                if (KEYS[GLFW_KEY_D] == InputState::down) {
                    entity->rotation += PLAYER_ROTATION_SPEED;
                }
            }
        }

        draw_texture(&state.renderer, entity->texture, entity->position, entity->size, entity->rotation, WHITE);
    }
}

void physics() {
    for (int i = 0; i < state.entities.len; i++) {
        Entity* entity = &state.entities[i];

        entity->position.X += entity->velocity.X;
        entity->position.Y += entity->velocity.Y;
    }
}
