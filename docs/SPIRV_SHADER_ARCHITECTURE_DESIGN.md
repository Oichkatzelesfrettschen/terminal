# SPIR-V Shader Architecture Design
## Ultra-Riced Windows Terminal - Pure SPIR-V Shader System

**Date**: 2025-10-11
**Status**: Design Document - Ready for Implementation
**Target**: Unified shader source with multi-backend compilation

---

## Executive Summary

This document details a complete SPIR-V-based shader architecture where all shaders are written once in HLSL Shader Model 6.0+ and compiled to SPIR-V as an intermediate representation. SPIRV-Cross then transpiles to target-specific shader languages (HLSL for D3D11, GLSL for OpenGL, MSL for Metal).

**Key Benefits**:
- **Write Once**: Single HLSL source for all backends
- **Zero Runtime Cost**: All compilation happens at build time
- **Future Proof**: Microsoft adopting SPIR-V for DirectX SM 7+
- **High Quality**: SPIRV-Cross generates human-readable output
- **Maintainable**: Single source of truth for shader logic

---

## Architecture Overview

### Compilation Pipeline

```
┌─────────────────────────────────────────────────────────────────────┐
│                        SHADER SOURCE LAYER                          │
│                                                                     │
│  src/renderer/atlas/shaders/hlsl/                                  │
│  ├── common/                                                        │
│  │   ├── types.hlsl           (Shared data structures)             │
│  │   ├── color.hlsl           (Color utilities)                    │
│  │   ├── dwrite.hlsl          (DirectWrite gamma correction)       │
│  │   └── math.hlsl            (Math utilities)                     │
│  ├── vertex/                                                        │
│  │   ├── main_vs.hlsl         (Main vertex shader)                 │
│  │   └── custom_vs.hlsl       (Custom shader vertex)               │
│  ├── pixel/                                                         │
│  │   ├── main_ps.hlsl         (Main pixel shader - 11 types)       │
│  │   └── custom_ps.hlsl       (User custom pixel shader)           │
│  └── compute/                                                       │
│      ├── glyph_rasterize_cs.hlsl  (Glyph atlas population)         │
│      └── grid_generate_cs.hlsl    (Grid generation)                │
└─────────────────────────────────────────────────────────────────────┘
                                ↓
┌─────────────────────────────────────────────────────────────────────┐
│                     BUILD-TIME COMPILATION                          │
│                                                                     │
│  DXC (DirectX Shader Compiler)                                      │
│  ├── Compile HLSL → SPIR-V binary                                  │
│  ├── Shader Model 6.0+ with SPIR-V code generation                 │
│  ├── Optimization: -O3                                             │
│  ├── Validation: Enabled                                           │
│  └── Output: .spv files                                            │
│                                                                     │
│  Command: dxc -spirv -T [vs|ps|cs]_6_0 -E main -O3 \              │
│           -fspv-reflect -fvk-use-dx-layout \                       │
│           input.hlsl -Fo output.spv                                │
└─────────────────────────────────────────────────────────────────────┘
                                ↓
┌─────────────────────────────────────────────────────────────────────┐
│                    SPIR-V INTERMEDIATE LAYER                        │
│                                                                     │
│  bin/shaders/spirv/                                                 │
│  ├── main_vs.spv               (Binary SPIR-V)                     │
│  ├── main_ps.spv                                                   │
│  ├── custom_vs.spv                                                 │
│  ├── custom_ps.spv                                                 │
│  ├── glyph_rasterize_cs.spv                                        │
│  └── grid_generate_cs.spv                                          │
│                                                                     │
│  spirv-opt (SPIR-V Optimizer)                                       │
│  ├── Dead code elimination                                         │
│  ├── Constant folding                                              │
│  ├── Loop unrolling                                                │
│  └── Inlining                                                      │
└─────────────────────────────────────────────────────────────────────┘
                                ↓
┌─────────────────────────────────────────────────────────────────────┐
│                    SPIRV-CROSS TRANSPILATION                        │
│                                                                     │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐         │
│  │ D3D11 Target │    │ OpenGL Target│    │ Metal Target │         │
│  ├──────────────┤    ├──────────────┤    ├──────────────┤         │
│  │ SPIR-V → HLSL│    │ SPIR-V → GLSL│    │ SPIR-V → MSL │         │
│  │ SM 5.0       │    │ 3.30 Core    │    │ 2.3          │         │
│  └──────────────┘    └──────────────┘    └──────────────┘         │
│                                                                     │
│  Output directories:                                                │
│  ├── bin/shaders/hlsl/     (D3D11 HLSL shaders)                    │
│  ├── bin/shaders/glsl/     (OpenGL GLSL shaders)                   │
│  └── bin/shaders/msl/      (Metal MSL shaders)                     │
└─────────────────────────────────────────────────────────────────────┘
                                ↓
┌─────────────────────────────────────────────────────────────────────┐
│                    TARGET-SPECIFIC COMPILATION                      │
│                                                                     │
│  D3D11: fxc.exe (HLSL → D3D bytecode)                              │
│  ├── fxc /T [vs|ps]_5_0 /E main /O3 input.hlsl /Fo output.cso     │
│  └── Output: .cso files embedded in binary                         │
│                                                                     │
│  D3D12: dxc.exe (HLSL → DXIL)                                      │
│  ├── dxc -T [vs|ps]_6_0 -E main -O3 input.hlsl -Fo output.dxil    │
│  └── Output: .dxil files embedded in binary                        │
│                                                                     │
│  OpenGL: Runtime compilation                                       │
│  ├── Load GLSL source at startup                                   │
│  ├── glCompileShader() + glLinkProgram()                           │
│  └── Cache compiled programs                                       │
└─────────────────────────────────────────────────────────────────────┘
                                ↓
┌─────────────────────────────────────────────────────────────────────┐
│                        RUNTIME LAYER                                │
│                                                                     │
│  ShaderManager                                                      │
│  ├── Load compiled shaders from embedded resources                 │
│  ├── Create pipeline state objects                                 │
│  ├── Hot reload support (debug builds)                             │
│  └── Shader variant management                                     │
└─────────────────────────────────────────────────────────────────────┘
```

---

## File Structure

### Source Directory Layout

```
src/renderer/atlas/shaders/
├── hlsl/                              # Primary HLSL source (SM 6.0+)
│   ├── common/
│   │   ├── types.hlsl                 # Data structure definitions
│   │   ├── color.hlsl                 # Color utilities
│   │   ├── dwrite.hlsl                # DirectWrite gamma correction
│   │   ├── math.hlsl                  # Math utilities
│   │   └── defines.hlsl               # Shader type constants
│   │
│   ├── vertex/
│   │   ├── main_vs.hlsl               # Main vertex shader
│   │   └── custom_vs.hlsl             # Custom shader vertex pass
│   │
│   ├── pixel/
│   │   ├── main_ps.hlsl               # Main pixel shader (all 11 types)
│   │   └── custom_ps.hlsl             # User-provided custom shader
│   │
│   └── compute/
│       ├── glyph_rasterize_cs.hlsl    # Glyph rasterization
│       └── grid_generate_cs.hlsl      # Grid generation
│
├── glsl/                              # Hand-written GLSL fallbacks (optional)
│   └── (compatibility shaders for older OpenGL)
│
└── build/                             # Build scripts
    ├── compile_shaders.ps1            # PowerShell build script
    ├── compile_shaders.py             # Python build script (cross-platform)
    └── shader_manifest.json           # Shader compilation manifest
```

### Output Directory Layout

```
bin/shaders/
├── spirv/                             # SPIR-V intermediate
│   ├── main_vs.spv
│   ├── main_ps.spv
│   ├── custom_vs.spv
│   ├── custom_ps.spv
│   ├── glyph_rasterize_cs.spv
│   └── grid_generate_cs.spv
│
├── d3d11/                             # D3D11 compiled shaders
│   ├── main_vs.cso
│   ├── main_ps.cso
│   ├── custom_vs.cso
│   └── custom_ps.cso
│
├── d3d12/                             # D3D12 compiled shaders
│   ├── main_vs.dxil
│   ├── main_ps.dxil
│   ├── custom_vs.dxil
│   ├── custom_ps.dxil
│   ├── glyph_rasterize_cs.dxil
│   └── grid_generate_cs.dxil
│
├── glsl/                              # OpenGL GLSL source
│   ├── main_vs.glsl
│   ├── main_ps.glsl
│   ├── custom_vs.glsl
│   ├── custom_ps.glsl
│   ├── glyph_rasterize_cs.glsl
│   └── grid_generate_cs.glsl
│
└── msl/                               # Metal shaders (future)
    ├── main_vs.metal
    ├── main_ps.metal
    └── (etc.)
```

---

## Shader Entry Points

### Vertex Shaders

#### 1. Main Vertex Shader (`main_vs.hlsl`)

**Purpose**: Transform vertex data from instance data to screen space

**Entry Point**: `main()`

**Input Layout**:
```hlsl
struct VSData
{
    float2 vertex : SV_Position;          // Quad corner (0,0), (1,0), (0,1), (1,1)
    uint shadingType : shadingType;        // SHADING_TYPE_* constant
    uint2 renditionScale : renditionScale; // Double width/height for line rendition
    int2 position : position;              // Screen position in pixels
    uint2 size : size;                     // Quad size in pixels
    uint2 texcoord : texcoord;             // Atlas texture coordinates
    float4 color : color;                  // RGBA color (packed uint32 → float4)
};
```

**Output**:
```hlsl
struct PSData
{
    float4 position : SV_Position;             // Clip space position
    float2 texcoord : texcoord;                // Interpolated texture coords
    nointerpolation uint shadingType : shadingType;
    nointerpolation float2 renditionScale : renditionScale;
    nointerpolation float4 color : color;
};
```

**Constant Buffer**:
```hlsl
cbuffer VSConstBuffer : register(b0)
{
    float2 positionScale;  // Screen space transform (2/width, -2/height)
};
```

#### 2. Custom Vertex Shader (`custom_vs.hlsl`)

**Purpose**: Vertex shader for user-provided custom pixel shaders

**Entry Point**: `main()`

**Same I/O as main_vs.hlsl**

---

### Pixel/Fragment Shaders

#### 1. Main Pixel Shader (`main_ps.hlsl`)

**Purpose**: Render all 11 shading types

**Entry Point**: `main()`

**Shading Types**:
```hlsl
#define SHADING_TYPE_TEXT_BACKGROUND    0  // Background bitmap
#define SHADING_TYPE_TEXT_GRAYSCALE     1  // Grayscale antialiased text
#define SHADING_TYPE_TEXT_CLEARTYPE     2  // ClearType subpixel text
#define SHADING_TYPE_TEXT_BUILTIN_GLYPH 3  // Box drawing characters
#define SHADING_TYPE_TEXT_PASSTHROUGH   4  // Direct texture sample
#define SHADING_TYPE_DOTTED_LINE        5  // Dotted underline
#define SHADING_TYPE_DASHED_LINE        6  // Dashed underline
#define SHADING_TYPE_CURLY_LINE         7  // Curly/wavy underline
#define SHADING_TYPE_SOLID_LINE         8  // Solid underline
#define SHADING_TYPE_CURSOR             9  // Cursor rendering
#define SHADING_TYPE_FILLED_RECT       10  // Selection rectangle
```

**Constant Buffer**:
```hlsl
cbuffer PSConstBuffer : register(b0)
{
    float4 backgroundColor;        // Background clear color
    float2 backgroundCellSize;     // Cell size in pixels
    float2 backgroundCellCount;    // Grid dimensions
    float4 gammaRatios;            // Gamma correction ratios
    float enhancedContrast;        // Contrast enhancement factor
    float underlineWidth;          // Underline thickness
    float doubleUnderlineWidth;    // Double underline thickness
    float curlyLineHalfHeight;     // Curly underline amplitude
    float shadedGlyphDotSize;      // Builtin glyph dot size
};
```

**Textures**:
```hlsl
Texture2D<float4> background : register(t0);   // Background bitmap (per-cell colors)
Texture2D<float4> glyphAtlas : register(t1);   // Glyph atlas texture
```

**Output**:
```hlsl
struct Output
{
    float4 color : SV_Target0;     // RGB color (premultiplied alpha)
    float4 weights : SV_Target1;   // ClearType weights (for dual-source blending)
};
```

#### 2. Custom Pixel Shader (`custom_ps.hlsl`)

**Purpose**: User-provided custom shader support

**Entry Point**: `customPixelShader()`

**Constant Buffer** (Additional):
```hlsl
cbuffer CustomConstBuffer : register(b1)
{
    float time;                    // Animation time
    float2 resolution;             // Screen resolution
    float4 customBackgroundColor;  // Background for shader
};
```

---

### Compute Shaders

#### 1. Glyph Rasterization (`glyph_rasterize_cs.hlsl`)

**Purpose**: GPU-accelerated glyph rasterization (future optimization)

**Entry Point**: `main()`

**Thread Group Size**: `[numthreads(8, 8, 1)]`

**Resources**:
```hlsl
RWTexture2D<float4> glyphAtlas : register(u0);  // Output atlas
Texture2D<float4> fontTexture : register(t0);   // Input font texture
StructuredBuffer<GlyphInfo> glyphs : register(t1);  // Glyph metadata
```

#### 2. Grid Generation (`grid_generate_cs.hlsl`)

**Purpose**: Generate text grid instance data on GPU

**Entry Point**: `main()`

**Thread Group Size**: `[numthreads(64, 1, 1)]`

**Resources**:
```hlsl
RWStructuredBuffer<VSData> instances : register(u0);  // Output instance data
StructuredBuffer<Cell> cells : register(t0);          // Input text buffer
ConstantBuffer<GridParams> params : register(b0);     // Grid parameters
```

---

## Shared Shader Library Components

### color.hlsl

```hlsl
// Color utility functions
float4 premultiplyColor(float4 color)
{
    color.rgb *= color.a;
    return color;
}

float4 alphaBlendPremultiplied(float4 bottom, float4 top)
{
    bottom *= 1 - top.a;
    return bottom + top;
}

float4 decodeRGBA(uint packed)
{
    return (packed >> uint4(0, 8, 16, 24) & 0xff) / 255.0f;
}

uint encodeRGBA(float4 color)
{
    uint4 c = uint4(color * 255.0f + 0.5f);
    return (c.r << 0) | (c.g << 8) | (c.b << 16) | (c.a << 24);
}

float luminance(float3 rgb)
{
    return dot(rgb, float3(0.299f, 0.587f, 0.114f));
}

float3 linearToSRGB(float3 linear)
{
    return pow(linear, 1.0f / 2.2f);
}

float3 sRGBToLinear(float3 srgb)
{
    return pow(srgb, 2.2f);
}
```

### dwrite.hlsl

```hlsl
// DirectWrite-compatible gamma correction and contrast enhancement
float DWrite_ApplyLightOnDarkContrastAdjustment(float enhancedContrast, float3 color)
{
    float intensity = luminance(color);
    return enhancedContrast * (1.0f - intensity);
}

float DWrite_CalcColorIntensity(float3 color)
{
    return luminance(color);
}

float DWrite_EnhanceContrast(float alpha, float contrast)
{
    return pow(alpha, 1.0f + contrast);
}

float3 DWrite_EnhanceContrast3(float3 alpha, float contrast)
{
    return pow(alpha, 1.0f + contrast);
}

float DWrite_ApplyAlphaCorrection(float alpha, float intensity, float4 gammaRatios)
{
    // Apply gamma correction based on foreground intensity
    float gamma = lerp(gammaRatios.x, gammaRatios.y, intensity);
    return pow(alpha, gamma);
}

float3 DWrite_ApplyAlphaCorrection3(float3 alpha, float3 color, float4 gammaRatios)
{
    // Per-channel gamma correction for ClearType
    return float3(
        pow(alpha.r, gammaRatios.r),
        pow(alpha.g, gammaRatios.g),
        pow(alpha.b, gammaRatios.b)
    );
}
```

### math.hlsl

```hlsl
// Math utilities
float2 rotate2D(float2 v, float angle)
{
    float s = sin(angle);
    float c = cos(angle);
    return float2(v.x * c - v.y * s, v.x * s + v.y * c);
}

float sdBox(float2 p, float2 size)
{
    float2 d = abs(p) - size;
    return length(max(d, 0.0f)) + min(max(d.x, d.y), 0.0f);
}

float sdRoundedBox(float2 p, float2 size, float radius)
{
    float2 d = abs(p) - size + radius;
    return length(max(d, 0.0f)) - radius + min(max(d.x, d.y), 0.0f);
}

float sdCircle(float2 p, float radius)
{
    return length(p) - radius;
}

float sdLine(float2 p, float2 a, float2 b)
{
    float2 pa = p - a;
    float2 ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0f, 1.0f);
    return length(pa - ba * h);
}
```

---

## Build System Integration

### MSBuild Integration

**File**: `src/renderer/atlas/shaders/shaders.vcxproj`

```xml
<Project>
  <PropertyGroup>
    <DxcPath>$(VULKAN_SDK)\Bin\dxc.exe</DxcPath>
    <SpirvOptPath>$(VULKAN_SDK)\Bin\spirv-opt.exe</SpirvOptPath>
    <SpirvCrossPath>$(SolutionDir)tools\spirv-cross\spirv-cross.exe</SpirvCrossPath>
    <ShaderOutputDir>$(OutDir)shaders\</ShaderOutputDir>
  </PropertyGroup>

  <ItemGroup>
    <HLSLShader Include="hlsl\vertex\main_vs.hlsl">
      <ShaderType>Vertex</ShaderType>
      <ShaderModel>6_0</ShaderModel>
      <EntryPoint>main</EntryPoint>
    </HLSLShader>
    <HLSLShader Include="hlsl\pixel\main_ps.hlsl">
      <ShaderType>Pixel</ShaderType>
      <ShaderModel>6_0</ShaderModel>
      <EntryPoint>main</EntryPoint>
    </HLSLShader>
    <!-- Add more shaders -->
  </ItemGroup>

  <Target Name="CompileShaders" BeforeTargets="ClCompile">
    <Exec Command="powershell -ExecutionPolicy Bypass -File $(ProjectDir)build\compile_shaders.ps1" />
  </Target>
</Project>
```

### PowerShell Build Script

**File**: `src/renderer/atlas/shaders/build/compile_shaders.ps1`

```powershell
# Shader Compilation Pipeline
param(
    [string]$Config = "Debug",
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

# Paths
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$rootDir = Split-Path -Parent $scriptDir
$hlslDir = Join-Path $rootDir "hlsl"
$outputDir = Join-Path $rootDir "..\..\..\..\..\bin\shaders"
$spirvDir = Join-Path $outputDir "spirv"
$d3d11Dir = Join-Path $outputDir "d3d11"
$d3d12Dir = Join-Path $outputDir "d3d12"
$glslDir = Join-Path $outputDir "glsl"

# Tools (from Vulkan SDK or custom install)
$dxc = "dxc.exe"
$spirvOpt = "spirv-opt.exe"
$spirvCross = Join-Path $scriptDir "..\..\..\..\tools\spirv-cross\spirv-cross.exe"
$fxc = "fxc.exe"

# Create output directories
foreach ($dir in @($spirvDir, $d3d11Dir, $d3d12Dir, $glslDir)) {
    if (!(Test-Path $dir)) {
        New-Item -ItemType Directory -Path $dir | Out-Null
    }
}

# Shader manifest
$manifest = @(
    @{Name="main_vs"; Type="vs"; Entry="main"; File="vertex\main_vs.hlsl"}
    @{Name="main_ps"; Type="ps"; Entry="main"; File="pixel\main_ps.hlsl"}
    @{Name="custom_vs"; Type="vs"; Entry="main"; File="vertex\custom_vs.hlsl"}
    @{Name="custom_ps"; Type="ps"; Entry="customPixelShader"; File="pixel\custom_ps.hlsl"}
    @{Name="glyph_rasterize_cs"; Type="cs"; Entry="main"; File="compute\glyph_rasterize_cs.hlsl"}
    @{Name="grid_generate_cs"; Type="cs"; Entry="main"; File="compute\grid_generate_cs.hlsl"}
)

Write-Host "==================================" -ForegroundColor Cyan
Write-Host "Shader Compilation Pipeline" -ForegroundColor Cyan
Write-Host "==================================" -ForegroundColor Cyan

foreach ($shader in $manifest) {
    $name = $shader.Name
    $type = $shader.Type
    $entry = $shader.Entry
    $file = $shader.File

    $hlslPath = Join-Path $hlslDir $file
    $spirvPath = Join-Path $spirvDir "$name.spv"
    $spirvOptPath = Join-Path $spirvDir "$name.opt.spv"

    Write-Host "`nCompiling: $name ($type)" -ForegroundColor Yellow

    # Step 1: HLSL → SPIR-V (DXC)
    Write-Host "  [1/4] DXC: HLSL → SPIR-V" -ForegroundColor Gray
    & $dxc -spirv -T "$type`_6_0" -E $entry -O3 `
        -fspv-reflect -fvk-use-dx-layout `
        -I $hlslDir `
        $hlslPath -Fo $spirvPath

    if ($LASTEXITCODE -ne 0) {
        Write-Error "DXC compilation failed for $name"
    }

    # Step 2: Optimize SPIR-V
    Write-Host "  [2/4] spirv-opt: Optimization" -ForegroundColor Gray
    & $spirvOpt -O $spirvPath -o $spirvOptPath
    Move-Item -Force $spirvOptPath $spirvPath

    # Step 3: SPIR-V → GLSL (SPIRV-Cross)
    Write-Host "  [3/4] SPIRV-Cross: SPIR-V → GLSL" -ForegroundColor Gray
    $glslPath = Join-Path $glslDir "$name.glsl"
    & $spirvCross --version 330 --no-es --output $glslPath $spirvPath

    # Step 4: SPIR-V → HLSL → D3D11 bytecode (for D3D11 backend)
    Write-Host "  [4/4] FXC: HLSL → D3D11 bytecode" -ForegroundColor Gray
    $hlslTranspiledPath = Join-Path $d3d11Dir "$name.transpiled.hlsl"
    & $spirvCross --hlsl --shader-model 50 --output $hlslTranspiledPath $spirvPath

    $csoPath = Join-Path $d3d11Dir "$name.cso"
    & $fxc /T "$type`_5_0" /E $entry /O3 $hlslTranspiledPath /Fo $csoPath

    # For D3D12: compile original HLSL with DXC
    if ($type -ne "cs" -or $Config -eq "Release") {
        $dxilPath = Join-Path $d3d12Dir "$name.dxil"
        & $dxc -T "$type`_6_0" -E $entry -O3 $hlslPath -Fo $dxilPath
    }

    Write-Host "  ✓ Complete" -ForegroundColor Green
}

Write-Host "`n==================================" -ForegroundColor Cyan
Write-Host "Shader compilation complete!" -ForegroundColor Green
Write-Host "==================================" -ForegroundColor Cyan
```

---

## Runtime Shader Loading

### ShaderManager Class

**File**: `src/renderer/atlas/ShaderManager.h`

```cpp
#pragma once
#include "common.h"

namespace Microsoft::Console::Render::Atlas
{
    enum class ShaderBackend
    {
        D3D11,
        D3D12,
        OpenGL,
        Vulkan,
        Metal
    };

    struct ShaderBytecode
    {
        const void* data;
        size_t size;
    };

    class ShaderManager
    {
    public:
        ShaderManager(ShaderBackend backend);

        // Load compiled shader from embedded resources
        ShaderBytecode LoadVertexShader(const char* name);
        ShaderBytecode LoadPixelShader(const char* name);
        ShaderBytecode LoadComputeShader(const char* name);

        // Hot reload support (debug builds only)
        void EnableHotReload(const wchar_t* shaderDirectory);
        bool CheckForUpdates();  // Returns true if shaders changed

        // Custom shader loading
        ShaderBytecode LoadCustomShader(const wchar_t* filePath);

    private:
        ShaderBackend m_backend;
        std::unordered_map<std::string, ShaderBytecode> m_cache;

#ifdef ATLAS_DEBUG_SHADER_HOT_RELOAD
        wil::unique_hfile m_hotReloadDirectory;
        FILETIME m_lastModifiedTime;
#endif
    };
}
```

**Implementation**: `src/renderer/atlas/ShaderManager.cpp`

```cpp
#include "pch.h"
#include "ShaderManager.h"

namespace Microsoft::Console::Render::Atlas
{
    ShaderManager::ShaderManager(ShaderBackend backend)
        : m_backend(backend)
    {
    }

    ShaderBytecode ShaderManager::LoadVertexShader(const char* name)
    {
        // Check cache first
        auto it = m_cache.find(name);
        if (it != m_cache.end())
        {
            return it->second;
        }

        // Load from embedded resources
        const char* resourcePrefix = nullptr;
        const char* resourceExtension = nullptr;

        switch (m_backend)
        {
        case ShaderBackend::D3D11:
            resourcePrefix = "SHADER_D3D11";
            resourceExtension = "cso";
            break;
        case ShaderBackend::D3D12:
            resourcePrefix = "SHADER_D3D12";
            resourceExtension = "dxil";
            break;
        case ShaderBackend::OpenGL:
            resourcePrefix = "SHADER_GLSL";
            resourceExtension = "glsl";
            break;
        default:
            THROW_HR(E_NOTIMPL);
        }

        // Load from resource
        std::string resourceName = std::string(resourcePrefix) + "_" + name;
        HRSRC hResource = FindResourceA(nullptr, resourceName.c_str(), "SHADER");
        THROW_LAST_ERROR_IF_NULL(hResource);

        HGLOBAL hGlobal = LoadResource(nullptr, hResource);
        THROW_LAST_ERROR_IF_NULL(hGlobal);

        void* data = LockResource(hGlobal);
        THROW_HR_IF_NULL(E_POINTER, data);

        size_t size = SizeofResource(nullptr, hResource);

        ShaderBytecode bytecode{ data, size };
        m_cache[name] = bytecode;
        return bytecode;
    }

#ifdef ATLAS_DEBUG_SHADER_HOT_RELOAD
    void ShaderManager::EnableHotReload(const wchar_t* shaderDirectory)
    {
        m_hotReloadDirectory.reset(CreateFileW(
            shaderDirectory,
            FILE_LIST_DIRECTORY,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS,
            nullptr
        ));
        THROW_LAST_ERROR_IF(!m_hotReloadDirectory);
    }

    bool ShaderManager::CheckForUpdates()
    {
        // Check file modification times
        // If changed, clear cache and reload
        // Return true to signal re-compilation needed
        return false;  // Simplified
    }
#endif
}
```

---

## Shader Variant Management

### Backend-Specific Features

Some features are backend-specific and need conditional compilation:

**Example**: Compute shaders only on D3D12/Vulkan

```hlsl
// In main shader source
#ifdef BACKEND_D3D12
    // D3D12-specific optimizations
    [numthreads(64, 1, 1)]
    void ComputeMain(uint3 dispatchThreadID : SV_DispatchThreadID)
    {
        // GPU-driven rendering
    }
#endif
```

**Build Script** passes defines:
```bash
dxc -spirv -D BACKEND_D3D12 -T cs_6_0 ...
```

### Feature Detection

**File**: `src/renderer/atlas/ShaderFeatures.h`

```cpp
struct ShaderFeatures
{
    bool computeShaders;        // CS 5.0+
    bool dualSourceBlending;    // For ClearType
    bool conservativeRaster;    // For text rendering
    bool variableRateShading;   // D3D12.2+
    bool meshShaders;           // D3D12.2+, Vulkan 1.2+
};

ShaderFeatures DetectShaderFeatures(IBackend* backend);
```

---

## Implementation Plan

### Phase 1: Toolchain Setup (Week 1 - 8 hours)

**Tasks**:
1. Download and install Vulkan SDK (includes DXC, spirv-opt, spirv-val) - 0.5h
2. Build SPIRV-Cross from source or download prebuilt - 1h
3. Integrate tools into repository `/tools/` directory - 0.5h
4. Create PowerShell build script `compile_shaders.ps1` - 2h
5. Create Python build script for cross-platform support - 2h
6. Test compilation pipeline manually - 1h
7. Verify output quality of GLSL/HLSL transpilation - 1h

**Validation**: Successfully compile one shader through full pipeline

### Phase 2: Shader Source Refactoring (Week 1-2 - 12 hours)

**Tasks**:
1. Create new directory structure `shaders/hlsl/` - 0.5h
2. Refactor `shader_common.hlsl` into modular includes - 2h
   - types.hlsl
   - color.hlsl
   - dwrite.hlsl
   - math.hlsl
3. Update `main_vs.hlsl` with new includes - 1h
4. Update `main_ps.hlsl` with new includes - 2h
5. Port compute shaders to new structure - 2h
6. Create shader manifest JSON - 1h
7. Test compilation of all shaders - 2h
8. Verify SPIR-V output with spirv-val - 0.5h
9. Commit refactored shaders - 1h

**Validation**: All shaders compile to SPIR-V without errors

### Phase 3: MSBuild Integration (Week 2 - 8 hours)

**Tasks**:
1. Create `shaders.vcxproj` project - 1h
2. Add CustomBuild step for shader compilation - 2h
3. Integrate with main solution build - 1h
4. Create pre-build event to run shader compilation - 1h
5. Configure incremental compilation (timestamp checking) - 2h
6. Test clean build - 0.5h
7. Test incremental build - 0.5h

**Validation**: Shaders rebuild automatically when source changes

### Phase 4: Resource Embedding (Week 2 - 6 hours)

**Tasks**:
1. Create `ShaderResources.rc` resource file - 1h
2. Embed compiled shaders as binary resources - 2h
3. Create resource IDs header `ShaderResourceIDs.h` - 1h
4. Implement ShaderManager::LoadShader() - 2h

**Validation**: Shaders load from embedded resources at runtime

### Phase 5: Hot Reload Support (Week 3 - 8 hours)

**Tasks**:
1. Implement file watcher for shader directory - 3h
2. Add shader recompilation on file change - 2h
3. Integrate with D3D12 pipeline recreation - 2h
4. Add error reporting UI for shader compilation errors - 1h

**Validation**: Edit shader file, see changes in running application

### Phase 6: OpenGL Integration (Week 3-4 - 12 hours)

**Tasks**:
1. Update BackendOpenGL to load GLSL shaders - 3h
2. Implement glCompileShader wrapper with error handling - 2h
3. Parse SPIRV-Cross reflection data for uniform locations - 3h
4. Test all 11 shading types in OpenGL - 3h
5. Performance profiling vs hand-written GLSL - 1h

**Validation**: OpenGL backend renders identically to D3D11

### Phase 7: Documentation (Week 4 - 4 hours)

**Tasks**:
1. Write shader authoring guide - 2h
2. Document build system - 1h
3. Create troubleshooting guide - 1h

**Validation**: Another developer can add a new shader

---

## Total Time Estimate

| Phase | Duration | Hours |
|-------|----------|-------|
| 1. Toolchain Setup | Week 1 | 8h |
| 2. Shader Refactoring | Week 1-2 | 12h |
| 3. MSBuild Integration | Week 2 | 8h |
| 4. Resource Embedding | Week 2 | 6h |
| 5. Hot Reload | Week 3 | 8h |
| 6. OpenGL Integration | Week 3-4 | 12h |
| 7. Documentation | Week 4 | 4h |
| **Total** | **4 weeks** | **58 hours** |

---

## Success Criteria

1. ✅ All shaders compile from single HLSL source
2. ✅ SPIRV-Cross generates readable, efficient target code
3. ✅ Zero runtime overhead (compile-time only)
4. ✅ Incremental builds work (only recompile changed shaders)
5. ✅ Hot reload works in debug builds
6. ✅ OpenGL backend achieves feature parity with D3D11
7. ✅ Performance is equivalent to hand-written shaders (<5% difference)
8. ✅ Build system is documented and maintainable

---

## Future Enhancements

1. **Vulkan Backend**: Add SPIR-V → Vulkan pipeline (direct SPIR-V load)
2. **Metal Backend**: Add SPIR-V → MSL transpilation for macOS
3. **WebGPU**: Add WGSL output for web platform
4. **Shader Caching**: Cache compiled shaders to disk for faster startup
5. **Shader Preprocessing**: Add custom preprocessor for advanced features
6. **Permutation Generation**: Auto-generate shader variants from single source

---

**Document Status**: Ready for Review and Implementation
**Next Step**: Begin Phase 1 - Toolchain Setup
