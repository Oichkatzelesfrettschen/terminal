# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is an optimized fork of Windows Terminal focused on high-performance rendering through modern graphics APIs. The project implements multiple rendering backends (D3D11, D3D12, D2D, and experimental OpenGL) with aggressive compiler optimizations and AVX2/SIMD vectorization.

**Key Enhancement**: Added UltraPerformance build configuration targeting AVX2-capable CPUs (Intel Haswell 2013+ / AMD Excavator 2015+) with 20-40% performance improvements over Release builds.

**Active Development Focus**: Completing D3D12 backend feature parity with D3D11, implementing OpenGL backend for cross-platform support, and integrating SPIR-V shader cross-compilation.

## Build System

### Prerequisites
- Visual Studio 2022
- Windows SDK 10.0.22621.0+
- PowerShell 7+
- Developer Mode enabled in Windows Settings
- Git submodules: `git submodule update --init --recursive`

### Building

**Primary solution file**: `OpenConsole.slnx` (Visual Studio solution)

#### PowerShell (Recommended)
```powershell
Import-Module .\tools\OpenConsole.psm1
Set-MsBuildDevEnvironment
Invoke-OpenConsoleBuild
```

Additional commands:
- `Invoke-OpenConsoleBuild -Configuration UltraPerformance -Platform x64` - Build with AVX2 optimizations
- `Invoke-OpenConsoleTests` - Run unit tests
- `Start-OpenConsole` - Launch built executable (x64 default)
- `Debug-OpenConsole` - Launch with debugger attached
- `Invoke-CodeFormat` - Format code with clang-format

#### Cmd
```cmd
.\tools\razzle.cmd
bcz
```

Test scripts: `runut.cmd`, `runft.cmd`, `runuia.cmd`, `runformat`

### Build Configurations

1. **Debug** - Development with full debug symbols, no optimizations
2. **Release** - Standard production build, baseline x64 ISA (SSE2)
3. **UltraPerformance** - AVX2/SIMD optimizations, aggressive inlining, LTCG
   - Configuration file: `src/common.build.ultraperformance.props`
   - 20-40% performance gain over Release
   - **Requires AVX2 CPU** - Will crash on older hardware
   - Build: `msbuild OpenConsole.sln /p:Configuration=UltraPerformance /p:Platform=x64 /m`
4. **AuditMode** - Static analysis enabled (PREfast, C++ Core Check)
5. **Fuzzing** - ASAN enabled for fuzz testing

### Running & Debugging

To debug Windows Terminal in VS:
1. Right-click `CascadiaPackage` in Solution Explorer
2. Go to Properties > Debug
3. Change "Application process" and "Background task process" to "Native Only"
4. Press F5

**Note**: Cannot launch `WindowsTerminal.exe` directly - must deploy as package (see #926, #4043)

### Building Terminal Package

```cmd
pushd %OPENCON%\src\cascadia\CascadiaPackage
bx
"C:\Program Files\Microsoft Visual Studio\2022\Preview\Common7\IDE\DeployAppRecipe.exe" bin\%ARCH%\%_LAST_BUILD_CONF%\CascadiaPackage.build.appxrecipe
popd
```

## Architecture

### Rendering System (AtlasEngine)

**Core Component**: `src/renderer/atlas/` - High-performance text rendering engine

The AtlasEngine is a modern renderer that replaced the legacy GDI renderer. It uses:
- **Glyph atlas**: Texture-based glyph caching with DirectWrite integration
- **Batch rendering**: Instanced rendering of quads with per-instance data
- **Multiple backends**: Swappable graphics API implementations

#### Backend Architecture

All backends implement a common interface but with **significant feature gaps**:

**Backend Files**:
- `Backend.h` - Base interface and common utilities
- `BackendD2D.cpp/h` - Direct2D implementation (legacy, 75/127 features)
- `BackendD3D.cpp/h` - Direct3D 11 implementation (124/127 features, production reference)
- `BackendD3D12.cpp/h` - Direct3D 12 implementation (24/127 features, **INCOMPLETE**)
- `BackendD3D12.compute.cpp` - D3D12 compute shader support
- `BackendOpenGL.h` - OpenGL 3.3 implementation (98/127 features, **IN PROGRESS**)

**Critical Insight**: D3D12 backend has excellent low-level architecture but **cannot display text** - missing DirectWrite integration, glyph atlas system, and 103 other features. See `MASTER_IMPLEMENTATION_ROADMAP.md` for complete feature gap analysis.

#### Key AtlasEngine Components

- `AtlasEngine.cpp` - Main rendering logic, text shaping, glyph mapping
- `AtlasEngine.api.cpp` - Public API implementation (IRenderEngine interface)
- `AtlasEngine.r.cpp` - Resource management, backend selection
- `dwrite.cpp` - DirectWrite text analysis and font management
- `DWriteTextAnalysis.cpp` - Text shaping, bidi, script analysis
- `BuiltinGlyphs.cpp` - Box drawing characters, powerline glyphs
- `wic.cpp` - Windows Imaging Component for texture loading
- `stb_rect_pack.cpp` - Rectangle packing for glyph atlas

#### Backend Selection

`AtlasEngine.r.cpp` contains backend instantiation based on `GraphicsAPI` enum:
- `GraphicsAPI::Automatic` - Selects D3D11 by default
- `GraphicsAPI::Direct2D` - Legacy backend
- `GraphicsAPI::Direct3D11` - Current production backend
- `GraphicsAPI::Direct3D12` - **Not yet integrated** (enum value needs to be added)

### Project Structure

Key directories:
- `/src/cascadia/` - Windows Terminal application
  - `TerminalCore/` - Core terminal logic (buffer, VT parsing, input)
  - `TerminalControl/` - UWP XAML control with DX renderer
  - `TerminalApp/` - Terminal application UI (tabs, panes, settings)
  - `WindowsTerminal/` - Win32 hosting and XAML Islands
  - `CascadiaPackage/` - MSIX packaging
  - `TerminalConnection/` - Connection backends (ConPTY, Azure, SSH)
  - `TerminalSettings/` - Settings abstraction layer
- `/src/host/` - Console host (conhost.exe) implementation
- `/src/renderer/` - Rendering abstraction layer
  - `base/` - Base renderer interface
  - `atlas/` - Modern AtlasEngine renderer
  - `gdi/` - Legacy GDI renderer
- `/src/terminal/` - VT sequence support
  - `parser/` - VT sequence state machine
  - `adapter/` - VT-to-console-API translation
- `/src/types/` - Shared types, utilities
  - `CpuFeatures.cpp/h` - CPU feature detection (AVX2, SSE, etc.)

### Shader Architecture

**Active Research**: SPIR-V cross-compilation for shader portability

Current shader files:
- D3D12: `shader_d3d12_vs.hlsl`, `shader_d3d12_ps.hlsl`
- D3D12 Compute: `glyph_rasterize_cs.hlsl`, `grid_generate_cs.hlsl`
- OpenGL: `shader_gl_vs.glsl`, `shader_gl_fs.glsl`, `shader_gl_common.glsl`

**Future Plan**: HLSL -> SPIR-V -> GLSL/MSL pipeline using DXC + SPIRV-Cross (see `docs/SHADER_CROSS_COMPILATION_RESEARCH.md`)

## Implementation Roadmap

**Phase 0** (URGENT - 2 hours): Fix D3D12 compilation blockers
1. Download d3dx12.h from DirectX-Headers GitHub
2. Fix shader Output semantics (add `: SV_Target0` to shader_d3d12_ps.hlsl:34-38)
3. Add `GraphicsAPI::Direct3D12` enum value
4. Update AtlasEngine.r.cpp backend instantiation
5. Fix DXGI version mismatch (standardize on dxgi1_6.h)
6. Move static quad buffers to BackendD3D12 members
7. Fix input layout semantic mismatch (use POSITION semantic)

**Phase 1** (2-3 weeks): Font abstraction layer
- Create `IFontBackend` interface (RasterizeGlyph, GetMetrics, LoadFont)
- Implement `DirectWriteFontBackend` for Windows
- Implement `FreeTypeFontBackend` for Linux/cross-platform
- Required for both D3D12 text rendering and OpenGL backend

**Phase 2** (8 weeks): OpenGL MVP
- Context creation (WGL/GLX)
- Shader compilation and pipeline
- Basic glyph atlas
- Text rendering
- Background colors and one cursor type

**Phase 3** (6 weeks): Feature completeness
- All cursor types (6 variants)
- Underlines (solid, double, dotted, dashed, curly)
- Custom shaders
- Line renditions

**Phase 4** (6 weeks): Modern D3D12 optimizations
- Enhanced Barriers API
- ExecuteIndirect (GPU-driven rendering)
- Ring buffer upload heaps
- Variable Rate Shading
- Target: <5ms latency, <8% CPU

See `MASTER_IMPLEMENTATION_ROADMAP.md` for complete 26-week plan and `COMPREHENSIVE_AUDIT_SUMMARY.md` for detailed feature matrix.

## Key Files for Optimization Work

Performance-critical paths:
- `src/renderer/atlas/Backend.h` - Hot path rendering utilities
- `src/renderer/atlas/AtlasEngine.cpp:_mapRegularText()` - Text shaping (called per frame)
- `src/types/CpuFeatures.cpp` - CPU feature detection for SIMD
- `src/common.build.ultraperformance.props` - Compiler optimization flags

Debugging aids in `Backend.h`:
- `ATLAS_DEBUG_DISABLE_PARTIAL_INVALIDATION` - Benchmark text shaping
- `ATLAS_DEBUG_CONTINUOUS_REDRAW` - Shader debugging
- `ATLAS_DEBUG_SHADER_HOT_RELOAD` - Live shader reload
- `ATLAS_DEBUG_SHOW_DIRTY` - Visualize dirty rects
- `ATLAS_DEBUG_DUMP_RENDER_TARGET` - Save frames as PNG

## Development Patterns

1. **Follow existing code patterns** - Consistency is critical
2. **Package into libraries** with clean interfaces
3. **Unit tests required**: `ut_*` folders alongside implementation
4. **Functional tests**: `ft_*` folders for integration tests
5. **Interfaces in `inc/` folders** for public APIs
6. **No placeholders or TODOs** - Complete implementations only
7. **Test on multiple backends** - Changes should not regress D3D11

## Common Pitfalls

1. **D3D12 backend is not functional** - Do not assume it works like D3D11
2. **AVX2 builds crash on old CPUs** - Verify CPU support before using UltraPerformance
3. **Cannot run WindowsTerminal.exe directly** - Must deploy as packaged app
4. **DirectWrite/D2D interop is complex** - Reference D3D11 implementation
5. **Shader semantics must match** - Input layout and shader must agree exactly
6. **LTCG significantly slows builds** - Use Release for iterative development

## Testing

Run all tests:
```powershell
Invoke-OpenConsoleTests
```

Run specific test types:
```cmd
runut.cmd   # Unit tests
runft.cmd   # Feature tests
runuia.cmd  # UI Automation tests
```

Benchmarking:
```bash
# Using vtebench (external tool)
cargo install vtebench
vtebench --bench scrolling bin/x64/UltraPerformance/WindowsTerminal.exe
```

## Documentation Resources

Project documentation in `/doc/`:
- `ORGANIZATION.md` - Detailed code organization
- `STYLE.md` - Coding style guidelines
- `building.md` - Detailed build instructions
- `Debugging.md` - Debugging tips and tricks
- `WIL.md` - Windows Implementation Library patterns

Performance research in `/research/` and `/docs/`:
- `MASTER_IMPLEMENTATION_ROADMAP.md` - Complete implementation plan (860-1280 hours)
- `COMPREHENSIVE_AUDIT_SUMMARY.md` - Feature gap analysis (127 features audited)
- `BUILD_CONFIGURATIONS.md` - Build configuration details
- `docs/COMPLETE_RENDERER_FEATURE_AUDIT.md` - Backend feature matrix
- `docs/SHADER_CROSS_COMPILATION_RESEARCH.md` - SPIR-V toolchain analysis
- `research/advanced-rendering-techniques-report.md` - 60+ optimization techniques

## Special Considerations for This Fork

1. **SPIR-V Integration**: When working on shaders, prepare for SPIR-V as intermediate format (DXC -> SPIR-V -> SPIRV-Cross)
2. **Cross-Platform Focus**: OpenGL backend is priority for Linux/WSL2 support
3. **Performance First**: Prefer zero-overhead abstractions, avoid virtual dispatch in hot paths
4. **AVX2 Utilization**: Vectorize text processing, UTF-8/UTF-16 conversion, memory operations
5. **GPU-Driven Rendering**: Move work to GPU where possible (ExecuteIndirect, compute shaders)

## Current State (2025-10-11)

**Production Ready**: D3D11 backend (BackendD3D.cpp)
**In Development**: D3D12 backend (needs font system, 103 missing features)
**Experimental**: OpenGL backend (header exists, implementation in progress)
**Build System**: UltraPerformance configuration tested and working

Next immediate action: Complete Phase 0 (fix D3D12 compilation) to unblock further development.
