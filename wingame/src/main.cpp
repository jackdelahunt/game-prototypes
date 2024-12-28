#include <windows.h>
#include <stdio.h>

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM w_param, LPARAM l_param) {
    switch (message) {
        case WM_DESTROY: {
            PostQuitMessage(0);
            return 0;
        }
        case WM_SIZE: {
            int width = LOWORD(l_param);
            int height = HIWORD(l_param);
    
            OutputDebugStringA("Resizing\n");
            break;
        }
        case WM_PAINT: {
            PAINTSTRUCT painter;
            HDC context = BeginPaint(window, &painter);
    
            int x = painter.rcPaint.left;
            int y = painter.rcPaint.top;
            int w = painter.rcPaint.right - painter.rcPaint.left;
            int h = painter.rcPaint.bottom - painter.rcPaint.top;

            PatBlt(context, x, y, w, h, BLACKNESS); 
            
            EndPaint(window, &painter);
    
            OutputDebugStringA("Resizing\n");
            break;
        }
    }

    return DefWindowProcA(window, message, w_param, l_param);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous_instance, PSTR cmd_args, int nCmdShow)
{
    WNDCLASS window_class = {};
    window_class.lpfnWndProc = WindowProc;
    window_class.hInstance = instance;
    window_class.lpszClassName = "game";

    RegisterClassA(&window_class);

    HWND window = CreateWindowExA(
        0, 
        window_class.lpszClassName, 
        window_class.lpszClassName,
        WS_OVERLAPPEDWINDOW, 
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 
        NULL,
        NULL,
        instance,
        NULL
    );

    if (window == NULL)
    {
        return 0;
    }

    ShowWindow(window, nCmdShow);

    // Run the message loop.

    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
