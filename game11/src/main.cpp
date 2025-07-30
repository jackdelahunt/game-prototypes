#include "libs/libs.h"
#include "ack.cpp"
#include "math.cpp"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

// Total: 5:00
// Started: 13:00

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

struct Server {
    bool running;
    ISteamNetworkingSockets *interface;
    SteamNetworkingMicroseconds start_time;
    HSteamListenSocket socket;
    HSteamNetPollGroup poll_group;
};

struct Client {
    bool running;
    ISteamNetworkingSockets *interface;
    SteamNetworkingMicroseconds start_time;
    HSteamNetConnection connection;
};

struct State {
    f64 time;

    Server server;
    Client client;

    StackArray<Entity, MAX_ENTITIES> entities;

} state = {};

void update_entities(f32 delta_time);
void draw_entities(f32 delta_time);
void physics(f32 delta_time);

Entity *spawn_entity(Entity entity);

void networking_debug_callback(ESteamNetworkingSocketsDebugOutputType type, const char *message);
void server_network_connection_status_changed_callback(SteamNetConnectionStatusChangedCallback_t *info);
void client_network_connection_status_changed_callback(SteamNetConnectionStatusChangedCallback_t *info);

int main() {
    state = State {};

    const f64 MAX_TIME = 15.0f;

#ifdef SERVER
    { // init networking
	printf("Running as a server\n");

        SteamDatagramErrMsg errMsg;
	if (!GameNetworkingSockets_Init(nullptr, errMsg)) {
	    printf("GameNetworkingSockets_Init failed.  %s", errMsg);
            return 1;
        }
    
        state.server.start_time = SteamNetworkingUtils()->GetLocalTimestamp();
        SteamNetworkingUtils()->SetDebugOutputFunction(k_ESteamNetworkingSocketsDebugOutputType_Msg, networking_debug_callback);

        // init server 
        u16 port = 27020;
        state.server.interface = SteamNetworkingSockets();

        SteamNetworkingIPAddr address; 
	address.Clear();
	address.m_port = port;

        SteamNetworkingConfigValue_t opt;
	opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void*)server_network_connection_status_changed_callback);

	state.server.socket = state.server.interface->CreateListenSocketIP(address, 1, &opt);
	if (state.server.socket == k_HSteamListenSocket_Invalid) {
	    printf("Error creating socket, failed to listen on port %d", port);
        }
	
        state.server.poll_group = state.server.interface->CreatePollGroup();
	if (state.server.poll_group == k_HSteamNetPollGroup_Invalid) {
	    printf("Error creating poll group, failed to listen on port %d", port);
        }
	
        printf("Server listening on port %d\n", port);

        f64 time_seconds = 0;

        while (time_seconds < MAX_TIME) {
            time_seconds = f64(SteamNetworkingUtils()->GetLocalTimestamp() - state.server.start_time) / 1000000;
            state.server.interface->RunCallbacks();
        }

        printf("Closing server reached time limit\n");
    }
#endif

#ifdef CLIENT
    { // init networking

	printf("Running as a client\n");

        SteamDatagramErrMsg errMsg;
	if (!GameNetworkingSockets_Init(nullptr, errMsg)) {
	    printf("GameNetworkingSockets_Init failed.  %s", errMsg);
            return 1;
        }
    
        state.client.running = true;
        state.client.start_time = SteamNetworkingUtils()->GetLocalTimestamp();
        SteamNetworkingUtils()->SetDebugOutputFunction(k_ESteamNetworkingSocketsDebugOutputType_Msg, networking_debug_callback);
        state.client.interface = SteamNetworkingSockets();

        // init client 
        u16 port = 27020;

        SteamNetworkingIPAddr address; 
	address.Clear();

        ASSERT(address.ParseString("::1"));
	address.m_port = port;

        char address_buffer[ SteamNetworkingIPAddr::k_cchMaxString ];
	address.ToString(address_buffer, sizeof(address_buffer), true);
	printf("Connecting to server at %s\n", address_buffer);

        SteamNetworkingConfigValue_t opt;
	opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void*)client_network_connection_status_changed_callback);

        state.client.connection = state.client.interface->ConnectByIPAddress(address, 1, &opt);
	if (state.client.connection == k_HSteamNetConnection_Invalid ) {
	    printf("Failed to create connection to server\n");
            BREAKPOINT;
        }

        f64 time_seconds = 0;

        while (time_seconds < MAX_TIME && state.client.running) {
            time_seconds = f64(SteamNetworkingUtils()->GetLocalTimestamp() - state.client.start_time) / 1000000;
            state.client.interface->RunCallbacks();
        }

        printf("Closing client reached time limit\n");
    }
#endif

#if 0
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
#endif

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

void networking_debug_callback(ESteamNetworkingSocketsDebugOutputType type, const char *message) {
    printf("[NETWORK]: %s\n", message);

    if (type == k_ESteamNetworkingSocketsDebugOutputType_Bug) {
        printf("[NETWORK]: fatal error\n");
        BREAKPOINT;
    }
}

void server_network_connection_status_changed_callback(SteamNetConnectionStatusChangedCallback_t *info) {
    switch (info->m_info.m_eState) {
	case k_ESteamNetworkingConnectionState_None:
            break;
	case k_ESteamNetworkingConnectionState_ClosedByPeer:
            break;
	case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
            break;
	case k_ESteamNetworkingConnectionState_Connecting: {
	    printf("Connection request from %s\n", info->m_info.m_szConnectionDescription);
            state.server.interface->CloseConnection(info->m_hConn, 0, nullptr, false);
	    printf("Forced closed connection\n");
        } break;
	case k_ESteamNetworkingConnectionState_Connected:
            break;
        default: break;
    }
}

void client_network_connection_status_changed_callback(SteamNetConnectionStatusChangedCallback_t *info) {
    ASSERT(info->m_hConn == state.client.connection || state.client.connection == k_HSteamNetConnection_Invalid );

    switch (info->m_info.m_eState) {
	case k_ESteamNetworkingConnectionState_None:
            break;
	case k_ESteamNetworkingConnectionState_ClosedByPeer:
            break;
	case k_ESteamNetworkingConnectionState_ProblemDetectedLocally: {
            state.client.running = false;

	    if (info->m_eOldState == k_ESteamNetworkingConnectionState_Connecting ) {
		printf("Tried to connect but failed: %s\n", info->m_info.m_szEndDebug );
	    }
	    else if (info->m_info.m_eState == k_ESteamNetworkingConnectionState_ProblemDetectedLocally ) {
		printf("Lost contact with the host: %s\n", info->m_info.m_szEndDebug );
	    }
	    else {
		printf("Disconnected from server %s\n", info->m_info.m_szEndDebug);
	    }

	    // Clean up the connection.  This is important!
	    // The connection is "closed" in the network sense, but
	    // it has not been destroyed.  We must close it on our end, too
	    // to finish up.  The reason information do not matter in this case,
	    // and we cannot linger because it's already closed on the other end,
	    // so we just pass 0's.
	    state.client.interface->CloseConnection(info->m_hConn, 0, nullptr, false );
	    state.client.connection = k_HSteamNetConnection_Invalid;
        } break;
	case k_ESteamNetworkingConnectionState_Connecting: {
        } break;
	case k_ESteamNetworkingConnectionState_Connected: {
	    printf("Client connected to server\n");
        } break;
        default: break;
    }
}
