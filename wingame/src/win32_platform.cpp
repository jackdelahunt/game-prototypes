#include "game.h"
#include "platform.h"
#include "common.h"
#include "game.cpp"

#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <d3d11.h>
#include <dxgi.h>
#include <assert.h>

// needs to be included after game.cpp so their 
// includes of sokol dont have SOKOL_IMPL defined
#define SOKOL_D3D11
#define SOKOL_IMPL
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_log.h"

static LRESULT CALLBACK winproc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

internal void d3d11_create_default_render_targets();
internal void d3d11_destroy_default_render_targets();
internal void d3d11_update_default_render_targets();

internal const IID _d3d11entry_IID_ID3D11Texture2D = { 0x6f15aaf2,0xd208,0x4e89,0x9a,0xb4,0x48,0x95,0x35,0xd3,0x4f,0x9c };

struct Win32State {
    HWND hwnd;
    DWORD win_style;
    DWORD win_ex_style;

    i32 width;
    i32 height;
    i32 sample_count;
    bool no_depth_buffer;

    i32 sync_interval; // 0 for vsync off

    HANDLE std_out;

    DXGI_SWAP_CHAIN_DESC swap_chain_desc;
    ID3D11Device* device;
    ID3D11DeviceContext* device_context;
    IDXGISwapChain* swap_chain;
    ID3D11Texture2D* rt_tex;
    ID3D11RenderTargetView* rt_view;
    ID3D11Texture2D* msaa_tex;
    ID3D11RenderTargetView* msaa_view;
    ID3D11Texture2D* ds_tex;
    ID3D11DepthStencilView* ds_view;
};

internal Win32State win32_state;

void platform_init_window(i32 width, i32 height, const char *title) {
    assert(width > 0);
    assert(height > 0);
    assert(title);

    win32_state = {
        .win_style = WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SIZEBOX,
        .win_ex_style = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE,
        .sync_interval = 1,
    };

    bool ok = AllocConsole();
    if(!ok) {
        return;
    }

    win32_state.std_out = GetStdHandle(STD_OUTPUT_HANDLE);

    int sample_count = 1;

    win32_state.width = width;
    win32_state.height = height;
    win32_state.sample_count = sample_count;
    win32_state.no_depth_buffer = false;

    // register window class
    WNDCLASSA window_class = {
        .style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC,
        .lpfnWndProc = (WNDPROC) winproc,
        .hInstance = GetModuleHandleW(NULL),
        .hIcon = LoadIcon(NULL, IDI_WINLOGO),
        .hCursor = LoadCursor(NULL, IDC_ARROW),
        .lpszClassName = "GameWindowClass"
    };

    RegisterClassA(&window_class);

    // create window
    RECT rect = { .left = 0, .top = 0, .right = win32_state.width, .bottom = win32_state.height };
    AdjustWindowRectEx(&rect, win32_state.win_style, FALSE, win32_state.win_ex_style);

    const int win_width = rect.right - rect.left;
    const int win_height = rect.bottom - rect.top;

    win32_state.hwnd = CreateWindowExA(
        win32_state.win_ex_style, // dwExStyle
        window_class.lpszClassName, // lpClassName
        title,              // lpWindowName
        win32_state.win_style,    // dwStyle
        CW_USEDEFAULT,      // X
        CW_USEDEFAULT,      // Y
        win_width,          // nWidth
        win_height,         // nHeight
        NULL,               // hWndParent
        NULL,               // hMenu
        GetModuleHandle(NULL),  //hInstance
        NULL                // lpParam
    );              

    ShowWindow(win32_state.hwnd, SW_SHOW);

    // create device and swap chain
    win32_state.swap_chain_desc = {
        .BufferDesc = {
            .Width = (UINT)win32_state.width,
            .Height = (UINT)win32_state.height,
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
        .OutputWindow = win32_state.hwnd,
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
            &win32_state.swap_chain_desc,     // pSwapChainDesc
            &win32_state.swap_chain,          // ppSwapChain
            &win32_state.device,              // ppDevice
            &feature_level,             // pFeatureLevel
            &win32_state.device_context);     // ppImmediateContext
        if (SUCCEEDED(hr)) {
            break;
        } else {
            create_flags &= ~D3D11_CREATE_DEVICE_DEBUG;
        }
    }
    assert(SUCCEEDED(hr) && win32_state.swap_chain && win32_state.device && win32_state.device_context);

    // default render target and depth-stencil-buffer
    d3d11_create_default_render_targets();
}

void platform_process_events() {
    MSG msg;
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

void platform_present() {
    win32_state.swap_chain->Present(win32_state.sync_interval, 0);

    /* handle window resizing */
    RECT r;
    if (GetClientRect(win32_state.hwnd, &r)) {
        const int cur_width = r.right - r.left;
        const int cur_height = r.bottom - r.top;
        if (((cur_width > 0) && (cur_width != win32_state.width)) ||
            ((cur_height > 0) && (cur_height != win32_state.height)))
        {
            /* need to reallocate the default render target */
            win32_state.width = cur_width;
            win32_state.height = cur_height;
            d3d11_update_default_render_targets();
        }
    }
}

sg_swapchain platform_swapchain() {
    return {
        .width = win32_state.width,
        .height = win32_state.height,
        .sample_count = win32_state.sample_count,
        .color_format = SG_PIXELFORMAT_BGRA8,
        .depth_format = win32_state.no_depth_buffer ? SG_PIXELFORMAT_NONE : SG_PIXELFORMAT_DEPTH_STENCIL,
        .d3d11 = {
            .render_view = (win32_state.sample_count == 1) ? win32_state.rt_view : win32_state.msaa_view,
            .resolve_view = (win32_state.sample_count == 1) ? 0 : win32_state.rt_view,
            .depth_stencil_view = win32_state.ds_view,
        }
    };
}

sg_environment platform_enviroment() {
    return {
        .defaults = {
            .color_format = SG_PIXELFORMAT_BGRA8,
            .depth_format = win32_state.no_depth_buffer ? SG_PIXELFORMAT_NONE : SG_PIXELFORMAT_DEPTH_STENCIL,
            .sample_count = win32_state.sample_count,
        },
        .d3d11 = {
            .device = (const void*) win32_state.device,
            .device_context = (const void*) win32_state.device_context,
        }
    };
}

internal
Key convert_key(WPARAM w_param) {
    // https://learn.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes
    i64 key_code = (i64)w_param;

    switch (key_code) {
        case VK_ESCAPE: return KEY_ESCAPE;
    }

    // 0-9
    if (key_code >= 0x30 && key_code <= 0x39) {
        return (Key) (key_code - 0x30);
    }

    // A-Z
    if (key_code >= 0x41 && key_code <= 0x5A) {
        return (Key)(KEY_A + (key_code - 0x41));
    }

    // could not convert it, :[
    return _KEY_LAST_;
}

internal
LRESULT CALLBACK winproc(HWND window_handle, UINT message, WPARAM w_param, LPARAM l_param) {
    switch (message) {
        case WM_CLOSE:
            game_quit();
            return 0;
        case WM_ERASEBKGND:
            return TRUE;
        case WM_LBUTTONDOWN: {
            mouse_button_event(MOUSE_LEFT, InputState::DOWN);
            break;
        }
        case WM_RBUTTONDOWN: {
            mouse_button_event(MOUSE_RIGHT, InputState::DOWN);
            break;
        }
        case WM_LBUTTONUP: {
            mouse_button_event(MOUSE_LEFT, InputState::UP);
            break;
        }
        case WM_RBUTTONUP: {
            mouse_button_event(MOUSE_RIGHT, InputState::UP);
            break;
        }
        case WM_MOUSEMOVE: {
            f32 x = (f32) GET_X_LPARAM(l_param);
            f32 y = (f32) GET_Y_LPARAM(l_param);

            // Game is always assuming y0 is bottom, so need
            // to convert it, windows is y0 at the top
            y = win32_state.height - y;

            mouse_move_event(x, y);
            break;
        }
        case WM_MOUSEWHEEL:
            break;
        case WM_CHAR:
            break;
        case WM_KEYDOWN: {
            Key key = convert_key(w_param);
            if (key == _KEY_LAST_) {
                break;
            }

            key_event(key, InputState::DOWN);
            break;
        }
        case WM_KEYUP: {
            Key key = convert_key(w_param);
            if (key == _KEY_LAST_) {
                break;
            }

            key_event(key, InputState::UP);
            break;
        }
        case WM_SIZING: {
            RECT *window_rect = (RECT *)l_param;

            i32 width = window_rect->left - window_rect->right;
            i32 height = window_rect->top - window_rect->bottom;

            win32_state.width = width;
            win32_state.height = height;

            window_resize(width, height);
            break;
        }
        default:
            break;
    }

    return DefWindowProcW(window_handle, message, w_param, l_param);
}

internal
void d3d11_create_default_render_targets(void) {
    HRESULT hr;
    hr = win32_state.swap_chain->GetBuffer(0, _d3d11entry_IID_ID3D11Texture2D, (void**)&win32_state.rt_tex);
    assert(SUCCEEDED(hr) && win32_state.rt_tex);

    hr = win32_state.device->CreateRenderTargetView((ID3D11Resource*)win32_state.rt_tex, NULL, &win32_state.rt_view);
    assert(SUCCEEDED(hr) && win32_state.rt_view);

    D3D11_TEXTURE2D_DESC tex_desc = {
        .Width = (UINT)win32_state.width,
        .Height = (UINT)win32_state.height,
        .MipLevels = 1,
        .ArraySize = 1,
        .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
        .SampleDesc = {
            .Count = (UINT)win32_state.sample_count,
            .Quality = (UINT) (win32_state.sample_count > 1 ? D3D11_STANDARD_MULTISAMPLE_PATTERN : 0),
        },
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_RENDER_TARGET,
        
    };
    // MSAA render target and view
    if (win32_state.sample_count > 1) {
        hr = win32_state.device->CreateTexture2D(&tex_desc, NULL, &win32_state.msaa_tex);
        assert(SUCCEEDED(hr) && win32_state.msaa_tex);

        hr = win32_state.device->CreateRenderTargetView((ID3D11Resource*)win32_state.msaa_tex, NULL, &win32_state.msaa_view);
        assert(SUCCEEDED(hr) && win32_state.msaa_view);
    }

    // depth-stencil render target and view
    if (!win32_state.no_depth_buffer) {
        tex_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        tex_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

        hr = win32_state.device->CreateTexture2D(&tex_desc, NULL, &win32_state.ds_tex);
        assert(SUCCEEDED(hr) && win32_state.ds_tex);

        hr = win32_state.device->CreateDepthStencilView((ID3D11Resource*)win32_state.ds_tex, NULL, &win32_state.ds_view);
        assert(SUCCEEDED(hr) && win32_state.ds_view);
    }
}

internal
void d3d11_destroy_default_render_targets(void) {
    if (win32_state.rt_tex != nullptr) {
        win32_state.rt_tex->Release();
    }
    
    if (win32_state.rt_view != nullptr) {
        win32_state.rt_view->Release();
    }

    if (win32_state.ds_tex != nullptr) {
        win32_state.ds_tex->Release();
    }
    
    if (win32_state.ds_view != nullptr) {
        win32_state.ds_view->Release();
    }

    if (win32_state.msaa_tex != nullptr) {
        win32_state.msaa_tex->Release();
    }

    if (win32_state.msaa_view != nullptr) {
        win32_state.msaa_view->Release();
    }
}

internal
void d3d11_update_default_render_targets(void) {
    if (win32_state.swap_chain) {
        d3d11_destroy_default_render_targets();
        win32_state.swap_chain->ResizeBuffers(2, win32_state.width, win32_state.height, DXGI_FORMAT_B8G8R8A8_UNORM, 0);
        d3d11_create_default_render_targets();
    }
}

internal
void d3d11_shutdown() {
    FreeConsole();
    d3d11_destroy_default_render_targets();
    win32_state.swap_chain->Release();
    win32_state.device_context->Release();
    win32_state.device->Release();
    DestroyWindow(win32_state.hwnd); 
    win32_state.hwnd = 0;
    UnregisterClassW(L"SOKOLD3D11", GetModuleHandleW(NULL));
}

internal 
void platform_stdout(const char *text, i64 length) {
    assert(length >= 0);

    if(length == 0) return;

    assert(text != nullptr);
    WriteConsoleA(win32_state.std_out, text, (DWORD)length, NULL, NULL); // TODO: figure out what DWORD is and see if I care
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    game_main();
    d3d11_shutdown();
    return 0;
}
