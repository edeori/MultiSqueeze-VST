#pragma once

#include <array>
#include <atomic>

class SpectrumFifo
{
public:
    static constexpr int fftOrder = 12;
    static constexpr int fftSize = 1 << fftOrder;

    void push (float sample) noexcept
    {
        incoming[(size_t) writeIndex++] = sample;
        if (writeIndex == fftSize)
        {
            writeIndex = 0;
            if (! ready.load (std::memory_order_acquire))
            {
                pending = incoming;
                ready.store (true, std::memory_order_release);
            }
        }
    }

    bool pullLatestBlock (std::array<float, fftSize>& destination) noexcept
    {
        if (! ready.load (std::memory_order_acquire))
            return false;
        destination = pending;
        ready.store (false, std::memory_order_release);
        return true;
    }

private:
    std::array<float, fftSize> incoming {}, pending {};
    int writeIndex = 0;
    std::atomic<bool> ready { false };
};
