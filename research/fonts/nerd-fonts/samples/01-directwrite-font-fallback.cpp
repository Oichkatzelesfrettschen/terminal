/*
 * DirectWrite Font Fallback for Nerd Fonts
 *
 * This sample demonstrates how to create a DirectWrite font fallback chain
 * that routes Nerd Fonts PUA (Private Use Area) glyphs to a Nerd Font while
 * using system fonts for standard characters.
 *
 * Research Date: 2025-10-11
 * Platform: Windows 11, DirectWrite 3.0+
 */

#include <dwrite_3.h>
#include <wrl/client.h>
#include <vector>
#include <stdexcept>

using Microsoft::WRL::ComPtr;

// Nerd Fonts PUA range definitions (v3.3.0)
struct PUARange {
    const wchar_t* name;
    uint32_t rangeStart;
    uint32_t rangeEnd;
};

const PUARange g_nerdFontPUARanges[] = {
    {L"Pomicons",              0xE000, 0xE00A},
    {L"Powerline",             0xE0A0, 0xE0D7},
    {L"Font Awesome Extension", 0xE200, 0xE2A9},
    {L"Weather Icons",         0xE300, 0xE3E3},
    {L"Seti-UI",               0xE5FA, 0xE6B7},
    {L"Devicons",              0xE700, 0xE8EF},
    {L"Codicons",              0xEA60, 0xEC1E},
    {L"Font Awesome",          0xED00, 0xF2FF},
    {L"Font Logos",            0xF300, 0xF381},
    {L"Octicons",              0xF400, 0xF533},
    {L"Material Design",       0xF0001, 0xF1AF0},
};

/*
 * Creates a DirectWrite font fallback chain for Nerd Fonts
 *
 * Maps all Nerd Fonts PUA ranges to the specified Nerd Font family,
 * then falls back to system fonts for non-PUA characters.
 *
 * @param factory - DirectWrite factory (must be IDWriteFactory5+)
 * @param nerdFontFamily - Name of Nerd Font (e.g., "CaskaydiaCove Nerd Font Mono")
 * @param fontFallback - Output: created font fallback chain
 * @return HRESULT - S_OK on success
 */
HRESULT CreateNerdFontFallback(
    IDWriteFactory5* factory,
    const wchar_t* nerdFontFamily,
    IDWriteFontFallback** fontFallback)
{
    if (!factory || !nerdFontFamily || !fontFallback) {
        return E_INVALIDARG;
    }

    // Create font fallback builder
    ComPtr<IDWriteFontFallbackBuilder> builder;
    HRESULT hr = factory->CreateFontFallbackBuilder(&builder);
    if (FAILED(hr)) {
        return hr;
    }

    // Add mappings for all Nerd Fonts PUA ranges
    for (const auto& range : g_nerdFontPUARanges) {
        DWRITE_UNICODE_RANGE unicodeRange = {
            range.rangeStart,
            range.rangeEnd
        };

        hr = builder->AddMapping(
            &unicodeRange, 1,           // Unicode ranges
            &nerdFontFamily, 1,         // Target font families
            nullptr,                    // Font collection (nullptr = system)
            L"",                        // Locale (empty = all locales)
            L"",                        // Base family name
            1.0f                        // Scale factor
        );

        if (FAILED(hr)) {
            return hr;
        }
    }

    // Add system font fallback for non-PUA characters
    ComPtr<IDWriteFontFallback> systemFallback;
    hr = factory->GetSystemFontFallback(&systemFallback);
    if (FAILED(hr)) {
        return hr;
    }

    hr = builder->AddMappings(systemFallback.Get());
    if (FAILED(hr)) {
        return hr;
    }

    // Build final fallback chain
    return builder->CreateFontFallback(fontFallback);
}

/*
 * Applies Nerd Fonts fallback to a text layout
 *
 * @param textLayout - DirectWrite text layout
 * @param fontFallback - Font fallback chain (from CreateNerdFontFallback)
 * @return HRESULT - S_OK on success
 */
HRESULT ApplyNerdFontFallback(
    IDWriteTextLayout* textLayout,
    IDWriteFontFallback* fontFallback)
{
    if (!textLayout || !fontFallback) {
        return E_INVALIDARG;
    }

    return textLayout->SetFontFallback(fontFallback);
}

/*
 * Example: Complete rendering pipeline with Nerd Fonts
 */
class NerdFontRenderer {
public:
    HRESULT Initialize(const wchar_t* nerdFontFamily) {
        // Create DirectWrite factory
        HRESULT hr = DWriteCreateFactory(
            DWRITE_FACTORY_TYPE_SHARED,
            __uuidof(IDWriteFactory5),
            reinterpret_cast<IUnknown**>(m_factory.GetAddressOf())
        );
        if (FAILED(hr)) {
            return hr;
        }

        // Create Nerd Font fallback chain
        hr = CreateNerdFontFallback(
            m_factory.Get(),
            nerdFontFamily,
            m_fontFallback.GetAddressOf()
        );
        if (FAILED(hr)) {
            return hr;
        }

        // Create text format
        hr = m_factory->CreateTextFormat(
            nerdFontFamily,                 // Font family
            nullptr,                        // Font collection (system)
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            12.0f,                          // Font size
            L"en-us",                       // Locale
            m_textFormat.GetAddressOf()
        );
        return hr;
    }

    HRESULT RenderText(const wchar_t* text, float maxWidth, float maxHeight) {
        // Create text layout
        ComPtr<IDWriteTextLayout> textLayout;
        HRESULT hr = m_factory->CreateTextLayout(
            text,
            static_cast<UINT32>(wcslen(text)),
            m_textFormat.Get(),
            maxWidth,
            maxHeight,
            textLayout.GetAddressOf()
        );
        if (FAILED(hr)) {
            return hr;
        }

        // Apply Nerd Font fallback
        hr = ApplyNerdFontFallback(textLayout.Get(), m_fontFallback.Get());
        if (FAILED(hr)) {
            return hr;
        }

        // Render text layout with Direct2D
        // (Assumes m_d2dContext is initialized)
        // m_d2dContext->DrawTextLayout(
        //     D2D1::Point2F(0, 0),
        //     textLayout.Get(),
        //     m_brush.Get()
        // );

        return S_OK;
    }

private:
    ComPtr<IDWriteFactory5> m_factory;
    ComPtr<IDWriteFontFallback> m_fontFallback;
    ComPtr<IDWriteTextFormat> m_textFormat;
    // ComPtr<ID2D1DeviceContext> m_d2dContext;
    // ComPtr<ID2D1SolidColorBrush> m_brush;
};

/*
 * Example usage
 */
int main() {
    // Initialize COM
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        return 1;
    }

    NerdFontRenderer renderer;

    // Initialize with Cascadia Code Nerd Font Mono
    hr = renderer.Initialize(L"CaskaydiaCove Nerd Font Mono");
    if (FAILED(hr)) {
        CoUninitialize();
        return 1;
    }

    // Render text with Nerd Font icons
    const wchar_t* text = L"  ~/projects/terminal  main* ";
    hr = renderer.RenderText(text, 1000.0f, 100.0f);

    CoUninitialize();
    return SUCCEEDED(hr) ? 0 : 1;
}

/*
 * Expected behavior:
 * - ASCII characters (~/projects/terminal, main*) render using system font
 * - Nerd Font icons (folder, git branch, powerline separators) render from Nerd Font
 * - Seamless fallback chain ensures all characters display correctly
 */
