/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- D3D12Engine.cpp

Abstract:
- This module provides the implementation for the D3D12 rendering engine.

Author(s):
- Your Name (your-github-alias) 14-Oct-2025

--*/

#include "precomp.h"
#include "D3D12Engine.h"

using namespace Microsoft::Console::Render;

D3D12Engine::D3D12Engine()
{
}

D3D12Engine::~D3D12Engine()
{
}

[[nodiscard]] HRESULT D3D12Engine::SetHwnd(HWND hwnd)
{
    _hwnd = hwnd;
    return S_OK;
}

void D3D12Engine::SetBackgroundColor(COLORREF color)
{
    _backgroundColor = color;
}

[[nodiscard]] HRESULT D3D12Engine::Initialize()
{
#if defined(_DEBUG)
    // Enable the D3D12 debug layer.
    {
        Microsoft::WRL::ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            debugController->EnableDebugLayer();
        }
    }
#endif

    Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
    RETURN_IF_FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));

    Microsoft::WRL::ComPtr<IDXGIAdapter1> hardwareAdapter;
    for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != factory->EnumAdapters1(adapterIndex, &hardwareAdapter); ++adapterIndex)
    {
        DXGI_ADAPTER_DESC1 desc;
        hardwareAdapter->GetDesc1(&desc);

        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        {
            continue;
        }

        if (SUCCEEDED(D3D12CreateDevice(hardwareAdapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
        {
            break;
        }
    }

    RETURN_IF_FAILED(D3D12CreateDevice(hardwareAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&_device)));

    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    RETURN_IF_FAILED(_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&_commandQueue)));

    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = FrameCount;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    RETURN_IF_FAILED(_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&_rtvHeap)));

    _rtvDescriptorSize = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    RETURN_IF_FAILED(_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_commandAllocator)));

    RETURN_IF_FAILED(_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence)));
    _fenceValue = 1;

    _fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (_fenceEvent == nullptr)
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    RETURN_IF_FAILED(_createRootSignature());

    RETURN_IF_FAILED(_createPipelineState());

    RETURN_IF_FAILED(_createVertexBuffer());

    RETURN_IF_FAILED(_createShaderResources());

    return S_OK;
}

[[nodiscard]] HRESULT D3D12Engine::StartPaint()
{
    RETURN_IF_FAILED(_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _commandAllocator.Get(), _pipelineState.Get(), IID_PPV_ARGS(&_commandList)));

    if (!_swapChain)
    {
        Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
        RETURN_IF_FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));

        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
        swapChainDesc.BufferCount = FrameCount;
        swapChainDesc.Width = 0;
        swapChainDesc.Height = 0;
        swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDesc.SampleDesc.Count = 1;

        Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain;
        RETURN_IF_FAILED(factory->CreateSwapChainForHwnd(
            _commandQueue.Get(),
            _hwnd,
            &swapChainDesc,
            nullptr,
            nullptr,
            &swapChain
        ));

        RETURN_IF_FAILED(swapChain.As(&_swapChain));
    }

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(_rtvHeap->GetCPUDescriptorHandleForHeapStart());

    for (UINT n = 0; n < FrameCount; n++)
    {
        RETURN_IF_FAILED(_swapChain->GetBuffer(n, IID_PPV_ARGS(&_renderTargets[n])));
        _device->CreateRenderTargetView(_renderTargets[n].Get(), nullptr, rtvHandle);
        rtvHandle.Offset(1, _rtvDescriptorSize);
    }

    _frameIndex = _swapChain->GetCurrentBackBufferIndex();

    return S_OK;
}

#include "shader_ps.h"
#include "shader_vs.h"

void D3D12Engine::_drawQuads(const std::span<const D3D12_VERTEX_BUFFER_VIEW> vertexBuffers, const std::span<const D3D12_INDEX_BUFFER_VIEW> indexBuffers)
{
    _commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    _commandList->IASetVertexBuffers(0, vertexBuffers.size(), vertexBuffers.data());
    _commandList->IASetIndexBuffer(indexBuffers.data());
    _commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);
}

void D3D12Engine::_setViewport()
{
    _commandList->RSSetViewports(1, &_viewport);
    _commandList->RSSetScissorRects(1, &_scissorRect);
}

void D3D12Engine::_setRenderTarget()
{
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(_rtvHeap->GetCPUDescriptorHandleForHeapStart(), _frameIndex, _rtvDescriptorSize);
    _commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
}

[[nodiscard]] HRESULT D3D12Engine::_createShaderResources()
{
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = 1;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    RETURN_IF_FAILED(_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&_srvHeap)));

    const UINT constantBufferSize = sizeof(float) * 4 * 256;

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Width = constantBufferSize;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    RETURN_IF_FAILED(_device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&_constantBuffer)));

    CD3DX12_RANGE readRange(0, 0);
    RETURN_IF_FAILED(_constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&_pCbvDataBegin)));

    return S_OK;
}

[[nodiscard]] HRESULT D3D12Engine::_createVertexBuffer()
{
    const float vertices[] =
    {
        0.0f, 0.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 1.0f,
        1.0f, 1.0f, 1.0f, 1.0f,
    };

    const UINT vertexBufferSize = sizeof(vertices);

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Width = vertexBufferSize;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    RETURN_IF_FAILED(_device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&_vertexBuffer)));

    UINT8* pVertexDataBegin;
    CD3DX12_RANGE readRange(0, 0);
    RETURN_IF_FAILED(_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
    memcpy(pVertexDataBegin, vertices, sizeof(vertices));
    _vertexBuffer->Unmap(0, nullptr);

    _vertexBufferView.BufferLocation = _vertexBuffer->GetGPUVirtualAddress();
    _vertexBufferView.StrideInBytes = sizeof(float) * 4;
    _vertexBufferView.SizeInBytes = vertexBufferSize;

    const WORD indices[] =
    {
        0, 1, 2,
        1, 3, 2,
    };

    const UINT indexBufferSize = sizeof(indices);

    resourceDesc.Width = indexBufferSize;

    RETURN_IF_FAILED(_device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&_indexBuffer)));

    UINT8* pIndexDataBegin;
    RETURN_IF_FAILED(_indexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pIndexDataBegin)));
    memcpy(pIndexDataBegin, indices, sizeof(indices));
    _indexBuffer->Unmap(0, nullptr);

    _indexBufferView.BufferLocation = _indexBuffer->GetGPUVirtualAddress();
    _indexBufferView.Format = DXGI_FORMAT_R16_UINT;
    _indexBufferView.SizeInBytes = indexBufferSize;

    return S_OK;
}

[[nodiscard]] HRESULT D3D12Engine::_createPipelineState()
{
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
    psoDesc.pRootSignature = _rootSignature.Get();
    psoDesc.VS = { g_pso_shader_vs, sizeof(g_pso_shader_vs) };
    psoDesc.PS = { g_pso_shader_ps, sizeof(g_pso_shader_ps) };

    D3D12_RASTERIZER_DESC rasterizerDesc = {};
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.RasterizerState = rasterizerDesc;

    D3D12_BLEND_DESC blendDesc = {};
    blendDesc.AlphaToCoverageEnable = FALSE;
    blendDesc.IndependentBlendEnable = FALSE;
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    psoDesc.BlendState = blendDesc;

    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;

    RETURN_IF_FAILED(_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&_pipelineState)));

    return S_OK;
}

[[nodiscard]] HRESULT D3D12Engine::_createRootSignature()
{
    D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
    featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

    if (FAILED(_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
    {
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    }

    CD3DX12_DESCRIPTOR_RANGE1 ranges[3];
    ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
    ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
    ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);

    CD3DX12_ROOT_PARAMETER1 rootParameters[4];
    rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_VERTEX);
    rootParameters[1].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_PIXEL);
    rootParameters[2].InitAsDescriptorTable(1, &ranges[2], D3D12_SHADER_VISIBILITY_PIXEL);
    rootParameters[3].InitAsConstants(1, 1, 0, D3D12_SHADER_VISIBILITY_VERTEX);

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    Microsoft::WRL::ComPtr<ID3DBlob> signature;
    Microsoft::WRL::ComPtr<ID3DBlob> error;
    RETURN_IF_FAILED(D3D12SerializeVersionedRootSignature(&rootSignatureDesc, &signature, &error));
    RETURN_IF_FAILED(_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&_rootSignature)));

    return S_OK;
}

[[nodiscard]] HRESULT D3D12Engine::EndPaint()
{
    RETURN_IF_FAILED(_commandList->Close());

    ID3D12CommandList* ppCommandLists[] = { _commandList.Get() };
    _commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    RETURN_IF_FAILED(_swapChain->Present(1, 0));

    const UINT64 fence = _fenceValue;
    RETURN_IF_FAILED(_commandQueue->Signal(_fence.Get(), fence));
    _fenceValue++;

    if (_fence->GetCompletedValue() < fence)
    {
        RETURN_IF_FAILED(_fence->SetEventOnCompletion(fence, _fenceEvent));
        WaitForSingleObject(_fenceEvent, INFINITE);
    }

    return S_OK;
}

[[nodiscard]] bool D3D12Engine::RequiresContinuousRedraw()
{
    return false;
}

void D3D12Engine::WaitUntilCanRender()
{
}

[[nodiscard]] HRESULT D3D12Engine::PaintBackground()
{
    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(_renderTargets[_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    _commandList->ResourceBarrier(1, &barrier);

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(_rtvHeap->GetCPUDescriptorHandleForHeapStart(), _frameIndex, _rtvDescriptorSize);

    const float clearColor[] = { GetRValue(_backgroundColor) / 255.0f, GetGValue(_backgroundColor) / 255.0f, GetBValue(_backgroundColor) / 255.0f, 1.0f };
    _commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

    return S_OK;
}

[[nodiscard]] HRESULT D3D12Engine::PaintBufferLine(const std::span<const Cluster> /*clusters*/, const til::point /*coord*/, const bool /*fTrimLeft*/, const bool /*lineWrapped*/)
{
    _commandList->SetGraphicsRootSignature(_rootSignature.Get());

    _commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    D3D12_VERTEX_BUFFER_VIEW vertexBufferViews[] = { _vertexBufferView };
    D3D12_INDEX_BUFFER_VIEW indexBufferViews[] = { _indexBufferView };
    _drawQuads(vertexBufferViews, indexBufferViews);

    return S_OK;
}

[[nodiscard]] HRESULT D3D12Engine::PaintBufferGridLines(const GridLineSet /*lines*/, const COLORREF /*color*/, const size_t /*cchLine*/, const til::point /*coordTarget*/)
{
    return S_OK;
}

[[nodiscard]] HRESULT D3D12Engine::PaintSelection(const til::rect& /*rect*/)
{
    return S_OK;
}

[[nodiscard]] HRESULT D3D12Engine::PaintCursor(const CursorOptions& /*options*/)
{
    return S_OK;
}

[[nodiscard]] HRESULT D3D12Engine::UpdateDrawingBrushes(const TextAttribute& /*textAttributes*/, const RenderSettings& /*renderSettings*/, const gsl::not_null<ID2D1RenderTarget*> /*d2dRenderTarget*/, const bool /*isErase*/, const bool /*isSettingDefaultBrushes*/)
{
    return S_OK;
}

[[nodiscard]] HRESULT D3D12Engine::UpdateFont(const FontInfo& /*fontInfo*/, FontInfo& /*fontInfoActual*/)
{
    return S_OK;
}

[[nodiscard]] HRESULT D3D12Engine::UpdateDpi(const int /*iDpi*/)
{
    return S_OK;
}

[[nodiscard]] HRESULT D3D12Engine::UpdateViewport(const til::inclusive_rect& /*srNewViewport*/)
{
    return S_OK;
}

[[nodiscard]] HRESULT D3D12Engine::GetProposedFont(const FontInfo& /*fontInfo*/, FontInfo& /*fontInfoActual*/)
{
    return S_OK;
}

[[nodiscard]] HRESULT D3D12Engine::GetDirtyArea(std::span<const til::rect>& /*area*/)
{
    return S_OK;
}

[[nodiscard]] HRESULT D3D12Engine::GetFontSize(_Out_ til::size& /*pFontSize*/)
{
    return S_OK;
}

[[nodiscard]] HRESULT D3D12Engine::IsGlyphWide(const std::wstring_view /*glyph*/, _Out_ bool* const /*pIsWide*/)
{
    return S_OK;
}

[[nodiscard]] HRESULT D3D12Engine::ScrollFrame()
{
    return S_OK;
}

[[nodiscard]] HRESULT D3D12Engine::InvalidateTitle(const std::wstring_view /*proposedTitle*/)
{
    return S_OK;
}

[[nodiscard]] HRESULT D3D12Engine::UpdateTitle(const std::wstring_view /*newTitle*/)
{
    return S_OK;
}
