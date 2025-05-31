#include "libs/libs.h"
#include "ack.cpp"
#include "engine.cpp"

#include <cmath>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

// cursed c++ headers to get saving working
#include <vector>
#include <fstream>
#include <string>

// Total: 09:00
// Started: 13:00

// Performance (20 * 20 * 20):
// start                            == ~21 fps  - 48,000 quads
// no block unless touching air     == ~78 fps  - 13,008 quads
// no face unless touching air      == ~350 fps - 2,400 quads

#define ALLOW_EDITOR 1
#define MAX_ENTITIES 2000
#define DEFAULT_SAVE_FILE "resources/saves/scene.json"

enum SpriteHandle {
    SH_NONE,
    SH_BRICK,
    SH_COUNT_
};

Sprite *sprites[SH_COUNT_];

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
    EF_PLAYER           = 1 << 0,
    EF_LIGHT            = 1 << 1,
    EF_ANIMATED_SPRITE  = 1 << 2,
    EF_DELETE           = 1 << 16,
};

#define CHUNK_WIDTH 100
#define CHUNK_HEIGHT 100
#define CHUNK_DEPTH 100

enum class BlockType {
    AIR,
    BRICK
};

struct Chunk {
    Slice<BlockType> blocks;
};

struct State {
    Camera camera;
    Window window;
    Renderer renderer;
    SoundEngine sound_engine;
    Editor editor;

    i64 quads_last_frame;

    Chunk chunk;

    struct {
        f32 cutoff;
        f32 frequency;
    } noise;

    f64 time;

    StackArray<Entity, MAX_ENTITIES> entities;
} state = {};

struct CollisionIterator {
    Entity* entity;
    i64 index;
};

void update_and_draw(f32 delta_time);
void physics(f32 delta_time);
void draw_editor(f32 delta_time);

Chunk new_chunk();
void draw_chunk(Chunk *chunk);
void generate_blocks(Chunk *chunk);
v3 block_index_to_chunk_position(i64 index);
i64 chunk_position_to_block_index(i64 x, i64 y, i64 z);
BlockType get_block_neighbour(Chunk *chunk, i64 x, i64 y, i64 z, i64 x_offset, i64 y_offset, i64 z_offset);

Entity *spawn_entity(Entity entity);

void to_json(json& j, const Entity& entity);
void from_json(const json& j, Entity& entity);

void backup_scene();
void save_scene(State *state);
void load_scene(State *state);

CollisionIterator new_collision_iterator(Entity *entity);
Entity *next(CollisionIterator *iterator);

Sprite *get_sprite(SpriteHandle handle);

int main() {
    state = State {
        .camera = {
            .fov = 90,
            .position = {0, 0, 0},
            .rotation = {0, 0, 0},
            .orthographic_size = 5,
            .near_plane = 0.1f,
            .far_plane = 1000.0f,
        },
        .renderer = {
            .global_light = {1, 1, 1, 1},
            .clear_colour = CORNFLOUR_BLUE,
        },
        .editor = {
            .snap_to_grid = true,
            .grid_size = {50, 50},
            .selection_range = {0, 20},
        },
        .chunk = new_chunk(),
        .noise = {
            .cutoff = 0.7,
            .frequency = 0.05
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

            sprite = load_sprite(&state.renderer, "resources/textures/brick.png", "");
            if (sprite == NULL) {
                return 1;
            }

            sprites[SH_BRICK] = sprite;
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

    generate_blocks(&state.chunk);

    while (!glfwWindowShouldClose(state.window.glfw_window)) {
        f64 current_time    = state.time;
        f64 new_time        = glfwGetTime();
        f32 delta_time      = (f32) (new_time - current_time);
        state.time          = new_time;

        if (KEYS[GLFW_KEY_ESCAPE] == InputState::down) {
            glfwSetWindowShouldClose(state.window.glfw_window, GLFW_TRUE);
        }

        state.quads_last_frame = state.renderer.quads.len;
        new_frame(&state.renderer, &state.window, state.camera);

        poll_inputs(); 
        update_and_draw(delta_time);
        physics(delta_time); 

        draw_frame(&state.renderer, &state.window, state.camera); 

#if ALLOW_EDITOR
        draw_editor(delta_time); 
#endif

        swap_buffers(&state.window);
    }

    glfwTerminate();

    return 0;
}

void update_and_draw(f32 delta_time) {
    { // toggle mouse capture
        if (KEYS[GLFW_KEY_F1] == InputState::down) {
            set_mouse_captured(&state.window, !state.window.mouse_captured);
        }
    }

    { // update camera position
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
    
        const f32 move_speed = 10;
        v3 forward = get_forward_direction(state.camera);
        v3 right = get_right_direction(state.camera);
    
        if(input.y != 0) {
            state.camera.position += forward * (input.y * move_speed * delta_time);
        }
    
        if(input.x != 0) {
            state.camera.position += right * (input.x * move_speed * delta_time);
        }
    }

    if (state.window.mouse_captured) { // update camera rotation (looking at)
        f32 sensitivity = 0.3;

        v2 mouse_input = MOUSE.delta;

        if(length(mouse_input) != 0) {
            state.camera.rotation += v3{mouse_input.y, mouse_input.x, 0} * sensitivity;

            state.camera.rotation.x = clamp(-90, state.camera.rotation.x, 90);
        }
    }

    draw_chunk(&state.chunk);

#if ALLOW_EDITOR
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
#endif

#if 0
    for (int i = 0; i < state.entities.len; i++) {
        Entity* entity = &state.entities[i];

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

            entity->velocity = input * 250;
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
                draw_animated_sprite(&state.renderer, sprite, entity->animation_cycle, entity->position, entity->size, entity->rotation, draw_colour);
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
#endif
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

    ImGui::Text("Quads: %llu", state.quads_last_frame);
    ImGui::SameLine();
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

    if(ImGui::CollapsingHeader("Noise")) {
        ImGui::SliderFloat("Cutoff", &state.noise.cutoff, 0, 1);
        ImGui::SliderFloat("Frequency", &state.noise.frequency, 0, 0.4);

        if(ImGui::Button("Regenerate")) {
            generate_blocks(&state.chunk);
        }
    }

    if(ImGui::CollapsingHeader("Grid")) {
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

    if(ImGui::CollapsingHeader("Camera")) {
        ImGui::SliderFloat("FOV", &state.camera.fov, 1, 360);
        ImGui::SliderFloat3("Camera position", &state.camera.position[0], -50, 50);
        ImGui::SliderFloat3("Camera rotation", &state.camera.rotation[0], -360, 360);
        ImGui::SliderFloat("Orthographic size", &state.camera.orthographic_size, 10, 2000);
        ImGui::InputFloat4("Global light", &state.renderer.global_light[0]);
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

Chunk new_chunk() {
    return Chunk {
        .blocks = mem_alloc<BlockType>(CHUNK_WIDTH * CHUNK_HEIGHT * CHUNK_DEPTH)
    };
}

void draw_chunk(Chunk *chunk) {
    Sprite *sprite = get_sprite(SH_BRICK);

    for(i64 i = 0; i < chunk->blocks.len; i++) {
        if(chunk->blocks[i] == BlockType::AIR) {
            continue;
        }

        v3 position = block_index_to_chunk_position(i);

        v4 colour = {position.x / (f32) CHUNK_WIDTH, position.y / (f32) CHUNK_HEIGHT, position.z / (f32) CHUNK_DEPTH, 1};

        BlockType up = get_block_neighbour(chunk, (i64) position.x, (i64) position.y, (i64) position.z, 0, 1, 0);
        BlockType down = get_block_neighbour(chunk, (i64) position.x, (i64) position.y, (i64) position.z, 0, -1, 0);
        BlockType left = get_block_neighbour(chunk, (i64) position.x, (i64) position.y, (i64) position.z, -1, 0, 0);
        BlockType right = get_block_neighbour(chunk, (i64) position.x, (i64) position.y, (i64) position.z, 1, 0, 0);
        BlockType front = get_block_neighbour(chunk, (i64) position.x, (i64) position.y, (i64) position.z, 0, 0, -1);
        BlockType back = get_block_neighbour(chunk, (i64) position.x, (i64) position.y, (i64) position.z, 0, 0, 1);

        const v2 CUBE_SIZE = {1, 1};
    

        if (up == BlockType::AIR) {
            push_quad(&state.renderer, position + v3{0, 0.5, 0}, CUBE_SIZE, {-90, 0, 0}, colour, sprite->albedo->uvs, state.renderer.default_normal->uvs, DrawType::TEXTURE); // top
        }

        if (down == BlockType::AIR) {
            push_quad(&state.renderer, position + v3{0, -0.5, 0}, CUBE_SIZE, {90, 0, 0}, colour, sprite->albedo->uvs, state.renderer.default_normal->uvs, DrawType::TEXTURE); // bottom
        }

        if (left == BlockType::AIR) {
            push_quad(&state.renderer, position + v3{-0.5,    0,    0}, CUBE_SIZE, {  0, -90, 0}, colour, sprite->albedo->uvs, state.renderer.default_normal->uvs, DrawType::TEXTURE); // left
        }


        if (right == BlockType::AIR) {
            push_quad(&state.renderer, position + v3{ 0.5,    0,    0}, CUBE_SIZE, {  0,  90, 0}, colour, sprite->albedo->uvs, state.renderer.default_normal->uvs, DrawType::TEXTURE); // right
        }


        if (front == BlockType::AIR) {
            push_quad(&state.renderer, position + v3{   0,    0, -0.5}, CUBE_SIZE, {  0,   0, 0}, colour, sprite->albedo->uvs, state.renderer.default_normal->uvs, DrawType::TEXTURE); // front
        }


        if (back == BlockType::AIR) {
            push_quad(&state.renderer, position + v3{   0,    0,  0.5}, CUBE_SIZE, {  0, 180, 0}, colour, sprite->albedo->uvs, state.renderer.default_normal->uvs, DrawType::TEXTURE); // back
        }
    }
}

void generate_blocks(Chunk *chunk) {
    memset(chunk->blocks.ptr, 0, sizeof(BlockType) * chunk->blocks.len);

    FastNoiseLite noise;
    noise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    noise.SetFrequency(state.noise.frequency);

    i64 index = 0;

    for (i64 z = 0; z < CHUNK_DEPTH; z++) {
        for (i64 y = 0; y < CHUNK_HEIGHT; y++) {
            for (i64 x = 0; x < CHUNK_WIDTH; x++) {
                f32 n = noise.GetNoise((f32) x, (f32) y, (f32) z);
                    
                if (n > state.noise.cutoff) {
                    chunk->blocks[index] = BlockType::BRICK;
                }

                index++;
            }
        }
    }
}

v3 block_index_to_chunk_position(i64 index) {
    i64 x = index % CHUNK_WIDTH;
    i64 y = (index / CHUNK_DEPTH) % CHUNK_HEIGHT;
    i64 z = index / (CHUNK_HEIGHT * CHUNK_DEPTH);

    return {(f32) x, (f32) y, (f32) z};
}

i64 chunk_position_to_block_index(i64 x, i64 y, i64 z) {
    return x + (CHUNK_WIDTH * y) + (CHUNK_WIDTH * CHUNK_HEIGHT * z);
}

BlockType get_block_neighbour(Chunk *chunk, i64 x, i64 y, i64 z, i64 x_offset, i64 y_offset, i64 z_offset) {
    i64 neighbour_x = x + x_offset;
    i64 neighbour_y = y + y_offset;
    i64 neighbour_z = z + z_offset;

    if(neighbour_x < 0 || neighbour_y < 0 || neighbour_z < 0) {
        return BlockType::AIR;
    }

    if(neighbour_x >= CHUNK_WIDTH || neighbour_y >= CHUNK_HEIGHT || neighbour_z >= CHUNK_DEPTH) {
        return BlockType::AIR;
    }

    i64 neighbour_index = chunk_position_to_block_index(neighbour_x, neighbour_y, neighbour_z);
    return chunk->blocks[neighbour_index];
}

Entity *spawn_entity(Entity entity) {
    Entity *ptr = push(&state.entities);
    *ptr = entity;

    return ptr;
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
