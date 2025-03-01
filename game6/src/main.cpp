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

        draw_texture(&state.renderer, TH_ALIEN, {-50, -50, 10}, {100, 100}, WHITE);
        draw_rectangle(&state.renderer, {0, 0, 1}, {100, 100}, alpha(RED, 0.2));

        draw_frame(&state.renderer, state.window, state.camera);
        glfwSwapBuffers(state.window.glfw_window);
    }

    glfwTerminate();

    return 0;
}
