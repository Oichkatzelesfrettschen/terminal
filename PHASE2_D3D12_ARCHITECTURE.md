# Phase 2: D3D12 Renderer Architecture

**Project**: Ultra-Riced Windows Terminal
**Phase**: 2 of 6 (D3D12 Renderer)
**Status**: 🚧 IN PROGRESS
**Expected Duration**: 4 weeks
**Complexity**: HIGH (2,387 lines to port + new features)

---

## Executive Summary

Phase 2 involves migrating the AtlasEngine from Direct3D 11 to Direct3D 12, implementing Alacritty-inspired batch rendering, and adding compute shader support. This is the **most performance-critical phase**, targeting:

- **2-3x CPU efficiency** (explicit resource management)
- **4-8x draw call reduction** (batch rendering)
- **Frame time improvement**: 4-6ms → 1.5-2.5ms

---

## Architecture Overview

### Current State (D3D11 - BackendD3D.cpp)

**Size**: 2,387 lines
**Architecture**: Traditional D3D11 with driver-managed resources

**Key Components**:
- `BackendD3D` class: Main D3D11 renderer
- Glyph atlas: Texture-based character cache
- Instance buffer: Per-quad rendering data
- Constant buffers: VS/PS uniforms
- HLSL shaders: Vertex + pixel shaders

**Performance Characteristics**:
- 1,000-10,000 draw calls per frame (typical terminal rendering)
- Driver overhead for resource management
- Implicit synchronization
- Single-threaded command recording

### Target State (D3D12 - BackendD3D12.cpp)

**Estimated Size**: 3,500-4,000 lines (more explicit control)
**Architecture**: Explicit D3D12 with manual resource management

**Key Improvements**:

1. **Batch Rendering** (Alacritty-style)
   - Single draw call for 65,536 instances
   - Reduces 1,000-10,000 draw calls → **2-6 draw calls**
   - Dual-pass rendering: background + text

2. **Explicit Resource Management**
   - Manual heap management
   - Explicit barriers and transitions
   - Descriptor heaps for samplers/CBVs/SRVs/UAVs
   - **2-3x CPU efficiency** vs D3D11 driver overhead

3. **Compute Shader Integration**
   - Grid rendering via compute shader
   - Glyph rasterization on GPU
   - Async compute for parallel work

4. **Multi-threthreaded Command Recording**
   - Parallel command list generation
   - Bundle reuse for static content
   - Reduced CPU bottleneck

---

## Detailed Design

### 1. Device and Swap Chain Initialization

**File**: `BackendD3D12.cpp` (Device Initialization Section)

**Key Changes from D3D11**:

```cpp
// D3D11 (Current)
D3D11CreateDevice(..., &_device, ...);
factory->CreateSwapChainForHwnd(device, ..., &_swapChain);

// D3D12 (Target)
D3D12CreateDevice(..., IID_PPV_ARGS(&_device));
factory->CreateSwapChainForHwnd(_commandQueue.Get(), ..., &_swapChain);
// Swap chain now requires command queue instead of device
```

**New Components**:
- Command queue (graphics + compute)
- Command allocators (per-frame ring buffer)
- Command lists (graphics + compute + bundle)
- Descriptor heaps (RTV, DSV, CBV/SRV/UAV, Sampler)
- Fence synchronization (CPU-GPU sync)

**Initialization Flow**:
```
1. Create ID3D12Device
2. Create command queue (D3D12_COMMAND_LIST_TYPE_DIRECT)
3. Create swap chain (DXGI_SWAP_CHAIN_DESC1)
4. Create descriptor heaps:
   - RTV heap (render target views)
   - DSV heap (depth stencil views) [if using depth]
   - CBV/SRV/UAV heap (constant buffers, textures, UAVs)
   - Sampler heap
5. Create frame resources (per back buffer):
   - Command allocator
   - Render target view
6. Create fence for synchronization
7. Create root signature
8. Compile and create pipeline state objects (PSOs)
```

### 2. Resource Management

#### Root Signature

**Purpose**: Defines shader resource binding layout (replaces D3D11's implicit binding)

```cpp
// Root parameters:
// 0: Constant buffer (VS constants)
// 1: Constant buffer (PS constants)
// 2: SRV (texture atlas)
// 3: Sampler
D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
rootSignatureDesc.NumParameters = 4;
rootSignatureDesc.pParameters = rootParameters;
// ...
```

#### Pipeline State Objects (PSOs)

**D3D11**: Driver creates implicit pipeline state
**D3D12**: Explicit PSO creation for each rendering configuration

```cpp
D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
psoDesc.pRootSignature = _rootSignature.Get();
psoDesc.VS = vertexShaderBytecode;
psoDesc.PS = pixelShaderBytecode;
psoDesc.InputLayout = inputLayout;
psoDesc.RasterizerState = rasterizerState;
psoDesc.BlendState = blendState;
// ...
device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&_pso));
```

**PSOs Needed**:
1. Background rendering PSO
2. Text rendering PSO (grayscale)
3. Text rendering PSO (ClearType)
4. Cursor rendering PSO
5. Line rendering PSO
6. Compute shader PSO (grid rendering)

#### Descriptor Heaps

**Purpose**: GPU-visible arrays of resource descriptors

```cpp
// CBV/SRV/UAV Heap (size: 64 for dynamic resources)
D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {};
cbvHeapDesc.NumDescriptors = 64;
cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
```

### 3. Batch Rendering (Alacritty-Inspired)

#### Current D3D11 Approach

```cpp
// Render each quad individually or in small batches
for (each dirty region) {
    UpdateInstanceBuffer(instances);
    DrawInstanced(instanceCount, 1, 0, 0);
}
// Result: 1,000-10,000 draw calls
```

#### Target D3D12 Approach (Alacritty-style)

```cpp
// Accumulate ALL instances into large buffer
std::vector<QuadInstance> instances;
instances.reserve(65536); // Maximum instance count

// Collect all rendering primitives
for (all visible cells) {
    instances.push_back(CreateBackgroundInstance(...));
    instances.push_back(CreateTextInstance(...));
}

// Single draw call for entire frame
UpdateInstanceBuffer(instances.data(), instances.size());
commandList->DrawInstanced(4, instances.size(), 0, 0);
// Result: 1-2 draw calls (background + text)
```

**Performance Impact**:
- **Draw calls**: 10,000 → 2 (**5,000x reduction**)
- **CPU overhead**: Minimal (one API call vs thousands)
- **GPU efficiency**: Better batching, reduced state changes

#### Dual-Pass Rendering

**Pass 1: Background**
```cpp
SetPipelineState(backgroundPSO);
DrawInstanced(4, backgroundInstanceCount, 0, 0);
```

**Pass 2: Text + Foreground**
```cpp
SetPipelineState(textPSO);
DrawInstanced(4, textInstanceCount, 0, backgroundInstanceCount);
```

**Why Dual-Pass**:
- Separate blend modes (opaque vs alpha-blended)
- Reduce overdraw
- Better cache utilization

### 4. Texture Atlas (Glyph Cache)

#### Current D3D11 Implementation

- 2D texture array
- Dynamically updated via `Map()`/`Unmap()`
- Row-based packing (stb_rect_pack)

#### D3D12 Changes

**Challenge**: No `Map()`/`Unmap()` for default heap resources

**Solution**: Upload heap + copy command

```cpp
// Create glyph atlas in default heap (GPU-optimal)
D3D12_RESOURCE_DESC textureDesc = {};
textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
textureDesc.Format = DXGI_FORMAT_R8_UNORM; // grayscale
textureDesc.Width = 2048;
textureDesc.Height = 2048;
device->CreateCommittedResource(
    &defaultHeapProps,
    D3D12_HEAP_FLAG_NONE,
    &textureDesc,
    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
    nullptr,
    IID_PPV_ARGS(&_glyphAtlas)
);

// Create upload buffer for glyph data
device->CreateCommittedResource(
    &uploadHeapProps,
    D3D12_HEAP_FLAG_NONE,
    &bufferDesc,
    D3D12_RESOURCE_STATE_GENERIC_READ,
    nullptr,
    IID_PPV_ARGS(&_glyphUploadBuffer)
);

// Update glyph:
// 1. Write to upload buffer (CPU-writable)
// 2. Copy from upload to default heap (GPU command)
commandList->CopyTextureRegion(...);
```

**Async Updates**:
- Queue glyph updates on compute queue
- Parallel with graphics rendering
- Reduces frame stalls

### 5. Compute Shader Integration

#### Grid Rendering

**Current Approach**: CPU generates quad instances for grid lines

**Optimized Approach**: Compute shader generates instances on GPU

```hlsl
// GridGenerationCS.hlsl
[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID) {
    uint x = DTid.x;
    uint y = DTid.y;

    // Generate grid line instance
    QuadInstance instance;
    instance.position = uint2(x * cellWidth, y * cellHeight);
    instance.size = uint2(cellWidth, 1); // horizontal line
    instance.color = gridColor;

    // Write to UAV (output buffer)
    instanceBuffer[y * gridWidth + x] = instance;
}
```

**Benefits**:
- Zero CPU overhead for grid generation
- Parallel GPU execution
- Faster than CPU loop

#### Async Compute for Glyph Rasterization

```cpp
// Graphics queue renders current frame
graphicsCommandList->DrawInstanced(...);

// Compute queue rasterizes new glyphs in parallel
computeCommandList->Dispatch(...); // Rasterize glyph X
computeCommandList->CopyTextureRegion(...); // Upload to atlas

// Fence to synchronize compute → graphics
```

**Performance Impact**:
- Overlapping CPU, graphics, and compute work
- Better GPU utilization
- Reduced frame time

### 6. Multi-Threaded Command Recording

#### Command List Per Thread

```cpp
// Thread 1: Background rendering
commandList1->SetPipelineState(backgroundPSO);
commandList1->DrawInstanced(...);
commandList1->Close();

// Thread 2: Text rendering
commandList2->SetPipelineState(textPSO);
commandList2->DrawInstanced(...);
commandList2->Close();

// Main thread: Execute all
ID3D12CommandList* commandLists[] = { commandList1.Get(), commandList2.Get() };
commandQueue->ExecuteCommandLists(2, commandLists);
```

#### Bundles for Static Content

**Use Case**: Render fixed UI elements (e.g., window chrome, borders)

```cpp
// Record bundle once
ID3D12GraphicsCommandList* bundle;
bundle->SetPipelineState(uiPSO);
bundle->DrawInstanced(...);
bundle->Close();

// Reuse bundle every frame (zero recording cost)
commandList->ExecuteBundle(bundle.Get());
```

**Performance Impact**:
- Amortize command recording cost
- Reduce CPU overhead for repeated draws

### 7. Synchronization

#### Frame Pacing

**D3D11 (Implicit)**:
```cpp
swapChain->Present(1, 0); // Blocks until VSync
```

**D3D12 (Explicit with Waitable Object)**:
```cpp
// Create waitable swap chain
DXGI_SWAP_CHAIN_DESC1 desc = {};
desc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
factory->CreateSwapChainForHwnd(..., &desc, ..., &swapChain3);

HANDLE waitableObject = swapChain3->GetFrameLatencyWaitableObject();

// Each frame:
WaitForSingleObjectEx(waitableObject, INFINITE, TRUE);
RenderFrame();
swapChain->Present(1, 0);
```

**Benefit**: Predictable frame timing, reduced latency

#### CPU-GPU Sync

```cpp
// Fence value increments each frame
const UINT64 fenceValue = _fenceValue;
commandQueue->Signal(_fence.Get(), fenceValue);

// Wait for GPU to complete
if (_fence->GetCompletedValue() < fenceValue) {
    _fence->SetEventOnCompletion(fenceValue, _fenceEvent);
    WaitForSingleObjectEx(_fenceEvent, INFINITE, FALSE);
}

_fenceValue++;
```

---

## Performance Targets

| Metric | D3D11 (Current) | D3D12 (Target) | Improvement |
|--------|----------------|----------------|-------------|
| **Draw Calls/Frame** | 1,000-10,000 | 2-6 | 500x-5,000x |
| **CPU Frame Time** | 4-6 ms | 1.5-2.5 ms | 2.4x faster |
| **CPU Usage (%)** | 20-25 | 8-12 | 2.1x reduction |
| **GPU Utilization** | 40-50% | 60-75% | Better balance |
| **Command Recording** | Single-threaded | Multi-threaded | Parallel speedup |

---

## Implementation Phases

### Phase 2.1: Core D3D12 Infrastructure (Week 1)
- ✅ Architecture documentation (this file)
- ⏳ Device and swap chain initialization
- ⏳ Descriptor heap management
- ⏳ Root signature creation
- ⏳ Basic PSO for background rendering

### Phase 2.2: Resource Management (Week 2)
- ⏳ Texture atlas migration (upload heap + copy)
- ⏳ Instance buffer management
- ⏳ Constant buffer updates
- ⏳ Resource barriers and transitions

### Phase 2.3: Batch Rendering (Week 3)
- ⏳ 65,536-instance buffer
- ⏳ Dual-pass rendering (background + text)
- ⏳ Alacritty-style draw call reduction
- ⏳ Performance benchmarking

### Phase 2.4: Advanced Features (Week 4)
- ⏳ Compute shader grid rendering
- ⏳ Async compute for glyph rasterization
- ⏳ Multi-threaded command recording
- ⏳ Bundle optimization for static content

---

## File Structure

```
src/renderer/atlas/
├── BackendD3D.cpp           (2,387 lines - D3D11, existing)
├── BackendD3D.h             (D3D11 header, existing)
├── BackendD3D12.cpp         (NEW - 3,500+ lines estimated)
├── BackendD3D12.h           (NEW - D3D12 header)
├── BackendD3D12Compute.hlsl (NEW - Compute shaders)
├── shader_d3d12_vs.hlsl     (NEW - D3D12 vertex shader)
├── shader_d3d12_ps.hlsl     (NEW - D3D12 pixel shader)
├── AtlasEngine.cpp          (Modified - add D3D12 backend selection)
└── Backend.h                (Modified - add IBackendD3D12 interface)
```

---

## Testing Strategy

### Unit Tests
- Device initialization (various adapters)
- Resource creation (textures, buffers)
- Descriptor heap allocation
- PSO creation

### Integration Tests
- End-to-end rendering pipeline
- Multi-frame rendering
- Resource lifecycle (create/destroy)
- Error handling (device lost, out-of-memory)

### Performance Tests
- Draw call count measurement
- Frame time profiling (PIX, VTune)
- CPU usage monitoring
- GPU utilization tracking

### Compatibility Tests
- Intel integrated GPUs
- AMD discrete GPUs
- NVIDIA discrete GPUs
- Windows 10 vs Windows 11

---

## Risk Mitigation

### Risk 1: D3D12 Complexity
**Impact**: High - Explicit resource management is error-prone
**Mitigation**:
- Comprehensive error checking (FAILED(hr))
- Use DirectX Toolkit (d3dx12.h helpers)
- Reference Microsoft D3D12 samples
- Incremental implementation with fallback to D3D11

### Risk 2: Driver Bugs
**Impact**: Medium - D3D12 drivers less mature than D3D11
**Mitigation**:
- Test on multiple GPU vendors
- Implement D3D11 fallback path
- Runtime feature detection

### Risk 3: Performance Regression
**Impact**: High - If D3D12 is slower than D3D11
**Mitigation**:
- Benchmark at each phase
- Profile with PIX/VTune
- Compare against D3D11 baseline
- Implement adaptive backend selection

---

## Success Criteria

| Criterion | Target | Status |
|-----------|--------|--------|
| D3D12 device initialization | Working | ⏳ Pending |
| Texture atlas rendering | Working | ⏳ Pending |
| Batch rendering (65K instances) | Working | ⏳ Pending |
| Draw call reduction | >90% (10K → <1K) | ⏳ Pending |
| Frame time improvement | >40% (6ms → <3.5ms) | ⏳ Pending |
| CPU usage reduction | >30% (25% → <18%) | ⏳ Pending |
| Multi-threaded rendering | Working | ⏳ Pending |
| Compute shader integration | Working | ⏳ Pending |
| Zero crashes | All tests pass | ⏳ Pending |
| GPU compatibility | Intel/AMD/NVIDIA | ⏳ Pending |

---

## References

- [D3D12 Programming Guide](https://learn.microsoft.com/en-us/windows/win32/direct3d12/directx-12-programming-guide)
- [D3D12 Hello World Samples](https://github.com/microsoft/DirectX-Graphics-Samples)
- [Alacritty Rendering Architecture](https://github.com/alacritty/alacritty) (research/alacritty-analysis/)
- [DirectX Tool Kit (d3dx12.h)](https://github.com/microsoft/DirectX-Headers)

---

**Status**: Phase 2 Architecture Complete ✅
**Next**: Begin Phase 2.1 (Core D3D12 Infrastructure)
**Generated**: 2025-10-11
