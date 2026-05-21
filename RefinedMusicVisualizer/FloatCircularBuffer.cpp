#include "pch.h"
#include "FloatCircularBuffer.h"

using namespace std::literals::chrono_literals;

FloatCircularBuffer::FloatCircularBuffer(size_t minimumCapacity)
{
    HANDLE hProcess = GetCurrentProcess();
    SYSTEM_INFO sysInfo{};
    GetSystemInfo(&sysInfo);
    size_t allocGranularity = sysInfo.dwAllocationGranularity;
    _capacityBytes = std::bit_ceil(max(minimumCapacity * sizeof(float), allocGranularity));
    _capacity = _capacityBytes / sizeof(float);
    _mask = _capacity - 1;
    _hFileMapping = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, _capacityBytes >> 32, (DWORD)_capacityBytes, NULL);
    if (!_hFileMapping) THROW_LAST_ERROR();

    void* basePtr = nullptr;
    for (size_t i = 0; i < 10; ++i)
    {
        if (basePtr = VirtualAlloc2(hProcess, NULL, _capacityBytes * 2, MEM_RESERVE | MEM_RESERVE_PLACEHOLDER, PAGE_NOACCESS, NULL, 0))
            break;
        std::this_thread::sleep_for(10ms);
    }
    if (!basePtr)
        THROW_LAST_ERROR();

    if (!VirtualFreeEx(hProcess, basePtr, _capacityBytes, MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER))
        THROW_LAST_ERROR();

    _view1 = MapViewOfFile3(_hFileMapping, hProcess, basePtr, 0, _capacityBytes, MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, NULL, 0);
    if (!_view1)
        THROW_LAST_ERROR();

    _view2 = MapViewOfFile3(_hFileMapping, hProcess, (BYTE*)basePtr + _capacityBytes, 0, _capacityBytes, MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, NULL, 0);
    if (!_view2)
        THROW_LAST_ERROR();
    _buffer = static_cast<float*>(basePtr);
}

FloatCircularBuffer::~FloatCircularBuffer()
{
    if (_view1) UnmapViewOfFile(_view1);
    if (_view2) UnmapViewOfFile(_view2);
    if (_hFileMapping) CloseHandle(_hFileMapping);
}

FloatCircularBuffer::FloatCircularBuffer(FloatCircularBuffer&& a) :
    _hFileMapping(std::exchange(a._hFileMapping, (HANDLE)NULL)),
    _view1(std::exchange(a._view1, (LPVOID)NULL)),
    _view2(std::exchange(a._view2, (LPVOID)NULL)),
    _buffer(std::exchange(a._buffer, nullptr)),
    _mask(std::exchange(a._mask, 0)),
    _capacity(std::exchange(a._capacity, 0)),
    _capacityBytes(std::exchange(a._capacityBytes, 0)),
    _readIndex(a._readIndex.exchange(0)),
    _writeIndex(a._writeIndex.exchange(0))
{

}

FloatCircularBuffer& FloatCircularBuffer::operator=(FloatCircularBuffer&& a)
{
    if (_view1) UnmapViewOfFile(_view1);
    if (_view2) UnmapViewOfFile(_view2);
    if (_hFileMapping) CloseHandle(_hFileMapping);

    _hFileMapping = std::exchange(a._hFileMapping, (HANDLE)NULL);
    _view1 = std::exchange(a._view1, (LPVOID)NULL);
    _view2 = std::exchange(a._view2, (LPVOID)NULL);
    _buffer = std::exchange(a._buffer, nullptr);
    _mask = std::exchange(a._mask, 0);
    _capacity = std::exchange(a._capacity, 0);
    _capacityBytes = std::exchange(a._capacityBytes, 0);
    _readIndex = a._readIndex.exchange(0);
    _writeIndex = a._writeIndex.exchange(0);

    return *this;
}

bool FloatCircularBuffer::Write(float* source, size_t offset, size_t count)
{
    if (count > _capacity || count == 0) return false;
    size_t writeIndex = _writeIndex.load(std::memory_order_relaxed);
    size_t readIndex = _readIndex.load(std::memory_order_acquire);
    size_t usedSize = writeIndex - readIndex;
    size_t lastSize = _capacity - usedSize;
    if (lastSize < count) return false;
    memcpy(_buffer + (writeIndex & _mask), source + offset, count * sizeof(float));
    size_t newWriteIndex = writeIndex + count;
    _writeIndex.store(newWriteIndex, std::memory_order_release);
    return true;
}

size_t FloatCircularBuffer::Read(float* dest, size_t offset, size_t count)
{
    if (count == 0) return 0;
    size_t writeIndex = _writeIndex.load(std::memory_order_acquire);
    size_t readIndex = _readIndex.load(std::memory_order_relaxed);
    size_t read = min(writeIndex - readIndex, count);
    memcpy(dest + offset, _buffer + (readIndex & _mask), read * sizeof(float));
    _readIndex.store(readIndex + read, std::memory_order_release);
    return read;
}

size_t FloatCircularBuffer::ReadAllOrDrop(float* dest, size_t offset, size_t count)
{
    if (count == 0) return 0;
    size_t writeIndex = _writeIndex.load(std::memory_order_acquire);
    size_t readIndex = _readIndex.load(std::memory_order_relaxed);
    size_t read = min(writeIndex - readIndex, count);
    memcpy(dest + offset, _buffer + (readIndex & _mask), read * sizeof(float));
    _readIndex.store(writeIndex, std::memory_order_release);
    return read;
}

size_t FloatCircularBuffer::DropData(size_t count)
{
    if (count == 0) return 0;
    size_t writeIndex = _writeIndex.load(std::memory_order_acquire);
    size_t readIndex = _readIndex.load(std::memory_order_relaxed);
    size_t read = min(writeIndex - readIndex, count);
    _readIndex.store(readIndex + read, std::memory_order_release);
    return read;
}

void FloatCircularBuffer::DropAll()
{
    _readIndex.store(_writeIndex.load(std::memory_order_relaxed), std::memory_order_relaxed);
}
