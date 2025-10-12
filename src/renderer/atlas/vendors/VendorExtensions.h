#pragma once

#include <optional>
#include <string>
#include <wil/resource.h>

namespace Microsoft::Console::Render::Atlas::Vendors
{
    enum class Vendor
    {
        Unknown,
        Nvidia,
        AMD,
        Intel
    };

    struct Capabilities
    {
        Vendor vendor = Vendor::Unknown;
        bool nvapiAvailable = false;
        bool agsAvailable = false;
        bool reflexLowLatency = false;
        bool amdAntiLag = false;
    };

    Capabilities Initialize(const LUID& adapterLuid) noexcept;
    void Shutdown() noexcept;
    bool EnableReflex(bool enable) noexcept;
    bool EnableAntiLag(bool enable) noexcept;
}
