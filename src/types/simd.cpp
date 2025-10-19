#include "pch.h"
#include "simd.h"
#include "CpuFeatures.h"
#include <immintrin.h>

namespace simd
{
    int CountSpaces_Scalar(const char* s, size_t len)
    {
        int count = 0;
        for (size_t i = 0; i < len; ++i)
        {
            if (s[i] == ' ')
            {
                count++;
            }
        }
        return count;
    }

    int CountSpaces_AVX2(const char* s, size_t len)
    {
        int count = 0;
        const __m256i space = _mm256_set1_epi8(' ');
        size_t i = 0;
        for (; i + 32 <= len; i += 32)
        {
            const __m256i chunk = _mm256_loadu_si256((const __m256i*)(s + i));
            const __m256i result = _mm256_cmpeq_epi8(chunk, space);
            const int mask = _mm256_movemask_epi8(result);
            count += _mm_popcnt_u32(mask);
        }

        for (; i < len; ++i)
        {
            if (s[i] == ' ')
            {
                count++;
            }
        }

        return count;
    }

    decltype(&CountSpaces) CountSpaces_ptr = nullptr;

    void Initialize()
    {
        const auto features = CpuFeatures::Detect();
        if (features.SupportsX86_64_V3())
        {
            CountSpaces_ptr = &CountSpaces_AVX2;
        }
        else
        {
            CountSpaces_ptr = &CountSpaces_Scalar;
        }
    }

    int CountSpaces(const char* s, size_t len)
    {
        return CountSpaces_ptr(s, len);
    }
}
