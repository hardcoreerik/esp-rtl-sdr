#pragma once
#include <cstddef>
namespace rtl_usb {
constexpr size_t pull_min_bytes = 64u * 1024u;
// Only CPU copies use this buffer. USB DMA allocations remain in IDF's allocator.
template<class Heap> unsigned char *allocate_pull(Heap &heap, size_t requested, bool automatic, size_t &actual)
{
    actual = requested;
    if (auto *p = heap.psram(requested)) return p;
    if (!automatic) return heap.internal(requested);
    size_t n = requested < heap.internal_budget() ? requested : heap.internal_budget();
    n &= ~size_t{1};
    while (n >= pull_min_bytes) {
        if (auto *p = heap.internal(n)) { actual = n; return p; }
        if (n == pull_min_bytes) break;
        n = n / 2u < pull_min_bytes ? pull_min_bytes : (n / 2u) & ~size_t{1};
    }
    return nullptr;
}
}
