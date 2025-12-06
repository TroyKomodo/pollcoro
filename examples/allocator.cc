#define POLLCORO_USE_CONCEPTS 0

#include <cstring>
#include <iostream>
#include <pollcoro/allocator.hpp>
#include <pollcoro/block_on.hpp>
#include <pollcoro/task.hpp>
#include <pollcoro/yield.hpp>
#include <pollcoro/wait_all.hpp>

template<size_t BlockSize, size_t BlockCount>
class bitmap_allocator {
    alignas(std::max_align_t) std::byte data_[BlockSize * BlockCount];
    static constexpr size_t BitmapSize = (BlockCount + 63) / 64;
    uint64_t bitmap_[BitmapSize] = {};

  public:
    bool owns(const void* ptr) const noexcept {
        auto* p = static_cast<const std::byte*>(ptr);
        return p >= data_ && p < data_ + sizeof(data_);
    }

    void* allocate() {
        for (size_t i = 0; i < BitmapSize; ++i) {
            if (bitmap_[i] != ~0ULL) {  // has at least one free bit
                int bit = std::countr_one(bitmap_[i]);
                size_t index = i * 64 + bit;
                if (index >= BlockCount)
                    throw std::bad_alloc();
                bitmap_[i] |= (1ULL << bit);
                return data_ + index * BlockSize;
            }
        }
        throw std::bad_alloc();
    }

    void deallocate(void* ptr) noexcept {
        size_t index = (static_cast<std::byte*>(ptr) - data_) / BlockSize;
        bitmap_[index / 64] &= ~(1ULL << (index % 64));
    }

    size_t allocated_count() const noexcept {
        size_t count = 0;
        for (size_t i = 0; i < BitmapSize; ++i) {
            count += std::popcount(bitmap_[i]);
        }
        return count;
    }

    size_t allocated_bytes() const noexcept {
        return allocated_count() * BlockSize;
    }
};

template<size_t N>
class slab_allocator {
    bitmap_allocator<128, N / 128> small_;
    bitmap_allocator<512, N / 512> medium_;
    bitmap_allocator<1024, N / 1024> large_;

  public:
    void* allocate(size_t size) {
        std::cout << "slab_allocator::allocate " << size << std::endl;
        if (size <= 128)
            return small_.allocate();
        if (size <= 512)
            return medium_.allocate();
        if (size <= 1024)
            return large_.allocate();
        throw std::bad_alloc();
    }

    void deallocate(void* ptr) noexcept {
        if (small_.owns(ptr))
            return small_.deallocate(ptr);
        if (medium_.owns(ptr))
            return medium_.deallocate(ptr);
        if (large_.owns(ptr))
            return large_.deallocate(ptr);
    }

    size_t allocated_bytes() const noexcept {
        return small_.allocated_bytes() + medium_.allocated_bytes() + large_.allocated_bytes();
    }

    size_t allocated_count() const noexcept {
        return small_.allocated_count() + medium_.allocated_count() + large_.allocated_count();
    }
};

pollcoro::task<> test2() {
    int a[100];
    co_return;
}

pollcoro::task<> yield() {
    co_await test2();
    co_await pollcoro::yield();
}

pollcoro::task<> test() {
    auto alloc = slab_allocator<10240>();
    co_await pollcoro::allocate_in(alloc, yield);
    co_await yield();
}

pollcoro::task<> square() {
    for (int i = 0; i < 1000; ++i) {
        co_await pollcoro::allocate_in(pollcoro::default_allocator, yield);
    }
}

int main() {
    auto result = pollcoro::block_on(pollcoro::wait_all(test(), square()));
    return 0;
}
