#include "PingPong.h"

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
int screenWidth = GetSystemMetrics(SM_CXSCREEN);
int screenHeight = GetSystemMetrics(SM_CYSCREEN);
int taskbarHeight;
RECT b;
Ball ball(screenWidth / 2, screenHeight / 2);

int WINAPI WinMain(HINSTANCE currentInstance, HINSTANCE previousInstance, PSTR cmdLine, INT cmdCount) {
	// Ignore the return value because we want to run the program even in the
	// unlikely event that HeapSetInformation fails.
	HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);

	if (SUCCEEDED(CoInitialize(NULL)))
	{
		{
			PingPongApp app;

			if (SUCCEEDED(app.init()))
			{
				Beep(750, 800);

				app.RunMessageLoop();
				app.DiscardDeviceResources();
			}
		}
		CoUninitialize();
	}

	return 0;
}

PingPongApp::PingPongApp()
{
	//m_hwnd(nullptr);
}

PingPongApp::~PingPongApp()
{
	//SafeRelease(m_hwnd);
}

HRESULT PingPongApp::init()
{
	HRESULT hr;
	std::wstring CLASS_NAME = L"window";
	LPCWSTR NAME;
	NAME = CLASS_NAME.c_str();
	// Initialize device-indpendent resources
	Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

	HBRUSH brush = CreateSolidBrush(RGB(255, 255, 255));
	// Register the window class.
	WNDCLASSEX wcex = { sizeof(WNDCLASSEX) };
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = PingPongApp::WindowProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = sizeof(LONG_PTR);
	wcex.hInstance = HINST_THISCOMPONENT;
	wcex.hbrBackground = brush;
	wcex.lpszMenuName = NULL;
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.lpszClassName = CLASS_NAME.c_str();

	::RegisterClassEx(&wcex);

	// Create the application window.

	m_hwnd_owner = CreateWindow(NAME, "PingPongBall",
		WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
		CW_USEDEFAULT, CW_USEDEFAULT,
		0, 0,
		nullptr, nullptr, nullptr, nullptr);
	hr = m_hwnd_owner ? S_OK : E_FAIL;

	m_hwnd = CreateWindow(CLASS_NAME.c_str(), "PingPongBall",
		WS_EX_TOOLWINDOW | WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		800, 600,
		nullptr, nullptr, nullptr, nullptr);
	hr = m_hwnd ? S_OK : E_FAIL;

	if (SUCCEEDED(hr))
	{
		SetWindowLong(m_hwnd, GWL_EXSTYLE, GetWindowLong(m_hwnd, GWL_EXSTYLE) | WS_EX_LAYERED);
		SetLayeredWindowAttributes(m_hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);
		ShowWindow(m_hwnd, SW_MAXIMIZE);
		LONG cur_style = GetWindowLong(m_hwnd, GWL_EXSTYLE);
		SetWindowLong(m_hwnd, GWL_EXSTYLE, cur_style | WS_EX_TRANSPARENT | WS_EX_LAYERED);
		SetWindowLongPtr(m_hwnd, GWLP_HWNDPARENT, (LONG)m_hwnd_owner);
		SetWindowPos(m_hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	}

	return hr;
}

void PingPongApp::RunMessageLoop()
{
	MSG msg{};
	while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
		if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN) {
			continue;
		}
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

bool PingPongApp::isFullscreen(HWND window)
{
	RECT a;
	GetWindowRect(window, &a);
	return (a.left == b.left &&
		a.top == b.top &&
		a.right == b.right &&
		a.bottom == b.bottom);
}

void PingPongApp::DiscardDeviceResources()
{
	Gdiplus::GdiplusShutdown(gdiplusToken);
}

HRESULT PingPongApp::OnRender()
{
	return E_NOTIMPL;
}

void PingPongApp::OnResize(UINT width, UINT height)
{
}

LRESULT PingPongApp::WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	wprm = wParam;
	lprm = lParam;

	std::this_thread::sleep_for(std::chrono::milliseconds(1));
	switch (message) {

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

			PostMessage(m_hwnd, WM_CLOSE, wprm, lprm);

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

		if (hwnd != m_hwnd && hwnd != m_hwnd_owner && !IsIconic(hwnd)) {

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
		return DefWindowProc(hwnd, message, wParam, lParam);
	}
	}
}
