#pragma once
// Tracks *live* heap bytes (allocated minus freed), so the examples print
// measured memory, not estimates. Each block carries a 16-byte header holding
// its size, so delete knows how much to subtract. Replaces the global
// operator new/delete: include it from exactly one .cpp per binary.
// noinline: stops GCC's -Wmismatched-new-delete from seeing malloc/free
// through the inlined replacement and flagging a false mismatch at -O2.

#include <cstddef>
#include <cstdlib>
#include <new>

inline std::size_t g_heap_bytes = 0;

namespace alloc_counter {
inline constexpr std::size_t kHeader = 16;  // keeps the returned pointer 16-byte aligned
}

[[gnu::noinline]] void* operator new(std::size_t n) {
    auto* raw = static_cast<unsigned char*>(std::malloc(n + alloc_counter::kHeader));
    if (!raw) throw std::bad_alloc{};
    *reinterpret_cast<std::size_t*>(raw) = n;
    g_heap_bytes += n;
    return raw + alloc_counter::kHeader;
}

[[gnu::noinline]] void operator delete(void* p) noexcept {
    if (!p) return;
    auto* raw = static_cast<unsigned char*>(p) - alloc_counter::kHeader;
    g_heap_bytes -= *reinterpret_cast<std::size_t*>(raw);
    std::free(raw);
}

[[gnu::noinline]] void operator delete(void* p, std::size_t) noexcept { operator delete(p); }
