/*
 * DirectWrite Integration Example for Spline Sans Mono
 * Windows Terminal Font Loading and Rendering
 *
 * This example demonstrates how to:
 * 1. Enumerate system fonts to find Spline Sans Mono
 * 2. Load Spline Sans Mono from bundled TTF file
 * 3. Create font face for rendering
 * 4. Configure optimal rendering parameters
 *
 * Compiler: MSVC 2019+ or Clang-CL
 * Target: Windows 10 1809+ / Windows 11
 * Dependencies: dwrite.lib, d2d1.lib
 */

#include <windows.h>
#include <dwrite.h>
#include <dwrite_3.h>
#include <d2d1.h>
#include <wrl/client.h>
#include <string>
#include <vector>
#include <iostream>

using Microsoft::WRL::ComPtr;

/* ========================================================================
 * SECTION 1: Font Enumeration
 * ======================================================================== */

/**
 * Checks if Spline Sans Mono is installed on the system
 * Returns: true if font family is found, false otherwise
 */
bool IsSplineSansMonoInstalled(IDWriteFactory* factory)
{
    ComPtr<IDWriteFontCollection> fontCollection;
    HRESULT hr = factory->GetSystemFontCollection(&fontCollection);
    if (FAILED(hr)) {
        std::cerr << "Failed to get system font collection: 0x"
                  << std::hex << hr << std::endl;
        return false;
    }

    UINT32 familyIndex;
    BOOL exists = FALSE;
    hr = fontCollection->FindFamilyName(L"Spline Sans Mono", &familyIndex, &exists);
    if (FAILED(hr)) {
        std::cerr << "Failed to find family name: 0x"
                  << std::hex << hr << std::endl;
        return false;
    }

    if (exists) {
        std::cout << "Spline Sans Mono found at family index: "
                  << familyIndex << std::endl;
    } else {
        std::cout << "Spline Sans Mono NOT found in system fonts" << std::endl;
    }

    return exists == TRUE;
}

/**
 * Enumerates all available weights and styles for Spline Sans Mono
 */
void EnumerateSplineSansMonoWeights(IDWriteFactory* factory)
{
    ComPtr<IDWriteFontCollection> fontCollection;
    HRESULT hr = factory->GetSystemFontCollection(&fontCollection);
    if (FAILED(hr)) return;

    UINT32 familyIndex;
    BOOL exists;
    hr = fontCollection->FindFamilyName(L"Spline Sans Mono", &familyIndex, &exists);
    if (FAILED(hr) || !exists) return;

    ComPtr<IDWriteFontFamily> fontFamily;
    hr = fontCollection->GetFontFamily(familyIndex, &fontFamily);
    if (FAILED(hr)) return;

    UINT32 fontCount = fontFamily->GetFontCount();
    std::cout << "Spline Sans Mono variants (" << fontCount << " fonts):" << std::endl;

    for (UINT32 i = 0; i < fontCount; ++i) {
        ComPtr<IDWriteFont> font;
        hr = fontFamily->GetFont(i, &font);
        if (FAILED(hr)) continue;

        DWRITE_FONT_WEIGHT weight = font->GetWeight();
        DWRITE_FONT_STYLE style = font->GetStyle();
        DWRITE_FONT_STRETCH stretch = font->GetStretch();

        ComPtr<IDWriteLocalizedStrings> faceNames;
        hr = font->GetFaceNames(&faceNames);
        if (FAILED(hr)) continue;

        UINT32 nameLength;
        hr = faceNames->GetStringLength(0, &nameLength);
        if (FAILED(hr)) continue;

        std::wstring faceName(nameLength + 1, L'\0');
        hr = faceNames->GetString(0, &faceName[0], nameLength + 1);
        if (FAILED(hr)) continue;

        std::wcout << L"  - " << faceName
                   << L" (Weight: " << weight
                   << L", Style: " << style
                   << L", Stretch: " << stretch << L")" << std::endl;
    }
}

/* ========================================================================
 * SECTION 2: Font Face Creation from System Font
 * ======================================================================== */

/**
 * Creates a font face for Spline Sans Mono Regular from system fonts
 * Returns: IDWriteFontFace interface or nullptr on failure
 */
ComPtr<IDWriteFontFace> CreateSplineSansMonoFace_FromSystem(
    IDWriteFactory* factory,
    DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_REGULAR,
    DWRITE_FONT_STYLE style = DWRITE_FONT_STYLE_NORMAL)
{
    ComPtr<IDWriteFontCollection> fontCollection;
    HRESULT hr = factory->GetSystemFontCollection(&fontCollection);
    if (FAILED(hr)) return nullptr;

    UINT32 familyIndex;
    BOOL exists;
    hr = fontCollection->FindFamilyName(L"Spline Sans Mono", &familyIndex, &exists);
    if (FAILED(hr) || !exists) return nullptr;

    ComPtr<IDWriteFontFamily> fontFamily;
    hr = fontCollection->GetFontFamily(familyIndex, &fontFamily);
    if (FAILED(hr)) return nullptr;

    ComPtr<IDWriteFont> font;
    hr = fontFamily->GetFirstMatchingFont(
        weight,
        DWRITE_FONT_STRETCH_NORMAL,
        style,
        &font);
    if (FAILED(hr)) return nullptr;

    ComPtr<IDWriteFontFace> fontFace;
    hr = font->CreateFontFace(&fontFace);
    if (FAILED(hr)) return nullptr;

    std::cout << "Created font face for Spline Sans Mono (weight: "
              << weight << ", style: " << style << ")" << std::endl;

    return fontFace;
}

/* ========================================================================
 * SECTION 3: Font Face Creation from File (Bundled Font)
 * ======================================================================== */

/**
 * Creates a font face from a TTF file path (for bundled fonts)
 * This is useful when distributing fonts with your application
 */
ComPtr<IDWriteFontFace> CreateSplineSansMonoFace_FromFile(
    IDWriteFactory* factory,
    const wchar_t* fontFilePath)
{
    ComPtr<IDWriteFontFile> fontFile;
    HRESULT hr = factory->CreateFontFileReference(fontFilePath, nullptr, &fontFile);
    if (FAILED(hr)) {
        std::wcerr << L"Failed to create font file reference: " << fontFilePath
                   << L" (0x" << std::hex << hr << L")" << std::endl;
        return nullptr;
    }

    /* Analyze font file to determine face type */
    BOOL isSupportedFontType;
    DWRITE_FONT_FILE_TYPE fileType;
    DWRITE_FONT_FACE_TYPE faceType;
    UINT32 numberOfFaces;

    hr = fontFile->Analyze(&isSupportedFontType, &fileType, &faceType, &numberOfFaces);
    if (FAILED(hr) || !isSupportedFontType) {
        std::wcerr << L"Font file not supported or invalid" << std::endl;
        return nullptr;
    }

    std::wcout << L"Font file analysis: Type=" << fileType
               << L", FaceType=" << faceType
               << L", Faces=" << numberOfFaces << std::endl;

    /* Create font face from file */
    ComPtr<IDWriteFontFace> fontFace;
    IDWriteFontFile* fontFiles[] = { fontFile.Get() };
    hr = factory->CreateFontFace(
        faceType,
        1,                                      /* number of files */
        fontFiles,
        0,                                      /* face index (0 for TTF) */
        DWRITE_FONT_SIMULATIONS_NONE,          /* no bold/italic simulation */
        &fontFace);

    if (FAILED(hr)) {
        std::wcerr << L"Failed to create font face from file (0x"
                   << std::hex << hr << L")" << std::endl;
        return nullptr;
    }

    std::wcout << L"Created font face from file: " << fontFilePath << std::endl;
    return fontFace;
}

/* ========================================================================
 * SECTION 4: Font Face Creation from Resource (Embedded Font)
 * ======================================================================== */

/**
 * Custom font file loader for loading fonts from memory/resources
 */
class MemoryFontFileLoader : public IDWriteFontFileLoader
{
private:
    ULONG refCount_;
    const void* fontData_;
    UINT32 fontSize_;

public:
    MemoryFontFileLoader(const void* fontData, UINT32 fontSize)
        : refCount_(1), fontData_(fontData), fontSize_(fontSize) {}

    /* IUnknown methods */
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
    {
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IDWriteFontFileLoader)) {
            *ppvObject = this;
            AddRef();
            return S_OK;
        }
        *ppvObject = nullptr;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return InterlockedIncrement(&refCount_);
    }

    ULONG STDMETHODCALLTYPE Release() override
    {
        ULONG newCount = InterlockedDecrement(&refCount_);
        if (newCount == 0) delete this;
        return newCount;
    }

    /* IDWriteFontFileLoader methods */
    HRESULT STDMETHODCALLTYPE CreateStreamFromKey(
        void const* fontFileReferenceKey,
        UINT32 fontFileReferenceKeySize,
        IDWriteFontFileStream** fontFileStream) override
    {
        /* Implementation would create a memory stream wrapper */
        /* Simplified for this example */
        return E_NOTIMPL;
    }
};

/**
 * Loads Spline Sans Mono from embedded resource
 * Resource must be added to .rc file as:
 * IDR_FONT_SPLINE_MONO_REGULAR FONT "fonts\\SplineSansMono-Regular.ttf"
 */
ComPtr<IDWriteFontFace> CreateSplineSansMonoFace_FromResource(
    IDWriteFactory* factory,
    HINSTANCE hInstance,
    int resourceId)
{
    /* Find and load the font resource */
    HRSRC hResource = FindResource(hInstance, MAKEINTRESOURCE(resourceId), L"FONT");
    if (!hResource) {
        std::cerr << "Failed to find font resource: " << resourceId << std::endl;
        return nullptr;
    }

    HGLOBAL hMemory = LoadResource(hInstance, hResource);
    if (!hMemory) {
        std::cerr << "Failed to load font resource" << std::endl;
        return nullptr;
    }

    void* fontData = LockResource(hMemory);
    DWORD fontSize = SizeofResource(hInstance, hResource);

    if (!fontData || fontSize == 0) {
        std::cerr << "Invalid font resource data" << std::endl;
        return nullptr;
    }

    std::cout << "Loaded font resource: " << fontSize << " bytes" << std::endl;

    /* Note: Full implementation requires custom font file loader and stream */
    /* This is a simplified example showing the resource loading part */
    /* In production, use IDWriteFactory5::CreateInMemoryFontFileLoader (Windows 10 1809+) */

    return nullptr; /* Placeholder - full implementation needed */
}

/* ========================================================================
 * SECTION 5: Rendering Configuration
 * ======================================================================== */

/**
 * Gets recommended rendering mode for terminal text
 * Terminal fonts benefit from symmetric rendering for consistent character widths
 */
DWRITE_RENDERING_MODE GetOptimalRenderingMode(
    IDWriteFontFace* fontFace,
    float fontSize,
    float dpiX,
    float dpiY)
{
    /* Query for IDWriteFontFace3 interface (Windows 10+) */
    ComPtr<IDWriteFontFace3> fontFace3;
    HRESULT hr = fontFace->QueryInterface(__uuidof(IDWriteFontFace3),
                                          reinterpret_cast<void**>(fontFace3.GetAddressOf()));

    if (SUCCEEDED(hr)) {
        DWRITE_RENDERING_MODE1 recommendedMode;
        hr = fontFace3->GetRecommendedRenderingMode(
            fontSize,
            dpiX,
            dpiY,
            nullptr,  /* no transform */
            FALSE,    /* not sideways */
            DWRITE_OUTLINE_THRESHOLD_ALIASED,
            DWRITE_MEASURING_MODE_NATURAL,
            nullptr,  /* no rendering params */
            &recommendedMode);

        if (SUCCEEDED(hr)) {
            std::cout << "Recommended rendering mode: " << recommendedMode << std::endl;
            return static_cast<DWRITE_RENDERING_MODE>(recommendedMode);
        }
    }

    /* Fallback for older interfaces or if query fails */
    /* For terminal/monospace fonts, symmetric ClearType works best */
    std::cout << "Using default: CLEARTYPE_NATURAL_SYMMETRIC" << std::endl;
    return DWRITE_RENDERING_MODE_CLEARTYPE_NATURAL_SYMMETRIC;
}

/**
 * Creates optimized rendering parameters for Spline Sans Mono
 */
ComPtr<IDWriteRenderingParams> CreateOptimalRenderingParams(
    IDWriteFactory* factory,
    float gamma = 2.2f,
    float enhancedContrast = 0.5f,
    float clearTypeLevel = 1.0f)
{
    ComPtr<IDWriteRenderingParams> renderingParams;
    HRESULT hr = factory->CreateCustomRenderingParams(
        gamma,
        enhancedContrast,
        clearTypeLevel,
        DWRITE_PIXEL_GEOMETRY_RGB,
        DWRITE_RENDERING_MODE_CLEARTYPE_NATURAL_SYMMETRIC,
        &renderingParams);

    if (FAILED(hr)) {
        std::cerr << "Failed to create rendering params: 0x"
                  << std::hex << hr << std::endl;
        return nullptr;
    }

    std::cout << "Created custom rendering parameters:"
              << "\n  Gamma: " << gamma
              << "\n  Enhanced Contrast: " << enhancedContrast
              << "\n  ClearType Level: " << clearTypeLevel << std::endl;

    return renderingParams;
}

/* ========================================================================
 * SECTION 6: Glyph Metrics and Monospace Validation
 * ======================================================================== */

/**
 * Verifies that Spline Sans Mono has consistent character widths (monospace)
 * Tests common ASCII characters to ensure fixed-width spacing
 */
bool ValidateMonospaceMetrics(IDWriteFontFace* fontFace)
{
    /* Test characters (ASCII printable range) */
    const wchar_t testChars[] = L"iMW|.0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    const size_t numChars = wcslen(testChars);

    std::vector<UINT16> glyphIndices(numChars);
    std::vector<DWRITE_GLYPH_METRICS> glyphMetrics(numChars);

    /* Convert characters to glyph indices */
    HRESULT hr = fontFace->GetGlyphIndices(
        reinterpret_cast<const UINT32*>(testChars),
        static_cast<UINT32>(numChars),
        glyphIndices.data());

    if (FAILED(hr)) {
        std::cerr << "Failed to get glyph indices" << std::endl;
        return false;
    }

    /* Get glyph metrics */
    hr = fontFace->GetDesignGlyphMetrics(
        glyphIndices.data(),
        static_cast<UINT32>(numChars),
        glyphMetrics.data(),
        FALSE);  /* not sideways */

    if (FAILED(hr)) {
        std::cerr << "Failed to get glyph metrics" << std::endl;
        return false;
    }

    /* Check for consistent advance width */
    INT32 expectedAdvance = glyphMetrics[0].advanceWidth;
    bool isMonospace = true;

    std::cout << "Monospace validation (design units):" << std::endl;
    std::cout << "  Expected advance width: " << expectedAdvance << std::endl;

    for (size_t i = 0; i < numChars; ++i) {
        if (glyphMetrics[i].advanceWidth != expectedAdvance) {
            std::wcout << L"  WARNING: Character '" << testChars[i]
                       << L"' has advance width " << glyphMetrics[i].advanceWidth
                       << L" (expected " << expectedAdvance << L")" << std::endl;
            isMonospace = false;
        }
    }

    if (isMonospace) {
        std::cout << "  VERIFIED: All test characters have consistent width (true monospace)"
                  << std::endl;
    } else {
        std::cout << "  WARNING: Inconsistent character widths detected!" << std::endl;
    }

    return isMonospace;
}

/**
 * Gets font metrics for calculating line height, baseline, etc.
 */
void PrintFontMetrics(IDWriteFontFace* fontFace)
{
    DWRITE_FONT_METRICS fontMetrics;
    fontFace->GetMetrics(&fontMetrics);

    std::cout << "Font Metrics (design units):" << std::endl;
    std::cout << "  Design Units Per EM: " << fontMetrics.designUnitsPerEm << std::endl;
    std::cout << "  Ascent: " << fontMetrics.ascent << std::endl;
    std::cout << "  Descent: " << fontMetrics.descent << std::endl;
    std::cout << "  Line Gap: " << fontMetrics.lineGap << std::endl;
    std::cout << "  Cap Height: " << fontMetrics.capHeight << std::endl;
    std::cout << "  x-Height: " << fontMetrics.xHeight << std::endl;
    std::cout << "  Underline Position: " << fontMetrics.underlinePosition << std::endl;
    std::cout << "  Underline Thickness: " << fontMetrics.underlineThickness << std::endl;
    std::cout << "  Strikethrough Position: " << fontMetrics.strikethroughPosition << std::endl;
    std::cout << "  Strikethrough Thickness: " << fontMetrics.strikethroughThickness << std::endl;

    /* Calculate recommended line height */
    INT32 lineHeight = fontMetrics.ascent + fontMetrics.descent + fontMetrics.lineGap;
    std::cout << "  Recommended Line Height: " << lineHeight
              << " (" << (float)lineHeight / fontMetrics.designUnitsPerEm << " em)" << std::endl;
}

/* ========================================================================
 * SECTION 7: Example Usage
 * ======================================================================== */

int main()
{
    std::cout << "=== Spline Sans Mono DirectWrite Integration Example ===" << std::endl;
    std::cout << std::endl;

    /* Initialize DirectWrite factory */
    ComPtr<IDWriteFactory> factory;
    HRESULT hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(factory.GetAddressOf()));

    if (FAILED(hr)) {
        std::cerr << "Failed to create DirectWrite factory: 0x"
                  << std::hex << hr << std::endl;
        return 1;
    }

    std::cout << "DirectWrite factory created successfully" << std::endl;
    std::cout << std::endl;

    /* EXAMPLE 1: Check if Spline Sans Mono is installed */
    std::cout << "--- Example 1: Font Detection ---" << std::endl;
    bool isInstalled = IsSplineSansMonoInstalled(factory.Get());
    std::cout << std::endl;

    if (isInstalled) {
        /* EXAMPLE 2: Enumerate all weights and styles */
        std::cout << "--- Example 2: Enumerate Weights ---" << std::endl;
        EnumerateSplineSansMonoWeights(factory.Get());
        std::cout << std::endl;

        /* EXAMPLE 3: Create font face from system fonts */
        std::cout << "--- Example 3: Create Font Face (System) ---" << std::endl;
        auto fontFace = CreateSplineSansMonoFace_FromSystem(
            factory.Get(),
            DWRITE_FONT_WEIGHT_REGULAR,
            DWRITE_FONT_STYLE_NORMAL);

        if (fontFace) {
            /* EXAMPLE 4: Get optimal rendering mode */
            std::cout << std::endl;
            std::cout << "--- Example 4: Rendering Configuration ---" << std::endl;
            DWRITE_RENDERING_MODE renderingMode = GetOptimalRenderingMode(
                fontFace.Get(),
                11.0f,  /* font size in points */
                96.0f,  /* DPI X (standard monitor) */
                96.0f); /* DPI Y */

            /* EXAMPLE 5: Create rendering parameters */
            auto renderingParams = CreateOptimalRenderingParams(factory.Get());

            /* EXAMPLE 6: Validate monospace characteristics */
            std::cout << std::endl;
            std::cout << "--- Example 5: Monospace Validation ---" << std::endl;
            ValidateMonospaceMetrics(fontFace.Get());

            /* EXAMPLE 7: Print font metrics */
            std::cout << std::endl;
            std::cout << "--- Example 6: Font Metrics ---" << std::endl;
            PrintFontMetrics(fontFace.Get());
        }
    } else {
        std::cout << "Spline Sans Mono not found. Install the font and try again." << std::endl;
    }

    /* EXAMPLE 8: Load from file (for bundled fonts) */
    std::cout << std::endl;
    std::cout << "--- Example 7: Load from File ---" << std::endl;
    const wchar_t* fontPath = L"C:\\Users\\<USER>\\AppData\\Local\\Microsoft\\Windows\\Fonts\\SplineSansMono-Regular.ttf";
    std::wcout << L"Attempting to load: " << fontPath << std::endl;

    auto fontFaceFromFile = CreateSplineSansMonoFace_FromFile(factory.Get(), fontPath);
    if (fontFaceFromFile) {
        std::cout << "Successfully loaded font from file" << std::endl;
    } else {
        std::cout << "Failed to load font from file (check path)" << std::endl;
    }

    std::cout << std::endl;
    std::cout << "=== End of Examples ===" << std::endl;

    return 0;
}

/* ========================================================================
 * COMPILATION INSTRUCTIONS
 * ========================================================================
 *
 * Using MSVC (Visual Studio 2019+):
 * ----------------------------------
 * cl /EHsc /std:c++17 directwrite_integration_example.cpp dwrite.lib d2d1.lib
 *
 * Using Clang-CL:
 * ---------------
 * clang-cl /EHsc /std:c++17 directwrite_integration_example.cpp dwrite.lib d2d1.lib
 *
 * Using CMake:
 * ------------
 * add_executable(directwrite_example directwrite_integration_example.cpp)
 * target_link_libraries(directwrite_example dwrite d2d1)
 * target_compile_features(directwrite_example PRIVATE cxx_std_17)
 *
 * ========================================================================
 * RUNTIME REQUIREMENTS
 * ========================================================================
 *
 * - Windows 10 1809 or later (for IDWriteFontFace3)
 * - DirectWrite runtime (included in Windows)
 * - Spline Sans Mono installed (for system font examples)
 *
 * ========================================================================
 */
