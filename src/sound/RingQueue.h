#pragma once

#include <array>
#include <atomic>
#include <cstddef>

namespace easyforge::internal
{
    // A fixed-size queue for one producer and one consumer at a time, neither
    // of which ever waits for the other. Pushing to a full queue fails.
    template <typename Item, std::size_t Capacity>
    class RingQueue
    {
    public:
        bool Push(const Item& item)
        {
            std::size_t write = Write.load(std::memory_order_relaxed);
            if (write - Read.load(std::memory_order_acquire) >= Capacity)
            {
                return false;
            }
            Items[write % Capacity] = item;
            Write.store(write + 1, std::memory_order_release);
            return true;
        }

        bool Pop(Item& item)
        {
            std::size_t read = Read.load(std::memory_order_relaxed);
            if (read == Write.load(std::memory_order_acquire))
            {
                return false;
            }
            item = Items[read % Capacity];
            Read.store(read + 1, std::memory_order_release);
            return true;
        }

    private:
        std::array<Item, Capacity> Items {};
        std::atomic<std::size_t> Write { 0 };
        std::atomic<std::size_t> Read { 0 };
    };
}
