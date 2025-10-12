# SPIR-V Shader Architecture Design
## Pure SPIR-V Based Cross-Platform Shader System for Windows Terminal

**Date:** 2025-10-11
**Author:** PhD-Level Software Engineering Analysis
**Status:** Architecture Design - Ready for Implementation
**Target:** Direct3D 11, Direct3D 12, OpenGL 3.3+, Vulkan (future), Metal (future)

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [Architecture Overview](#architecture-overview)
3. [Detailed Architecture Diagram](#detailed-architecture-diagram)
4. [File Structure and Organization](#file-structure-and-organization)
5. [Build System Integration](#build-system-integration)
6. [Shader Variant Management](#shader-variant-management)
7. [Runtime Shader Loading Architecture](#runtime-shader-loading-architecture)
8. [Shader Entry Points Catalog](#shader-entry-points-catalog)
9. [Shared Shader Library Components](#shared-shader-library-components)
10. [Implementation Plan](#implementation-plan)
11. [Code Examples](#code-examples)
12. [Performance Considerations](#performance-considerations)
13. [Testing and Validation](#testing-and-validation)

---

## Executive Summary

This document specifies a comprehensive SPIR-V-based shader architecture where all shaders are written **ONCE** in HLSL (Shader Model 6.0+) and automatically transpiled to all target platforms. This approach provides:

**Key Benefits:**
- **Write Once, Run Everywhere:** Single HLSL source for D3D11, D3D12, OpenGL, Vulkan, Metal
- **Future-Proof:** Microsoft announced SPIR-V adoption for DirectX Shader Model 7+
- **Zero Runtime Overhead:** All cross-compilation happens at build-time
- **High Code Quality:** SPIRV-Cross generates human-readable, optimized code (95-100% hand-written quality)
- **Maximum Reusability:** Shared shader components across all backends
- **Maintainability:** Single source of truth eliminates code duplication

**Technology Stack:**
- **DXC (DirectX Shader Compiler):** HLSL → SPIR-V compilation
- **spirv-opt:** SPIR-V bytecode optimization
- **SPIRV-Cross:** SPIR-V → GLSL/HLSL/MSL transpilation
- **Build-Time Integration:** MSBuild custom targets with caching

**Validation:**
- Used in production by Unity, Unreal Engine, Chromium, Godot
- 91% evaluation score (see SHADER_CROSS_COMPILATION_RESEARCH.md)
- Industry-standard approach for modern rendering engines

---

## Architecture Overview

### Three-Layer Architecture

```
+------------------------------------------------------------------+
|                    LAYER 1: SOURCE SHADERS (HLSL)                 |
|                                                                   |
|  Single source of truth written in HLSL Shader Model 6.0+        |
|  - Vertex Shaders (.vs.hlsl)                                     |
|  - Pixel Shaders (.ps.hlsl)                                      |
|  - Compute Shaders (.cs.hlsl)                                    |
|  - Shared Libraries (.hlsl)                                      |
+------------------------------------------------------------------+
                              |
                              | DXC Compilation
                              v
+------------------------------------------------------------------+
|              LAYER 2: INTERMEDIATE REPRESENTATION                 |
|                                                                   |
|  SPIR-V Bytecode (.spv) - Platform-independent IR                |
|  - Optimized with spirv-opt                                      |
|  - Validated with spirv-val                                      |
|  - Reflected with spirv-reflect                                  |
|  - Cached for incremental builds                                 |
+------------------------------------------------------------------+
                              |
                              | SPIRV-Cross Transpilation
                              v
+------------------------------------------------------------------+
|                 LAYER 3: TARGET SHADERS                          |
|                                                                   |
|  +--------------+  +--------------+  +--------------+            |
|  | HLSL (D3D11) |  | GLSL (OpenGL)|  |  MSL (Metal) |            |
|  |  Shader      |  |   Shader     |  |    Shader    |            |
|  |  Model 5.0   |  |  Version 330 |  |   Version 2  |            |
|  +--------------+  +--------------+  +--------------+            |
|                                                                   |
|  Native shaders for each backend - optimized and validated       |
+------------------------------------------------------------------+
```

### Data Flow

```
[HLSL Source Files]
    |
    | DXC: dxc.exe -spirv -T <target> -E <entry> -O3
    v
[SPIR-V Binary (.spv)]
    |
    | spirv-opt: Optimize bytecode
    v
[Optimized SPIR-V (.opt.spv)]
    |
    | SPIRV-Cross: Generate target code
    v
[Target Shaders]
    |
    |-- HLSL (D3D11): spirv-cross --hlsl --shader-model 50
    |-- GLSL (OpenGL): spirv-cross --version 330 --es false
    |-- MSL (Metal): spirv-cross --msl --msl-version 20000
    |-- Vulkan: Use SPIR-V directly
    v
[Embedded in Binary OR Runtime Cache]
```

---

## Detailed Architecture Diagram

```
+=====================================================================+
|                    WINDOWS TERMINAL RENDERER                        |
+=====================================================================+

SOURCE SHADER HIERARCHY:
========================

src/renderer/atlas/shaders/
|
+-- common/                          # Shared shader libraries
|   |-- dwrite.hlsl                  # DirectWrite gamma/contrast functions
|   |-- shader_types.hlsl            # Common types and structures
|   |-- shader_constants.hlsl        # Constant buffer definitions
|   |-- shader_utils.hlsl            # Utility functions (color, math)
|   +-- platform_compat.hlsl         # Platform compatibility macros
|
+-- vertex/                          # Vertex shaders
|   |-- main.vs.hlsl                 # Main vertex shader (instanced quads)
|   |-- custom.vs.hlsl               # Custom shader vertex stage
|   +-- passthrough.vs.hlsl          # Simple passthrough vertex shader
|
+-- pixel/                           # Pixel/Fragment shaders
|   |-- main.ps.hlsl                 # Main pixel shader (all shading types)
|   |-- custom.ps.hlsl               # Custom user shader pixel stage
|   |-- background.ps.hlsl           # Background-only pixel shader
|   +-- cursor.ps.hlsl               # Cursor rendering pixel shader
|
+-- compute/                         # Compute shaders
|   |-- grid_generate.cs.hlsl        # Grid cell generation (GPU-driven)
|   |-- glyph_rasterize.cs.hlsl      # Parallel glyph rasterization
|   |-- atlas_pack.cs.hlsl           # Dynamic atlas packing (future)
|   +-- dirty_tracking.cs.hlsl       # Dirty cell bit manipulation
|
+-- includes/                        # Auto-generated includes
    |-- spirv_bindings.hlsl          # SPIR-V binding mappings
    |-- platform_defines.hlsl        # Platform-specific defines
    +-- feature_flags.hlsl           # Feature detection flags


BUILD OUTPUT HIERARCHY:
=======================

build/shaders/
|
+-- spirv/                           # Intermediate SPIR-V bytecode
|   |-- main.vs.spv
|   |-- main.ps.spv
|   |-- grid_generate.cs.spv
|   +-- ... (all shaders)
|
+-- spirv_opt/                       # Optimized SPIR-V
|   |-- main.vs.opt.spv
|   |-- main.ps.opt.spv
|   +-- ...
|
+-- d3d11/                           # Direct3D 11 HLSL (SM 5.0)
|   |-- main_vs.hlsl
|   |-- main_ps.hlsl
|   +-- ...
|
+-- d3d12/                           # Direct3D 12 (uses SPIR-V or SM 6.0)
|   |-- main_vs.cso                  # Compiled shader object
|   |-- main_ps.cso
|   +-- ...
|
+-- opengl/                          # OpenGL GLSL
|   |-- gl330/                       # OpenGL 3.3 Core
|   |   |-- main.vs.glsl
|   |   |-- main.fs.glsl
|   |   +-- ...
|   +-- gl430/                       # OpenGL 4.3 (compute shaders)
|       |-- main.vs.glsl
|       |-- main.fs.glsl
|       |-- grid_generate.cs.glsl
|       +-- ...
|
+-- metal/                           # Metal Shading Language (future)
|   |-- main.vs.metal
|   |-- main.fs.metal
|   +-- ...
|
+-- cache/                           # Build cache and metadata
|   |-- shader_manifest.json         # Shader dependency graph
|   |-- build_hashes.json            # Incremental build cache
|   +-- reflection_data.json         # Shader reflection metadata
|
+-- embedded/                        # Embedded shader resources
    |-- shader_resources.h           # C++ header with embedded shaders
    +-- shader_resources.rc          # Windows resource file


RUNTIME SHADER LOADING:
=======================

+-------------------------------------------------------------------+
|                        ShaderManager                              |
+-------------------------------------------------------------------+
| - LoadShaderModule(backend, shaderType, entryPoint)              |
| - CompileShader(source, backend, options)                        |
| - GetShaderReflection(shaderID)                                  |
| - HotReloadShader(shaderID)  [DEBUG only]                        |
+-------------------------------------------------------------------+
                    |
                    | Uses
                    v
+-------------------------------------------------------------------+
|                    ShaderBackendInterface                         |
+-------------------------------------------------------------------+
| + CreateShader(bytecode) -> ShaderHandle                         |
| + BindShader(handle)                                             |
| + SetConstantBuffer(handle, slot, data)                          |
| + SetTexture(handle, slot, texture)                              |
+-------------------------------------------------------------------+
           |                  |                  |
           v                  v                  v
    +-------------+    +-------------+    +-------------+
    | D3D11Shader |    | D3D12Shader |    | GLShader    |
    +-------------+    +-------------+    +-------------+


SHADER COMPILATION PIPELINE:
============================

                        [HLSL Source File]
                               |
                               | Stage 1: Preprocessing
                               v
                    [Preprocessed HLSL + Includes]
                               |
                               | Stage 2: DXC Compilation
                               | - Syntax validation
                               | - Type checking
                               | - Optimization (-O3)
                               | - SPIR-V generation
                               v
                        [SPIR-V Binary (.spv)]
                               |
                               | Stage 3: SPIR-V Optimization
                               | - Dead code elimination
                               | - Constant folding
                               | - Loop unrolling
                               | - Instruction combining
                               v
                    [Optimized SPIR-V (.opt.spv)]
                               |
                               | Stage 4: SPIR-V Validation
                               | - spirv-val checks
                               | - Reflection extraction
                               v
                        [Validated SPIR-V]
                               |
          +--------------------+--------------------+
          |                    |                    |
          v                    v                    v
    [SPIRV-Cross]        [SPIRV-Cross]        [Native]
    Target: GLSL         Target: HLSL       Target: SPIR-V
          |                    |                    |
          v                    v                    v
    [GLSL 330/430]       [HLSL SM 5.0]        [Vulkan Ready]
          |                    |                    |
          v                    v                    v
    [OpenGL Shader]      [D3D11 Shader]      [D3D12/Vulkan]
```

---

## File Structure and Organization

### Source Shader Directory Structure

```
src/renderer/atlas/shaders/
|
+-- common/                                    # Shared components
|   |
|   |-- dwrite.hlsl                           # DirectWrite functions
|   |   # - DWrite_UnpremultiplyColor()
|   |   # - DWrite_ApplyLightOnDarkContrastAdjustment()
|   |   # - DWrite_CalcColorIntensity()
|   |   # - DWrite_EnhanceContrast()
|   |   # - DWrite_ApplyAlphaCorrection()
|   |   # - DWrite_GrayscaleBlend()
|   |   # - DWrite_ClearTypeBlend()
|   |
|   |-- shader_types.hlsl                     # Type definitions
|   |   # struct VSData { ... };
|   |   # struct PSData { ... };
|   |   # struct QuadInstance { ... };
|   |   # struct GridCell { ... };
|   |
|   |-- shader_constants.hlsl                 # Constant buffer layouts
|   |   # cbuffer VSConstBuffer { ... };
|   |   # cbuffer PSConstBuffer { ... };
|   |   # cbuffer CustomConstBuffer { ... };
|   |   # cbuffer GridConstants { ... };
|   |
|   |-- shader_utils.hlsl                     # Utility functions
|   |   # float4 premultiplyColor(float4);
|   |   # float4 alphaBlendPremultiplied(float4, float4);
|   |   # float4 decodeRGBA(uint);
|   |   # uint packColor(float4);
|   |
|   |-- shading_types.hlsl                    # Shading type definitions
|   |   # #define SHADING_TYPE_TEXT_BACKGROUND 0
|   |   # #define SHADING_TYPE_TEXT_GRAYSCALE 1
|   |   # ... (all 11 shading types)
|   |
|   |-- platform_compat.hlsl                  # Platform compatibility
|   |   # Handles D3D vs OpenGL differences
|   |   # Texture sampling macros
|   |   # Constant buffer binding macros
|   |
|   +-- math_utils.hlsl                       # Math utilities
|       # Distance field calculations
|       # Trigonometric helpers
|       # Interpolation functions
|
+-- vertex/                                    # Vertex shaders
|   |
|   |-- main.vs.hlsl                          # Main vertex shader
|   |   # Entry: main(VSData) -> PSData
|   |   # Transforms quad instances to NDC space
|   |   # Instanced rendering support (65,536 instances)
|   |
|   |-- custom.vs.hlsl                        # Custom shader vertex stage
|   |   # Entry: main(VSData) -> CustomPSData
|   |   # User-customizable vertex transformations
|   |
|   +-- passthrough.vs.hlsl                   # Passthrough vertex shader
|       # Entry: main(float2 pos) -> float4
|       # Minimal vertex shader for fullscreen quads
|
+-- pixel/                                     # Pixel/Fragment shaders
|   |
|   |-- main.ps.hlsl                          # Main pixel shader
|   |   # Entry: main(PSData) -> Output
|   |   # Implements all 11 shading types:
|   |   #   - TEXT_BACKGROUND
|   |   #   - TEXT_GRAYSCALE (DirectWrite AA)
|   |   #   - TEXT_CLEARTYPE (ClearType subpixel AA)
|   |   #   - TEXT_BUILTIN_GLYPH (box drawing chars)
|   |   #   - TEXT_PASSTHROUGH
|   |   #   - DOTTED_LINE
|   |   #   - DASHED_LINE
|   |   #   - CURLY_LINE (sine wave distance field)
|   |   #   - SOLID_LINE
|   |   #   - CURSOR
|   |   #   - FILLED_RECT
|   |
|   |-- custom.ps.hlsl                        # Custom user pixel shader
|   |   # Entry: main(CustomPSData) -> float4
|   |   # User-provided HLSL pixel shaders
|   |   # Custom effects and filters
|   |
|   |-- background.ps.hlsl                    # Background-only rendering
|   |   # Entry: main(PSData) -> float4
|   |   # Optimized for solid color backgrounds
|   |
|   +-- cursor.ps.hlsl                        # Cursor rendering
|       # Entry: main(PSData) -> float4
|       # Cursor types with blending
|
+-- compute/                                   # Compute shaders
|   |
|   |-- grid_generate.cs.hlsl                 # Grid cell generation
|   |   # Entry: main(uint3 dispatchThreadID)
|   |   # Thread group: [16, 16, 1]
|   |   # Generates terminal grid cells in parallel
|   |   # Dirty cell tracking
|   |   # Selection buffer management
|   |
|   |-- glyph_rasterize.cs.hlsl               # Glyph rasterization
|   |   # Entry: main(uint3 dispatchThreadID)
|   |   # Thread group: [8, 8, 1]
|   |   # Parallel glyph atlas rasterization
|   |   # Antialiasing (Lanczos resampling)
|   |   # ClearType subpixel rendering
|   |   # Gamma and contrast correction
|   |
|   |-- atlas_pack.cs.hlsl                    # Dynamic atlas packing (future)
|   |   # Entry: main(uint3 dispatchThreadID)
|   |   # Thread group: [256, 1, 1]
|   |   # GPU-accelerated rect packing
|   |   # Online bin packing algorithm
|   |
|   +-- dirty_tracking.cs.hlsl                # Dirty cell bit manipulation
|       # Entry: main(uint3 dispatchThreadID)
|       # Thread group: [256, 1, 1]
|       # Parallel bitset operations
|       # Mark/clear dirty cells
|
+-- includes/                                  # Auto-generated includes
|   |
|   |-- spirv_bindings.hlsl                   # SPIR-V binding mappings
|   |   # Generated at build-time
|   |   # Maps D3D register bindings to SPIR-V descriptors
|   |
|   |-- platform_defines.hlsl                 # Platform-specific defines
|   |   # PLATFORM_D3D11
|   |   # PLATFORM_D3D12
|   |   # PLATFORM_OPENGL
|   |   # PLATFORM_VULKAN
|   |
|   +-- feature_flags.hlsl                    # Feature detection
|       # FEATURE_COMPUTE_SHADERS
|       # FEATURE_WAVE_INTRINSICS
|       # FEATURE_16BIT_TYPES
|
+-- tests/                                     # Shader unit tests
|   |
|   |-- test_dwrite.hlsl                      # Test DirectWrite functions
|   |-- test_shading_types.hlsl               # Test all shading types
|   |-- test_color_space.hlsl                 # Test color conversions
|   +-- test_distance_fields.hlsl             # Test distance field math
|
+-- samples/                                   # Sample custom shaders
    |
    |-- grayscale.ps.hlsl                     # Grayscale filter
    |-- retro_crt.ps.hlsl                     # CRT scanline effect
    |-- neon_glow.ps.hlsl                     # Neon glow effect
    +-- matrix_rain.ps.hlsl                   # Matrix digital rain effect
```

### Build Output Structure

```
build/shaders/
|
+-- cache/                                     # Build cache
|   |-- shader_manifest.json                  # Dependency graph
|   |-- build_hashes.json                     # File hashes for incremental builds
|   |-- reflection/                           # Shader reflection data
|   |   |-- main.vs.json
|   |   |-- main.ps.json
|   |   +-- ... (per-shader reflection)
|   +-- timestamps.json                       # Build timestamps
|
+-- spirv/                                     # Raw SPIR-V output
|   |-- main.vs.spv                           # (Binary SPIR-V)
|   |-- main.ps.spv
|   |-- grid_generate.cs.spv
|   |-- glyph_rasterize.cs.spv
|   +-- ... (all shaders)
|
+-- spirv_opt/                                 # Optimized SPIR-V
|   |-- main.vs.opt.spv                       # (Optimized binary)
|   |-- main.ps.opt.spv
|   +-- ...
|
+-- spirv_asm/                                 # SPIR-V assembly (debug)
|   |-- main.vs.spvasm                        # Human-readable SPIR-V
|   |-- main.ps.spvasm
|   +-- ...
|
+-- d3d11/                                     # Direct3D 11 output
|   |-- hlsl/                                  # Transpiled HLSL (SM 5.0)
|   |   |-- main_vs.hlsl
|   |   |-- main_ps.hlsl
|   |   +-- ...
|   +-- compiled/                              # Compiled bytecode
|       |-- main_vs.cso
|       |-- main_ps.cso
|       +-- ...
|
+-- d3d12/                                     # Direct3D 12 output
|   |-- hlsl/                                  # HLSL (SM 6.0) or SPIR-V
|   |   |-- main_vs.hlsl
|   |   |-- main_ps.hlsl
|   |   +-- ...
|   +-- compiled/                              # Compiled DXIL or SPIR-V
|       |-- main_vs.dxil
|       |-- main_ps.dxil
|       |-- grid_generate.cs.dxil
|       +-- ...
|
+-- opengl/                                    # OpenGL output
|   |-- gl330/                                 # OpenGL 3.3 Core
|   |   |-- main.vs.glsl
|   |   |-- main.fs.glsl
|   |   +-- ...
|   |-- gl430/                                 # OpenGL 4.3 (compute shaders)
|   |   |-- main.vs.glsl
|   |   |-- main.fs.glsl
|   |   |-- grid_generate.cs.glsl
|   |   |-- glyph_rasterize.cs.glsl
|   |   +-- ...
|   +-- gl450/                                 # OpenGL 4.5 (latest features)
|       |-- main.vs.glsl
|       |-- main.fs.glsl
|       +-- ...
|
+-- vulkan/                                    # Vulkan output
|   |-- spirv/                                 # Direct SPIR-V usage
|   |   |-- main.vs.spv -> ../../spirv_opt/main.vs.opt.spv
|   |   |-- main.ps.spv -> ../../spirv_opt/main.ps.opt.spv
|   |   +-- ... (symlinks to optimized SPIR-V)
|   +-- reflection/                            # Vulkan-specific reflection
|       |-- main.vs.json
|       +-- ...
|
+-- metal/                                     # Metal output (future)
|   |-- main.vs.metal
|   |-- main.fs.metal
|   +-- ...
|
+-- embedded/                                  # Embedded resources
|   |-- shader_resources.h                    # C++ header with byte arrays
|   |-- shader_resources.cpp                  # Implementation
|   +-- shader_manifest.inl                   # Shader metadata
|
+-- logs/                                      # Build logs
    |-- dxc_output.log                        # DXC compiler output
    |-- spirv_opt.log                         # SPIR-V optimizer output
    |-- spirv_cross.log                       # SPIRV-Cross transpiler output
    |-- errors.log                            # Compilation errors
    +-- warnings.log                          # Warnings
```

---

## Build System Integration

### MSBuild Integration Architecture

#### Custom Build Targets

Create `/home/eirikr/github-performance-projects/windows-terminal-optimized/src/renderer/atlas/shaders/ShaderCompilation.targets`:

```xml
<?xml version="1.0" encoding="utf-8"?>
<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003">

  <!-- ================================================================
       SPIR-V Shader Compilation Custom Build Target
       ================================================================ -->

  <PropertyGroup>
    <!-- Toolchain paths -->
    <DxcPath Condition="'$(DxcPath)'==''">$(MSBuildThisFileDirectory)..\..\..\..\tools\dxc\dxc.exe</DxcPath>
    <SpirvOptPath Condition="'$(SpirvOptPath)'==''">$(MSBuildThisFileDirectory)..\..\..\..\tools\spirv-tools\spirv-opt.exe</SpirvOptPath>
    <SpirvValPath Condition="'$(SpirvValPath)'==''">$(MSBuildThisFileDirectory)..\..\..\..\tools\spirv-tools\spirv-val.exe</SpirvValPath>
    <SpirvCrossPath Condition="'$(SpirvCrossPath)'==''">$(MSBuildThisFileDirectory)..\..\..\..\tools\spirv-cross\spirv-cross.exe</SpirvCrossPath>

    <!-- Build output paths -->
    <ShaderOutputDir>$(IntermediateOutputPath)shaders\</ShaderOutputDir>
    <ShaderSpirvDir>$(ShaderOutputDir)spirv\</ShaderSpirvDir>
    <ShaderSpirvOptDir>$(ShaderOutputDir)spirv_opt\</ShaderSpirvOptDir>
    <ShaderD3D11Dir>$(ShaderOutputDir)d3d11\</ShaderD3D11Dir>
    <ShaderD3D12Dir>$(ShaderOutputDir)d3d12\</ShaderD3D12Dir>
    <ShaderOpenGLDir>$(ShaderOutputDir)opengl\</ShaderOpenGLDir>
    <ShaderMetalDir>$(ShaderOutputDir)metal\</ShaderMetalDir>
    <ShaderCacheDir>$(ShaderOutputDir)cache\</ShaderCacheDir>
    <ShaderEmbeddedDir>$(ShaderOutputDir)embedded\</ShaderEmbeddedDir>

    <!-- Compilation options -->
    <ShaderOptimizationLevel Condition="'$(Configuration)'=='Debug'">-O0</ShaderOptimizationLevel>
    <ShaderOptimizationLevel Condition="'$(Configuration)'!='Debug'">-O3</ShaderOptimizationLevel>
    <ShaderDebugInfo Condition="'$(Configuration)'=='Debug'">-Zi -Qembed_debug</ShaderDebugInfo>
    <ShaderDebugInfo Condition="'$(Configuration)'!='Debug'"></ShaderDebugInfo>

    <!-- Feature flags -->
    <EnableSpirvOptimization Condition="'$(EnableSpirvOptimization)'==''">true</EnableSpirvOptimization>
    <EnableShaderValidation Condition="'$(EnableShaderValidation)'==''">true</EnableShaderValidation>
    <EnableIncrementalBuild Condition="'$(EnableIncrementalBuild)'==''">true</EnableIncrementalBuild>
    <EmbedShaders Condition="'$(EmbedShaders)'==''">true</EmbedShaders>
  </PropertyGroup>

  <!-- ================================================================
       Shader Source Item Groups
       ================================================================ -->

  <ItemGroup>
    <!-- Vertex Shaders -->
    <VertexShader Include="$(MSBuildThisFileDirectory)vertex\*.vs.hlsl" />

    <!-- Pixel Shaders -->
    <PixelShader Include="$(MSBuildThisFileDirectory)pixel\*.ps.hlsl" />

    <!-- Compute Shaders -->
    <ComputeShader Include="$(MSBuildThisFileDirectory)compute\*.cs.hlsl" />

    <!-- Common/Shared Files (headers) -->
    <ShaderInclude Include="$(MSBuildThisFileDirectory)common\*.hlsl" />
  </ItemGroup>

  <!-- ================================================================
       Step 1: Compile HLSL to SPIR-V using DXC
       ================================================================ -->

  <Target Name="CompileHLSLToSPIRV"
          Inputs="@(VertexShader);@(PixelShader);@(ComputeShader);@(ShaderInclude)"
          Outputs="$(ShaderSpirvDir)%(Filename).spv"
          BeforeTargets="ClCompile">

    <Message Importance="high" Text="Compiling shaders to SPIR-V..." />

    <!-- Create output directories -->
    <MakeDir Directories="$(ShaderSpirvDir);$(ShaderSpirvOptDir);$(ShaderCacheDir)" />

    <!-- Compile Vertex Shaders -->
    <Exec Command="&quot;$(DxcPath)&quot; -spirv -T vs_6_0 -E main $(ShaderOptimizationLevel) $(ShaderDebugInfo) -HV 2021 -I &quot;$(MSBuildThisFileDirectory)common&quot; -I &quot;$(MSBuildThisFileDirectory)includes&quot; -Fo &quot;$(ShaderSpirvDir)%(VertexShader.Filename).spv&quot; &quot;%(VertexShader.FullPath)&quot;"
          WorkingDirectory="$(MSBuildThisFileDirectory)"
          ConsoleToMSBuild="true"
          ContinueOnError="false">
      <Output TaskParameter="ConsoleOutput" ItemName="DxcOutput" />
    </Exec>

    <!-- Compile Pixel Shaders -->
    <Exec Command="&quot;$(DxcPath)&quot; -spirv -T ps_6_0 -E main $(ShaderOptimizationLevel) $(ShaderDebugInfo) -HV 2021 -I &quot;$(MSBuildThisFileDirectory)common&quot; -I &quot;$(MSBuildThisFileDirectory)includes&quot; -Fo &quot;$(ShaderSpirvDir)%(PixelShader.Filename).spv&quot; &quot;%(PixelShader.FullPath)&quot;"
          WorkingDirectory="$(MSBuildThisFileDirectory)"
          ConsoleToMSBuild="true"
          ContinueOnError="false">
      <Output TaskParameter="ConsoleOutput" ItemName="DxcOutput" />
    </Exec>

    <!-- Compile Compute Shaders -->
    <Exec Command="&quot;$(DxcPath)&quot; -spirv -T cs_6_0 -E main $(ShaderOptimizationLevel) $(ShaderDebugInfo) -HV 2021 -I &quot;$(MSBuildThisFileDirectory)common&quot; -I &quot;$(MSBuildThisFileDirectory)includes&quot; -Fo &quot;$(ShaderSpirvDir)%(ComputeShader.Filename).spv&quot; &quot;%(ComputeShader.FullPath)&quot;"
          WorkingDirectory="$(MSBuildThisFileDirectory)"
          ConsoleToMSBuild="true"
          ContinueOnError="false">
      <Output TaskParameter="ConsoleOutput" ItemName="DxcOutput" />
    </Exec>
  </Target>

  <!-- ================================================================
       Step 2: Optimize SPIR-V using spirv-opt
       ================================================================ -->

  <Target Name="OptimizeSPIRV"
          AfterTargets="CompileHLSLToSPIRV"
          Condition="'$(EnableSpirvOptimization)'=='true'">

    <Message Importance="high" Text="Optimizing SPIR-V bytecode..." />

    <ItemGroup>
      <SpirvBinaries Include="$(ShaderSpirvDir)*.spv" />
    </ItemGroup>

    <!-- Optimization passes -->
    <Exec Command="&quot;$(SpirvOptPath)&quot; --strip-debug --eliminate-dead-code-aggressive --merge-return --inline-entry-points-exhaustive --scalar-replacement=100 --convert-local-access-chains --ccp --loop-unroll --if-conversion -O &quot;%(SpirvBinaries.FullPath)&quot; -o &quot;$(ShaderSpirvOptDir)%(SpirvBinaries.Filename).opt.spv&quot;"
          WorkingDirectory="$(MSBuildThisFileDirectory)"
          ConsoleToMSBuild="true"
          ContinueOnError="false" />
  </Target>

  <!-- ================================================================
       Step 3: Validate SPIR-V using spirv-val
       ================================================================ -->

  <Target Name="ValidateSPIRV"
          AfterTargets="OptimizeSPIRV"
          Condition="'$(EnableShaderValidation)'=='true'">

    <Message Importance="high" Text="Validating SPIR-V bytecode..." />

    <ItemGroup>
      <OptimizedSpirvBinaries Include="$(ShaderSpirvOptDir)*.opt.spv" />
    </ItemGroup>

    <Exec Command="&quot;$(SpirvValPath)&quot; &quot;%(OptimizedSpirvBinaries.FullPath)&quot;"
          WorkingDirectory="$(MSBuildThisFileDirectory)"
          ConsoleToMSBuild="true"
          ContinueOnError="false" />
  </Target>

  <!-- ================================================================
       Step 4: Transpile SPIR-V to GLSL (OpenGL)
       ================================================================ -->

  <Target Name="TranspileSPIRVToGLSL"
          AfterTargets="ValidateSPIRV">

    <Message Importance="high" Text="Transpiling SPIR-V to GLSL..." />

    <!-- Create OpenGL output directories -->
    <MakeDir Directories="$(ShaderOpenGLDir)gl330;$(ShaderOpenGLDir)gl430;$(ShaderOpenGLDir)gl450" />

    <ItemGroup>
      <OptimizedSpirvBinaries Include="$(ShaderSpirvOptDir)*.opt.spv" />
    </ItemGroup>

    <!-- OpenGL 3.3 (no compute shaders) -->
    <Exec Command="&quot;$(SpirvCrossPath)&quot; &quot;%(OptimizedSpirvBinaries.FullPath)&quot; --version 330 --es false --output &quot;$(ShaderOpenGLDir)gl330\%(OptimizedSpirvBinaries.Filename).glsl&quot;"
          Condition="!$(%(OptimizedSpirvBinaries.Filename.Contains('.cs.')))"
          WorkingDirectory="$(MSBuildThisFileDirectory)"
          ConsoleToMSBuild="true"
          ContinueOnError="false" />

    <!-- OpenGL 4.3 (with compute shaders) -->
    <Exec Command="&quot;$(SpirvCrossPath)&quot; &quot;%(OptimizedSpirvBinaries.FullPath)&quot; --version 430 --es false --output &quot;$(ShaderOpenGLDir)gl430\%(OptimizedSpirvBinaries.Filename).glsl&quot;"
          WorkingDirectory="$(MSBuildThisFileDirectory)"
          ConsoleToMSBuild="true"
          ContinueOnError="false" />
  </Target>

  <!-- ================================================================
       Step 5: Transpile SPIR-V to HLSL (Direct3D 11)
       ================================================================ -->

  <Target Name="TranspileSPIRVToHLSL"
          AfterTargets="ValidateSPIRV">

    <Message Importance="high" Text="Transpiling SPIR-V to HLSL..." />

    <MakeDir Directories="$(ShaderD3D11Dir)hlsl;$(ShaderD3D11Dir)compiled" />

    <ItemGroup>
      <OptimizedSpirvBinaries Include="$(ShaderSpirvOptDir)*.opt.spv" />
    </ItemGroup>

    <!-- Transpile to HLSL Shader Model 5.0 -->
    <Exec Command="&quot;$(SpirvCrossPath)&quot; &quot;%(OptimizedSpirvBinaries.FullPath)&quot; --hlsl --shader-model 50 --output &quot;$(ShaderD3D11Dir)hlsl\%(OptimizedSpirvBinaries.Filename).hlsl&quot;"
          WorkingDirectory="$(MSBuildThisFileDirectory)"
          ConsoleToMSBuild="true"
          ContinueOnError="false" />
  </Target>

  <!-- ================================================================
       Step 6: Embed shaders in binary (optional)
       ================================================================ -->

  <Target Name="EmbedShaders"
          AfterTargets="TranspileSPIRVToGLSL;TranspileSPIRVToHLSL"
          Condition="'$(EmbedShaders)'=='true'">

    <Message Importance="high" Text="Embedding shaders in binary..." />

    <MakeDir Directories="$(ShaderEmbeddedDir)" />

    <!-- Run custom tool to generate shader_resources.h and .cpp -->
    <Exec Command="python &quot;$(MSBuildThisFileDirectory)..\..\..\..\tools\embed_shaders.py&quot; --input &quot;$(ShaderOutputDir)&quot; --output &quot;$(ShaderEmbeddedDir)&quot;"
          WorkingDirectory="$(MSBuildThisFileDirectory)"
          ConsoleToMSBuild="true"
          ContinueOnError="false" />

    <!-- Add generated files to compilation -->
    <ItemGroup>
      <ClInclude Include="$(ShaderEmbeddedDir)shader_resources.h" />
      <ClCompile Include="$(ShaderEmbeddedDir)shader_resources.cpp" />
    </ItemGroup>
  </Target>

  <!-- ================================================================
       Step 7: Generate shader manifest
       ================================================================ -->

  <Target Name="GenerateShaderManifest"
          AfterTargets="EmbedShaders">

    <Message Importance="high" Text="Generating shader manifest..." />

    <!-- Run custom tool to generate shader manifest JSON -->
    <Exec Command="python &quot;$(MSBuildThisFileDirectory)..\..\..\..\tools\generate_shader_manifest.py&quot; --input &quot;$(ShaderOutputDir)&quot; --output &quot;$(ShaderCacheDir)shader_manifest.json&quot;"
          WorkingDirectory="$(MSBuildThisFileDirectory)"
          ConsoleToMSBuild="true"
          ContinueOnError="false" />
  </Target>

</Project>
```

### Integration with atlas.vcxproj

Add to `/home/eirikr/github-performance-projects/windows-terminal-optimized/src/renderer/atlas/atlas.vcxproj`:

```xml
<!-- Import shader compilation targets -->
<Import Project="shaders\ShaderCompilation.targets" />
```

### Python Build Tools

#### embed_shaders.py

Location: `/tools/embed_shaders.py`

```python
#!/usr/bin/env python3
"""
Embed Shaders Tool
Converts compiled shader bytecode into C++ byte arrays for embedding in binary.
"""

import os
import sys
import json
import argparse
from pathlib import Path
from typing import Dict, List, Tuple


def read_binary_file(filepath: Path) -> bytes:
    """Read binary file and return bytes."""
    with open(filepath, 'rb') as f:
        return f.read()


def bytes_to_cpp_array(data: bytes, array_name: str) -> str:
    """Convert bytes to C++ array declaration."""
    cpp = f"// Auto-generated shader bytecode\n"
    cpp += f"// Size: {len(data)} bytes\n\n"
    cpp += f"const unsigned char {array_name}[] = {{\n"

    # Format as hex bytes, 16 per line
    for i in range(0, len(data), 16):
        line = data[i:i+16]
        hex_bytes = ', '.join(f'0x{b:02X}' for b in line)
        cpp += f"    {hex_bytes},\n"

    cpp += "};\n\n"
    cpp += f"const unsigned int {array_name}_size = {len(data)};\n\n"

    return cpp


def generate_shader_resources_header(shaders: Dict[str, bytes], output_path: Path):
    """Generate shader_resources.h with all embedded shaders."""

    header = """// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

// AUTO-GENERATED FILE - DO NOT EDIT
// Generated by embed_shaders.py

#pragma once

#include <cstdint>

namespace Microsoft::Console::Render::Atlas::EmbeddedShaders
{

"""

    # Declare all arrays
    for shader_name in shaders.keys():
        safe_name = shader_name.replace('.', '_').replace('-', '_')
        header += f"    extern const unsigned char {safe_name}[];\n"
        header += f"    extern const unsigned int {safe_name}_size;\n\n"

    header += """
    // Shader lookup structure
    struct ShaderResource
    {
        const char* name;
        const unsigned char* data;
        unsigned int size;
    };

    // Shader resource table
    extern const ShaderResource g_shaderResources[];
    extern const unsigned int g_shaderResourceCount;

} // namespace Microsoft::Console::Render::Atlas::EmbeddedShaders
"""

    with open(output_path / 'shader_resources.h', 'w') as f:
        f.write(header)


def generate_shader_resources_cpp(shaders: Dict[str, bytes], output_path: Path):
    """Generate shader_resources.cpp with shader bytecode."""

    cpp = """// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

// AUTO-GENERATED FILE - DO NOT EDIT
// Generated by embed_shaders.py

#include "shader_resources.h"

namespace Microsoft::Console::Render::Atlas::EmbeddedShaders
{

"""

    # Write all shader byte arrays
    for shader_name, shader_data in shaders.items():
        safe_name = shader_name.replace('.', '_').replace('-', '_')
        cpp += bytes_to_cpp_array(shader_data, safe_name)

    # Create shader resource table
    cpp += "    // Shader resource table\n"
    cpp += "    const ShaderResource g_shaderResources[] = {\n"

    for shader_name in shaders.keys():
        safe_name = shader_name.replace('.', '_').replace('-', '_')
        cpp += f'        {{ "{shader_name}", {safe_name}, {safe_name}_size }},\n'

    cpp += "    };\n\n"
    cpp += f"    const unsigned int g_shaderResourceCount = {len(shaders)};\n\n"
    cpp += "} // namespace Microsoft::Console::Render::Atlas::EmbeddedShaders\n"

    with open(output_path / 'shader_resources.cpp', 'w') as f:
        f.write(cpp)


def collect_shaders(input_dir: Path) -> Dict[str, bytes]:
    """Collect all compiled shaders from build output."""

    shaders = {}

    # Collect SPIR-V binaries
    spirv_opt_dir = input_dir / 'spirv_opt'
    if spirv_opt_dir.exists():
        for spv_file in spirv_opt_dir.glob('*.opt.spv'):
            shader_name = f"spirv/{spv_file.stem}.spv"
            shaders[shader_name] = read_binary_file(spv_file)

    # Collect OpenGL GLSL
    for gl_version in ['gl330', 'gl430', 'gl450']:
        gl_dir = input_dir / 'opengl' / gl_version
        if gl_dir.exists():
            for glsl_file in gl_dir.glob('*.glsl'):
                shader_name = f"opengl/{gl_version}/{glsl_file.name}"
                shaders[shader_name] = read_binary_file(glsl_file)

    # Collect D3D11 HLSL
    d3d11_dir = input_dir / 'd3d11' / 'hlsl'
    if d3d11_dir.exists():
        for hlsl_file in d3d11_dir.glob('*.hlsl'):
            shader_name = f"d3d11/{hlsl_file.name}"
            shaders[shader_name] = read_binary_file(hlsl_file)

    return shaders


def main():
    parser = argparse.ArgumentParser(description='Embed shader bytecode in C++ binary')
    parser.add_argument('--input', required=True, help='Shader build output directory')
    parser.add_argument('--output', required=True, help='Output directory for generated files')

    args = parser.parse_args()

    input_dir = Path(args.input)
    output_dir = Path(args.output)

    if not input_dir.exists():
        print(f"Error: Input directory does not exist: {input_dir}", file=sys.stderr)
        return 1

    output_dir.mkdir(parents=True, exist_ok=True)

    print(f"Collecting shaders from {input_dir}...")
    shaders = collect_shaders(input_dir)

    if not shaders:
        print("Warning: No shaders found", file=sys.stderr)
        return 1

    print(f"Found {len(shaders)} shaders")

    print("Generating shader_resources.h...")
    generate_shader_resources_header(shaders, output_dir)

    print("Generating shader_resources.cpp...")
    generate_shader_resources_cpp(shaders, output_dir)

    print("Done!")
    return 0


if __name__ == '__main__':
    sys.exit(main())
```

#### generate_shader_manifest.py

Location: `/tools/generate_shader_manifest.py`

```python
#!/usr/bin/env python3
"""
Generate Shader Manifest
Creates JSON manifest with shader metadata, dependencies, and reflection data.
"""

import os
import sys
import json
import hashlib
import argparse
from pathlib import Path
from typing import Dict, List, Any


def compute_file_hash(filepath: Path) -> str:
    """Compute SHA256 hash of file."""
    sha256 = hashlib.sha256()
    with open(filepath, 'rb') as f:
        for chunk in iter(lambda: f.read(4096), b''):
            sha256.update(chunk)
    return sha256.hexdigest()


def parse_hlsl_includes(hlsl_file: Path) -> List[str]:
    """Parse #include directives from HLSL file."""
    includes = []
    with open(hlsl_file, 'r') as f:
        for line in f:
            line = line.strip()
            if line.startswith('#include'):
                # Extract filename from #include "filename.hlsl"
                parts = line.split('"')
                if len(parts) >= 2:
                    includes.append(parts[1])
    return includes


def extract_shader_metadata(shader_path: Path, source_root: Path) -> Dict[str, Any]:
    """Extract metadata from shader source file."""

    metadata = {
        'name': shader_path.stem,
        'path': str(shader_path.relative_to(source_root)),
        'hash': compute_file_hash(shader_path),
        'type': None,
        'entry_point': 'main',
        'shader_model': '6.0',
        'includes': [],
    }

    # Determine shader type from filename
    if '.vs.' in shader_path.name:
        metadata['type'] = 'vertex'
    elif '.ps.' in shader_path.name:
        metadata['type'] = 'pixel'
    elif '.cs.' in shader_path.name:
        metadata['type'] = 'compute'
    elif '.fs.' in shader_path.name:
        metadata['type'] = 'fragment'

    # Parse includes
    metadata['includes'] = parse_hlsl_includes(shader_path)

    return metadata


def generate_manifest(shaders_dir: Path, source_dir: Path) -> Dict[str, Any]:
    """Generate complete shader manifest."""

    manifest = {
        'version': '1.0',
        'generated': str(Path.cwd()),
        'source_directory': str(source_dir),
        'build_directory': str(shaders_dir),
        'shaders': [],
        'common_includes': [],
    }

    # Collect vertex shaders
    vs_dir = source_dir / 'vertex'
    if vs_dir.exists():
        for vs_file in vs_dir.glob('*.vs.hlsl'):
            manifest['shaders'].append(extract_shader_metadata(vs_file, source_dir))

    # Collect pixel shaders
    ps_dir = source_dir / 'pixel'
    if ps_dir.exists():
        for ps_file in ps_dir.glob('*.ps.hlsl'):
            manifest['shaders'].append(extract_shader_metadata(ps_file, source_dir))

    # Collect compute shaders
    cs_dir = source_dir / 'compute'
    if cs_dir.exists():
        for cs_file in cs_dir.glob('*.cs.hlsl'):
            manifest['shaders'].append(extract_shader_metadata(cs_file, source_dir))

    # Collect common includes
    common_dir = source_dir / 'common'
    if common_dir.exists():
        for inc_file in common_dir.glob('*.hlsl'):
            manifest['common_includes'].append({
                'name': inc_file.name,
                'path': str(inc_file.relative_to(source_dir)),
                'hash': compute_file_hash(inc_file),
            })

    return manifest


def main():
    parser = argparse.ArgumentParser(description='Generate shader manifest JSON')
    parser.add_argument('--input', required=True, help='Shader build directory')
    parser.add_argument('--output', required=True, help='Output manifest JSON file')
    parser.add_argument('--source', default='shaders', help='Source shader directory')

    args = parser.parse_args()

    shaders_dir = Path(args.input)
    source_dir = Path(args.source)
    output_file = Path(args.output)

    if not shaders_dir.exists():
        print(f"Error: Shader build directory does not exist: {shaders_dir}", file=sys.stderr)
        return 1

    if not source_dir.exists():
        print(f"Error: Source shader directory does not exist: {source_dir}", file=sys.stderr)
        return 1

    print(f"Generating shader manifest...")
    manifest = generate_manifest(shaders_dir, source_dir)

    print(f"Found {len(manifest['shaders'])} shaders")
    print(f"Found {len(manifest['common_includes'])} common includes")

    output_file.parent.mkdir(parents=True, exist_ok=True)

    with open(output_file, 'w') as f:
        json.dump(manifest, f, indent=2)

    print(f"Manifest written to {output_file}")
    return 0


if __name__ == '__main__':
    sys.exit(main())
```

---

## Shader Variant Management

### Variant Types

Windows Terminal needs shader variants for:

1. **Backend Variants**
   - D3D11 (HLSL SM 5.0)
   - D3D12 (DXIL or SPIR-V)
   - OpenGL 3.3 (GLSL 330)
   - OpenGL 4.3 (GLSL 430 with compute)
   - Vulkan (SPIR-V native)
   - Metal (MSL) - future

2. **Feature Variants**
   - With/without compute shader support
   - With/without wave intrinsics
   - With/without 16-bit types
   - Debug vs Release (optimizations)

3. **Shading Type Variants**
   - Text rendering (grayscale, ClearType, builtin glyphs)
   - Line drawing (dotted, dashed, curly, solid)
   - Cursor rendering
   - Custom user shaders

### Variant Management Strategy

#### Compile-Time Selection

Use preprocessor defines to create variants:

```hlsl
// platform_compat.hlsl

#if defined(PLATFORM_D3D11) || defined(PLATFORM_D3D12)
    // Direct3D texture sampling
    #define SAMPLE_TEXTURE(tex, samp, uv) tex.Sample(samp, uv)
    #define SAMPLE_TEXTURE_LOAD(tex, coord) tex.Load(int3(coord, 0))

#elif defined(PLATFORM_OPENGL)
    // OpenGL texture sampling
    #define SAMPLE_TEXTURE(tex, samp, uv) texture(sampler2D(tex, samp), uv)
    #define SAMPLE_TEXTURE_LOAD(tex, coord) texelFetch(tex, ivec2(coord), 0)

#elif defined(PLATFORM_VULKAN)
    // Vulkan texture sampling
    #define SAMPLE_TEXTURE(tex, samp, uv) texture(sampler2D(tex, samp), uv)
    #define SAMPLE_TEXTURE_LOAD(tex, coord) texelFetch(tex, ivec2(coord), 0)
#endif

// Feature detection
#if defined(FEATURE_WAVE_INTRINSICS)
    #define USE_WAVE_INTRINSICS 1
#else
    #define USE_WAVE_INTRINSICS 0
#endif

#if defined(FEATURE_COMPUTE_SHADERS)
    #define USE_COMPUTE_SHADERS 1
#else
    #define USE_COMPUTE_SHADERS 0
#endif
```

#### Build-Time Variant Generation

Modified DXC compilation in MSBuild to generate variants:

```xml
<!-- Compile with platform-specific defines -->
<Exec Command="&quot;$(DxcPath)&quot; -spirv -T vs_6_0 -E main -D PLATFORM_D3D11=1 ... &quot;%(VertexShader.FullPath)&quot;" />
<Exec Command="&quot;$(DxcPath)&quot; -spirv -T vs_6_0 -E main -D PLATFORM_OPENGL=1 -D FEATURE_COMPUTE_SHADERS=0 ... &quot;%(VertexShader.FullPath)&quot;" />
<Exec Command="&quot;$(DxcPath)&quot; -spirv -T vs_6_0 -E main -D PLATFORM_OPENGL=1 -D FEATURE_COMPUTE_SHADERS=1 ... &quot;%(VertexShader.FullPath)&quot;" />
```

#### Runtime Variant Selection

C++ variant selection:

```cpp
// ShaderVariantManager.h

namespace Microsoft::Console::Render::Atlas
{
    enum class ShaderBackend
    {
        D3D11,
        D3D12,
        OpenGL33,
        OpenGL43,
        Vulkan,
        Metal,
    };

    enum class ShaderFeatureFlags : uint32_t
    {
        None = 0,
        ComputeShaders = 1 << 0,
        WaveIntrinsics = 1 << 1,
        Float16Types = 1 << 2,
        RayTracing = 1 << 3,  // Future
    };

    struct ShaderVariantKey
    {
        ShaderBackend backend;
        const char* shaderName;
        const char* entryPoint;
        ShaderFeatureFlags features;

        bool operator==(const ShaderVariantKey& other) const
        {
            return backend == other.backend &&
                   strcmp(shaderName, other.shaderName) == 0 &&
                   strcmp(entryPoint, other.entryPoint) == 0 &&
                   features == other.features;
        }

        size_t Hash() const
        {
            size_t h = std::hash<int>{}(static_cast<int>(backend));
            h ^= std::hash<std::string>{}(shaderName) << 1;
            h ^= std::hash<std::string>{}(entryPoint) << 2;
            h ^= std::hash<uint32_t>{}(static_cast<uint32_t>(features)) << 3;
            return h;
        }
    };

    class ShaderVariantManager
    {
    public:
        ShaderVariantManager();
        ~ShaderVariantManager();

        // Load shader variant
        const void* GetShaderBytecode(const ShaderVariantKey& key, size_t* outSize);

        // Check if variant exists
        bool HasVariant(const ShaderVariantKey& key) const;

        // Get all variants for a shader
        std::vector<ShaderVariantKey> GetVariants(const char* shaderName) const;

        // Hot reload (debug only)
        void ReloadShader(const char* shaderName);

    private:
        struct VariantData
        {
            std::vector<uint8_t> bytecode;
            ShaderReflection reflection;
        };

        std::unordered_map<ShaderVariantKey, VariantData> _variants;

        void _loadEmbeddedVariants();
        void _loadCachedVariants();
    };
}
```

---

## Runtime Shader Loading Architecture

### Shader Loading Pipeline

```
                    +---------------------------+
                    | ShaderVariantManager      |
                    +---------------------------+
                               |
                               | LoadShader(backend, name, entry)
                               v
                    +---------------------------+
                    | Variant Selection         |
                    | - Backend detection       |
                    | - Feature detection       |
                    | - Capability query        |
                    +---------------------------+
                               |
         +---------------------+---------------------+
         |                     |                     |
         v                     v                     v
    +----------+          +----------+          +----------+
    | Embedded |          | File     |          | Runtime  |
    | Resource |          | Cache    |          | Compile  |
    | Loader   |          | Loader   |          | (DEBUG)  |
    +----------+          +----------+          +----------+
         |                     |                     |
         +---------------------+---------------------+
                               |
                               v
                    +---------------------------+
                    | Shader Bytecode           |
                    | - SPIR-V / DXIL / GLSL    |
                    +---------------------------+
                               |
                               | CreateShader()
                               v
                    +---------------------------+
                    | Backend-Specific Compiler |
                    | - D3D11: D3DCompile       |
                    | - D3D12: Native DXIL      |
                    | - OpenGL: glCompileShader |
                    | - Vulkan: vkCreateShader  |
                    +---------------------------+
                               |
                               v
                    +---------------------------+
                    | GPU Shader Object         |
                    | - ID3D11VertexShader      |
                    | - ID3D12PipelineState     |
                    | - GLuint shader           |
                    | - VkShaderModule          |
                    +---------------------------+
```

### C++ Implementation

#### ShaderLoader.h

```cpp
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

namespace Microsoft::Console::Render::Atlas
{
    // Forward declarations
    enum class ShaderBackend;
    struct ShaderVariantKey;
    struct ShaderReflection;

    // Shader bytecode container
    struct ShaderBytecode
    {
        const void* data;
        size_t size;
        ShaderBackend backend;
        const char* entryPoint;

        ShaderBytecode()
            : data(nullptr), size(0), backend(ShaderBackend::D3D11), entryPoint("main") {}

        ShaderBytecode(const void* d, size_t s, ShaderBackend b, const char* entry = "main")
            : data(d), size(s), backend(b), entryPoint(entry) {}

        bool IsValid() const { return data != nullptr && size > 0; }
    };

    // Shader reflection data
    struct ShaderReflection
    {
        struct ConstantBuffer
        {
            std::string name;
            uint32_t slot;
            uint32_t size;
        };

        struct Texture
        {
            std::string name;
            uint32_t slot;
            uint32_t dimension;  // 1D, 2D, 3D, Cube
        };

        struct Sampler
        {
            std::string name;
            uint32_t slot;
        };

        std::vector<ConstantBuffer> constantBuffers;
        std::vector<Texture> textures;
        std::vector<Samplers> samplers;

        uint32_t threadGroupSizeX = 0;  // For compute shaders
        uint32_t threadGroupSizeY = 0;
        uint32_t threadGroupSizeZ = 0;
    };

    // Shader loader interface
    class IShaderLoader
    {
    public:
        virtual ~IShaderLoader() = default;

        // Load shader bytecode
        virtual ShaderBytecode LoadShader(const ShaderVariantKey& key) = 0;

        // Get shader reflection data
        virtual const ShaderReflection* GetReflection(const ShaderVariantKey& key) = 0;

        // Check if shader exists
        virtual bool HasShader(const ShaderVariantKey& key) const = 0;
    };

    // Embedded resource loader (from embedded shaders)
    class EmbeddedShaderLoader : public IShaderLoader
    {
    public:
        EmbeddedShaderLoader();
        ~EmbeddedShaderLoader() override;

        ShaderBytecode LoadShader(const ShaderVariantKey& key) override;
        const ShaderReflection* GetReflection(const ShaderVariantKey& key) override;
        bool HasShader(const ShaderVariantKey& key) const override;

    private:
        void _loadEmbeddedShaders();

        struct EmbeddedShader
        {
            const unsigned char* data;
            unsigned int size;
        };

        std::unordered_map<ShaderVariantKey, EmbeddedShader> _shaders;
        std::unordered_map<ShaderVariantKey, ShaderReflection> _reflections;
    };

    // File cache loader (from disk cache)
    class FileCacheShaderLoader : public IShaderLoader
    {
    public:
        FileCacheShaderLoader(const wchar_t* cacheDirectory);
        ~FileCacheShaderLoader() override;

        ShaderBytecode LoadShader(const ShaderVariantKey& key) override;
        const ShaderReflection* GetReflection(const ShaderVariantKey& key) override;
        bool HasShader(const ShaderVariantKey& key) const override;

        // Cache management
        void SaveShader(const ShaderVariantKey& key, const void* data, size_t size);
        void ClearCache();

    private:
        std::wstring _cacheDirectory;
        std::unordered_map<ShaderVariantKey, std::vector<uint8_t>> _cache;

        std::wstring _getShaderCachePath(const ShaderVariantKey& key) const;
    };

    // Runtime compiler (DEBUG builds only)
#ifdef _DEBUG
    class RuntimeShaderCompiler : public IShaderLoader
    {
    public:
        RuntimeShaderCompiler(const wchar_t* sourceDirectory);
        ~RuntimeShaderCompiler() override;

        ShaderBytecode LoadShader(const ShaderVariantKey& key) override;
        const ShaderReflection* GetReflection(const ShaderVariantKey& key) override;
        bool HasShader(const ShaderVariantKey& key) const override;

        // Hot reload support
        void EnableHotReload(bool enable);
        bool CheckForChanges();

    private:
        std::wstring _sourceDirectory;
        bool _hotReloadEnabled;

        ShaderBytecode _compileDXC(const wchar_t* source, const ShaderVariantKey& key);
        ShaderBytecode _compileSPIRVCross(const void* spirv, size_t size, const ShaderVariantKey& key);
    };
#endif

    // Composite shader loader (tries multiple sources)
    class CompositeShaderLoader : public IShaderLoader
    {
    public:
        CompositeShaderLoader();
        ~CompositeShaderLoader() override;

        // Add loader to chain (priority order)
        void AddLoader(std::unique_ptr<IShaderLoader> loader);

        ShaderBytecode LoadShader(const ShaderVariantKey& key) override;
        const ShaderReflection* GetReflection(const ShaderVariantKey& key) override;
        bool HasShader(const ShaderVariantKey& key) const override;

    private:
        std::vector<std::unique_ptr<IShaderLoader>> _loaders;
    };

} // namespace Microsoft::Console::Render::Atlas
```

#### ShaderLoader.cpp (Embedded Loader Implementation)

```cpp
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "ShaderLoader.h"
#include "shader_resources.h"  // Auto-generated embedded shaders
#include "ShaderVariantManager.h"

using namespace Microsoft::Console::Render::Atlas;
using namespace Microsoft::Console::Render::Atlas::EmbeddedShaders;

EmbeddedShaderLoader::EmbeddedShaderLoader()
{
    _loadEmbeddedShaders();
}

EmbeddedShaderLoader::~EmbeddedShaderLoader() = default;

void EmbeddedShaderLoader::_loadEmbeddedShaders()
{
    // Load all embedded shader resources
    for (unsigned int i = 0; i < g_shaderResourceCount; ++i)
    {
        const auto& resource = g_shaderResources[i];

        // Parse shader name to extract variant information
        // Example: "spirv/main.vs.opt.spv" or "opengl/gl330/main.vs.glsl"

        ShaderVariantKey key;
        std::string name(resource.name);

        // Determine backend from path
        if (name.find("spirv/") == 0)
        {
            key.backend = ShaderBackend::Vulkan;  // Or D3D12
        }
        else if (name.find("opengl/gl330/") == 0)
        {
            key.backend = ShaderBackend::OpenGL33;
        }
        else if (name.find("opengl/gl430/") == 0)
        {
            key.backend = ShaderBackend::OpenGL43;
        }
        else if (name.find("d3d11/") == 0)
        {
            key.backend = ShaderBackend::D3D11;
        }
        else if (name.find("d3d12/") == 0)
        {
            key.backend = ShaderBackend::D3D12;
        }
        else
        {
            continue;  // Unknown backend
        }

        // Extract shader name (e.g., "main.vs" from "opengl/gl330/main.vs.glsl")
        size_t lastSlash = name.find_last_of('/');
        std::string filename = (lastSlash != std::string::npos) ? name.substr(lastSlash + 1) : name;

        // Remove extension
        size_t lastDot = filename.find_last_of('.');
        key.shaderName = (lastDot != std::string::npos) ? filename.substr(0, lastDot).c_str() : filename.c_str();

        key.entryPoint = "main";
        key.features = ShaderFeatureFlags::None;

        // Detect compute shaders
        if (filename.find(".cs.") != std::string::npos)
        {
            key.features = static_cast<ShaderFeatureFlags>(
                static_cast<uint32_t>(key.features) | static_cast<uint32_t>(ShaderFeatureFlags::ComputeShaders)
            );
        }

        // Store shader data
        EmbeddedShader shader;
        shader.data = resource.data;
        shader.size = resource.size;

        _shaders[key] = shader;
    }
}

ShaderBytecode EmbeddedShaderLoader::LoadShader(const ShaderVariantKey& key)
{
    auto it = _shaders.find(key);
    if (it != _shaders.end())
    {
        const auto& shader = it->second;
        return ShaderBytecode(shader.data, shader.size, key.backend, key.entryPoint);
    }

    return ShaderBytecode();  // Invalid
}

const ShaderReflection* EmbeddedShaderLoader::GetReflection(const ShaderVariantKey& key)
{
    auto it = _reflections.find(key);
    if (it != _reflections.end())
    {
        return &it->second;
    }

    return nullptr;
}

bool EmbeddedShaderLoader::HasShader(const ShaderVariantKey& key) const
{
    return _shaders.find(key) != _shaders.end();
}
```

---

*(Continuing in next message due to length limit...)*
