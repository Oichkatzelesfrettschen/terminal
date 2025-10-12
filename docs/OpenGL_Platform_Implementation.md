# OpenGL Platform-Specific Implementation Guide

## Overview

This document provides detailed implementation guidance for the OpenGL backend across different platforms. Each platform has unique requirements for context creation, extension loading, and window system integration.

## Table of Contents

1. [Windows (WGL)](#windows-wgl)
2. [Linux (GLX)](#linux-glx)
3. [WSL2 Considerations](#wsl2-considerations)
4. [Extension Loading](#extension-loading)
5. [Error Handling](#error-handling)
6. [Performance Tuning](#performance-tuning)

---

## Windows (WGL)

### Context Creation

Windows uses WGL (Windows OpenGL) for OpenGL integration. Modern context creation requires the `WGL_ARB_create_context` extension.

#### Step 1: Create a Temporary Context for Extension Loading

```cpp
#include <Windows.h>
#include <GL/gl.h>
#include <GL/wglext.h>

bool CreateWGLContext(HWND hwnd, HDC& hdc, HGLRC& context)
{
    hdc = GetDC(hwnd);
    if (!hdc) {
        return false;
    }

    // Step 1: Set up a basic pixel format
    PIXELFORMATDESCRIPTOR pfd = {
        .nSize = sizeof(PIXELFORMATDESCRIPTOR),
        .nVersion = 1,
        .dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        .iPixelType = PFD_TYPE_RGBA,
        .cColorBits = 32,
        .cRedBits = 0,
        .cRedShift = 0,
        .cGreenBits = 0,
        .cGreenShift = 0,
        .cBlueBits = 0,
        .cBlueShift = 0,
        .cAlphaBits = 8,
        .cAlphaShift = 0,
        .cAccumBits = 0,
        .cAccumRedBits = 0,
        .cAccumGreenBits = 0,
        .cAccumBlueBits = 0,
        .cAccumAlphaBits = 0,
        .cDepthBits = 0,        // No depth buffer needed for 2D rendering
        .cStencilBits = 0,      // No stencil buffer needed
        .cAuxBuffers = 0,
        .iLayerType = PFD_MAIN_PLANE,
        .bReserved = 0,
        .dwLayerMask = 0,
        .dwVisibleMask = 0,
        .dwDamageMask = 0
    };

    int pixelFormat = ChoosePixelFormat(hdc, &pfd);
    if (!pixelFormat) {
        return false;
    }

    if (!SetPixelFormat(hdc, pixelFormat, &pfd)) {
        return false;
    }

    // Step 2: Create a temporary legacy context to load extensions
    HGLRC tempContext = wglCreateContext(hdc);
    if (!tempContext) {
        return false;
    }

    wglMakeCurrent(hdc, tempContext);

    // Step 3: Load wglCreateContextAttribsARB function
    PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB =
        (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");

    if (!wglCreateContextAttribsARB) {
        // Fallback to legacy context if extension not available
        context = tempContext;
        return true;
    }

    // Step 4: Create a modern OpenGL 3.3 Core Profile context
    const int attribs[] = {
        WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
        WGL_CONTEXT_MINOR_VERSION_ARB, 3,
        WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
#ifdef _DEBUG
        WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_DEBUG_BIT_ARB,
#endif
        0
    };

    context = wglCreateContextAttribsARB(hdc, nullptr, attribs);
    if (!context) {
        // Fallback to legacy context
        context = tempContext;
        return true;
    }

    // Switch to the new context and delete the temporary one
    wglMakeCurrent(hdc, context);
    wglDeleteContext(tempContext);

    return true;
}
```

#### Step 2: Enable VSync Control

```cpp
bool SetVSyncWGL(bool enable)
{
    // Load WGL_EXT_swap_control extension
    PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT =
        (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");

    if (!wglSwapIntervalEXT) {
        return false;
    }

    // 0 = vsync off, 1 = vsync on, -1 = adaptive vsync (if supported)
    return wglSwapIntervalEXT(enable ? 1 : 0) == TRUE;
}
```

#### Step 3: Buffer Swap

```cpp
void SwapBuffersWGL(HDC hdc)
{
    SwapBuffers(hdc);
}
```

#### Step 4: Cleanup

```cpp
void DestroyWGLContext(HDC hdc, HGLRC context)
{
    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(context);
}
```

### Windows-Specific Considerations

1. **Pixel Format Selection**
   - Use `wglChoosePixelFormatARB` for advanced pixel format selection
   - Ensure sRGB framebuffer if needed: `WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB`

2. **Multi-Monitor Support**
   - Use `wglGetPixelFormatAttribivARB` to query capabilities
   - Handle DPI scaling correctly for high-DPI displays

3. **Performance**
   - Windows 10+ has better OpenGL driver support
   - Consider using ANGLE (OpenGL ES over D3D11) for better compatibility

---

## Linux (GLX)

### Context Creation

Linux uses GLX (OpenGL Extension to the X Window System) for OpenGL integration.

#### Step 1: Choose Frame Buffer Configuration

```cpp
#include <X11/Xlib.h>
#include <GL/glx.h>
#include <GL/glxext.h>

bool CreateGLXContext(Display* display, Window window, GLXContext& context)
{
    // Step 1: Check for GLX support
    int glxMajor, glxMinor;
    if (!glXQueryVersion(display, &glxMajor, &glxMinor)) {
        return false;
    }

    if (glxMajor < 1 || (glxMajor == 1 && glxMinor < 3)) {
        // Need at least GLX 1.3 for modern context creation
        return false;
    }

    // Step 2: Choose a suitable frame buffer configuration
    const int visualAttribs[] = {
        GLX_X_RENDERABLE,    True,
        GLX_DRAWABLE_TYPE,   GLX_WINDOW_BIT,
        GLX_RENDER_TYPE,     GLX_RGBA_BIT,
        GLX_X_VISUAL_TYPE,   GLX_TRUE_COLOR,
        GLX_RED_SIZE,        8,
        GLX_GREEN_SIZE,      8,
        GLX_BLUE_SIZE,       8,
        GLX_ALPHA_SIZE,      8,
        GLX_DEPTH_SIZE,      0,      // No depth buffer needed
        GLX_STENCIL_SIZE,    0,      // No stencil buffer needed
        GLX_DOUBLEBUFFER,    True,
        None
    };

    int fbCount;
    GLXFBConfig* fbc = glXChooseFBConfig(display, DefaultScreen(display),
                                         visualAttribs, &fbCount);
    if (!fbc || fbCount == 0) {
        return false;
    }

    // Pick the best configuration (first one is usually best)
    GLXFBConfig bestFbc = fbc[0];
    XFree(fbc);

    // Step 3: Load glXCreateContextAttribsARB extension
    PFNGLXCREATECONTEXTATTRIBSARBPROC glXCreateContextAttribsARB =
        (PFNGLXCREATECONTEXTATTRIBSARBPROC)glXGetProcAddressARB(
            (const GLubyte*)"glXCreateContextAttribsARB");

    if (!glXCreateContextAttribsARB) {
        // Fallback to legacy context creation
        XVisualInfo* vi = glXGetVisualFromFBConfig(display, bestFbc);
        context = glXCreateContext(display, vi, nullptr, GL_TRUE);
        XFree(vi);
        return context != nullptr;
    }

    // Step 4: Create a modern OpenGL 3.3 Core Profile context
    const int contextAttribs[] = {
        GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
        GLX_CONTEXT_MINOR_VERSION_ARB, 3,
        GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
#ifdef _DEBUG
        GLX_CONTEXT_FLAGS_ARB, GLX_CONTEXT_DEBUG_BIT_ARB,
#endif
        None
    };

    context = glXCreateContextAttribsARB(display, bestFbc, nullptr, True, contextAttribs);
    if (!context) {
        return false;
    }

    // Step 5: Make the context current
    glXMakeCurrent(display, window, context);

    return true;
}
```

#### Step 2: Enable VSync Control

```cpp
bool SetVSyncGLX(Display* display, Window window, bool enable)
{
    // Try GLX_EXT_swap_control first (most common)
    PFNGLXSWAPINTERVALEXTPROC glXSwapIntervalEXT =
        (PFNGLXSWAPINTERVALEXTPROC)glXGetProcAddressARB(
            (const GLubyte*)"glXSwapIntervalEXT");

    if (glXSwapIntervalEXT) {
        glXSwapIntervalEXT(display, window, enable ? 1 : 0);
        return true;
    }

    // Try GLX_MESA_swap_control as fallback
    PFNGLXSWAPINTERVALMESAPROC glXSwapIntervalMESA =
        (PFNGLXSWAPINTERVALMESAPROC)glXGetProcAddressARB(
            (const GLubyte*)"glXSwapIntervalMESA");

    if (glXSwapIntervalMESA) {
        glXSwapIntervalMESA(enable ? 1 : 0);
        return true;
    }

    return false;
}
```

#### Step 3: Buffer Swap

```cpp
void SwapBuffersGLX(Display* display, Window window)
{
    glXSwapBuffers(display, window);
}
```

#### Step 4: Cleanup

```cpp
void DestroyGLXContext(Display* display, GLXContext context)
{
    glXMakeCurrent(display, None, nullptr);
    glXDestroyContext(display, context);
}
```

### Linux-Specific Considerations

1. **Mesa Drivers**
   - Ensure Mesa 18.0+ for proper OpenGL 3.3 support
   - Check `glxinfo` for available OpenGL version:
     ```bash
     glxinfo | grep "OpenGL version"
     ```

2. **Wayland Support**
   - Use EGL instead of GLX for native Wayland support
   - GLX still works via XWayland compatibility layer

3. **Multi-GPU Systems**
   - Use `DRI_PRIME=1` environment variable to select discrete GPU
   - Example:
     ```bash
     DRI_PRIME=1 ./windows-terminal
     ```

---

## WSL2 Considerations

### Overview

WSL2 (Windows Subsystem for Linux 2) presents unique challenges for OpenGL rendering:
- No native GPU pass-through in older WSL2 versions
- Requires X11 server on Windows side
- WSLg (Windows Subsystem for Linux GUI) provides native GPU acceleration

### Option 1: WSLg (Recommended for Windows 11)

Windows 11 includes WSLg with native GPU acceleration:

```bash
# Check if WSLg is available
echo $DISPLAY
# Should output: :0

# Test OpenGL support
glxinfo | grep "OpenGL version"
# Should show hardware-accelerated renderer
```

**No additional configuration needed** - WSLg automatically provides:
- OpenGL 4.6+ support (depending on Windows GPU driver)
- Direct GPU access via DX12 interop
- Native Wayland support

### Option 2: VcXsrv (For Windows 10 or older WSL2)

Install and configure VcXsrv on Windows:

1. **Download and Install VcXsrv**
   - https://sourceforge.net/projects/vcxsrv/

2. **Launch VcXsrv with OpenGL support**
   ```
   vcxsrv.exe :0 -multiwindow -clipboard -wgl
   ```

3. **Configure WSL2 to use VcXsrv**
   ```bash
   # Add to ~/.bashrc or ~/.zshrc
   export DISPLAY=$(cat /etc/resolv.conf | grep nameserver | awk '{print $2}'):0
   export LIBGL_ALWAYS_INDIRECT=0  # Enable direct rendering
   ```

4. **Allow X11 connections in Windows Firewall**
   - Add inbound rule for VcXsrv on private networks

### Performance Optimization for WSL2

```bash
# Force OpenGL version override if needed
export MESA_GL_VERSION_OVERRIDE=3.3

# Disable vsync for better performance (if needed)
export vblank_mode=0

# Use hardware acceleration
export LIBGL_ALWAYS_INDIRECT=0

# Enable threaded optimization (NVIDIA)
export __GL_THREADED_OPTIMIZATIONS=1

# Test rendering performance
glxgears -info
```

### Troubleshooting WSL2

1. **Black screen or no rendering**
   ```bash
   # Check X11 connection
   xclock
   # If this doesn't work, X11 forwarding is broken
   ```

2. **Software rendering only**
   ```bash
   glxinfo | grep "OpenGL renderer"
   # If showing "llvmpipe" (software), GPU acceleration is not working
   ```

3. **Mesa version too old**
   ```bash
   # Update Mesa drivers (Ubuntu/Debian)
   sudo apt update
   sudo apt install mesa-utils mesa-common-dev

   # Check version
   glxinfo | grep "OpenGL version"
   ```

---

## Extension Loading

### Using GLAD (Recommended)

GLAD is a modern, header-only OpenGL loader:

1. **Generate GLAD headers**
   - Visit https://glad.dav1d.de/
   - Select OpenGL 3.3 Core Profile
   - Add extensions: `GL_ARB_debug_output`, `GL_ARB_timer_query`, etc.
   - Download `glad.h` and `glad.c`

2. **Initialize GLAD**
   ```cpp
   #include <glad/glad.h>

   bool InitializeOpenGL(void* (*loadProc)(const char*))
   {
       // Windows
       if (!gladLoadGLLoader((GLADloadproc)wglGetProcAddress)) {
           return false;
       }

       // Linux
       if (!gladLoadGLLoader((GLADloadproc)glXGetProcAddressARB)) {
           return false;
       }

       // Check version
       if (!GLAD_GL_VERSION_3_3) {
           // OpenGL 3.3 not supported
           return false;
       }

       return true;
   }
   ```

3. **Check for extensions**
   ```cpp
   void DetectExtensions(BackendOpenGL::Features& features)
   {
       features.persistent_mapped_buffers = GLAD_GL_ARB_buffer_storage;
       features.direct_state_access = GLAD_GL_ARB_direct_state_access;
       features.multi_draw_indirect = GLAD_GL_ARB_multi_draw_indirect;
       features.compute_shaders = GLAD_GL_VERSION_4_3;
       features.bindless_textures = GLAD_GL_ARB_bindless_texture;
       features.texture_storage = GLAD_GL_ARB_texture_storage;
       features.debug_output = GLAD_GL_KHR_debug;
       features.timer_query = GLAD_GL_ARB_timer_query;
   }
   ```

### Alternative: GLEW

```cpp
#include <GL/glew.h>

bool InitializeOpenGL()
{
    glewExperimental = GL_TRUE;  // Required for Core Profile
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        return false;
    }

    if (!GLEW_VERSION_3_3) {
        return false;
    }

    return true;
}
```

---

## Error Handling

### Debug Callback (OpenGL 4.3+ or ARB_debug_output)

```cpp
void APIENTRY OpenGLDebugCallback(
    GLenum source,
    GLenum type,
    GLuint id,
    GLenum severity,
    GLsizei length,
    const GLchar* message,
    const void* userParam)
{
    // Filter out non-significant messages
    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) {
        return;
    }

    const char* sourceStr = "UNKNOWN";
    switch (source) {
        case GL_DEBUG_SOURCE_API: sourceStr = "API"; break;
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM: sourceStr = "WINDOW_SYSTEM"; break;
        case GL_DEBUG_SOURCE_SHADER_COMPILER: sourceStr = "SHADER_COMPILER"; break;
        case GL_DEBUG_SOURCE_THIRD_PARTY: sourceStr = "THIRD_PARTY"; break;
        case GL_DEBUG_SOURCE_APPLICATION: sourceStr = "APPLICATION"; break;
        case GL_DEBUG_SOURCE_OTHER: sourceStr = "OTHER"; break;
    }

    const char* typeStr = "UNKNOWN";
    switch (type) {
        case GL_DEBUG_TYPE_ERROR: typeStr = "ERROR"; break;
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: typeStr = "DEPRECATED"; break;
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: typeStr = "UNDEFINED"; break;
        case GL_DEBUG_TYPE_PORTABILITY: typeStr = "PORTABILITY"; break;
        case GL_DEBUG_TYPE_PERFORMANCE: typeStr = "PERFORMANCE"; break;
        case GL_DEBUG_TYPE_MARKER: typeStr = "MARKER"; break;
        case GL_DEBUG_TYPE_OTHER: typeStr = "OTHER"; break;
    }

    const char* severityStr = "UNKNOWN";
    switch (severity) {
        case GL_DEBUG_SEVERITY_HIGH: severityStr = "HIGH"; break;
        case GL_DEBUG_SEVERITY_MEDIUM: severityStr = "MEDIUM"; break;
        case GL_DEBUG_SEVERITY_LOW: severityStr = "LOW"; break;
        case GL_DEBUG_SEVERITY_NOTIFICATION: severityStr = "NOTIFICATION"; break;
    }

    fprintf(stderr, "OpenGL [%s] [%s] [%s]: %s\n",
            severityStr, sourceStr, typeStr, message);

    // Break on errors in debug builds
    if (type == GL_DEBUG_TYPE_ERROR) {
        __debugbreak();  // Windows
        // raise(SIGTRAP);  // Linux
    }
}

void EnableDebugOutput()
{
    if (GLAD_GL_KHR_debug) {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(OpenGLDebugCallback, nullptr);
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
    }
}
```

### Manual Error Checking

```cpp
void CheckGLError(const char* file, int line)
{
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        const char* errorStr = "UNKNOWN";
        switch (err) {
            case GL_INVALID_ENUM: errorStr = "INVALID_ENUM"; break;
            case GL_INVALID_VALUE: errorStr = "INVALID_VALUE"; break;
            case GL_INVALID_OPERATION: errorStr = "INVALID_OPERATION"; break;
            case GL_OUT_OF_MEMORY: errorStr = "OUT_OF_MEMORY"; break;
            case GL_INVALID_FRAMEBUFFER_OPERATION: errorStr = "INVALID_FRAMEBUFFER_OPERATION"; break;
        }
        fprintf(stderr, "OpenGL error at %s:%d - %s (0x%x)\n", file, line, errorStr, err);
    }
}

#ifdef _DEBUG
#define CHECK_GL_ERROR() CheckGLError(__FILE__, __LINE__)
#else
#define CHECK_GL_ERROR() ((void)0)
#endif
```

---

## Performance Tuning

### Buffer Streaming Strategies

#### Option 1: Triple Buffering (OpenGL 3.3)

```cpp
class TripleBufferedVBO {
    static constexpr int BUFFER_COUNT = 3;
    GLuint buffers[BUFFER_COUNT];
    int currentBuffer = 0;

    void Create(size_t size) {
        glGenBuffers(BUFFER_COUNT, buffers);
        for (int i = 0; i < BUFFER_COUNT; ++i) {
            glBindBuffer(GL_ARRAY_BUFFER, buffers[i]);
            glBufferData(GL_ARRAY_BUFFER, size, nullptr, GL_STREAM_DRAW);
        }
    }

    void Update(const void* data, size_t size) {
        currentBuffer = (currentBuffer + 1) % BUFFER_COUNT;
        glBindBuffer(GL_ARRAY_BUFFER, buffers[currentBuffer]);
        glBufferSubData(GL_ARRAY_BUFFER, 0, size, data);
    }

    GLuint GetCurrent() const { return buffers[currentBuffer]; }
};
```

#### Option 2: Buffer Orphaning (OpenGL 3.3)

```cpp
void UpdateBufferOrphaning(GLuint buffer, const void* data, size_t size)
{
    glBindBuffer(GL_ARRAY_BUFFER, buffer);

    // Orphan the old buffer (allows GPU to continue using it)
    glBufferData(GL_ARRAY_BUFFER, size, nullptr, GL_STREAM_DRAW);

    // Upload new data
    glBufferSubData(GL_ARRAY_BUFFER, 0, size, data);
}
```

#### Option 3: Persistent Mapping (OpenGL 4.4+)

```cpp
class PersistentMappedBuffer {
    GLuint buffer;
    void* mapped_ptr;
    size_t buffer_size;

    void Create(size_t size) {
        buffer_size = size;
        glGenBuffers(1, &buffer);
        glBindBuffer(GL_ARRAY_BUFFER, buffer);

        GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
        glBufferStorage(GL_ARRAY_BUFFER, size, nullptr, flags);

        mapped_ptr = glMapBufferRange(GL_ARRAY_BUFFER, 0, size, flags);
    }

    void Update(const void* data, size_t size) {
        memcpy(mapped_ptr, data, size);
        // No need to unmap - it's persistent
    }
};
```

### State Change Optimization

```cpp
class GLStateManager {
    struct State {
        GLuint program = 0;
        GLuint vao = 0;
        GLuint texture_2d[16] = {};
        GLenum blend_eq = GL_FUNC_ADD;
        GLenum blend_src = GL_ONE;
        GLenum blend_dst = GL_ZERO;
        bool blend_enabled = false;
    } current;

    void BindProgram(GLuint program) {
        if (current.program != program) {
            glUseProgram(program);
            current.program = program;
        }
    }

    void BindTexture(GLuint unit, GLuint texture) {
        if (current.texture_2d[unit] != texture) {
            glActiveTexture(GL_TEXTURE0 + unit);
            glBindTexture(GL_TEXTURE_2D, texture);
            current.texture_2d[unit] = texture;
        }
    }

    void SetBlendState(GLenum src, GLenum dst) {
        if (!current.blend_enabled) {
            glEnable(GL_BLEND);
            current.blend_enabled = true;
        }
        if (current.blend_src != src || current.blend_dst != dst) {
            glBlendFunc(src, dst);
            current.blend_src = src;
            current.blend_dst = dst;
        }
    }
};
```

### Texture Upload Optimization (PBO)

```cpp
class TextureUploader {
    GLuint pbo;

    void CreatePBO(size_t size) {
        glGenBuffers(1, &pbo);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, size, nullptr, GL_STREAM_DRAW);
    }

    void UploadAsync(GLuint texture, int width, int height, const void* data) {
        // Upload to PBO
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
        void* ptr = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
        memcpy(ptr, data, width * height);
        glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);

        // DMA transfer from PBO to texture
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RED, GL_UNSIGNED_BYTE, nullptr);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    }
};
```

---

## Summary

This guide provides platform-specific implementation details for:
1. **Windows (WGL)**: Modern context creation, VSync control, and extension loading
2. **Linux (GLX)**: Frame buffer configuration, multi-GPU support, and Mesa considerations
3. **WSL2**: WSLg native support, VcXsrv fallback, and performance tuning
4. **Extension Loading**: GLAD and GLEW integration
5. **Error Handling**: Debug callbacks and manual error checking
6. **Performance**: Buffer streaming, state caching, and async uploads

Each section includes production-ready code examples that can be integrated into the BackendOpenGL implementation.