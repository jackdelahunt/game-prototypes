#ifndef RENDERER_CPP
#define RENDERER_CPP

#include <stddef.h>

#define MAX_QUADS 100

struct Vertex {
    v3 position;
    v4 colour;
};

struct Quad {
    Vertex vertices[4];
};

struct DrawCommand {
    v3 position;
    v2 size;
};

struct Camera {
    v3 position;
    f32 orthographic_size;
    f32 near_plane;
    f32 far_plane;
};

struct Renderer {
    DrawCommand commands[MAX_QUADS];
    i64 command_count;

    Quad quads[MAX_QUADS];
    i64 quad_count;

    u32 vertex_array_id;
    u32 vertex_buffer_id;
    u32 index_buffer_id;
    u32 shader_program_id;
};

v4 RED      = {1, 0, 0, 1};
v4 GREEN    = {0, 1, 0, 1};
v4 BLUE     = {0, 0, 1, 1};

bool init_renderer(Renderer *renderer, Window window);
void draw_quad(Renderer *renderer, v3 position, v2 size);
void new_frame(Renderer *renderer);
void draw_frame(Renderer *renderer, Window window, Camera camera);
m4 get_view_matrix(Camera camera);
m4 get_projection_matrix(Camera camera, f32 aspect);

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
    
        Slice<char> vertex_shader_source = read_file("./src/shaders/vertex.shader");
        if (vertex_shader_source.len == 0) {
            printf("failed to load vertex shader\n");
            return false;
        }

        Slice<char> fragment_shader_source = read_file("./src/shaders/fragment.shader");
        if (fragment_shader_source.len == 0) {
            printf("failed to load vertex shader\n");
            return false;
        }

        u32 vertex_shader = glCreateShader(GL_VERTEX_SHADER);

        glShaderSource(vertex_shader, 1, &vertex_shader_source.data, NULL);
        glCompileShader(vertex_shader);

        glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &compile_status);
        if (compile_status == 0) {
            glGetShaderInfoLog(vertex_shader, buffer_size, nullptr, &error_buffer[0]);
            printf("failed to compile vertex shader: %s", error_buffer);
            return false;
        }

        u32 fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

        glShaderSource(fragment_shader, 1, &fragment_shader_source.data, NULL);
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
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
    }

    return true;
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
        }
    }

    { // draw the things we have sent to the renderer
        glViewport(0, 0, window.width, window.height);
        glBindBuffer(GL_ARRAY_BUFFER, renderer->vertex_buffer_id);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(Quad) * renderer->quad_count, renderer->quads);
        glUseProgram(renderer->shader_program_id);
        glBindVertexArray(renderer->vertex_array_id);
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

#endif
