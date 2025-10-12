// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "BackendD3D12.h"

#include <string>
#include <cstdlib>

#include "../../types/inc/Viewport.hpp"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

using namespace Microsoft::Console::Render::Atlas;

namespace
{
    // Helper function to get hardware adapter
    Microsoft::WRL::ComPtr<IDXGIAdapter1> GetHardwareAdapter(IDXGIFactory4* factory)
    {
        Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;

        for (UINT adapterIndex = 0; SUCCEEDED(factory->EnumAdapters1(adapterIndex, &adapter)); ++adapterIndex)
        {
            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);

            // Skip software adapter
            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                continue;
            }

            // Check if adapter supports D3D12
            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
            {
                return adapter;
            }
        }

        return nullptr;
    }

    struct QuadVertex
    {
        f32x2 position;
    };

    constexpr QuadVertex g_quadVertices[] = {
        {{ 0.0f, 0.0f }},
        {{ 1.0f, 0.0f }},
        {{ 1.0f, 1.0f }},
        {{ 0.0f, 1.0f }},
    };

    constexpr u16 g_quadIndices[] = {
        0, 1, 2,
        2, 3, 0,
    };
}

void BackendD3D12::_refreshDirectStorageStatus() noexcept
{
    std::wstring baseStatus;
    if (_directStorage)
    {
        baseStatus.assign(_directStorage->Status());
    }

    const auto snapshot = Telemetry::GetDirectStorageCacheSnapshot();
    std::wstring composed = baseStatus;

    const auto appendSeparator = [&]() {
        if (!composed.empty())
        {
            composed.append(L" | ");
        }
    };

    if (!snapshot.status.empty() && snapshot.status != baseStatus)
    {
        appendSeparator();
        composed.append(snapshot.status);
    }

    if (snapshot.cacheHits || snapshot.cacheMisses || snapshot.lastCopyMilliseconds > 0.0)
    {
        appendSeparator();
        wchar_t buffer[128];
        if (snapshot.lastCopyMilliseconds > 0.0)
        {
            swprintf_s(buffer, L"Cache hits=%llu misses=%llu last=%.2fms",
                       static_cast<unsigned long long>(snapshot.cacheHits),
                       static_cast<unsigned long long>(snapshot.cacheMisses),
                       snapshot.lastCopyMilliseconds);
        }
        else
        {
            swprintf_s(buffer, L"Cache hits=%llu misses=%llu",
                       static_cast<unsigned long long>(snapshot.cacheHits),
                       static_cast<unsigned long long>(snapshot.cacheMisses));
        }
        composed.append(buffer);
    }

    const std::lock_guard guard{ _directStorageMutex };
    _directStorageStatus = std::move(composed);
}

std::wstring BackendD3D12::DirectStorageStatus() const
{
    const std::lock_guard guard{ _directStorageMutex };
    return _directStorageStatus;
}

VendorDiagnostics BackendD3D12::GetVendorStatus() const noexcept
{
    VendorDiagnostics diagnostics;
    diagnostics.vendor = _vendorName;
    diagnostics.nvapiAvailable = _vendorCaps.nvapiAvailable;
    diagnostics.agsAvailable = _vendorCaps.agsAvailable;
    diagnostics.reflexEnabled = _runtime.reflexLowLatency;
    diagnostics.antiLagEnabled = _runtime.amdAntiLag;
    return diagnostics;
}

void BackendD3D12::EnableVariableRateShading(bool enabled) noexcept
{
    if (!SupportsVariableRateShading())
    {
        _runtime.variableRateShadingEnabled = false;
        return;
    }

    _runtime.variableRateShadingEnabled = enabled;
}

void BackendD3D12::EnableSamplerFeedback(bool enabled) noexcept
{
    if (!SupportsSamplerFeedback())
    {
        _runtime.samplerFeedbackEnabled = false;
        return;
    }

    _runtime.samplerFeedbackEnabled = enabled;
}

// Constructor
BackendD3D12::BackendD3D12(const RenderingPayload& p)
{
#ifdef _DEBUG
    _enableDebugLayer();
#endif

    _createDevice();
    _createCommandQueues();
    _createSwapChain(p);
    _createDescriptorHeaps();
    _createFrameResources();
    _createSynchronizationObjects();
    _createRootSignature();
    _createPipelineStates();
    _createResources();

    _directStorage = std::make_unique<Storage::DirectStorageManager>();
    const bool directStorageReady = _directStorage->Initialize(_device.Get());
    _refreshDirectStorageStatus();
    _runtime.directStorageCacheEnabled = _directStorage ? _directStorage->CacheEnabled() : true;

    std::wstring statusForTelemetry;
    {
        const std::lock_guard guard{ _directStorageMutex };
        if (!directStorageReady)
        {
            _directStorageStatus.append(L" (disabled)");
        }
        statusForTelemetry = _directStorageStatus;
    }

#ifdef _DEBUG
    if (!statusForTelemetry.empty())
    {
        std::wstring message(statusForTelemetry);
        message.append(L"\n");
        OutputDebugStringW(message.c_str());
    }
#endif

    Telemetry::ReportDirectStorageEvent({ statusForTelemetry, directStorageReady, false });

    std::wstring_view vendorName = L"Unknown";
    switch (_vendorCaps.vendor)
    {
    case Vendors::Vendor::Nvidia:
        vendorName = L"NVIDIA";
        break;
    case Vendors::Vendor::AMD:
        vendorName = L"AMD";
        break;
    case Vendors::Vendor::Intel:
        vendorName = L"Intel";
        break;
    default:
        vendorName = L"Unknown";
        break;
    }

    _vendorName = vendorName;

    std::wstring vendorMessage = L"[Vendor] ";
    vendorMessage.append(_vendorName);
    vendorMessage.append(L" | NVAPI=");
    vendorMessage.append(_vendorCaps.nvapiAvailable ? L"yes" : L"no");
    vendorMessage.append(L" | AGS=");
    vendorMessage.append(_vendorCaps.agsAvailable ? L"yes" : L"no");
    vendorMessage.append(L"\n");
    OutputDebugStringW(vendorMessage.c_str());

    _runtime.reflexLowLatency = false;
    _runtime.amdAntiLag = false;

    Telemetry::ReportVendorEvent({ _vendorName, _runtime.reflexLowLatency, _runtime.amdAntiLag });

    // Initialize state
    _state.width = p.s->targetSize.x;
    _state.height = p.s->targetSize.y;
    _state.vsync = true;
}

// Destructor
BackendD3D12::~BackendD3D12()
{
    // Wait for GPU to finish all work before cleanup
    _waitForGpu();

    if (_directStorage)
    {
        _directStorage->WaitForIdle();
        _directStorage->Shutdown();
        _refreshDirectStorageStatus();
    }

    Vendors::Shutdown();

    _vertexBuffer.Reset();
    _indexBuffer.Reset();

    // Release synchronization objects
    if (_fenceEvent)
    {
        CloseHandle(_fenceEvent);
        _fenceEvent = nullptr;
    }

    if (_computeFenceEvent)
    {
        CloseHandle(_computeFenceEvent);
        _computeFenceEvent = nullptr;
    }

    if (_copyFenceEvent)
    {
        CloseHandle(_copyFenceEvent);
        _copyFenceEvent = nullptr;
    }
}

void BackendD3D12::ReleaseResources() noexcept
{
    try
    {
        _waitForGpu();

    if (_directStorage)
    {
        _directStorage->WaitForIdle();
        _directStorage->Shutdown();
        _refreshDirectStorageStatus();
    }

    // Clear batch data
        _instances.clear();
        _batches.clear();

        // Release D3D12 resources
        _glyphAtlas.Reset();
        _glyphAtlasUploadBuffer.Reset();
        _vertexBuffer.Reset();
        _indexBuffer.Reset();
        _instanceBuffer.Reset();
        _instanceUploadBuffer.Reset();
        _vsConstantBuffer.Reset();
        _psConstantBuffer.Reset();
        _customConstantBuffer.Reset();

        _vertexBufferView = {};
        _indexBufferView = {};

        // Release PSOs and root signature
        _backgroundPSO.Reset();
        _textGrayscalePSO.Reset();
        _textClearTypePSO.Reset();
        _cursorPSO.Reset();
        _linePSO.Reset();
        _computePSO.Reset();
        _rootSignature.Reset();

        // Release descriptor heaps
        _rtvHeap.Reset();
        _cbvSrvUavHeap.Reset();
        _samplerHeap.Reset();

        // Release frame resources
        for (auto& frame : _frameResources)
        {
            frame.commandAllocator.Reset();
            frame.renderTarget.Reset();
        }

        // Release command lists
        _commandList.Reset();
        _computeCommandList.Reset();
        _staticBundle.Reset();

        // Release core objects
        _fence.Reset();
        _computeFence.Reset();
        _copyFence.Reset();
        _swapChain.Reset();
        _commandQueue.Reset();
        _computeQueue.Reset();
        _copyQueue.Reset();
        _device.Reset();

#ifdef _DEBUG
        _debugController.Reset();
#endif
    }
    catch (...)
    {
        // Swallow exceptions in cleanup
    }
}

bool BackendD3D12::RequiresContinuousRedraw() noexcept
{
    // Return true if custom shader uses time-based animation
    return false;
}

void BackendD3D12::Render(RenderingPayload& payload)
{
    _beginFrame();
    _populateCommandList(payload);
    _executeCommandLists();
    _present();
    _moveToNextFrame();
}

// ============================================================================
// Initialization Methods
// ============================================================================

#ifdef _DEBUG
void BackendD3D12::_enableDebugLayer()
{
    Microsoft::WRL::ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
    {
        debugController->EnableDebugLayer();
        _debugController = debugController;
    }
}
#endif

void BackendD3D12::_createDevice()
{
    // Create DXGI factory
    Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
    THROW_IF_FAILED(CreateDXGIFactory2(
#ifdef _DEBUG
        DXGI_CREATE_FACTORY_DEBUG,
#else
        0,
#endif
        IID_PPV_ARGS(&factory)));

    // Get hardware adapter
    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter = GetHardwareAdapter(factory.Get());
    if (!adapter)
    {
        THROW_HR(E_FAIL); // No compatible adapter found
    }

    DXGI_ADAPTER_DESC1 adapterDesc{};
    THROW_IF_FAILED(adapter->GetDesc1(&adapterDesc));
    _adapterLuid = adapterDesc.AdapterLuid;

    // Create D3D12 device
    THROW_IF_FAILED(D3D12CreateDevice(
        adapter.Get(),
        D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(&_device)));

    _queryFeatureSupport();

    Vendors::Shutdown();
    _vendorCaps = Vendors::Initialize(_adapterLuid);

    // Get descriptor sizes (these are hardware-dependent)
    _rtvDescriptorSize = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    _cbvSrvUavDescriptorSize = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    _samplerDescriptorSize = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
}

void BackendD3D12::_queryFeatureSupport()
{
    if (!_device)
    {
        return;
    }

    D3D12_FEATURE_DATA_D3D12_OPTIONS6 options6{};
    if (SUCCEEDED(_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS6, &options6, sizeof(options6))))
    {
        _features.variableShadingRateTier = options6.VariableShadingRateTier;
    }
    else
    {
        Telemetry::ReportDirectStorageEvent({ L"CheckFeatureSupport(D3D12_OPTIONS6) failed", true, false });
    }

    D3D12_FEATURE_DATA_D3D12_OPTIONS7 options7{};
    if (SUCCEEDED(_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &options7, sizeof(options7))))
    {
        _features.meshShaderTier = options7.MeshShaderTier;
        _features.samplerFeedbackTier = options7.SamplerFeedbackTier;
    }
#ifdef _DEBUG
    
    std::wstring featureMessage = L"[Features] VRS tier=";
    featureMessage.append(std::to_wstring(static_cast<int>(_features.variableShadingRateTier)));
    featureMessage.append(L" mesh tier=");
    featureMessage.append(std::to_wstring(static_cast<int>(_features.meshShaderTier)));
    featureMessage.append(L" sampler feedback tier=");
    featureMessage.append(std::to_wstring(static_cast<int>(_features.samplerFeedbackTier)));
    featureMessage.append(L"\n");
    OutputDebugStringW(featureMessage.c_str());
#endif

#ifdef D3D12_FEATURE_D3D12_OPTIONS14
    D3D12_FEATURE_DATA_D3D12_OPTIONS14 options14{};
    if (SUCCEEDED(_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS14, &options14, sizeof(options14))))
    {
#ifdef D3D12_WORK_GRAPHS_TIER
        _features.supportsWorkGraphs = options14.WorkGraphsTier != D3D12_WORK_GRAPHS_TIER_NONE;
#else
        _features.supportsWorkGraphs = false;
#endif
    }
#endif
}

void BackendD3D12::_applyVendorOptions(const RenderingPayload& payload)
{
    const bool desiredReflex = payload.s->misc->enableVendorReflex && _vendorCaps.nvapiAvailable;
    if (_runtime.reflexLowLatency != desiredReflex)
    {
        const bool success = Vendors::EnableReflex(desiredReflex);
        if (success || !desiredReflex)
        {
            _runtime.reflexLowLatency = desiredReflex;
            Telemetry::ReportVendorEvent({ _vendorName, _runtime.reflexLowLatency, _runtime.amdAntiLag });
        }
        else
        {
            OutputDebugStringW(L"[Atlas][Vendor] Failed to toggle NVIDIA Reflex via NVAPI.\n");
        }
    }

    const bool desiredAntiLag = payload.s->misc->enableVendorAntiLag && _vendorCaps.agsAvailable;
    if (_runtime.amdAntiLag != desiredAntiLag)
    {
        const bool success = Vendors::EnableAntiLag(desiredAntiLag);
        if (success || !desiredAntiLag)
        {
            _runtime.amdAntiLag = desiredAntiLag;
            Telemetry::ReportVendorEvent({ _vendorName, _runtime.reflexLowLatency, _runtime.amdAntiLag });
        }
        else
        {
            OutputDebugStringW(L"[Atlas][Vendor] Failed to toggle AMD Anti-Lag via AGS.\n");
        }
    }

    if (_directStorage)
    {
        const bool desiredCacheEnabled = payload.s->misc->directStorageCacheEnabled;
        if (_runtime.directStorageCacheEnabled != desiredCacheEnabled)
        {
            const bool success = _directStorage->SetCacheEnabled(desiredCacheEnabled);
            if (success)
            {
                _runtime.directStorageCacheEnabled = desiredCacheEnabled;
                _refreshDirectStorageStatus();
            }
            else if (_directStorage->IsAvailable())
            {
                OutputDebugStringW(L"[Atlas][Storage] Failed to update DirectStorage cache state.\n");
            }
        }
    }
}


void BackendD3D12::SetDirectStorageCacheEnabled(bool enabled) noexcept
{
    if (!_directStorage)
    {
        return;
    }

    if (_runtime.directStorageCacheEnabled == enabled)
    {
        return;
    }

    if (_directStorage->SetCacheEnabled(enabled))
    {
        _runtime.directStorageCacheEnabled = enabled;
        _refreshDirectStorageStatus();
    }
    else if (_directStorage->IsAvailable())
    {
        OutputDebugStringW(L"[Atlas][Storage] Failed to update DirectStorage cache state.\n");
    }
}

void BackendD3D12::ClearDirectStorageCache() noexcept
{
    if (_directStorage)
    {
        if (_directStorage->ClearCache())
        {
            _refreshDirectStorageStatus();
        }
        else if (_directStorage->IsAvailable())
        {
            OutputDebugStringW(L"[Atlas][Storage] Failed to clear DirectStorage cache.\n");
        }
    }
}

void BackendD3D12::_createCommandQueues()
{
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.NodeMask = 0;

    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    THROW_IF_FAILED(_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&_commandQueue)));

    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
    THROW_IF_FAILED(_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&_computeQueue)));

    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
    THROW_IF_FAILED(_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&_copyQueue)));
}

void BackendD3D12::_createSwapChain(const RenderingPayload& p)
{
    // Create DXGI factory (needed for swap chain creation)
    Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
    THROW_IF_FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)));

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = p.s->targetSize.x;
    swapChainDesc.Height = p.s->targetSize.y;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.Stereo = FALSE;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = FrameCount;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    swapChainDesc.Flags = 0;

    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain;
    THROW_IF_FAILED(factory->CreateSwapChainForHwnd(
        _commandQueue.Get(),
        static_cast<HWND>(p.s->targetWindow),
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain));

    // Disable Alt+Enter fullscreen toggle
    THROW_IF_FAILED(factory->MakeWindowAssociation(static_cast<HWND>(p.s->targetWindow), DXGI_MWA_NO_ALT_ENTER));

    // Query for IDXGISwapChain3 interface
    THROW_IF_FAILED(swapChain.As(&_swapChain));
    _currentFrameIndex = _swapChain->GetCurrentBackBufferIndex();
}

void BackendD3D12::_createDescriptorHeaps()
{
    // Create RTV descriptor heap (one per frame buffer)
    {
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.NumDescriptors = FrameCount;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        rtvHeapDesc.NodeMask = 0;

        THROW_IF_FAILED(_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&_rtvHeap)));
    }

    // Create CBV/SRV/UAV descriptor heap (GPU-visible)
    // Allocations:
    // - 3 constant buffers (VS, PS, Custom)
    // - 1 glyph atlas SRV
    // - Future: additional SRVs/UAVs for compute shaders
    {
        D3D12_DESCRIPTOR_HEAP_DESC cbvSrvUavHeapDesc = {};
        cbvSrvUavHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        cbvSrvUavHeapDesc.NumDescriptors = DescriptorIndices::Count;
        cbvSrvUavHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        cbvSrvUavHeapDesc.NodeMask = 0;

        THROW_IF_FAILED(_device->CreateDescriptorHeap(&cbvSrvUavHeapDesc, IID_PPV_ARGS(&_cbvSrvUavHeap)));
    }

    // Create sampler descriptor heap (GPU-visible)
    {
        D3D12_DESCRIPTOR_HEAP_DESC samplerHeapDesc = {};
        samplerHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
        samplerHeapDesc.NumDescriptors = 16; // Room for multiple samplers
        samplerHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        samplerHeapDesc.NodeMask = 0;

        THROW_IF_FAILED(_device->CreateDescriptorHeap(&samplerHeapDesc, IID_PPV_ARGS(&_samplerHeap)));
    }
}

void BackendD3D12::_createFrameResources()
{
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = _rtvHeap->GetCPUDescriptorHandleForHeapStart();

    // Create render target view for each frame
    for (u32 i = 0; i < FrameCount; ++i)
    {
        // Get render target from swap chain
        THROW_IF_FAILED(_swapChain->GetBuffer(i, IID_PPV_ARGS(&_frameResources[i].renderTarget)));

        // Create RTV
        _device->CreateRenderTargetView(_frameResources[i].renderTarget.Get(), nullptr, rtvHandle);
        _frameResources[i].rtvHandle = rtvHandle;

        // Advance handle to next descriptor
        rtvHandle.ptr += _rtvDescriptorSize;

        // Create command allocator for this frame
        THROW_IF_FAILED(_device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&_frameResources[i].commandAllocator)));
    }

    // Create command list using the first frame's allocator
    THROW_IF_FAILED(_device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        _frameResources[0].commandAllocator.Get(),
        nullptr,
        IID_PPV_ARGS(&_commandList)));

    // Command lists are created in recording state - close it for now
    THROW_IF_FAILED(_commandList->Close());

    // Create compute command list (for async compute work)
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> computeAllocator;
    THROW_IF_FAILED(_device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_COMPUTE,
        IID_PPV_ARGS(&computeAllocator)));

    THROW_IF_FAILED(_device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_COMPUTE,
        computeAllocator.Get(),
        nullptr,
        IID_PPV_ARGS(&_computeCommandList)));

    THROW_IF_FAILED(_computeCommandList->Close());
}

void BackendD3D12::_createSynchronizationObjects()
{
    // Create fence for GPU synchronization
    THROW_IF_FAILED(_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence)));
    THROW_IF_FAILED(_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_computeFence)));
    THROW_IF_FAILED(_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_copyFence)));
    _fenceValue = 1;
    _computeFenceValue = 1;
    _copyFenceValue = 1;

    // Create event for fence signaling
    _fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    _computeFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    _copyFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!_fenceEvent || !_computeFenceEvent || !_copyFenceEvent)
    {
        THROW_HR(HRESULT_FROM_WIN32(GetLastError()));
    }
}

void BackendD3D12::_createRootSignature()
{
    // Root parameter layout:
    // [0] CBV: VS constant buffer (b0)
    // [1] CBV: PS constant buffer (b1)
    // [2] CBV: Custom constant buffer (b2)
    // [3] Descriptor table: SRVs (glyph atlas, etc.)
    // [4] Descriptor table: UAVs (glyph compute outputs)
    // [5] Descriptor table: Samplers

    D3D12_DESCRIPTOR_RANGE descriptorRanges[3] = {};

    // SRV descriptor range (glyph atlas and future resources)
    descriptorRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRanges[0].NumDescriptors = 8;
    descriptorRanges[0].BaseShaderRegister = 0; // t0
    descriptorRanges[0].RegisterSpace = 0;
    descriptorRanges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // UAV descriptor range (compute outputs)
    descriptorRanges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    descriptorRanges[1].NumDescriptors = 8;
    descriptorRanges[1].BaseShaderRegister = 0; // u0
    descriptorRanges[1].RegisterSpace = 0;
    descriptorRanges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // Sampler descriptor range
    descriptorRanges[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
    descriptorRanges[2].NumDescriptors = 4;
    descriptorRanges[2].BaseShaderRegister = 0; // s0
    descriptorRanges[2].RegisterSpace = 0;
    descriptorRanges[2].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[6] = {};

    // VS constant buffer
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].Descriptor.ShaderRegister = 0; // b0
    rootParameters[0].Descriptor.RegisterSpace = 0;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    // PS constant buffer
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[1].Descriptor.ShaderRegister = 1; // b1
    rootParameters[1].Descriptor.RegisterSpace = 0;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // Custom constant buffer
    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[2].Descriptor.ShaderRegister = 2; // b2
    rootParameters[2].Descriptor.RegisterSpace = 0;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // SRV descriptor table (glyph atlas)
    rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[3].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[3].DescriptorTable.pDescriptorRanges = &descriptorRanges[0];
    rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // UAV descriptor table (compute outputs)
    rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[4].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[4].DescriptorTable.pDescriptorRanges = &descriptorRanges[1];
    rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // Sampler descriptor table
    rootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[5].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[5].DescriptorTable.pDescriptorRanges = &descriptorRanges[2];
    rootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    rootSignatureDesc.NumParameters = _countof(rootParameters);
    rootSignatureDesc.pParameters = rootParameters;
    rootSignatureDesc.NumStaticSamplers = 0;
    rootSignatureDesc.pStaticSamplers = nullptr;
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    Microsoft::WRL::ComPtr<ID3DBlob> signature;
    Microsoft::WRL::ComPtr<ID3DBlob> error;

    HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
    if (FAILED(hr))
    {
        if (error)
        {
            OutputDebugStringA(static_cast<const char*>(error->GetBufferPointer()));
        }
        THROW_HR(hr);
    }

    THROW_IF_FAILED(_device->CreateRootSignature(
        0,
        signature->GetBufferPointer(),
        signature->GetBufferSize(),
        IID_PPV_ARGS(&_rootSignature)));
}

void BackendD3D12::_createPipelineStates()
{
    // Load compiled shaders (generated by build system as .h files)
    #include "shader_d3d12_vs.h"
    #include "shader_d3d12_ps.h"

    // Common pipeline state configuration
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = _rootSignature.Get();
    psoDesc.VS = { shader_d3d12_vs, sizeof(shader_d3d12_vs) };
    psoDesc.PS = { shader_d3d12_ps, sizeof(shader_d3d12_ps) };
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.RasterizerState.DepthClipEnable = FALSE;
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;

    // Input layout for instance data
    D3D12_INPUT_ELEMENT_DESC inputElements[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "SHADINGTYPE", 0, DXGI_FORMAT_R16_UINT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "RENDITIONSCALE", 0, DXGI_FORMAT_R8G8_UINT, 1, 2, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "INSTANCE_POSITION", 0, DXGI_FORMAT_R16G16_SINT, 1, 4, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "INSTANCE_SIZE", 0, DXGI_FORMAT_R16G16_UINT, 1, 8, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "INSTANCE_TEXCOORD", 0, DXGI_FORMAT_R16G16_UINT, 1, 12, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "INSTANCE_COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 1, 16, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
    };

    psoDesc.InputLayout.pInputElements = inputElements;
    psoDesc.InputLayout.NumElements = _countof(inputElements);

    // Background PSO (opaque rendering)
    {
        psoDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;
        psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

        THROW_IF_FAILED(_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&_backgroundPSO)));
    }

    // Text grayscale PSO (alpha blending)
    {
        psoDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
        psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
        psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
        psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
        psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

        THROW_IF_FAILED(_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&_textGrayscalePSO)));
    }

    // Text ClearType PSO (subpixel rendering)
    {
        // ClearType uses dual-source blending for subpixel rendering
        psoDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
        psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC1_COLOR;
        psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC1_COLOR;
        psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ZERO;
        psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
        psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

        THROW_IF_FAILED(_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&_textClearTypePSO)));
    }

    // Cursor PSO (alpha blending)
    {
        psoDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
        psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
        psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
        psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
        psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

        THROW_IF_FAILED(_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&_cursorPSO)));
    }

    // Line PSO (alpha blending for line rendering)
    {
        psoDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
        psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
        psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
        psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
        psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

        THROW_IF_FAILED(_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&_linePSO)));
    }

    // Compute PSO for grid generation will be added when compute shader is implemented
}

void BackendD3D12::_createResources()
{
    // Create constant buffers (upload heap for CPU write, GPU read)
    const u32 constantBufferSize = (sizeof(VSConstBuffer) + 255) & ~255; // 256-byte aligned

    {
        const D3D12_HEAP_PROPERTIES heapProps = {
            D3D12_HEAP_TYPE_UPLOAD,
            D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
            D3D12_MEMORY_POOL_UNKNOWN,
            0,
            0
        };

        const D3D12_RESOURCE_DESC resourceDesc = {
            D3D12_RESOURCE_DIMENSION_BUFFER,
            0,
            constantBufferSize,
            1,
            1,
            1,
            DXGI_FORMAT_UNKNOWN,
            { 1, 0 },
            D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
            D3D12_RESOURCE_FLAG_NONE
        };

        // VS constant buffer
        THROW_IF_FAILED(_device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&_vsConstantBuffer)));

        // PS constant buffer
        const u32 psConstantBufferSize = (sizeof(PSConstBuffer) + 255) & ~255;
        D3D12_RESOURCE_DESC psResourceDesc = resourceDesc;
        psResourceDesc.Width = psConstantBufferSize;

        THROW_IF_FAILED(_device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &psResourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&_psConstantBuffer)));

        // Custom constant buffer
        const u32 customConstantBufferSize = (sizeof(CustomConstBuffer) + 255) & ~255;
        D3D12_RESOURCE_DESC customResourceDesc = resourceDesc;
        customResourceDesc.Width = customConstantBufferSize;

        THROW_IF_FAILED(_device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &customResourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&_customConstantBuffer)));
    }

    // Create CBVs in descriptor heap
    const auto cbvSrvUavCpuStart = _cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart();

    // VS CBV
    {
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
        cbvDesc.BufferLocation = _vsConstantBuffer->GetGPUVirtualAddress();
        cbvDesc.SizeInBytes = constantBufferSize;

        const auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(cbvSrvUavCpuStart, DescriptorIndices::VsCBV, _cbvSrvUavDescriptorSize);
        _device->CreateConstantBufferView(&cbvDesc, handle);
        _vsConstantBufferCBV = handle;
    }

    // PS CBV
    {
        const u32 psConstantBufferSize = (sizeof(PSConstBuffer) + 255) & ~255;
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
        cbvDesc.BufferLocation = _psConstantBuffer->GetGPUVirtualAddress();
        cbvDesc.SizeInBytes = psConstantBufferSize;

        const auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(cbvSrvUavCpuStart, DescriptorIndices::PsCBV, _cbvSrvUavDescriptorSize);
        _device->CreateConstantBufferView(&cbvDesc, handle);
        _psConstantBufferCBV = handle;
    }

    // Custom CBV
    {
        const u32 customConstantBufferSize = (sizeof(CustomConstBuffer) + 255) & ~255;
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
        cbvDesc.BufferLocation = _customConstantBuffer->GetGPUVirtualAddress();
        cbvDesc.SizeInBytes = customConstantBufferSize;

        const auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(cbvSrvUavCpuStart, DescriptorIndices::CustomCBV, _cbvSrvUavDescriptorSize);
        _device->CreateConstantBufferView(&cbvDesc, handle);
        _customConstantBufferCBV = handle;
    }

    // Glyph atlas SRV/UAV placeholders
    _glyphAtlasSRV = CD3DX12_CPU_DESCRIPTOR_HANDLE(cbvSrvUavCpuStart, DescriptorIndices::GlyphAtlasSRV, _cbvSrvUavDescriptorSize);
    _glyphAtlasUAV = CD3DX12_CPU_DESCRIPTOR_HANDLE(cbvSrvUavCpuStart, DescriptorIndices::GlyphAtlasUAV, _cbvSrvUavDescriptorSize);

    // Create a null UAV descriptor to silence debug layer complaints until resources are bound.
    D3D12_UNORDERED_ACCESS_VIEW_DESC nullUavDesc{};
    _device->CreateUnorderedAccessView(nullptr, nullptr, &nullUavDesc, _glyphAtlasUAV);

    // Create static quad geometry buffers (upload heap for simplicity)
    {
        CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);

        const UINT vertexBufferSize = static_cast<UINT>(sizeof(g_quadVertices));
        auto vertexResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);

        THROW_IF_FAILED(_device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &vertexResourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&_vertexBuffer)));

        void* mappedData = nullptr;
        THROW_IF_FAILED(_vertexBuffer->Map(0, nullptr, &mappedData));
        memcpy(mappedData, g_quadVertices, vertexBufferSize);
        _vertexBuffer->Unmap(0, nullptr);

        _vertexBufferView.BufferLocation = _vertexBuffer->GetGPUVirtualAddress();
        _vertexBufferView.SizeInBytes = vertexBufferSize;
        _vertexBufferView.StrideInBytes = sizeof(QuadVertex);

        const UINT indexBufferSize = static_cast<UINT>(sizeof(g_quadIndices));
        auto indexResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize);

        THROW_IF_FAILED(_device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &indexResourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&_indexBuffer)));

        THROW_IF_FAILED(_indexBuffer->Map(0, nullptr, &mappedData));
        memcpy(mappedData, g_quadIndices, indexBufferSize);
        _indexBuffer->Unmap(0, nullptr);

        _indexBufferView.BufferLocation = _indexBuffer->GetGPUVirtualAddress();
        _indexBufferView.SizeInBytes = indexBufferSize;
        _indexBufferView.Format = DXGI_FORMAT_R16_UINT;
    }

    // Create instance buffer (upload heap)
    {
        const u32 instanceBufferSize = sizeof(QuadInstance) * MaxInstances;

        CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
        auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(instanceBufferSize);

        THROW_IF_FAILED(_device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&_instanceUploadBuffer)));

        // Create instance buffer view
        _instanceBufferView.BufferLocation = _instanceUploadBuffer->GetGPUVirtualAddress();
        _instanceBufferView.SizeInBytes = instanceBufferSize;
        _instanceBufferView.StrideInBytes = sizeof(QuadInstance);
    }

    // Reserve batch storage
    _instances.reserve(MaxInstances);
    _batches.reserve(1024);

    // Create default sampler
    {
        D3D12_CPU_DESCRIPTOR_HANDLE samplerHandle = _samplerHeap->GetCPUDescriptorHandleForHeapStart();

        D3D12_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        samplerDesc.MipLODBias = 0;
        samplerDesc.MaxAnisotropy = 1;
        samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        samplerDesc.BorderColor[0] = 0.0f;
        samplerDesc.BorderColor[1] = 0.0f;
        samplerDesc.BorderColor[2] = 0.0f;
        samplerDesc.BorderColor[3] = 0.0f;
        samplerDesc.MinLOD = 0.0f;
        samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;

        _device->CreateSampler(&samplerDesc, samplerHandle);
    }
}

// ============================================================================
// Frame Rendering Methods
// ============================================================================

void BackendD3D12::_beginFrame()
{
    // Wait for previous frame to complete if GPU is behind
    FrameResource& currentFrame = _frameResources[_currentFrameIndex];

    if (_fence->GetCompletedValue() < currentFrame.fenceValue)
    {
        THROW_IF_FAILED(_fence->SetEventOnCompletion(currentFrame.fenceValue, _fenceEvent));
        WaitForSingleObject(_fenceEvent, INFINITE);
    }

    // Reset command allocator for this frame
    THROW_IF_FAILED(currentFrame.commandAllocator->Reset());

    // Reset command list
    THROW_IF_FAILED(_commandList->Reset(currentFrame.commandAllocator.Get(), nullptr));
}

void BackendD3D12::_populateCommandList(RenderingPayload& payload)
{
    // Set descriptor heaps
    ID3D12DescriptorHeap* heaps[] = { _cbvSrvUavHeap.Get(), _samplerHeap.Get() };
    _commandList->SetDescriptorHeaps(_countof(heaps), heaps);

    // Set root signature
    _commandList->SetGraphicsRootSignature(_rootSignature.Get());

    // Bind constant buffers
    _commandList->SetGraphicsRootConstantBufferView(0, _vsConstantBuffer->GetGPUVirtualAddress());
    _commandList->SetGraphicsRootConstantBufferView(1, _psConstantBuffer->GetGPUVirtualAddress());
    _commandList->SetGraphicsRootConstantBufferView(2, _customConstantBuffer->GetGPUVirtualAddress());

    // Transition render target to render target state
    FrameResource& currentFrame = _frameResources[_currentFrameIndex];
    _transitionResource(currentFrame.renderTarget.Get(),
                       D3D12_RESOURCE_STATE_PRESENT,
                       D3D12_RESOURCE_STATE_RENDER_TARGET);

    // Set render target
    _commandList->OMSetRenderTargets(1, &currentFrame.rtvHandle, FALSE, nullptr);

    // Clear render target
    const float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    _commandList->ClearRenderTargetView(currentFrame.rtvHandle, clearColor, 0, nullptr);

    // Set viewport and scissor
    D3D12_VIEWPORT viewport = {
        0.0f,
        0.0f,
        static_cast<float>(_state.width),
        static_cast<float>(_state.height),
        0.0f,
        1.0f
    };
    _commandList->RSSetViewports(1, &viewport);

    D3D12_RECT scissorRect = {
        0,
        0,
        static_cast<LONG>(_state.width),
        static_cast<LONG>(_state.height)
    };
    _commandList->RSSetScissorRects(1, &scissorRect);

    // Update constant buffers with current frame data
    _updateConstantBuffers(payload);

    // Begin batching instances
    _batchBegin();

    // Add test instances for demonstration
    // In a real implementation, these would come from the text rendering system
    for (u32 i = 0; i < 100; ++i)
    {
        QuadInstance instance{};
        instance.shadingType = static_cast<u16>(ShadingType::Background);
        instance.renditionScale = { 1, 1 };
        instance.position = { static_cast<i16>(i * 10), static_cast<i16>(i * 10) };
        instance.size = { 50, 20 };
        instance.texcoord = { 0, 0 };
        instance.color = 0xFF00FF00; // Green color in RGBA

        _batchAddInstance(instance);
    }

    // Finalize batches and update instance buffer
    _batchEnd();

    // Render all batches
    _batchRender();

    // Transition render target back to present state
    _transitionResource(currentFrame.renderTarget.Get(),
                       D3D12_RESOURCE_STATE_RENDER_TARGET,
                       D3D12_RESOURCE_STATE_PRESENT);
}

void BackendD3D12::_executeCommandLists()
{
    // Close command list
    THROW_IF_FAILED(_commandList->Close());

    // Execute command list
    ID3D12CommandList* commandLists[] = { _commandList.Get() };
    _commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);
}

void BackendD3D12::_present()
{
    // Present frame
    const UINT syncInterval = _state.vsync ? 1 : 0;
    const UINT presentFlags = 0;

    HRESULT hr = _swapChain->Present(syncInterval, presentFlags);

    // Handle device lost scenarios
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
    {
        // Device lost - would need to recreate everything
        // For now, just propagate the error
        THROW_HR(hr);
    }

    THROW_IF_FAILED(hr);
}

void BackendD3D12::_waitForGpu()
{
    auto signalAndWait = [](ID3D12CommandQueue* queue, ID3D12Fence* fence, UINT64& value, HANDLE event)
    {
        if (!queue || !fence || !event)
        {
            return;
        }

        THROW_IF_FAILED(queue->Signal(fence, value));
        THROW_IF_FAILED(fence->SetEventOnCompletion(value, event));
        WaitForSingleObject(event, INFINITE);
        value++;
    };

    signalAndWait(_commandQueue.Get(), _fence.Get(), _fenceValue, _fenceEvent);
    signalAndWait(_computeQueue.Get(), _computeFence.Get(), _computeFenceValue, _computeFenceEvent);
    signalAndWait(_copyQueue.Get(), _copyFence.Get(), _copyFenceValue, _copyFenceEvent);
}

void BackendD3D12::_moveToNextFrame()
{
    // Signal fence for current frame
    const UINT64 currentFenceValue = _fenceValue;
    THROW_IF_FAILED(_commandQueue->Signal(_fence.Get(), currentFenceValue));

    // Store fence value for this frame
    _frameResources[_currentFrameIndex].fenceValue = currentFenceValue;

    // Move to next frame
    _currentFrameIndex = _swapChain->GetCurrentBackBufferIndex();
    _fenceValue++;

    // Update frame count and time tracking
    _frameCount++;
}

// ============================================================================
// Resource Management Methods
// ============================================================================

void BackendD3D12::_updateInstanceBuffer(const QuadInstance* instances, u32 count)
{
    if (count == 0 || count > MaxInstances)
    {
        return;
    }

    // Map instance buffer and copy data
    void* mappedData = nullptr;
    const D3D12_RANGE readRange = { 0, 0 }; // We don't read from this buffer
    THROW_IF_FAILED(_instanceUploadBuffer->Map(0, &readRange, &mappedData));

    memcpy(mappedData, instances, sizeof(QuadInstance) * count);

    _instanceUploadBuffer->Unmap(0, nullptr);

    _instanceCount = count;
}

void BackendD3D12::_updateConstantBuffers(const RenderingPayload& payload)
{
    // Update VS constant buffer
    {
        VSConstBuffer vsData = {};
        vsData.positionScale = f32x2{
            2.0f / static_cast<float>(_state.width),
            -2.0f / static_cast<float>(_state.height)
        };

        void* mappedData = nullptr;
        const D3D12_RANGE readRange = { 0, 0 };
        THROW_IF_FAILED(_vsConstantBuffer->Map(0, &readRange, &mappedData));
        memcpy(mappedData, &vsData, sizeof(VSConstBuffer));
        _vsConstantBuffer->Unmap(0, nullptr);
    }

    // Update PS constant buffer
    {
        PSConstBuffer psData = {};
        // Extract background color from payload
        const auto bgColor = payload.s->misc->backgroundColor;
        psData.backgroundColor = f32x4{
            static_cast<float>(GetRValue(bgColor)) / 255.0f,
            static_cast<float>(GetGValue(bgColor)) / 255.0f,
            static_cast<float>(GetBValue(bgColor)) / 255.0f,
            1.0f
        };

        psData.backgroundCellSize = f32x2{
            static_cast<float>(payload.s->font->cellSize.x),
            static_cast<float>(payload.s->font->cellSize.y)
        };

        psData.backgroundCellCount = f32x2{
            static_cast<float>(payload.s->targetSize.x / payload.s->font->cellSize.x),
            static_cast<float>(payload.s->targetSize.y / payload.s->font->cellSize.y)
        };

        void* mappedData = nullptr;
        const D3D12_RANGE readRange = { 0, 0 };
        THROW_IF_FAILED(_psConstantBuffer->Map(0, &readRange, &mappedData));
        memcpy(mappedData, &psData, sizeof(PSConstBuffer));
        _psConstantBuffer->Unmap(0, nullptr);
    }

    // Update custom constant buffer
    {
        CustomConstBuffer customData = {};
        customData.time = _accumulatedTime;
        customData.resolution = f32x2{
            static_cast<float>(_state.width),
            static_cast<float>(_state.height)
        };

        const auto bgColor = payload.s->misc->backgroundColor;
        customData.background = f32x4{
            static_cast<float>(GetRValue(bgColor)) / 255.0f,
            static_cast<float>(GetGValue(bgColor)) / 255.0f,
            static_cast<float>(GetBValue(bgColor)) / 255.0f,
            1.0f
        };

        void* mappedData = nullptr;
        const D3D12_RANGE readRange = { 0, 0 };
        THROW_IF_FAILED(_customConstantBuffer->Map(0, &readRange, &mappedData));
        memcpy(mappedData, &customData, sizeof(CustomConstBuffer));
        _customConstantBuffer->Unmap(0, nullptr);
    }
}

void BackendD3D12::_transitionResource(
    ID3D12Resource* resource,
    D3D12_RESOURCE_STATES stateBefore,
    D3D12_RESOURCE_STATES stateAfter)
{
    if (stateBefore == stateAfter)
    {
        return;
    }

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = stateBefore;
    barrier.Transition.StateAfter = stateAfter;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    _commandList->ResourceBarrier(1, &barrier);
}

// ============================================================================
// Glyph Atlas Methods (to be implemented)
// ============================================================================

void BackendD3D12::_createGlyphAtlas(u32 width, u32 height)
{
    // Create glyph atlas texture (default heap)
    const D3D12_HEAP_PROPERTIES heapProps = {
        D3D12_HEAP_TYPE_DEFAULT,
        D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
        D3D12_MEMORY_POOL_UNKNOWN,
        0,
        0
    };

    const D3D12_RESOURCE_DESC resourceDesc = {
        D3D12_RESOURCE_DIMENSION_TEXTURE2D,
        0,
        width,
        height,
        1,
        1,
        DXGI_FORMAT_R8_UNORM,
        { 1, 0 },
        D3D12_TEXTURE_LAYOUT_UNKNOWN,
        D3D12_RESOURCE_FLAG_NONE
    };

    THROW_IF_FAILED(_device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,  // Start in shader resource state
        nullptr,
        IID_PPV_ARGS(&_glyphAtlas)));

    // Create upload buffer for glyph atlas updates
    const UINT64 uploadBufferSize = GetRequiredIntermediateSize(_glyphAtlas.Get(), 0, 1);

    const D3D12_HEAP_PROPERTIES uploadHeapProps = {
        D3D12_HEAP_TYPE_UPLOAD,
        D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
        D3D12_MEMORY_POOL_UNKNOWN,
        0,
        0
    };

    const D3D12_RESOURCE_DESC uploadResourceDesc = {
        D3D12_RESOURCE_DIMENSION_BUFFER,
        0,
        uploadBufferSize,
        1,
        1,
        1,
        DXGI_FORMAT_UNKNOWN,
        { 1, 0 },
        D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
        D3D12_RESOURCE_FLAG_NONE
    };

    THROW_IF_FAILED(_device->CreateCommittedResource(
        &uploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &uploadResourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&_glyphAtlasUploadBuffer)));

    // Create SRV for glyph atlas
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R8_UNORM;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = 1;

    _device->CreateShaderResourceView(_glyphAtlas.Get(), &srvDesc, _glyphAtlasSRV);
}

void BackendD3D12::_updateGlyphAtlas(const void* data, u32 x, u32 y, u32 width, u32 height)
{
    // Validate inputs
    if (!_glyphAtlas || !_glyphAtlasUploadBuffer || !data || width == 0 || height == 0)
    {
        return;
    }

    // Get the texture description
    const D3D12_RESOURCE_DESC atlasDesc = _glyphAtlas->GetDesc();

    // Validate that the update region is within bounds
    if (x + width > atlasDesc.Width || y + height > atlasDesc.Height)
    {
        return;
    }

    // Calculate the required size for the upload
    const u32 bytesPerPixel = 1; // R8_UNORM format
    const u32 srcPitch = width * bytesPerPixel;
    const u32 alignedPitch = (srcPitch + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1);

    // Map the upload buffer
    void* mappedData = nullptr;
    const D3D12_RANGE readRange = { 0, 0 }; // We won't read from this buffer
    THROW_IF_FAILED(_glyphAtlasUploadBuffer->Map(0, &readRange, &mappedData));

    // Copy the data to the upload buffer with proper row pitch alignment
    const u8* srcData = static_cast<const u8*>(data);
    u8* dstData = static_cast<u8*>(mappedData);

    for (u32 row = 0; row < height; ++row)
    {
        memcpy(dstData + row * alignedPitch, srcData + row * srcPitch, srcPitch);
    }

    // Unmap the upload buffer
    _glyphAtlasUploadBuffer->Unmap(0, nullptr);

    // Transition the glyph atlas from PIXEL_SHADER_RESOURCE to COPY_DEST
    _transitionResource(_glyphAtlas.Get(),
                       D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                       D3D12_RESOURCE_STATE_COPY_DEST);

    // Setup the copy operation
    D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
    srcLocation.pResource = _glyphAtlasUploadBuffer.Get();
    srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    srcLocation.PlacedFootprint.Offset = 0;
    srcLocation.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8_UNORM;
    srcLocation.PlacedFootprint.Footprint.Width = width;
    srcLocation.PlacedFootprint.Footprint.Height = height;
    srcLocation.PlacedFootprint.Footprint.Depth = 1;
    srcLocation.PlacedFootprint.Footprint.RowPitch = alignedPitch;

    D3D12_TEXTURE_COPY_LOCATION dstLocation = {};
    dstLocation.pResource = _glyphAtlas.Get();
    dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dstLocation.SubresourceIndex = 0;

    // Create the source box for the copy region
    D3D12_BOX srcBox = {};
    srcBox.left = 0;
    srcBox.top = 0;
    srcBox.right = width;
    srcBox.bottom = height;
    srcBox.front = 0;
    srcBox.back = 1;

    // Copy from upload buffer to the glyph atlas at the specified location
    _commandList->CopyTextureRegion(&dstLocation, x, y, 0, &srcLocation, &srcBox);

    // Transition the glyph atlas back to PIXEL_SHADER_RESOURCE
    _transitionResource(_glyphAtlas.Get(),
                       D3D12_RESOURCE_STATE_COPY_DEST,
                       D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

bool BackendD3D12::_updateGlyphAtlasFromFile(std::wstring_view path, u32 x, u32 y, u32 width, u32 height, UINT64 fileOffset)
{
    if (!_directStorage || !_directStorage->IsAvailable() || !_glyphAtlas || !_glyphAtlasUploadBuffer || width == 0 || height == 0)
    {
        return false;
    }

    const auto atlasDesc = _glyphAtlas->GetDesc();
    if (x + width > atlasDesc.Width || y + height > atlasDesc.Height)
    {
        return false;
    }

    const UINT64 bytesPerPixel = 1; // R8_UNORM
    const UINT64 fullRowPitch = (static_cast<UINT64>(atlasDesc.Width) * bytesPerPixel + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1);
    const UINT64 destBase = static_cast<UINT64>(y) * fullRowPitch + x;

    for (u32 row = 0; row < height; ++row)
    {
        const UINT64 dstOffset = destBase + static_cast<UINT64>(row) * fullRowPitch;
        const UINT64 srcOffset = fileOffset + static_cast<UINT64>(row) * width;

        if (!_directStorage->EnqueueFileRead(path, _glyphAtlasUploadBuffer.Get(), dstOffset, width, srcOffset))
        {
            Telemetry::ReportDirectStorageEvent({ _directStorage->Status(), true, false });
            return false;
        }
    }

    Telemetry::ReportDirectStorageEvent({ _directStorage->Status(), true, false });
    _directStorage->Submit();
    _directStorage->WaitForIdle();
    Telemetry::ReportDirectStorageEvent({ _directStorage->Status(), false, true });

    _transitionResource(_glyphAtlas.Get(),
                       D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                       D3D12_RESOURCE_STATE_COPY_DEST);

    D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
    srcLocation.pResource = _glyphAtlasUploadBuffer.Get();
    srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    srcLocation.PlacedFootprint.Offset = 0;
    srcLocation.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8_UNORM;
    srcLocation.PlacedFootprint.Footprint.Width = width;
    srcLocation.PlacedFootprint.Footprint.Height = height;
    srcLocation.PlacedFootprint.Footprint.Depth = 1;
    srcLocation.PlacedFootprint.Footprint.RowPitch = (width * bytesPerPixel + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1);

    D3D12_TEXTURE_COPY_LOCATION dstLocation = {};
    dstLocation.pResource = _glyphAtlas.Get();
    dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dstLocation.SubresourceIndex = 0;

    D3D12_BOX srcBox = {};
    srcBox.left = 0;
    srcBox.top = 0;
    srcBox.front = 0;
    srcBox.right = width;
    srcBox.bottom = height;
    srcBox.back = 1;

    _commandList->CopyTextureRegion(&dstLocation, x, y, 0, &srcLocation, &srcBox);

    _transitionResource(_glyphAtlas.Get(),
                       D3D12_RESOURCE_STATE_COPY_DEST,
                       D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    Telemetry::ReportDirectStorageEvent({ L"Glyph upload complete", false, true });
    return true;
}

void BackendD3D12::_clearGlyphAtlas()
{
    if (!_glyphAtlas || !_glyphAtlasUploadBuffer)
    {
        return;
    }

    // Get the texture description
    const D3D12_RESOURCE_DESC atlasDesc = _glyphAtlas->GetDesc();
    const u32 width = static_cast<u32>(atlasDesc.Width);
    const u32 height = static_cast<u32>(atlasDesc.Height);

    // Calculate the buffer size with proper alignment
    const u32 bytesPerPixel = 1; // R8_UNORM format
    const u32 srcPitch = width * bytesPerPixel;
    const u32 alignedPitch = (srcPitch + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1);
    const u32 bufferSize = alignedPitch * height;

    // Map the upload buffer
    void* mappedData = nullptr;
    const D3D12_RANGE readRange = { 0, 0 };
    THROW_IF_FAILED(_glyphAtlasUploadBuffer->Map(0, &readRange, &mappedData));

    // Clear the buffer to zero (black)
    memset(mappedData, 0, bufferSize);

    // Unmap the upload buffer
    _glyphAtlasUploadBuffer->Unmap(0, nullptr);

    // Transition the glyph atlas to COPY_DEST
    _transitionResource(_glyphAtlas.Get(),
                       D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                       D3D12_RESOURCE_STATE_COPY_DEST);

    // Setup the copy operation for the entire texture
    D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
    srcLocation.pResource = _glyphAtlasUploadBuffer.Get();
    srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    srcLocation.PlacedFootprint.Offset = 0;
    srcLocation.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8_UNORM;
    srcLocation.PlacedFootprint.Footprint.Width = width;
    srcLocation.PlacedFootprint.Footprint.Height = height;
    srcLocation.PlacedFootprint.Footprint.Depth = 1;
    srcLocation.PlacedFootprint.Footprint.RowPitch = alignedPitch;

    D3D12_TEXTURE_COPY_LOCATION dstLocation = {};
    dstLocation.pResource = _glyphAtlas.Get();
    dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dstLocation.SubresourceIndex = 0;

    // Copy the entire cleared buffer to the atlas
    _commandList->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, nullptr);

    // Transition back to PIXEL_SHADER_RESOURCE
    _transitionResource(_glyphAtlas.Get(),
                       D3D12_RESOURCE_STATE_COPY_DEST,
                       D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

// ============================================================================
// Batch Rendering Methods (Alacritty-style batch rendering)
// ============================================================================

void BackendD3D12::_batchBegin()
{
    // Clear instance and batch vectors for new frame
    _instances.clear();
    _batches.clear();
    _instanceCount = 0;
}

void BackendD3D12::_batchAddInstance(const QuadInstance& instance)
{
    // Add instance to collection
    // Check if we need to start a new batch (shading type changed)
    if (_instances.size() >= MaxInstances)
    {
        // Flush if we hit the max instance limit
        return;
    }

    // Check if we need to create a new batch or can extend the current one
    if (_batches.empty() || _batches.back().shadingType != static_cast<ShadingType>(instance.shadingType))
    {
        // Start a new batch with different shading type
        BatchedDrawCall batch{};
        batch.instanceOffset = static_cast<u32>(_instances.size());
        batch.instanceCount = 1;
        batch.shadingType = static_cast<ShadingType>(instance.shadingType);
        _batches.push_back(batch);
    }
    else
    {
        // Extend current batch
        _batches.back().instanceCount++;
    }

    _instances.push_back(instance);
    _instanceCount = static_cast<u32>(_instances.size());
}

void BackendD3D12::_batchEnd()
{
    if (_instances.empty())
    {
        return;
    }

    // Update the instance buffer with all collected instances
    _updateInstanceBuffer(_instances.data(), _instanceCount);
}

void BackendD3D12::_batchRender()
{
    if (_instances.empty() || _batches.empty())
    {
        return;
    }

    // Set primitive topology to triangle list
    _commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    if (!_vertexBuffer || !_indexBuffer)
    {
        return;
    }

    // Set vertex and instance buffers
    D3D12_VERTEX_BUFFER_VIEW vertexBuffers[2] = { _vertexBufferView, _instanceBufferView };
    _commandList->IASetVertexBuffers(0, 2, vertexBuffers);

    // Set index buffer
    _commandList->IASetIndexBuffer(&_indexBufferView);

    // Bind glyph atlas SRV if available
    if (_glyphAtlas)
    {
        const auto baseGpu = _cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart();
        const auto srvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(baseGpu, DescriptorIndices::GlyphAtlasSRV, _cbvSrvUavDescriptorSize);
        _commandList->SetGraphicsRootDescriptorTable(3, srvHandle);
    }

    // Bind sampler
    if (_samplerHeap)
    {
        D3D12_GPU_DESCRIPTOR_HANDLE samplerHandle = _samplerHeap->GetGPUDescriptorHandleForHeapStart();
        _commandList->SetGraphicsRootDescriptorTable(5, samplerHandle);
    }

    // Process each batch
    for (const auto& batch : _batches)
    {
        // Select appropriate PSO based on shading type
        ID3D12PipelineState* pso = nullptr;

        switch (batch.shadingType)
        {
        case ShadingType::Background:
        case ShadingType::Default:
            pso = _backgroundPSO.Get();
            break;

        case ShadingType::TextGrayscale:
            pso = _textGrayscalePSO.Get();
            break;

        case ShadingType::TextClearType:
            pso = _textClearTypePSO.Get();
            break;

        case ShadingType::TextBuiltinGlyph:
        case ShadingType::TextPassthrough:
        case ShadingType::SolidLine:
        case ShadingType::FilledRect:
            // These use alpha blending similar to grayscale text
            pso = _textGrayscalePSO.Get();
            break;

        case ShadingType::DottedLine:
        case ShadingType::DashedLine:
        case ShadingType::CurlyLine:
            pso = _linePSO.Get();
            break;

        case ShadingType::Cursor:
            pso = _cursorPSO.Get();
            break;

        default:
            // Fallback to background PSO
            pso = _backgroundPSO.Get();
            break;
        }

        // Bind the PSO
        if (pso)
        {
            _commandList->SetPipelineState(pso);
        }

        // Issue indexed instanced draw call
        // 6 indices per quad (2 triangles), N instances
        // Using triangle list topology with indices: 0,1,2, 2,3,0
        _commandList->DrawIndexedInstanced(6, batch.instanceCount, 0, 0, batch.instanceOffset);
    }

    // Clear instances after rendering
    _instances.clear();
    _batches.clear();
    _instanceCount = 0;
}

// ============================================================================
// Compute Shader Methods (to be implemented)
// ============================================================================

void BackendD3D12::_dispatchGridGeneration()
{
    // Dispatch compute shader for grid generation
    // Implementation will be added in compute shader phase
}

void BackendD3D12::_dispatchGlyphRasterization()
{
    // Dispatch compute shader for glyph rasterization
    // Implementation will be added in compute shader phase
}

// ============================================================================
// Multi-threading Methods (to be implemented)
// ============================================================================

void BackendD3D12::_recordStaticBundle()
{
    // Record static content into reusable bundle
    // Implementation will be added in multi-threading phase
}

void BackendD3D12::_executeStaticBundle()
{
    // Execute pre-recorded static bundle
    // Implementation will be added in multi-threading phase
}
