# Rendering Architecture Refactoring - Quick Reference

**For:** Ultra-Riced Windows Terminal
**Date:** 2025-10-11
**Full Report:** See [RENDERING_ARCHITECTURE_ANALYSIS.md](RENDERING_ARCHITECTURE_ANALYSIS.md)

---

## Executive Summary

**Recommendation:** Adopt **Hybrid Abstraction Pattern** - three-tier architecture

**Expected Benefits:**
- 85% reduction in code duplication (from ~3,500 lines to ~500 lines)
- < 5% performance overhead
- Easier to add new backends (Vulkan, WebGPU, Metal)
- Single source of truth for shaders (HLSL -> SPIR-V -> GLSL cross-compilation)
- Improved maintainability and testability

**Timeline:** 13-16 weeks (3-4 months)

---

## Three-Tier Architecture

```
Tier 1: Common Utilities (Backend-Agnostic)
    - GlyphAtlas<TextureHandle>      (~800 lines, shared)
    - InstanceBatcher                 (~200 lines, shared)
    - ShaderCompiler                  (~300 lines, shared)
    - FontRasterizer                  (~400 lines, shared)
    Total: ~1,700 lines (shared across all backends)

Tier 2: Abstract Resources (Thin Wrappers)
    - Texture2D<Backend>              (~300 lines per backend)
    - Buffer<Backend>                 (~200 lines per backend)
    - Shader<Backend>                 (~150 lines per backend)
    - Pipeline<Backend>               (~150 lines per backend)
    Total: ~800 lines per backend specialization

Tier 3: Backend Implementations (API-Specific)
    - BackendD3D11                    (~1,500 lines)
    - BackendD3D12                    (~1,500 lines)
    - BackendOpenGL                   (~1,500 lines)
    - BackendVulkan (future)          (~1,500 lines)
    Total: ~1,500 lines per backend
```

**Code Count Comparison:**

| Component | Current | Recommended | Savings |
|-----------|---------|-------------|---------|
| Duplicated code | ~3,500 lines | ~500 lines | **85% reduction** |
| Backend-specific | ~7,500 lines | ~6,000 lines | 20% reduction |
| Shared utilities | 0 lines | ~2,500 lines | New infrastructure |
| **Total** | **~7,500 lines** | **~9,500 lines** | +27% (but much cleaner) |

---

## Phased Refactoring Plan

### Phase 1: Extract Common Utilities (2-3 weeks)
- Extract GlyphAtlas template class
- Extract InstanceBatcher
- Extract shader compilation utilities
- Test: No regressions

**Files to Create:**
- `/src/renderer/atlas/GlyphAtlas.h`
- `/src/renderer/atlas/InstanceBatcher.h`
- `/src/renderer/atlas/InstanceBatcher.cpp`
- `/src/renderer/atlas/ShaderCompiler.h`

### Phase 2: Create Abstract Resources (2-3 weeks)
- Implement Texture2D<Backend>
- Implement Buffer<Backend>
- Migrate BackendD3D to use abstractions
- Test: BackendD3D still works

**Files to Create:**
- `/src/renderer/atlas/Resources.h`
- `/src/renderer/atlas/ResourcesD3D11.cpp`
- `/src/renderer/atlas/ResourcesD3D12.cpp`
- `/src/renderer/atlas/ResourcesOpenGL.cpp`

### Phase 3: Migrate Remaining Backends (3-4 weeks)
- Migrate BackendD3D12
- Migrate BackendOpenGL
- Remove duplicated code
- Test: All backends work

**Files to Update:**
- `/src/renderer/atlas/BackendD3D.h`
- `/src/renderer/atlas/BackendD3D.cpp`
- `/src/renderer/atlas/BackendD3D12.h`
- `/src/renderer/atlas/BackendD3D12.cpp`
- `/src/renderer/atlas/BackendOpenGL.h`
- `/src/renderer/atlas/BackendOpenGL.cpp`

### Phase 4: Shader Cross-Compilation (2 weeks)
- Set up DXC + SPIRV-Cross pipeline
- Migrate shaders to unified HLSL
- Generate GLSL for OpenGL
- Test: Shader output identical

**Files to Create:**
- `/src/renderer/atlas/shaders/hlsl/*.hlsl` (source shaders)
- CMake shader compilation script
- Shader loading infrastructure

### Phase 5: Font Rasterization Abstraction (2 weeks)
- Implement IFontRasterizer interface
- Implement DirectWriteRasterizer (Windows)
- Implement FreeTypeRasterizer (Linux, future)
- Test: Font quality unchanged

**Files to Create:**
- `/src/renderer/atlas/FontRasterizer.h`
- `/src/renderer/atlas/FontRasterizerDirectWrite.cpp`
- `/src/renderer/atlas/FontRasterizerFreeType.cpp` (placeholder)

### Phase 6: Testing & Optimization (2 weeks)
- Performance benchmarks
- Memory usage analysis
- Cross-backend feature parity tests
- Bug fixes

---

## Key Code Examples

### GlyphAtlas Template Usage

```cpp
// BackendD3D.h
class BackendD3D : IBackend {
    GlyphAtlas<wil::com_ptr<ID3D11Texture2D>> _glyphAtlas;
};

// BackendD3D12.h
class BackendD3D12 : IBackend {
    GlyphAtlas<Microsoft::WRL::ComPtr<ID3D12Resource>> _glyphAtlas;
};

// Usage (same for all backends)
auto* entry = _glyphAtlas.AllocateGlyph(glyphIndex, {32, 32});
if (entry) {
    _glyphAtlas.RasterizeGlyph(entry, glyphBitmap.data());
}
```

### Texture2D Resource Wrapper

```cpp
// Before (D3D11-specific, duplicated)
wil::com_ptr<ID3D11Texture2D> _backgroundBitmap;
wil::com_ptr<ID3D11ShaderResourceView> _backgroundBitmapView;

void CreateTexture() {
    D3D11_TEXTURE2D_DESC desc{};
    // ... 10 lines of setup ...
    device->CreateTexture2D(&desc, nullptr, _backgroundBitmap.put());
    device->CreateShaderResourceView(...);
}

// After (backend-agnostic)
Texture2D<D3D11Backend> _backgroundBitmap;

void CreateTexture() {
    _backgroundBitmap.Create(device, width, height, DXGI_FORMAT_R8G8B8A8_UNORM);
}
```

### Shader Cross-Compilation

```
Source (HLSL)          Intermediate (SPIR-V)       Target
    |                         |                       |
quad_vs.hlsl  ----DXC----> quad_vs.spv         +-> quad_vs.cso (D3D11/D3D12)
                               |                |
                               +--SPIRV-Cross---+-> quad_vs.glsl (OpenGL)
                                                |
                                                +-> quad_vs.msl (Metal, future)
```

**Build Command:**
```bash
# Compile to SPIR-V
dxc -T vs_5_0 -spirv quad_vs.hlsl -Fo quad_vs.spv

# Compile to DXBC
dxc -T vs_5_0 quad_vs.hlsl -Fo quad_vs.cso

# Cross-compile to GLSL
spirv-cross quad_vs.spv --output quad_vs.glsl --version 330
```

---

## Risk Mitigation

| Risk | Mitigation |
|------|------------|
| Performance regression | Benchmarks before/after, inline hot paths, profiler analysis |
| Rendering quality loss | Pixel comparison, visual inspection, font quality tests |
| Template compilation errors | Concepts (C++20), unit tests, clear static_asserts |
| Increased build time | Parallel builds, shader caching, pre-built binaries |

---

## Testing Strategy

**Unit Tests:**
```cpp
TEST(GlyphAtlasTest, AllocateGlyph) {
    GlyphAtlas<MockTexture> atlas({.initialWidth = 1024});
    auto* entry = atlas.AllocateGlyph(42, {32, 32});
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->glyphIndex, 42);
}
```

**Integration Tests:**
```cpp
TEST(BackendIntegrationTest, RenderComparison) {
    RenderingPayload payload = CreateTestPayload();

    auto d3d11 = std::make_unique<BackendD3D>();
    auto d3d12 = std::make_unique<BackendD3D12>(payload);

    d3d11->Render(payload);
    auto frame1 = CaptureFrameBuffer();

    d3d12->Render(payload);
    auto frame2 = CaptureFrameBuffer();

    EXPECT_FRAMES_SIMILAR(frame1, frame2, 0.01f);
}
```

**Performance Benchmarks:**
```cpp
TEST(PerformanceTest, RenderingBenchmark) {
    auto backend = std::make_unique<BackendD3D>();
    auto payload = CreateLargePayload();  // 120x30 full of text

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; ++i) {
        backend->Render(payload);
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto avgFrameTime = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0;
    EXPECT_LT(avgFrameTime, 2000.0);  // < 2ms per frame (500 FPS)
}
```

---

## Success Criteria

**Must Have:**
- [ ] No performance regression (< 5% slower)
- [ ] No visual quality degradation
- [ ] All existing backends work identically
- [ ] All unit tests pass
- [ ] Code duplication reduced by > 50%

**Should Have:**
- [ ] Performance improvement (> 10% faster)
- [ ] Reduced binary size (< 10% smaller)
- [ ] Easier to add new backends
- [ ] Improved debuggability
- [ ] Shader hot-reload on all backends

**Nice to Have:**
- [ ] Vulkan backend implementation
- [ ] Metal backend (macOS)
- [ ] WebGPU backend
- [ ] Render graph visualization tools

---

## Rollback Plan

**Immediate (< 1 day):** Revert last commit, re-run CI/CD

**Partial (1 week):** Keep Tier 1 utilities, revert Tier 2 abstractions

**Full (2 weeks):** Archive refactored code, restore original architecture

---

## Key Decisions

1. **Use template-based backend selection** (not virtual function overhead)
2. **HLSL as source language** (cross-compile to GLSL via SPIR-V)
3. **DirectWrite on Windows, FreeType on Linux** (native quality)
4. **Keep existing IBackend interface** (minimal disruption)
5. **Phased migration** (one backend at a time)
6. **Extensive testing** (unit, integration, performance)

---

## Resources

**Full Documentation:**
- [RENDERING_ARCHITECTURE_ANALYSIS.md](RENDERING_ARCHITECTURE_ANALYSIS.md) - Complete 50+ page analysis

**Industry References:**
- Chromium Skia Graphite: https://blog.chromium.org/2025/07/introducing-skia-graphite-chromes.html
- bgfx: https://github.com/bkaradzic/bgfx
- DiligentEngine: https://github.com/DiligentGraphics/DiligentEngine
- SPIRV-Cross: https://github.com/KhronosGroup/SPIRV-Cross

**Tools:**
- DirectX Shader Compiler (DXC): https://github.com/microsoft/DirectXShaderCompiler
- stb_rect_pack: https://github.com/nothings/stb

---

**Next Steps:**
1. Review and approve this refactoring plan
2. Set up performance benchmarking infrastructure
3. Create Phase 1 task breakdown
4. Begin implementation of GlyphAtlas extraction

---

**Questions? See the full analysis document or contact the architecture team.**
