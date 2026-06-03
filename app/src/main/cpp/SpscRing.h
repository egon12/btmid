#pragma once
#include <atomic>
#include <array>

// Single-producer single-consumer lock-free ring buffer.
// N must be a power of two.
template<typename T, size_t N>
class SpscRing {
    static_assert((N & (N - 1)) == 0, "N must be a power of two");
public:
    bool push(const T& val) noexcept {
        const size_t h    = mHead.load(std::memory_order_relaxed);
        const size_t next = (h + 1) & (N - 1);
        if (next == mTail.load(std::memory_order_acquire)) return false;
        mBuf[h] = val;
        mHead.store(next, std::memory_order_release);
        return true;
    }

    bool pop(T& val) noexcept {
        const size_t t = mTail.load(std::memory_order_relaxed);
        if (t == mHead.load(std::memory_order_acquire)) return false;
        val = mBuf[t];
        mTail.store((t + 1) & (N - 1), std::memory_order_release);
        return true;
    }

private:
    std::array<T, N>    mBuf{};
    std::atomic<size_t> mHead{0};
    std::atomic<size_t> mTail{0};
};
