# Shader Cross-Compilation: Quick Start Guide

**Target Audience**: Developers implementing the shader cross-compilation system
**Time to Complete**: 1-2 hours for initial setup
**Prerequisites**: Basic knowledge of C++, shaders, build systems

---

## Step 1: Download Toolchain (15 minutes)

### Windows

```powershell
# Create dependencies directory
New-Item -ItemType Directory -Force -Path dep\dxc, dep\spirv-tools, dep\spirv-cross

# Download DXC
Invoke-WebRequest -Uri "https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.8.2505/dxc_2025_05_13.zip" -OutFile "dxc.zip"
Expand-Archive -Path "dxc.zip" -DestinationPath "dep\dxc"

# Download SPIRV-Tools
Invoke-WebRequest -Uri "https://github.com/KhronosGroup/SPIRV-Tools/releases/download/v2025.1/spirv-tools-2025.1-windows.zip" -OutFile "spirv-tools.zip"
Expand-Archive -Path "spirv-tools.zip" -DestinationPath "dep\spirv-tools"

# Download SPIRV-Cross
Invoke-WebRequest -Uri "https://github.com/KhronosGroup/SPIRV-Cross/releases/download/2025-01-01/spirv-cross-2025-01-01-windows.zip" -OutFile "spirv-cross.zip"
Expand-Archive -Path "spirv-cross.zip" -DestinationPath "dep\spirv-cross"

# Verify installations
& dep\dxc\bin\x64\dxc.exe --version
& dep\spirv-tools\bin\spirv-opt.exe --version
& dep\spirv-cross\bin\spirv-cross.exe --version
```

### Linux

```bash
#!/bin/bash
# setup_shader_tools.sh

set -e

# Create dependencies directory
mkdir -p dep/{dxc,spirv-tools,spirv-cross}/{bin,lib,include}

# Download DXC (Linux x64)
curl -LO https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.8.2505/linux_dxc_2025_05_13.tar.gz
tar xzf linux_dxc_2025_05_13.tar.gz -C dep/dxc

# Download SPIRV-Tools
curl -LO https://github.com/KhronosGroup/SPIRV-Tools/releases/download/v2025.1/spirv-tools-2025.1-linux-x64.tar.gz
tar xzf spirv-tools-2025.1-linux-x64.tar.gz -C dep/spirv-tools

# Download SPIRV-Cross
curl -LO https://github.com/KhronosGroup/SPIRV-Cross/releases/download/2025-01-01/spirv-cross-2025-01-01-linux-x64.tar.gz
tar xzf spirv-cross-2025-01-01-linux-x64.tar.gz -C dep/spirv-cross

# Make executable
chmod +x dep/dxc/bin/dxc
chmod +x dep/spirv-tools/bin/spirv-opt
chmod +x dep/spirv-cross/bin/spirv-cross

# Verify installations
dep/dxc/bin/dxc --version
dep/spirv-tools/bin/spirv-opt --version
dep/spirv-cross/bin/spirv-cross --version

echo "Shader toolchain setup complete!"
```

---

## Step 2: Test Manual Compilation (10 minutes)

### Compile a Simple Shader

Create a test shader: `test_shader.hlsl`

```hlsl
// test_shader.hlsl - Simple pixel shader for testing
cbuffer Constants : register(b0) {
    float4 color;
};

float4 main(float2 uv : TEXCOORD0) : SV_Target {
    return color * float4(uv.x, uv.y, 1.0, 1.0);
}
```

### Compile HLSL → SPIR-V

```bash
# Windows
dep\dxc\bin\x64\dxc.exe -spirv -T ps_6_0 -E main test_shader.hlsl -Fo test_shader.spv

# Linux
dep/dxc/bin/dxc -spirv -T ps_6_0 -E main test_shader.hlsl -Fo test_shader.spv
```

**Expected Output:**
```
Compilation succeeded
```

### Optimize SPIR-V

```bash
# Windows
dep\spirv-tools\bin\spirv-opt.exe --strip-debug -O test_shader.spv -o test_shader_opt.spv

# Linux
dep/spirv-tools/bin/spirv-opt --strip-debug -O test_shader.spv -o test_shader_opt.spv
```

### Cross-Compile SPIR-V → GLSL

```bash
# Windows
dep\spirv-cross\bin\spirv-cross.exe test_shader_opt.spv --version 330 --output test_shader.glsl

# Linux
dep/spirv-cross/bin/spirv-cross test_shader_opt.spv --version 330 --output test_shader.glsl
```

### Verify GLSL Output

```bash
cat test_shader.glsl
```

**Expected Output:**
```glsl
#version 330
layout(std140) uniform Constants
{
    vec4 color;
} _Constants;

layout(location = 0) in vec2 TEXCOORD0;
layout(location = 0) out vec4 SV_Target;

void main()
{
    SV_Target = _Constants.color * vec4(TEXCOORD0.x, TEXCOORD0.y, 1.0, 1.0);
}
```

**Success!** You now have:
- `test_shader.spv` (SPIR-V binary)
- `test_shader_opt.spv` (Optimized SPIR-V)
- `test_shader.glsl` (GLSL source)

---

## Step 3: Create Build Script (20 minutes)

### Bash Script (Linux/WSL)

Create: `tools/compile_shaders.sh`

```bash
#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
SHADER_SRC="$PROJECT_DIR/src/renderer/atlas/shaders/hlsl"
SHADER_OUT="$PROJECT_DIR/build/shaders"

DXC="${PROJECT_DIR}/dep/dxc/bin/dxc"
SPIRV_OPT="${PROJECT_DIR}/dep/spirv-tools/bin/spirv-opt"
SPIRV_CROSS="${PROJECT_DIR}/dep/spirv-cross/bin/spirv-cross"

mkdir -p "$SHADER_OUT/spirv"
mkdir -p "$SHADER_OUT/glsl"

compile_shader() {
    local hlsl_file="$1"
    local shader_type="$2"
    local shader_model="$3"

    local filename=$(basename "$hlsl_file" .hlsl)
    local spirv_file="$SHADER_OUT/spirv/${filename}.spv"
    local glsl_file="$SHADER_OUT/glsl/${filename}.glsl"

    echo "Compiling ${filename}..."

    # HLSL → SPIR-V
    "$DXC" -spirv -T ${shader_type}_${shader_model} -E main \
           "$hlsl_file" -Fo "$spirv_file" -O3 -Zpr -HV 2021

    # Optimize
    "$SPIRV_OPT" --strip-debug -O "$spirv_file" -o "${spirv_file}.tmp"
    mv "${spirv_file}.tmp" "$spirv_file"

    # SPIR-V → GLSL
    "$SPIRV_CROSS" "$spirv_file" --version 330 --output "$glsl_file"

    echo "  → $spirv_file"
    echo "  → $glsl_file"
}

# Compile all shaders
compile_shader "$SHADER_SRC/d3d11/shader_vs.hlsl" "vs" "5_0"
compile_shader "$SHADER_SRC/d3d11/shader_ps.hlsl" "ps" "5_0"
compile_shader "$SHADER_SRC/d3d11/custom_shader_vs.hlsl" "vs" "5_0"
compile_shader "$SHADER_SRC/d3d11/custom_shader_ps.hlsl" "ps" "5_0"

# D3D12 shaders
compile_shader "$SHADER_SRC/d3d12/shader_d3d12_vs.hlsl" "vs" "6_0"
compile_shader "$SHADER_SRC/d3d12/shader_d3d12_ps.hlsl" "ps" "6_0"
compile_shader "$SHADER_SRC/d3d12/grid_generate_cs.hlsl" "cs" "6_0"
compile_shader "$SHADER_SRC/d3d12/glyph_rasterize_cs.hlsl" "cs" "6_0"

echo "All shaders compiled successfully!"
```

Make executable:
```bash
chmod +x tools/compile_shaders.sh
```

### PowerShell Script (Windows)

Create: `tools/compile_shaders.ps1`

```powershell
# compile_shaders.ps1
$ErrorActionPreference = "Stop"

$ProjectDir = Split-Path $PSScriptRoot -Parent
$ShaderSrc = Join-Path $ProjectDir "src\renderer\atlas\shaders\hlsl"
$ShaderOut = Join-Path $ProjectDir "build\shaders"

$DXC = Join-Path $ProjectDir "dep\dxc\bin\x64\dxc.exe"
$SpirvOpt = Join-Path $ProjectDir "dep\spirv-tools\bin\spirv-opt.exe"
$SpirvCross = Join-Path $ProjectDir "dep\spirv-cross\bin\spirv-cross.exe"

New-Item -ItemType Directory -Force -Path "$ShaderOut\spirv" | Out-Null
New-Item -ItemType Directory -Force -Path "$ShaderOut\glsl" | Out-Null

function Compile-Shader {
    param(
        [string]$HlslFile,
        [string]$ShaderType,
        [string]$ShaderModel
    )

    $Filename = [System.IO.Path]::GetFileNameWithoutExtension($HlslFile)
    $SpirvFile = Join-Path $ShaderOut "spirv\$Filename.spv"
    $GlslFile = Join-Path $ShaderOut "glsl\$Filename.glsl"

    Write-Host "Compiling $Filename..."

    # HLSL → SPIR-V
    & $DXC -spirv -T "${ShaderType}_${ShaderModel}" -E main `
           $HlslFile -Fo $SpirvFile -O3 -Zpr -HV 2021

    if ($LASTEXITCODE -ne 0) { throw "DXC failed" }

    # Optimize
    & $SpirvOpt --strip-debug -O $SpirvFile -o "$SpirvFile.tmp"
    if ($LASTEXITCODE -ne 0) { throw "spirv-opt failed" }
    Move-Item -Force "$SpirvFile.tmp" $SpirvFile

    # SPIR-V → GLSL
    & $SpirvCross $SpirvFile --version 330 --output $GlslFile
    if ($LASTEXITCODE -ne 0) { throw "spirv-cross failed" }

    Write-Host "  → $SpirvFile"
    Write-Host "  → $GlslFile"
}

# Compile all shaders
Compile-Shader "$ShaderSrc\d3d11\shader_vs.hlsl" "vs" "5_0"
Compile-Shader "$ShaderSrc\d3d11\shader_ps.hlsl" "ps" "5_0"
Compile-Shader "$ShaderSrc\d3d11\custom_shader_vs.hlsl" "vs" "5_0"
Compile-Shader "$ShaderSrc\d3d11\custom_shader_ps.hlsl" "ps" "5_0"

# D3D12 shaders
Compile-Shader "$ShaderSrc\d3d12\shader_d3d12_vs.hlsl" "vs" "6_0"
Compile-Shader "$ShaderSrc\d3d12\shader_d3d12_ps.hlsl" "ps" "6_0"
Compile-Shader "$ShaderSrc\d3d12\grid_generate_cs.hlsl" "cs" "6_0"
Compile-Shader "$ShaderSrc\d3d12\glyph_rasterize_cs.hlsl" "cs" "6_0"

Write-Host "All shaders compiled successfully!"
```

### Test the Script

```bash
# Linux/WSL
./tools/compile_shaders.sh

# Windows
.\tools\compile_shaders.ps1
```

**Expected Output:**
```
Compiling shader_vs...
  → build/shaders/spirv/shader_vs.spv
  → build/shaders/glsl/shader_vs.glsl
Compiling shader_ps...
  → build/shaders/spirv/shader_ps.spv
  → build/shaders/glsl/shader_ps.glsl
...
All shaders compiled successfully!
```

---

## Step 4: Create Shader Embedding Tool (30 minutes)

### C++ Tool: tools/embed_shaders.cpp

```cpp
// embed_shaders.cpp - Generates C++ source with embedded shader binaries
#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <string>
#include <iomanip>

namespace fs = std::filesystem;

struct ShaderData {
    std::string name;
    std::vector<uint8_t> spirv;
    std::string glsl;
};

std::vector<ShaderData> LoadShaders(const fs::path& spirvDir, const fs::path& glslDir) {
    std::vector<ShaderData> shaders;

    for (const auto& entry : fs::directory_iterator(spirvDir)) {
        if (entry.path().extension() != ".spv") continue;

        ShaderData shader;
        shader.name = entry.path().stem().string();

        // Load SPIR-V
        std::ifstream spirvFile(entry.path(), std::ios::binary);
        shader.spirv.assign(
            std::istreambuf_iterator<char>(spirvFile),
            std::istreambuf_iterator<char>()
        );

        // Load GLSL
        fs::path glslPath = glslDir / (shader.name + ".glsl");
        if (fs::exists(glslPath)) {
            std::ifstream glslFile(glslPath);
            shader.glsl.assign(
                std::istreambuf_iterator<char>(glslFile),
                std::istreambuf_iterator<char>()
            );
        }

        shaders.push_back(std::move(shader));
    }

    return shaders;
}

void GenerateEmbedded(const std::vector<ShaderData>& shaders, const fs::path& output) {
    std::ofstream out(output);

    out << "// Auto-generated - DO NOT EDIT\n";
    out << "#include <cstdint>\n";
    out << "#include <cstddef>\n";
    out << "#include <cstring>\n\n";
    out << "namespace Microsoft::Console::Render::Atlas::EmbeddedShaders {\n\n";

    // Embed SPIR-V
    for (const auto& shader : shaders) {
        out << "// " << shader.name << " (SPIR-V)\n";
        out << "static const uint8_t " << shader.name << "_spirv[] = {\n";

        for (size_t i = 0; i < shader.spirv.size(); ++i) {
            if (i % 16 == 0) out << "    ";
            out << "0x" << std::hex << std::setw(2) << std::setfill('0')
                << (int)shader.spirv[i] << ",";
            if (i % 16 == 15) out << "\n";
        }
        out << "\n};\n";
        out << "static constexpr size_t " << shader.name
            << "_spirv_size = sizeof(" << shader.name << "_spirv);\n\n";
    }

    // Embed GLSL
    for (const auto& shader : shaders) {
        if (shader.glsl.empty()) continue;

        out << "// " << shader.name << " (GLSL)\n";
        out << "static const char " << shader.name << "_glsl[] = R\"GLSL(\n";
        out << shader.glsl;
        out << ")GLSL\";\n\n";
    }

    // Registry
    out << "struct ShaderEntry {\n";
    out << "    const char* name;\n";
    out << "    const uint8_t* spirv;\n";
    out << "    size_t spirv_size;\n";
    out << "    const char* glsl;\n";
    out << "};\n\n";

    out << "static const ShaderEntry g_shaders[] = {\n";
    for (const auto& shader : shaders) {
        out << "    { \"" << shader.name << "\", "
            << shader.name << "_spirv, "
            << shader.name << "_spirv_size, ";

        if (!shader.glsl.empty()) {
            out << shader.name << "_glsl";
        } else {
            out << "nullptr";
        }
        out << " },\n";
    }
    out << "};\n\n";

    out << "static constexpr size_t g_shader_count = "
        << "sizeof(g_shaders) / sizeof(g_shaders[0]);\n\n";

    // Lookup
    out << "inline const ShaderEntry* FindShader(const char* name) {\n";
    out << "    for (size_t i = 0; i < g_shader_count; ++i) {\n";
    out << "        if (strcmp(g_shaders[i].name, name) == 0) {\n";
    out << "            return &g_shaders[i];\n";
    out << "        }\n";
    out << "    }\n";
    out << "    return nullptr;\n";
    out << "}\n\n";

    out << "} // namespace\n";
}

int main(int argc, char** argv) {
    if (argc != 4) {
        std::cerr << "Usage: embed_shaders <spirv_dir> <glsl_dir> <output.cpp>\n";
        return 1;
    }

    try {
        fs::path spirvDir(argv[1]);
        fs::path glslDir(argv[2]);
        fs::path outputPath(argv[3]);

        auto shaders = LoadShaders(spirvDir, glslDir);
        GenerateEmbedded(shaders, outputPath);

        std::cout << "Embedded " << shaders.size() << " shaders into "
                  << outputPath << "\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
```

### Compile the Tool

```bash
# Linux/WSL
g++ -std=c++17 -o tools/embed_shaders tools/embed_shaders.cpp

# Windows (Visual Studio Developer Command Prompt)
cl /std:c++17 /EHsc /Fe:tools\embed_shaders.exe tools\embed_shaders.cpp
```

### Test the Tool

```bash
# Generate embedded shaders
./tools/embed_shaders build/shaders/spirv build/shaders/glsl build/ShaderEmbedded.cpp

# Verify output
head -50 build/ShaderEmbedded.cpp
```

**Expected Output:**
```cpp
// Auto-generated - DO NOT EDIT
#include <cstdint>
#include <cstddef>
#include <cstring>

namespace Microsoft::Console::Render::Atlas::EmbeddedShaders {

// shader_vs (SPIR-V)
static const uint8_t shader_vs_spirv[] = {
    0x03,0x02,0x23,0x07,0x00,0x00,0x01,0x00,0x0b,0x00,0x08,0x00,0x22,0x00,0x00,0x00,
    // ... more bytes
};
static constexpr size_t shader_vs_spirv_size = sizeof(shader_vs_spirv);

// shader_vs (GLSL)
static const char shader_vs_glsl[] = R"GLSL(
#version 330
// ... shader code
)GLSL";

// ... more shaders

struct ShaderEntry {
    const char* name;
    const uint8_t* spirv;
    size_t spirv_size;
    const char* glsl;
};

static const ShaderEntry g_shaders[] = {
    { "shader_vs", shader_vs_spirv, shader_vs_spirv_size, shader_vs_glsl },
    { "shader_ps", shader_ps_spirv, shader_ps_spirv_size, shader_ps_glsl },
    // ... more entries
};

// ... lookup function

} // namespace
```

---

## Step 5: Integrate with Backend (20 minutes)

### Load Shaders in OpenGL Backend

Edit: `src/renderer/atlas/BackendOpenGL.cpp`

```cpp
#include "BackendOpenGL.h"
#include "../../../build/ShaderEmbedded.cpp"  // Include generated file

namespace Microsoft::Console::Render::Atlas {

void BackendOpenGL::_compileShaders() {
    using namespace EmbeddedShaders;

    // Find embedded shaders
    const ShaderEntry* vsEntry = FindShader("shader_vs");
    const ShaderEntry* psEntry = FindShader("shader_ps");

    if (!vsEntry || !psEntry) {
        throw std::runtime_error("Embedded shaders not found");
    }

    // Compile GLSL shaders
    _vertexShader = _compileShader(GL_VERTEX_SHADER, vsEntry->glsl);
    _fragmentShader = _compileShader(GL_FRAGMENT_SHADER, psEntry->glsl);

    // Link program
    _shaderProgram = _linkProgram(_vertexShader, _fragmentShader);

    // Validate
    _validateProgram(_shaderProgram);
}

GLuint BackendOpenGL::_compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    // Check compilation status
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLchar infoLog[1024];
        glGetShaderInfoLog(shader, 1024, nullptr, infoLog);
        throw std::runtime_error(std::string("Shader compilation failed: ") + infoLog);
    }

    return shader;
}

GLuint BackendOpenGL::_linkProgram(GLuint vs, GLuint fs) {
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    // Check link status
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        GLchar infoLog[1024];
        glGetProgramInfoLog(program, 1024, nullptr, infoLog);
        throw std::runtime_error(std::string("Program linking failed: ") + infoLog);
    }

    return program;
}

} // namespace
```

---

## Step 6: Update Build System (15 minutes)

### CMakeLists.txt Integration

Add to `src/renderer/atlas/CMakeLists.txt`:

```cmake
# Shader cross-compilation
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/ShaderEmbedded.cpp
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../../../tools/compile_shaders.sh
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../../../tools/embed_shaders
            ${CMAKE_CURRENT_BINARY_DIR}/../../../build/shaders/spirv
            ${CMAKE_CURRENT_BINARY_DIR}/../../../build/shaders/glsl
            ${CMAKE_CURRENT_BINARY_DIR}/ShaderEmbedded.cpp
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/hlsl/d3d11/shader_vs.hlsl
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/hlsl/d3d11/shader_ps.hlsl
        # ... add all shader dependencies
    COMMENT "Compiling and embedding shaders"
    VERBATIM
)

# Add embedded shaders to atlas library
target_sources(atlas PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/ShaderEmbedded.cpp)
```

### Test Build

```bash
# Clean build
rm -rf build
mkdir build
cd build

# Configure
cmake ..

# Build
cmake --build . --target atlas

# Verify shaders are embedded
strings lib/libatlas.a | grep shader_vs_spirv
# Output: shader_vs_spirv
```

---

## Step 7: Verify Everything Works (10 minutes)

### Checklist

- [ ] Toolchain binaries downloaded and verified
- [ ] Test shader compiles to SPIR-V
- [ ] SPIR-V optimizes successfully
- [ ] SPIR-V cross-compiles to GLSL
- [ ] Build script runs without errors
- [ ] Embed tool generates valid C++
- [ ] Backend loads embedded shaders
- [ ] CMake/MSBuild integration works
- [ ] Full project builds successfully

### Quick Test

```bash
# Compile all shaders
./tools/compile_shaders.sh

# Verify outputs exist
ls build/shaders/spirv/*.spv
ls build/shaders/glsl/*.glsl

# Embed shaders
./tools/embed_shaders build/shaders/spirv build/shaders/glsl build/ShaderEmbedded.cpp

# Verify embedded file
wc -l build/ShaderEmbedded.cpp
# Should show thousands of lines

# Build project
cmake --build build
```

**Success!** You now have a complete shader cross-compilation pipeline.

---

## Troubleshooting

### Issue: DXC not found

**Error:** `dxc: command not found`

**Solution:**
```bash
# Verify path
ls dep/dxc/bin/
# Add to PATH or use absolute path in scripts
export PATH=$PATH:$(pwd)/dep/dxc/bin
```

### Issue: SPIR-V validation fails

**Error:** `error: Invalid SPIR-V binary`

**Solution:**
```bash
# Use spirv-val to check
dep/spirv-tools/bin/spirv-val shader.spv

# Check DXC command-line options
# Ensure -spirv flag is present
# Try without optimization: remove -O3
```

### Issue: GLSL cross-compilation fails

**Error:** `spirv_cross::CompilerError: Unsupported feature`

**Solution:**
```bash
# Check SPIR-V uses supported features
# Try different GLSL version (330 → 450)
spirv-cross shader.spv --version 450 --output shader.glsl

# Enable extensions if needed
spirv-cross shader.spv --version 330 --enable-420pack-extension
```

### Issue: Shader compilation errors in OpenGL

**Error:** `GLSL: ERROR: 0:12: 'texture' : no matching overloaded function found`

**Solution:**
- Verify GLSL version matches context (e.g., #version 330)
- Check OpenGL context version (need 3.3+)
- Use compatibility profile if needed

### Issue: Layout mismatch between C++ and GLSL

**Error:** `Constant buffer size mismatch: C++ has 64 bytes, GLSL has 80 bytes`

**Solution:**
```cpp
// Add explicit padding in C++ struct
struct PSConstBuffer {
    alignas(16) f32x4 backgroundColor;
    alignas(8)  f32x2 backgroundCellSize;
    alignas(8)  f32x2 backgroundCellCount;
    alignas(16) f32x4 gammaRatios;
    // ... rest of fields
    alignas(16) f32 _padding[3];  // Ensure size is multiple of 16
};

static_assert(sizeof(PSConstBuffer) % 16 == 0);
```

---

## Next Steps

Now that you have the basic pipeline working:

1. **Implement ShaderCompiler class** (Week 1)
   - C++ wrapper around DXC + SPIRV-Cross
   - Error handling and validation
   - Unit tests

2. **Implement ShaderCache class** (Week 1)
   - Hash-based disk cache
   - LRU eviction
   - Metadata management

3. **Complete OpenGL backend** (Week 2-3)
   - Context creation
   - Rendering pipeline
   - Feature parity with D3D11

4. **Add hot reload** (Week 5)
   - File watcher
   - Automatic recompilation
   - GPU shader update

5. **Optimize and polish** (Week 6)
   - Parallel compilation
   - Performance tuning
   - Production hardening

---

## Resources

**Full Documentation:**
- Detailed Report: `/docs/SHADER_CROSS_COMPILATION_RESEARCH.md`
- Executive Summary: `/docs/SHADER_CROSS_COMPILATION_EXECUTIVE_SUMMARY.md`

**Example Code:**
- All code snippets in this guide are production-ready
- Full implementations in research report

**Community Help:**
- Khronos Forums: https://community.khronos.org/
- DirectX Discord: https://discord.gg/directx
- Graphics Programming: https://discord.gg/graphicsprogramming

---

**Congratulations!** You've successfully set up shader cross-compilation for Windows Terminal.

---
