#pragma once

#include <cstdint>

#if defined(_M_X64) || defined(__x86_64__) || defined(_M_IX86) || defined(__i386__)
 #include <xmmintrin.h>
 #define AUGUR_X86 1
#endif

namespace augur
{

// Sets flush-to-zero / denormals-are-zero for the current scope (used by offline tools and tests;
// the plugin additionally relies on juce::ScopedNoDenormals).
class ScopedFlushDenormals
{
public:
    ScopedFlushDenormals() noexcept
    {
#if defined(AUGUR_X86)
        previous = _mm_getcsr();
        _mm_setcsr (static_cast<unsigned int> (previous) | 0x8040u); // FTZ | DAZ
#elif defined(__aarch64__)
        std::uint64_t fpcr;
        asm volatile ("mrs %0, fpcr" : "=r"(fpcr));
        previous = fpcr;
        asm volatile ("msr fpcr, %0" : : "r"(fpcr | (1ull << 24)));
#endif
    }

    ~ScopedFlushDenormals()
    {
#if defined(AUGUR_X86)
        _mm_setcsr (static_cast<unsigned int> (previous));
#elif defined(__aarch64__)
        asm volatile ("msr fpcr, %0" : : "r"(previous));
#endif
    }

    ScopedFlushDenormals (const ScopedFlushDenormals&) = delete;
    ScopedFlushDenormals& operator= (const ScopedFlushDenormals&) = delete;

private:
    std::uint64_t previous = 0;
};

} // namespace augur
