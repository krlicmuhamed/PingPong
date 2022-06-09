#include <Windows.h>
#include <cassert>

#include <iostream>
#include <vector>
#include <stdlib.h>
#include <time.h>   
#include <cmath>
#include <thread>
#include <chrono>

#include <gdiplus.h>
#include <d3d12.h>
#include <d2d1.h>
#include <dxgi1_4.h>


using namespace std;

#define DX12_ENABLE_DEBUG_LAYER 0

struct FrameContext
{
    ID3D12CommandAllocator* CommandAllocator;
    UINT64                  FenceValue;
};

// Data
static int const                    NUM_FRAMES_IN_FLIGHT = 3;
static FrameContext                 g_frameContext[NUM_FRAMES_IN_FLIGHT] = {};
static UINT                         g_frameIndex = 0;

static int const                    NUM_BACK_BUFFERS = 3;
static ID3D12Device*                g_pd3dDevice = NULL;
static ID3D12DescriptorHeap*        g_pd3dRtvDescHeap = NULL;
static ID3D12DescriptorHeap*        g_pd3dSrvDescHeap = NULL;
static ID3D12CommandQueue*          g_pd3dCommandQueue = NULL;
static ID3D12GraphicsCommandList*   g_pd3dCommandList = NULL;
static ID3D12Fence*                 g_fence = NULL;
static HANDLE                       g_fenceEvent = NULL;
static UINT64                       g_fenceLastSignaledValue = 0;
static IDXGISwapChain3*             g_pSwapChain = NULL;
static HANDLE                       g_hSwapChainWaitableObject = NULL;
static ID3D12Resource*              g_mainRenderTargetResource[NUM_BACK_BUFFERS] = {};
static D3D12_CPU_DESCRIPTOR_HANDLE  g_mainRenderTargetDescriptor[NUM_BACK_BUFFERS] = {};

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
void WaitForLastSubmittedFrame();
FrameContext* WaitForNextFrameResources();
void ResizeSwapChain(HWND hWnd, int width, int height);

// Helper functions

template<class Interface>
inline void SafeRelease(Interface *& pInterfaceToRelease)
{
	if (pInterfaceToRelease != NULL)
	{
		pInterfaceToRelease->Release();
		pInterfaceToRelease = NULL;
	}
}

bool CreateDeviceD3D(HWND hWnd)
{
    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC1 sd;
    {
        ZeroMemory(&sd, sizeof(sd));
        sd.BufferCount = NUM_BACK_BUFFERS;
        sd.Width = 0;
        sd.Height = 0;
        sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.SampleDesc.Count = 1;
        sd.SampleDesc.Quality = 0;
        sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        sd.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
        sd.Scaling = DXGI_SCALING_STRETCH;
        sd.Stereo = FALSE;
    }

    if (DX12_ENABLE_DEBUG_LAYER)
    {
        ID3D12Debug* dx12Debug = NULL;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&dx12Debug))))
        {
            dx12Debug->EnableDebugLayer();
            dx12Debug->Release();
        }
    }

    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    if (D3D12CreateDevice(NULL, featureLevel, IID_PPV_ARGS(&g_pd3dDevice)) != S_OK)
        return false;

    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        desc.NumDescriptors = NUM_BACK_BUFFERS;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        desc.NodeMask = 1;
        if (g_pd3dDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_pd3dRtvDescHeap)) != S_OK)
            return false;

        SIZE_T rtvDescriptorSize = g_pd3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = g_pd3dRtvDescHeap->GetCPUDescriptorHandleForHeapStart();
        for (UINT i = 0; i < NUM_BACK_BUFFERS; i++)
        {
            g_mainRenderTargetDescriptor[i] = rtvHandle;
            rtvHandle.ptr += rtvDescriptorSize;
        }
    }

    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.NumDescriptors = 1;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        if (g_pd3dDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_pd3dSrvDescHeap)) != S_OK)
            return false;
    }

    {
        D3D12_COMMAND_QUEUE_DESC desc = {};
        desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        desc.NodeMask = 1;
        if (g_pd3dDevice->CreateCommandQueue(&desc, IID_PPV_ARGS(&g_pd3dCommandQueue)) != S_OK)
            return false;
    }

    for (UINT i = 0; i < NUM_FRAMES_IN_FLIGHT; i++)
        if (g_pd3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_frameContext[i].CommandAllocator)) != S_OK)
            return false;

    if (g_pd3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_frameContext[0].CommandAllocator, NULL, IID_PPV_ARGS(&g_pd3dCommandList)) != S_OK ||
        g_pd3dCommandList->Close() != S_OK)
        return false;

    if (g_pd3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_fence)) != S_OK)
        return false;

    g_fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (g_fenceEvent == NULL)
        return false;

    {
        IDXGIFactory4* dxgiFactory = NULL;
        IDXGISwapChain1* swapChain1 = NULL;
        if (CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory)) != S_OK ||
            dxgiFactory->CreateSwapChainForHwnd(g_pd3dCommandQueue, hWnd, &sd, NULL, NULL, &swapChain1) != S_OK ||
            swapChain1->QueryInterface(IID_PPV_ARGS(&g_pSwapChain)) != S_OK)
            return false;
        swapChain1->Release();
        dxgiFactory->Release();
        g_pSwapChain->SetMaximumFrameLatency(NUM_BACK_BUFFERS);
        g_hSwapChainWaitableObject = g_pSwapChain->GetFrameLatencyWaitableObject();
    }

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = NULL; }
    if (g_hSwapChainWaitableObject != NULL) { CloseHandle(g_hSwapChainWaitableObject); }
    for (UINT i = 0; i < NUM_FRAMES_IN_FLIGHT; i++)
        if (g_frameContext[i].CommandAllocator) { g_frameContext[i].CommandAllocator->Release(); g_frameContext[i].CommandAllocator = NULL; }
    if (g_pd3dCommandQueue) { g_pd3dCommandQueue->Release(); g_pd3dCommandQueue = NULL; }
    if (g_pd3dCommandList) { g_pd3dCommandList->Release(); g_pd3dCommandList = NULL; }
    if (g_pd3dRtvDescHeap) { g_pd3dRtvDescHeap->Release(); g_pd3dRtvDescHeap = NULL; }
    if (g_pd3dSrvDescHeap) { g_pd3dSrvDescHeap->Release(); g_pd3dSrvDescHeap = NULL; }
    if (g_fence) { g_fence->Release(); g_fence = NULL; }
    if (g_fenceEvent) { CloseHandle(g_fenceEvent); g_fenceEvent = NULL; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
}

void CreateRenderTarget()
{
    for (UINT i = 0; i < NUM_BACK_BUFFERS; i++)
    {
        ID3D12Resource* pBackBuffer = NULL;
        g_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer));
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, g_mainRenderTargetDescriptor[i]);
        g_mainRenderTargetResource[i] = pBackBuffer;
    }
}

void CleanupRenderTarget()
{
    WaitForLastSubmittedFrame();

    for (UINT i = 0; i < NUM_BACK_BUFFERS; i++)
        if (g_mainRenderTargetResource[i]) { g_mainRenderTargetResource[i]->Release(); g_mainRenderTargetResource[i] = NULL; }
}

void WaitForLastSubmittedFrame()
{
    FrameContext* frameCtxt = &g_frameContext[g_frameIndex % NUM_FRAMES_IN_FLIGHT];

    UINT64 fenceValue = frameCtxt->FenceValue;
    if (fenceValue == 0)
        return; // No fence was signaled

    frameCtxt->FenceValue = 0;
    if (g_fence->GetCompletedValue() >= fenceValue)
        return;

    g_fence->SetEventOnCompletion(fenceValue, g_fenceEvent);
    WaitForSingleObject(g_fenceEvent, INFINITE);
}

FrameContext* WaitForNextFrameResources()
{
    UINT nextFrameIndex = g_frameIndex + 1;
    g_frameIndex = nextFrameIndex;

    HANDLE waitableObjects[] = { g_hSwapChainWaitableObject, NULL };
    DWORD numWaitableObjects = 1;

    FrameContext* frameCtxt = &g_frameContext[nextFrameIndex % NUM_FRAMES_IN_FLIGHT];
    UINT64 fenceValue = frameCtxt->FenceValue;
    if (fenceValue != 0) // means no fence was signaled
    {
        frameCtxt->FenceValue = 0;
        g_fence->SetEventOnCompletion(fenceValue, g_fenceEvent);
        waitableObjects[1] = g_fenceEvent;
        numWaitableObjects = 2;
    }

    WaitForMultipleObjects(numWaitableObjects, waitableObjects, TRUE, INFINITE);

    return frameCtxt;
}

void ResizeSwapChain(HWND hWnd, int width, int height)
{
    DXGI_SWAP_CHAIN_DESC1 sd;
    g_pSwapChain->GetDesc1(&sd);
    sd.Width = width;
    sd.Height = height;

    IDXGIFactory4* dxgiFactory = NULL;
    g_pSwapChain->GetParent(IID_PPV_ARGS(&dxgiFactory));

    g_pSwapChain->Release();
    CloseHandle(g_hSwapChainWaitableObject);

    IDXGISwapChain1* swapChain1 = NULL;
    dxgiFactory->CreateSwapChainForHwnd(g_pd3dCommandQueue, hWnd, &sd, NULL, NULL, &swapChain1);
    swapChain1->QueryInterface(IID_PPV_ARGS(&g_pSwapChain));
    swapChain1->Release();
    dxgiFactory->Release();

    g_pSwapChain->SetMaximumFrameLatency(NUM_BACK_BUFFERS);

    g_hSwapChainWaitableObject = g_pSwapChain->GetFrameLatencyWaitableObject();
    assert(g_hSwapChainWaitableObject != NULL);
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

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
    //Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    //ULONG_PTR gdiplusToken;
    //Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

    const char* CLASS_NAME = "window";
    WNDCLASS wc{};
    wc.hInstance = currentInstance; // current instance
    wc.lpszClassName = CLASS_NAME; // class name
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW); // cursor
    HBRUSH brush = CreateSolidBrush(RGB(255, 255, 255));
    wc.hbrBackground = brush; // background color
    wc.lpfnWndProc = WindowProcessMessages; //this is the function that will be called when a message is sent to the window
    ::RegisterClass(&wc); // register the window class

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

    // Initialize Direct3D
    if (!CreateDeviceD3D(hWnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClass(wc.lpszClassName, wc.hInstance);
        return 1;
    }

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

    WaitForLastSubmittedFrame();

    CleanupDeviceD3D();
    ::DestroyWindow(hWnd);
    ::UnregisterClass(wc.lpszClassName, wc.hInstance);
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
        HGDIOBJ original = nullptr;
        //Gdiplus::Graphics graphics(hdcMem);
        //graphics.Clear(Gdiplus::Color(255, 0, 0, 0));

        //Gdiplus::SolidBrush brush(Gdiplus::Color(255, 65, 125, 245));
        //graphics.FillEllipse(&brush, ball.x, ball.y, float(ball.rad), float(ball.rad));

        // Direct2D stuff
        ID2D1Factory* g_pD2DFactory;
        HRESULT hr = D2D1CreateFactory(
            D2D1_FACTORY_TYPE_SINGLE_THREADED,
            &g_pD2DFactory
        );

        // Create a Direct2D render target          
        ID2D1HwndRenderTarget* pRT = NULL;
        hr = g_pD2DFactory->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(),
            D2D1::HwndRenderTargetProperties(
                hWnd,
                D2D1::SizeU(
                    b.right - b.left,
                    b.bottom - b.top)
            ),
            &pRT
        );
		
        ID2D1SolidColorBrush* pBlackBrush = NULL;
        if (SUCCEEDED(hr))
        {

            pRT->CreateSolidColorBrush(
                D2D1::ColorF(D2D1::ColorF::Black),
                &pBlackBrush
            );
        }

        pRT->BeginDraw();

        pRT->DrawEllipse(
			D2D1::Ellipse(
				D2D1::Point2F(ball.x, ball.y),
				float(ball.rad),
				float(ball.rad)
			),
			pBlackBrush
		);

		
        //BitBlt(hdc, 0, 0, screenWidth, screenHeight, hdcMem, 0, 0, SRCCOPY);
        //BitBlt(hdc, 0, 0, screenWidth, screenHeight, hdcMem, 0, 0, SRCAND);
        SelectObject(hdcMem, hOld);

        ::SafeRelease(pRT);
        ::SafeRelease(pBlackBrush);
        ::SafeRelease(g_pD2DFactory);
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