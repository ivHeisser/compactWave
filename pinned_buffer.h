#pragma once

#include <cuda_runtime.h>
#include <stdexcept>
#include <cstring>

class PinnedBuffer {
public:
    void* ptr = nullptr;
    size_t size = 0;

    PinnedBuffer() = default;

    explicit PinnedBuffer(size_t bytes) {
        allocate(bytes);
    }

    void allocate(size_t bytes) {
        free();

        size = bytes;

        cudaError_t err = cudaMallocHost(&ptr, size); // pinned memory
        if (err != cudaSuccess) {
            ptr = nullptr;
            throw std::runtime_error("cudaMallocHost failed");
        }
    }

    void free() {
        if (ptr) {
            cudaFreeHost(ptr);
            ptr = nullptr;
            size = 0;
        }
    }

    void* data() { return ptr; }
    const void* data() const { return ptr; }

    ~PinnedBuffer() {
        free();
    }

    // запрет копирования (важно для безопасности)
    PinnedBuffer(const PinnedBuffer&) = delete;
    PinnedBuffer& operator=(const PinnedBuffer&) = delete;

    // разрешаем move
    PinnedBuffer(PinnedBuffer&& other) noexcept {
        ptr = other.ptr;
        size = other.size;
        other.ptr = nullptr;
        other.size = 0;
    }

    PinnedBuffer& operator=(PinnedBuffer&& other) noexcept {
        if (this != &other) {
            free();
            ptr = other.ptr;
            size = other.size;
            other.ptr = nullptr;
            other.size = 0;
        }
        return *this;
    }
};