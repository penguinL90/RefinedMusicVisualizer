#include "pch.h"
#include "ProcessCore.h"

#define CheckDeviceLoss(hr, deviceLost) \
if (FAILED(hr)) \
{ \
    if (hr == AUDCLNT_E_DEVICE_INVALIDATED) \
    { \
        deviceLost = true; \
        continue; \
    }\
    THROW_HR(hr); \
}

ProcessCore::ProcessCore(HWND _hwnd, UINT _width, UINT _height, NodeOptions const& _nodeOptions, ProcessOptions const& _processOptions) :
    drawCore(_hwnd, _width, _height, _nodeOptions), processOptions(_processOptions), nodeCount(_nodeOptions.Count), hwnd(_hwnd)
{
    if (_processOptions.ProcessRate >= 1000)
        throw std::runtime_error("Process Rate cannot be greater than or equal to 1000");
    if (_processOptions.MaxSoundIndexExp10 <= _processOptions.MinSoundIndexExp10)
        throw std::runtime_error("Sound Range cannot be less than or equal to 0");
    fftSampleCount = 1 << _processOptions.FFTSampleSizeExp2;
    fftPositiveSampleCount = 1 << (_processOptions.FFTSampleSizeExp2 - 1) + 1;
    SoundIndexRangeExp10 = _processOptions.MaxSoundIndexExp10 - _processOptions.MinSoundIndexExp10;
    processThread = std::thread(&ProcessCore::processWorker, this);
    captureThread = std::thread(&ProcessCore::captureWorker, this);
}

ProcessCore::~ProcessCore()
{
    running.store(false, std::memory_order_relaxed);
    if (captureThread.joinable())
        captureThread.join();
    if (processThread.joinable())
        processThread.join();
}

void ProcessCore::captureWorker()
{
    bool initialized = false;
    try
    {
        HRESULT hr = CoInitializeEx(NULL, COINIT_DISABLE_OLE1DDE | COINIT_APARTMENTTHREADED);
        THROW_IF_FAILED(hr);
        initialized = true;

        wil::com_ptr<IMMDeviceEnumerator> pEnumerator;
        wil::com_ptr<IAudioClient> pClient;
        wil::com_ptr<IAudioCaptureClient> pCapture;

        hr = CoCreateInstance(
            __uuidof(MMDeviceEnumerator),
            NULL,
            CLSCTX_ALL,
            __uuidof(IMMDeviceEnumerator),
            pEnumerator.put_void()
        );
        THROW_IF_FAILED(hr);

        wil::unique_handle eventHandle{ CreateEventW(NULL, FALSE, FALSE, NULL) };
        THROW_LAST_ERROR_IF(!eventHandle.is_valid());
        initializeDevice(pEnumerator, pClient, pCapture, eventHandle.get());
        std::shared_ptr<BufferPack> bufferPack{ pBufferPack.load(std::memory_order_acquire) };
        DWORD channels = bufferPack->Channels;
        std::reference_wrapper<FloatCircularBuffer> buffer = bufferPack->Buffer;

        DWORD waitTimeMill = max((DWORD)(2.f / processOptions.ProcessRate), 100UL);
        bool deviceLost = false;
        while (running.load(std::memory_order_relaxed))
        {
            if (deviceLost)
            {
                initializeDevice(pEnumerator, pClient, pCapture, eventHandle.get());
                bufferPack = pBufferPack.load(std::memory_order_acquire);
                channels = bufferPack->Channels;
                buffer = bufferPack->Buffer;
                deviceLost = false;
            }
            DWORD result = WaitForSingleObject(eventHandle.get(), waitTimeMill);
            THROW_LAST_ERROR_IF(result == WAIT_FAILED);
            UINT32 nextPacketSize = 0;
            hr = pCapture->GetNextPacketSize(&nextPacketSize);
            CheckDeviceLoss(hr, deviceLost);
            while (nextPacketSize > 0 && !deviceLost)
            {
                BYTE* pData = nullptr;
                UINT32 frames = 0;
                DWORD flags = 0;
                hr = pCapture->GetBuffer(&pData, &frames, &flags, NULL, NULL);
                CheckDeviceLoss(hr, deviceLost);

                VectorPool<float>::PoolVector pVec = floatVecPool.Rent();
                auto& vec = *pVec;
                vec.resize(frames);

                float* pos = (float*)pData;
                for (size_t i = 0; i < frames; ++i)
                {
                    float val = 0;
                    for (size_t j = 0; j < channels; ++j)
                        val += *(pos++);
                    vec[i] = val / channels;
                }

                buffer.get().Write(vec.data(), 0, frames);

                hr = pCapture->ReleaseBuffer(frames);
                CheckDeviceLoss(hr, deviceLost);

                hr = pCapture->GetNextPacketSize(&nextPacketSize);
                CheckDeviceLoss(hr, deviceLost);
            }
        }
    }
    catch (std::exception const& ex)
    {
        const char* mes = ex.what();
        size_t len = strlen(mes) + 1;
        char* message = new char[len];
        memcpy(message, mes, len);
        PostMessageA(hwnd, WM_ERROR_FROM_OTHER_THREAD, reinterpret_cast<WPARAM>(message), NULL);
        running.store(false, std::memory_order_relaxed);
    }
    if (initialized) CoUninitialize();
}

void ProcessCore::processWorker()
{
    bool initialized = false;
    try
    {
        LARGE_INTEGER t1{}, q{}, dueTime{};
        THROW_LAST_ERROR_IF(!QueryPerformanceFrequency(&q));
        double QPCInterval = 1.f / q.QuadPart;

        wil::unique_handle timer{ CreateWaitableTimerExW(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS) };
        THROW_LAST_ERROR_IF(!timer.is_valid());

        long periodMill = (long)(1000.0 / (double)processOptions.ProcessRate);
        dueTime.QuadPart = -1;

        THROW_LAST_ERROR_IF(!SetWaitableTimer(timer.get(), &dueTime, periodMill, NULL, NULL, FALSE));
        THROW_LAST_ERROR_IF(!QueryPerformanceCounter(&t1));

        while (running.load(std::memory_order_relaxed))
        {
            DWORD waitResult = WaitForSingleObject(timer.get(), INFINITE);
            THROW_LAST_ERROR_IF(waitResult == WAIT_FAILED);
            LARGE_INTEGER t2{};
            THROW_LAST_ERROR_IF(!QueryPerformanceCounter(&t2));
            double realTimeInterval = (t2.QuadPart - t1.QuadPart) * QPCInterval;
            t1 = t2;

            auto bufferPack = pBufferPack.load(std::memory_order_acquire);
            if (!bufferPack) continue;

            UINT32 frameCount = (UINT32)ceil(bufferPack->SampleRate * realTimeInterval);

            auto pRealSample = floatVecPool.Rent();
            auto& realSample = *pRealSample;
            realSample.resize(frameCount);
            frameCount = bufferPack->Buffer.Read(realSample.data(), 0, frameCount);
            realSample.resize(frameCount);

            auto pImagSample = floatVecPool.Rent();
            auto& imagSample = *pImagSample;
            imagSample.resize(frameCount);

            std::fill(imagSample.begin(), imagSample.end(), 0);

            windowedFFTForward(realSample, imagSample);
            auto pResult = applyFilterbank(realSample, imagSample, bufferPack->FilterbankMatrix);

            drawCore.Draw(*pResult);
        }
    }
    catch (std::exception const& ex)
    {
        const char* mes = ex.what();
        size_t len = strlen(mes) + 1;
        char* message = new char[len];
        memcpy(message, mes, len);
        PostMessageA(hwnd, WM_ERROR_FROM_OTHER_THREAD, reinterpret_cast<WPARAM>(message), NULL);
        running.store(false, std::memory_order_relaxed);
    }
}

void ProcessCore::initializeDevice(wil::com_ptr<IMMDeviceEnumerator> const& pEnumerator, wil::com_ptr<IAudioClient>& pClient, wil::com_ptr<IAudioCaptureClient>& pCapture, const HANDLE eventHandle)
{
    ResetEvent(eventHandle);
    HRESULT hr = S_OK;
    wil::com_ptr<IMMDevice> pDevice;
    hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, pDevice.put());
    THROW_IF_FAILED(hr);
    hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, pClient.put_void());
    THROW_IF_FAILED(hr);
    WAVEFORMATEX* pwfx{};
    hr = pClient->GetMixFormat(&pwfx);
    THROW_IF_FAILED(hr);
    std::unique_ptr<WAVEFORMATEX, decltype(&::CoTaskMemFree)> safePwfx(pwfx, &::CoTaskMemFree);
    if (pwfx->wFormatTag != WAVE_FORMAT_EXTENSIBLE || ((WAVEFORMATEXTENSIBLE*)pwfx)->SubFormat != KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)
        throw std::runtime_error("WASAPI format isn't supported.");
    hr = pClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_LOOPBACK,
        0, 0, pwfx, NULL);
    THROW_IF_FAILED(hr);
    hr = pClient->SetEventHandle(eventHandle);
    THROW_IF_FAILED(hr);
    hr = pClient->GetService(__uuidof(IAudioCaptureClient), pCapture.put_void());
    THROW_IF_FAILED(hr);
    hr = pClient->Start();
    THROW_IF_FAILED(hr);

    DWORD sampleRate = pwfx->nSamplesPerSec;
    pBufferPack.store(
        std::make_shared<BufferPack>(
            BufferPack
            {
                .Buffer = FloatCircularBuffer((size_t)(sampleRate * 0.5f)),
                .SampleRate = sampleRate,
                .Channels = pwfx->nChannels,
                .FrameSampleCount = (DWORD)(sampleRate / processOptions.ProcessRate),
                .FilterbankMatrix = initializeFilterbank(sampleRate),
            }),
            std::memory_order_release);
}

std::vector<float> ProcessCore::initializeFilterbank(DWORD sampleRate) const
{
    size_t r = nodeCount, c = fftPositiveSampleCount;

    float min = freqToMel(processOptions.MinFrequency);
    float max = freqToMel((processOptions.MaxFrequency == 0) ? sampleRate / 2.0f : processOptions.MaxFrequency);
    float step = (max - min) / (r + 1);

    size_t fftBinPointsSize = r + 2;
    std::vector<size_t> fftBinPoints(fftBinPointsSize);

    for (size_t i = 0; i < fftBinPointsSize; ++i)
        fftBinPoints[i] = std::clamp((size_t)(melToFreq(min + step * i) * fftSampleCount / sampleRate), (size_t)0, c - 1);

    std::vector<float> vec(r * c);

    size_t offset = 0;
    for (size_t i = 1; i <= r; ++i)
    {
        size_t left = fftBinPoints[i - 1];
        size_t mid = fftBinPoints[i];
        size_t right = fftBinPoints[i + 1]; 
        float height = (left == right) ? 0 : 2.0f / (right - left);
        for (size_t j = left; j < mid; ++j)
            vec[offset + j] = (j - left) / (float)(mid - left) * height;
        for (size_t j = mid; j < right; ++j)
            vec[offset + j] = (right - j) / (float)(right - mid) * height;
        offset += c;
    }
    return vec;
}

void ProcessCore::windowedFFTForward(std::vector<float>& realPart, std::vector<float>& imaginePart) const
{
    size_t windowSize = min(realPart.size(), fftSampleCount);
    float cosCoff = 2.0f * 3.1415927f / windowSize;
    for (size_t i = 0; i < windowSize; ++i)
    {
        float coff = (0.3635819f
            - 0.4891775f * cosf(i * cosCoff)
            + 0.1365995f * cosf(2 * i * cosCoff)
            - 0.0106411f * cosf(3 * i * cosCoff));
        realPart[i] *= coff;
        imaginePart[i] *= coff;
    }
    realPart.resize(fftSampleCount);
    imaginePart.resize(fftSampleCount);
    inplaceFFTForward(realPart, imaginePart);
}

VectorPool<float>::PoolVector ProcessCore::applyFilterbank(std::span<float> realPart, std::span<float> imaginePart, std::span<float> filterbankMatrix)
{
    size_t size = realPart.size();
    VectorPool<float>::PoolVector pResult = floatVecPool.Rent(), pTemp = floatVecPool.Rent();
    auto& result = *pResult;
    result.resize(nodeCount);
    auto& temp = *pTemp;
    temp.resize(size);

    for (size_t i = 0; i < size; ++i)
    {
        float re = realPart[i];
        float im = imaginePart[i];
        temp[i] += sqrtf(re * re + im * im);
    }
    size_t index = 0;
    for (size_t i = 0; i < nodeCount; ++i)
    {
        float val = 0;
        for (size_t j = 0; j < fftPositiveSampleCount; ++j)
            val += temp[j] * filterbankMatrix[index++];
        result[i] = (std::clamp(log10f(val), processOptions.MinSoundIndexExp10, processOptions.MaxSoundIndexExp10) - processOptions.MinSoundIndexExp10) / SoundIndexRangeExp10;
    }
    return pResult;
}

void ProcessCore::inplaceFFTForward(std::vector<float>& realPart, std::vector<float>& imaginePart)
{
    size_t N = realPart.size();
    size_t bits = 0;
    while (((size_t)1 << bits) < N) ++bits;
    for (size_t i = 0; i < N; ++i)
    {
        size_t targetIndex = bitReversal(i, bits);
        if (i < targetIndex)
        {
            std::swap(realPart[i], realPart[targetIndex]);
            std::swap(imaginePart[i], imaginePart[targetIndex]);
        }
    }

    for (size_t len = 2; len <= N; len <<= 1)
    {
        float angle = -2 * 3.1415927f / len;
        size_t half_len = len / 2;
        float omega_len_R = cosf(angle), omega_len_I = sinf(angle);
        for (size_t i = 0; i < N; i += len)
        {
            float omega_R = 1, omega_I = 0;
            for (size_t j = 0; j < half_len; ++j) {
                float u_R = realPart[i + j];
                float u_I = imaginePart[i + j];
                float t_R = 0, t_I = 0;
                complexMultiply(omega_R, omega_I, realPart[i + j + half_len], imaginePart[i + j + half_len], t_R, t_I);
                realPart[i + j] = u_R + t_R;
                imaginePart[i + j] = u_I + t_I;
                realPart[i + j + half_len] = u_R - t_R;
                imaginePart[i + j + half_len] = u_I - t_I;
                float tempOR = omega_R, tempOI = omega_I;
                complexMultiply(tempOR, tempOI, omega_len_R, omega_len_I, omega_R, omega_I);
            }
        }
    }
}

inline size_t ProcessCore::bitReversal(size_t i, const size_t bits)
{
    size_t j = 0;
    for (size_t k = 0; k < bits; ++k)
    {
        j = (j << 1) | (i & 1);
        i >>= 1;
    }
    return j;
}

inline float ProcessCore::freqToMel(float freq)
{
    return (float)(2595.0 * log10(1.0 + freq / 700.0));
}

inline float ProcessCore::melToFreq(float mel)
{
    return (float)(700.0 * (pow(10.0, mel / 2595.0) - 1.0));
}

inline void ProcessCore::complexMultiply(float realA, float imagA, float realB, float imagB, float& realC, float& imagC)
{
    realC = realA * realB - imagA * imagB;
    imagC = realA * imagB + imagA * realB;
}
