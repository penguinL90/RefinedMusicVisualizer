#include "pch.h"
#include "MainWindow.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR pCmdLine, int nCmdShow)
{
    NodeOptions n
    {
        .Gap = 6,
        .MaxLength = 60,
        .MinLength = 2,
        .Width = 5,
        .Count = 121,
        .DampingRate = 0.025,
        .Color = { .95f, .95f, .95f, 1 }
    };

    ProcessOptions p
    {
        .FFTSampleSizeExp2 = 12,
        .MaxSoundIndexExp10 = 2.5,
        .MinSoundIndexExp10 = -0.1,
        .MaxFrequency = 12000,
        .MinFrequency = 60,
        .ProcessRate = 30,
    };

    MainWindow window(781, 140, n, p);

    return window.Run(nCmdShow);
}
