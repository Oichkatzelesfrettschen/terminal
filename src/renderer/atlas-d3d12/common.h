#pragma once

#include <til/generational.h>
#include <string>

#include "../../renderer/inc/IRenderEngine.hpp"

namespace Microsoft::Console::Render::Atlas
{
#define ATLAS_FLAG_OPS(type, underlying)                                                       \
    constexpr type operator~(type v) noexcept                                                  \
    {
        return static_cast<type>(~static_cast<underlying>(v));                                 \
    }                                                                                          \
    constexpr type operator|(type lhs, type rhs) noexcept                                      \
    {
        return static_cast<type>(static_cast<underlying>(lhs) | static_cast<underlying>(rhs)); \
    }                                                                                          \
    constexpr type operator&(type lhs, type rhs) noexcept                                      \
    {
        return static_cast<type>(static_cast<underlying>(lhs) & static_cast<underlying>(rhs)); \
    }                                                                                          \
    constexpr type operator^(type lhs, type rhs) noexcept                                      \
    {
        return static_cast<type>(static_cast<underlying>(lhs) ^ static_cast<underlying>(rhs)); \
    }                                                                                          \
    constexpr void operator|=(type& lhs, type rhs) noexcept                                    \
    {
        lhs = lhs | rhs;                                                                       \
    }                                                                                          \
    constexpr void operator&=(type& lhs, type rhs) noexcept                                    \
    {
        lhs = lhs & rhs;                                                                       \
    }                                                                                          \
    constexpr void operator^=(type& lhs, type rhs) noexcept                                    \
    {
        lhs = lhs ^ rhs;                                                                       \
    }

#define ATLAS_POD_OPS(type)                                    \
    constexpr bool operator==(const type& rhs) const noexcept  \
    {
        return __builtin_memcmp(this, &rhs, sizeof(rhs)) == 0; \
    }                                                          \
                                                               \
    constexpr bool operator!=(const type& rhs) const noexcept  \
    {
        return !(*this == rhs);                                \
    }

    // My best effort of replicating __attribute__((cold)) from gcc/clang.
#define ATLAS_ATTR_COLD __declspec(noinline)

#define ATLAS_ENGINE_ERROR_MAC_TYPE MAKE_HRESULT(SEVERITY_SUCCESS, FACILITY_ITF, 'MT')

    template<typename T>
    struct vec2
    {
        T x;
        T y;

        ATLAS_POD_OPS(vec2)
    };

    template<typename T>
    struct vec4
    {
        T x;
        T y;
        T z;
        T w;

        ATLAS_POD_OPS(vec4)
    };

    template<typename T>
    struct rect
    {
        T left;
        T top;
        T right;
        T bottom;

        ATLAS_POD_OPS(rect)

        constexpr bool empty() const noexcept
        {
            return left >= right || top >= bottom;
        }

        constexpr bool non_empty() const noexcept
        {
            return left < right && top < bottom;
        }
    };

    template<typename T>
    struct range
    {
        T start;
        T end;

        ATLAS_POD_OPS(range)

        constexpr bool empty() const noexcept
        {
            return start >= end;
        }

        constexpr bool non_empty() const noexcept
        {
            return start < end;
        }

        constexpr bool contains(T v) const noexcept
        {
            return v >= start && v < end;
        }
    };

    using u8 = uint8_t;
    using u8x2 = vec2<u8>;

    using u16 = uint16_t;
    using u16x2 = vec2<u16>;
    using u16r = rect<u16>;

    using i16 = int16_t;
    using i16x2 = vec2<i16>;
    using i16x4 = vec4<i16>;
    using i16r = rect<i16>;

    using u32 = uint32_t;
    using u32x2 = vec2<u32>;
    using u32x4 = vec4<u32>;
    using u32r = rect<u32>;

    using i32 = int32_t;
    using i32x2 = vec2<i32>;
    using i32x4 = vec4<i32>;
    using i32r = rect<i32>;

    using u64 = uint64_t;

    using f32 = float;
    using f32x2 = vec2<f32>;
    using f32x4 = vec4<f32>;
    using f32r = rect<f32>;

    template<typename T, size_t Alignment = alignof(T)>
    struct Buffer
    {
        constexpr Buffer() noexcept = default;

        explicit Buffer(size_t size) :
            _data{ allocate(size) },
            _size{ size }
        {
            std::uninitialized_default_construct_n(_data, size);
        }

        Buffer(const T* data, size_t size) :
            _data{ allocate(size) },
            _size{ size }
        {
#pragma warning(suppress : 26459) 
            std::uninitialized_copy_n(data, size, _data);
        }

        ~Buffer()
        {
            destroy();
        }

        Buffer(const Buffer& other) = delete;
        Buffer& operator=(const Buffer& other) = delete;

        Buffer(Buffer&& other) noexcept :
            _data{ std::exchange(other._data, nullptr) },
            _size{ std::exchange(other._size, 0) }
        {
        }

        Buffer& operator=(Buffer&& other) noexcept
        {
            destroy();
            _data = std::exchange(other._data, nullptr);
            _size = std::exchange(other._size, 0);
            return *this;
        }

        explicit operator bool() const noexcept
        {
            return _data != nullptr;
        }

        T& operator[](size_t index) noexcept
        {
            assert(index < _size);
#pragma warning(suppress : 26481) 
            return _data[index];
        }

        const T& operator[](size_t index) const noexcept
        {
            assert(index < _size);
#pragma warning(suppress : 26481) 
            return _data[index];
        }

        T* data() noexcept
        {
            return _data;
        }

        const T* data() const noexcept
        {
            return _data;
        }

        size_t size() const noexcept
        {
            return _size;
        }

        T* begin() noexcept
        {
            return _data;
        }

        const T* begin() const noexcept
        {
            return _data;
        }

        T* end() noexcept
        {
#pragma warning(suppress : 26481) 
            return _data + _size;
        }

        const T* end() const noexcept
        {
#pragma warning(suppress : 26481) 
            return _data + _size;
        }

    private:
#pragma warning(push)
#pragma warning(disable : 26402) 
#pragma warning(disable : 26409) 
        static T* allocate(size_t size)
        {
            if (!size)
            {
                return nullptr;
            }
            if constexpr (Alignment <= __STDCPP_DEFAULT_NEW_ALIGNMENT__)
            {
                return static_cast<T*>(::operator new(size * sizeof(T)));
            }
            else
            {
                return static_cast<T*>(::operator new(size * sizeof(T), static_cast<std::align_val_t>(Alignment)));
            }
        }

        static void deallocate(T* data) noexcept
        {
            if constexpr (Alignment <= __STDCPP_DEFAULT_NEW_ALIGNMENT__)
            {
                ::operator delete(data);
            }
            else
            {
                ::operator delete(data, static_cast<std::align_val_t>(Alignment));
            }
        }
#pragma warning(pop)

        void destroy() noexcept
        {
            std::destroy_n(_data, _size);
            deallocate(_data);
        }

        T* _data = nullptr;
        size_t _size = 0;
    };

    struct TextAnalysisSinkResult
    {
        uint32_t textPosition;
        uint32_t textLength;
        DWRITE_SCRIPT_ANALYSIS analysis;
    };

    class FontInfoBase
    {
    public:
        FontInfoBase(const std::wstring_view& faceName,
                     const unsigned char family,
                     const unsigned int weight,
                     const bool fSetDefaultRasterFont,
                     const unsigned int uiCodePage) noexcept;

        bool operator==(const FontInfoBase& other) noexcept;

        unsigned char GetFamily() const noexcept;
        unsigned int GetWeight() const noexcept;
        const std::wstring& GetFaceName() const noexcept;
        unsigned int GetCodePage() const noexcept;
        void FillLegacyNameBuffer(wchar_t (&buffer)[LF_FACESIZE]) const noexcept;
        bool IsTrueTypeFont() const noexcept;
        void SetFromEngine(const std::wstring_view& faceName,
                           const unsigned char family,
                           const unsigned int weight,
                           const bool fSetDefaultRasterFont) noexcept;
        bool WasDefaultRasterSetFromEngine() const noexcept;
        void ValidateFont() noexcept;

    protected:
        bool IsDefaultRasterFontNoSize() const noexcept;

    private:
        std::wstring _faceName;
        unsigned int _weight;
        unsigned char _family;
        unsigned int _codePage;
        bool _fDefaultRasterSetFromEngine;
    };

    class FontInfo : public FontInfoBase
    {
    public:
        FontInfo(const std::wstring_view& faceName,
                 const unsigned char family,
                 const unsigned int weight,
                 const til::size coordSize,
                 const unsigned int codePage,
                 const bool fSetDefaultRasterFont = false) noexcept;

        bool operator==(const FontInfo& other) noexcept;

        til::size GetSize() const noexcept;
        til::size GetUnscaledSize() const noexcept;
        void SetFromEngine(const std::wstring_view& faceName,
                           const unsigned char family,
                           const unsigned int weight,
                           const bool fSetDefaultRasterFont,
                           const til::size coordSize,
                           const til::size coordSizeUnscaled) noexcept;
        bool GetFallback() const noexcept;
        void SetFallback(const bool didFallback) noexcept;
        void ValidateFont() noexcept;

    private:
        void _ValidateCoordSize() noexcept;

        til::size _coordSize;
        til::size _coordSizeUnscaled;
        bool _didFallback;
    };

    struct CSSLengthPercentage
    {
        enum class ReferenceFrame : uint8_t
        {
            None,
            Absolute,
            FontSize,
            AdvanceWidth,
        };

        static CSSLengthPercentage FromString(const wchar_t* str);

        float Resolve(float fallback, float dpi, float fontSize, float advanceWidth) const noexcept;

    private:
        float _value = 0;
        ReferenceFrame _referenceFrame = ReferenceFrame::None;
    };

    class FontInfoDesired : public FontInfoBase
    {
    public:
        FontInfoDesired(const std::wstring_view& faceName,
                        const unsigned char family,
                        const unsigned int weight,
                        const float fontSize,
                        const unsigned int uiCodePage) noexcept;
        FontInfoDesired(const FontInfo& fiFont) noexcept;

        bool operator==(const FontInfoDesired& other) = delete;

        void SetCellSize(const CSSLengthPercentage& cellWidth, const CSSLengthPercentage& cellHeight) noexcept;
        void SetEnableBuiltinGlyphs(bool builtinGlyphs) noexcept;
        void SetEnableColorGlyphs(bool colorGlyphs) noexcept;

        const CSSLengthPercentage& GetCellWidth() const noexcept;
        const CSSLengthPercentage& GetCellHeight() const noexcept;
        bool GetEnableBuiltinGlyphs() const noexcept;
        bool GetEnableColorGlyphs() const noexcept;
        float GetFontSize() const noexcept;
        til::size GetEngineSize() const noexcept;
        bool IsDefaultRasterFont() const noexcept;

    private:
        til::size _coordSizeDesired;
        float _fontSize;
        CSSLengthPercentage _cellWidth;
        CSSLengthPercentage _cellHeight;
        bool _builtinGlyphs = false;
        bool _colorGlyphs = true;
    };

    enum class GraphicsAPI
    {
        Automatic,
        Direct2D,
        Direct3D11,
        Direct3D12,
    };

    struct TargetSettings
    {
        HWND hwnd = nullptr;
        bool useAlpha = false;
        bool useWARP = false;
        bool disablePresent1 = false;
        GraphicsAPI graphicsAPI = GraphicsAPI::Automatic;
    };

    enum class AntialiasingMode : u8
    {
        ClearType = D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE,
        Grayscale = D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE,
        Aliased = D2D1_TEXT_ANTIALIAS_MODE_ALIASED,
    };

    inline constexpr auto DefaultAntialiasingMode = AntialiasingMode::ClearType;

    struct FontDecorationPosition
    {
        u16 position = 0;
        u16 height = 0;
    };

    struct FontSettings
    {
        wil::com_ptr<IDWriteFontCollection> fontCollection;
        wil::com_ptr<IDWriteFontFallback> fontFallback;
        wil::com_ptr<IDWriteFontFallback1> fontFallback1; // optional, might be nullptr
        std::wstring fontName;
        std::vector<DWRITE_FONT_FEATURE> fontFeatures;
        std::vector<DWRITE_FONT_AXIS_VALUE> fontAxisValues;
        f32 fontSize = 0;
        u16x2 cellSize;
        u16 fontWeight = 0;
        u16 advanceWidth = 0;
        u16 baseline = 0;
        u16 descender = 0;
        u16 thinLineWidth = 0;

        FontDecorationPosition gridTop;
        FontDecorationPosition gridBottom;
        FontDecorationPosition gridLeft;
        FontDecorationPosition gridRight;

        FontDecorationPosition underline;
        FontDecorationPosition strikethrough;
        FontDecorationPosition doubleUnderline[2];
        FontDecorationPosition overline;

        u16 dpi = 96;
        AntialiasingMode antialiasingMode = DefaultAntialiasingMode;
        bool builtinGlyphs = false;
        bool colorGlyphs = true;

        std::vector<uint16_t> softFontPattern;
        til::size softFontCellSize;
    };

    struct CursorSettings
    {
        ATLAS_POD_OPS(CursorSettings)

        u32 cursorColor = 0xffffffff;
        u16 cursorType = 0;
        u16 heightPercentage = 20;
    };

    struct MiscellaneousSettings
    {
        u32 backgroundColor = 0;
        u32 foregroundColor = 0;
        u32 selectionColor = 0xffffffff;
        u32 selectionForeground = 0xff000000;
        std::wstring customPixelShaderPath;
        std::wstring customPixelShaderImagePath;
        bool useRetroTerminalEffect = false;
        bool enableVendorReflex = false;
        bool enableVendorAntiLag = false;
        bool directStorageCacheEnabled = true;
    };

    struct Settings
    {
        til::generational<TargetSettings> target;
        til::generational<FontSettings> font;
        til::generational<CursorSettings> cursor;
        til::generational<MiscellaneousSettings> misc;
        // Size of the viewport / swap chain in pixel.
        u16x2 targetSize{ 0, 0 };
        // Size of the portion of the text buffer that we're drawing on the screen.
        u16x2 viewportCellCount{ 0, 0 };
    };

    using GenerationalSettings = til::generational<Settings>;

    inline GenerationalSettings DirtyGenerationalSettings() noexcept
    {
        return GenerationalSettings{
            til::generation_t{ 1 },
            til::generational<TargetSettings>{ til::generation_t{ 1 } },
            til::generational<FontSettings>{ til::generation_t{ 1 } },
            til::generational<CursorSettings>{ til::generation_t{ 1 } },
            til::generational<MiscellaneousSettings>{ til::generation_t{ 1 } },
        };
    }
}
