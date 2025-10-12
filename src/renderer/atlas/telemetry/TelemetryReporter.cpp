#include "pch.h"
#include "TelemetryReporter.h"

#include <Windows.h>
#include <mutex>

using namespace Microsoft::Console::Render::Atlas::Telemetry;

namespace
{
    std::mutex g_cacheMutex;
    DirectStorageCacheSnapshot g_cacheSnapshot{};
}

void Microsoft::Console::Render::Atlas::Telemetry::ReportDirectStorageEvent(const DirectStorageEvent& evt) noexcept
{
    std::wstring message = L"[DirectStorage] ";
    if (!evt.status.empty())
    {
        message.append(evt.status);
    }
    if (evt.queued)
    {
        message.append(L" | queued");
    }
    if (evt.completed)
    {
        message.append(L" | completed");
    }
    message.append(L"\n");

    OutputDebugStringW(message.c_str());
}

void Microsoft::Console::Render::Atlas::Telemetry::ReportVendorEvent(const VendorEvent& evt) noexcept
{
    std::wstring message = L"[Vendor] ";
    if (!evt.vendor.empty())
    {
        message.append(evt.vendor);
    }
    message.append(L" | Reflex=");
    message.append(evt.reflexEnabled ? L"enabled" : L"disabled");
    message.append(L" | AntiLag=");
    message.append(evt.antiLagEnabled ? L"enabled" : L"disabled");
    message.append(L"\n");

    OutputDebugStringW(message.c_str());
}

void Microsoft::Console::Render::Atlas::Telemetry::ReportDirectStorageCacheEvent(const DirectStorageCacheEvent& evt) noexcept
{
    {
        const std::lock_guard guard(g_cacheMutex);
        g_cacheSnapshot.status.assign(evt.status.begin(), evt.status.end());
        g_cacheSnapshot.cacheHits = evt.cacheHits;
        g_cacheSnapshot.cacheMisses = evt.cacheMisses;
        g_cacheSnapshot.lastCopyMilliseconds = evt.lastCopyMilliseconds;
    }

    std::wstring message = L"[DirectStorageCache] ";
    if (!evt.status.empty())
    {
        message.append(evt.status);
    }
    message.append(L" | hits=");
    message.append(std::to_wstring(evt.cacheHits));
    message.append(L" | misses=");
    message.append(std::to_wstring(evt.cacheMisses));
    message.append(L" | lastCopyMs=");
    message.append(std::to_wstring(evt.lastCopyMilliseconds));
    message.append(L"\n");

    OutputDebugStringW(message.c_str());
}

DirectStorageCacheSnapshot Microsoft::Console::Render::Atlas::Telemetry::GetDirectStorageCacheSnapshot() noexcept
{
    const std::lock_guard guard(g_cacheMutex);
    return g_cacheSnapshot;
}
