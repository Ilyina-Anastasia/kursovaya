// Чтобы функции WinAPI принимали строки wchar_t*, а не char*
#ifndef UNICODE
#define UNICODE
#endif

#include "MyOGL.h"

#include <windows.h>

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// Функция main под Windows
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    // Регистрируем класс окна
    const wchar_t CLASS_NAME[] = L"Window Class";

    WNDCLASS wc = {};

    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    // Создаём окно
	HWND hwnd = CreateWindowEx(0,                    // Опциональные стили
                               CLASS_NAME,           // Класс окна
                               L"Лабораторка по КГ", // Имя окна
                               WS_OVERLAPPEDWINDOW,  // Стиль окна

                               // Размер и позиция
                               CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,

                               NULL,      // Предшествующее окно
                               NULL,      // Меню
                               hInstance, // Управление экземпляром
                               NULL       // Дополнительная информация
    );

    if (hwnd == NULL)
    {
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);

    // Run the message loop.

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

bool trackMouse = false;

LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Message m = {uMsg, wParam, lParam};

    switch (uMsg)
    {
    case WM_CREATE: {
        setHwnd(hWnd);
        start_msg_thread();
        start_gl_thread();

        return 0;
    }

    case WM_MOUSELEAVE: {
        trackMouse = false;
        add_message(m);
        return 0;
    }

    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN: {
        SetCapture(hWnd);
        add_message(m);
        return 0;
    }
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP: {
        ReleaseCapture();
        add_message(m);
        return 0;
    }

    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_MOUSEWHEEL:
        add_message(m);
        return 0;
    case WM_MOUSEMOVE: {
        if (!trackMouse)
        {
            TRACKMOUSEEVENT tme_arg;
            tme_arg.cbSize = sizeof(TRACKMOUSEEVENT);
            tme_arg.dwFlags = TME_LEAVE;
            tme_arg.dwHoverTime = HOVER_DEFAULT;
            tme_arg.hwndTrack = hWnd;
            trackMouse = TrackMouseEvent(&tme_arg);
        }
        add_message(m);
        return 0;
    }
    case WM_SIZE:
        add_message(m);
        return 0;

    case WM_CLOSE:
        add_message(m);
        stop_all_threads();
        DestroyWindow(hWnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        // All painting occurs here, between BeginPaint and EndPaint.

        // FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));

        EndPaint(hWnd, &ps);
    }
        return 0;
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}
