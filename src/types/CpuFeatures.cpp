#include "pch.h"
#include "CpuFeatures.h"
#include <intrin.h>

CpuFeatures CpuFeatures::Detect()
{
    CpuFeatures features;
    int cpuInfo[4];

    // Check AVX2 (EAX=7, ECX=0): EBX bit 5
    __cpuidex(cpuInfo, 7, 0);
    features.hasAVX2 = (cpuInfo[1] & (1 << 5)) != 0;
    features.hasBMI2 = (cpuInfo[1] & (1 << 8)) != 0;

    // Check FMA (EAX=1): ECX bit 12
    __cpuid(cpuInfo, 1);
    features.hasFMA = (cpuInfo[2] & (1 << 12)) != 0;
    features.hasF16C = (cpuInfo[2] & (1 << 29)) != 0;

    return features;
}

bool CpuFeatures::SupportsX86_64_V3() const
{
    return hasAVX2 && hasBMI2 && hasFMA && hasF16C;
}
