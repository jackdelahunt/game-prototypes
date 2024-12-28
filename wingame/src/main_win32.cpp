#include "platform_win32.h"

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
static void d3d11_create_default_render_targets(void);
static void d3d11_destroy_default_render_targets(void);
static void d3d11_update_default_render_targets(void);

static const IID _d3d11entry_IID_ID3D11Texture2D = { 0x6f15aaf2,0xd208,0x4e89,0x9a,0xb4,0x48,0x95,0x35,0xd3,0x4f,0x9c };

#define _d3d11_def(val, def) (((val) == 0) ? (def) : (val))

struct State {
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

State state = {};

void d3d11_init(const d3d11_desc_t* desc) {
    assert(desc);
    assert(desc->width > 0);
    assert(desc->height > 0);
    assert(desc->title);

    state.win_style = WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SIZEBOX;
    state.win_ex_style = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;

    d3d11_desc_t desc_def = *desc;
    desc_def.sample_count = _d3d11_def(desc_def.sample_count, 1);

    state.width = desc_def.width;
    state.height = desc_def.height;
    state.sample_count = desc_def.sample_count;
    state.no_depth_buffer = desc_def.no_depth_buffer;

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
        desc_def.title,     // lpWindowName
        state.win_style,    // dwStyle
        CW_USEDEFAULT,      // X
        CW_USEDEFAULT,      // Y
        win_width,          // nWidth
        win_height,         // nHeight
        NULL,               // hWndParent
        NULL,               // hMenu
        GetModuleHandle(NULL),  //hInstance
        NULL);              // lpParam
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

void d3d11_shutdown(void) {
    d3d11_destroy_default_render_targets();
    state.swap_chain->Release();
    state.device_context->Release();
    state.device->Release();
    DestroyWindow(state.hwnd); state.hwnd = 0;
    UnregisterClassW(L"SOKOLD3D11", GetModuleHandleW(NULL));
}

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

void d3d11_update_default_render_targets(void) {
    if (state.swap_chain) {
        d3d11_destroy_default_render_targets();
        state.swap_chain->ResizeBuffers(2, state.width, state.height, DXGI_FORMAT_B8G8R8A8_UNORM, 0);
        d3d11_create_default_render_targets();
    }
}

bool d3d11_process_events(void) {
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

void d3d11_present(void) {
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

static const void* d3d11_device(void) {
    return (const void*) state.device;
}

static const void* d3d11_device_context(void) {
    return (const void*) state.device_context;
}

sg_environment d3d11_environment(void) {
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

sg_swapchain d3d11_swapchain(void) {
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

int d3d11_width(void) {
    return state.width;
}

int d3d11_height() {
    return state.height;
}

/* register input callbacks */
void d3d11_key_down(d3d11_key_func f) {
    state.key_down_func = f;
}

void d3d11_key_up(d3d11_key_func f) {
    state.key_up_func = f;
}

void d3d11_char(d3d11_char_func f) {
    state.char_func = f;
}

void d3d11_mouse_btn_down(d3d11_mouse_btn_func f) {
    state.mouse_btn_down_func = f;
}

void d3d11_mouse_btn_up(d3d11_mouse_btn_func f) {
    state.mouse_btn_up_func = f;
}

void d3d11_mouse_pos(d3d11_mouse_pos_func f) {
    state.mouse_pos_func = f;
}

void d3d11_mouse_wheel(d3d11_mouse_wheel_func f) {
    state.mouse_wheel_func = f;
}

bool process_events() {
    return d3d11_process_events();
}

void present() {
    d3d11_present();
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // setup the D3D11 app wrapper
    d3d11_desc_t d3d_description = {};
    d3d_description.width = 640;
    d3d_description.height = 480;
    d3d_description.title = L"Hello";

    d3d11_init(&d3d_description);

    // setup sokol
    sg_desc sg_description {};
    sg_description.logger.func = slog_func;
    sg_description.environment = d3d11_environment();

    sg_setup(&sg_description);

    start();

    // initial pass action: clear to red
    sg_pass_action pass_action = {};
    pass_action.colors[0].load_action = SG_LOADACTION_CLEAR;
    pass_action.colors[0].clear_value = { 1.0f, 0.0f, 0.0f, 1.0f };

    // draw loop
    while (d3d11_process_events()) {

        float g = pass_action.colors[0].clear_value.g + 0.01f;
        if (g > 1.0f) g = 0.0f;

        sg_pass pass = {};
        pass.action = pass_action;
        pass.swapchain = d3d11_swapchain();

        pass_action.colors[0].clear_value.g = g;
        sg_begin_pass(&pass);

        sg_end_pass();
        sg_commit();
        d3d11_present();
    }

    // shutdown sokol_gfx and D3D11 app wrapper
    sg_shutdown();
    d3d11_shutdown();
    return 0;
}
