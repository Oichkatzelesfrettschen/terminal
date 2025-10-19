#pragma once

struct CpuFeatures {
    bool hasAVX2 = false;
    bool hasBMI2 = false;
    bool hasFMA = false;
    bool hasF16C = false;

    static CpuFeatures Detect();

    bool SupportsX86_64_V3() const;
};
