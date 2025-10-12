#include "pch.h"
#include "VendorExtensions.h"

#include <Windows.h>
#include <dxgi1_6.h>

using namespace Microsoft::Console::Render::Atlas::Vendors;

// TODO(Atlas Vendors): When the NVAPI and AGS SDKs are bundled,
//  - resolve NvAPI_QueryInterface to call NvAPI_Initialize / NvAPI_D3D_SetSleepMode (Reflex)
//  - resolve agsInitialize / agsDriverExtensionsDX12_SetSleepMode (Anti-Lag)
// At the moment we track the toggles for telemetry so we can light up the UI
// without shipping the proprietary headers in-tree.

namespace
{
    struct NvapiGuards
    {
        wil::unique_hmodule module;
    } nvapi;

    struct AgsGuards
    {
        wil::unique_hmodule module;
    } ags;

    bool g_reflexEnabled = false;
    bool g_antiLagEnabled = false;

    Vendor VendorFromAdapter(const LUID& luid) noexcept
    {
        Microsoft::WRL::ComPtr<IDXGIFactory6> factory;
        if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory))))
        {
            return Vendor::Unknown;
        }

        Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
        for (UINT index = 0; SUCCEEDED(factory->EnumAdapters1(index, &adapter)); ++index)
        {
            DXGI_ADAPTER_DESC1 desc{};
            if (FAILED(adapter->GetDesc1(&desc)))
            {
                continue;
            }

            if (desc.AdapterLuid.LowPart == luid.LowPart && desc.AdapterLuid.HighPart == luid.HighPart)
            {
                switch (desc.VendorId)
                {
                case 0x10DE:
                    return Vendor::Nvidia;
                case 0x1002:
                case 0x1022:
                    return Vendor::AMD;
                case 0x8086:
                    return Vendor::Intel;
                default:
                    return Vendor::Unknown;
                }
            }
        }

        return Vendor::Unknown;
    }
}

Capabilities Vendors::Initialize(const LUID& adapterLuid) noexcept
{
    Capabilities caps{};
    caps.vendor = VendorFromAdapter(adapterLuid);

    if (caps.vendor == Vendor::Nvidia)
    {
        nvapi.module.reset(LoadLibraryW(L"nvapi64.dll"));
        caps.nvapiAvailable = nvapi.module.is_valid();
        caps.reflexLowLatency = caps.nvapiAvailable && g_reflexEnabled;
    }

    if (caps.vendor == Vendor::AMD)
    {
        ags.module.reset(LoadLibraryW(L"amd_ags_x64.dll"));
        caps.agsAvailable = ags.module.is_valid();
        caps.amdAntiLag = caps.agsAvailable && g_antiLagEnabled;
    }

    return caps;
}

void Vendors::Shutdown() noexcept
{
    nvapi.module.reset();
    ags.module.reset();
    g_reflexEnabled = false;
    g_antiLagEnabled = false;
}

bool Vendors::EnableReflex(bool enable) noexcept
{
    if (!nvapi.module)
    {
        if (enable)
        {
            OutputDebugStringW(L"[Atlas][Vendor] NVAPI not loaded; cannot enable NVIDIA Reflex.\n");
        }

        g_reflexEnabled = false;
        return false;
    }

    g_reflexEnabled = enable;
    OutputDebugStringW(enable ? L"[Atlas][Vendor] NVIDIA Reflex requested (NVAPI hook pending).\n" : L"[Atlas][Vendor] NVIDIA Reflex disabled.\n");

    // TODO: Wire EnableReflex to NvAPI_D3D_SetSleepMode once the NVAPI SDK is bundled.
    // The call requires the active D3D12 command queue and NV_SET_SLEEP_MODE_PARAMS.

    return true;
}

bool Vendors::EnableAntiLag(bool enable) noexcept
{
    if (!ags.module)
    {
        if (enable)
        {
            OutputDebugStringW(L"[Atlas][Vendor] AMD AGS not loaded; cannot enable Anti-Lag.\n");
        }

        g_antiLagEnabled = false;
        return false;
    }

    g_antiLagEnabled = enable;
    OutputDebugStringW(enable ? L"[Atlas][Vendor] AMD Anti-Lag requested (AGS hook pending).\n" : L"[Atlas][Vendor] AMD Anti-Lag disabled.\n");

    // TODO: Wire EnableAntiLag to the AGS SDK low-latency entry point once it is redistributed.
    // AGS 6 exposes agsDriverExtensionsDX12_SetSleepMode which mirrors NvAPI_D3D_SetSleepMode.

    return true;
}
