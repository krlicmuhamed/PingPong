#include <windows.h>
#include <iostream>
#include <vector>
#include <stdlib.h>
#include <time.h>   
#include <gdiplus.h>
#include <cmath>
#include <thread>
#include <chrono>

using namespace std;

class Ball {

public:
    float x;
    float y;
    float xvel;
    float yvel;
    int rad = 20;

    Ball(float x, float y) {

        srand(time(NULL));
        this->x = x;
        this->y = y;
        this->xvel = -sin(rand() % 359) * 5;
        this->yvel = cos(rand() % 359) * 5;

    }

};

vector<HWND> list;
LRESULT CALLBACK WindowProcessMessages(HWND hwnd, UINT msg, WPARAM param, LPARAM lparam);
HWND hWnd;
HWND hwndowner;
int screenWidth = GetSystemMetrics(SM_CXSCREEN);
int screenHeight = GetSystemMetrics(SM_CYSCREEN);
int taskbarHeight;
POINT cursorPos;
HDC hdc;
PAINTSTRUCT ps;
WPARAM wprm;
LPARAM lprm;
RECT b;


//make window stuff
int WINAPI WinMain(HINSTANCE currentInstance, HINSTANCE previousInstance, PSTR cmdLine, INT cmdCount) {
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

    const char* CLASS_NAME = "window";
    WNDCLASS wc{};
    wc.hInstance = currentInstance; // current instance
    wc.lpszClassName = CLASS_NAME; // class name
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW); // cursor
    HBRUSH brush = CreateSolidBrush(RGB(255, 255, 255));
    wc.hbrBackground = brush; // background color
    wc.lpfnWndProc = WindowProcessMessages; //this is the function that will be called when a message is sent to the window
    RegisterClass(&wc); // register the window class

    hwndowner = CreateWindow(CLASS_NAME, "PingPongBall",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT,
        0, 0,
        nullptr, nullptr, nullptr, nullptr);


    hWnd = CreateWindow(CLASS_NAME, "PingPongBall",
        WS_EX_TOOLWINDOW | WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT,
        800, 600,
        nullptr, nullptr, nullptr, nullptr);

    SetWindowLong(hWnd, GWL_EXSTYLE, GetWindowLong(hWnd, GWL_EXSTYLE) | WS_EX_LAYERED);
    SetLayeredWindowAttributes(hWnd, RGB(0, 0, 0), 0, LWA_COLORKEY);
    ShowWindow(hWnd, SW_MAXIMIZE);
    LONG cur_style = GetWindowLong(hWnd, GWL_EXSTYLE);
    SetWindowLong(hWnd, GWL_EXSTYLE, cur_style | WS_EX_TRANSPARENT | WS_EX_LAYERED);
    SetWindowLongPtr(hWnd, GWLP_HWNDPARENT, (LONG)hwndowner);
    SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

    MSG msg{};
	// thread sleep for 3 seconds
	// this thread, sleep 3000 milliseconds
	//std::this_thread::sleep_for(std::chrono::milliseconds(3000));

    Beep(750, 800);

    //while (GetMessage(&msg, nullptr, 0, 0)) {
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        // chrono sleep for 10 ms
		// this thread, sleep 10 milliseconds
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN) {
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    Gdiplus::GdiplusShutdown(gdiplusToken);
    return 0;
}

Ball ball(screenWidth / 2, screenHeight / 2);

bool isFullscreen(HWND window)
{
    RECT a;
    GetWindowRect(window, &a);
    return (a.left == b.left &&
        a.top == b.top &&
        a.right == b.right &&
        a.bottom == b.bottom);
}


LRESULT CALLBACK WindowProcessMessages(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	
    wprm = wparam;
    lprm = lprm;
	
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

    switch (msg) {

    case WM_CREATE:
    {

        SetTimer(hwnd, 1, 11, NULL);
        GetWindowRect(GetDesktopWindow(), &b);
        RECT rect;
        SystemParametersInfo(SPI_GETWORKAREA, 0, &rect, 0);
        taskbarHeight = screenHeight - rect.bottom; //height w/o taskbar

    }

    case WM_TIMER:
    {

        InvalidateRect(hwnd, NULL, FALSE);

        if (GetAsyncKeyState(VK_TAB) < 0) {

            PostMessage(hWnd, WM_CLOSE, wprm, lprm);

        }

        //check if ball is going outside screen area
        if (ball.x + ball.xvel < 0 || ball.x + ball.rad + ball.xvel > screenWidth) {

            ball.xvel *= -1;
            //Beep(750, 800);

        }

        if (ball.y + ball.yvel < 0 || ball.y + ball.rad + ball.yvel > screenHeight - taskbarHeight) {

            ball.yvel *= -1;
            //Beep(750, 800);

        }

        HWND hwnd = GetForegroundWindow();
		
        if (hwnd != hWnd && hwnd != hwndowner && !IsIconic(hwnd)) {

            //if (!IsWindowVisible(hwnd))
            //    continue;

            //int length = GetWindowTextLength(hwnd);
            //if (length == 0)
            //    continue;

            if (!isFullscreen(hwnd)) {

                RECT rect;
                GetWindowRect(hwnd, &rect);
                //horizontal collision
                if (ball.y + ball.yvel + ball.rad > rect.top && ball.y + ball.yvel < rect.bottom) {

                    //left collision
                    if (ball.xvel > 0 && ball.x + ball.xvel + ball.rad > rect.left && ball.x + ball.xvel + ball.rad <= rect.left + ball.xvel) {

                        ball.xvel *= -1;
                        //Beep(750, 800);
                    }
                    //right collision

                    if (ball.xvel < 0 && ball.x + ball.xvel < rect.right && ball.x + ball.xvel >= rect.right + ball.xvel) {

                        ball.xvel *= -1;
                        //Beep(750, 800);
                    }

                }

                //vertical collision
                if (ball.x + ball.xvel + ball.rad > rect.left && ball.x + ball.xvel < rect.right) {

                    //top collision
                    if (ball.yvel > 0 && ball.y + ball.yvel + ball.rad > rect.top && ball.y + ball.yvel + ball.rad <= rect.top + ball.yvel) {

                        ball.yvel *= -1;
                        //Beep(750, 800);

                    }

                    //bottom collision
                    if (ball.yvel < 0 && ball.y + ball.yvel < rect.bottom && ball.y + ball.yvel >= rect.bottom + ball.yvel) {

                        ball.yvel *= -1;
                        //Beep(750, 800);

                    }

                }

            }

        }

        ball.x += ball.xvel;
        ball.y += ball.yvel;

    }
    case WM_ERASEBKGND:
    {
        return true;

    }
    case WM_PAINT:  
    {

        hdc = BeginPaint(hwnd, &ps);
        HDC hdcMem = CreateCompatibleDC(hdc);
        HBITMAP hbmMem = CreateCompatibleBitmap(hdc, screenWidth, screenHeight);
        HBITMAP hOld = (HBITMAP)SelectObject(hdcMem, hbmMem);
        Gdiplus::Graphics graphics(hdcMem);
        graphics.Clear(Gdiplus::Color(255, 0, 0, 0));

        Gdiplus::SolidBrush brush(Gdiplus::Color(255, 65, 125, 245));
        graphics.FillEllipse(&brush, ball.x, ball.y, float(ball.rad), float(ball.rad));

        BitBlt(hdc, 0, 0, screenWidth, screenHeight, hdcMem, 0, 0, SRCCOPY);
        SelectObject(hdcMem, hOld);
        DeleteObject(hbmMem);
        DeleteDC(hdcMem);
        EndPaint(hwnd, &ps);

        return 0;

    }

    case WM_DESTROY:
    {
        PostQuitMessage(0);
        return 0;
    }
    default:
    {
        return DefWindowProc(hwnd, msg, wparam, lparam);
    }
    }
}