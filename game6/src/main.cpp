#include <stdio.h>

#include "libs/libs.h"
#include "game.h"

// Total: 14

struct State {
    Camera camera;
    Window window;
    Renderer renderer;
} state = {};

int main() {
    state = {
        .camera = {
            .position = {0, 0, 0},
            .orthographic_size = 250,
            .near_plane = 0.1f,
            .far_plane = 100.0f,
        }
    };

    {
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

    while (!glfwWindowShouldClose(state.window.glfw_window)) {
        glfwPollEvents();

        if (KEYS[GLFW_KEY_ESCAPE] == InputState::down) {
            glfwSetWindowShouldClose(state.window.glfw_window, GLFW_TRUE);
        }

        new_frame(&state.renderer);

        static v3 position = {0, 0, 1};
        static v2 size = {100, 100};

        draw_quad(&state.renderer, position, size);
        draw_quad(&state.renderer, v3{position.X + (size.X * 2), position.Y  + (size.Y * 2), position.Z}, size);

        if (ImGui::Begin("Inspector", 0, ImGuiChildFlags_AlwaysAutoResize)) {
            ImGui::PushID("camera");
            ImGui::SeparatorText("Camera");
            ImGui::SliderFloat2("position", (f32 *) &state.camera.position, -500, 500);
            ImGui::SliderFloat("ortho size", &state.camera.orthographic_size, 1, 500);
            ImGui::PopID();
            ImGui::PushID("quad");
            ImGui::SeparatorText("Quad");
            ImGui::SliderFloat3("position", (f32 *) &position, -10, 10);
            ImGui::SliderFloat2("size", (f32 *) &size, 1, 300);
            ImGui::PopID();
            v3 ndc = state.renderer.quads[0].vertices[0].position;
            ImGui::Text("%f %f %f", ndc.X, ndc.Y, ndc.Z);
            ImGui::End();
        }

        draw_frame(&state.renderer, state.window, state.camera);
        glfwSwapBuffers(state.window.glfw_window);
    }

    glfwTerminate();

    return 0;
}
