#include "libs/libs.h"
#include "ack.cpp"
#include "libs/raylib/include/raylib.h"
#include "math.cpp"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

// Total: 0:00
// Started: 15:00

#define MAX_ENTITIES 2000

struct Entity {
    // meta
    u64 flags;

    // base
    v3 position;
    v3 size;
    v3 rotation;
    v3 velocity;

    // rendering
    v4 colour;
};

enum EntityFlags {
    EF_PLAYER           = 1 << 0,
    EF_DELETE           = 1 << 16,
};

struct State {
    f64 time;

    StackArray<Entity, MAX_ENTITIES> entities;

} state = {};

void update_entities(f32 delta_time);
void draw_entities(f32 delta_time);
void physics(f32 delta_time);

Entity *spawn_entity(Entity entity);

int main() {
    state = State {
    };

    { // init networking
        SteamNetworkingIPAddr server_address;
        server_address.Clear();

        SteamDatagramErrMsg errMsg;
		if (!GameNetworkingSockets_Init(nullptr, errMsg)) {
			printf("GameNetworkingSockets_Init failed.  %s", errMsg);
            return 1;
        }
    }


    srand(time(NULL));

    InitWindow(1080, 720, "Game11");

    while (!WindowShouldClose()) {
        f32 delta_time = 0;
        update_entities(delta_time);
        physics(delta_time);
        draw_entities(delta_time);

        BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawText("Congrats! You created your first window!", 190, 200, 20, LIGHTGRAY);
        EndDrawing();
    }

    CloseWindow();  

    return 0;
}

void update_entities(f32 delta_time) {
    for (Entity &entity : state.entities) {
    }
}

void draw_entities(f32 delta_time) {
    for (Entity &entity : state.entities) {
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

Entity *spawn_entity(Entity entity) {
    Entity *ptr = push(&state.entities);
    *ptr = entity;

    return ptr;
}
