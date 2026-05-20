#pragma once
#include "DrawCore.h"
#include "VectorPool.h"
#include "ProcessOptions.h"
#include "FloatCircularBuffer.h"
#include <mmdeviceapi.h>
#include <Audioclient.h>

struct ProcessCore
{
    

    ProcessCore(HWND hwnd, UINT width, UINT height, NodeOptions const& nodeOptions, ProcessOptions const& processOptions);
    ~ProcessCore();
    ProcessCore(const ProcessCore&) = delete;
    ProcessCore(ProcessCore&&) = delete;
    ProcessCore& operator=(const ProcessCore&) = delete;
    ProcessCore& operator=(ProcessCore&&) = delete;
private:
    struct BufferPack
    {
        FloatCircularBuffer Buffer;
        DWORD SampleRate;
        DWORD Channels;
        DWORD FrameSampleCount;
        std::vector<float> FilterbankMatrix;
    };

    HWND hwnd;
    ProcessOptions processOptions;
    UINT nodeCount;
    DrawCore drawCore;
    std::thread captureThread;
    std::thread processThread;

    VectorPool<float>& floatVecPool = VectorPool<float>::GetSingleton();
    VectorPool<std::complex<float>>& complexVecPool = VectorPool<std::complex<float>>::GetSingleton();
    UINT32 fftSampleCount;
    UINT32 fftPositiveSampleCount;
    float SoundIndexRangeExp10;
    std::atomic<std::shared_ptr<BufferPack>> pBufferPack;
    std::atomic_bool running = true;

    void captureWorker();
    void processWorker();
    void initializeDevice(wil::com_ptr<IMMDeviceEnumerator> const& pEnumerator, wil::com_ptr<IAudioClient>& pClient, wil::com_ptr<IAudioCaptureClient>& pCapture, const HANDLE eventHandle);
    std::vector<float> initializeFilterbank(DWORD sampleRate) const;
    void windowedFFTForward(std::vector<float>& realPart, std::vector<float>& imaginePart) const;
    VectorPool<float>::PoolVector applyFilterbank(std::span<float> realPart, std::span<float> imaginePart, std::span<float> filterbankMatrix);
    static void inplaceFFTForward(std::vector<float>& realPart, std::vector<float>& imaginePart);
    static inline size_t bitReversal(size_t i, const size_t bits);
    static inline float freqToMel(float freq);
    static inline float melToFreq(float mel);
    static inline void complexMultiply(float realA, float imagA, float realB, float imagB, float& realC, float& imagC);
};
