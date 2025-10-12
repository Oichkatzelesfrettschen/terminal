# OpenGL Backend Quick Start Guide

**For Developers**: This guide provides everything you need to begin implementing the OpenGL backend.

---

## Prerequisites

### Build Dependencies

```bash
# Windows (vcpkg)
vcpkg install glad:x64-windows
vcpkg install stb:x64-windows

# Linux (Ubuntu/Debian)
sudo apt install libgl1-mesa-dev libglu1-mesa-dev
sudo apt install libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev

# Linux (Fedora)
sudo dnf install mesa-libGL-devel mesa-libGLU-devel
sudo dnf install libX11-devel libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel
```

### Generate GLAD Loader

1. Visit https://glad.dav1d.de/
2. Select:
   - Language: C/C++
   - Specification: OpenGL
   - API gl: Version 3.3 (or 4.5)
   - Profile: Core
3. Extensions:
   - `GL_ARB_buffer_storage` (for persistent mapping)
   - `GL_ARB_direct_state_access` (for DSA)
   - `GL_ARB_multi_draw_indirect` (for MDI)
   - `GL_ARB_bindless_texture` (for bindless)
   - `GL_KHR_debug` (for debug callbacks)
4. Download and extract to `src/renderer/atlas/glad/`

---

## Project Structure

```
src/renderer/atlas/
  +-- BackendOpenGL.h          (Interface definition)
  +-- BackendOpenGL.cpp        (Core implementation)
  +-- BackendOpenGL_WGL.cpp    (Windows-specific)
  +-- BackendOpenGL_GLX.cpp    (Linux-specific)
  +-- shader_gl_common.glsl    (Common shader definitions)
  +-- shader_gl_vs.glsl        (Vertex shader)
  +-- shader_gl_fs.glsl        (Fragment shader)
  +-- glad/
      +-- glad.h
      +-- glad.c
```

---

## Implementation Checklist

### Phase 1: Basic Setup (Day 1-2)

- [ ] Add `BackendOpenGL.cpp` to CMakeLists.txt/vcxproj
- [ ] Implement constructor/destructor
- [ ] Create context (WGL on Windows, GLX on Linux)
- [ ] Load GLAD extensions
- [ ] Detect OpenGL version and features
- [ ] Create basic VAO with quad vertices
- [ ] Compile simple test shader (solid color)
- [ ] Render a test quad to verify setup

**Test**: Run terminal, should see a solid color rectangle

### Phase 2: Shader System (Day 3-4)

- [ ] Load vertex shader from `shader_gl_vs.glsl`
- [ ] Load fragment shader from `shader_gl_fs.glsl`
- [ ] Compile and link shader program
- [ ] Create uniform buffer objects (UBOs)
- [ ] Implement `_updateConstantBuffers()`
- [ ] Test with background rendering (solid color)

**Test**: Terminal background renders correctly

### Phase 3: Instance Rendering (Day 5-6)

- [ ] Create instance buffer (VBO)
- [ ] Set up vertex attributes for QuadInstance
- [ ] Implement `_appendQuad()` batch assembly
- [ ] Implement `_flushQuads()` draw call
- [ ] Test with simple rectangles (FilledRect mode)

**Test**: Colored rectangles render at correct positions

### Phase 4: Glyph Atlas (Day 7-9)

- [ ] Create glyph atlas texture (2048x2048 R8)
- [ ] Implement `_recreateGlyphAtlas()`
- [ ] Port DirectWrite glyph rasterization
- [ ] Implement `_updateGlyphAtlasRegion()`
- [ ] Test with grayscale text rendering

**Test**: ASCII text renders correctly

### Phase 5: Text Rendering (Day 10-12)

- [ ] Implement grayscale text shader path
- [ ] Implement ClearType shader path
- [ ] Add DirectWrite contrast enhancement
- [ ] Add gamma correction
- [ ] Test with various fonts

**Test**: All text rendering modes work correctly

### Phase 6: Advanced Features (Day 13-15)

- [ ] Implement builtin glyph patterns
- [ ] Add line rendering (dotted, dashed, curly)
- [ ] Implement cursor rendering
- [ ] Implement selection rendering
- [ ] Test all shading types

**Test**: All terminal features functional

### Phase 7: Optimization (Day 16-20)

- [ ] Implement state caching (`GLStateManager`)
- [ ] Add buffer triple-buffering
- [ ] Implement PBO for async texture uploads
- [ ] Profile and optimize hotspots
- [ ] Add OpenGL 4.x enhancements

**Test**: Performance within 20% of D3D12

---

## Minimal Working Example

Here's a skeleton implementation to get started:

### BackendOpenGL.cpp (Minimal)

```cpp
#include "pch.h"
#include "BackendOpenGL.h"
#include <glad/glad.h>

using namespace Microsoft::Console::Render::Atlas;

BackendOpenGL::BackendOpenGL(const RenderingPayload& p)
{
    // 1. Create OpenGL context
    _createContext(p);

    // 2. Load OpenGL functions
    _loadExtensions();

    // 3. Detect features
    _detectFeatures();

    // 4. Create resources
    _createBuffers();
    _createTextures(p);
    _compileShaders();
    _setupVertexAttributes();
    _setupBlendState();
}

BackendOpenGL::~BackendOpenGL()
{
    ReleaseResources();
}

void BackendOpenGL::ReleaseResources() noexcept
{
    // Delete all OpenGL objects
    if (_glyphAtlas) glDeleteTextures(1, &_glyphAtlas);
    if (_backgroundBitmap) glDeleteTextures(1, &_backgroundBitmap);
    if (_instanceBuffer) glDeleteBuffers(1, &_instanceBuffer);
    if (_vertexBuffer) glDeleteBuffers(1, &_vertexBuffer);
    if (_vao) glDeleteVertexArrays(1, &_vao);
    if (_shaderProgram) glDeleteProgram(_shaderProgram);

    // Destroy context
#ifdef _WIN32
    _destroyWGLContext();
#else
    _destroyGLXContext();
#endif
}

void BackendOpenGL::Render(RenderingPayload& payload)
{
    // 1. Begin frame
    _beginFrame();

    // 2. Update resources
    _updateConstantBuffers(payload);

    // 3. Render background
    _drawBackground(payload);

    // 4. Render text
    _drawText(payload);

    // 5. Render cursor and selection
    _drawCursorBackground(payload);
    _drawSelection(payload);

    // 6. End frame
    _endFrame();

    // 7. Present
    _present();
}

bool BackendOpenGL::RequiresContinuousRedraw() noexcept
{
    return _requiresContinuousRedraw;
}

void BackendOpenGL::_createContext(const RenderingPayload& p)
{
#ifdef _WIN32
    _createWGLContext(/* HWND from p.swapChain.handle */);
#else
    _createGLXContext(/* Display/Window from payload */);
#endif
}

void BackendOpenGL::_loadExtensions()
{
    if (!gladLoadGL()) {
        THROW_HR(E_FAIL);
    }

    if (!GLAD_GL_VERSION_3_3) {
        THROW_HR(E_FAIL);  // Minimum OpenGL 3.3 required
    }
}

void BackendOpenGL::_detectFeatures()
{
    glGetIntegerv(GL_MAJOR_VERSION, &_features.major_version);
    glGetIntegerv(GL_MINOR_VERSION, &_features.minor_version);

    _features.persistent_mapped_buffers = GLAD_GL_ARB_buffer_storage;
    _features.direct_state_access = GLAD_GL_ARB_direct_state_access;
    _features.multi_draw_indirect = GLAD_GL_ARB_multi_draw_indirect;
    _features.compute_shaders = GLAD_GL_VERSION_4_3;
    _features.bindless_textures = GLAD_GL_ARB_bindless_texture;
    _features.debug_output = GLAD_GL_KHR_debug;
}

void BackendOpenGL::_createBuffers()
{
    // Create VAO
    glGenVertexArrays(1, &_vao);
    glBindVertexArray(_vao);

    // Create vertex buffer (quad vertices)
    static constexpr f32x2 vertices[] = {
        { 0, 0 },  // Bottom-left
        { 1, 0 },  // Bottom-right
        { 1, 1 },  // Top-right
        { 0, 1 },  // Top-left
    };
    glGenBuffers(1, &_vertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, _vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Create instance buffer
    _instanceBufferCapacity = MaxInstances;
    glGenBuffers(1, &_instanceBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, _instanceBuffer);
    glBufferData(GL_ARRAY_BUFFER, _instanceBufferCapacity * sizeof(QuadInstance),
                 nullptr, GL_DYNAMIC_DRAW);

    // Create UBOs
    glGenBuffers(1, &_vsConstantBuffer);
    glBindBuffer(GL_UNIFORM_BUFFER, _vsConstantBuffer);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(VSConstBuffer), nullptr, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &_psConstantBuffer);
    glBindBuffer(GL_UNIFORM_BUFFER, _psConstantBuffer);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(PSConstBuffer), nullptr, GL_DYNAMIC_DRAW);
}

void BackendOpenGL::_createTextures(const RenderingPayload& p)
{
    // Create glyph atlas
    _glyphAtlasSize = { 2048, 2048 };
    glGenTextures(1, &_glyphAtlas);
    glBindTexture(GL_TEXTURE_2D, _glyphAtlas);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_R8, _glyphAtlasSize.x, _glyphAtlasSize.y);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Create background texture (will be resized as needed)
    glGenTextures(1, &_backgroundBitmap);
    glBindTexture(GL_TEXTURE_2D, _backgroundBitmap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

void BackendOpenGL::_compileShaders()
{
    // Read shader source from files
    // (In production, embed these in the binary or compile at build time)

    // Compile vertex shader
    _vertexShader = _compileShader(GL_VERTEX_SHADER, /* vertex shader source */);

    // Compile fragment shader
    _fragmentShader = _compileShader(GL_FRAGMENT_SHADER, /* fragment shader source */);

    // Link program
    _shaderProgram = _linkProgram(_vertexShader, _fragmentShader);

    // Get uniform block indices
    GLuint vsBlockIndex = glGetUniformBlockIndex(_shaderProgram, "VSConstants");
    GLuint psBlockIndex = glGetUniformBlockIndex(_shaderProgram, "PSConstants");

    // Bind to binding points
    glUniformBlockBinding(_shaderProgram, vsBlockIndex, 0);
    glUniformBlockBinding(_shaderProgram, psBlockIndex, 1);

    // Bind UBOs to binding points
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, _vsConstantBuffer);
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, _psConstantBuffer);
}

void BackendOpenGL::_setupVertexAttributes()
{
    glBindVertexArray(_vao);

    // Per-vertex attributes (from vertex buffer)
    glBindBuffer(GL_ARRAY_BUFFER, _vertexBuffer);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(f32x2), (void*)0);
    glEnableVertexAttribArray(0);

    // Per-instance attributes (from instance buffer)
    glBindBuffer(GL_ARRAY_BUFFER, _instanceBuffer);

    // shadingType (location 1)
    glVertexAttribIPointer(1, 1, GL_UNSIGNED_SHORT, sizeof(QuadInstance),
                          (void*)offsetof(QuadInstance, shadingType));
    glVertexAttribDivisor(1, 1);
    glEnableVertexAttribArray(1);

    // renditionScale (location 2)
    glVertexAttribIPointer(2, 2, GL_UNSIGNED_BYTE, sizeof(QuadInstance),
                          (void*)offsetof(QuadInstance, renditionScale));
    glVertexAttribDivisor(2, 1);
    glEnableVertexAttribArray(2);

    // position (location 3)
    glVertexAttribIPointer(3, 2, GL_SHORT, sizeof(QuadInstance),
                          (void*)offsetof(QuadInstance, position));
    glVertexAttribDivisor(3, 1);
    glEnableVertexAttribArray(3);

    // size (location 4)
    glVertexAttribIPointer(4, 2, GL_UNSIGNED_SHORT, sizeof(QuadInstance),
                          (void*)offsetof(QuadInstance, size));
    glVertexAttribDivisor(4, 1);
    glEnableVertexAttribArray(4);

    // texcoord (location 5)
    glVertexAttribIPointer(5, 2, GL_UNSIGNED_SHORT, sizeof(QuadInstance),
                          (void*)offsetof(QuadInstance, texcoord));
    glVertexAttribDivisor(5, 1);
    glEnableVertexAttribArray(5);

    // color (location 6)
    glVertexAttribPointer(6, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(QuadInstance),
                         (void*)offsetof(QuadInstance, color));
    glVertexAttribDivisor(6, 1);
    glEnableVertexAttribArray(6);
}

void BackendOpenGL::_setupBlendState()
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);  // Premultiplied alpha
    glBlendEquation(GL_FUNC_ADD);
}

void BackendOpenGL::_beginFrame()
{
    // Clear the framebuffer
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Reset instance count
    _instancesCount = 0;
}

void BackendOpenGL::_flushQuads(const RenderingPayload& p)
{
    if (_instancesCount == 0) {
        return;
    }

    // Upload instance data
    glBindBuffer(GL_ARRAY_BUFFER, _instanceBuffer);
    glBufferSubData(GL_ARRAY_BUFFER, 0, _instancesCount * sizeof(QuadInstance),
                    _instances.data());

    // Bind shader program
    glUseProgram(_shaderProgram);

    // Bind textures
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, _backgroundBitmap);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, _glyphAtlas);

    // Draw instanced quads
    glBindVertexArray(_vao);
    glDrawArraysInstanced(GL_TRIANGLE_FAN, 0, 4, _instancesCount);

    // Reset for next batch
    _instancesCount = 0;
}

void BackendOpenGL::_endFrame()
{
    // Flush any remaining instances
    _flushQuads();
}

void BackendOpenGL::_present()
{
#ifdef _WIN32
    _wglSwapBuffers();
#else
    _glxSwapBuffers();
#endif
}
```

---

## Common Pitfalls

### 1. Forgetting to Bind VAO
```cpp
// WRONG
glDrawArraysInstanced(GL_TRIANGLE_FAN, 0, 4, count);

// CORRECT
glBindVertexArray(_vao);
glDrawArraysInstanced(GL_TRIANGLE_FAN, 0, 4, count);
```

### 2. Incorrect Vertex Attribute Setup
```cpp
// WRONG: Using glVertexAttribPointer for integer types
glVertexAttribPointer(1, 1, GL_UNSIGNED_SHORT, GL_FALSE, ...);

// CORRECT: Use glVertexAttribIPointer for integer attributes
glVertexAttribIPointer(1, 1, GL_UNSIGNED_SHORT, ...);
```

### 3. Missing glVertexAttribDivisor for Instancing
```cpp
// WRONG: Divisor not set
glEnableVertexAttribArray(1);

// CORRECT: Set divisor for per-instance attributes
glVertexAttribDivisor(1, 1);  // 1 = one value per instance
glEnableVertexAttribArray(1);
```

### 4. Incorrect Uniform Buffer Alignment
```cpp
// WRONG: No alignment
struct VSConstBuffer {
    vec2 positionScale;  // May be misaligned
};

// CORRECT: Explicit alignment
struct alignas(16) VSConstBuffer {
    alignas(sizeof(f32x2)) f32x2 positionScale;  // 16-byte aligned
};
```

### 5. Premultiplied Alpha Blending
```cpp
// WRONG: Standard alpha blending
glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

// CORRECT: Premultiplied alpha (matches D3D12)
glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
```

---

## Debugging Tips

### Enable Debug Output
```cpp
if (GLAD_GL_KHR_debug) {
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(OpenGLDebugCallback, nullptr);
}
```

### Check for Errors After Each Call
```cpp
#ifdef _DEBUG
#define CHECK_GL() do { \
    GLenum err = glGetError(); \
    if (err != GL_NO_ERROR) { \
        fprintf(stderr, "OpenGL error 0x%x at %s:%d\n", err, __FILE__, __LINE__); \
        __debugbreak(); \
    } \
} while(0)
#else
#define CHECK_GL() ((void)0)
#endif
```

### Validate Shaders
```cpp
void ValidateShader(GLuint shader) {
    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (!status) {
        GLint length;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        char* log = new char[length];
        glGetShaderInfoLog(shader, length, nullptr, log);
        fprintf(stderr, "Shader compilation failed:\n%s\n", log);
        delete[] log;
    }
}
```

### Profile with Timer Queries
```cpp
GLuint query;
glGenQueries(1, &query);
glBeginQuery(GL_TIME_ELAPSED, query);
// ... rendering code ...
glEndQuery(GL_TIME_ELAPSED);

GLint64 elapsed;
glGetQueryObjecti64v(query, GL_QUERY_RESULT, &elapsed);
printf("Render time: %.2f ms\n", elapsed / 1000000.0);
```

---

## Testing Procedure

### Step 1: Minimal Test
```cpp
// Test: Render single solid quad
QuadInstance quad = {
    .shadingType = (u16)ShadingType::FilledRect,
    .renditionScale = { 1, 1 },
    .position = { 100, 100 },
    .size = { 200, 100 },
    .texcoord = { 0, 0 },
    .color = 0xFF0000FF  // Red
};
_instances.push_back(quad);
_instancesCount = 1;
_flushQuads();
```

**Expected**: Red rectangle at (100, 100) with size 200x100

### Step 2: Batch Test
```cpp
// Test: Render 100 quads in different colors
for (int i = 0; i < 100; ++i) {
    QuadInstance quad = {
        .shadingType = (u16)ShadingType::FilledRect,
        .renditionScale = { 1, 1 },
        .position = { (i16)(i * 10), 100 },
        .size = { 8, 50 },
        .texcoord = { 0, 0 },
        .color = (u32)(0xFF000000 | (i * 2 << 16))  // Gradient
    };
    _appendQuad(quad);
}
_flushQuads();
```

**Expected**: Horizontal gradient bar

### Step 3: Text Test
```cpp
// Test: Render "Hello" using glyph atlas
// (Requires glyph rasterization and atlas upload)
```

---

## Performance Targets

| Configuration | Target Frame Time | Target FPS | Max CPU % | Max GPU % |
|--------------|------------------|-----------|-----------|-----------|
| 1080p (239x67) | <5ms | 200+ | 8% | 15% |
| 1440p (319x89) | <7ms | 140+ | 10% | 20% |
| 4K (479x134) | <12ms | 80+ | 15% | 30% |

---

## Resources

### OpenGL References
- [OpenGL 3.3 Core Specification](https://www.khronos.org/registry/OpenGL/specs/gl/glspec33.core.pdf)
- [GLSL 3.30 Specification](https://www.khronos.org/registry/OpenGL/specs/gl/GLSLangSpec.3.30.pdf)
- [OpenGL API Documentation](https://docs.gl/)

### Learning Resources
- [Learn OpenGL](https://learnopengl.com/)
- [OpenGL SuperBible](https://www.openglsuperbible.com/)
- [OpenGL Best Practices](https://www.khronos.org/opengl/wiki/Performance)

### Tools
- [RenderDoc](https://renderdoc.org/) - OpenGL debugging and profiling
- [NVIDIA Nsight Graphics](https://developer.nvidia.com/nsight-graphics) - Advanced profiling
- [glslangValidator](https://github.com/KhronosGroup/glslang) - GLSL shader validation

---

## Next Steps

1. Start with Phase 1 (Basic Setup)
2. Test on Windows first, then Linux
3. Add WSL2 testing once basic rendering works
4. Profile early and often
5. Document any driver quirks or workarounds

**Good luck with the implementation!**