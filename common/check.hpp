#pragma once
// Minimal self-check macros. Unlike assert(), these stay active under NDEBUG,
// so every example binary is also its own test (run by ctest).

#include <cstdlib>
#include <iostream>
#include <type_traits>
#include <utility>

namespace gw::detail {

template <class A, class B>
void check_eq(const A& a, const B& b, const char* ea, const char* eb,
              const char* file, int line) {
    bool ok;
    if constexpr (std::is_integral_v<A> && std::is_integral_v<B>) {
        ok = std::cmp_equal(a, b);  // no signed/unsigned surprises
    } else {
        ok = (a == b);
    }
    if (!ok) {
        std::cerr << file << ':' << line << ": CHECK_EQ failed: " << ea
                  << " == " << eb << "  (" << a << " vs " << b << ")\n";
        std::abort();
    }
}

}  // namespace gw::detail

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            std::cerr << __FILE__ << ':' << __LINE__                       \
                      << ": CHECK failed: " #cond "\n";                    \
            std::abort();                                                  \
        }                                                                  \
    } while (0)

#define CHECK_EQ(a, b) \
    ::gw::detail::check_eq((a), (b), #a, #b, __FILE__, __LINE__)
