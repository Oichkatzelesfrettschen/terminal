// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "inc/CpuFeatures.h"

#include <intrin.h>  // For __cpuid, __cpuidex intrinsics
#include <cstring>   // For memcpy, memset

using namespace Microsoft::Console::Types;

namespace
{
    // CPUID function numbers
    constexpr uint32_t CPUID_BASIC_INFO = 0x00000000;
    constexpr uint32_t CPUID_FEATURE_INFO = 0x00000001;
    constexpr uint32_t CPUID_EXTENDED_FEATURE_INFO = 0x00000007;
    constexpr uint32_t CPUID_EXTENDED_INFO = 0x80000000;
    constexpr uint32_t CPUID_BRAND_STRING_1 = 0x80000002;
    constexpr uint32_t CPUID_BRAND_STRING_2 = 0x80000003;
    constexpr uint32_t CPUID_BRAND_STRING_3 = 0x80000004;

    // Feature bit positions (CPUID function 1, ECX register)
    constexpr uint32_t ECX_SSE3 = 1 << 0;
    constexpr uint32_t ECX_SSSE3 = 1 << 9;
    constexpr uint32_t ECX_FMA = 1 << 12;
    constexpr uint32_t ECX_SSE41 = 1 << 19;
    constexpr uint32_t ECX_SSE42 = 1 << 20;
    constexpr uint32_t ECX_MOVBE = 1 << 22;
    constexpr uint32_t ECX_POPCNT = 1 << 23;
    constexpr uint32_t ECX_AES = 1 << 25;
    constexpr uint32_t ECX_AVX = 1 << 28;
    constexpr uint32_t ECX_F16C = 1 << 29;

    // Feature bit positions (CPUID function 1, EDX register)
    constexpr uint32_t EDX_SSE = 1 << 25;
    constexpr uint32_t EDX_SSE2 = 1 << 26;

    // Extended feature bit positions (CPUID function 7, EBX register)
    constexpr uint32_t EBX_BMI1 = 1 << 3;
    constexpr uint32_t EBX_AVX2 = 1 << 5;
    constexpr uint32_t EBX_BMI2 = 1 << 8;
    constexpr uint32_t EBX_AVX512F = 1 << 16;
    constexpr uint32_t EBX_AVX512DQ = 1 << 17;
    constexpr uint32_t EBX_AVX512CD = 1 << 28;
    constexpr uint32_t EBX_AVX512BW = 1 << 30;
    constexpr uint32_t EBX_AVX512VL = 1 << 31;
    constexpr uint32_t EBX_SHA = 1 << 29;

    // Extended feature bit positions (CPUID function 0x80000001, ECX register)
    constexpr uint32_t EXT_ECX_LZCNT = 1 << 5;

    // Helper to call CPUID
    void Cpuid(int info[4], int function, int subfunction = 0)
    {
        __cpuidex(info, function, subfunction);
    }
}

CpuFeatures Microsoft::Console::Types::DetectCpuFeatures() noexcept
{
    CpuFeatures features{};
    int info[4] = { 0 };

    // Get vendor string
    Cpuid(info, CPUID_BASIC_INFO);
    const uint32_t maxBasicFunction = static_cast<uint32_t>(info[0]);
    memcpy(features.vendor + 0, &info[1], 4);
    memcpy(features.vendor + 4, &info[3], 4);
    memcpy(features.vendor + 8, &info[2], 4);
    features.vendor[12] = '\0';

    // Get basic feature information (function 1)
    if (maxBasicFunction >= CPUID_FEATURE_INFO)
    {
        Cpuid(info, CPUID_FEATURE_INFO);

        const uint32_t ecx = static_cast<uint32_t>(info[2]);
        const uint32_t edx = static_cast<uint32_t>(info[3]);

        // Extract family, model, stepping from EAX
        const uint32_t eax = static_cast<uint32_t>(info[0]);
        features.stepping = eax & 0xF;
        features.model = (eax >> 4) & 0xF;
        features.family = (eax >> 8) & 0xF;

        // Extended model and family (for newer CPUs)
        if (features.family == 0x0F || features.family == 0x06)
        {
            features.model += ((eax >> 16) & 0xF) << 4;
        }
        if (features.family == 0x0F)
        {
            features.family += (eax >> 20) & 0xFF;
        }

        // Detect individual features from ECX
        features.sse3 = (ecx & ECX_SSE3) != 0;
        features.ssse3 = (ecx & ECX_SSSE3) != 0;
        features.fma = (ecx & ECX_FMA) != 0;
        features.sse41 = (ecx & ECX_SSE41) != 0;
        features.sse42 = (ecx & ECX_SSE42) != 0;
        features.movbe = (ecx & ECX_MOVBE) != 0;
        features.popcnt = (ecx & ECX_POPCNT) != 0;
        features.aes = (ecx & ECX_AES) != 0;
        features.avx = (ecx & ECX_AVX) != 0;
        features.f16c = (ecx & ECX_F16C) != 0;

        // Detect individual features from EDX
        features.sse = (edx & EDX_SSE) != 0;
        features.sse2 = (edx & EDX_SSE2) != 0;
    }

    // Get extended feature information (function 7)
    if (maxBasicFunction >= CPUID_EXTENDED_FEATURE_INFO)
    {
        Cpuid(info, CPUID_EXTENDED_FEATURE_INFO, 0);

        const uint32_t ebx = static_cast<uint32_t>(info[1]);

        features.bmi1 = (ebx & EBX_BMI1) != 0;
        features.avx2 = (ebx & EBX_AVX2) != 0;
        features.bmi2 = (ebx & EBX_BMI2) != 0;
        features.avx512f = (ebx & EBX_AVX512F) != 0;
        features.avx512dq = (ebx & EBX_AVX512DQ) != 0;
        features.avx512cd = (ebx & EBX_AVX512CD) != 0;
        features.avx512bw = (ebx & EBX_AVX512BW) != 0;
        features.avx512vl = (ebx & EBX_AVX512VL) != 0;
        features.sha = (ebx & EBX_SHA) != 0;
    }

    // Get extended processor info (function 0x80000001)
    Cpuid(info, CPUID_EXTENDED_INFO);
    const uint32_t maxExtendedFunction = static_cast<uint32_t>(info[0]);

    if (maxExtendedFunction >= 0x80000001)
    {
        Cpuid(info, 0x80000001);
        const uint32_t ecx = static_cast<uint32_t>(info[2]);

        features.lzcnt = (ecx & EXT_ECX_LZCNT) != 0;
    }

    // Determine x86-64 microarchitecture levels
    features.x64v1 = features.sse && features.sse2;

    features.x64v2 = features.x64v1 &&
                     features.sse3 &&
                     features.ssse3 &&
                     features.sse41 &&
                     features.sse42 &&
                     features.popcnt;

    features.x64v3 = features.x64v2 &&
                     features.avx &&
                     features.avx2 &&
                     features.bmi1 &&
                     features.bmi2 &&
                     features.f16c &&
                     features.fma &&
                     features.lzcnt &&
                     features.movbe;

    features.x64v4 = features.x64v3 &&
                     features.avx512f &&
                     features.avx512bw &&
                     features.avx512cd &&
                     features.avx512dq &&
                     features.avx512vl;

    return features;
}

bool Microsoft::Console::Types::IsX64V3Supported() noexcept
{
    const auto features = DetectCpuFeatures();
    return features.x64v3;
}

bool Microsoft::Console::Types::IsAvx2Supported() noexcept
{
    const auto features = DetectCpuFeatures();
    return features.avx2;
}

void Microsoft::Console::Types::GetCpuBrandString(char* buffer, size_t bufferSize) noexcept
{
    if (buffer == nullptr || bufferSize == 0)
    {
        return;
    }

    int info[4] = { 0 };

    // Check if extended CPUID is available
    Cpuid(info, CPUID_EXTENDED_INFO);
    const uint32_t maxExtendedFunction = static_cast<uint32_t>(info[0]);

    if (maxExtendedFunction >= CPUID_BRAND_STRING_3)
    {
        // Brand string is returned in 3 CPUID calls (48 bytes total)
        char brandString[49] = { 0 };

        Cpuid(info, CPUID_BRAND_STRING_1);
        memcpy(brandString + 0, info, 16);

        Cpuid(info, CPUID_BRAND_STRING_2);
        memcpy(brandString + 16, info, 16);

        Cpuid(info, CPUID_BRAND_STRING_3);
        memcpy(brandString + 32, info, 16);

        brandString[48] = '\0';

        // Copy to output buffer (respecting buffer size)
        strncpy_s(buffer, bufferSize, brandString, _TRUNCATE);
    }
    else
    {
        // Fallback if brand string not available
        strncpy_s(buffer, bufferSize, "Unknown CPU", _TRUNCATE);
    }
}

bool Microsoft::Console::Types::ValidateBuildConfiguration() noexcept
{
#ifdef ULTRA_PERFORMANCE_BUILD
    // This is an UltraPerformance build - check if CPU supports x86-64-v3
    return IsX64V3Supported();
#else
    // Standard build - compatible with all x64 CPUs
    return true;
#endif
}

const char* Microsoft::Console::Types::GetRecommendedBuildConfiguration() noexcept
{
    const auto features = DetectCpuFeatures();

    if (features.x64v4)
    {
        return "UltraPerformance (AVX-512 capable)";
    }
    else if (features.x64v3)
    {
        return "UltraPerformance (x86-64-v3)";
    }
    else if (features.x64v2)
    {
        return "Release (x86-64-v2)";
    }
    else
    {
        return "Release (baseline x64)";
    }
}
