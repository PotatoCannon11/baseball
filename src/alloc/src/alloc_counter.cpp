#include "alloc/alloc_counter.h"

#include <atomic>
#include <cstdlib>
#include <new>

#if defined(__linux__) || defined(__APPLE__)
#define BASEBALL_ALLOC_HAVE_BACKTRACE 1
#include <cstdio>
#include <execinfo.h>
#endif

namespace alloc {
namespace {

// Constant-initialized (zero) at compile time, so these are valid before any
// dynamic initializer in any translation unit runs -- important since the
// global operator new/delete below can be called during other libraries'
// static initialization, before main().
std::atomic<std::uint64_t> g_alloc_count{0};
std::atomic<std::uint64_t> g_free_count{0};
std::atomic<std::uint64_t> g_bytes_allocated{0};
std::atomic<std::uint64_t> g_bytes_freed{0};
std::atomic<int> g_backtrace_log_remaining{0};

// Every tracked allocation is prefixed with its requested size so operator
// delete -- which is not always given a size -- can find out how many bytes
// to report freed. 16 bytes keeps the returned pointer's alignment equal to
// malloc's (at least alignof(std::max_align_t), commonly 16), since offsetting
// a 16-aligned address by a multiple of 16 preserves alignment up to 16.
// Requests with alignment >16 go through the C++17 aligned-new overloads,
// which this file does not override (see alloc_counter.h "known gap").
constexpr std::size_t kHeaderSize = 16;

void* tracked_alloc(std::size_t size) noexcept {
    void* raw = std::malloc(size + kHeaderSize);
    if (!raw) {
        return nullptr;
    }
    *reinterpret_cast<std::size_t*>(raw) = size;
    g_alloc_count.fetch_add(1, std::memory_order_relaxed);
    g_bytes_allocated.fetch_add(size, std::memory_order_relaxed);

#if defined(BASEBALL_ALLOC_HAVE_BACKTRACE)
    int remaining = g_backtrace_log_remaining.load(std::memory_order_relaxed);
    while (remaining > 0) {
        if (g_backtrace_log_remaining.compare_exchange_weak(remaining, remaining - 1, std::memory_order_relaxed)) {
            void* frames[16];
            const int n = backtrace(frames, 16);
            std::fprintf(stderr, "[alloc] %zu bytes:\n", size);
            backtrace_symbols_fd(frames, n, 2);
            break;
        }
    }
#endif

    return static_cast<char*>(raw) + kHeaderSize;
}

void tracked_free(void* ptr) noexcept {
    if (!ptr) {
        return;
    }
    char* raw = static_cast<char*>(ptr) - kHeaderSize;
    const std::size_t size = *reinterpret_cast<std::size_t*>(raw);
    g_free_count.fetch_add(1, std::memory_order_relaxed);
    g_bytes_freed.fetch_add(size, std::memory_order_relaxed);
    std::free(raw);
}

}  // namespace

AllocStats get_stats() {
    AllocStats s;
    s.total_allocations = g_alloc_count.load(std::memory_order_relaxed);
    s.total_frees = g_free_count.load(std::memory_order_relaxed);
    s.bytes_allocated = g_bytes_allocated.load(std::memory_order_relaxed);
    s.bytes_freed = g_bytes_freed.load(std::memory_order_relaxed);
    return s;
}

void debug_log_next_allocations(int count) {
#if defined(BASEBALL_ALLOC_HAVE_BACKTRACE)
    g_backtrace_log_remaining.store(count, std::memory_order_relaxed);
#else
    (void)count;
#endif
}

}  // namespace alloc

// --- Global operator new/delete overrides ----------------------------------

void* operator new(std::size_t size) {
    void* p = alloc::tracked_alloc(size);
    if (!p) {
        throw std::bad_alloc();
    }
    return p;
}

void* operator new[](std::size_t size) {
    void* p = alloc::tracked_alloc(size);
    if (!p) {
        throw std::bad_alloc();
    }
    return p;
}

void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    return alloc::tracked_alloc(size);
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    return alloc::tracked_alloc(size);
}

void operator delete(void* ptr) noexcept { alloc::tracked_free(ptr); }
void operator delete[](void* ptr) noexcept { alloc::tracked_free(ptr); }
void operator delete(void* ptr, std::size_t) noexcept { alloc::tracked_free(ptr); }
void operator delete[](void* ptr, std::size_t) noexcept { alloc::tracked_free(ptr); }
void operator delete(void* ptr, const std::nothrow_t&) noexcept { alloc::tracked_free(ptr); }
void operator delete[](void* ptr, const std::nothrow_t&) noexcept { alloc::tracked_free(ptr); }
