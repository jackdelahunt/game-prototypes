#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl.h>

#include <iostream>

#include "libs/libs.h"
#include "ack.cpp"
#include "math.cpp"

// Total: 02:00
// Started: 12:30

enum class InputState {
    UP,
    DOWN,
    PRESSED
};

StackArray<InputState, 348> KEYS = {};

template <typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

ID3D11Device*            g_pd3dDevice = nullptr;
ID3D11DeviceContext*     g_pd3dDeviceContext = nullptr;
IDXGISwapChain*          g_pSwapChain = nullptr;
bool                     g_SwapChainOccluded = false;
UINT                     g_ResizeWidth = 0, g_ResizeHeight = 0;
ID3D11RenderTargetView*  g_mainRenderTargetView = nullptr;

bool create_device_d3d(HWND window_handle);
void create_render_target();

ComPtr<ID3DBlob> compile_shader(string path);

LRESULT CALLBACK winproc(HWND window_handle, UINT message, WPARAM w_param, LPARAM l_param);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASSW window_class = {
        .style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC,
        .lpfnWndProc = (WNDPROC) winproc,
        .hInstance = GetModuleHandleW(NULL),
        .hIcon = LoadIcon(NULL, IDI_WINLOGO),
        .hCursor = LoadCursor(NULL, IDC_ARROW),
        .lpszClassName = L"WindowClass"
    };

    RegisterClassW(&window_class);

    HWND window_handle = CreateWindowW(
        window_class.lpszClassName,
        L"game10",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1920,
        1080,
        NULL,
        NULL,
        window_class.hInstance,
        NULL
    );

    if (!create_device_d3d(window_handle)) {
        return 1;
    }

    ShowWindow(window_handle, SW_SHOW);
    UpdateWindow(window_handle);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(window_handle);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    ComPtr<ID3DBlob> blob = compile_shader("resources/shaders/vs.hlsl");

    bool running = true;
    v4 clear_colour = {0.7, 0.7, 1, 1};

    while (running) {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);

            if (msg.message == WM_QUIT) {
                running = false;
            }
        }


        if (KEYS[VK_ESCAPE] == InputState::DOWN) {
            running = false;
        }

        // Start the Dear ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // ImGui::ShowDemoWindow();

        ImGui::Begin("Settings");
        ImGui::SliderFloat4("Clear colour", &clear_colour[0], 0, 1);
        ImGui::End();

        ImGui::Render();
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, &clear_colour[0]);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        // Present
        HRESULT hr = g_pSwapChain->Present(1, 0);   // Present with vsync
        //HRESULT hr = g_pSwapChain->Present(0, 0); // Present without vsync
        g_SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    DestroyWindow(window_handle);
    UnregisterClassW(window_class.lpszClassName, window_class.hInstance);

    return 0;
}

bool create_device_d3d(HWND window_handle) {
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = window_handle;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT create_device_flags = 0;
    //createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
    
    D3D_FEATURE_LEVEL feature_level;
    const D3D_FEATURE_LEVEL feature_level_array[2] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, create_device_flags, feature_level_array, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &feature_level, &g_pd3dDeviceContext);

    if (res == DXGI_ERROR_UNSUPPORTED) { // Try high-performance WARP software driver if hardware is not available
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, create_device_flags, feature_level_array, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &feature_level, &g_pd3dDeviceContext);
    }

    if (res != S_OK) {
        return false;
    }

    create_render_target();

    return true;
}

void create_render_target() {
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}


ComPtr<ID3DBlob> compile_shader(string path) {
    UINT compileFlags = D3DCOMPILE_ENABLE_STRICTNESS;

    ComPtr<ID3DBlob> blob = nullptr;
    ComPtr<ID3DBlob> error = nullptr;

    HRESULT result =  D3DCompileFromFile(
        path.w(),
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "Main",
        "vs_5_0",
        compileFlags,
        0,
        &blob,
        &error
    );

    if (FAILED(result)) {
        std::cout << "D3D11: Failed to read shader from file\n";
        if (error != nullptr) {
            std::cout << "D3D11: With message: " << 
            static_cast<const char*>(error->GetBufferPointer()) << "\n";
        }

        return nullptr;
    }

    return blob;
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK winproc(HWND window_handle, UINT message, WPARAM w_param, LPARAM l_param) {
    if (ImGui_ImplWin32_WndProcHandler(window_handle, message, w_param, l_param)) {
        return true;
    }

    switch (message) {
        case WM_DESTROY: {
            PostQuitMessage(0);
        } break;
        case WM_KEYDOWN: {
            if (w_param < KEYS.size) {
                if (KEYS[w_param] == InputState::UP) {
                    KEYS[w_param] = InputState::DOWN;
                } 
                else if (KEYS[w_param] == InputState::DOWN) {
                    KEYS[w_param] = InputState::PRESSED;
                }
            }
        } break;
        case WM_KEYUP: {
            if (w_param < KEYS.size) {
                KEYS[w_param] = InputState::UP;
            }
        } break;
        // case WM_CLOSE:
        // case WM_ERASEBKGND:
        // case WM_LBUTTONDOWN:
        // case WM_RBUTTONDOWN:
        // case WM_LBUTTONUP:
        // case WM_RBUTTONUP:
        // case WM_MOUSEMOVE:
        // case WM_MOUSEWHEEL:
        // case WM_CHAR:
        // case WM_SIZING:
        default: {
            return DefWindowProcW(window_handle, message, w_param, l_param);
        }
    }

    return 0;
}

