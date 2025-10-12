// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

// BackendD3D12 Compute Shader Implementation
// This file contains the complete implementation of compute shader dispatch methods
// for grid generation and glyph rasterization

#include "pch.h"
#include "BackendD3D12.h"

using namespace Microsoft::Console::Render::Atlas;

// ============================================================================
// Compute Shader Infrastructure Implementation
// ============================================================================

void BackendD3D12::_createComputeResources()
{
    // Create compute command allocator
    THROW_IF_FAILED(_device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_COMPUTE,
        IID_PPV_ARGS(&_computeCommandAllocator)));

    // Create compute fence for synchronization
    THROW_IF_FAILED(_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_computeFence)));
    _computeFenceValue = 1;

    // Create compute fence event
    _computeFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!_computeFenceEvent)
    {
        THROW_HR(HRESULT_FROM_WIN32(GetLastError()));
    }

    // Create grid cell buffer (structured buffer for grid cells)
    const u32 gridCellCount = 120 * 40; // Example: 120x40 terminal grid
    const u32 gridCellBufferSize = sizeof(GridCell) * gridCellCount;

    {
        const D3D12_HEAP_PROPERTIES heapProps = {
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
            D3D12_MEMORY_POOL_UNKNOWN,
            0,
            0
        };

        const D3D12_RESOURCE_DESC resourceDesc = {
            D3D12_RESOURCE_DIMENSION_BUFFER,
            0,
            gridCellBufferSize,
            1,
            1,
            1,
            DXGI_FORMAT_UNKNOWN,
            { 1, 0 },
            D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
        };

        THROW_IF_FAILED(_device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&_gridCellBuffer)));
    }

    // Create dirty cell buffer (bit array for tracking dirty cells)
    const u32 dirtyCellBufferSize = ((gridCellCount + 31) / 32) * sizeof(u32);
    {
        const D3D12_HEAP_PROPERTIES heapProps = {
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
            D3D12_MEMORY_POOL_UNKNOWN,
            0,
            0
        };

        const D3D12_RESOURCE_DESC resourceDesc = {
            D3D12_RESOURCE_DIMENSION_BUFFER,
            0,
            dirtyCellBufferSize,
            1,
            1,
            1,
            DXGI_FORMAT_UNKNOWN,
            { 1, 0 },
            D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
        };

        THROW_IF_FAILED(_device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&_dirtyCellBuffer)));
    }

    // Create selection buffer (bit array for terminal selection)
    const u32 selectionBufferSize = ((gridCellCount + 31) / 32) * sizeof(u32);
    {
        const D3D12_HEAP_PROPERTIES heapProps = {
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
            D3D12_MEMORY_POOL_UNKNOWN,
            0,
            0
        };

        const D3D12_RESOURCE_DESC resourceDesc = {
            D3D12_RESOURCE_DIMENSION_BUFFER,
            0,
            selectionBufferSize,
            1,
            1,
            1,
            DXGI_FORMAT_UNKNOWN,
            { 1, 0 },
            D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
        };

        THROW_IF_FAILED(_device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&_selectionBuffer)));
    }

    // Create constant buffers for compute shaders
    _createComputeConstantBuffers();

    // Create UAV/SRV descriptors for compute resources
    _createComputeDescriptors();
}

void BackendD3D12::_createComputeConstantBuffers()
{
    // Create grid constants buffer
    const u32 gridConstantBufferSize = (sizeof(GridConstants) + 255) & ~255;
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
            gridConstantBufferSize,
            1,
            1,
            1,
            DXGI_FORMAT_UNKNOWN,
            { 1, 0 },
            D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
            D3D12_RESOURCE_FLAG_NONE
        };

        THROW_IF_FAILED(_device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&_gridConstantBuffer)));
    }

    // Create glyph constants buffer
    const u32 glyphConstantBufferSize = (sizeof(GlyphConstants) + 255) & ~255;
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
            glyphConstantBufferSize,
            1,
            1,
            1,
            DXGI_FORMAT_UNKNOWN,
            { 1, 0 },
            D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
            D3D12_RESOURCE_FLAG_NONE
        };

        THROW_IF_FAILED(_device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&_glyphConstantBuffer)));
    }
}

void BackendD3D12::_createComputeDescriptors()
{
    // Get current descriptor handle
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = _cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart();

    // Skip existing descriptors (3 CBVs + 1 SRV for glyph atlas)
    cpuHandle.ptr += _cbvSrvUavDescriptorSize * 4;

    // Store offsets for compute descriptors
    _gridCellBufferUAVOffset = 4;
    _dirtyCellBufferUAVOffset = 5;
    _selectionBufferUAVOffset = 6;
    _glyphAtlasUAVOffset = 7;
    _glyphDescriptorsSRVOffset = 8;
    _gridConstantBufferCBVOffset = 9;
    _glyphConstantBufferCBVOffset = 10;

    // Create UAV for grid cell buffer
    {
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.Format = DXGI_FORMAT_UNKNOWN;
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        uavDesc.Buffer.FirstElement = 0;
        uavDesc.Buffer.NumElements = 120 * 40; // Grid cell count
        uavDesc.Buffer.StructureByteStride = sizeof(GridCell);
        uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

        _device->CreateUnorderedAccessView(_gridCellBuffer.Get(), nullptr, &uavDesc, cpuHandle);
        cpuHandle.ptr += _cbvSrvUavDescriptorSize;
    }

    // Create UAV for dirty cell buffer
    {
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.Format = DXGI_FORMAT_R32_UINT;
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        uavDesc.Buffer.FirstElement = 0;
        uavDesc.Buffer.NumElements = ((120 * 40 + 31) / 32);
        uavDesc.Buffer.StructureByteStride = 0;
        uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

        _device->CreateUnorderedAccessView(_dirtyCellBuffer.Get(), nullptr, &uavDesc, cpuHandle);
        cpuHandle.ptr += _cbvSrvUavDescriptorSize;
    }

    // Create UAV for selection buffer
    {
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.Format = DXGI_FORMAT_R32_UINT;
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        uavDesc.Buffer.FirstElement = 0;
        uavDesc.Buffer.NumElements = ((120 * 40 + 31) / 32);
        uavDesc.Buffer.StructureByteStride = 0;
        uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

        _device->CreateUnorderedAccessView(_selectionBuffer.Get(), nullptr, &uavDesc, cpuHandle);
        cpuHandle.ptr += _cbvSrvUavDescriptorSize;
    }

    // Create UAV for glyph atlas (will be created when atlas is initialized)
    // Skip for now
    cpuHandle.ptr += _cbvSrvUavDescriptorSize;

    // Create SRV for glyph descriptors buffer
    // Skip for now
    cpuHandle.ptr += _cbvSrvUavDescriptorSize;

    // Create CBV for grid constants
    {
        const u32 gridConstantBufferSize = (sizeof(GridConstants) + 255) & ~255;
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
        cbvDesc.BufferLocation = _gridConstantBuffer->GetGPUVirtualAddress();
        cbvDesc.SizeInBytes = gridConstantBufferSize;
        _device->CreateConstantBufferView(&cbvDesc, cpuHandle);
        cpuHandle.ptr += _cbvSrvUavDescriptorSize;
    }

    // Create CBV for glyph constants
    {
        const u32 glyphConstantBufferSize = (sizeof(GlyphConstants) + 255) & ~255;
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
        cbvDesc.BufferLocation = _glyphConstantBuffer->GetGPUVirtualAddress();
        cbvDesc.SizeInBytes = glyphConstantBufferSize;
        _device->CreateConstantBufferView(&cbvDesc, cpuHandle);
        cpuHandle.ptr += _cbvSrvUavDescriptorSize;
    }
}

void BackendD3D12::_createComputeRootSignature()
{
    // Compute root signature layout:
    // [0] CBV: Constants buffer (b0)
    // [1] Descriptor table: UAVs (u0-u2)
    // [2] Descriptor table: SRVs (t0-t1)

    D3D12_DESCRIPTOR_RANGE uavRanges[1] = {};
    uavRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    uavRanges[0].NumDescriptors = 3; // Grid cells, dirty cells, selection
    uavRanges[0].BaseShaderRegister = 0; // u0
    uavRanges[0].RegisterSpace = 0;
    uavRanges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE srvRanges[1] = {};
    srvRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRanges[0].NumDescriptors = 2; // Glyph descriptors, glyph data
    srvRanges[0].BaseShaderRegister = 0; // t0
    srvRanges[0].RegisterSpace = 0;
    srvRanges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[3] = {};

    // Constants CBV
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].Descriptor.ShaderRegister = 0; // b0
    rootParameters[0].Descriptor.RegisterSpace = 0;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // UAV descriptor table
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[1].DescriptorTable.pDescriptorRanges = uavRanges;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // SRV descriptor table
    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[2].DescriptorTable.pDescriptorRanges = srvRanges;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    rootSignatureDesc.NumParameters = _countof(rootParameters);
    rootSignatureDesc.pParameters = rootParameters;
    rootSignatureDesc.NumStaticSamplers = 0;
    rootSignatureDesc.pStaticSamplers = nullptr;
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

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
        IID_PPV_ARGS(&_computeRootSignature)));
}

void BackendD3D12::_createComputePipelineStates()
{
    // Load compiled compute shaders
    #include "grid_generate_cs.h"
    #include "glyph_rasterize_cs.h"

    // Create grid generation PSO
    {
        D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
        computePsoDesc.pRootSignature = _computeRootSignature.Get();
        computePsoDesc.CS = { grid_generate_cs, sizeof(grid_generate_cs) };
        computePsoDesc.NodeMask = 0;
        computePsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

        THROW_IF_FAILED(_device->CreateComputePipelineState(&computePsoDesc, IID_PPV_ARGS(&_gridGenerationPSO)));
    }

    // Create glyph rasterization PSO
    {
        D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
        computePsoDesc.pRootSignature = _computeRootSignature.Get();
        computePsoDesc.CS = { glyph_rasterize_cs, sizeof(glyph_rasterize_cs) };
        computePsoDesc.NodeMask = 0;
        computePsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

        THROW_IF_FAILED(_device->CreateComputePipelineState(&computePsoDesc, IID_PPV_ARGS(&_glyphRasterizationPSO)));
    }
}

// ============================================================================
// Compute Shader Dispatch Methods
// ============================================================================

void BackendD3D12::_dispatchGridGeneration()
{
    // Ensure compute resources are initialized
    if (!_computeCommandAllocator || !_computeCommandList || !_gridGenerationPSO)
    {
        return;
    }

    // Reset compute command allocator and list
    THROW_IF_FAILED(_computeCommandAllocator->Reset());
    THROW_IF_FAILED(_computeCommandList->Reset(_computeCommandAllocator.Get(), _gridGenerationPSO.Get()));

    // Set compute root signature
    _computeCommandList->SetComputeRootSignature(_computeRootSignature.Get());

    // Set descriptor heaps
    ID3D12DescriptorHeap* heaps[] = { _cbvSrvUavHeap.Get() };
    _computeCommandList->SetDescriptorHeaps(_countof(heaps), heaps);

    // Update grid constants
    {
        GridConstants constants = {};
        constants.gridDimensions = { 120, 40 }; // Example grid size
        constants.viewportSize = { _state.width, _state.height };
        constants.cellSize = { 8, 16 }; // Example cell size
        constants.positionScale = { 2.0f / _state.width, -2.0f / _state.height };
        constants.backgroundColor = { 0.0f, 0.0f, 0.0f, 1.0f };
        constants.frameNumber = _frameCount;
        constants.flags = 0;
        constants.scrollOffset = { 0, 0 };

        void* mappedData = nullptr;
        const D3D12_RANGE readRange = { 0, 0 };
        THROW_IF_FAILED(_gridConstantBuffer->Map(0, &readRange, &mappedData));
        memcpy(mappedData, &constants, sizeof(GridConstants));
        _gridConstantBuffer->Unmap(0, nullptr);
    }

    // Bind grid constants CBV
    _computeCommandList->SetComputeRootConstantBufferView(0, _gridConstantBuffer->GetGPUVirtualAddress());

    // Bind UAVs for grid generation
    D3D12_GPU_DESCRIPTOR_HANDLE uavHandle = _cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart();
    uavHandle.ptr += _cbvSrvUavDescriptorSize * _gridCellBufferUAVOffset;
    _computeCommandList->SetComputeRootDescriptorTable(1, uavHandle);

    // Calculate dispatch dimensions
    const u32 threadGroupSizeX = 16;
    const u32 threadGroupSizeY = 16;
    const u32 gridWidth = 120;
    const u32 gridHeight = 40;
    const u32 numGroupsX = (gridWidth + threadGroupSizeX - 1) / threadGroupSizeX;
    const u32 numGroupsY = (gridHeight + threadGroupSizeY - 1) / threadGroupSizeY;

    // Dispatch compute shader
    _computeCommandList->Dispatch(numGroupsX, numGroupsY, 1);

    // Insert UAV barrier
    D3D12_RESOURCE_BARRIER uavBarrier = {};
    uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    uavBarrier.UAV.pResource = _gridCellBuffer.Get();
    _computeCommandList->ResourceBarrier(1, &uavBarrier);

    // Transition grid cell buffer from UAV to SRV
    if (_gridCellBuffer)
    {
        D3D12_RESOURCE_BARRIER transition = {};
        transition.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        transition.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        transition.Transition.pResource = _gridCellBuffer.Get();
        transition.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        transition.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        transition.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        _computeCommandList->ResourceBarrier(1, &transition);
    }

    // Close and execute
    THROW_IF_FAILED(_computeCommandList->Close());
    ID3D12CommandList* commandLists[] = { _computeCommandList.Get() };
    _commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

    // Signal fence for compute work completion
    _computeFenceValue++;
    THROW_IF_FAILED(_commandQueue->Signal(_computeFence.Get(), _computeFenceValue));
}

void BackendD3D12::_dispatchGlyphRasterization()
{
    // Ensure compute resources are initialized
    if (!_computeCommandAllocator || !_computeCommandList || !_glyphRasterizationPSO)
    {
        return;
    }

    // Reset compute command allocator and list
    THROW_IF_FAILED(_computeCommandAllocator->Reset());
    THROW_IF_FAILED(_computeCommandList->Reset(_computeCommandAllocator.Get(), _glyphRasterizationPSO.Get()));

    // Set compute root signature
    _computeCommandList->SetComputeRootSignature(_computeRootSignature.Get());

    // Set descriptor heaps
    ID3D12DescriptorHeap* heaps[] = { _cbvSrvUavHeap.Get() };
    _computeCommandList->SetDescriptorHeaps(_countof(heaps), heaps);

    // Update glyph constants
    {
        GlyphConstants constants = {};
        constants.atlasSize = { 2048, 2048 };
        constants.glyphSize = { 32, 32 };
        constants.glyphCount = 256; // Example: ASCII characters
        constants.glyphsPerRow = 2048 / 32;
        constants.gamma = 2.2f;
        constants.contrast = 1.5f;
        constants.subpixelMask = { 0xFF0000, 0x00FF00, 0x0000FF, 0 };
        constants.renderScale = { 1.0f, 1.0f };
        constants.flags = 0x02; // Antialiased grayscale

        void* mappedData = nullptr;
        const D3D12_RANGE readRange = { 0, 0 };
        THROW_IF_FAILED(_glyphConstantBuffer->Map(0, &readRange, &mappedData));
        memcpy(mappedData, &constants, sizeof(GlyphConstants));
        _glyphConstantBuffer->Unmap(0, nullptr);
    }

    // Bind glyph constants CBV
    _computeCommandList->SetComputeRootConstantBufferView(0, _glyphConstantBuffer->GetGPUVirtualAddress());

    // Bind glyph atlas UAV
    D3D12_GPU_DESCRIPTOR_HANDLE uavHandle = _cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart();
    uavHandle.ptr += _cbvSrvUavDescriptorSize * _glyphAtlasUAVOffset;
    _computeCommandList->SetComputeRootDescriptorTable(1, uavHandle);

    // Bind glyph descriptor buffer SRV
    D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = _cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart();
    srvHandle.ptr += _cbvSrvUavDescriptorSize * _glyphDescriptorsSRVOffset;
    _computeCommandList->SetComputeRootDescriptorTable(2, srvHandle);

    // Calculate dispatch dimensions
    const u32 threadGroupSizeX = 8;
    const u32 threadGroupSizeY = 8;
    const u32 atlasWidth = 2048;
    const u32 atlasHeight = 2048;
    const u32 numGroupsX = (atlasWidth + threadGroupSizeX - 1) / threadGroupSizeX;
    const u32 numGroupsY = (atlasHeight + threadGroupSizeY - 1) / threadGroupSizeY;

    // Dispatch compute shader
    _computeCommandList->Dispatch(numGroupsX, numGroupsY, 1);

    // Insert UAV barrier
    D3D12_RESOURCE_BARRIER uavBarrier = {};
    uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    uavBarrier.UAV.pResource = _glyphAtlas.Get();
    _computeCommandList->ResourceBarrier(1, &uavBarrier);

    // Transition glyph atlas from UAV to SRV
    if (_glyphAtlas)
    {
        D3D12_RESOURCE_BARRIER transition = {};
        transition.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        transition.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        transition.Transition.pResource = _glyphAtlas.Get();
        transition.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        transition.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        transition.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        _computeCommandList->ResourceBarrier(1, &transition);
    }

    // Close and execute
    THROW_IF_FAILED(_computeCommandList->Close());
    ID3D12CommandList* commandLists[] = { _computeCommandList.Get() };
    _commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

    // Signal fence for compute work completion
    _computeFenceValue++;
    THROW_IF_FAILED(_commandQueue->Signal(_computeFence.Get(), _computeFenceValue));

    // Optional: Wait for compute to complete
    if (_computeFence->GetCompletedValue() < _computeFenceValue)
    {
        THROW_IF_FAILED(_computeFence->SetEventOnCompletion(_computeFenceValue, _computeFenceEvent));
        WaitForSingleObject(_computeFenceEvent, INFINITE);
    }
}

// ============================================================================
// Cleanup Methods
// ============================================================================

void BackendD3D12::_releaseComputeResources()
{
    // Wait for all compute work to complete
    if (_computeFence && _computeFenceEvent)
    {
        const UINT64 lastCompletedFence = _computeFence->GetCompletedValue();
        if (lastCompletedFence < _computeFenceValue)
        {
            _computeFence->SetEventOnCompletion(_computeFenceValue, _computeFenceEvent);
            WaitForSingleObject(_computeFenceEvent, INFINITE);
        }
    }

    // Release compute fence event
    if (_computeFenceEvent)
    {
        CloseHandle(_computeFenceEvent);
        _computeFenceEvent = nullptr;
    }

    // Release compute resources
    _gridCellBuffer.Reset();
    _dirtyCellBuffer.Reset();
    _selectionBuffer.Reset();
    _gridConstantBuffer.Reset();
    _glyphConstantBuffer.Reset();
    _glyphDescriptorBuffer.Reset();
    _glyphDataBuffer.Reset();

    // Release compute PSOs
    _gridGenerationPSO.Reset();
    _glyphRasterizationPSO.Reset();

    // Release compute root signature
    _computeRootSignature.Reset();

    // Release compute command allocator
    _computeCommandAllocator.Reset();

    // Release compute fence
    _computeFence.Reset();
}