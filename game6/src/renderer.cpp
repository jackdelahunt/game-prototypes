#ifndef RENDERER_CPP
#define RENDERER_CPP

#include "libs/libs.h"
#include "game.h"

#define MAX_QUADS 2000

struct Vertex {
    v3 position;
    v4 colour;
    v2 uv;
    i32 draw_type;
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
    TH_PLAYER,
    TH_MISSLE,
    TH_COUNT__
};

struct Texture {
    TextureHandle handle;
    i64 width;
    i64 height;
    v2 uvs[4];
    u8 *data;
};

struct Atlas {
    i64 width;
    i64 height;
    u8 *data;
};

struct Font {
    i64 width;
    i64 height;
    Array<stbtt_bakedchar, 96> characters;
    u8 *bitmap_data;
};

struct Renderer {
    Array<Quad, MAX_QUADS> quads;

    m4 view_projection_matrix;

    Array<Texture, TH_COUNT__> textures;
    Atlas atlas;

    Font font;

    u32 vertex_array_id;
    u32 vertex_buffer_id;
    u32 index_buffer_id;
    u32 shader_program_id;

    u32 atlas_texture_id;
    u32 font_texture_id;
};

v4 WHITE      = {1, 1, 1, 1};
v4 BLACK      = {0, 0, 0, 1};
v4 RED      = {1, 0, 0, 1};
v4 GREEN    = {0, 1, 0, 1};
v4 BLUE     = {0, 0, 1, 1};

bool init_renderer(Renderer *renderer, Window *window);
bool load_textures(Renderer *renderer);
u32 upload_texture_to_gpu(Renderer *renderer, i32 width, i32 height, u8 *data);
u32 upload_font_to_gpu(Renderer *renderer, i32 width, i32 height, u8 *data);
bool load_font(Renderer *renderer, string path, i64 width, i64 height, f32 pixel_height);

void draw_rectangle(Renderer *renderer, v3 position, v2 size, v4 color);
void draw_circle(Renderer *renderer, v3 position, f32 radius, v4 color);
void draw_texture(Renderer *renderer, TextureHandle handle, v3 position, v2 size, f32 rotation, v4 color);
void draw_text(Renderer *renderer, string text, v3 position, f32 font_size, v4 color);
void new_frame(Renderer *renderer, Window *window, Camera camera);
void draw_frame(Renderer *renderer, Window *window);
void push_quad(Renderer *renderer, v3 position, v2 size, f32 rotation, v4 color, v2 uvs[4], i32 draw_type);

m4 get_view_matrix(Camera camera);
m4 get_projection_matrix(Camera camera, f32 aspect);
const char *texture_path(TextureHandle handle);

v4 alpha(v4 base, f32 alpha);

bool init_renderer(Renderer *renderer, Window *window) {
    { // init opengl
        GLenum result = glewInit();
        if (result != GLEW_OK) {
            return false;
        }

        // alpha blend settings
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        float f = 0.0f;
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
    
        ImGui_ImplGlfw_InitForOpenGL(window->glfw_window, true);
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
        glUniform1i(glGetUniformLocation(shader_program, "font_texture"), 1);

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
        glBufferData(GL_ARRAY_BUFFER, sizeof(Quad) * MAX_QUADS, renderer->quads.data, GL_DYNAMIC_DRAW);

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
        glVertexAttribIPointer(3, 1, GL_INT, sizeof(Vertex), (void *) offsetof(Vertex, draw_type));             // draw_type

        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glEnableVertexAttribArray(2);
        glEnableVertexAttribArray(3);
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

    const i64 ATLAS_WIDTH     = 128;
    const i64 ATLAS_HEIGHT    = 128;
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

            texture->uvs[0] = {left_x_uv, top_y_uv};
            texture->uvs[1] = {right_x_uv, top_y_uv};
            texture->uvs[2] = {right_x_uv, bottom_y_uv};
            texture->uvs[3] = {left_x_uv, bottom_y_uv};

            for (i64 row = 0; row < rect->h; row++) {
                u8 *source_row = texture->data + (row * rect->w * BYTES_PER_PIXEL);
                u8 *dest_row = atlas_data + (((rect->y + row) * ATLAS_WIDTH + rect->x) * BYTES_PER_PIXEL);
                memcpy(dest_row, source_row, rect->w * BYTES_PER_PIXEL);
            }
        }

        u32 texture_id = upload_texture_to_gpu(renderer, ATLAS_WIDTH, ATLAS_HEIGHT, atlas_data);
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

u32 upload_texture_to_gpu(Renderer *renderer, i32 width, i32 height, u8 *data) {
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

u32 upload_font_to_gpu(Renderer *renderer, i32 width, i32 height, u8 *data) {
    u32 texture_id = 0;
    glGenTextures(1, &texture_id);

    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // border param might fix texture bleeding
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, data);

    return texture_id;
}

bool load_font(Renderer *renderer, string path, i64 width, i64 height, f32 pixel_height) {
    Font font = Font{
        .width = width,
        .height = height,
        .characters = {},
        .bitmap_data = (u8 *) malloc(width * height),
    };

    Slice<u8> font_data = read_file(path.c());
    if (font_data.len == 0) {
        printf("failed to load font \"%s\"\n", path.c());
        return false;
    }

    i64 bake_result = stbtt_BakeFontBitmap((const u8*)font_data.c(), 0, pixel_height, font.bitmap_data, font.width, font.height, 32, font.characters.size, font.characters.data);
    if (bake_result <= 0) {
        printf("failed to bake font \"%s\"\n", path.c());
        return false;
    }

    { // write debug image out
        stbi_flip_vertically_on_write(false);

        i64 write_result = stbi_write_png("build/font.png", font.width, font.height, 1, font.bitmap_data, font.width);
        if (write_result == 0) {
            printf("error writing font to build folder\n");
            return false;
        }
    }

    renderer->font_texture_id = upload_font_to_gpu(renderer, font.width, font.height, font.bitmap_data);
    assert(renderer->font_texture_id != 0);

    renderer->font = font;

    return true;
}

void draw_rectangle(Renderer *renderer, v3 position, v2 size, v4 color) {
    v2 uvs[4] = {
        {0, 1},
        {1, 1},
        {1, 0},
        {0, 0},
    };

    push_quad(renderer, position, size, 0, color, uvs, 0);
}

void draw_circle(Renderer *renderer, v3 position, f32 radius, v4 color) {
    v2 size = {radius * 2, radius * 2};

    v2 uvs[4] = {
        {0, 1},
        {1, 1},
        {1, 0},
        {0, 0},
    };

    push_quad(renderer, position, size, 0, color, uvs, 1);
}

void draw_texture(Renderer *renderer, TextureHandle handle, v3 position, v2 size, f32 rotation, v4 color) {
    Texture *texture = &renderer->textures[handle];
    push_quad(renderer, position, size, rotation, color, texture->uvs, 2);
}

void draw_text(Renderer *renderer, string text, v3 position, f32 font_size, v4 color) {
    if (text.len == 0) {
        return;
    }

    struct Glyph {
        v2 position;
        v2 size;
        v2 uvs[4];
    };

    Slice<Glyph> glyphs = mem_alloc<Glyph>(text.len);

    f32 total_text_width = 0;
    f32 text_height = 0;

    for (i64 i = 0; i < text.len; i++) {
        char c = text[i];

        f32 advanced_x = 0;
        f32 advanced_y = 0;
        stbtt_aligned_quad aligned_quad = {};

        // this is the the data for the aligned_quad we're given, with y+ going down
        //	   x0, y0       x1, y0
        //     s0, t0       s1, t0
        //	    o tl        o tr
                        
                        
        //     x0, y1      x1, y1
        //     s0, t1      s1, t1
        //	    o bl        o br
        // 
        // x, and y and expected vertex positions
        // s and t are texture uv position
 
        stbtt_GetBakedQuad(renderer->font.characters.data, renderer->font.width, renderer->font.height, c - 32, &advanced_x, &advanced_y, &aligned_quad, false);

        f32 bottom_y = -aligned_quad.y1;
        f32 top_y = -aligned_quad.y0;

        f32 height = top_y - bottom_y;
        f32 width = aligned_quad.x1 - aligned_quad.x0;

        if (height > text_height) {
            text_height = height;
        }

        v2 top_left_uv     = v2{aligned_quad.s0, aligned_quad.t0};
        v2 top_right_uv    = v2{aligned_quad.s1, aligned_quad.t0};
        v2 bottom_right_uv = v2{aligned_quad.s1, aligned_quad.t1};
        v2 bottom_left_uv  = v2{aligned_quad.s0, aligned_quad.t1};

        glyphs[i] = {
            .position = {total_text_width, bottom_y},
            .size = {width, height},
            .uvs = {top_left_uv, top_right_uv, bottom_right_uv, bottom_left_uv}
        };

        // if the character is not the last then add the advanced x to the total width
        // because this includes the with of the character and also the kerning gap added
        // for the next character, if it is the last one then just take the width and have
        // no extra gap at the end - 20/01/25
        if (i < text.len - 1) {
            total_text_width += advanced_x;
        } else {
            total_text_width += width;
        }
    }

    v2 pivot_point_translation = {};
    f32 scale = font_size / text_height;

    for (i64 i = 0; i < glyphs.len; i++) {
        Glyph *glyph = &glyphs[i];

        v2 scaled_position = glyph->position * scale;
        v2 scaled_size = glyph->size * scale;
        v2 translated_position = scaled_position + pivot_point_translation + position.XY;

        // quad needs position to be centre of quad so just convert that here
        v2 quad_centered_position = translated_position + (scaled_size * 0.5f);

        push_quad(renderer, v3{quad_centered_position.X, quad_centered_position.Y, 0}, scaled_size, 0, color, glyph->uvs, 3);
   }

    mem_free(glyphs);
}

void new_frame(Renderer *renderer, Window *window, Camera camera) {
    reset(&renderer->quads);

    renderer->view_projection_matrix = HMM_MulM4(get_projection_matrix(camera, (f32) window->width / (f32) window->height), get_view_matrix(camera));

    { // new frame for imgui
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame(); 
    }

    glClear(GL_COLOR_BUFFER_BIT);
}

void draw_frame(Renderer *renderer, Window *window) {
    { // update the quad buffer and draw
        glViewport(0, 0, window->width, window->height);

        glBindBuffer(GL_ARRAY_BUFFER, renderer->vertex_buffer_id);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(Quad) * renderer->quads.len, renderer->quads.data);
        glBindVertexArray(renderer->vertex_array_id);

        glUseProgram(renderer->shader_program_id);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, renderer->atlas_texture_id);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, renderer->font_texture_id);

        glDrawElements(GL_TRIANGLES, 6 * renderer->quads.len, GL_UNSIGNED_INT, 0);
    }

    { // imgui rendering
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        GLFWwindow *current = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(current);
    }

    glfwSwapBuffers(window->glfw_window);
}

void push_quad(Renderer *renderer, v3 position, v2 size, f32 rotation, v4 color, v2 uvs[4], i32 draw_type) {
    const v4 top_left      = {-0.5,   0.5, 0, 1};
    const v4 top_right     = { 0.5,   0.5, 0, 1};
    const v4 bottom_right  = { 0.5,  -0.5, 0, 1};
    const v4 bottom_left   = {-0.5,  -0.5, 0, 1};

    m4 model_matrix = HMM_M4D(1.0f);
    model_matrix = HMM_MulM4(model_matrix, HMM_Translate(position));
    model_matrix = HMM_MulM4(model_matrix, HMM_Scale({size.X, size.Y, 1}));
    model_matrix = HMM_MulM4(model_matrix, HMM_Rotate_LH(rotation * HMM_DegToRad, {0, 0, 1}));
                
    m4 mvp_matrix = HMM_MulM4(renderer->view_projection_matrix, model_matrix);

    Quad *quad = push(&renderer->quads);
               
    quad->vertices[0].position = HMM_MulM4V4(mvp_matrix, top_left).XYZ;
    quad->vertices[1].position = HMM_MulM4V4(mvp_matrix, top_right).XYZ;
    quad->vertices[2].position = HMM_MulM4V4(mvp_matrix, bottom_right).XYZ;
    quad->vertices[3].position = HMM_MulM4V4(mvp_matrix, bottom_left).XYZ;
                
    quad->vertices[0].colour = color;
    quad->vertices[1].colour = color;
    quad->vertices[2].colour = color;
    quad->vertices[3].colour = color;

    quad->vertices[0].uv = uvs[0];
    quad->vertices[1].uv = uvs[1];
    quad->vertices[2].uv = uvs[2];
    quad->vertices[3].uv = uvs[3];

    quad->vertices[0].draw_type = draw_type;
    quad->vertices[1].draw_type = draw_type;
    quad->vertices[2].draw_type = draw_type;
    quad->vertices[3].draw_type = draw_type;
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
        case TH_PLAYER: 
            return "resources/textures/player.png";
        case TH_MISSLE: 
            return "resources/textures/missle.png";
        case TH_COUNT__: 
            assert(0);
    }

    return nullptr;
}

v4 alpha(v4 base, f32 alpha) {
    return {base.R, base.G, base.B, alpha};
}

#endif
