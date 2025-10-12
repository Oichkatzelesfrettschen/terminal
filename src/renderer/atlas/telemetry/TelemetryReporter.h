#pragma once

#include <string>
#include <string_view>

namespace Microsoft::Console::Render::Atlas::Telemetry
{
    struct DirectStorageEvent
    {
        std::wstring_view status;
        bool queued = false;
        bool completed = false;
    };

    void ReportDirectStorageEvent(const DirectStorageEvent& evt) noexcept;

    struct VendorEvent
    {
        std::wstring_view vendor;
        bool reflexEnabled = false;
        bool antiLagEnabled = false;
    };

    void ReportVendorEvent(const VendorEvent& evt) noexcept;

    struct DirectStorageCacheEvent
    {
        std::wstring_view status;
        uint64_t cacheHits = 0;
        uint64_t cacheMisses = 0;
        double lastCopyMilliseconds = 0.0;
    };

    void ReportDirectStorageCacheEvent(const DirectStorageCacheEvent& evt) noexcept;

    struct DirectStorageCacheSnapshot
    {
        std::wstring status;
        uint64_t cacheHits = 0;
        uint64_t cacheMisses = 0;
        double lastCopyMilliseconds = 0.0;
    };

    DirectStorageCacheSnapshot GetDirectStorageCacheSnapshot() noexcept;
}
