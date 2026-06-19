#pragma once
/*
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

class MappedFile {
public:
    void* data = nullptr;
    size_t size = 0;

#ifdef _WIN32
    HANDLE hFile = INVALID_HANDLE_VALUE;
    HANDLE hMap = NULL;
#else
    int fd = -1;
#endif

    bool open(const char* path) {
#ifdef _WIN32
        hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ,
            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) return false;

        LARGE_INTEGER sz;
        GetFileSizeEx(hFile, &sz);
        size = static_cast<size_t>(sz.QuadPart);

        hMap = CreateFileMappingA(hFile, NULL, PAGE_READONLY,
            0, 0, NULL);
        if (!hMap) return false;

        data = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
        return data != nullptr;
#else
        fd = ::open(path, O_RDONLY);
        if (fd < 0) return false;

        struct stat st;
        if (fstat(fd, &st) != 0) return false;
        size = st.st_size;

        data = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
        return data != MAP_FAILED;
#endif
    }

    void close() {
#ifdef _WIN32
        if (data) UnmapViewOfFile(data);
        if (hMap) CloseHandle(hMap);
        if (hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);
#else
        if (data && data != MAP_FAILED) munmap(data, size);
        if (fd >= 0) ::close(fd);
#endif
    }
};
*/

/*
* Если нужен именно mmap/memory-mapped file с RAII и поддержкой Windows + Linux, 
* можно сделать обёртку, которая автоматически открывает файл, отображает его в память и освобождает ресурсы в деструкторе.
Преимущества такого подхода:

RAII — никаких ручных close().
Move-only семантика.
Работает на Windows и Linux.
Файл не копируется в отдельный буфер процесса (ОС подгружает страницы по требованию).
Для очень больших файлов обычно эффективнее, чем std::ifstream + std::vector.

Если данные потом сразу отправляются на GPU, можно дополнительно рассмотреть связку mmap + cudaHostRegister(), 
чтобы зарегистрировать отображённые страницы как pinned memory и ускорить передачу в CUDA. 
Это часто даёт лучший результат, чем сначала читать файл в обычный буфер, а потом копировать в pinned-память.
*/

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

class MappedFile {
public:
    MappedFile() = default;

    explicit MappedFile(const std::string& path) {
        open(path);
    }

    ~MappedFile() {
        close();
    }

    MappedFile(const MappedFile&) = delete;
    MappedFile& operator=(const MappedFile&) = delete;

    MappedFile(MappedFile&& other) noexcept {
        move_from(std::move(other));
    }

    MappedFile& operator=(MappedFile&& other) noexcept {
        if (this != &other) {
            close();
            move_from(std::move(other));
        }
        return *this;
    }

    void open(const std::string& path) {
        close();

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

        LARGE_INTEGER fileSize;
        if (!GetFileSizeEx(hFile_, &fileSize))
            throw std::runtime_error("GetFileSizeEx failed");

        size_ = static_cast<size_t>(fileSize.QuadPart);

        hMapping_ = CreateFileMappingA(
            hFile_,
            nullptr,
            PAGE_READONLY,
            0,
            0,
            nullptr);

        if (!hMapping_)
            throw std::runtime_error("CreateFileMapping failed");

        data_ = MapViewOfFile(
            hMapping_,
            FILE_MAP_READ,
            0,
            0,
            0);

        if (!data_)
            throw std::runtime_error("MapViewOfFile failed");

#else
        fd_ = ::open(path.c_str(), O_RDONLY);
        if (fd_ < 0)
            throw std::runtime_error("open failed");

        struct stat st {};
        if (::fstat(fd_, &st) != 0)
            throw std::runtime_error("fstat failed");

        size_ = static_cast<size_t>(st.st_size);

        data_ = ::mmap(
            nullptr,
            size_,
            PROT_READ,
            MAP_PRIVATE,
            fd_,
            0);

        if (data_ == MAP_FAILED) {
            data_ = nullptr;
            throw std::runtime_error("mmap failed");
        }
#endif
    }

    [[nodiscard]] const void* data() const noexcept {
        return data_;
    }

    [[nodiscard]] void* data() noexcept {
        return data_;
    }

    [[nodiscard]] size_t size() const noexcept {
        return size_;
    }

    template<typename T>
    [[nodiscard]] const T* as() const noexcept {
        return static_cast<const T*>(data_);
    }

private:
    void close() noexcept {
#ifdef _WIN32
        if (data_) {
            UnmapViewOfFile(data_);
            data_ = nullptr;
        }

        if (hMapping_) {
            CloseHandle(hMapping_);
            hMapping_ = nullptr;
        }

        if (hFile_ != INVALID_HANDLE_VALUE) {
            CloseHandle(hFile_);
            hFile_ = INVALID_HANDLE_VALUE;
        }
#else
        if (data_) {
            ::munmap(data_, size_);
            data_ = nullptr;
        }

        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
#endif

        size_ = 0;
    }

    void move_from(MappedFile&& other) noexcept {
        data_ = other.data_;
        size_ = other.size_;

        other.data_ = nullptr;
        other.size_ = 0;

#ifdef _WIN32
        hFile_ = other.hFile_;
        hMapping_ = other.hMapping_;

        other.hFile_ = INVALID_HANDLE_VALUE;
        other.hMapping_ = nullptr;
#else
        fd_ = other.fd_;
        other.fd_ = -1;
#endif
    }

private:
    void* data_ = nullptr;
    size_t size_ = 0;

#ifdef _WIN32
    HANDLE hFile_ = INVALID_HANDLE_VALUE;
    HANDLE hMapping_ = nullptr;
#else
    int fd_ = -1;
#endif
};

// Использование:
void test1() {
    MappedFile file("data.bin");
    auto* bytes = static_cast<const std::byte*>(file.data());
    std::cout << "size = " << file.size() << '\n';
}

// Или для массива float:
void test2 {
    MappedFile file("weights.bin");
    auto* weights = file.as<float>();
    size_t count = file.size() / sizeof(float);
}