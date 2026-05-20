#include "pch.h"
#include "MainWindow.h"

using namespace std::chrono_literals;

MainWindow::MainWindow(UINT width, UINT height, NodeOptions const& _nodeOptions, ProcessOptions const& _processOptions) :
    nodeOptions(_nodeOptions), processOptions(_processOptions)
{
    HMODULE hInstance = GetModuleHandleW(NULL);
    WNDCLASSEX wc
    {
        .cbSize = sizeof(WNDCLASSEX),
        .lpfnWndProc = windowProc,
        .hInstance = hInstance,
        .hIcon = LoadIconA(hInstance, MAKEINTRESOURCEA(IDI_ICON1)),
        .hbrBackground = NULL,
        .lpszClassName = ClassName(),
        .hIconSm = (HICON)LoadImageA(hInstance, MAKEINTRESOURCEA(IDI_ICON1), IMAGE_ICON, 16, 16, 0),
    };

    DWORD style = WS_MINIMIZEBOX | WS_SYSMENU;
    DWORD exStyle = WS_EX_TOPMOST | WS_EX_NOREDIRECTIONBITMAP;
    RECT rect{ 0, 0, width, height };
    AdjustWindowRectEx(&rect, style, FALSE, exStyle);

    width = rect.right - rect.left;
    height = rect.bottom - rect.top;

    RegisterClassExW(&wc);
    hwnd = CreateWindowExW
    (
        WS_EX_TOPMOST | WS_EX_NOREDIRECTIONBITMAP,
        ClassName(),
        L"Refined Music Visualizer Win32",
        WS_MINIMIZEBOX | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, width, height,
        NULL,
        NULL,
        hInstance,
        this
    );
}

MainWindow::~MainWindow()
{

}

int MainWindow::Run(int nCmdShow)
{
    if (!hwnd)
    {
        //
        return -1;
    }

    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hr))
    {
        //
        return -1;
    }

    ShowWindow(hwnd, nCmdShow);
    MSG msg{};
    while (GetMessageW(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    CoUninitialize();
    return 0;
}

LRESULT MainWindow::windowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    MainWindow* pThis = NULL;
    if (uMsg == WM_CREATE)
    {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
        pThis = (MainWindow*)pCreate->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
        pThis->hwnd = hwnd;
    }
    else
        pThis = (MainWindow*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (pThis)
        return pThis->messageHandler(uMsg, wParam, lParam);
    else
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT MainWindow::messageHandler(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    try
    {
        switch (uMsg)
        {
        case WM_CREATE:
        {
            DWM_SYSTEMBACKDROP_TYPE bdType = DWMSBT_TRANSIENTWINDOW;
            BOOL enableDarkMode = TRUE;
            DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &bdType, sizeof(bdType));
            DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &enableDarkMode, sizeof(enableDarkMode));

            RECT rc{};
            GetClientRect(hwnd, &rc);
            UINT w = rc.right - rc.left, h = rc.bottom - rc.top;

            pProcessCore = std::make_unique<ProcessCore>(hwnd, w, h, nodeOptions, processOptions);

            break;
        }
        case WM_ERROR_FROM_OTHER_THREAD:
        {

            char* mes = reinterpret_cast<char*>(wParam);
            std::unique_ptr<char> pMes{ mes };
            throw std::runtime_error(mes);
        }
        case WM_DESTROY:
        {
            PostQuitMessage(0);
            break;
        }
        case WM_NCACTIVATE:
        {
            if (wParam == FALSE)
                return DefWindowProcW(hwnd, uMsg, TRUE, lParam);
        }
        default:
        {
            return DefWindowProcW(hwnd, uMsg, wParam, lParam);
        }
        }
    }
    catch (const std::exception& e)
    {
        const char* str = e.what();
        if (str)
        {
            int len = (int)strlen(str);
            int size = MultiByteToWideChar(CP_UTF8, 0, str, len, NULL, 0);
            std::wstring message(size, 0);
            MultiByteToWideChar(CP_UTF8, 0, str, len, &message[0], size);
            MessageBoxW(hwnd, (L"Error: " + message).c_str(), L"錯誤", MB_ICONERROR | MB_OK);
        }
        PostQuitMessage(0);
    }
}
