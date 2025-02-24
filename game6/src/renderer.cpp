#ifndef RENDERER_CPP
#define RENDERER_CPP

#include <stddef.h>

#include "common.cpp"
#include "libs/libs.h"
#include "game.h"

#define MAX_QUADS 100

struct Vertex {
    v3 position;
    v4 colour;
    v2 uv;
};

struct Quad {
    Vertex vertices[4];
};

struct Camera {
    v3 position;
    f32 orthographic_size;
    f32 near_plane;
    f32 far_plane;
};

enum TextureHandle {
    TH_ALIEN,
    TH_BLUE_FACE,
    TH_COUNT__
};

struct Texture {
    TextureHandle handle;
    i64 width;
    i64 height;
    v2 uv[4];
    u8 *data;
};

struct Atlas {
    i64 width;
    i64 height;
    u8 *data;
};

struct DrawCommand {
    v3 position;
    v2 size;
    TextureHandle texture_handle;
};

struct Renderer {
    DrawCommand commands[MAX_QUADS];
    i64 command_count;

    Quad quads[MAX_QUADS];
    i64 quad_count;

    Array<Texture, TH_COUNT__> textures;
    Atlas atlas;

    u32 vertex_array_id;
    u32 vertex_buffer_id;
    u32 index_buffer_id;
    u32 shader_program_id;

    u32 atlas_texture_id;
};

v4 RED      = {1, 0, 0, 1};
v4 GREEN    = {0, 1, 0, 1};
v4 BLUE     = {0, 0, 1, 1};

bool init_renderer(Renderer *renderer, Window window);
bool load_textures(Renderer *renderer);
u32 send_bitmap_to_gpu(Renderer *renderer, i32 width, i32 height, u8 *data);
void draw_quad(Renderer *renderer, v3 position, v2 size);
void new_frame(Renderer *renderer);
void draw_frame(Renderer *renderer, Window window, Camera camera);
m4 get_view_matrix(Camera camera);
m4 get_projection_matrix(Camera camera, f32 aspect);
const char *texture_path(TextureHandle handle);

bool init_renderer(Renderer *renderer, Window window) {
    { // init opengl
        GLenum result = glewInit();
        if (result != GLEW_OK) {
            return false;
        }
    
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
        float f = 0.8f;
        glClearColor(f, f, f, 1.0f);
    }

    { // init imgui
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
    
        ImGui::StyleColorsDark();
    
        ImGuiIO& io = ImGui::GetIO();
    
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    
        ImGui_ImplGlfw_InitForOpenGL(window.glfw_window, true);
        ImGui_ImplOpenGL3_Init("#version 460");
    }

    { // load and compile shaders
        const i64 buffer_size = 640;
        i32 compile_status = 0;
        i32 link_status = 0;
        char error_buffer[buffer_size];
    
        Slice<u8> vertex_shader_source = read_file("./resources/shaders/vertex.shader");
        if (vertex_shader_source.len == 0) {
            printf("failed to load vertex shader");
            return false;
        }

        Slice<u8> fragment_shader_source = read_file("./resources/shaders/fragment.shader");
        if (fragment_shader_source.len == 0) {
            printf("failed to load fragment shader");
            return false;
        }

        u32 vertex_shader = glCreateShader(GL_VERTEX_SHADER);

        glShaderSource(vertex_shader, 1, (char **) &vertex_shader_source.ptr, NULL);
        glCompileShader(vertex_shader);

        glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &compile_status);
        if (compile_status == 0) {
            glGetShaderInfoLog(vertex_shader, buffer_size, nullptr, &error_buffer[0]);
            printf("failed to compile vertex shader: %s", error_buffer);
            return false;
        }

        u32 fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

        glShaderSource(fragment_shader, 1, (char**) &fragment_shader_source.ptr, NULL);
        glCompileShader(fragment_shader);

        glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &compile_status);
        if (compile_status == 0) {
            glGetShaderInfoLog(fragment_shader, buffer_size, nullptr, &error_buffer[0]);
            printf("failed to compile fragment shader: %s", error_buffer);
            return false;
        }

        u32 shader_program = glCreateProgram();
        glAttachShader(shader_program, vertex_shader);
        glAttachShader(shader_program, fragment_shader);
        glLinkProgram(shader_program);

        glGetProgramiv(shader_program, GL_LINK_STATUS, &link_status);
        if (link_status == 0) {
            glGetProgramInfoLog(shader_program, buffer_size, nullptr, &error_buffer[0]);
            printf("failed to link shader program: %s", error_buffer);
            return false;
        }

        renderer->shader_program_id = shader_program;

        glUseProgram(shader_program);
        glUniform1i(glGetUniformLocation(shader_program, "atlas_texture"), 0);

        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
    }

    { // vertex array
        u32 vertex_array;
        glGenVertexArrays(1, &vertex_array);
        glBindVertexArray(vertex_array);

        renderer->vertex_array_id = vertex_array;
    }

    { // vertex buffer
        u32 vertex_buffer;
        glGenBuffers(1, &vertex_buffer);
        glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(Quad) * MAX_QUADS, renderer->quads, GL_DYNAMIC_DRAW);

        renderer->vertex_buffer_id = vertex_buffer;
    }

    { // index buffer
        const i64 index_buffer_length = MAX_QUADS * 6;
        u32 indices[index_buffer_length];

        i64 i = 0;
        while (i < index_buffer_length) {
            // vertex offset pattern to draw a quad
            // { 0, 1, 2,  0, 2, 3 }
            indices[i + 0] = ((i/6)*4 + 0);
            indices[i + 1] = ((i/6)*4 + 1);
            indices[i + 2] = ((i/6)*4 + 2);
            indices[i + 3] = ((i/6)*4 + 0);
            indices[i + 4] = ((i/6)*4 + 2);
            indices[i + 5] = ((i/6)*4 + 3);
            i += 6;
        }

        u32 index_buffer;
        glGenBuffers(1, &index_buffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(u32) * index_buffer_length, indices, GL_STATIC_DRAW);

        renderer->index_buffer_id = index_buffer;
    }

    { // vertex attributes
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *) offsetof(Vertex, position));   // position
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *) offsetof(Vertex, colour));     // colour
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *) offsetof(Vertex, uv));         // uv

        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glEnableVertexAttribArray(2);
    }

    return true;
}

bool load_textures(Renderer *renderer) {

    stbi_set_flip_vertically_on_load(true);

    for(i64 i = 0; i < renderer->textures.size; i++) {
        TextureHandle handle = (TextureHandle) i;

        const char *path = texture_path(handle);

        i32 width       = 0;
        i32 height      = 0;
        i32 channels    = 0;
        u8 *image_data  = nullptr;
    
        image_data = stbi_load(path, &width, &height, &channels, 4);
        if (!image_data) {
            printf("Failed to load texture: %s\n", path);
            return false;
        }

        renderer->textures[i] = {
            .handle = handle,
            .width = width,
            .height = height,
            .data = image_data,   
        };
    }

    const i64 ATLAS_WIDTH     = 256;
    const i64 ATLAS_HEIGHT    = 256;
    const i64 BYTES_PER_PIXEL = 4;
    const i64 CHANNELS        = 4;
    const i64 ATLAS_BYTE_SIZE = ATLAS_WIDTH * ATLAS_HEIGHT * BYTES_PER_PIXEL;

    u8 *atlas_data = (u8 *) malloc(ATLAS_BYTE_SIZE);

    { // fill in atlas default data
        i64 i = 0;
        while (i < ATLAS_BYTE_SIZE) {
            atlas_data[i]       = 255;  // r
            atlas_data[i + 1]   = 0;    // g
            atlas_data[i + 2]   = 255;  // b
            atlas_data[i + 3]   = 255;  // a
     
            i += 4;
        }
    }

    { // copy textures into atlas with rect pack and send to gpu
        const i64 RECT_COUNT = TH_COUNT__;

        stbrp_context rp_context;
        stbrp_node nodes[ATLAS_WIDTH];
        stbrp_rect rects[RECT_COUNT];

        stbrp_init_target(&rp_context, ATLAS_WIDTH, ATLAS_HEIGHT, nodes, ATLAS_WIDTH);
        for(i64 i = 0; i < renderer->textures.size; i++) {
            TextureHandle texture_handle = (TextureHandle) i; 
            Texture *texture = &renderer->textures[texture_handle];

            rects[i] = stbrp_rect {
                .id = texture_handle,
                .w = (i32) texture->width,
                .h = (i32) texture->height,
            };
        }

        i64 status = stbrp_pack_rects(&rp_context, rects, RECT_COUNT);
        if (status == 0) {
            printf("error packing textures into atlas\n");
            return false;
        }

        for(int i = 0; i < RECT_COUNT; i++) {
            stbrp_rect *rect = &rects[i];
            Texture *texture = &renderer->textures[rect->id];

            f32 bottom_y_uv = (f32) rect->y             / (f32) ATLAS_HEIGHT;
            f32 top_y_uv    = (f32) (rect->y + rect->h) / (f32) ATLAS_HEIGHT;
            f32 left_x_uv   = (f32) rect->x             / (f32) ATLAS_WIDTH;
            f32 right_x_uv  = (f32) (rect->x + rect->w) / (f32) ATLAS_WIDTH;

            texture->uv[0] = {left_x_uv, top_y_uv};
            texture->uv[1] = {right_x_uv, top_y_uv};
            texture->uv[2] = {right_x_uv, bottom_y_uv};
            texture->uv[3] = {left_x_uv, bottom_y_uv};

            for (i64 row = 0; row < rect->h; row++) {
                u8 *source_row = texture->data + (row * rect->w * BYTES_PER_PIXEL);
                u8 *dest_row = atlas_data + (((rect->y + row) * ATLAS_WIDTH + rect->x) * BYTES_PER_PIXEL);
                memcpy(dest_row, source_row, rect->w * BYTES_PER_PIXEL);
            }
        }

        u32 texture_id = send_bitmap_to_gpu(renderer, ATLAS_WIDTH, ATLAS_HEIGHT, atlas_data);
        assert(texture_id != 0);

        renderer->atlas_texture_id = texture_id;
    }

    { // write atlas to build folder
        stbi_flip_vertically_on_write(true);
        i64 status = stbi_write_png("build/atlas.png", ATLAS_WIDTH, ATLAS_HEIGHT, 4, atlas_data, ATLAS_WIDTH * BYTES_PER_PIXEL);
        if (status == 0) {
            printf("error writing atlas to build folder\n");
            return false;
        }
    }

    return true;
}

u32 send_bitmap_to_gpu(Renderer *renderer, i32 width, i32 height, u8 *data) {
    u32 texture_id = 0;
    glGenTextures(1, &texture_id);

    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // border param might fix texture bleeding
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

    return texture_id;
}

void draw_quad(Renderer *renderer, v3 position, v2 size) {
    DrawCommand *command = &renderer->commands[renderer->command_count];
    renderer->command_count++;

    command->position = position;
    command->size = size;
}

void new_frame(Renderer *renderer) {
    renderer->quad_count = 0;
    renderer->command_count = 0;

    { // new frame for imgui
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame(); 
    }

    glClear(GL_COLOR_BUFFER_BIT);
}

void draw_frame(Renderer *renderer, Window window, Camera camera) {
    { // update quad buffer based on draw commands
        const v4 top_left      = {-0.5,   0.5, 0, 1};
        const v4 top_right     = { 0.5,   0.5, 0, 1};
        const v4 bottom_right  = { 0.5,  -0.5, 0, 1};
        const v4 bottom_left   = {-0.5,  -0.5, 0, 1};

        m4 view_projection = HMM_MulM4(get_projection_matrix(camera, (f32) window.width / (f32) window.height), get_view_matrix(camera));
    
        renderer->quad_count = renderer->command_count;
        
        for (i64 i = 0; i < renderer->command_count; i++) {
            DrawCommand *command    = &renderer->commands[i];
            Quad *quad              = &renderer->quads[i];
    
            m4 model_matrix = HMM_M4D(1.0f);
            model_matrix = HMM_MulM4(model_matrix, HMM_Translate(command->position));
            model_matrix = HMM_MulM4(model_matrix, HMM_Scale({command->size.X, command->size.Y, 1}));
        
            m4 mvp_matrix = HMM_MulM4(view_projection, model_matrix);
       
            quad->vertices[0].position = HMM_MulM4V4(mvp_matrix, top_left).XYZ;
            quad->vertices[1].position = HMM_MulM4V4(mvp_matrix, top_right).XYZ;
            quad->vertices[2].position = HMM_MulM4V4(mvp_matrix, bottom_right).XYZ;
            quad->vertices[3].position = HMM_MulM4V4(mvp_matrix, bottom_left).XYZ;
        
            quad->vertices[0].colour = RED;
            quad->vertices[1].colour = GREEN;
            quad->vertices[2].colour = BLUE;
            quad->vertices[3].colour = RED;

            #if 0
            quad->vertices[0].uv = {0, 1};
            quad->vertices[1].uv = {1, 1};
            quad->vertices[2].uv = {1, 0};
            quad->vertices[3].uv = {0, 0};
            #endif

            Texture *texture = &renderer->textures[command->texture_handle];

            quad->vertices[0].uv = texture->uv[0];
            quad->vertices[1].uv = texture->uv[1];
            quad->vertices[2].uv = texture->uv[2];
            quad->vertices[3].uv = texture->uv[3];
        }
    }

    { // draw the things we have sent to the renderer
        glViewport(0, 0, window.width, window.height);

        // update quad buffer on gou and bind for shader program
        glBindBuffer(GL_ARRAY_BUFFER, renderer->vertex_buffer_id);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(Quad) * renderer->quad_count, renderer->quads);
        glBindVertexArray(renderer->vertex_array_id);

        glUseProgram(renderer->shader_program_id);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, renderer->atlas_texture_id);

        glDrawElements(GL_TRIANGLES, 6 * renderer->quad_count, GL_UNSIGNED_INT, 0);
    }

    { // imgui rendering
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        GLFWwindow *current = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(current);
    }
}

m4 get_view_matrix(Camera camera) {
    return HMM_LookAt_LH(
        camera.position, 
        {camera.position.X, camera.position.Y, camera.position.Z + 1}, 
        {0, 1, 0}
    );
}

m4 get_projection_matrix(Camera camera, f32 aspect) {
    return HMM_Orthographic_LH_NO(
        -camera.orthographic_size * aspect,  // left
         camera.orthographic_size * aspect,  // right
        -camera.orthographic_size,           // bottom
         camera.orthographic_size,           // top
         camera.near_plane, 
         camera.far_plane 
    );
}

const char *texture_path(TextureHandle handle) {
    switch (handle) {
        case TH_ALIEN: 
            return "resources/textures/alien.png";
            break;
        case TH_BLUE_FACE: 
            return "resources/textures/blue_face.png";
            break;
        case TH_COUNT__: assert(0);
            break;
    }

    return nullptr;
}

#endif
