#pragma once

struct FloatCircularBuffer
{
    FloatCircularBuffer(size_t miniumCapacity = 16384);
    ~FloatCircularBuffer();
    FloatCircularBuffer(const FloatCircularBuffer&) = delete;
    FloatCircularBuffer(FloatCircularBuffer&&);
    FloatCircularBuffer& operator=(const FloatCircularBuffer&) = delete;
    FloatCircularBuffer& operator=(FloatCircularBuffer&&);

    bool Write(float* source, size_t offset, size_t count);
    size_t Read(float* dest, size_t offset, size_t count);
    size_t ReadAllOrDrop(float* dest, size_t offset, size_t count);
    size_t DropData(size_t count);
    void DropAll();
    size_t Capacity() const { return _capacity; };
private:
    HANDLE _hFileMapping{};
    LPVOID _view1{};
    LPVOID _view2{};
    float* _buffer{};
    size_t _mask{};
    size_t _capacity{};
    size_t _capacityBytes{};
    alignas(std::hardware_destructive_interference_size) std::atomic_size_t _readIndex{};
    alignas(std::hardware_destructive_interference_size) std::atomic_size_t _writeIndex{};
};

