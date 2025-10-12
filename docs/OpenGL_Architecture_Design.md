# OpenGL 3.3+ Backend Architecture for Windows Terminal

## Executive Summary

This document outlines the design and implementation strategy for an OpenGL 3.3+ backend as a cross-platform fallback renderer for the Ultra-Riced Windows Terminal. The OpenGL backend provides compatibility with systems that lack Direct3D 11/12 support, including Linux via WSL2, older Windows systems, and potential future cross-platform deployments.

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Feature Mapping: D3D12 to OpenGL](#feature-mapping-d3d12-to-opengl)
3. [Implementation Strategy](#implementation-strategy)
4. [Performance Optimization](#performance-optimization)
5. [Platform-Specific Considerations](#platform-specific-considerations)
6. [Shader Architecture](#shader-architecture)
7. [Resource Management](#resource-management)
8. [Timeline and Milestones](#timeline-and-milestones)

---

## Architecture Overview

The OpenGL backend implements the `IBackend` interface, maintaining feature parity with D3D11/D3D12 backends while leveraging OpenGL-specific optimizations:

### Core Design Principles

1. **Baseline: OpenGL 3.3 Core Profile**
   - Maximum compatibility across platforms
   - Available on Windows Vista+, Linux, macOS (deprecated but functional)
   - Supports all required rendering features

2. **Progressive Enhancement**
   - Detect and use OpenGL 4.x features when available
   - Runtime feature detection for optimal performance
   - Graceful fallback for older hardware

3. **Batch-Oriented Rendering**
   - Instance rendering for up to 65,536 quads per draw call
   - Minimize state changes and draw calls
   - Efficient texture atlas management

### Rendering Pipeline

```
Input Processing -> Batch Assembly -> State Setup -> Draw Calls -> Present
       |                |                |              |            |
   Text Shaping    Instance Buffer   PSO Mapping    Instanced    SwapBuffers
   Glyph Cache     Texture Updates   UBO Updates     Rendering
```

---

## Feature Mapping: D3D12 to OpenGL

### Core Components

| D3D12 Feature | OpenGL 3.3 Equivalent | OpenGL 4.x Enhancement |
|--------------|----------------------|------------------------|
| **Device & Context** | | |
| ID3D12Device | OpenGL Context | - |
| ID3D12CommandQueue | Immediate Mode | Command Buffers (4.4+) |
| ID3D12CommandList | glDrawElementsInstanced | glMultiDrawElementsIndirect (4.3+) |
| **Resources** | | |
| ID3D12Resource (Buffer) | VBO/IBO | Persistent Mapped Buffers (4.4+) |
| ID3D12Resource (Texture) | glTexture2D | Sparse Textures (4.3+) |
| Constant Buffer | Uniform Buffer Object | - |
| **Pipeline State** | | |
| Root Signature | UBO Binding Points | Bindless (4.4+) |
| Pipeline State Object | Shader Program + State | Separate Shader Objects (4.1+) |
| Input Layout | VAO + glVertexAttribPointer | - |
| **Synchronization** | | |
| ID3D12Fence | glFenceSync | - |
| Resource Barriers | glMemoryBarrier | - |
| **Descriptors** | | |
| Descriptor Heap | Texture Units + UBO Indices | Bindless Textures (4.4+) |
| CBV/SRV/UAV | glBindTexture/glBindBufferBase | Direct State Access (4.5+) |
| **Swap Chain** | | |
| IDXGISwapChain | WGL/GLX SwapBuffers | - |
| Present | SwapBuffers | - |

### Shader Stage Mapping

| HLSL Component | GLSL 330 Equivalent | Notes |
|---------------|-------------------|--------|
| cbuffer | uniform block | 16-byte alignment required |
| Texture2D | sampler2D | Combined sampler/texture |
| SV_Position | gl_Position | Built-in variable |
| SV_InstanceID | gl_InstanceID | Built-in variable |
| semantics | layout qualifiers | Explicit locations |
| float4x4 | mat4 | Column-major by default |

### Rendering Features

| Feature | OpenGL Implementation | Performance Impact |
|---------|---------------------|-------------------|
| Instanced Rendering | glDrawElementsInstanced | Baseline |
| Glyph Atlas | R8 texture + glTexSubImage2D | PBO for async upload |
| Background Bitmap | RGBA8 texture array | Single texture bind |
| Blending | glBlendFunc/glBlendEquation | State cached |
| Scissor Test | glScissor | Dirty region culling |

---

## Implementation Strategy

### Phase 1: Core Infrastructure (Week 1)

1. **Context Creation**
   ```cpp
   // Windows: WGL
   HGLRC CreateWGLContext(HDC hdc) {
       PIXELFORMATDESCRIPTOR pfd = {
           .dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
           .iPixelType = PFD_TYPE_RGBA,
           .cColorBits = 32,
           .cDepthBits = 0,  // No depth buffer needed
           .iLayerType = PFD_MAIN_PLANE
       };
       // Set pixel format, create context, make current
   }

   // Linux: GLX
   GLXContext CreateGLXContext(Display* display, Window window) {
       // Use glXChooseFBConfig for modern context creation
       // Request OpenGL 3.3 Core Profile
   }
   ```

2. **Extension Loading**
   - Use GLAD or GLEW for extension management
   - Runtime feature detection for 4.x capabilities

3. **Basic Resource Management**
   - VBO/VAO creation for quad vertices
   - Instance buffer allocation
   - Texture creation for glyph atlas

### Phase 2: Shader System (Week 2)

1. **GLSL Shader Templates**
   - Port HLSL shaders to GLSL 3.30
   - Implement uniform buffer layout
   - Handle texture sampling

2. **Shader Compilation Pipeline**
   ```cpp
   class ShaderCompiler {
       GLuint CompileShader(const char* source, GLenum type);
       GLuint LinkProgram(GLuint vs, GLuint fs);
       void ValidateProgram(GLuint program);
   };
   ```

### Phase 3: Batch Rendering (Week 3)

1. **Instance Buffer Management**
   ```cpp
   class InstanceBuffer {
       GLuint vbo;
       QuadInstance* mapped_ptr;  // Persistent mapping if available
       u32 capacity = 65536;

       void UpdateInstances(const QuadInstance* data, u32 count) {
           if (has_persistent_mapping) {
               memcpy(mapped_ptr, data, count * sizeof(QuadInstance));
               glFlushMappedBufferRange(...);
           } else {
               glBufferSubData(GL_ARRAY_BUFFER, 0, size, data);
           }
       }
   };
   ```

2. **Draw Call Batching**
   - Group instances by shading type
   - Minimize state changes between batches

### Phase 4: Texture Management (Week 4)

1. **Glyph Atlas**
   ```cpp
   class GlyphAtlas {
       GLuint texture;
       u32 width = 2048, height = 2048;

       void Initialize() {
           glGenTextures(1, &texture);
           glBindTexture(GL_TEXTURE_2D, texture);
           glTexStorage2D(GL_TEXTURE_2D, 1, GL_R8, width, height);
           glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
       }

       void UpdateRegion(u32 x, u32 y, u32 w, u32 h, const u8* data) {
           glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h,
                          GL_RED, GL_UNSIGNED_BYTE, data);
       }
   };
   ```

2. **Background/Foreground Bitmaps**
   - Use texture arrays or separate textures
   - Implement efficient update patterns

### Phase 5: Optimization (Week 5)

1. **OpenGL 4.x Features**
   - Direct State Access (DSA) for reduced overhead
   - Persistent mapped buffers for CPU-GPU streaming
   - Bindless textures for unlimited texture access

2. **Performance Profiling**
   - GPU timer queries for draw call timing
   - CPU profiling for batch assembly
   - Memory usage optimization

---

## Performance Optimization

### OpenGL 3.3 Baseline Optimizations

1. **State Management**
   ```cpp
   class GLStateCache {
       struct State {
           GLuint program = 0;
           GLuint vao = 0;
           GLuint textures[16] = {};
           GLuint ubos[8] = {};
           GLenum blend_src = GL_ONE;
           GLenum blend_dst = GL_ZERO;
       } current, desired;

       void Apply() {
           if (current.program != desired.program) {
               glUseProgram(desired.program);
               current.program = desired.program;
           }
           // Similar for other state...
       }
   };
   ```

2. **Buffer Streaming**
   - Triple buffer instance data
   - Use GL_DYNAMIC_DRAW for frequently updated buffers
   - Orphan buffers to avoid synchronization

### OpenGL 4.x Enhancements

1. **Persistent Mapped Buffers (4.4+)**
   ```cpp
   void* ptr = glMapBufferRange(GL_ARRAY_BUFFER, 0, size,
                                GL_MAP_WRITE_BIT |
                                GL_MAP_PERSISTENT_BIT |
                                GL_MAP_COHERENT_BIT);
   ```

2. **Multi-Draw Indirect (4.3+)**
   ```cpp
   struct DrawCommand {
       GLuint count;
       GLuint instanceCount;
       GLuint first;
       GLuint baseInstance;
   };

   glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_SHORT,
                               commands, drawCount, 0);
   ```

3. **Compute Shaders (4.3+)**
   - Grid generation on GPU
   - Glyph rasterization acceleration

### Expected Performance Metrics

| Metric | OpenGL 3.3 | OpenGL 4.5 | D3D12 |
|--------|------------|------------|-------|
| Draw Calls/Frame | 10-20 | 5-10 | 3-5 |
| CPU Usage | 5-8% | 3-5% | 2-3% |
| GPU Usage | 15-20% | 12-15% | 10-12% |
| Frame Time (144Hz) | 5-6ms | 4-5ms | 3-4ms |
| Memory Usage | 150MB | 140MB | 130MB |

---

## Platform-Specific Considerations

### Windows (WGL)

```cpp
class WGLContext {
    HGLRC context;
    HDC hdc;

    void CreateModernContext() {
        // Use wglCreateContextAttribsARB for Core Profile
        int attribs[] = {
            WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
            WGL_CONTEXT_MINOR_VERSION_ARB, 3,
            WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
            0
        };
        context = wglCreateContextAttribsARB(hdc, 0, attribs);
    }

    void EnableVSync(bool enable) {
        if (wglSwapIntervalEXT) {
            wglSwapIntervalEXT(enable ? 1 : 0);
        }
    }
};
```

### Linux (GLX)

```cpp
class GLXContext {
    GLXContext context;
    Display* display;
    Window window;

    void CreateModernContext() {
        int attribs[] = {
            GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
            GLX_CONTEXT_MINOR_VERSION_ARB, 3,
            GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
            None
        };
        context = glXCreateContextAttribsARB(display, fbconfig, 0, True, attribs);
    }
};
```

### WSL2 Considerations

1. **Mesa Driver Support**
   - Requires Mesa 18.0+ for OpenGL 3.3
   - Check for llvmpipe (software) vs hardware acceleration

2. **X11 Forwarding**
   ```bash
   # Enable GLX support
   export LIBGL_ALWAYS_INDIRECT=0
   export MESA_GL_VERSION_OVERRIDE=3.3
   ```

3. **Performance Tuning**
   - Use VcXsrv or WSLg for best performance
   - Enable GPU acceleration if available

### macOS (Deprecated Path)

- OpenGL deprecated since macOS 10.14
- Maximum support: OpenGL 4.1 Core Profile
- Consider Metal backend for future macOS support

---

## Shader Architecture

### Vertex Shader (GLSL 3.30)

```glsl
#version 330 core

// Vertex attributes
layout(location = 0) in vec2 in_vertex;

// Instance attributes
layout(location = 1) in uint in_shadingType;
layout(location = 2) in uvec2 in_renditionScale;
layout(location = 3) in ivec2 in_position;
layout(location = 4) in uvec2 in_size;
layout(location = 5) in uvec2 in_texcoord;
layout(location = 6) in vec4 in_color;

// Uniform buffer
layout(std140) uniform VSConstants {
    vec2 positionScale;
} vs_cb;

// Outputs to fragment shader
out vec2 texcoord;
flat out uint shadingType;
flat out vec2 renditionScale;
flat out vec4 color;

void main() {
    // Transform from pixel space to NDC
    vec2 pos = (in_position + in_vertex * in_size) * vs_cb.positionScale + vec2(-1.0, 1.0);
    gl_Position = vec4(pos, 0.0, 1.0);

    // Pass through to fragment shader
    texcoord = in_texcoord + in_vertex * in_size;
    shadingType = in_shadingType;
    renditionScale = vec2(in_renditionScale);
    color = in_color;
}
```

### Fragment Shader (GLSL 3.30)

```glsl
#version 330 core

// Shading type constants
#define SHADING_TYPE_TEXT_BACKGROUND    0
#define SHADING_TYPE_TEXT_GRAYSCALE     1
#define SHADING_TYPE_TEXT_CLEARTYPE     2
#define SHADING_TYPE_TEXT_BUILTIN_GLYPH 3
#define SHADING_TYPE_TEXT_PASSTHROUGH   4
#define SHADING_TYPE_DOTTED_LINE        5
#define SHADING_TYPE_DASHED_LINE        6
#define SHADING_TYPE_CURLY_LINE         7
#define SHADING_TYPE_SOLID_LINE         8
#define SHADING_TYPE_CURSOR             9
#define SHADING_TYPE_FILLED_RECT       10

// Inputs from vertex shader
in vec2 texcoord;
flat in uint shadingType;
flat in vec2 renditionScale;
flat in vec4 color;

// Uniform buffer
layout(std140) uniform PSConstants {
    vec4 backgroundColor;
    vec2 backgroundCellSize;
    vec2 backgroundCellCount;
    vec4 gammaRatios;
    float enhancedContrast;
    float underlineWidth;
    float doubleUnderlineWidth;
    float curlyLineHalfHeight;
    float shadedGlyphDotSize;
} ps_cb;

// Textures
uniform sampler2D background;
uniform sampler2D glyphAtlas;

// Output
out vec4 fragColor;

vec4 premultiplyColor(vec4 c) {
    return vec4(c.rgb * c.a, c.a);
}

vec4 alphaBlendPremultiplied(vec4 bottom, vec4 top) {
    return bottom * (1.0 - top.a) + top;
}

// DWrite emulation functions
float DWrite_CalcColorIntensity(vec3 color) {
    return dot(color, vec3(0.299, 0.587, 0.114));
}

float DWrite_EnhanceContrast(float alpha, float enhancedContrast) {
    float ec_half = enhancedContrast * 0.5;
    float ec_half_sqr = ec_half * ec_half;
    float alpha_half = alpha - 0.5;
    if (alpha_half > 0.0) {
        return alpha_half / sqrt(ec_half_sqr + alpha_half * alpha_half) * ec_half + 0.5;
    } else {
        return -alpha_half / sqrt(ec_half_sqr + alpha_half * alpha_half) * ec_half + 0.5;
    }
}

void main() {
    vec4 outputColor;

    switch(shadingType) {
        case SHADING_TYPE_TEXT_BACKGROUND: {
            vec2 cell = gl_FragCoord.xy / ps_cb.backgroundCellSize;
            if (all(lessThan(cell, ps_cb.backgroundCellCount))) {
                outputColor = texture(background, cell / ps_cb.backgroundCellCount);
            } else {
                outputColor = ps_cb.backgroundColor;
            }
            break;
        }

        case SHADING_TYPE_TEXT_GRAYSCALE: {
            vec4 foreground = premultiplyColor(color);
            float glyph = texture(glyphAtlas, texcoord).r;
            float intensity = DWrite_CalcColorIntensity(color.rgb);
            float contrasted = DWrite_EnhanceContrast(glyph, ps_cb.enhancedContrast);
            // Simplified gamma correction
            float alphaCorrected = pow(contrasted, 1.0 / 2.2) * intensity;
            outputColor = foreground * alphaCorrected;
            break;
        }

        case SHADING_TYPE_TEXT_CLEARTYPE: {
            vec3 glyph = texture(glyphAtlas, texcoord).rgb;
            vec3 contrasted = vec3(
                DWrite_EnhanceContrast(glyph.r, ps_cb.enhancedContrast),
                DWrite_EnhanceContrast(glyph.g, ps_cb.enhancedContrast),
                DWrite_EnhanceContrast(glyph.b, ps_cb.enhancedContrast)
            );
            // Simplified ClearType blending
            vec3 alphaCorrected = pow(contrasted, vec3(1.0 / 2.2));
            outputColor = vec4(color.rgb * alphaCorrected, dot(alphaCorrected, vec3(0.333)));
            break;
        }

        case SHADING_TYPE_TEXT_BUILTIN_GLYPH: {
            vec4 glyph = texture(glyphAtlas, texcoord);
            vec2 pos = floor(gl_FragCoord.xy / (ps_cb.shadedGlyphDotSize * renditionScale));

            // Checkerboard pattern generation
            float stretched = step(fract(dot(pos, vec2(glyph.r * -0.25 + 0.5, 0.5))), 0.0) * glyph.a;
            float inverted = abs(glyph.g - stretched);
            float filled = max(glyph.b, inverted);

            outputColor = premultiplyColor(color) * filled;
            break;
        }

        case SHADING_TYPE_SOLID_LINE:
        case SHADING_TYPE_CURSOR:
        case SHADING_TYPE_FILLED_RECT: {
            outputColor = premultiplyColor(color);
            break;
        }

        case SHADING_TYPE_DOTTED_LINE: {
            float pattern = step(0.5, fract(gl_FragCoord.x * 0.25));
            outputColor = premultiplyColor(color) * pattern;
            break;
        }

        case SHADING_TYPE_DASHED_LINE: {
            float pattern = step(0.5, fract(gl_FragCoord.x * 0.125));
            outputColor = premultiplyColor(color) * pattern;
            break;
        }

        case SHADING_TYPE_CURLY_LINE: {
            float offset = texcoord.y - 0.5;
            float curve = sin(texcoord.x * 3.14159 * 4.0) * ps_cb.curlyLineHalfHeight;
            float dist = abs(offset - curve);
            float alpha = 1.0 - smoothstep(0.0, ps_cb.underlineWidth, dist);
            outputColor = premultiplyColor(color) * alpha;
            break;
        }

        default: {
            outputColor = vec4(1.0, 0.0, 1.0, 1.0); // Magenta for debugging
            break;
        }
    }

    fragColor = outputColor;
}
```

---

## Resource Management

### Buffer Management Strategy

```cpp
class BufferManager {
    struct BufferPool {
        std::vector<GLuint> free_buffers;
        std::vector<GLuint> in_use;

        GLuint Acquire(size_t size) {
            GLuint buffer;
            if (!free_buffers.empty()) {
                buffer = free_buffers.back();
                free_buffers.pop_back();
            } else {
                glGenBuffers(1, &buffer);
            }
            glBindBuffer(GL_ARRAY_BUFFER, buffer);
            glBufferData(GL_ARRAY_BUFFER, size, nullptr, GL_DYNAMIC_DRAW);
            in_use.push_back(buffer);
            return buffer;
        }

        void Release(GLuint buffer) {
            auto it = std::find(in_use.begin(), in_use.end(), buffer);
            if (it != in_use.end()) {
                in_use.erase(it);
                free_buffers.push_back(buffer);
            }
        }
    };

    BufferPool vertex_buffers;
    BufferPool instance_buffers;
    BufferPool uniform_buffers;
};
```

### Texture Atlas Management

```cpp
class TextureAtlasManager {
    struct AtlasNode {
        u16 x, y, width, height;
        bool occupied;
    };

    GLuint atlas_texture;
    std::vector<AtlasNode> nodes;
    stbrp_context rect_packer;

    bool AllocateRegion(u16 width, u16 height, u16& out_x, u16& out_y) {
        stbrp_rect rect = { 0, width, height };
        if (stbrp_pack_rects(&rect_packer, &rect, 1)) {
            out_x = rect.x;
            out_y = rect.y;
            return true;
        }
        return false;
    }

    void Defragment() {
        // Repack all glyphs to minimize fragmentation
        // This is done during idle time or when allocation fails
    }
};
```

### Memory Budget

| Resource Type | OpenGL 3.3 | OpenGL 4.5 | Notes |
|--------------|------------|------------|-------|
| Glyph Atlas | 16MB (2048x2048xR8) | 32MB (4096x4096xR8) | Sparse texture support |
| Instance Buffer | 4MB (65536 instances) | 4MB | Persistent mapping |
| Constant Buffers | 64KB | 64KB | Per-frame updates |
| Vertex Buffers | 1KB | 1KB | Static quad vertices |
| Background Bitmaps | 8MB | 8MB | Viewport-sized RGBA |
| Total VRAM | ~30MB | ~45MB | Conservative estimate |

---

## Timeline and Milestones

### Development Schedule (6 Weeks Total)

#### Week 1: Foundation
- [ ] Context creation (WGL/GLX)
- [ ] Extension loading and feature detection
- [ ] Basic VAO/VBO setup
- [ ] Shader compilation infrastructure

#### Week 2: Core Rendering
- [ ] Port vertex/fragment shaders to GLSL
- [ ] Implement uniform buffer updates
- [ ] Basic instanced rendering
- [ ] State management cache

#### Week 3: Texture System
- [ ] Glyph atlas creation and management
- [ ] Background bitmap rendering
- [ ] Texture update optimization (PBO)
- [ ] Mipmap generation for scaling

#### Week 4: Advanced Features
- [ ] ClearType text rendering
- [ ] Built-in glyph patterns
- [ ] Line rendering (dotted, dashed, curly)
- [ ] Cursor and selection rendering

#### Week 5: Optimization
- [ ] OpenGL 4.x feature detection and usage
- [ ] Persistent mapped buffers
- [ ] Direct State Access
- [ ] Multi-draw indirect

#### Week 6: Platform Integration
- [ ] Windows WGL integration
- [ ] Linux GLX/EGL support
- [ ] WSL2 testing and optimization
- [ ] Performance profiling and tuning

### Validation Criteria

1. **Functional Requirements**
   - [ ] Renders all text correctly
   - [ ] Supports all shading types
   - [ ] Handles viewport resizing
   - [ ] Maintains 60+ FPS

2. **Performance Requirements**
   - [ ] <10ms frame time at 1080p
   - [ ] <20 draw calls per frame
   - [ ] <5% CPU usage
   - [ ] <150MB memory usage

3. **Compatibility Requirements**
   - [ ] Windows 7+ support
   - [ ] Linux with Mesa 18.0+
   - [ ] WSL2 with GPU acceleration
   - [ ] Graceful fallback for missing features

### Risk Mitigation

| Risk | Probability | Impact | Mitigation |
|------|------------|--------|------------|
| Driver bugs | Medium | High | Extensive testing, workarounds |
| Performance regression | Low | High | Profile early and often |
| Platform incompatibility | Low | Medium | CI/CD testing matrix |
| Shader compilation failures | Low | Low | Fallback shaders |

---

## Conclusion

The OpenGL 3.3+ backend provides a robust, cross-platform rendering solution that maintains feature parity with the D3D12 backend while ensuring broad compatibility. By targeting OpenGL 3.3 as the baseline with progressive enhancement for 4.x features, we achieve:

1. **Universal Compatibility**: Runs on Windows Vista+, Linux, and legacy macOS
2. **High Performance**: 65K instance batching, optimized state management
3. **Feature Parity**: All text rendering features, effects, and optimizations
4. **Future-Proof**: Ready for OpenGL 4.x and potential Vulkan migration

The implementation follows a phased approach with clear milestones, ensuring systematic development and thorough testing at each stage. The architecture is designed for maintainability and performance, with careful attention to platform-specific requirements and optimization opportunities.