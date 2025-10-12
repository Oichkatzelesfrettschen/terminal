// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <filesystem>
#include <fstream>
#include <vector>

#include <stb_rect_pack.h>
#include <til/flat_set.h>

#ifdef _WIN32
#include <Windows.h>
#endif

// OpenGL includes (use GLAD, GLEW, or similar loader)
#include <GL/gl.h>
#include <GL/glext.h>
#ifndef _WIN32
#include <GL/glx.h>
#endif

#include "Backend.h"

namespace Microsoft::Console::Render::Atlas
{
    // BackendOpenGL: OpenGL 3.3+ renderer for cross-platform support
    //
    // This backend provides a fallback renderer compatible with:
    // - Windows Vista+ (via WGL)
    // - Linux (via GLX/EGL)
    // - WSL2 (via VcXsrv/WSLg)
    // - macOS (deprecated but functional via CGL)
    //
    // Target baseline: OpenGL 3.3 Core Profile
    // Progressive enhancement: OpenGL 4.x features detected at runtime
    //
    // Key features:
    // - Instanced rendering: 65,536 instances per draw call
    // - Glyph atlas: Dynamic texture atlas for character caching
    // - Batch rendering: Minimize state changes and draw calls
    // - Feature detection: Use modern extensions when available
    struct BackendOpenGL : IBackend
    {
        BackendOpenGL(const RenderingPayload& p);
        ~BackendOpenGL() override;

        void ReleaseResources() noexcept override;
        void Render(RenderingPayload& payload) override;
        bool RequiresContinuousRedraw() noexcept override;

        // ============================================================================
        // Constant Buffers (std140 layout, 16-byte aligned)
        // ============================================================================

        struct alignas(16) VSConstBuffer
        {
            alignas(sizeof(f32x2)) f32x2 positionScale;
#pragma warning(suppress : 4324)
        };

        struct alignas(16) PSConstBuffer
        {
            alignas(sizeof(f32x4)) f32x4 backgroundColor;
            alignas(sizeof(f32x2)) f32x2 backgroundCellSize;
            alignas(sizeof(f32x2)) f32x2 backgroundCellCount;
            alignas(sizeof(f32x4)) f32 gammaRatios[4]{};
            alignas(sizeof(f32)) f32 enhancedContrast = 0;
            alignas(sizeof(f32)) f32 underlineWidth = 0;
            alignas(sizeof(f32)) f32 doubleUnderlineWidth = 0;
            alignas(sizeof(f32)) f32 curlyLineHalfHeight = 0;
            alignas(sizeof(f32)) f32 shadedGlyphDotSize = 0;
#pragma warning(suppress : 4324)
        };

        struct alignas(16) CustomConstBuffer
        {
            alignas(sizeof(f32)) f32 time = 0;
            alignas(sizeof(f32)) f32 scale = 0;
            alignas(sizeof(f32x2)) f32x2 resolution;
            alignas(sizeof(f32x4)) f32x4 background;
#pragma warning(suppress : 4324)
        };

        // ============================================================================
        // Shading Types (matches BackendD3D for compatibility)
        // ============================================================================

        enum class ShadingType : u8
        {
            Default = 0,
            Background = 0,

            // Text drawing primitives (TextDrawingFirst to TextDrawingLast)
            TextGrayscale,
            TextClearType,
            TextBuiltinGlyph,
            TextPassthrough,
            DottedLine,
            DashedLine,
            CurlyLine,
            SolidLine, // All items from here draw as solid RGBA

            Cursor,
            FilledRect,

            TextDrawingFirst = TextGrayscale,
            TextDrawingLast = SolidLine,
        };

        // ============================================================================
        // Quad Instance Structure (matches BackendD3D)
        // ============================================================================

        struct QuadInstance
        {
            alignas(u16) u16 shadingType;
            alignas(u16) u8x2 renditionScale;
            alignas(u32) i16x2 position;
            alignas(u32) u16x2 size;
            alignas(u32) u16x2 texcoord;
            alignas(u32) u32 color;
        };

        static_assert(sizeof(QuadInstance) == 16);
        static_assert(alignof(QuadInstance) == 4);

        // Maximum instances per draw call (Alacritty-inspired batch size)
        static constexpr u32 MaxInstances = 65536;

        // ============================================================================
        // Glyph Atlas Entry (matches BackendD3D)
        // ============================================================================

        struct AtlasGlyphEntry
        {
            u32 glyphIndex;
            u8 occupied;
            ShadingType shadingType;
            u16 overlapSplit;
            i16x2 offset;
            u16x2 size;
            u16x2 texcoord;
        };

        struct AtlasGlyphEntryHashTrait
        {
            static constexpr bool occupied(const AtlasGlyphEntry& entry) noexcept
            {
                return entry.occupied != 0;
            }

            static constexpr size_t hash(const u16 glyphIndex) noexcept
            {
                return til::flat_set_hash_integer(glyphIndex);
            }

            static constexpr size_t hash(const AtlasGlyphEntry& entry) noexcept
            {
                return til::flat_set_hash_integer(entry.glyphIndex);
            }

            static constexpr bool equals(const AtlasGlyphEntry& entry, u16 glyphIndex) noexcept
            {
                return entry.glyphIndex == glyphIndex;
            }

            static constexpr void assign(AtlasGlyphEntry& entry, u16 glyphIndex) noexcept
            {
                entry.glyphIndex = glyphIndex;
                entry.occupied = 1;
            }
        };

        struct AtlasFontFaceEntry
        {
            wil::com_ptr<IDWriteFontFace2> fontFace;
            til::linear_flat_set<AtlasGlyphEntry, AtlasGlyphEntryHashTrait> glyphs[4];
        };

        struct AtlasFontFaceEntryHashTrait
        {
            static bool occupied(const AtlasFontFaceEntry& entry) noexcept
            {
                return static_cast<bool>(entry.fontFace);
            }

            static constexpr size_t hash(const IDWriteFontFace2* fontFace) noexcept
            {
                return til::flat_set_hash_integer(std::bit_cast<uintptr_t>(fontFace));
            }

            static size_t hash(const AtlasFontFaceEntry& entry) noexcept
            {
                return hash(entry.fontFace.get());
            }

            static bool equals(const AtlasFontFaceEntry& entry, const IDWriteFontFace2* fontFace) noexcept
            {
                return entry.fontFace.get() == fontFace;
            }

            static void assign(AtlasFontFaceEntry& entry, IDWriteFontFace2* fontFace) noexcept
            {
                entry.fontFace = fontFace;
            }
        };

        struct AtlasBitmap
        {
            u64 key;
            u16x2 size;
            u16x2 texcoord;
        };

        struct AtlasBitmapHashTrait
        {
            static bool occupied(const AtlasBitmap& entry) noexcept
            {
                return entry.key != 0;
            }

            static constexpr size_t hash(const u64 key) noexcept
            {
                return til::flat_set_hash_integer(gsl::narrow_cast<size_t>(key));
            }

            static size_t hash(const AtlasBitmap& entry) noexcept
            {
                return hash(entry.key);
            }

            static bool equals(const AtlasBitmap& entry, const u64 key) noexcept
            {
                return entry.key == key;
            }

            static void assign(AtlasBitmap& entry, u64 key) noexcept
            {
                entry.key = key;
            }
        };

    private:
        struct CursorRect
        {
            i16x2 position;
            u16x2 size;
            u32 background;
            u32 foreground;
        };

        // ============================================================================
        // OpenGL Core Objects
        // ============================================================================

        // Context and surface (platform-specific)
#ifdef _WIN32
        HDC _hdc = nullptr;
        HGLRC _context = nullptr;
#else
        // GLX/EGL context will be stored here
        void* _display = nullptr;
        void* _context = nullptr;
        void* _surface = nullptr;
#endif

        // Vertex Array Object (VAO)
        GLuint _vao = 0;

        // Buffers
        GLuint _vertexBuffer = 0;      // Quad vertices (static)
        GLuint _indexBuffer = 0;       // Quad indices (static)
        GLuint _instanceBuffer = 0;    // QuadInstance array (dynamic)

        // Uniform Buffer Objects (UBOs)
        GLuint _vsConstantBuffer = 0;
        GLuint _psConstantBuffer = 0;
        GLuint _customConstantBuffer = 0;

        // Textures
        GLuint _glyphAtlas = 0;           // R8 texture for glyphs
        GLuint _backgroundBitmap = 0;     // RGBA8 texture for background colors

        // Pixel Buffer Objects (PBOs) for async texture uploads
        GLuint _glyphAtlasUploadPBO = 0;
        GLuint _backgroundUploadPBO = 0;

        // Shaders and programs
        GLuint _vertexShader = 0;
        GLuint _fragmentShader = 0;
        GLuint _shaderProgram = 0;
        std::wstring _shaderSpirvVS;
        std::wstring _shaderSpirvPS;
        std::wstring _shaderGlslVS;
        std::wstring _shaderGlslPS;

        // Custom shaders (if enabled)
        GLuint _customVertexShader = 0;
        GLuint _customFragmentShader = 0;
        GLuint _customShaderProgram = 0;
        GLuint _customOffscreenTexture = 0;
        GLuint _customOffscreenFramebuffer = 0;

        // ============================================================================
        // OpenGL Feature Detection
        // ============================================================================

        struct Features
        {
            // Core GL version
            int major_version = 3;
            int minor_version = 3;

            // OpenGL 4.x features
            bool persistent_mapped_buffers = false;  // GL 4.4+
            bool direct_state_access = false;        // GL 4.5+
            bool multi_draw_indirect = false;        // GL 4.3+
            bool compute_shaders = false;            // GL 4.3+
            bool bindless_textures = false;          // GL 4.4+ (ARB extension)
            bool spirv_shaders = false;              // GL_ARB_gl_spirv

            // Common extensions
            bool texture_storage = false;            // GL 4.2+ or ARB_texture_storage
            bool debug_output = false;               // GL 4.3+ or KHR_debug
            bool timer_query = false;                // GL 3.3+ or ARB_timer_query

            // Platform-specific
            bool vsync_control = false;              // WGL_EXT_swap_control / GLX_EXT_swap_control
        } _features;

        void _detectFeatures();

        // ============================================================================
        // Initialization and Setup
        // ============================================================================

        void _createContext(const RenderingPayload& p);
        void _loadExtensions();
        void _createBuffers();
        void _createTextures(const RenderingPayload& p);
        void _compileShaders();
        GLuint _compileShaderFromSpirv(GLenum stage, const std::wstring& spirvPath);
        GLuint _compileShaderFromGlsl(GLenum stage, const std::wstring& glslPath);
        void _setupVertexAttributes();
        void _setupBlendState();

        // ============================================================================
        // Rendering Pipeline
        // ============================================================================

        void _handleSettingsUpdate(const RenderingPayload& p);
        void _updateFontDependents(const RenderingPayload& p);
        void _recreateGlyphAtlas(const RenderingPayload& p, u32 minWidth, u32 minHeight);
        void _resizeGlyphAtlas(const RenderingPayload& p, u16 u, u16 v);

        void _beginFrame();
        void _updateConstantBuffers(const RenderingPayload& p);
        void _updateInstanceBuffer();
        void _drawBackground(const RenderingPayload& p);
        void _uploadBackgroundBitmap(const RenderingPayload& p);
        void _drawText(RenderingPayload& p);
        void _drawGlyph(const RenderingPayload& p, const ShapedRow& row, AtlasFontFaceEntry& fontFaceEntry, u32 glyphIndex);
        void _drawBuiltinGlyph(const RenderingPayload& p, const ShapedRow& row, AtlasFontFaceEntry& fontFaceEntry, u32 glyphIndex);
        void _drawSoftFontGlyph(const RenderingPayload& p, const D2D1_RECT_F& rect, u32 glyphIndex);
        void _drawGlyphAtlasAllocate(const RenderingPayload& p, stbrp_rect& rect);
        void _drawGridlines(const RenderingPayload& p, u16 y);
        void _drawBitmap(const RenderingPayload& p, const ShapedRow* row, u16 y);
        void _drawCursorBackground(const RenderingPayload& p);
        void _drawCursorForeground();
        void _drawSelection(const RenderingPayload& p);
        void _executeCustomShader(RenderingPayload& p);
        void _endFrame();
        void _present();

        // ============================================================================
        // Batch Rendering
        // ============================================================================

        QuadInstance& _getLastQuad() noexcept;
        QuadInstance& _appendQuad();
        void _flushQuads(const RenderingPayload& p);

        Buffer<QuadInstance, 32> _instances;
        size_t _instancesCount = 0;
        size_t _instanceBufferCapacity = 0;

        // ============================================================================
        // State Management and Caching
        // ============================================================================

        struct GLState
        {
            GLuint program = 0;
            GLuint vao = 0;
            GLuint textures[16] = {};
            GLuint ubos[8] = {};
            GLenum blend_src_rgb = GL_ONE;
            GLenum blend_dst_rgb = GL_ZERO;
            GLenum blend_src_alpha = GL_ONE;
            GLenum blend_dst_alpha = GL_ZERO;
            bool blend_enabled = false;
            bool scissor_enabled = false;
            i32x4 scissor_rect = {};
            i32x4 viewport = {};
        } _currentState, _desiredState;

        void _applyState();
        void _bindProgram(GLuint program);
        void _bindTexture(GLuint unit, GLuint texture);
        void _bindUBO(GLuint index, GLuint ubo);
        void _setBlendState(GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha);

        // ============================================================================
        // Glyph Atlas Management
        // ============================================================================

        til::linear_flat_set<AtlasFontFaceEntry, AtlasFontFaceEntryHashTrait> _glyphAtlasMap;
        til::linear_flat_set<AtlasBitmap, AtlasBitmapHashTrait> _glyphAtlasBitmaps;
        AtlasFontFaceEntry _builtinGlyphs;
        Buffer<stbrp_node> _rectPackerData;
        stbrp_context _rectPacker{};
        til::CoordType _ligatureOverhangTriggerLeft = 0;
        til::CoordType _ligatureOverhangTriggerRight = 0;

        u16x2 _glyphAtlasSize = {};

        void _updateGlyphAtlasRegion(u32 x, u32 y, u32 width, u32 height, const void* data);

        // ============================================================================
        // D2D Integration (for glyph rasterization)
        // ============================================================================

        wil::com_ptr<ID2D1DeviceContext> _d2dRenderTarget;
        wil::com_ptr<ID2D1DeviceContext4> _d2dRenderTarget4;
        wil::com_ptr<ID2D1SolidColorBrush> _emojiBrush;
        wil::com_ptr<ID2D1SolidColorBrush> _brush;
        wil::com_ptr<ID2D1Bitmap1> _softFontBitmap;
        bool _d2dBeganDrawing = false;
        bool _fontChangedResetGlyphAtlas = false;

        void _d2dBeginDrawing() noexcept;
        void _d2dEndDrawing();
        void _d2dRenderTargetUpdateFontSettings(const RenderingPayload& p) const noexcept;

        // ============================================================================
        // Shader Management
        // ============================================================================

        GLuint _compileShader(GLenum type, const char* source);
        GLuint _linkProgram(GLuint vs, GLuint fs);
        void _validateProgram(GLuint program);
        void _recreateCustomShader(const RenderingPayload& p);

        // ============================================================================
        // Performance Tracking
        // ============================================================================

        float _gamma = 0;
        float _cleartypeEnhancedContrast = 0;
        float _grayscaleEnhancedContrast = 0;
        wil::com_ptr<IDWriteRenderingParams1> _textRenderingParams;

        til::generation_t _generation;
        til::generation_t _fontGeneration;
        til::generation_t _miscGeneration;
        u16x2 _targetSize{};
        u16x2 _viewportCellCount{};
        ShadingType _textShadingType = ShadingType::Default;

        til::small_vector<CursorRect, 6> _cursorRects;
        til::rect _cursorPosition;

        f32 _curlyLineHalfHeight = 0.0f;
        FontDecorationPosition _curlyUnderline;

        bool _requiresContinuousRedraw = false;

        // ============================================================================
        // Debugging
        // ============================================================================

#if ATLAS_DEBUG_SHOW_DIRTY
        i32r _presentRects[9]{};
        size_t _presentRectsPos = 0;
#endif

#if ATLAS_DEBUG_DUMP_RENDER_TARGET
        wchar_t _dumpRenderTargetBasePath[MAX_PATH]{};
        size_t _dumpRenderTargetCounter = 0;
#endif

#if ATLAS_DEBUG_COLORIZE_GLYPH_ATLAS
        size_t _colorizeGlyphAtlasCounter = 0;
#endif

#if ATLAS_DEBUG_SHADER_HOT_RELOAD
        std::filesystem::path _sourceDirectory;
        wil::unique_folder_change_reader_nothrow _sourceCodeWatcher;
        std::atomic<int64_t> _sourceCodeInvalidationTime{ INT64_MAX };
        void _debugUpdateShaders(const RenderingPayload& p) noexcept;
#endif

        void _debugShowDirty(const RenderingPayload& p);
        void _debugDumpRenderTarget(const RenderingPayload& p);

        // ============================================================================
        // Platform-Specific Helpers
        // ============================================================================

#ifdef _WIN32
        void _createWGLContext(HWND hwnd);
        void _destroyWGLContext();
        void _wglSwapBuffers();
        bool _wglSetVSync(bool enable);
#else
        void _createGLXContext(void* display, void* window);
        void _destroyGLXContext();
        void _glxSwapBuffers();
        bool _glxSetVSync(bool enable);
#endif

        // ============================================================================
        // Buffer Streaming Helpers
        // ============================================================================

        void* _mapBuffer(GLuint buffer, size_t size, bool persistent = false);
        void _unmapBuffer(GLuint buffer);
        void _orphanBuffer(GLuint buffer, size_t size, GLenum usage);

        // ============================================================================
        // Utility Functions
        // ============================================================================

        static const char* _getShaderInfoLog(GLuint shader);
        static const char* _getProgramInfoLog(GLuint program);
        static void APIENTRY _debugCallback(GLenum source, GLenum type, GLuint id,
                                           GLenum severity, GLsizei length,
                                           const GLchar* message, const void* userParam);
    };

} // namespace Microsoft::Console::Render::Atlas
