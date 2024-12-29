#include "platform.h"
#include "common.h"

#include <assert.h>
#include <cstdio>
#include <cstdlib>
#include <stdlib.h>

#pragma warning(disable:4201)   // needed for /W4 and including d3d11.h

#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <d3d11.h>
#include <dxgi.h>

#define SOKOL_D3D11
#define SOKOL_IMPL
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_log.h"

static LRESULT CALLBACK d3d11_winproc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static void d3d11_create_default_render_targets();
static void d3d11_destroy_default_render_targets();
static void d3d11_update_default_render_targets();
static const void* d3d11_device();
static const void* d3d11_device_context();

static const IID _d3d11entry_IID_ID3D11Texture2D = { 0x6f15aaf2,0xd208,0x4e89,0x9a,0xb4,0x48,0x95,0x35,0xd3,0x4f,0x9c };

typedef void(*d3d11_key_func)(int key);
typedef void(*d3d11_char_func)(wchar_t c);
typedef void(*d3d11_mouse_btn_func)(int btn);
typedef void(*d3d11_mouse_pos_func)(float x, float y);
typedef void(*d3d11_mouse_wheel_func)(float v);

struct State {
    HMODULE game_dll;
    HANDLE std_out;
    HANDLE std_err;

    bool quit_requested;
    bool in_create_window;
    HWND hwnd;
    DWORD win_style;
    DWORD win_ex_style;
    DXGI_SWAP_CHAIN_DESC swap_chain_desc;
    int width;
    int height;
    int sample_count;
    bool no_depth_buffer; 
    ID3D11Device* device;
    ID3D11DeviceContext* device_context;
    IDXGISwapChain* swap_chain;
    ID3D11Texture2D* rt_tex;
    ID3D11RenderTargetView* rt_view;
    ID3D11Texture2D* msaa_tex;
    ID3D11RenderTargetView* msaa_view;
    ID3D11Texture2D* ds_tex;
    ID3D11DepthStencilView* ds_view;
    d3d11_key_func key_down_func;
    d3d11_key_func key_up_func;
    d3d11_char_func char_func;
    d3d11_mouse_btn_func mouse_btn_down_func;
    d3d11_mouse_btn_func mouse_btn_up_func;
    d3d11_mouse_pos_func mouse_pos_func;
    d3d11_mouse_wheel_func mouse_wheel_func;
};

State state = {
    .win_style = WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SIZEBOX,
    .win_ex_style = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE,
};

void platform_init(int width, int height, const wchar_t *title) {
    assert(width > 0);
    assert(height > 0);
    assert(title);

    bool ok = AllocConsole();
    if(!ok) {
        return;
    }

    state.std_out = GetStdHandle(STD_OUTPUT_HANDLE);
    state.std_err = GetStdHandle(STD_ERROR_HANDLE);

    int sample_count = 1;

    state.width = width;
    state.height = height;
    state.sample_count = sample_count;
    state.no_depth_buffer = false;

    // register window class
    WNDCLASSW window_class = {
        .style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC,
        .lpfnWndProc = (WNDPROC) d3d11_winproc,
        .hInstance = GetModuleHandleW(NULL),
        .hIcon = LoadIcon(NULL, IDI_WINLOGO),
        .hCursor = LoadCursor(NULL, IDC_ARROW),
        .lpszClassName = L"SOKOLD3D11"
    };

    RegisterClassW(&window_class);

    // create window
    state.in_create_window = true;
    RECT rect = { .left = 0, .top = 0, .right = state.width, .bottom = state.height };
    AdjustWindowRectEx(&rect, state.win_style, FALSE, state.win_ex_style);
    const int win_width = rect.right - rect.left;
    const int win_height = rect.bottom - rect.top;
    state.hwnd = CreateWindowExW(
        state.win_ex_style, // dwExStyle
        L"SOKOLD3D11",      // lpClassName
        title,              // lpWindowName
        state.win_style,    // dwStyle
        CW_USEDEFAULT,      // X
        CW_USEDEFAULT,      // Y
        win_width,          // nWidth
        win_height,         // nHeight
        NULL,               // hWndParent
        NULL,               // hMenu
        GetModuleHandle(NULL),  //hInstance
        NULL                // lpParam
    );              

    ShowWindow(state.hwnd, SW_SHOW);
    state.in_create_window = false;

    // create device and swap chain
    state.swap_chain_desc = {
        .BufferDesc = {
            .Width = (UINT)state.width,
            .Height = (UINT)state.height,
            .RefreshRate = {
                .Numerator = 60,
                .Denominator = 1
            },
            .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
        },
        .SampleDesc = {
            .Count = (UINT) 1,
            .Quality = (UINT) 0,
        },
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount = 2,
        .OutputWindow = state.hwnd,
        .Windowed = true,
        .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
    };

    UINT create_flags = D3D11_CREATE_DEVICE_SINGLETHREADED;
    #ifdef _DEBUG
        create_flags |= D3D11_CREATE_DEVICE_DEBUG;
    #endif
    D3D_FEATURE_LEVEL feature_level;
    // NOTE: on some Win10 configs (like my gaming PC), device creation
    // with the debug flag fails
    HRESULT hr;
    for (int i = 0; i < 2; i++) {
        hr = D3D11CreateDeviceAndSwapChain(
            NULL,                       // pAdapter (use default)
            D3D_DRIVER_TYPE_HARDWARE,   // DriverType
            NULL,                       // Software
            create_flags,               // Flags
            NULL,                       // pFeatureLevels
            0,                          // FeatureLevels
            D3D11_SDK_VERSION,          // SDKVersion
            &state.swap_chain_desc,     // pSwapChainDesc
            &state.swap_chain,          // ppSwapChain
            &state.device,              // ppDevice
            &feature_level,             // pFeatureLevel
            &state.device_context);     // ppImmediateContext
        if (SUCCEEDED(hr)) {
            break;
        } else {
            create_flags &= ~D3D11_CREATE_DEVICE_DEBUG;
        }
    }
    assert(SUCCEEDED(hr) && state.swap_chain && state.device && state.device_context);

    // default render target and depth-stencil-buffer
    d3d11_create_default_render_targets();
}

bool platform_process_events() {
    MSG msg;
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (WM_QUIT == msg.message) {
            state.quit_requested = true;
        }
        else {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    return !state.quit_requested;
}

void platform_present() {
    state.swap_chain->Present(1, 0);

    /* handle window resizing */
    RECT r;
    if (GetClientRect(state.hwnd, &r)) {
        const int cur_width = r.right - r.left;
        const int cur_height = r.bottom - r.top;
        if (((cur_width > 0) && (cur_width != state.width)) ||
            ((cur_height > 0) && (cur_height != state.height)))
        {
            /* need to reallocate the default render target */
            state.width = cur_width;
            state.height = cur_height;
            d3d11_update_default_render_targets();
        }
    }
}

sg_swapchain platform_swapchain() {
    return {
        .width = state.width,
        .height = state.height,
        .sample_count = state.sample_count,
        .color_format = SG_PIXELFORMAT_BGRA8,
        .depth_format = state.no_depth_buffer ? SG_PIXELFORMAT_NONE : SG_PIXELFORMAT_DEPTH_STENCIL,
        .d3d11 = {
            .render_view = (state.sample_count == 1) ? state.rt_view : state.msaa_view,
            .resolve_view = (state.sample_count == 1) ? 0 : state.rt_view,
            .depth_stencil_view = state.ds_view,
        }
    };
}

sg_environment platform_enviroment() {
    return {
        .defaults = {
            .color_format = SG_PIXELFORMAT_BGRA8,
            .depth_format = state.no_depth_buffer ? SG_PIXELFORMAT_NONE : SG_PIXELFORMAT_DEPTH_STENCIL,
            .sample_count = state.sample_count,
        },
        .d3d11 = {
            .device = d3d11_device(),
            .device_context = d3d11_device_context(),
        }
    };
}

internal
void d3d11_shutdown() {
    FreeConsole();
    d3d11_destroy_default_render_targets();
    state.swap_chain->Release();
    state.device_context->Release();
    state.device->Release();
    DestroyWindow(state.hwnd); state.hwnd = 0;
    UnregisterClassW(L"SOKOLD3D11", GetModuleHandleW(NULL));
}

internal
void d3d11_create_default_render_targets(void) {
    HRESULT hr;
    hr = state.swap_chain->GetBuffer(0, _d3d11entry_IID_ID3D11Texture2D, (void**)&state.rt_tex);
    assert(SUCCEEDED(hr) && state.rt_tex);

    hr = state.device->CreateRenderTargetView((ID3D11Resource*)state.rt_tex, NULL, &state.rt_view);
    assert(SUCCEEDED(hr) && state.rt_view);

    D3D11_TEXTURE2D_DESC tex_desc = {
        .Width = (UINT)state.width,
        .Height = (UINT)state.height,
        .MipLevels = 1,
        .ArraySize = 1,
        .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
        .SampleDesc = {
            .Count = (UINT)state.sample_count,
            .Quality = (UINT) (state.sample_count > 1 ? D3D11_STANDARD_MULTISAMPLE_PATTERN : 0),
        },
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_RENDER_TARGET,
        
    };
    // MSAA render target and view
    if (state.sample_count > 1) {
        hr = state.device->CreateTexture2D(&tex_desc, NULL, &state.msaa_tex);
        assert(SUCCEEDED(hr) && state.msaa_tex);

        hr = state.device->CreateRenderTargetView((ID3D11Resource*)state.msaa_tex, NULL, &state.msaa_view);
        assert(SUCCEEDED(hr) && state.msaa_view);
    }

    // depth-stencil render target and view
    if (!state.no_depth_buffer) {
        tex_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        tex_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

        hr = state.device->CreateTexture2D(&tex_desc, NULL, &state.ds_tex);
        assert(SUCCEEDED(hr) && state.ds_tex);

        hr = state.device->CreateDepthStencilView((ID3D11Resource*)state.ds_tex, NULL, &state.ds_view);
        assert(SUCCEEDED(hr) && state.ds_view);
    }
}

internal
void d3d11_destroy_default_render_targets(void) {
    if (state.rt_tex != nullptr) {
        state.rt_tex->Release();
    }
    
    if (state.rt_view != nullptr) {
        state.rt_view->Release();
    }

    if (state.ds_tex != nullptr) {
        state.ds_tex->Release();
    }
    
    if (state.ds_view != nullptr) {
        state.ds_view->Release();
    }

    if (state.msaa_tex != nullptr) {
        state.msaa_tex->Release();
    }

    if (state.msaa_view != nullptr) {
        state.msaa_view->Release();
    }
}

internal
void d3d11_update_default_render_targets(void) {
    if (state.swap_chain) {
        d3d11_destroy_default_render_targets();
        state.swap_chain->ResizeBuffers(2, state.width, state.height, DXGI_FORMAT_B8G8R8A8_UNORM, 0);
        d3d11_create_default_render_targets();
    }
}

internal
LRESULT CALLBACK d3d11_winproc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CLOSE:
            state.quit_requested = true;
            return 0;
        case WM_ERASEBKGND:
            return TRUE;
        case WM_LBUTTONDOWN:
            if (state.mouse_btn_down_func) {
                state.mouse_btn_down_func(0);
            }
            break;
        case WM_RBUTTONDOWN:
            if (state.mouse_btn_down_func) {
                state.mouse_btn_down_func(1);
            }
            break;
        case WM_LBUTTONUP:
            if (state.mouse_btn_up_func) {
                state.mouse_btn_up_func(0);
            }
            break;
        case WM_RBUTTONUP:
            if (state.mouse_btn_up_func) {
                state.mouse_btn_up_func(1);
            }
            break;
        case WM_MOUSEMOVE:
            if (state.mouse_pos_func) {
                const int x = GET_X_LPARAM(lParam);
                const int y = GET_Y_LPARAM(lParam);
                state.mouse_pos_func((float)x, (float)y);
            }
            break;
        case WM_MOUSEWHEEL:
            if (state.mouse_wheel_func) {
                state.mouse_wheel_func((float)((SHORT)HIWORD(wParam) / 30.0f));
            }
            break;
        case WM_CHAR:
            if (state.char_func) {
                state.char_func((wchar_t)wParam);
            }
            break;
        case WM_KEYDOWN:
            if (state.key_down_func) {
                state.key_down_func((int)wParam);
            }
            break;
        case WM_KEYUP:
            if (state.key_up_func) {
                state.key_up_func((int)wParam);
            }
            break;
        default:
            break;
    }
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

internal
const void* d3d11_device(void) {
    return (const void*) state.device;
}

internal
const void* d3d11_device_context(void) {
    return (const void*) state.device_context;
}

internal
int d3d11_width(void) {
    return state.width;
}

internal
int d3d11_height() {
    return state.height;
}

internal
void d3d11_key_down(d3d11_key_func f) {
    state.key_down_func = f;
}

internal
void d3d11_key_up(d3d11_key_func f) {
    state.key_up_func = f;
}

internal
void d3d11_char(d3d11_char_func f) {
    state.char_func = f;
}

internal
void d3d11_mouse_btn_down(d3d11_mouse_btn_func f) {
    state.mouse_btn_down_func = f;
}

internal
void d3d11_mouse_btn_up(d3d11_mouse_btn_func f) {
    state.mouse_btn_up_func = f;
}

internal
void d3d11_mouse_pos(d3d11_mouse_pos_func f) {
    state.mouse_pos_func = f;
}

internal
void d3d11_mouse_wheel(d3d11_mouse_wheel_func f) {
    state.mouse_wheel_func = f;
}

void platform_write(const char *text, i32 length) {
    WriteConsoleA(state.std_out, text, length, NULL, NULL);
}

typedef decltype(set_platform) set_platform_type;
internal set_platform_type *set_platform_callback;

typedef decltype(start) start_type;
internal start_type *start_callback;

typedef decltype(reload) reload_type;
internal reload_type *reload_callback;

typedef decltype(run) run_type;
internal run_type *run_callback;

internal
void load_game_dll() {
    if (state.game_dll) {
        FreeLibrary(state.game_dll);
        DeleteFileA("build\\game.dll");
    }

    bool copy_ok = CopyFileA("build\\game_preload.dll", "build\\game.dll", true);

    state.game_dll = LoadLibrary("build\\game.dll");
    assert(state.game_dll);
    
    set_platform_callback = (set_platform_type*) GetProcAddress(state.game_dll, "set_platform");
    assert(set_platform_callback);

    start_callback = (start_type*) GetProcAddress(state.game_dll, "start");
    assert(start_callback);

    reload_callback = (reload_type*) GetProcAddress(state.game_dll, "reload");
    assert(reload_callback);

    run_callback = (run_type*) GetProcAddress(state.game_dll, "run");
    assert(run_callback);
}

internal void *game_state;

void *platform_alloc_state(i32 size) {
    game_state = malloc(size);
    assert(game_state);
    return game_state;
}

void platform_reload_game() {
    load_game_dll();
    set_platform_callback({
        .init = platform_init,
        .alloc_state = platform_alloc_state,
        .present = platform_present,
        .reload_game = platform_reload_game,
        .swapchain = platform_swapchain,
        .enviroment = platform_enviroment,
        .write = platform_write,
    });

    reload_callback(game_state);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    load_game_dll();

    set_platform_callback(Platform{
        .init = platform_init,
        .alloc_state = platform_alloc_state,
        .present = platform_present,
        .reload_game = platform_reload_game,
        .swapchain = platform_swapchain,
        .enviroment = platform_enviroment,
        .write = platform_write,
    });

    start_callback(); 

    while(platform_process_events()) {
        run_callback();
    }

    d3d11_shutdown();
    return 0;
}
