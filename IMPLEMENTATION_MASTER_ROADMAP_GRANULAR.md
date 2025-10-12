# Implementation Master Roadmap - Hyperfinegrained
## Ultra-Riced Windows Terminal - Complete Task Breakdown

**Date**: 2025-10-11
**Status**: Ready for Execution
**Total Duration**: 26 weeks (520 hours)
**Target**: Pure SPIR-V architecture + Full D3D12 features + Cross-platform OpenGL

---

## Architecture Documents Reference

This roadmap synthesizes the following design documents:
1. `docs/SPIRV_SHADER_ARCHITECTURE_DESIGN.md` - Shader compilation pipeline
2. `docs/SHARED_COMPONENT_ARCHITECTURE_DESIGN.md` - Code deduplication strategy
3. `docs/D3D12_ADVANCED_FEATURES_PLAN.md` - Modern D3D12 features
4. `MASTER_IMPLEMENTATION_ROADMAP.md` - Original strategic plan
5. `COMPREHENSIVE_AUDIT_SUMMARY.md` - Feature gap analysis

---

## Phase 0: Critical Compilation Fixes (URGENT - 2 hours)

**Goal**: Get D3D12 backend compiling and showing colored quads

### Task 0.1: Download d3dx12.h
- **Time**: 15 minutes
- **Complexity**: Low
- **Prerequisites**: None
- **Description**: Download d3dx12.h helper library from Microsoft DirectX-Headers GitHub repository
- **Steps**:
  1. Navigate to https://github.com/microsoft/DirectX-Headers
  2. Download `include/directx/d3dx12.h`
  3. Place in `src/renderer/atlas/`
  4. Add `#include "d3dx12.h"` to BackendD3D12.h
- **Validation**: File exists, compiles without errors
- **Files Modified**:
  - `src/renderer/atlas/d3dx12.h` (new)
  - `src/renderer/atlas/BackendD3D12.h`

### Task 0.2: Fix shader Output semantics
- **Time**: 10 minutes
- **Complexity**: Low
- **Prerequisites**: None
- **Description**: Add SV_Target semantics to pixel shader outputs
- **Steps**:
  1. Open `src/renderer/atlas/shader_d3d12_ps.hlsl`
  2. Find struct Output (lines 23-27)
  3. Change `float4 color;` to `float4 color : SV_Target0;`
  4. Change `float4 weights;` to `float4 weights : SV_Target1;`
- **Validation**: Shader compiles with fxc/dxc
- **Files Modified**:
  - `src/renderer/atlas/shader_d3d12_ps.hlsl`

### Task 0.3: Add GraphicsAPI::Direct3D12 enum value
- **Time**: 10 minutes
- **Complexity**: Low
- **Prerequisites**: None
- **Description**: Add enum value for D3D12 backend selection
- **Steps**:
  1. Open `src/renderer/inc/RenderSettings.hpp`
  2. Find enum class GraphicsAPI
  3. Add `Direct3D12` after `Direct3D11`
- **Validation**: Code compiles
- **Files Modified**:
  - `src/renderer/inc/RenderSettings.hpp`

### Task 0.4: Update AtlasEngine backend instantiation
- **Time**: 20 minutes
- **Complexity**: Low
- **Prerequisites**: Task 0.3
- **Description**: Wire up D3D12 backend in AtlasEngine
- **Steps**:
  1. Open `src/renderer/atlas/AtlasEngine.r.cpp`
  2. Find backend selection switch statement
  3. Add case for `GraphicsAPI::Direct3D12`
  4. Instantiate `BackendD3D12` with rendering payload
- **Validation**: Compiles, can select D3D12 in settings
- **Files Modified**:
  - `src/renderer/atlas/AtlasEngine.r.cpp`

### Task 0.5: Fix DXGI version mismatch
- **Time**: 15 minutes
- **Complexity**: Low
- **Prerequisites**: None
- **Description**: Standardize on dxgi1_6.h across all D3D12 files
- **Steps**:
  1. Open `src/renderer/atlas/pch.h`
  2. Replace `#include <dxgi1_3.h>` with `#include <dxgi1_6.h>`
  3. Search all BackendD3D12 files for dxgi includes
  4. Update to dxgi1_6.h consistently
- **Validation**: No linker errors, compiles cleanly
- **Files Modified**:
  - `src/renderer/atlas/pch.h`
  - `src/renderer/atlas/BackendD3D12.cpp`
  - `src/renderer/atlas/BackendD3D12.h`

### Task 0.6: Move static quad buffers to members
- **Time**: 30 minutes
- **Complexity**: Medium
- **Prerequisites**: None
- **Description**: Convert static vertex/index buffers to member variables
- **Steps**:
  1. Open `src/renderer/atlas/BackendD3D12.cpp` (lines 1283-1354)
  2. Find static quad vertex/index buffer definitions
  3. Move to BackendD3D12 class members
  4. Initialize in constructor
  5. Clean up in destructor
- **Validation**: Renders quad successfully
- **Files Modified**:
  - `src/renderer/atlas/BackendD3D12.h` (add members)
  - `src/renderer/atlas/BackendD3D12.cpp`

### Task 0.7: Fix input layout semantic mismatch
- **Time**: 20 minutes
- **Complexity**: Medium
- **Prerequisites**: None
- **Description**: Use POSITION semantic in input layout to match shader
- **Steps**:
  1. Open `src/renderer/atlas/BackendD3D12.cpp` (lines 462-470)
  2. Find D3D12_INPUT_ELEMENT_DESC array
  3. Change first element semantic from "SV_Position" to "POSITION"
  4. Verify matches vertex shader input
- **Validation**: Pipeline state creation succeeds
- **Files Modified**:
  - `src/renderer/atlas/BackendD3D12.cpp`

### Task 0.8: Build and test D3D12 backend
- **Time**: 20 minutes
- **Complexity**: Low
- **Prerequisites**: Tasks 0.1-0.7
- **Description**: Build with UltraPerformance configuration and validate
- **Steps**:
  1. Run `msbuild OpenConsole.sln /p:Configuration=UltraPerformance /p:Platform=x64 /m`
  2. Launch Terminal with D3D12 backend selected
  3. Verify colored quad renders
  4. Check for crashes/errors
- **Validation**: Terminal launches, shows colored background (no text yet)
- **Files Modified**: None

**Phase 0 Total**: 2 hours, 8 tasks

---

## Phase 1: Foundation Layer (Weeks 1-4, 58 hours)

**Goal**: SPIR-V toolchain + Font abstraction + Build integration

### Week 1: SPIR-V Toolchain Setup (16 hours)

#### Task 1.1: Install Vulkan SDK
- **Time**: 30 minutes
- **Complexity**: Low
- **Prerequisites**: None
- **Description**: Install Vulkan SDK 1.3.x for DXC and SPIR-V tools
- **Steps**:
  1. Download Vulkan SDK from https://vulkan.lunarg.com/
  2. Install with all components
  3. Verify DXC, spirv-opt, spirv-val in PATH
  4. Add to repository README
- **Validation**: `dxc --version` and `spirv-opt --version` work

#### Task 1.2: Build/Download SPIRV-Cross
- **Time**: 1 hour
- **Complexity**: Medium
- **Prerequisites**: None
- **Description**: Obtain SPIRV-Cross executable or build from source
- **Steps**:
  1. Clone https://github.com/KhronosGroup/SPIRV-Cross
  2. Build with CMake (or download prebuilt)
  3. Place executable in `tools/spirv-cross/`
  4. Test basic transpilation: SPIR-V → GLSL
- **Validation**: `spirv-cross --help` works

#### Task 1.3: Create shader directory structure
- **Time**: 30 minutes
- **Complexity**: Low
- **Prerequisites**: None
- **Description**: Reorganize shaders into new structure
- **Steps**:
  1. Create `src/renderer/atlas/shaders/hlsl/` hierarchy
  2. Create subdirectories: `common/`, `vertex/`, `pixel/`, `compute/`
  3. Create `src/renderer/atlas/shaders/build/` for scripts
  4. Update `.gitignore` for generated files
- **Validation**: Directory structure matches design doc

#### Task 1.4: Refactor shader_common.hlsl into modules
- **Time**: 2 hours
- **Complexity**: Medium
- **Prerequisites**: Task 1.3
- **Description**: Split monolithic shader_common.hlsl into focused files
- **Steps**:
  1. Create `types.hlsl` with data structures (VSData, PSData)
  2. Create `color.hlsl` with color utilities (premultiply, decode, etc.)
  3. Extract DirectWrite functions to `dwrite.hlsl`
  4. Create `math.hlsl` for math utilities
  5. Create `defines.hlsl` for SHADING_TYPE_* constants
- **Validation**: Each file compiles independently

#### Task 1.5: Update main_vs.hlsl with new includes
- **Time**: 1 hour
- **Complexity**: Low
- **Prerequisites**: Task 1.4
- **Description**: Update vertex shader to use modular includes
- **Steps**:
  1. Copy `src/renderer/atlas/shader_vs.hlsl` to `shaders/hlsl/vertex/main_vs.hlsl`
  2. Replace `#include "shader_common.hlsl"` with new includes
  3. Test compilation with DXC
  4. Verify output matches original
- **Validation**: Shader compiles to identical bytecode

#### Task 1.6: Update main_ps.hlsl with new includes
- **Time**: 2 hours
- **Complexity**: Medium
- **Prerequisites**: Task 1.4
- **Description**: Update pixel shader to use modular includes
- **Steps**:
  1. Copy `shader_ps.hlsl` to `shaders/hlsl/pixel/main_ps.hlsl`
  2. Update includes to use modular structure
  3. Verify all 11 shading types still work
  4. Test compilation
- **Validation**: Pixel shader compiles, visual output unchanged

#### Task 1.7: Port compute shaders to new structure
- **Time**: 2 hours
- **Complexity**: Medium
- **Prerequisites**: Task 1.4
- **Description**: Move compute shaders to new directory
- **Steps**:
  1. Copy `glyph_rasterize_cs.hlsl` to `shaders/hlsl/compute/`
  2. Copy `grid_generate_cs.hlsl` to `shaders/hlsl/compute/`
  3. Update includes
  4. Test compilation
- **Validation**: Compute shaders compile

#### Task 1.8: Create shader manifest JSON
- **Time**: 1 hour
- **Complexity**: Low
- **Prerequisites**: Tasks 1.5-1.7
- **Description**: Document all shaders for build system
- **Steps**:
  1. Create `shaders/build/manifest.json`
  2. List all shaders with entry points, types, targets
  3. Include compilation flags
- **Validation**: JSON is valid, complete

#### Task 1.9: Write PowerShell compilation script
- **Time**: 2 hours
- **Complexity**: Medium
- **Prerequisites**: Tasks 1.1-1.8
- **Description**: Automate shader compilation pipeline
- **Steps**:
  1. Create `shaders/build/compile_shaders.ps1`
  2. Implement: HLSL → SPIR-V (DXC)
  3. Implement: SPIR-V → GLSL (SPIRV-Cross)
  4. Implement: GLSL → validation
  5. Add error handling and logging
- **Validation**: Script compiles all shaders successfully

#### Task 1.10: Write Python compilation script (cross-platform)
- **Time**: 2 hours
- **Complexity**: Medium
- **Prerequisites**: Task 1.9
- **Description**: Python version for Linux/WSL support
- **Steps**:
  1. Create `shaders/build/compile_shaders.py`
  2. Implement same pipeline as PowerShell
  3. Test on WSL2 and Windows
- **Validation**: Script works on both platforms

#### Task 1.11: Test full shader pipeline manually
- **Time**: 1 hour
- **Complexity**: Low
- **Prerequisites**: Tasks 1.9-1.10
- **Description**: Validate end-to-end compilation
- **Steps**:
  1. Run PowerShell script
  2. Verify SPIR-V output in `bin/shaders/spirv/`
  3. Verify GLSL output in `bin/shaders/glsl/`
  4. Compare GLSL output quality
- **Validation**: All shaders compile, GLSL looks human-readable

#### Task 1.12: Verify SPIR-V output quality
- **Time**: 30 minutes
- **Complexity**: Low
- **Prerequisites**: Task 1.11
- **Description**: Validate SPIR-V bytecode
- **Steps**:
  1. Run `spirv-val` on all `.spv` files
  2. Check for errors/warnings
  3. Run `spirv-dis` (disassembler) for inspection
- **Validation**: All SPIR-V passes validation

**Week 1 Total**: 16 hours

### Week 2-3: Font Backend Abstraction (24 hours)

#### Task 1.13: Design IFontBackend interface
- **Time**: 2 hours
- **Complexity**: Medium
- **Prerequisites**: None
- **Description**: Create abstract font interface
- **Steps**:
  1. Create `src/renderer/atlas/shared/FontBackend.h`
  2. Define IFontBackend pure virtual interface
  3. Define FontMetrics, GlyphBitmap structs
  4. Define FontWeight, FontStyle enums
  5. Document interface contract
- **Validation**: Interface compiles, is well-documented

#### Task 1.14: Create DirectWriteFontBackend class stub
- **Time**: 1 hour
- **Complexity**: Low
- **Prerequisites**: Task 1.13
- **Description**: Create skeleton DirectWrite implementation
- **Steps**:
  1. Create `src/renderer/atlas/shared/DirectWriteFontBackend.h`
  2. Create `src/renderer/atlas/shared/DirectWriteFontBackend.cpp`
  3. Implement constructor/destructor
  4. Add member variables for DirectWrite objects
  5. Stub out all interface methods
- **Validation**: Class compiles, links

#### Task 1.15: Implement DirectWrite initialization
- **Time**: 2 hours
- **Complexity**: Medium
- **Prerequisites**: Task 1.14
- **Description**: Initialize DirectWrite factory and D2D
- **Steps**:
  1. Create IDWriteFactory5 in constructor
  2. Create ID2D1Factory
  3. Create ID2D1DCRenderTarget for glyph rasterization
  4. Add error handling
- **Validation**: Objects create successfully

#### Task 1.16: Implement loadSystemFont()
- **Time**: 3 hours
- **Complexity**: High
- **Prerequisites**: Task 1.15
- **Description**: Load system font by name
- **Steps**:
  1. Use IDWriteFontCollection to find font
  2. Match weight and style
  3. Create IDWriteFontFace3
  4. Store font metrics
  5. Handle errors (font not found, etc.)
- **Validation**: Can load "Consolas", "Cascadia Code"

#### Task 1.17: Implement rasterizeGlyph() - grayscale
- **Time**: 4 hours
- **Complexity**: High
- **Prerequisites**: Task 1.16
- **Description**: Rasterize single glyph to grayscale bitmap
- **Steps**:
  1. Create IDWriteGlyphRunAnalysis
  2. Get glyph bounds
  3. Create bitmap buffer
  4. Render with D2D1RenderTarget
  5. Extract pixel data
  6. Apply gamma correction
- **Validation**: Glyph renders correctly

#### Task 1.18: Implement rasterizeGlyph() - ClearType
- **Time**: 4 hours
- **Complexity**: Very High
- **Prerequisites**: Task 1.17
- **Description**: Rasterize with ClearType subpixel antialiasing
- **Steps**:
  1. Enable ClearType mode in D2D
  2. Render with RGB subpixel layout
  3. Extract 3-channel data
  4. Apply DirectWrite gamma correction
  5. Handle color emoji (COLR table)
- **Validation**: ClearType glyphs match D3D11 quality

#### Task 1.19: Implement getFontMetrics()
- **Time**: 1 hour
- **Complexity**: Low
- **Prerequisites**: Task 1.16
- **Description**: Return font metrics
- **Steps**:
  1. Query IDWriteFontFace for metrics
  2. Fill FontMetrics struct
  3. Convert design units to pixels
- **Validation**: Metrics match expected values

#### Task 1.20: Implement measureString()
- **Time**: 2 hours
- **Complexity**: Medium
- **Prerequisites**: Task 1.16
- **Description**: Calculate string width
- **Steps**:
  1. Create IDWriteTextLayout
  2. Set font and size
  3. Query metrics
  4. Return width
- **Validation**: String widths are accurate

#### Task 1.21: Create FreeTypeFontBackend stub
- **Time**: 1 hour
- **Complexity**: Low
- **Prerequisites**: Task 1.13
- **Description**: Skeleton for Linux support (future)
- **Steps**:
  1. Create FreeTypeFontBackend.h/cpp
  2. Stub out all methods
  3. Add TODO comments
  4. Return E_NOTIMPL for now
- **Validation**: Compiles, acknowledges future work

#### Task 1.22: Create font backend unit tests
- **Time**: 2 hours
- **Complexity**: Medium
- **Prerequisites**: Tasks 1.13-1.20
- **Description**: Test font loading and rasterization
- **Steps**:
  1. Create `src/renderer/atlas/ut_font_backend/`
  2. Test loading system fonts
  3. Test glyph rasterization
  4. Test metrics calculation
  5. Validate bitmap output
- **Validation**: All tests pass

#### Task 1.23: Integration test with D3D11 backend
- **Time**: 2 hours
- **Complexity**: Medium
- **Prerequisites**: Task 1.22
- **Description**: Verify DirectWriteFontBackend works in real renderer
- **Steps**:
  1. Temporarily integrate into BackendD3D
  2. Replace direct DirectWrite calls with IFontBackend
  3. Render text, compare visual output
  4. Validate no regressions
- **Validation**: Text renders identically

**Week 2-3 Total**: 24 hours

### Week 4: MSBuild Integration (18 hours)

#### Task 1.24: Create shaders.vcxproj
- **Time**: 2 hours
- **Complexity**: Medium
- **Prerequisites**: Week 1 complete
- **Description**: MSBuild project for shader compilation
- **Steps**:
  1. Create `src/renderer/atlas/shaders/shaders.vcxproj`
  2. Define HLSLShader item group
  3. List all shaders with metadata
  4. Define output directories
- **Validation**: Project loads in Visual Studio

#### Task 1.25: Add CustomBuild steps for shaders
- **Time**: 3 hours
- **Complexity**: High
- **Prerequisites**: Task 1.24
- **Description**: Hook shader compilation into build
- **Steps**:
  1. Add CustomBuild target
  2. Call PowerShell script
  3. Set inputs/outputs for incremental build
  4. Add to solution dependencies
- **Validation**: Shaders rebuild on change

#### Task 1.26: Configure incremental compilation
- **Time**: 2 hours
- **Complexity**: Medium
- **Prerequisites**: Task 1.25
- **Description**: Only rebuild changed shaders
- **Steps**:
  1. Implement timestamp checking
  2. Skip unchanged shaders
  3. Rebuild if script changes
  4. Test incremental build
- **Validation**: Only modified shaders recompile

#### Task 1.27: Create ShaderResources.rc
- **Time**: 2 hours
- **Complexity**: Medium
- **Prerequisites**: Task 1.26
- **Description**: Embed shaders as resources
- **Steps**:
  1. Create resource file
  2. Add all compiled shaders
  3. Define resource IDs
  4. Create ShaderResourceIDs.h
- **Validation**: Shaders embedded in binary

#### Task 1.28: Implement ShaderManager class
- **Time**: 4 hours
- **Complexity**: Medium
- **Prerequisites**: Task 1.27
- **Description**: Runtime shader loading
- **Steps**:
  1. Create `src/renderer/atlas/ShaderManager.h/cpp`
  2. Implement LoadVertexShader()
  3. Implement LoadPixelShader()
  4. Load from embedded resources
  5. Add caching
- **Validation**: Shaders load at runtime

#### Task 1.29: Add hot reload support
- **Time**: 3 hours
- **Complexity**: High
- **Prerequisites**: Task 1.28
- **Description**: Reload shaders on file change (debug only)
- **Steps**:
  1. Implement file watcher
  2. Detect shader changes
  3. Recompile and reload
  4. Recreate PSOs
  5. Add error reporting UI
- **Validation**: Edit shader, see changes live

#### Task 1.30: Test clean and incremental builds
- **Time**: 1 hour
- **Complexity**: Low
- **Prerequisites**: Tasks 1.24-1.29
- **Description**: Validate build system
- **Steps**:
  1. Clean build from scratch
  2. Incremental build (no changes)
  3. Incremental build (1 shader changed)
  4. Measure build times
- **Validation**: Incremental builds are fast (<5s)

#### Task 1.31: Document shader authoring workflow
- **Time**: 1 hour
- **Complexity**: Low
- **Prerequisites**: Tasks 1.24-1.30
- **Description**: Guide for adding new shaders
- **Steps**:
  1. Create `docs/SHADER_AUTHORING_GUIDE.md`
  2. Document workflow
  3. Provide examples
  4. Troubleshooting section
- **Validation**: Another developer can add shader

**Week 4 Total**: 18 hours

**Phase 1 Total**: 58 hours over 4 weeks

---

## Phase 2-6 Summary

Due to length constraints, see individual documents for full task breakdowns:

**Phase 2: Shared Components** (Weeks 5-13, 94 hours)
- See `docs/SHARED_COMPONENT_ARCHITECTURE_DESIGN.md` Section "Migration Strategy"
- Create GlyphAtlas<>, InstanceBatcher<>, resource abstractions
- Refactor D3D11, D3D12, implement OpenGL

**Phase 3: D3D12 Core Features** (Weeks 6-10 parallel, 80-100 hours)
- DirectWrite integration for text rendering
- Background/cursor rendering
- All 11 shading types
- See `MASTER_IMPLEMENTATION_ROADMAP.md` Phase 1-2 breakdown

**Phase 4: D3D12 Advanced Features** (Weeks 14-19, 128 hours)
- See `docs/D3D12_ADVANCED_FEATURES_PLAN.md` complete breakdown
- Enhanced Barriers, ExecuteIndirect, DirectStorage, VRS, Ring Buffers

**Phase 5: OpenGL Backend** (Weeks 15-20 parallel, 200-250 hours)
- Context creation, shader compilation
- Full feature parity with D3D11
- Linux/WSL2 support
- See `MASTER_IMPLEMENTATION_ROADMAP.md` OpenGL sections

**Phase 6: Testing & Optimization** (Weeks 21-26, 80-100 hours)
- Performance profiling
- Visual regression tests
- Cross-platform testing
- Documentation

---

## Complete Timeline Overview

```
Week 1:  [ SPIR-V Toolchain Setup           ] 16h
Week 2-3:[ Font Backend Abstraction         ] 24h
Week 4:  [ MSBuild Integration              ] 18h
Week 5:  [ Shared Components - Create       ] 16h
Week 6-7:[ D3D11 Refactor + D3D12 Core      ] 48h (parallel)
Week 8-9:[ D3D12 Refactor                   ] 40h
Week 10-12: [ OpenGL MVP                    ] 60h
Week 13: [ Shared Components - Polish       ] 8h
Week 14-15: [ D3D12 Advanced - Foundation   ] 22h
Week 16-17: [ D3D12 Advanced - GPU-Driven   ] 17h
Week 18: [ D3D12 Advanced - Optimization    ] 16h
Week 19: [ D3D12 Advanced - DirectStorage   ] 10h
Week 20: [ OpenGL Feature Complete          ] 40h
Week 21-22: [ Testing Infrastructure        ] 32h
Week 23-24: [ Performance Optimization      ] 32h
Week 25-26: [ Documentation & Polish        ] 16h
```

**Total**: 26 weeks, ~520 hours

---

## Success Criteria

### Phase 0 (Week 0)
✅ D3D12 backend compiles
✅ Renders colored quad without crash

### Phase 1 (Week 4)
✅ All shaders compile through SPIR-V pipeline
✅ Font abstraction layer complete
✅ Build system fully integrated

### Phase 2 (Week 13)
✅ Code duplication reduced by 85%
✅ All backends use shared components
✅ Performance overhead <5%

### Phase 3 (Week 10)
✅ D3D12 displays text correctly
✅ All cursor types work
✅ All shading types functional

### Phase 4 (Week 19)
✅ ExecuteIndirect reduces CPU by 40%+
✅ Frame time <6ms at 4K
✅ Memory usage <150MB

### Phase 5 (Week 20)
✅ OpenGL backend feature complete
✅ Runs on Linux and WSL2
✅ Visual parity with D3D11

### Phase 6 (Week 26)
✅ <5ms latency (90th percentile)
✅ <8% CPU usage (8-core)
✅ 100% visual regression tests pass
✅ All documentation complete

---

## Immediate Next Steps (This Week)

1. **TODAY**: Execute Phase 0 tasks (2 hours)
2. **Day 2**: Download Vulkan SDK, install SPIRV-Cross (Task 1.1-1.2)
3. **Day 3-4**: Create shader directory structure and refactor (Task 1.3-1.7)
4. **Day 5**: Write compilation scripts (Task 1.8-1.10)
5. **Week 2**: Begin font backend abstraction

---

**Document Status**: READY FOR EXECUTION
**Priority**: START WITH PHASE 0 IMMEDIATELY
