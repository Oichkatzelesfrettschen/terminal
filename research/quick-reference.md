# Advanced Rendering Techniques - Quick Reference

**For developers implementing optimizations**

---

## Phase 1: Immediate Wins (1-2 months)

### 1. Low-Latency Present Mode

**Goal:** Reduce input-to-display latency by 1-2 frames

```cpp
// Setup
DXGI_SWAP_CHAIN_DESC1 desc = {};
desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // Flip model
desc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT |
             DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

swapChain3->SetMaximumFrameLatency(1); // Lowest latency
HANDLE waitableObject = swapChain3->GetFrameLatencyWaitableObject();

// Render loop
WaitForSingleObjectEx(waitableObject, 1000, TRUE);
ProcessInput(); // Sample input late
RenderFrame();
swapChain3->Present(0, DXGI_PRESENT_ALLOW_TEARING);
```

**Expected:** 1-2 frame latency reduction
**References:** Microsoft "Reduce latency with DXGI 1.3 swap chains"

---

### 2. Dirty Cell Tracking

**Goal:** Only render cells that changed (10x-100x faster sparse updates)

```cpp
struct Cell {
    uint32_t codepoint;
    uint32_t fgColor;
    uint32_t bgColor;
    uint16_t attributes;
    bool dirty;
};

Rect dirtyRegion; // Tracks bounding box of dirty cells

void SetCell(int x, int y, const Cell& cell) {
    if (cells[y * width + x] != cell) {
        cells[y * width + x] = cell;
        cells[y * width + x].dirty = true;
        dirtyRegion.Expand(x, y);
    }
}

void Render() {
    if (dirtyRegion.IsEmpty()) return;

    // Only render dirty region
    for (int y = dirtyRegion.top; y <= dirtyRegion.bottom; y++) {
        for (int x = dirtyRegion.left; x <= dirtyRegion.right; x++) {
            if (cells[y * width + x].dirty) {
                RenderCell(x, y, cells[y * width + x]);
                cells[y * width + x].dirty = false;
            }
        }
    }

    dirtyRegion.Clear();
}
```

**Expected:** 10x-100x faster for sparse updates (cursor blink, typing)

---

### 3. Scroll Optimization (GPU Blit)

**Goal:** 10x faster scrolling

```cpp
void ScrollUp(int numLines) {
    // GPU copy: move existing content up
    D3D12_BOX srcBox = {
        0, numLines * cellHeight, 0,
        termWidth * cellWidth,
        (termRows - numLines) * cellHeight + numLines * cellHeight, 1
    };

    commandList->CopyTextureRegion(
        &destLoc, 0, 0, 0,     // dest: top of screen
        &srcLoc, 0, &srcBox    // src: scroll region
    );

    // Only render new lines at bottom
    RenderLines(termRows - numLines, termRows);
}
```

**Alternative: Ring Buffer**
```cpp
// Maintain framebuffer 2x screen height
// Scroll by changing viewport offset (zero-copy)
viewportOffset = (viewportOffset + numLines * cellHeight) % (2 * termHeight * cellHeight);
SetViewportOffset(viewportOffset);
RenderNewLines(); // Only render new lines
```

**Expected:** 10x faster scrolling (0.1-0.2ms vs 1-2ms)

---

### 4. Glyph Warming

**Goal:** Eliminate first-use stuttering

```cpp
void WarmGlyphCache() {
    std::thread([this]() {
        // Pre-rasterize ASCII
        for (uint32_t ch = 32; ch < 127; ch++) {
            RasterizeGlyph(ch);
        }

        // Pre-rasterize common Unicode (based on locale)
        for (uint32_t ch : commonUnicodeChars) {
            RasterizeGlyph(ch);
        }
    }).detach();
}
```

**Expected:** Smooth first-use experience (eliminate 1-5ms first-use penalty)

---

### 5. Input Sampling Optimization

**Goal:** 0.5-1 frame latency reduction

```cpp
void RenderFrame() {
    WaitForSwapChain();          // Wait for GPU ready

    ProcessKeyboardInput();      // Sample input NOW (as late as possible)
    ProcessMouseInput();

    UpdateTerminal();            // Update state
    Render();                    // Render with latest input
    Present();
}
```

**Expected:** 0.5-1 frame latency reduction

---

## Phase 2: Major Performance (3-4 months)

### 6. GPU Instance Generation

**Goal:** 40-60% CPU overhead reduction

```hlsl
// Compute shader
struct Cell {
    uint codepoint;
    uint fgColor;
    uint bgColor;
    uint attributes;
};

struct Instance {
    float2 position;
    uint glyphIndex;
    uint fgColor;
    uint bgColor;
    uint attributes;
};

StructuredBuffer<Cell> cellBuffer : register(t0);
RWStructuredBuffer<Instance> instanceBuffer : register(u0);

[numthreads(8, 8, 1)]
void GenerateInstances(uint3 id : SV_DispatchThreadID) {
    uint2 cellPos = id.xy;
    uint cellIndex = cellPos.y * termSize.x + cellPos.x;

    Cell cell = cellBuffer[cellIndex];

    Instance inst;
    inst.position = float2(cellPos) * cellSize;
    inst.glyphIndex = cell.codepoint;
    inst.fgColor = cell.fgColor;
    inst.bgColor = cell.bgColor;
    inst.attributes = cell.attributes;

    instanceBuffer[cellIndex] = inst;
}
```

```cpp
// CPU dispatch
commandList->SetPipelineState(computePSO.Get());
commandList->Dispatch((cols + 7) / 8, (rows + 7) / 8, 1);

// Barrier
D3D12_RESOURCE_BARRIER barrier = {};
barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
barrier.UAV.pResource = instanceBuffer.Get();
commandList->ResourceBarrier(1, &barrier);

// Single instanced draw call
commandList->DrawInstanced(4, cols * rows, 0, 0);
```

**Expected:** 40-60% CPU overhead reduction

---

### 7. Async Compute Pipeline

**Goal:** 20-40% better GPU utilization

```cpp
void RenderFramePipelined() {
    // COMPUTE QUEUE: Parse VT sequences
    computeQueue->ExecuteCommandLists(1, &parseCommandList);
    computeQueue->Signal(parseFence.Get(), currentFrame);

    // GRAPHICS QUEUE: Wait for parsing, then render
    graphicsQueue->Wait(parseFence.Get(), currentFrame);
    graphicsQueue->ExecuteCommandLists(1, &renderCommandList);
    graphicsQueue->Signal(renderFence.Get(), currentFrame);

    // COPY QUEUE: Upload new glyphs (concurrent)
    if (hasNewGlyphs)
        copyQueue->ExecuteCommandLists(1, &uploadCommandList);

    // Present
    swapChain->Present(0, DXGI_PRESENT_ALLOW_TEARING);
    currentFrame++;
}
```

**Expected:** 20-40% better GPU utilization, enables pipelining

---

### 8. Bindless Resources (Shader Model 6.6+)

**Goal:** 10-20% CPU overhead reduction

```hlsl
// Shader Model 6.6
Texture2D<float4> glyphAtlases[] : register(t0, space0);

float4 SampleGlyph(uint atlasIndex, uint glyphIndex, float2 uv) {
    return glyphAtlases[atlasIndex].Sample(samplerLinear, uv);
}
```

**Expected:** 10-20% CPU reduction (eliminate descriptor management)

---

### 9. BC4 Texture Compression

**Goal:** 50% memory reduction

```cpp
// Compress glyph atlas (R8 -> BC4)
DirectX::ScratchImage compressedImage;
DirectX::Compress(
    image.GetImages(), image.GetImageCount(), image.GetMetadata(),
    DXGI_FORMAT_BC4_UNORM,  // 4 bits per pixel (8:1 compression)
    DirectX::TEX_COMPRESS_DEFAULT,
    DirectX::TEX_THRESHOLD_DEFAULT,
    compressedImage
);

// Create texture with BC4 format
D3D12_RESOURCE_DESC desc = {};
desc.Format = DXGI_FORMAT_BC4_UNORM;
// ...
```

**Expected:** 50% memory reduction, hardware decompression (zero CPU cost)

---

### 10. LRU Glyph Cache

**Goal:** 50-90% memory savings for Unicode

```cpp
class GlyphCache {
    struct Entry {
        uint32_t codepoint;
        AtlasRect rect;
        uint64_t lastUsed;
    };

    std::unordered_map<uint32_t, Entry> cache;
    std::priority_queue<Entry, std::vector<Entry>, LRUCompare> lruQueue;

    AtlasRect GetGlyph(uint32_t codepoint) {
        auto it = cache.find(codepoint);
        if (it != cache.end()) {
            it->second.lastUsed = frameCounter++;
            return it->second.rect;
        }

        // Cache miss: rasterize and insert
        AtlasRect rect = RasterizeGlyph(codepoint);

        // Evict LRU if cache full
        if (cache.size() >= maxCacheSize) {
            EvictLRU();
        }

        cache[codepoint] = {codepoint, rect, frameCounter++};
        return rect;
    }
};
```

**Expected:** 50-90% memory savings, warm cache on startup

---

## Phase 3: DirectX 12 Migration (4-6 months)

### Key Changes

1. **Explicit resource management**
   - Create descriptor heaps
   - Manage resource lifetimes
   - Manual synchronization (fences)

2. **Command lists and bundles**
   - Record commands into lists
   - Submit to queues
   - Bundle frequently used commands

3. **Root signatures**
   - Define shader resource binding
   - Create pipeline state objects

4. **Multi-queue support**
   - Graphics queue
   - Compute queue
   - Copy queue

**References:**
- Microsoft: "DirectX 12 Programming Guide"
- 3D Game Engine Programming: "Learning DirectX 12"

---

## Phase 4: Cutting-Edge (6-12 months)

### Compute Shader Rendering (Zutty-style)

**Architecture:**
1. Cell buffer in GPU memory (SSBO/StructuredBuffer)
2. CPU updates buffer directly (mapped memory)
3. Compute shader: DispatchCompute(cols, rows, 1)
4. Each shader invocation renders one cell
5. Early-exit if cell unchanged

**Performance:** 6.5ms avg latency, 0.5ms stddev (extremely consistent)

```hlsl
// Compute shader
RWTexture2D<float4> framebuffer : register(u0);
StructuredBuffer<Cell> cells : register(t0);

[numthreads(1, 1, 1)]
void RenderCell(uint3 id : SV_DispatchThreadID) {
    uint cellIndex = id.y * termSize.x + id.x;
    Cell cell = cells[cellIndex];

    // Early-exit if unchanged
    if (!cell.dirty) return;

    // Render glyph to framebuffer
    uint2 pixelPos = id.xy * cellSize;
    RenderGlyphToFramebuffer(pixelPos, cell);
}
```

**References:** Zutty: "How Zutty works"

---

### GPU-Decoupled Architecture (Ghostty-style)

**Architecture:**
1. I/O processing thread (VT parsing)
2. Rendering thread (GPU)
3. Decoupled: I/O doesn't block rendering
4. Maintains 60 FPS even during heavy I/O

**Performance:** 2ms latency, 37% faster than Alacritty

**References:** Ghostty 1.0 release notes

---

## Performance Measurement

### Latency Measurement

```cpp
// Measure input-to-display latency
auto t1 = std::chrono::high_resolution_clock::now();

ProcessInput();
UpdateTerminal();
RenderFrame();
Present();

auto t2 = std::chrono::high_resolution_clock::now();
auto latency = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1);
```

### Frame Timing

```cpp
// Use QueryPerformanceCounter for accurate timing
LARGE_INTEGER freq, t1, t2;
QueryPerformanceFrequency(&freq);

QueryPerformanceCounter(&t1);
RenderFrame();
QueryPerformanceCounter(&t2);

double frameTime = (t2.QuadPart - t1.QuadPart) * 1000.0 / freq.QuadPart;
```

### GPU Profiling

- Use PIX (Performance Investigator for Xbox)
- NVIDIA Nsight Graphics
- RenderDoc
- Intel GPA

---

## Common Pitfalls

### 1. Not Using Flip Model
**Problem:** Traditional bitblt model has 3-4 frame latency
**Solution:** Use DXGI_SWAP_EFFECT_FLIP_DISCARD

### 2. Not Tracking Dirty Cells
**Problem:** Re-rendering entire screen every frame
**Solution:** Track dirty cells, only render changes

### 3. Not Optimizing Scrolling
**Problem:** Re-rendering entire screen on scroll
**Solution:** Use GPU blit to copy existing content

### 4. Incorrect Gamma Handling
**Problem:** Colors look wrong, blending is incorrect
**Solution:** Convert to linear space before blending

### 5. Not Pre-caching Common Glyphs
**Problem:** First-use stuttering
**Solution:** Warm cache with ASCII on startup

### 6. Too Many Draw Calls
**Problem:** CPU overhead from thousands of draw calls
**Solution:** Use instanced rendering or compute shader

### 7. Not Using Compression
**Problem:** High memory usage for glyph atlases
**Solution:** Use BC4 compression for grayscale

### 8. Inefficient Atlas Packing
**Problem:** Wasted atlas space
**Solution:** Use shelf packing algorithm

---

## Benchmarking Targets

| Terminal | Latency | Notes |
|----------|---------|-------|
| Ghostty | 2ms | Fastest |
| Zutty | 6.5ms | Most consistent |
| Target | 2-6ms | Best-in-class |

---

## Useful Resources

### Documentation
- Microsoft DirectX-Specs (GitHub)
- DirectX 12 Programming Guide (Microsoft)
- GPU Gems 3 (NVIDIA)
- AMD GPUOpen articles

### Tools
- PIX (Performance analysis)
- RenderDoc (Graphics debugger)
- DirectXTex (Texture utilities)
- typometer (Terminal latency measurement)

### Open Source
- Ghostty (Zig, Metal/OpenGL)
- Zutty (C++, OpenGL ES compute)
- Alacritty (Rust, OpenGL)
- Windows Terminal (C++, DirectX 11)

---

## Summary Checklist

### Phase 1 (Immediate)
- [ ] Flip model swap chain with waitable object
- [ ] Dirty cell tracking
- [ ] GPU blit scrolling
- [ ] Glyph warming
- [ ] Late input sampling
- [ ] Gamma-correct blending

### Phase 2 (Next)
- [ ] GPU instance generation (compute shader)
- [ ] Async compute pipeline (multi-queue)
- [ ] Bindless resources (SM 6.6+)
- [ ] BC4 texture compression
- [ ] Shelf packing algorithm
- [ ] LRU glyph cache

### Phase 3 (Medium-term)
- [ ] DirectX 12 migration
- [ ] ExecuteIndirect rendering
- [ ] Mesh shader prototype

### Phase 4 (Long-term)
- [ ] Compute shader rendering (Zutty-style)
- [ ] GPU-decoupled architecture (Ghostty-style)
- [ ] Work Graphs (if hardware available)

---

**Full Report:** advanced-rendering-techniques-report.md
**Executive Summary:** executive-summary.md
