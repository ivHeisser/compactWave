#include "pinned_buffer.h"

/*
* Почему это быстрее обычной памяти

Обычная память:

RAM → temporary staging buffer → GPU

Pinned memory:

RAM (page-locked) → GPU DMA (direct transfer)

✔ меньше копий
✔ можно cudaMemcpyAsync
✔ можно перекрывать копирование и вычисления

⚠️ Важно (очень)
1. Pinned memory дорогая
медленнее выделяется
ограниченный объём (не стоит делать “всё pinned”)
2. Лучше использовать пул

Если хочешь high-performance вариант — делается allocator/pool, а не new/free.

3. Альтернатива (иногда лучше)
cudaHostRegister()

Используется если у тебя уже есть существующий буфер.

*=========== дальше прокачать
* async pipeline CPU → GPU → CPU (double buffering)
* ring buffer для streaming данных (как в видео/LLM)
* CUDA stream overlap (compute + copy одновременно)
* zero-copy mapped memory (для iGPU / integrated GPU)
*/

void test() {
    PinnedBuffer host(1 << 20); // 1 MB

    float* h = static_cast<float*>(host.data());

    // заполнение CPU
    for (int i = 0; i < (host.size / sizeof(float)); i++) {
        h[i] = static_cast<float>(i);
    }

    float* d = nullptr;
    cudaMalloc(&d, host.size);

    // async copy (важно: pinned memory)
    cudaMemcpyAsync(d, host.data(), host.size, cudaMemcpyHostToDevice, 0);

    cudaDeviceSynchronize();
}