# Modern Rendering Architecture Patterns for Multi-Backend Terminal Rendering

**Document Version:** 1.0
**Date:** 2025-10-11
**Author:** Architecture Analysis
**Target:** Ultra-Riced Windows Terminal

---

## Executive Summary

This document provides a comprehensive analysis of modern rendering architecture patterns for multi-backend terminal rendering, evaluating industry-standard approaches from Chromium, Firefox, game engines, and rendering frameworks. It recommends a **Hybrid Abstraction Pattern** for Windows Terminal that balances code reuse with backend-specific optimizations, providing a clear refactoring path from the current architecture to a more maintainable and extensible system.

**Key Recommendations:**
1. Adopt a three-tier architecture: Common Utilities, Abstract Resource Layer, Backend Implementations
2. Extract shared glyph atlas management into cross-backend utilities
3. Implement a command buffer system for state batching and validation
4. Use template metaprogramming for compile-time backend selection
5. Create abstract resource wrapper classes with backend-specific implementations
6. Implement shader cross-compilation pipeline with SPIR-V as intermediate representation

---

## Table of Contents

1. [Current Architecture Analysis](#1-current-architecture-analysis)
2. [Industry Pattern Research](#2-industry-pattern-research)
3. [Pattern Evaluation for Terminal Rendering](#3-pattern-evaluation-for-terminal-rendering)
4. [Glyph Atlas Management Patterns](#4-glyph-atlas-management-patterns)
5. [Shader Management Patterns](#5-shader-management-patterns)
6. [Resource Management Patterns](#6-resource-management-patterns)
7. [Recommended Architecture](#7-recommended-architecture)
8. [Refactoring Plan](#8-refactoring-plan)
9. [Code Examples](#9-code-examples)
10. [Risk Analysis](#10-risk-analysis)

---

## 1. Current Architecture Analysis

### 1.1 Overview

Windows Terminal currently uses a **Low-Level Backend Abstraction** pattern with the following structure:

```
AtlasEngine (Frontend)
    |
    +-- IBackend (Minimal Interface)
         |
         +-- BackendD2D      (Direct2D - CPU/GPU hybrid)
         +-- BackendD3D      (Direct3D 11)
         +-- BackendD3D12    (Direct3D 12)
         +-- BackendOpenGL   (OpenGL 3.3+)
```

**Interface Definition:**
```cpp
struct IBackend {
    virtual ~IBackend() = default;
    virtual void ReleaseResources() noexcept = 0;
    virtual void Render(RenderingPayload& payload) = 0;
    virtual bool RequiresContinuousRedraw() noexcept = 0;
};
```

### 1.2 Data Flow

```
Terminal State (Text Buffer)
    |
    V
AtlasEngine::StartPaint()          // Lock acquisition
    |
    V
AtlasEngine::PaintBufferLine()     // Text shaping via DirectWrite
    |
    V
AtlasEngine::EndPaint()            // Prepare rendering data
    |
    V
AtlasEngine::Present()             // Call backend
    |
    V
IBackend::Render(RenderingPayload) // Backend-specific rendering
    |
    V
SwapChain::Present()               // Display
```

### 1.3 RenderingPayload Structure

The `RenderingPayload` is the primary data structure passed to backends:

```cpp
struct RenderingPayload {
    // Common resources (all backends)
    wil::com_ptr<ID2D1Factory> d2dFactory;
    wil::com_ptr<IDWriteFactory2> dwriteFactory;
    wil::com_ptr<IDWriteTextAnalyzer1> textAnalyzer;

    // DXGI resources (D3D backends only)
    struct {
        wil::com_ptr<IDXGIFactory2> factory;
        wil::com_ptr<IDXGIAdapter1> adapter;
        wil::com_ptr<IDXGISwapChain2> swapChain;
    } dxgi;

    // D3D11 resources (D3D11/D3D12 backends)
    wil::com_ptr<ID3D11Device2> device;
    wil::com_ptr<ID3D11DeviceContext2> deviceContext;

    // Settings (generational for change tracking)
    GenerationalSettings s;

    // Per-frame data
    Buffer<ShapedRow*> rows;              // Text rows with shaped glyphs
    Buffer<u32, 32> colorBitmap;          // Foreground/background colors
    til::rect dirtyRectInPx;              // Dirty region for partial updates
    til::rect cursorRect;                 // Cursor position
};
```

### 1.4 Strengths

1. **Minimal Abstraction Overhead**: Direct access to underlying APIs
2. **Backend-Specific Optimization**: Each backend can optimize independently
3. **Clean Separation**: Frontend (text shaping) vs backend (rendering)
4. **Proven Architecture**: Successfully supports D2D, D3D11, D3D12
5. **Shared Text Shaping**: DirectWrite used consistently across backends

### 1.5 Weaknesses

1. **Code Duplication**:
   - Glyph atlas management duplicated in BackendD3D, BackendD3D12, BackendOpenGL
   - Instance buffer management duplicated across backends
   - Constant buffer structures duplicated (VSConstBuffer, PSConstBuffer)
   - Atlas packing logic (stb_rect_pack) duplicated per backend

2. **Inconsistent Resource Management**:
   - D3D11 uses immediate context with implicit state management
   - D3D12 uses explicit command lists and resource barriers
   - OpenGL uses state machine with cached state tracking
   - No unified resource lifetime management

3. **Shader Divergence**:
   - HLSL shaders for D3D11/D3D12
   - GLSL shaders for OpenGL
   - No shared shader source or cross-compilation pipeline
   - Shader hot-reload logic duplicated per backend

4. **Limited Cross-Backend Testing**:
   - Difficult to ensure feature parity across backends
   - Bug fixes often need to be applied to multiple backends
   - No shared test harness for backend-agnostic rendering

5. **Hard to Add New Backends**:
   - Vulkan backend would require duplicating all atlas/instance logic again
   - WebGPU backend would face the same issues
   - Metal backend for future macOS support would duplicate code

### 1.6 Current Backend Complexity

**Lines of Code (Approximate):**
- BackendD3D.cpp: ~3,200 lines
- BackendD3D12.cpp: ~2,800 lines (+ 800 lines for compute shaders)
- BackendOpenGL.h: ~530 lines (implementation pending)
- Total backend-specific code: ~7,500 lines
- **Estimated duplicated code: ~40-50% (~3,000-3,750 lines)**

**Shared Structures:**
- QuadInstance: Identical across D3D, D3D12, OpenGL
- AtlasGlyphEntry: Identical across backends
- Constant buffers: Nearly identical (alignment differences only)

### 1.7 Pain Points for Developers

1. **Feature Implementation**: New features (e.g., CurlyLine underlines) require changes in 3+ backends
2. **Performance Optimization**: Optimizations in one backend don't transfer to others
3. **Debugging**: Backend-specific bugs are hard to isolate and fix
4. **Maintenance**: Changes to rendering logic require understanding 3+ APIs

---

## 2. Industry Pattern Research

### 2.1 Chromium Skia / Graphite

**Architecture Pattern:** High-level abstraction with backend specialization

**Key Insights:**

1. **Skia (Current):**
   - Ganesh rasterization backend designed around OpenGL
   - High-level Canvas API abstracts painting operations
   - Backend specialization for Vulkan, Metal, D3D12 added later
   - Performance bottleneck: OpenGL-centric design

2. **Graphite (2025):**
   - **Multi-threaded by default**: Core API uses independent Recorders producing Recordings
   - **WebGPU abstraction**: Uses Dawn (Chrome's WebGPU implementation) as a unified API
   - **Depth testing**: Assigns z-values to draw operations for overdraw minimization
   - **Pipeline consolidation**: Reduces pipeline state changes
   - **Performance**: 15% improvement on MotionMark 1.3 (M3 MacBook Pro)

**Architecture Diagram:**
```
Application
    |
    V
Skia Canvas API (High-level)
    |
    V
Graphite Recorder (Multi-threaded command recording)
    |
    V
Dawn (WebGPU abstraction layer)
    |
    +-- Vulkan Backend
    +-- Metal Backend
    +-- Direct3D 12 Backend
```

**Lessons for Terminal:**
- WebGPU-style abstraction provides good balance
- Multi-threaded command recording enables parallelism
- Depth-based rendering reduces overdraw (useful for cursor/selection overlays)

### 2.2 Firefox WebRender

**Architecture Pattern:** Multi-process, GPU-accelerated, Rust-based

**Key Insights:**

1. **Multi-Process Architecture:**
   - Content processes (4+) for rendering isolation
   - GPU process handles actual rendering
   - IPC-based display list serialization (C++ to Rust)

2. **Rendering Pipeline:**
   ```
   Content Process (C++)
       |
       V
   Display List (Binary serialization)
       |
       V  [IPC]
   GPU Process (Rust)
       |
       V
   WebRender Scene
       |
       V
   OpenGL rendering
   ```

3. **Work Stealing:**
   - Font glyph rasterization parallelized across cores
   - Work stealing algorithm for load balancing

**Lessons for Terminal:**
- Process isolation not needed (terminal is single-process)
- Work stealing useful for glyph rasterization
- Display list serialization pattern applicable for command buffers

### 2.3 bgfx

**Architecture Pattern:** Command buffer abstraction with backend plugins

**Key Insights:**

1. **Command Buffer Design:**
   - API thread records commands into command buffer
   - Render thread executes commands on graphics API
   - Multi-threaded rendering enabled by default

2. **Backend Plugin Architecture:**
   - Each backend implements same interface
   - Handle-based resource management
   - Automatic resource lifetime tracking

3. **Supported Backends:**
   - Direct3D 9, 11, 12
   - OpenGL 2.1+, OpenGL ES 2.0+
   - Vulkan, Metal
   - WebGL, WebGPU

**Architecture:**
```
API Thread                 Render Thread
    |                          |
    V                          V
Command Encoder ---------> Command Executor
                               |
                               V
                          Backend Interface
                               |
    +----------+----------+----+----+----------+
    |          |          |         |          |
    V          V          V         V          V
  D3D11      D3D12      Vulkan    Metal      OpenGL
```

**Lessons for Terminal:**
- Command buffer enables multi-threading
- Handle-based resources simplify lifetime management
- Backend plugin pattern reduces code duplication

### 2.4 The Forge

**Architecture Pattern:** Low-level abstraction matching modern APIs

**Key Insights:**

1. **Design Philosophy:**
   - Minimal abstraction over D3D12/Vulkan
   - High-performance focus
   - Used in AAA games (Forza Motorsport)

2. **Key Features:**
   - Asynchronous resource loading
   - Task-based rendering
   - Multi-threaded command recording
   - Cross-platform descriptor management

3. **Architecture:**
   ```
   Application
       |
       V
   The Forge API (Minimal abstraction)
       |
   +---+---+---+---+
   |   |   |   |   |
   V   V   V   V   V
  D3D12 Vulkan Metal PS5 Xbox
   ```

**Lessons for Terminal:**
- Minimal abstraction reduces overhead
- Terminal rendering is simpler than game rendering
- Don't over-abstract for terminal use case

### 2.5 DiligentEngine

**Architecture Pattern:** Unified interface with next-gen API focus

**Key Insights:**

1. **Design Principles:**
   - **Lightweight**: Close to native APIs
   - **Low overhead**: Minimal abstraction cost
   - **Multithreading**: Core design principle
   - **Extensibility**: Native API interop

2. **Key Features:**
   - Monolithic pipeline state objects (matches D3D12/Vulkan)
   - Descriptor tables/sets abstraction
   - HLSL as universal shading language
   - Multi-threaded command list recording

3. **Architecture:**
   ```
   Unified Interface
       |
   +---+---+---+---+---+---+
   |   |   |   |   |   |   |
   V   V   V   V   V   V   V
  D3D12 D3D11 Vulkan Metal OpenGL WebGPU
   ```

**Lessons for Terminal:**
- Unified interface with backend specialization
- HLSL as source language with cross-compilation
- Pipeline state objects reduce state changes

### 2.6 Sokol

**Architecture Pattern:** Minimal C header-only library

**Key Insights:**

1. **Design Philosophy:**
   - STB-style single-header libraries
   - GLES2/WebGL as "lead API" (lowest common denominator)
   - Metal/D3D11 ideas adapted to GLES2 constraints
   - Minimal bloat (33KB web demos)

2. **Key Design Decisions:**
   - **No shader cross-compilation**: Accepts API-specific shaders
   - **Simple and practical**: Favors ease of use over flexibility
   - **State batching**: Minimizes API calls

3. **Backends:**
   - GLES2/WebGL, GLES3/WebGL2
   - GL3.3, D3D11, Metal
   - WebGPU (experimental)

**Lessons for Terminal:**
- Simplicity valuable for maintenance
- No need for every advanced feature
- Focus on terminal-specific needs

### 2.7 Alacritty Terminal Renderer

**Architecture Pattern:** Minimal state changes, full-screen redraw

**Key Insights:**

1. **Rendering Strategy:**
   - **Full-screen redraw every frame**: Simpler than partial updates
   - **2 draw calls per frame**: Entire screen rendered in two passes
   - **Texture atlas**: Glyphs rasterized once, reused
   - **Instance data**: Uploaded once per frame

2. **Performance:**
   - ~500 FPS on large screen full of text
   - 2ms render budget at 60Hz (16.7ms frame time)
   - VSync limits to monitor refresh rate

3. **OpenGL Usage:**
   - Minimize state changes
   - Batch instance data
   - Simple shader pipeline

**Lessons for Terminal:**
- Full-screen redraw viable for terminal (unlike web browser)
- Texture atlas critical for performance
- Minimize draw calls and state changes

### 2.8 Render Graph / Frame Graph Pattern

**Architecture Pattern:** Deferred execution with automatic optimization

**Key Insights:**

1. **Core Concept:**
   - Declare rendering passes as graph nodes
   - Defer execution until entire frame recorded
   - Compile and optimize graph before execution
   - Dependency-sorted pass execution

2. **Benefits:**
   - **Automatic resource barriers**: D3D12/Vulkan synchronization
   - **Memory aliasing**: Reuse transient resources
   - **Async compute**: Automatic scheduling
   - **Optimization**: Eliminate redundant passes

3. **Industry Adoption:**
   - Frostbite (EA): FrameGraph (2017)
   - Unreal Engine: RDG (Rendering Dependency Graph)
   - Anvil Engine (Ubisoft)

4. **Example Structure:**
   ```
   FrameGraph
       |
   +---+---+---+
   |   |   |   |
   V   V   V   V
   Background  Text  Cursor  Selection
       |       |      |       |
       +-------+------+-------+
               |
               V
          SwapChain Present
   ```

**Lessons for Terminal:**
- Overkill for terminal rendering (only 4-5 passes)
- Automatic barriers useful for D3D12/Vulkan
- Simplified version could reduce boilerplate

---

## 3. Pattern Evaluation for Terminal Rendering

### 3.1 Pattern Classification

| Pattern                | Abstraction Level | Complexity | Code Reuse | Performance | Maintenance |
|------------------------|-------------------|------------|------------|-------------|-------------|
| Low-Level (Current)    | Minimal           | High       | Low        | Excellent   | Hard        |
| High-Level (Skia)      | High              | Medium     | High       | Good        | Easy        |
| Command Buffer (bgfx)  | Medium            | Medium     | Medium     | Good        | Medium      |
| Hybrid (Diligent)      | Medium-Low        | Medium     | Medium-High| Excellent   | Medium      |
| Render Graph           | High              | High       | High       | Good        | Hard        |

### 3.2 Terminal Rendering Characteristics

Terminal rendering has unique characteristics that influence pattern selection:

1. **Rendering Complexity: LOW**
   - 4-5 render passes: Background, Text, Gridlines, Cursor, Selection
   - Simple geometry: Quads/rectangles only
   - No complex materials, lighting, or post-processing
   - Fixed pipeline (no dynamic shader permutations)

2. **Latency Sensitivity: HIGH**
   - User input requires immediate visual feedback
   - Frame latency budget: 16.7ms @ 60Hz, 8.3ms @ 120Hz
   - Minimize CPU overhead in render path

3. **Text Quality: CRITICAL**
   - Glyph rasterization quality paramount
   - DirectWrite (Windows), FreeType (Linux) produce best results
   - Backend must preserve font rendering quality

4. **Resource Churn: LOW**
   - Glyph atlas stable (fonts don't change frequently)
   - Background colors change per cell, not per frame
   - Viewport size changes rarely

5. **Parallelism: MEDIUM**
   - Text shaping can be parallelized (per row)
   - Glyph rasterization can be parallelized
   - Rendering itself is serial (single GPU queue)

6. **Backend Count: MEDIUM**
   - Current: D2D, D3D11, D3D12, OpenGL
   - Future: Vulkan (Linux), potentially Metal (macOS), WebGPU

### 3.3 Pattern Evaluation

#### 3.3.1 Pattern 1: Low-Level Abstraction (Current)

**Characteristics:**
- Minimal IBackend interface (3 methods)
- Direct API access in each backend
- Maximum flexibility per backend

**Pros:**
- Zero abstraction overhead
- Backend-specific optimizations trivial
- Direct debugging of API calls
- Predictable performance

**Cons:**
- Code duplication (40-50%)
- Feature parity difficult to maintain
- New backends require complete reimplementation
- Bug fixes need multi-backend changes

**Terminal Rendering Fit: 7/10**
- Good for performance
- Poor for maintenance
- Acceptable for 2-3 backends, problematic for 5+

**Recommendation:** Needs improvement for code reuse

---

#### 3.3.2 Pattern 2: High-Level Abstraction (Skia/Graphite)

**Characteristics:**
- Canvas-style drawing API
- Backend translates high-level commands
- WebGPU-style intermediate layer

**Pros:**
- Minimal code duplication
- Easy to add new backends
- Uniform feature support
- Easier testing

**Cons:**
- Abstraction overhead (5-10% CPU)
- Harder to do backend-specific optimizations
- Potential for lowest-common-denominator features
- More complex architecture

**Terminal Rendering Fit: 5/10**
- Overhead unacceptable for terminal latency
- Overkill for simple rendering
- Glyph quality might suffer from abstraction

**Recommendation:** Too heavy for terminal use case

---

#### 3.3.3 Pattern 3: Command Buffer (bgfx)

**Characteristics:**
- Command recording on API thread
- Command execution on render thread
- Handle-based resource management

**Pros:**
- Multi-threading support built-in
- Resource lifetime management simplified
- Backend isolation clean
- Medium code reuse

**Cons:**
- Extra copy/serialization overhead
- Command buffer encoding cost
- Complexity for simple terminal rendering
- May not reduce code duplication enough

**Terminal Rendering Fit: 6/10**
- Multi-threading benefit minimal (terminal rendering is fast)
- Command buffer overhead > benefit
- Resource handles useful but not critical

**Recommendation:** Good ideas but too complex for terminals

---

#### 3.3.4 Pattern 4: Hybrid Abstraction (Diligent-style)

**Characteristics:**
- Unified resource wrapper classes
- Backend-specific fast paths
- Shared utilities for common operations
- Template-based backend selection

**Pros:**
- Good code reuse (50-70%)
- Low abstraction overhead (<5%)
- Backend optimizations preserved
- Easier maintenance than current

**Cons:**
- More complex than current architecture
- Requires careful API design
- Template compilation times

**Terminal Rendering Fit: 9/10**
- Perfect balance for terminal rendering
- Preserves performance
- Reduces duplication
- Maintainable

**Recommendation:** RECOMMENDED PATTERN

---

#### 3.3.5 Pattern 5: Render Graph

**Characteristics:**
- Declare passes as graph nodes
- Automatic barrier insertion
- Resource lifetime analysis
- Deferred execution

**Pros:**
- Automatic D3D12/Vulkan synchronization
- Resource aliasing
- Declarative rendering
- Optimization opportunities

**Cons:**
- High complexity for simple terminal rendering
- Overhead for graph construction/compilation
- Only 4-5 passes in terminal (not worth it)
- Harder to debug

**Terminal Rendering Fit: 4/10**
- Overkill for 4-5 pass rendering
- Complexity > benefit
- Useful ideas (barriers) can be applied separately

**Recommendation:** Not recommended for terminals, but borrow barrier automation ideas

---

### 3.4 Hybrid Pattern Detailed Design

Based on evaluation, a **Hybrid Abstraction Pattern** is recommended. This pattern combines:

1. **Shared Utilities (60% code reuse)**
   - Glyph atlas management
   - Instance buffer management
   - Constant buffer structures
   - Texture packing algorithms

2. **Abstract Resource Wrappers (30% code reuse)**
   - Texture2D (abstracts ID3D11Texture2D / ID3D12Resource / GLuint)
   - Buffer (abstracts vertex/index/constant buffers)
   - Shader (abstracts shaders across backends)
   - Pipeline (abstracts pipeline state)

3. **Backend-Specific Implementations (10% unique code)**
   - API initialization
   - State management
   - Resource creation
   - Command submission

**Three-Tier Architecture:**

```
Tier 1: Common Utilities (Backend-Agnostic)
    - GlyphAtlas (stb_rect_pack wrapper)
    - InstanceBatcher (QuadInstance management)
    - ShaderCompiler (HLSL -> SPIR-V -> target)
    - FontRasterizer (DirectWrite / FreeType abstraction)

Tier 2: Abstract Resources (Thin Wrappers)
    - Texture2D<Backend>
    - Buffer<Backend>
    - Shader<Backend>
    - Pipeline<Backend>

Tier 3: Backend Implementations
    - BackendD3D11 : IBackend
    - BackendD3D12 : IBackend
    - BackendVulkan : IBackend
    - BackendOpenGL : IBackend
```

**Code Sharing Estimation:**
- Tier 1 (Common): ~2,500 lines (shared across all backends)
- Tier 2 (Abstract): ~1,000 lines (shared across all backends)
- Tier 3 (Per Backend): ~1,500 lines (backend-specific)

**Total for 4 backends:**
- Current: ~7,500 lines (4 backends x 1,875 lines avg)
- Hybrid: ~9,500 lines (2,500 + 1,000 + 4 x 1,500)
- **Net change: +2,000 lines (+27%) but massively improved maintainability**

**Code Duplication:**
- Current: ~3,000-3,750 lines duplicated (40-50%)
- Hybrid: ~500 lines duplicated (5-7%)
- **Reduction: ~85% less duplication**

---

## 4. Glyph Atlas Management Patterns

### 4.1 Current Implementation Analysis

**Per-Backend Atlas Management:**

Each backend (D3D, D3D12, OpenGL) implements:
1. Atlas texture creation
2. Rectangle packing (stb_rect_pack)
3. Glyph rasterization
4. Texture upload
5. Cache management

**Code Duplication:**
```cpp
// BackendD3D.h
til::linear_flat_set<AtlasFontFaceEntry, ...> _glyphAtlasMap;
Buffer<stbrp_node> _rectPackerData;
stbrp_context _rectPacker;

// BackendD3D12.h - IDENTICAL
til::linear_flat_set<AtlasFontFaceEntry, ...> _glyphAtlasMap;
Buffer<stbrp_node> _rectPackerData;
stbrp_context _rectPacker;

// BackendOpenGL.h - IDENTICAL
til::linear_flat_set<AtlasFontFaceEntry, ...> _glyphAtlasMap;
Buffer<stbrp_node> _rectPackerData;
stbrp_context _rectPacker;
```

### 4.2 Industry Best Practices

**WebRender (Mozilla):**
- **Guillotine algorithm** with smallest-area heuristic
- **Separate glyph and image atlases** (different workload characteristics)
- **Region sizing:** Glyphs max 128px, smaller regions reduce waste
- **Lazy caching:** Rasterize on cache miss
- **Multiple atlases:** Create new atlas when full

**Warp Terminal:**
- **Shelf packing algorithm** for glyphs
- **Simple scanline packing:** Sort, jam into texture
- **Texture compression:** Reduce memory usage

**Alacritty:**
- **Single texture atlas** for all glyphs
- **Rasterize once, reuse forever**
- **No dynamic eviction** (glyphs stable)

### 4.3 Atlas Packing Algorithms

**Comparison:**

| Algorithm    | Complexity | Packing Efficiency | Dynamic Support | Best For    |
|--------------|------------|-------------------|-----------------|-------------|
| Scanline     | O(n log n) | 85-90%            | Poor            | Static      |
| Shelf        | O(n)       | 80-85%            | Good            | Glyphs      |
| Guillotine   | O(n log n) | 90-95%            | Excellent       | Mixed sizes |
| Skyline      | O(n^2)     | 95-98%            | Good            | High density|

**Recommendation for Terminal:**
- Use **Guillotine algorithm** (current: stb_rect_pack uses similar approach)
- Keep existing algorithm, extract to shared utility

### 4.4 Recommended Atlas Architecture

**Shared GlyphAtlas Class:**

```cpp
// File: /src/renderer/atlas/GlyphAtlas.h
namespace Microsoft::Console::Render::Atlas {

struct GlyphAtlasEntry {
    u32 glyphIndex;
    u8 occupied;
    ShadingType shadingType;
    u16 overlapSplit;
    i16x2 offset;
    u16x2 size;
    u16x2 texcoord;
};

template<typename TextureHandle>
class GlyphAtlas {
public:
    struct Config {
        u32 initialWidth = 2048;
        u32 initialHeight = 2048;
        u32 maxWidth = 8192;
        u32 maxHeight = 8192;
        bool supportResize = true;
        bool separateBitmapAtlas = true;
    };

    GlyphAtlas(const Config& config);

    // Core operations
    GlyphAtlasEntry* AllocateGlyph(u32 glyphIndex, u16x2 size);
    void RasterizeGlyph(GlyphAtlasEntry* entry, const void* data);
    void Clear();
    void Resize(u32 newWidth, u32 newHeight);

    // Query
    const GlyphAtlasEntry* FindGlyph(u32 glyphIndex) const;
    u16x2 GetAtlasSize() const { return _atlasSize; }
    TextureHandle GetTexture() const { return _atlasTexture; }

    // Statistics
    struct Stats {
        u32 glyphCount;
        u32 allocatedBytes;
        f32 utilizationPercent;
    };
    Stats GetStats() const;

private:
    Config _config;
    TextureHandle _atlasTexture;
    u16x2 _atlasSize;

    til::linear_flat_set<GlyphAtlasEntry, GlyphAtlasEntryHashTrait> _glyphs;
    Buffer<stbrp_node> _rectPackerData;
    stbrp_context _rectPacker;

    void _initializeRectPacker();
    bool _tryAllocate(u16x2 size, u16x2& outTexcoord);
};

} // namespace
```

**Backend Integration:**

```cpp
// BackendD3D.h
class BackendD3D : IBackend {
    GlyphAtlas<wil::com_ptr<ID3D11Texture2D>> _glyphAtlas;
};

// BackendD3D12.h
class BackendD3D12 : IBackend {
    GlyphAtlas<Microsoft::WRL::ComPtr<ID3D12Resource>> _glyphAtlas;
};

// BackendOpenGL.h
class BackendOpenGL : IBackend {
    GlyphAtlas<GLuint> _glyphAtlas;
};
```

### 4.5 Font Rasterization Abstraction

**Current:** DirectWrite used for all platforms (Windows-only)

**Recommended:** Platform-specific font rasterization

```cpp
// File: /src/renderer/atlas/FontRasterizer.h
namespace Microsoft::Console::Render::Atlas {

class IFontRasterizer {
public:
    virtual ~IFontRasterizer() = default;

    struct GlyphData {
        Buffer<u8> pixels;  // 8-bit grayscale or 32-bit RGBA
        u16x2 size;
        i16x2 bearing;
        u32 advance;
    };

    virtual GlyphData RasterizeGlyph(
        IDWriteFontFace* fontFace,
        u16 glyphIndex,
        f32 fontSize,
        bool useColor) = 0;
};

#ifdef _WIN32
class DirectWriteRasterizer : public IFontRasterizer {
    // Use DirectWrite + D2D for rasterization
};
#else
class FreeTypeRasterizer : public IFontRasterizer {
    // Use FreeType for rasterization
};
#endif

} // namespace
```

**Benefits:**
- Cross-platform font support
- Native rendering quality on each platform
- Abstraction allows future rasterizer backends (e.g., HarfBuzz)

---

## 5. Shader Management Patterns

### 5.1 Current Shader Management

**Current State:**
- HLSL shaders for D3D11/D3D12 (shader_ps.hlsl, shader_vs.hlsl)
- GLSL shaders for OpenGL (separate files)
- Manual shader compilation per backend
- Shader hot-reload per backend (#if ATLAS_DEBUG_SHADER_HOT_RELOAD)

**Pain Points:**
- Feature changes require updating multiple shader files
- Shader constants duplicated across HLSL/GLSL
- No shared shader utilities or includes

### 5.2 Industry Shader Patterns

**Uber-Shader Approach (Common in Games):**
- Single shader source with #ifdef permutations
- Runtime specialization via constants
- Pros: Single source, Cons: Large binary

**Shader Variants (Unreal, Unity):**
- Generate shader variants at build time
- Material graphs compile to shader code
- Pros: Optimized per-variant, Cons: Compilation time

**Cross-Compilation (Modern Approach):**
- Source: HLSL or GLSL
- Intermediate: SPIR-V
- Target: HLSL, GLSL, MSL (via SPIRV-Cross)

### 5.3 SPIR-V Cross-Compilation Pipeline

**Recommended Workflow (2025 Standard):**

```
Source Shaders (HLSL)
    |
    V
DirectX Shader Compiler (DXC)
    |
    V
SPIR-V Intermediate
    |
    +-------------------+------------------+
    |                   |                  |
    V                   V                  V
HLSL Bytecode      GLSL (via          MSL (via
(D3D11/D3D12)   SPIRV-Cross)      SPIRV-Cross)
```

**Tools:**
- **DXC (DirectXShaderCompiler):** HLSL -> SPIR-V
- **SPIRV-Cross:** SPIR-V -> GLSL/HLSL/MSL
- **glslangValidator:** GLSL -> SPIR-V (alternative)

**Benefits:**
1. Single source of truth (HLSL)
2. Automatic cross-compilation
3. Shader validation at build time
4. Future-proof (SPIR-V is D3D interchange format as of SM7)

### 5.4 Recommended Shader Architecture

**Directory Structure:**
```
/src/renderer/atlas/shaders/
    common/
        constants.hlsl          // Shared constant buffer definitions
        utils.hlsl              // Shared utility functions
    vertex/
        quad_vs.hlsl           // Quad vertex shader
    pixel/
        background_ps.hlsl     // Background rendering
        text_grayscale_ps.hlsl // Grayscale text
        text_cleartype_ps.hlsl // ClearType text
        cursor_ps.hlsl         // Cursor rendering
        line_ps.hlsl           // Gridlines
```

**Build-Time Compilation:**

```cmake
# CMakeLists.txt shader compilation
function(compile_shader SHADER_FILE SHADER_TYPE OUTPUT_DIR)
    # Compile to SPIR-V
    add_custom_command(
        OUTPUT ${OUTPUT_DIR}/${SHADER_FILE}.spv
        COMMAND dxc -T ${SHADER_TYPE} -spirv ${SHADER_FILE}
                    -Fo ${OUTPUT_DIR}/${SHADER_FILE}.spv
        DEPENDS ${SHADER_FILE}
    )

    # Cross-compile to GLSL
    add_custom_command(
        OUTPUT ${OUTPUT_DIR}/${SHADER_FILE}.glsl
        COMMAND spirv-cross ${OUTPUT_DIR}/${SHADER_FILE}.spv
                    --output ${OUTPUT_DIR}/${SHADER_FILE}.glsl
                    --version 330
        DEPENDS ${OUTPUT_DIR}/${SHADER_FILE}.spv
    )

    # Keep HLSL bytecode for D3D11/D3D12
    add_custom_command(
        OUTPUT ${OUTPUT_DIR}/${SHADER_FILE}.cso
        COMMAND dxc -T ${SHADER_TYPE} ${SHADER_FILE}
                    -Fo ${OUTPUT_DIR}/${SHADER_FILE}.cso
        DEPENDS ${SHADER_FILE}
    )
endfunction()

compile_shader(quad_vs.hlsl vs_5_0 ${CMAKE_BINARY_DIR}/shaders)
compile_shader(text_grayscale_ps.hlsl ps_5_0 ${CMAKE_BINARY_DIR}/shaders)
# ... repeat for all shaders
```

**Runtime Shader Loading:**

```cpp
// File: /src/renderer/atlas/ShaderManager.h
namespace Microsoft::Console::Render::Atlas {

enum class ShaderStage {
    Vertex,
    Pixel,
    Compute
};

enum class ShaderFormat {
    DXBC,     // D3D11/D3D12 bytecode
    SPIRV,    // Vulkan SPIR-V
    GLSL,     // OpenGL GLSL source
    MSL       // Metal Shading Language
};

template<typename ShaderHandle>
class ShaderManager {
public:
    ShaderHandle LoadShader(
        ShaderStage stage,
        ShaderFormat format,
        const wchar_t* path);

    void ReloadShaders();  // Hot-reload support

private:
    std::unordered_map<std::wstring, ShaderHandle> _shaderCache;
};

} // namespace
```

### 5.5 Shader Hot-Reload

**Current:** Per-backend hot-reload implementation

**Recommended:** Shared hot-reload infrastructure

```cpp
// File: /src/renderer/atlas/ShaderHotReload.h
#if ATLAS_DEBUG_SHADER_HOT_RELOAD

class ShaderHotReload {
public:
    using ReloadCallback = std::function<void(const wchar_t* path)>;

    ShaderHotReload(const wchar_t* watchDirectory);

    void RegisterCallback(const wchar_t* shaderPath, ReloadCallback callback);
    void Poll();  // Call once per frame

private:
    wil::unique_folder_change_reader_nothrow _watcher;
    std::atomic<int64_t> _invalidationTime{INT64_MAX};
    std::unordered_map<std::wstring, std::vector<ReloadCallback>> _callbacks;
};

#endif
```

**Benefits:**
- Single implementation for all backends
- Consistent behavior across backends
- Easier debugging

---

## 6. Resource Management Patterns

### 6.1 Current Resource Management

**Per-Backend Resource Handling:**

```cpp
// BackendD3D.h
wil::com_ptr<ID3D11Texture2D> _glyphAtlas;
wil::com_ptr<ID3D11Buffer> _instanceBuffer;
wil::com_ptr<ID3D11Buffer> _vsConstantBuffer;

// BackendD3D12.h
Microsoft::WRL::ComPtr<ID3D12Resource> _glyphAtlas;
Microsoft::WRL::ComPtr<ID3D12Resource> _instanceBuffer;
Microsoft::WRL::ComPtr<ID3D12Resource> _vsConstantBuffer;

// BackendOpenGL.h
GLuint _glyphAtlas;
GLuint _instanceBuffer;
GLuint _vsConstantBuffer;
```

**Issues:**
- Type safety: GLuint is just u32, no compile-time checking
- Lifetime: Manual resource cleanup per backend
- Synchronization: D3D12 requires manual barriers, D3D11 doesn't

### 6.2 Handle-Based Resource Management (bgfx-style)

**Concept:**
```cpp
struct TextureHandle { u16 id; };
struct BufferHandle { u16 id; };
struct ShaderHandle { u16 id; };

// Backend maintains resource pools
class BackendD3D {
    std::vector<wil::com_ptr<ID3D11Texture2D>> _textures;
    std::vector<wil::com_ptr<ID3D11Buffer>> _buffers;
};
```

**Pros:**
- Type-safe handles
- Unified resource creation API
- Automatic lifetime management

**Cons:**
- Indirection overhead (handle -> resource lookup)
- May prevent backend optimizations
- Extra complexity

**Recommendation:** Not worth it for terminal (only 10-20 resources total)

### 6.3 Resource Wrapper Pattern (Recommended)

**Typed Resource Wrappers:**

```cpp
// File: /src/renderer/atlas/Resources.h
namespace Microsoft::Console::Render::Atlas {

// Backend tag types for template specialization
struct D3D11Backend {};
struct D3D12Backend {};
struct OpenGLBackend {};
struct VulkanBackend {};

// Forward declarations
template<typename Backend> class Texture2D;
template<typename Backend> class Buffer;

// Texture2D specializations
template<>
class Texture2D<D3D11Backend> {
public:
    Texture2D() = default;
    Texture2D(ID3D11Device* device, u32 width, u32 height, DXGI_FORMAT format);

    void Upload(ID3D11DeviceContext* ctx, const void* data, u32 rowPitch);
    void Bind(ID3D11DeviceContext* ctx, u32 slot);

    ID3D11Texture2D* Get() { return _texture.get(); }

private:
    wil::com_ptr<ID3D11Texture2D> _texture;
    wil::com_ptr<ID3D11ShaderResourceView> _srv;
    u32 _width, _height;
};

template<>
class Texture2D<D3D12Backend> {
public:
    Texture2D() = default;
    Texture2D(ID3D12Device* device, u32 width, u32 height, DXGI_FORMAT format);

    void Upload(ID3D12GraphicsCommandList* cmdList, const void* data);
    void TransitionBarrier(ID3D12GraphicsCommandList* cmdList,
                          D3D12_RESOURCE_STATES before,
                          D3D12_RESOURCE_STATES after);

    ID3D12Resource* Get() { return _texture.Get(); }

private:
    Microsoft::WRL::ComPtr<ID3D12Resource> _texture;
    Microsoft::WRL::ComPtr<ID3D12Resource> _uploadBuffer;
    D3D12_CPU_DESCRIPTOR_HANDLE _srvHandle;
    u32 _width, _height;
};

template<>
class Texture2D<OpenGLBackend> {
public:
    Texture2D() = default;
    Texture2D(u32 width, u32 height, GLenum format);
    ~Texture2D();

    void Upload(const void* data, u32 level = 0);
    void Bind(u32 unit);

    GLuint Get() { return _texture; }

private:
    GLuint _texture = 0;
    u32 _width, _height;
    GLenum _format;
};

} // namespace
```

**Usage:**

```cpp
// BackendD3D.h
class BackendD3D : IBackend {
    Texture2D<D3D11Backend> _glyphAtlas;
    Buffer<D3D11Backend> _instanceBuffer;
};

// BackendD3D12.h
class BackendD3D12 : IBackend {
    Texture2D<D3D12Backend> _glyphAtlas;
    Buffer<D3D12Backend> _instanceBuffer;
};

// BackendOpenGL.h
class BackendOpenGL : IBackend {
    Texture2D<OpenGLBackend> _glyphAtlas;
    Buffer<OpenGLBackend> _instanceBuffer;
};
```

**Benefits:**
- Type safety: Can't mix backends
- Unified API: Upload/Bind methods consistent
- Backend-specific optimizations preserved
- Resource barriers abstracted (D3D12)
- Automatic RAII cleanup

### 6.4 Constant Buffer Management

**Current:** Duplicated structures

**Recommended:** Shared definitions with backend specialization

```cpp
// File: /src/renderer/atlas/ConstantBuffers.h
namespace Microsoft::Console::Render::Atlas {

// Shared constant buffer structure (matches HLSL)
struct alignas(16) VSConstBuffer {
    alignas(sizeof(f32x2)) f32x2 positionScale;
};

struct alignas(16) PSConstBuffer {
    alignas(sizeof(f32x4)) f32x4 backgroundColor;
    alignas(sizeof(f32x2)) f32x2 backgroundCellSize;
    alignas(sizeof(f32x2)) f32x2 backgroundCellCount;
    alignas(sizeof(f32x4)) f32 gammaRatios[4];
    alignas(sizeof(f32)) f32 enhancedContrast;
    alignas(sizeof(f32)) f32 underlineWidth;
    alignas(sizeof(f32)) f32 doubleUnderlineWidth;
    alignas(sizeof(f32)) f32 curlyLineHalfHeight;
    alignas(sizeof(f32)) f32 shadedGlyphDotSize;
};

// Constant buffer wrapper
template<typename Backend, typename T>
class ConstantBuffer {
public:
    void Update(const T& data);
    void Bind(u32 slot);

private:
    Buffer<Backend> _buffer;
    T _cachedData;
};

} // namespace
```

### 6.5 Memory Allocation Strategies

**Ring Buffer for Streaming Data:**

```cpp
// File: /src/renderer/atlas/RingBuffer.h
template<typename Backend>
class RingBuffer {
public:
    struct Allocation {
        void* cpuAddress;
        u64 gpuAddress;
        u32 offset;
    };

    RingBuffer(u32 size);

    Allocation Allocate(u32 size, u32 alignment);
    void FinishFrame();

private:
    Buffer<Backend> _buffer;
    u32 _size;
    u32 _offset;
    u32 _frameOffsets[3];  // Triple buffering
};
```

**Use Cases:**
- Instance data upload (changes every frame)
- Dynamic vertex data
- Temporary staging buffers

**Benefits:**
- Avoid per-frame allocations
- Reduce memory fragmentation
- Improve upload performance

---

## 7. Recommended Architecture

### 7.1 Three-Tier Architecture

```
+-------------------------------------------------------------------------+
|                         Tier 1: Common Utilities                         |
|                         (Backend-Agnostic)                               |
+-------------------------------------------------------------------------+
| - GlyphAtlas<TextureHandle>     | Glyph atlas management & packing    |
| - InstanceBatcher               | QuadInstance batching               |
| - ShaderCompiler                | HLSL->SPIR-V cross-compilation      |
| - FontRasterizer                | DirectWrite/FreeType abstraction    |
| - ShaderHotReload               | Shader file watching                |
| - ConstantBufferDefs            | Shared CB structures                |
+-------------------------------------------------------------------------+
                                  |
                                  V
+-------------------------------------------------------------------------+
|                     Tier 2: Abstract Resources                          |
|                     (Thin Backend Wrappers)                             |
+-------------------------------------------------------------------------+
| - Texture2D<Backend>            | Texture creation/upload/binding     |
| - Buffer<Backend>               | Buffer creation/upload/binding      |
| - Shader<Backend>               | Shader loading/binding              |
| - Pipeline<Backend>             | Pipeline state management           |
| - ConstantBuffer<Backend, T>    | Constant buffer updates             |
| - RingBuffer<Backend>           | Ring buffer allocator               |
+-------------------------------------------------------------------------+
                                  |
                                  V
+-------------------------------------------------------------------------+
|                    Tier 3: Backend Implementations                       |
|                    (API-Specific Code)                                   |
+-------------------------------------------------------------------------+
| BackendD3D11        | BackendD3D12      | BackendOpenGL  | BackendVulkan |
+-------------------------------------------------------------------------+
| - Device init       | - Device init     | - Context init | - Instance    |
| - State management  | - Command lists   | - State cache  | - Device      |
| - Resource creation | - Barriers        | - VAO setup    | - Queues      |
| - Draw calls        | - Async compute   | - Draw calls   | - Cmd buffers |
+-------------------------------------------------------------------------+
```

### 7.2 Component Breakdown

#### Tier 1: Common Utilities

**GlyphAtlas (2D Texture Packing):**
- Rectangle packing algorithm (Guillotine/stb_rect_pack)
- Glyph entry hash map
- Lazy rasterization
- Atlas resizing
- Statistics tracking

**InstanceBatcher:**
- QuadInstance array management
- Batching by shading type
- Overflow handling (create new batch)
- Upload optimization (ring buffer)

**ShaderCompiler:**
- HLSL source parsing
- DXC invocation (HLSL -> SPIR-V)
- SPIRV-Cross invocation (SPIR-V -> GLSL)
- Shader caching (avoid recompilation)

**FontRasterizer:**
- Platform detection (#ifdef _WIN32)
- DirectWrite wrapper (Windows)
- FreeType wrapper (Linux/macOS)
- Grayscale & RGBA rasterization

#### Tier 2: Abstract Resources

**Texture2D<Backend>:**
- Create(width, height, format)
- Upload(data, rowPitch)
- Bind(slot)
- Resize(newWidth, newHeight)

**Buffer<Backend>:**
- Create(size, usage, cpuAccess)
- Upload(data, offset, size)
- Map()/Unmap() for staging
- Bind(slot)

**Shader<Backend>:**
- LoadFromFile(path, format)
- Bind()
- SetConstantBuffer(slot, buffer)

**Pipeline<Backend>:**
- SetShaders(vs, ps)
- SetBlendState(...)
- SetRasterizerState(...)
- Bind()

#### Tier 3: Backend Implementations

**BackendD3D11:**
```cpp
class BackendD3D11 : IBackend {
public:
    void Render(RenderingPayload& payload) override {
        _updateConstantBuffers(payload);
        _uploadBackgroundBitmap(payload);
        _drawBackground(payload);
        _drawText(payload);
        _drawCursor(payload);
        _drawSelection(payload);
    }

private:
    // Tier 1 utilities
    GlyphAtlas<wil::com_ptr<ID3D11Texture2D>> _glyphAtlas;
    InstanceBatcher _instanceBatcher;
    FontRasterizer _fontRasterizer;

    // Tier 2 resources
    Texture2D<D3D11Backend> _backgroundBitmap;
    Buffer<D3D11Backend> _instanceBuffer;
    ConstantBuffer<D3D11Backend, VSConstBuffer> _vsConstants;
    ConstantBuffer<D3D11Backend, PSConstBuffer> _psConstants;
    Shader<D3D11Backend> _vertexShader;
    Shader<D3D11Backend> _pixelShader;
    Pipeline<D3D11Backend> _pipeline;

    // D3D11-specific
    wil::com_ptr<ID3D11Device> _device;
    wil::com_ptr<ID3D11DeviceContext> _context;
};
```

**BackendD3D12:**
```cpp
class BackendD3D12 : IBackend {
public:
    void Render(RenderingPayload& payload) override {
        _beginFrame();
        _recordCommands(payload);
        _executeCommands();
        _present();
        _moveToNextFrame();
    }

private:
    // Tier 1 utilities (same as D3D11)
    GlyphAtlas<Microsoft::WRL::ComPtr<ID3D12Resource>> _glyphAtlas;
    InstanceBatcher _instanceBatcher;
    FontRasterizer _fontRasterizer;

    // Tier 2 resources
    Texture2D<D3D12Backend> _backgroundBitmap;
    Buffer<D3D12Backend> _instanceBuffer;
    RingBuffer<D3D12Backend> _uploadRingBuffer;

    // D3D12-specific
    Microsoft::WRL::ComPtr<ID3D12Device> _device;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> _commandQueue;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> _commandList;
    Microsoft::WRL::ComPtr<ID3D12Fence> _fence;
    // Frame resources for triple buffering
    struct FrameResource {
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
        Microsoft::WRL::ComPtr<ID3D12Resource> renderTarget;
        u64 fenceValue;
    };
    std::array<FrameResource, 3> _frameResources;
};
```

**BackendOpenGL:**
```cpp
class BackendOpenGL : IBackend {
public:
    void Render(RenderingPayload& payload) override {
        _updateState();
        _updateConstantBuffers(payload);
        _drawBackground(payload);
        _drawText(payload);
        _drawCursor(payload);
        _drawSelection(payload);
    }

private:
    // Tier 1 utilities (same as D3D11)
    GlyphAtlas<GLuint> _glyphAtlas;
    InstanceBatcher _instanceBatcher;
    FontRasterizer _fontRasterizer;

    // Tier 2 resources
    Texture2D<OpenGLBackend> _backgroundBitmap;
    Buffer<OpenGLBackend> _instanceBuffer;

    // OpenGL-specific
    GLuint _vao;
    GLuint _program;

    // State caching
    struct GLState {
        GLuint program;
        GLuint textures[16];
        GLuint ubos[8];
        bool blendEnabled;
    } _currentState, _desiredState;
};
```

### 7.3 Backend Selection at Compile-Time

**Template-Based Backend Dispatch:**

```cpp
// File: /src/renderer/atlas/BackendFactory.h
namespace Microsoft::Console::Render::Atlas {

enum class GraphicsAPI {
    D2D,
    D3D11,
    D3D12,
    OpenGL,
    Vulkan,
    Automatic
};

std::unique_ptr<IBackend> CreateBackend(
    GraphicsAPI api,
    const RenderingPayload& payload) {

    switch (api) {
    case GraphicsAPI::D2D:
        return std::make_unique<BackendD2D>();
    case GraphicsAPI::D3D11:
        return std::make_unique<BackendD3D>();
    case GraphicsAPI::D3D12:
        return std::make_unique<BackendD3D12>(payload);
    case GraphicsAPI::OpenGL:
        return std::make_unique<BackendOpenGL>(payload);
    case GraphicsAPI::Vulkan:
        return std::make_unique<BackendVulkan>(payload);
    case GraphicsAPI::Automatic:
        return CreateBackendAutomatic(payload);
    }
}

std::unique_ptr<IBackend> CreateBackendAutomatic(
    const RenderingPayload& payload) {

    // Try D3D12 first (best performance on Windows 10+)
    if (auto backend = TryCreate<BackendD3D12>(payload))
        return backend;

    // Fall back to D3D11 (Windows 7+)
    if (auto backend = TryCreate<BackendD3D>(payload))
        return backend;

    // Fall back to OpenGL (cross-platform)
    if (auto backend = TryCreate<BackendOpenGL>(payload))
        return backend;

    // Last resort: D2D (slowest but most compatible)
    return std::make_unique<BackendD2D>();
}

} // namespace
```

### 7.4 Rendering Flow

**Unified Rendering Pipeline:**

```
StartPaint()
    |
    V
+-----------------------------------+
| Text Shaping (DirectWrite)        |
| - AtlasEngine::PaintBufferLine()  |
| - Glyph layout & analysis         |
| - Store in ShapedRow              |
+-----------------------------------+
    |
    V
EndPaint()
    |
    V
+-----------------------------------+
| Payload Preparation               |
| - Update colorBitmap              |
| - Update cursorRect               |
| - Calculate dirtyRectInPx         |
+-----------------------------------+
    |
    V
Present()
    |
    V
+-----------------------------------+
| Backend Rendering                 |
| - IBackend::Render(payload)       |
|   - Update constant buffers       |
|   - Upload background bitmap      |
|   - Draw background               |
|   - Draw text (with atlas)        |
|   - Draw gridlines                |
|   - Draw cursor                   |
|   - Draw selection                |
| - SwapChain::Present()            |
+-----------------------------------+
```

**Backend-Agnostic Drawing:**

```cpp
void BackendD3D11::_drawText(RenderingPayload& payload) {
    // 1. Rasterize missing glyphs into atlas (Tier 1)
    for (auto row : payload.rows) {
        for (auto& mapping : row->mappings) {
            for (u32 i = mapping.glyphsFrom; i < mapping.glyphsTo; ++i) {
                u16 glyphIndex = row->glyphIndices[i];
                auto* entry = _glyphAtlas.FindGlyph(glyphIndex);
                if (!entry) {
                    // Rasterize glyph using FontRasterizer (Tier 1)
                    auto glyphData = _fontRasterizer.RasterizeGlyph(
                        mapping.fontFace, glyphIndex,
                        payload.s->font->fontSize, false);

                    // Allocate space in atlas (Tier 1)
                    entry = _glyphAtlas.AllocateGlyph(glyphIndex, glyphData.size);

                    // Upload to atlas texture (Tier 2)
                    _glyphAtlas.RasterizeGlyph(entry, glyphData.pixels.data());
                }

                // Add quad instance (Tier 1)
                _instanceBatcher.AddQuad({
                    .shadingType = ShadingType::TextGrayscale,
                    .position = { x, y },
                    .size = entry->size,
                    .texcoord = entry->texcoord,
                    .color = row->colors[i]
                });
            }
        }
    }

    // 2. Upload instance data (Tier 2)
    auto instances = _instanceBatcher.GetInstances();
    _instanceBuffer.Upload(instances.data(), 0, instances.size() * sizeof(QuadInstance));

    // 3. Draw (Tier 3 - D3D11-specific)
    _context->IASetVertexBuffers(0, 1, _instanceBuffer.Get(), ...);
    _context->PSSetShaderResources(0, 1, _glyphAtlas.GetTexture()->GetSRV());
    _context->DrawInstanced(6, instances.size(), 0, 0);
}
```

---

## 8. Refactoring Plan

### 8.1 Phased Approach

**Phase 1: Extract Common Utilities (2-3 weeks)**
- Extract GlyphAtlas to shared template class
- Extract InstanceBatcher
- Extract shader compilation utilities
- Test: All backends still work, no regressions

**Phase 2: Create Abstract Resources (2-3 weeks)**
- Implement Texture2D<Backend>
- Implement Buffer<Backend>
- Implement Shader<Backend>
- Migrate BackendD3D to use abstractions
- Test: BackendD3D still works

**Phase 3: Migrate Remaining Backends (3-4 weeks)**
- Migrate BackendD3D12
- Migrate BackendOpenGL
- Remove duplicated code
- Test: All backends work with abstractions

**Phase 4: Shader Cross-Compilation (2 weeks)**
- Set up DXC + SPIRV-Cross build pipeline
- Migrate shaders to unified HLSL
- Generate GLSL for OpenGL backend
- Test: Shaders match on all backends

**Phase 5: Font Rasterization Abstraction (2 weeks)**
- Implement IFontRasterizer interface
- Implement DirectWriteRasterizer
- Implement FreeTypeRasterizer (for future Linux support)
- Test: Font quality unchanged

**Phase 6: Testing & Optimization (2 weeks)**
- Performance benchmarks (ensure no regressions)
- Memory usage analysis
- Cross-backend feature parity testing
- Fix any issues found

**Total Timeline: 13-16 weeks (3-4 months)**

### 8.2 Step-by-Step Migration

#### Step 1: Extract GlyphAtlas

**Before:**
```cpp
// BackendD3D.h
class BackendD3D : IBackend {
    til::linear_flat_set<AtlasFontFaceEntry, ...> _glyphAtlasMap;
    Buffer<stbrp_node> _rectPackerData;
    stbrp_context _rectPacker;
    wil::com_ptr<ID3D11Texture2D> _glyphAtlas;

    void _drawGlyph(...);
};
```

**After:**
```cpp
// GlyphAtlas.h (new file)
template<typename TextureHandle>
class GlyphAtlas {
    // ... (implementation shown in section 4.4)
};

// BackendD3D.h
class BackendD3D : IBackend {
    GlyphAtlas<wil::com_ptr<ID3D11Texture2D>> _glyphAtlas;

    void _drawGlyph(...);  // Now uses _glyphAtlas methods
};
```

**Migration Checklist:**
- [ ] Create GlyphAtlas.h with template implementation
- [ ] Move AtlasGlyphEntry, AtlasFontFaceEntry to GlyphAtlas.h
- [ ] Replace BackendD3D internals with GlyphAtlas
- [ ] Test BackendD3D still renders correctly
- [ ] Repeat for BackendD3D12, BackendOpenGL

#### Step 2: Extract InstanceBatcher

**Before:**
```cpp
// BackendD3D.cpp
void BackendD3D::_drawText(RenderingPayload& p) {
    _instances.clear();

    for (auto row : p.rows) {
        // ... add instances ...
        _instances.emplace_back(QuadInstance{...});
    }

    if (_instancesCount > _instanceBufferCapacity) {
        _bumpInstancesSize();
        _recreateInstanceBuffers(p);
    }

    // Upload instances...
}
```

**After:**
```cpp
// InstanceBatcher.h (new file)
class InstanceBatcher {
public:
    void Clear();
    QuadInstance& AddQuad();
    std::span<const QuadInstance> GetInstances() const;
    size_t GetInstanceCount() const;

private:
    Buffer<QuadInstance, 32> _instances;
    size_t _count = 0;
};

// BackendD3D.cpp
void BackendD3D::_drawText(RenderingPayload& p) {
    _instanceBatcher.Clear();

    for (auto row : p.rows) {
        // ... add instances ...
        auto& quad = _instanceBatcher.AddQuad();
        quad = QuadInstance{...};
    }

    auto instances = _instanceBatcher.GetInstances();
    _instanceBuffer.Upload(instances.data(), ...);
}
```

#### Step 3: Implement Texture2D<Backend>

**Implementation:**
```cpp
// Resources.h (new file)
template<typename Backend>
class Texture2D;

// Specialization for D3D11
template<>
class Texture2D<D3D11Backend> {
public:
    Texture2D() = default;

    void Create(ID3D11Device* device, u32 width, u32 height, DXGI_FORMAT format) {
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = format;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        THROW_IF_FAILED(device->CreateTexture2D(&desc, nullptr, _texture.put()));

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;

        THROW_IF_FAILED(device->CreateShaderResourceView(
            _texture.get(), &srvDesc, _srv.put()));

        _width = width;
        _height = height;
    }

    void Upload(ID3D11DeviceContext* ctx, const void* data, u32 rowPitch) {
        ctx->UpdateSubresource(_texture.get(), 0, nullptr, data, rowPitch, 0);
    }

    void Bind(ID3D11DeviceContext* ctx, u32 slot) {
        ID3D11ShaderResourceView* views[] = { _srv.get() };
        ctx->PSSetShaderResources(slot, 1, views);
    }

    ID3D11Texture2D* GetTexture() { return _texture.get(); }
    ID3D11ShaderResourceView* GetSRV() { return _srv.get(); }

private:
    wil::com_ptr<ID3D11Texture2D> _texture;
    wil::com_ptr<ID3D11ShaderResourceView> _srv;
    u32 _width = 0;
    u32 _height = 0;
};

// Specialization for D3D12 (similar pattern)
template<>
class Texture2D<D3D12Backend> {
    // ... D3D12 implementation ...
};

// Specialization for OpenGL (similar pattern)
template<>
class Texture2D<OpenGLBackend> {
    // ... OpenGL implementation ...
};
```

**Migration:**
```cpp
// BackendD3D.h - Before
class BackendD3D : IBackend {
    wil::com_ptr<ID3D11Texture2D> _backgroundBitmap;
    wil::com_ptr<ID3D11ShaderResourceView> _backgroundBitmapView;
};

// BackendD3D.h - After
class BackendD3D : IBackend {
    Texture2D<D3D11Backend> _backgroundBitmap;
};

// BackendD3D.cpp - Before
void BackendD3D::_recreateBackgroundColorBitmap(const RenderingPayload& p) {
    D3D11_TEXTURE2D_DESC desc{};
    // ... 10 lines of D3D11 setup ...
    THROW_IF_FAILED(p.device->CreateTexture2D(&desc, nullptr, _backgroundBitmap.put()));
    THROW_IF_FAILED(p.device->CreateShaderResourceView(...));
}

// BackendD3D.cpp - After
void BackendD3D::_recreateBackgroundColorBitmap(const RenderingPayload& p) {
    _backgroundBitmap.Create(p.device.get(), width, height, DXGI_FORMAT_R8G8B8A8_UNORM);
}
```

#### Step 4: Set Up Shader Cross-Compilation

**Directory Setup:**
```
/src/renderer/atlas/shaders/
    hlsl/                      # Source shaders (HLSL)
        common/
            constants.hlsli    # Shared constant buffers
            utils.hlsli        # Shared functions
        quad_vs.hlsl          # Vertex shader
        background_ps.hlsl    # Background pixel shader
        text_ps.hlsl          # Text pixel shader

    compiled/                  # Generated at build time
        d3d/
            quad_vs.cso       # DXBC for D3D11/D3D12
            background_ps.cso
            text_ps.cso
        glsl/
            quad_vs.glsl      # GLSL for OpenGL
            background_ps.glsl
            text_ps.glsl
        spirv/
            quad_vs.spv       # SPIR-V intermediate
            background_ps.spv
            text_ps.spv
```

**CMake Build Script:**
```cmake
# Find DXC (DirectX Shader Compiler)
find_program(DXC_EXECUTABLE dxc REQUIRED)

# Find SPIRV-Cross
find_program(SPIRV_CROSS_EXECUTABLE spirv-cross REQUIRED)

# Shader compilation function
function(add_shader TARGET SHADER_FILE SHADER_PROFILE)
    get_filename_component(SHADER_NAME ${SHADER_FILE} NAME_WE)

    set(SPIRV_OUTPUT ${CMAKE_BINARY_DIR}/shaders/spirv/${SHADER_NAME}.spv)
    set(DXBC_OUTPUT ${CMAKE_BINARY_DIR}/shaders/d3d/${SHADER_NAME}.cso)
    set(GLSL_OUTPUT ${CMAKE_BINARY_DIR}/shaders/glsl/${SHADER_NAME}.glsl)

    # Compile to SPIR-V
    add_custom_command(
        OUTPUT ${SPIRV_OUTPUT}
        COMMAND ${DXC_EXECUTABLE}
            -T ${SHADER_PROFILE}
            -spirv
            -Fo ${SPIRV_OUTPUT}
            ${CMAKE_SOURCE_DIR}/src/renderer/atlas/shaders/hlsl/${SHADER_FILE}
        DEPENDS ${SHADER_FILE}
        COMMENT "Compiling ${SHADER_FILE} to SPIR-V"
    )

    # Compile to DXBC
    add_custom_command(
        OUTPUT ${DXBC_OUTPUT}
        COMMAND ${DXC_EXECUTABLE}
            -T ${SHADER_PROFILE}
            -Fo ${DXBC_OUTPUT}
            ${CMAKE_SOURCE_DIR}/src/renderer/atlas/shaders/hlsl/${SHADER_FILE}
        DEPENDS ${SHADER_FILE}
        COMMENT "Compiling ${SHADER_FILE} to DXBC"
    )

    # Cross-compile SPIR-V to GLSL
    add_custom_command(
        OUTPUT ${GLSL_OUTPUT}
        COMMAND ${SPIRV_CROSS_EXECUTABLE}
            ${SPIRV_OUTPUT}
            --output ${GLSL_OUTPUT}
            --version 330
            --no-es
        DEPENDS ${SPIRV_OUTPUT}
        COMMENT "Cross-compiling ${SHADER_FILE} to GLSL"
    )

    # Add to target dependencies
    target_sources(${TARGET} PRIVATE ${SPIRV_OUTPUT} ${DXBC_OUTPUT} ${GLSL_OUTPUT})
endfunction()

# Add shaders to target
add_shader(AtlasEngine quad_vs.hlsl vs_5_0)
add_shader(AtlasEngine background_ps.hlsl ps_5_0)
add_shader(AtlasEngine text_ps.hlsl ps_5_0)
```

**Shader Loading:**
```cpp
// ShaderManager.h
template<typename Backend>
class ShaderManager {
public:
    void LoadShaders(const wchar_t* basePath) {
        #if defined(BACKEND_D3D11) || defined(BACKEND_D3D12)
            auto vsPath = std::wstring(basePath) + L"/d3d/quad_vs.cso";
            auto psPath = std::wstring(basePath) + L"/d3d/text_ps.cso";
            _vertexShader.LoadFromFile(vsPath.c_str());
            _pixelShader.LoadFromFile(psPath.c_str());
        #elif defined(BACKEND_OPENGL)
            auto vsPath = std::wstring(basePath) + L"/glsl/quad_vs.glsl";
            auto psPath = std::wstring(basePath) + L"/glsl/text_ps.glsl";
            _vertexShader.LoadFromFile(vsPath.c_str());
            _pixelShader.LoadFromFile(psPath.c_str());
        #endif
    }

private:
    Shader<Backend> _vertexShader;
    Shader<Backend> _pixelShader;
};
```

### 8.3 Testing Strategy

**Unit Tests:**
```cpp
// Test GlyphAtlas template with mock texture handle
TEST(GlyphAtlasTest, AllocateGlyph) {
    struct MockTexture {};
    GlyphAtlas<MockTexture> atlas({.initialWidth = 1024, .initialHeight = 1024});

    auto* entry = atlas.AllocateGlyph(42, {32, 32});
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->glyphIndex, 42);
    EXPECT_EQ(entry->size.x, 32);
    EXPECT_EQ(entry->size.y, 32);

    auto* found = atlas.FindGlyph(42);
    EXPECT_EQ(found, entry);
}

// Test InstanceBatcher
TEST(InstanceBatcherTest, AddQuad) {
    InstanceBatcher batcher;
    batcher.Clear();

    auto& quad1 = batcher.AddQuad();
    quad1.position = {10, 20};

    auto& quad2 = batcher.AddQuad();
    quad2.position = {30, 40};

    auto instances = batcher.GetInstances();
    EXPECT_EQ(instances.size(), 2);
    EXPECT_EQ(instances[0].position.x, 10);
    EXPECT_EQ(instances[1].position.x, 30);
}
```

**Integration Tests:**
```cpp
// Test that all backends render identically
TEST(BackendIntegrationTest, RenderComparison) {
    RenderingPayload payload = CreateTestPayload();

    auto d3d11Backend = std::make_unique<BackendD3D>();
    auto d3d12Backend = std::make_unique<BackendD3D12>(payload);
    auto openglBackend = std::make_unique<BackendOpenGL>(payload);

    // Render with each backend
    d3d11Backend->Render(payload);
    auto d3d11Frame = CaptureFrameBuffer();

    d3d12Backend->Render(payload);
    auto d3d12Frame = CaptureFrameBuffer();

    openglBackend->Render(payload);
    auto openglFrame = CaptureFrameBuffer();

    // Compare pixel-by-pixel (with tolerance for slight differences)
    EXPECT_FRAMES_SIMILAR(d3d11Frame, d3d12Frame, 0.01f);
    EXPECT_FRAMES_SIMILAR(d3d11Frame, openglFrame, 0.01f);
}
```

**Performance Tests:**
```cpp
// Benchmark rendering performance
TEST(PerformanceTest, RenderingBenchmark) {
    RenderingPayload payload = CreateLargePayload();  // 120x30 full of text

    auto backend = std::make_unique<BackendD3D>();

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; ++i) {
        backend->Render(payload);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    auto avgFrameTime = duration.count() / 1000.0;

    // Should render in < 2ms per frame (500 FPS)
    EXPECT_LT(avgFrameTime, 2000.0);
}
```

### 8.4 Risk Mitigation

**Risk 1: Performance Regression**

*Mitigation:*
- Establish baseline benchmarks before refactoring
- Run benchmarks after each phase
- Use profiler (VTune, PIX) to identify hotspots
- Keep fast-path code inline (avoid virtual calls in hot loops)
- Use templates to enable compile-time optimization

**Risk 2: Rendering Quality Degradation**

*Mitigation:*
- Capture reference screenshots before refactoring
- Pixel-by-pixel comparison after each change
- Visual inspection by designers/users
- Font rendering quality tests (ClearType, grayscale)

**Risk 3: Breaking Existing Backends**

*Mitigation:*
- Migrate one backend at a time
- Keep old code until new code proven
- Feature flags for new/old code paths
- Extensive testing before removing old code

**Risk 4: Increased Build Complexity**

*Mitigation:*
- Document build process thoroughly
- Provide pre-built shader binaries for development
- CI/CD integration for shader compilation
- Fallback to bundled shaders if build fails

**Risk 5: Template Compilation Errors**

*Mitigation:*
- Use concepts (C++20) to constrain templates
- Provide clear static_asserts for errors
- Extensive unit tests for template instantiations
- Documentation with examples for each backend

---

## 9. Code Examples

### 9.1 GlyphAtlas Implementation

```cpp
// File: /src/renderer/atlas/GlyphAtlas.h
#pragma once

#include "common.h"
#include <stb_rect_pack.h>
#include <til/flat_set.h>

namespace Microsoft::Console::Render::Atlas {

// Glyph atlas entry (backend-agnostic)
struct AtlasGlyphEntry {
    u32 glyphIndex;
    u8 occupied;
    ShadingType shadingType;
    u16 overlapSplit;
    i16x2 offset;
    u16x2 size;
    u16x2 texcoord;  // Position in atlas (pixels)
};

struct AtlasGlyphEntryHashTrait {
    static constexpr bool occupied(const AtlasGlyphEntry& entry) noexcept {
        return entry.occupied != 0;
    }

    static constexpr size_t hash(u32 glyphIndex) noexcept {
        return til::flat_set_hash_integer(glyphIndex);
    }

    static constexpr size_t hash(const AtlasGlyphEntry& entry) noexcept {
        return til::flat_set_hash_integer(entry.glyphIndex);
    }

    static constexpr bool equals(const AtlasGlyphEntry& entry, u32 glyphIndex) noexcept {
        return entry.glyphIndex == glyphIndex;
    }

    static constexpr void assign(AtlasGlyphEntry& entry, u32 glyphIndex) noexcept {
        entry.glyphIndex = glyphIndex;
        entry.occupied = 1;
    }
};

// Template glyph atlas (works with any texture handle type)
template<typename TextureHandle>
class GlyphAtlas {
public:
    struct Config {
        u32 initialWidth = 2048;
        u32 initialHeight = 2048;
        u32 maxWidth = 8192;
        u32 maxHeight = 8192;
        bool supportResize = true;
    };

    explicit GlyphAtlas(const Config& config = {})
        : _config(config)
        , _atlasSize{static_cast<u16>(config.initialWidth), static_cast<u16>(config.initialHeight)}
    {
        _initializeRectPacker();
    }

    // Allocate space for a glyph in the atlas
    // Returns nullptr if allocation fails (atlas full)
    AtlasGlyphEntry* AllocateGlyph(u32 glyphIndex, u16x2 size) {
        // Check if already exists
        auto* existing = FindGlyph(glyphIndex);
        if (existing) {
            return existing;
        }

        // Try to allocate space
        u16x2 texcoord;
        if (!_tryAllocate(size, texcoord)) {
            // Atlas full, try to resize
            if (_config.supportResize && _tryResize()) {
                // Retry allocation after resize
                if (!_tryAllocate(size, texcoord)) {
                    return nullptr;  // Failed even after resize
                }
            } else {
                return nullptr;  // Cannot resize or resize failed
            }
        }

        // Create entry
        auto* entry = _glyphs.insert(glyphIndex);
        entry->glyphIndex = glyphIndex;
        entry->size = size;
        entry->texcoord = texcoord;
        entry->shadingType = ShadingType::TextGrayscale;  // Default
        entry->overlapSplit = 0;
        entry->offset = {0, 0};

        return entry;
    }

    // Find existing glyph in atlas
    const AtlasGlyphEntry* FindGlyph(u32 glyphIndex) const {
        return _glyphs.find(glyphIndex);
    }

    AtlasGlyphEntry* FindGlyph(u32 glyphIndex) {
        return _glyphs.find(glyphIndex);
    }

    // Clear all glyphs (keeps atlas texture allocated)
    void Clear() {
        _glyphs.clear();
        _initializeRectPacker();
    }

    // Get atlas texture (backend-specific handle)
    TextureHandle GetTexture() const {
        return _atlasTexture;
    }

    void SetTexture(TextureHandle texture) {
        _atlasTexture = texture;
    }

    // Get atlas size in pixels
    u16x2 GetAtlasSize() const {
        return _atlasSize;
    }

    // Statistics
    struct Stats {
        u32 glyphCount;
        u32 atlasWidth;
        u32 atlasHeight;
        f32 utilizationPercent;
    };

    Stats GetStats() const {
        Stats stats{};
        stats.glyphCount = static_cast<u32>(_glyphs.size());
        stats.atlasWidth = _atlasSize.x;
        stats.atlasHeight = _atlasSize.y;

        // Calculate utilization (sum of glyph areas / total atlas area)
        u64 usedPixels = 0;
        for (const auto& entry : _glyphs) {
            usedPixels += static_cast<u64>(entry.size.x) * entry.size.y;
        }

        u64 totalPixels = static_cast<u64>(_atlasSize.x) * _atlasSize.y;
        stats.utilizationPercent = totalPixels > 0
            ? static_cast<f32>(usedPixels) / totalPixels * 100.0f
            : 0.0f;

        return stats;
    }

private:
    void _initializeRectPacker() {
        _rectPackerData = Buffer<stbrp_node>(static_cast<size_t>(_atlasSize.x));
        stbrp_init_target(&_rectPacker, _atlasSize.x, _atlasSize.y,
                          _rectPackerData.data(), _atlasSize.x);
    }

    bool _tryAllocate(u16x2 size, u16x2& outTexcoord) {
        stbrp_rect rect{};
        rect.w = static_cast<stbrp_coord>(size.x);
        rect.h = static_cast<stbrp_coord>(size.y);

        if (stbrp_pack_rects(&_rectPacker, &rect, 1) == 0) {
            return false;  // Allocation failed
        }

        outTexcoord = {static_cast<u16>(rect.x), static_cast<u16>(rect.y)};
        return true;
    }

    bool _tryResize() {
        // Try to double atlas size
        u32 newWidth = std::min(_atlasSize.x * 2, _config.maxWidth);
        u32 newHeight = std::min(_atlasSize.y * 2, _config.maxHeight);

        if (newWidth == _atlasSize.x && newHeight == _atlasSize.y) {
            return false;  // Already at max size
        }

        // Note: Caller must recreate texture and re-upload all glyphs
        _atlasSize = {static_cast<u16>(newWidth), static_cast<u16>(newHeight)};
        _initializeRectPacker();

        // Re-allocate all existing glyphs
        // (This is a simplified version; real implementation would preserve glyphs)
        return true;
    }

    Config _config;
    TextureHandle _atlasTexture{};
    u16x2 _atlasSize;

    til::linear_flat_set<AtlasGlyphEntry, AtlasGlyphEntryHashTrait> _glyphs;
    Buffer<stbrp_node> _rectPackerData;
    stbrp_context _rectPacker{};
};

} // namespace Microsoft::Console::Render::Atlas
```

### 9.2 Texture2D Resource Wrapper

```cpp
// File: /src/renderer/atlas/Resources.h
#pragma once

#include "common.h"

namespace Microsoft::Console::Render::Atlas {

// Backend tag types
struct D3D11Backend {};
struct D3D12Backend {};
struct OpenGLBackend {};
struct VulkanBackend {};

// Forward declarations
template<typename Backend> class Texture2D;
template<typename Backend> class Buffer;

//-----------------------------------------------------------------------------
// Texture2D<D3D11Backend> Specialization
//-----------------------------------------------------------------------------
template<>
class Texture2D<D3D11Backend> {
public:
    Texture2D() = default;
    ~Texture2D() = default;

    Texture2D(const Texture2D&) = delete;
    Texture2D& operator=(const Texture2D&) = delete;

    Texture2D(Texture2D&& other) noexcept
        : _texture(std::move(other._texture))
        , _srv(std::move(other._srv))
        , _width(other._width)
        , _height(other._height)
    {
        other._width = 0;
        other._height = 0;
    }

    Texture2D& operator=(Texture2D&& other) noexcept {
        if (this != &other) {
            _texture = std::move(other._texture);
            _srv = std::move(other._srv);
            _width = other._width;
            _height = other._height;
            other._width = 0;
            other._height = 0;
        }
        return *this;
    }

    // Create texture
    void Create(ID3D11Device* device, u32 width, u32 height, DXGI_FORMAT format,
                D3D11_USAGE usage = D3D11_USAGE_DEFAULT) {
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = format;
        desc.SampleDesc.Count = 1;
        desc.Usage = usage;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = (usage == D3D11_USAGE_DYNAMIC) ? D3D11_CPU_ACCESS_WRITE : 0;

        THROW_IF_FAILED(device->CreateTexture2D(&desc, nullptr, _texture.put()));

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;

        THROW_IF_FAILED(device->CreateShaderResourceView(
            _texture.get(), &srvDesc, _srv.put()));

        _width = width;
        _height = height;
    }

    // Upload data to texture
    void Upload(ID3D11DeviceContext* ctx, const void* data, u32 rowPitch) {
        assert(_texture);
        ctx->UpdateSubresource(_texture.get(), 0, nullptr, data, rowPitch, 0);
    }

    // Upload data to sub-region
    void UploadRegion(ID3D11DeviceContext* ctx, const void* data,
                      u32 x, u32 y, u32 width, u32 height, u32 rowPitch) {
        assert(_texture);
        D3D11_BOX box{};
        box.left = x;
        box.top = y;
        box.right = x + width;
        box.bottom = y + height;
        box.front = 0;
        box.back = 1;

        ctx->UpdateSubresource(_texture.get(), 0, &box, data, rowPitch, 0);
    }

    // Bind to pixel shader
    void Bind(ID3D11DeviceContext* ctx, u32 slot) {
        assert(_srv);
        ID3D11ShaderResourceView* views[] = { _srv.get() };
        ctx->PSSetShaderResources(slot, 1, views);
    }

    // Getters
    ID3D11Texture2D* GetTexture() const { return _texture.get(); }
    ID3D11ShaderResourceView* GetSRV() const { return _srv.get(); }
    u32 GetWidth() const { return _width; }
    u32 GetHeight() const { return _height; }

    explicit operator bool() const { return static_cast<bool>(_texture); }

private:
    wil::com_ptr<ID3D11Texture2D> _texture;
    wil::com_ptr<ID3D11ShaderResourceView> _srv;
    u32 _width = 0;
    u32 _height = 0;
};

//-----------------------------------------------------------------------------
// Texture2D<D3D12Backend> Specialization
//-----------------------------------------------------------------------------
template<>
class Texture2D<D3D12Backend> {
public:
    Texture2D() = default;
    ~Texture2D() = default;

    void Create(ID3D12Device* device, u32 width, u32 height, DXGI_FORMAT format) {
        // Create committed resource in default heap
        D3D12_HEAP_PROPERTIES heapProps{};
        heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width = width;
        desc.Height = height;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = format;
        desc.SampleDesc.Count = 1;
        desc.Flags = D3D12_RESOURCE_FLAG_NONE;

        THROW_IF_FAILED(device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(_texture.ReleaseAndGetAddressOf())));

        // Create upload buffer
        const u64 uploadBufferSize = GetRequiredIntermediateSize(
            _texture.Get(), 0, 1);

        heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC uploadDesc{};
        uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        uploadDesc.Width = uploadBufferSize;
        uploadDesc.Height = 1;
        uploadDesc.DepthOrArraySize = 1;
        uploadDesc.MipLevels = 1;
        uploadDesc.Format = DXGI_FORMAT_UNKNOWN;
        uploadDesc.SampleDesc.Count = 1;
        uploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        THROW_IF_FAILED(device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &uploadDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(_uploadBuffer.ReleaseAndGetAddressOf())));

        _width = width;
        _height = height;
        _currentState = D3D12_RESOURCE_STATE_COMMON;
    }

    void Upload(ID3D12GraphicsCommandList* cmdList, const void* data, u32 rowPitch) {
        assert(_texture);
        assert(_uploadBuffer);

        // Transition to COPY_DEST
        TransitionBarrier(cmdList, D3D12_RESOURCE_STATE_COPY_DEST);

        // Prepare subresource data
        D3D12_SUBRESOURCE_DATA subresourceData{};
        subresourceData.pData = data;
        subresourceData.RowPitch = rowPitch;
        subresourceData.SlicePitch = rowPitch * _height;

        // Upload using helper
        UpdateSubresources(cmdList, _texture.Get(), _uploadBuffer.Get(),
                          0, 0, 1, &subresourceData);

        // Transition to PIXEL_SHADER_RESOURCE
        TransitionBarrier(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }

    void TransitionBarrier(ID3D12GraphicsCommandList* cmdList,
                          D3D12_RESOURCE_STATES newState) {
        if (_currentState == newState) {
            return;  // Already in desired state
        }

        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = _texture.Get();
        barrier.Transition.StateBefore = _currentState;
        barrier.Transition.StateAfter = newState;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        cmdList->ResourceBarrier(1, &barrier);
        _currentState = newState;
    }

    ID3D12Resource* Get() const { return _texture.Get(); }
    D3D12_CPU_DESCRIPTOR_HANDLE GetSRV() const { return _srvHandle; }
    void SetSRV(D3D12_CPU_DESCRIPTOR_HANDLE handle) { _srvHandle = handle; }

    u32 GetWidth() const { return _width; }
    u32 GetHeight() const { return _height; }

    explicit operator bool() const { return static_cast<bool>(_texture); }

private:
    Microsoft::WRL::ComPtr<ID3D12Resource> _texture;
    Microsoft::WRL::ComPtr<ID3D12Resource> _uploadBuffer;
    D3D12_CPU_DESCRIPTOR_HANDLE _srvHandle{};
    D3D12_RESOURCE_STATES _currentState = D3D12_RESOURCE_STATE_COMMON;
    u32 _width = 0;
    u32 _height = 0;
};

//-----------------------------------------------------------------------------
// Texture2D<OpenGLBackend> Specialization
//-----------------------------------------------------------------------------
template<>
class Texture2D<OpenGLBackend> {
public:
    Texture2D() = default;

    ~Texture2D() {
        if (_texture != 0) {
            glDeleteTextures(1, &_texture);
            _texture = 0;
        }
    }

    Texture2D(const Texture2D&) = delete;
    Texture2D& operator=(const Texture2D&) = delete;

    Texture2D(Texture2D&& other) noexcept
        : _texture(other._texture)
        , _width(other._width)
        , _height(other._height)
        , _format(other._format)
    {
        other._texture = 0;
        other._width = 0;
        other._height = 0;
    }

    void Create(u32 width, u32 height, GLenum internalFormat) {
        assert(_texture == 0);

        glGenTextures(1, &_texture);
        glBindTexture(GL_TEXTURE_2D, _texture);

        // Allocate storage
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height,
                     0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        // Set texture parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glBindTexture(GL_TEXTURE_2D, 0);

        _width = width;
        _height = height;
        _format = internalFormat;
    }

    void Upload(const void* data, GLenum format, GLenum type, u32 level = 0) {
        assert(_texture != 0);

        glBindTexture(GL_TEXTURE_2D, _texture);
        glTexSubImage2D(GL_TEXTURE_2D, level, 0, 0, _width, _height,
                        format, type, data);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void UploadRegion(const void* data, u32 x, u32 y, u32 width, u32 height,
                      GLenum format, GLenum type) {
        assert(_texture != 0);

        glBindTexture(GL_TEXTURE_2D, _texture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, width, height, format, type, data);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void Bind(u32 unit) {
        assert(_texture != 0);
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, _texture);
    }

    GLuint Get() const { return _texture; }
    u32 GetWidth() const { return _width; }
    u32 GetHeight() const { return _height; }

    explicit operator bool() const { return _texture != 0; }

private:
    GLuint _texture = 0;
    u32 _width = 0;
    u32 _height = 0;
    GLenum _format = GL_RGBA8;
};

} // namespace Microsoft::Console::Render::Atlas
```

### 9.3 Backend Implementation Example

```cpp
// File: /src/renderer/atlas/BackendD3D.h (updated with new architecture)
#pragma once

#include "Backend.h"
#include "GlyphAtlas.h"
#include "InstanceBatcher.h"
#include "FontRasterizer.h"
#include "Resources.h"

namespace Microsoft::Console::Render::Atlas {

class BackendD3D : IBackend {
public:
    BackendD3D();
    ~BackendD3D() override;

    void ReleaseResources() noexcept override;
    void Render(RenderingPayload& payload) override;
    bool RequiresContinuousRedraw() noexcept override;

private:
    // Initialization
    void _initialize(const RenderingPayload& p);
    void _handleSettingsUpdate(const RenderingPayload& p);
    void _updateFontDependents(const RenderingPayload& p);

    // Rendering passes
    void _updateConstantBuffers(const RenderingPayload& p);
    void _uploadBackgroundBitmap(const RenderingPayload& p);
    void _drawBackground(const RenderingPayload& p);
    void _drawText(RenderingPayload& p);
    void _drawGridlines(const RenderingPayload& p);
    void _drawCursor(const RenderingPayload& p);
    void _drawSelection(const RenderingPayload& p);

    //-------------------------------------------------------------------------
    // Tier 1: Common Utilities (shared across all backends)
    //-------------------------------------------------------------------------
    GlyphAtlas<wil::com_ptr<ID3D11Texture2D>> _glyphAtlas;
    InstanceBatcher _instanceBatcher;
    DirectWriteRasterizer _fontRasterizer;  // Windows-specific

    //-------------------------------------------------------------------------
    // Tier 2: Abstract Resources (backend-specific wrappers)
    //-------------------------------------------------------------------------
    Texture2D<D3D11Backend> _backgroundBitmap;
    Buffer<D3D11Backend> _instanceBuffer;
    Buffer<D3D11Backend> _vertexBuffer;
    Buffer<D3D11Backend> _indexBuffer;
    ConstantBuffer<D3D11Backend, VSConstBuffer> _vsConstants;
    ConstantBuffer<D3D11Backend, PSConstBuffer> _psConstants;
    Shader<D3D11Backend> _vertexShader;
    Shader<D3D11Backend> _pixelShader;
    Pipeline<D3D11Backend> _pipeline;

    //-------------------------------------------------------------------------
    // Tier 3: D3D11-Specific Implementation Details
    //-------------------------------------------------------------------------
    wil::com_ptr<ID3D11Device> _device;
    wil::com_ptr<ID3D11DeviceContext> _context;
    wil::com_ptr<ID3D11RenderTargetView> _renderTargetView;
    wil::com_ptr<ID3D11BlendState> _blendState;

    // State tracking
    til::generation_t _generation;
    til::generation_t _fontGeneration;
    u16x2 _targetSize{};
    bool _requiresContinuousRedraw = false;
};

} // namespace Microsoft::Console::Render::Atlas
```

**BackendD3D.cpp (simplified rendering flow):**

```cpp
// File: /src/renderer/atlas/BackendD3D.cpp
#include "pch.h"
#include "BackendD3D.h"

using namespace Microsoft::Console::Render::Atlas;

BackendD3D::BackendD3D()
    : _glyphAtlas({.initialWidth = 2048, .initialHeight = 2048})
{
}

void BackendD3D::Render(RenderingPayload& payload) {
    // Handle settings changes
    if (_generation != payload.s.generation()) {
        _handleSettingsUpdate(payload);
        _generation = payload.s.generation();
    }

    // Update rendering state
    _updateConstantBuffers(payload);
    _uploadBackgroundBitmap(payload);

    // Rendering passes
    _drawBackground(payload);
    _drawText(payload);
    _drawGridlines(payload);
    _drawCursor(payload);
    _drawSelection(payload);
}

void BackendD3D::_drawText(RenderingPayload& payload) {
    // Clear instance batcher from previous frame
    _instanceBatcher.Clear();

    // Iterate through all rows
    for (auto row : payload.rows) {
        if (!row) continue;

        // Iterate through font mappings in this row
        for (auto& mapping : row->mappings) {
            IDWriteFontFace* fontFace = mapping.fontFace;

            // Process each glyph
            for (u32 i = mapping.glyphsFrom; i < mapping.glyphsTo; ++i) {
                u16 glyphIndex = row->glyphIndices[i];

                // Find glyph in atlas
                auto* entry = _glyphAtlas.FindGlyph(glyphIndex);

                if (!entry) {
                    // Glyph not in atlas, rasterize it
                    auto glyphData = _fontRasterizer.RasterizeGlyph(
                        fontFace,
                        glyphIndex,
                        payload.s->font->fontSize,
                        false  // grayscale
                    );

                    // Allocate space in atlas
                    entry = _glyphAtlas.AllocateGlyph(glyphIndex, glyphData.size);
                    if (!entry) {
                        // Atlas full, skip this glyph (or resize atlas)
                        continue;
                    }

                    // Upload glyph bitmap to atlas
                    auto atlasTexture = _glyphAtlas.GetTexture();
                    if (atlasTexture) {
                        D3D11_BOX box{};
                        box.left = entry->texcoord.x;
                        box.top = entry->texcoord.y;
                        box.right = entry->texcoord.x + glyphData.size.x;
                        box.bottom = entry->texcoord.y + glyphData.size.y;
                        box.front = 0;
                        box.back = 1;

                        _context->UpdateSubresource(
                            atlasTexture.get(), 0, &box,
                            glyphData.pixels.data(),
                            glyphData.size.x,  // row pitch
                            0
                        );
                    }
                }

                // Add quad instance for this glyph
                auto& quad = _instanceBatcher.AddQuad();
                quad.shadingType = static_cast<u16>(ShadingType::TextGrayscale);
                quad.position = {
                    static_cast<i16>(row->glyphOffsets[i].x),
                    static_cast<i16>(row->glyphOffsets[i].y)
                };
                quad.size = entry->size;
                quad.texcoord = entry->texcoord;
                quad.color = row->colors[i];
            }
        }
    }

    // Upload instance data to GPU
    auto instances = _instanceBatcher.GetInstances();
    if (!instances.empty()) {
        _instanceBuffer.Upload(_context.get(), instances.data(),
                              instances.size() * sizeof(QuadInstance));

        // Bind resources
        _pipeline.Bind(_context.get());
        _glyphAtlas.GetTexture()->Bind(_context.get(), 0);

        // Draw all instances
        _context->DrawInstanced(
            6,                          // 6 vertices per quad (2 triangles)
            instances.size(),           // instance count
            0,                          // start vertex
            0                           // start instance
        );
    }
}

void BackendD3D::_updateConstantBuffers(const RenderingPayload& p) {
    // Update vertex shader constants
    VSConstBuffer vsConstants{};
    vsConstants.positionScale = {
        2.0f / p.s->targetSize.x,
        -2.0f / p.s->targetSize.y
    };
    _vsConstants.Update(vsConstants);

    // Update pixel shader constants
    PSConstBuffer psConstants{};
    psConstants.backgroundColor = colorFromU32<f32x4>(p.s->misc->backgroundColor);
    psConstants.backgroundCellSize = {
        static_cast<f32>(p.s->font->cellSize.x),
        static_cast<f32>(p.s->font->cellSize.y)
    };
    psConstants.backgroundCellCount = {
        static_cast<f32>(p.s->viewportCellCount.x),
        static_cast<f32>(p.s->viewportCellCount.y)
    };
    _psConstants.Update(psConstants);
}

bool BackendD3D::RequiresContinuousRedraw() noexcept {
    return _requiresContinuousRedraw;
}
```

---

## 10. Risk Analysis

### 10.1 Technical Risks

| Risk | Probability | Impact | Mitigation | Contingency |
|------|------------|--------|------------|-------------|
| Performance regression | Medium | High | Benchmarks, profiling, inline hot paths | Revert specific changes |
| Rendering quality loss | Low | Critical | Pixel comparison, visual inspection | Immediate rollback |
| Template compilation errors | Medium | Medium | Unit tests, clear errors, concepts | Simplify templates |
| Increased build time | Medium | Low | Parallel builds, shader caching | Pre-build shaders |
| D3D12 barrier bugs | Medium | High | Automated barrier validation | Explicit barriers |
| OpenGL state leakage | Medium | Medium | State caching, validation layer | Reset state frequently |

### 10.2 Project Risks

| Risk | Probability | Impact | Mitigation | Contingency |
|------|------------|--------|------------|-------------|
| Timeline overrun | Medium | Medium | Phased approach, MVP first | Reduce scope |
| Breaking changes to API | Low | High | Semantic versioning, deprecation | Compatibility shims |
| Team unfamiliarity | Medium | Medium | Documentation, code reviews | Pair programming |
| Merge conflicts | High | Low | Frequent integration, small PRs | Dedicated merge week |

### 10.3 Success Criteria

**Must Have:**
1. No performance regression (< 5% slower)
2. No visual quality degradation
3. All existing backends work identically
4. All unit tests pass
5. Code duplication reduced by > 50%

**Should Have:**
1. Performance improvement (> 10% faster)
2. Reduced binary size (< 10% smaller)
3. Easier to add new backends
4. Improved debuggability
5. Shader hot-reload works on all backends

**Nice to Have:**
1. Vulkan backend implementation
2. Metal backend (for future macOS)
3. WebGPU backend (for web)
4. Advanced debugging tools (render graph visualization)

### 10.4 Rollback Plan

**Immediate Rollback (< 1 day):**
- Revert last commit
- Re-run CI/CD pipeline
- Verify all tests pass

**Partial Rollback (1 week):**
- Keep Tier 1 utilities (GlyphAtlas, InstanceBatcher)
- Revert Tier 2 abstractions (Resource wrappers)
- Maintain old backend implementations

**Full Rollback (2 weeks):**
- Archive refactored code in feature branch
- Restore original architecture
- Document lessons learned
- Plan future iteration

---

## Conclusion

This comprehensive analysis demonstrates that a **Hybrid Abstraction Pattern** is the optimal architecture for Ultra-Riced Windows Terminal's multi-backend rendering system. By adopting a three-tier architecture with shared utilities, abstract resource wrappers, and backend-specific implementations, the codebase can achieve:

1. **85% reduction in code duplication**
2. **Minimal performance overhead (< 5%)**
3. **Easier backend addition** (Vulkan, WebGPU, Metal)
4. **Improved maintainability** (single source of truth for shared logic)
5. **Better testing** (backend-agnostic unit tests)

The phased refactoring plan provides a safe, incremental path from the current architecture to the recommended design, with clear rollback points at each phase. With proper testing and benchmarking, this refactoring will position Windows Terminal for long-term success with minimal risk.

**Next Steps:**
1. Review and approve this architectural plan
2. Create detailed task breakdown for Phase 1
3. Set up performance benchmarking infrastructure
4. Begin Phase 1: Extract Common Utilities

---

## Appendix A: References

**Industry Rendering Frameworks:**
1. Chromium Skia Graphite: https://blog.chromium.org/2025/07/introducing-skia-graphite-chromes.html
2. Firefox WebRender: https://github.com/servo/webrender
3. bgfx: https://github.com/bkaradzic/bgfx
4. The Forge: https://github.com/ConfettiFX/The-Forge
5. DiligentEngine: https://github.com/DiligentGraphics/DiligentEngine
6. Sokol: https://github.com/floooh/sokol

**Shader Cross-Compilation:**
1. DirectX Shader Compiler (DXC): https://github.com/microsoft/DirectXShaderCompiler
2. SPIRV-Cross: https://github.com/KhronosGroup/SPIRV-Cross
3. glslang: https://github.com/KhronosGroup/glslang

**Terminal Renderers:**
1. Alacritty: https://github.com/alacritty/alacritty
2. Warp: https://www.warp.dev/blog/adventures-text-rendering-kerning-glyph-atlases

**Font Rasterization:**
1. font-kit (Rust): https://github.com/servo/font-kit
2. crossfont (Rust): https://lib.rs/crates/crossfont
3. FreeType: http://freetype.org/

**Render Graphs:**
1. GDC FrameGraph Talk: https://www.gdcvault.com/play/1024612/FrameGraph-Extensible-Rendering-Architecture-in
2. Render Graphs Blog: https://logins.github.io/graphics/2021/05/31/RenderGraphs.html

---

**End of Report**
