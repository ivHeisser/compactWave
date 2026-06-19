#pragma once

#pragma once

#include <span>
#include <string>
#include <stdexcept>
#include <cstddef>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

/*
* std::span отлично подходит поверх mmap, потому что mmap уже даёт непрерывный участок памяти. 
Можно сделать типизированную обёртку, которая возвращает std::span<const T> и скрывает все платформенные детали.
c++20 требуется

Плюс std::span даёт:

size()
size_bytes()
subspan()
first()
last()

без хранения лишних указателей и размеров вручную.

Если файл содержит структуры (struct Vertex, struct Particle и т.п.), такой шаблон особенно удобен: получаете типизированное представление файла без копирования данных.
*/

template<typename T>
class MappedSpan {
public:
    explicit MappedSpan(const std::string& path) {
        open(path);
    }

    ~MappedSpan() {
        close();
    }

    MappedSpan(const MappedSpan&) = delete;
    MappedSpan& operator=(const MappedSpan&) = delete;

    MappedSpan(MappedSpan&& other) noexcept {
        move_from(std::move(other));
    }

    MappedSpan& operator=(MappedSpan&& other) noexcept {
        if (this != &other) {
            close();
            move_from(std::move(other));
        }
        return *this;
    }

    [[nodiscard]]
    std::span<const T> span() const noexcept {
        return { data_, count_ };
    }

    [[nodiscard]]
    const T* data() const noexcept {
        return data_;
    }

    [[nodiscard]]
    size_t size() const noexcept {
        return count_;
    }

    [[nodiscard]]
    const T& operator[](size_t i) const noexcept {
        return data_[i];
    }

private:
    void open(const std::string& path) {
#ifdef _WIN32
        hFile_ = CreateFileA(
            path.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);

        if (hFile_ == INVALID_HANDLE_VALUE)
            throw std::runtime_error("CreateFile failed");

        LARGE_INTEGER sz{};
        GetFileSizeEx(hFile_, &sz);

        bytes_ = static_cast<size_t>(sz.QuadPart);

        if (bytes_ % sizeof(T) != 0)
            throw std::runtime_error("File size is not multiple of sizeof(T)");

        count_ = bytes_ / sizeof(T);

        hMap_ = CreateFileMappingA(
            hFile_,
            nullptr,
            PAGE_READONLY,
            0,
            0,
            nullptr);

        if (!hMap_)
            throw std::runtime_error("CreateFileMapping failed");

        data_ = static_cast<const T*>(
            MapViewOfFile(
                hMap_,
                FILE_MAP_READ,
                0,
                0,
                0));

        if (!data_)
            throw std::runtime_error("MapViewOfFile failed");

#else
        fd_ = ::open(path.c_str(), O_RDONLY);

        if (fd_ < 0)
            throw std::runtime_error("open failed");

        struct stat st {};
        if (::fstat(fd_, &st) != 0)
            throw std::runtime_error("fstat failed");

        bytes_ = static_cast<size_t>(st.st_size);

        if (bytes_ % sizeof(T) != 0)
            throw std::runtime_error("File size is not multiple of sizeof(T)");

        count_ = bytes_ / sizeof(T);

        void* ptr = ::mmap(
            nullptr,
            bytes_,
            PROT_READ,
            MAP_PRIVATE,
            fd_,
            0);

        if (ptr == MAP_FAILED)
            throw std::runtime_error("mmap failed");

        data_ = static_cast<const T*>(ptr);
#endif
    }

    void close() noexcept {
#ifdef _WIN32
        if (data_)
            UnmapViewOfFile(data_);

        if (hMap_)
            CloseHandle(hMap_);

        if (hFile_ != INVALID_HANDLE_VALUE)
            CloseHandle(hFile_);

        hMap_ = nullptr;
        hFile_ = INVALID_HANDLE_VALUE;
#else
        if (data_)
            ::munmap(const_cast<T*>(data_), bytes_);

        if (fd_ >= 0)
            ::close(fd_);

        fd_ = -1;
#endif

        data_ = nullptr;
        count_ = 0;
        bytes_ = 0;
    }

    void move_from(MappedSpan&& other) noexcept {
        data_ = other.data_;
        count_ = other.count_;
        bytes_ = other.bytes_;

        other.data_ = nullptr;
        other.count_ = 0;
        other.bytes_ = 0;

#ifdef _WIN32
        hFile_ = other.hFile_;
        hMap_ = other.hMap_;

        other.hFile_ = INVALID_HANDLE_VALUE;
        other.hMap_ = nullptr;
#else
        fd_ = other.fd_;
        other.fd_ = -1;
#endif
    }

private:
    const T* data_ = nullptr;
    size_t count_ = 0;
    size_t bytes_ = 0;

#ifdef _WIN32
    HANDLE hFile_ = INVALID_HANDLE_VALUE;
    HANDLE hMap_ = nullptr;
#else
    int fd_ = -1;
#endif
};


// Использование:
void test1() {
    MappedSpan<float> weights("weights.bin");

    std::span<const float> w = weights.span();

    for (float x : w) {
        // ...
    }
}

// Или ещё короче :

void test2() {
    MappedSpan<uint32_t> data("table.bin");

    auto view = data.span();

    std::ranges::sort(...); // если span не const
}

// Для CUDA особенно удобно :

void test_cuda() {
    MappedSpan<float> file("weights.bin");

    auto weights = file.span();

    float* d_weights;
    cudaMalloc(&d_weights, weights.size_bytes());

    cudaMemcpy(
        d_weights,
        weights.data(),
        weights.size_bytes(),
        cudaMemcpyHostToDevice);
}