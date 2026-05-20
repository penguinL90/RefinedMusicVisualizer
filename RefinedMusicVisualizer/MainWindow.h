#pragma once
#include "DrawCore.h"
#include "NodeOptions.h"
#include "ProcessOptions.h"
#include "ProcessCore.h"

struct MainWindow
{
    MainWindow(UINT width, UINT height, NodeOptions const& nodeOptions, ProcessOptions const& processOptions);
    ~MainWindow();
    MainWindow(const MainWindow&) = delete;
    MainWindow(MainWindow&&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;
    MainWindow& operator=(MainWindow&) = delete;
    int Run(int nCmdShow);
    PCWSTR ClassName() const { return L"ParticlesWin32"; }
private:
    HWND hwnd;
    static LRESULT CALLBACK windowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT messageHandler(UINT uMsg, WPARAM wParam, LPARAM lParam);

    NodeOptions nodeOptions;
    ProcessOptions processOptions;

    std::unique_ptr<ProcessCore> pProcessCore;
};
