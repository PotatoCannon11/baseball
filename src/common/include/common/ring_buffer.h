#pragma once

#include <array>
#include <cstddef>
#include <type_traits>

// Fixed-capacity circular buffer. No heap allocation, ever -- storage is an
// inline std::array, sized at compile time. This is the concrete type
// behind the "one per device, fixed power-of-two capacity, plain structs,
// about 1 s of history" IMU ring buffer requirement, and is reused for the
// button-event ring too.
namespace common {

template <typename T, std::size_t Capacity>
class RingBuffer {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two");
    static_assert(std::is_trivially_copyable_v<T>, "RingBuffer<T> expects a POD-like T");

public:
    void push(const T& value) {
        storage_[write_index_ & kMask] = value;
        ++write_index_;
        if (size_ < Capacity) ++size_;
    }

    std::size_t size() const { return size_; }
    static constexpr std::size_t capacity() { return Capacity; }
    bool empty() const { return size_ == 0; }

    // Total number of pushes ever made, including ones since overwritten.
    // Consumers that want to detect "has anything new arrived since I last
    // looked" should compare this rather than size(), which saturates.
    // std::size_t is 64-bit on every platform this project targets, so this
    // does not realistically wrap during a process lifetime.
    std::size_t total_pushed() const { return write_index_; }

    // index 0 is the oldest retained element, size()-1 is the newest.
    const T& operator[](std::size_t i) const {
        const std::size_t oldest = write_index_ - size_;
        return storage_[(oldest + i) & kMask];
    }

    const T& back() const { return (*this)[size_ - 1]; }
    const T& front() const { return (*this)[0]; }

    void clear() {
        write_index_ = 0;
        size_ = 0;
    }

private:
    static constexpr std::size_t kMask = Capacity - 1;
    std::array<T, Capacity> storage_{};
    std::size_t write_index_ = 0;
    std::size_t size_ = 0;
};

}  // namespace common
