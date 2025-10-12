// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

/*++
Module Name:
    CpuFeatures.h

Abstract:
    CPU feature detection for runtime optimization and compatibility checking.

    Detects x86-64 microarchitecture levels and specific instruction set features:
    - x86-64-v1 (baseline): SSE, SSE2
    - x86-64-v2 (2009+): SSSE3, SSE4.1, SSE4.2, POPCNT
    - x86-64-v3 (2013+): AVX, AVX2, BMI1, BMI2, F16C, FMA, LZCNT, MOVBE
    - x86-64-v4 (2017+): AVX-512F, AVX-512BW, AVX-512CD, AVX-512DQ, AVX-512VL

Author(s):
    Ultra-Riced Windows Terminal Project

--*/

#pragma once

#include <cstdint>

namespace Microsoft::Console::Types
{
    // CPU feature flags
    struct CpuFeatures
    {
        // x86-64 microarchitecture levels
        bool x64v1; // Baseline: SSE, SSE2 (all x64 CPUs)
        bool x64v2; // Nehalem 2009+: +SSSE3, SSE4.1, SSE4.2, POPCNT
        bool x64v3; // Haswell 2013+: +AVX, AVX2, BMI1, BMI2, F16C, FMA, LZCNT, MOVBE
        bool x64v4; // Skylake-X 2017+: +AVX-512F, AVX-512BW, AVX-512CD, AVX-512DQ, AVX-512VL

        // Individual instruction sets (for granular detection)
        bool sse;
        bool sse2;
        bool sse3;
        bool ssse3;
        bool sse41;
        bool sse42;
        bool avx;
        bool avx2;
        bool avx512f;
        bool fma;
        bool f16c;
        bool bmi1;
        bool bmi2;
        bool lzcnt;
        bool movbe;
        bool popcnt;
        bool aes;
        bool sha;

        // CPU vendor and model info
        char vendor[13];  // "GenuineIntel", "AuthenticAMD", etc.
        uint32_t family;
        uint32_t model;
        uint32_t stepping;
    };

    // Detect CPU features at runtime
    // This function uses CPUID instruction to query CPU capabilities
    CpuFeatures DetectCpuFeatures() noexcept;

    // Check if current CPU supports x86-64-v3 (required for UltraPerformance builds)
    bool IsX64V3Supported() noexcept;

    // Check if current CPU supports AVX2 specifically
    bool IsAvx2Supported() noexcept;

    // Get human-readable CPU name (e.g., "Intel Core i7-10700K")
    void GetCpuBrandString(char* buffer, size_t bufferSize) noexcept;

    // Validation: Check if current build configuration matches CPU capabilities
    // Returns true if compatible, false if running x86-64-v3 build on incompatible CPU
    bool ValidateBuildConfiguration() noexcept;

    // Get recommended build configuration for current CPU
    // Returns: "Release", "UltraPerformance", etc.
    const char* GetRecommendedBuildConfiguration() noexcept;

} // namespace Microsoft::Console::Types
