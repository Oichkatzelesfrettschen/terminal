/*
 * Font Installation Check and Validation
 *
 * This sample demonstrates how to:
 * - Check if a Nerd Font is installed on the system
 * - Validate that specific PUA glyphs are supported
 * - Provide helpful error messages to users
 *
 * Research Date: 2025-10-11
 * Platform: Windows 11, DirectWrite
 */

#include <dwrite.h>
#include <wrl/client.h>
#include <string>
#include <vector>
#include <cstdio>

using Microsoft::WRL::ComPtr;

// Check if a specific font family is installed
bool IsFont installed(IDWriteFactory* factory, const wchar_t* fontFamily) {
    if (!factory || !fontFamily) {
        return false;
    }

    // Get system font collection
    ComPtr<IDWriteFontCollection> fontCollection;
    HRESULT hr = factory->GetSystemFontCollection(&fontCollection);
    if (FAILED(hr)) {
        return false;
    }

    // Search for font family
    UINT32 index;
    BOOL exists;
    hr = fontCollection->FindFamilyName(fontFamily, &index, &exists);
    if (FAILED(hr)) {
        return false;
    }

    return exists == TRUE;
}

// Get font face for a given font family
HRESULT GetFontFace(IDWriteFactory* factory, const wchar_t* fontFamily,
                    IDWriteFontFace** fontFace)
{
    if (!factory || !fontFamily || !fontFace) {
        return E_INVALIDARG;
    }

    // Get system font collection
    ComPtr<IDWriteFontCollection> fontCollection;
    HRESULT hr = factory->GetSystemFontCollection(&fontCollection);
    if (FAILED(hr)) {
        return hr;
    }

    // Find font family
    UINT32 index;
    BOOL exists;
    hr = fontCollection->FindFamilyName(fontFamily, &index, &exists);
    if (FAILED(hr) || !exists) {
        return E_FAIL;
    }

    ComPtr<IDWriteFontFamily> family;
    hr = fontCollection->GetFontFamily(index, &family);
    if (FAILED(hr)) {
        return hr;
    }

    // Get regular font
    ComPtr<IDWriteFont> font;
    hr = family->GetFirstMatchingFont(
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        &font
    );
    if (FAILED(hr)) {
        return hr;
    }

    // Create font face
    return font->CreateFontFace(fontFace);
}

// Check if font supports a specific codepoint
bool FontSupportsCodepoint(IDWriteFontFace* fontFace, uint32_t codepoint) {
    if (!fontFace) {
        return false;
    }

    BOOL exists;
    HRESULT hr = fontFace->HasCharacter(codepoint, &exists);
    if (FAILED(hr)) {
        return false;
    }

    return exists == TRUE;
}

// Validate Nerd Font installation and PUA support
class NerdFontValidator {
public:
    struct ValidationResult {
        bool fontInstalled;
        bool puaSupported;
        std::vector<std::wstring> missingIconSets;
        std::wstring errorMessage;
    };

    ValidationResult ValidateNerdFont(IDWriteFactory* factory,
                                      const wchar_t* nerdFontFamily)
    {
        ValidationResult result = {};

        // Check if font is installed
        result.fontInstalled = IsFontInstalled(factory, nerdFontFamily);
        if (!result.fontInstalled) {
            result.errorMessage = L"Nerd Font not installed: ";
            result.errorMessage += nerdFontFamily;
            return result;
        }

        // Get font face
        ComPtr<IDWriteFontFace> fontFace;
        HRESULT hr = GetFontFace(factory, nerdFontFamily, &fontFace);
        if (FAILED(hr)) {
            result.errorMessage = L"Failed to load font: ";
            result.errorMessage += nerdFontFamily;
            return result;
        }

        // Test critical PUA glyphs
        struct TestGlyph {
            const wchar_t* name;
            uint32_t codepoint;
        };

        const TestGlyph testGlyphs[] = {
            {L"Powerline separator", 0xE0B0},
            {L"Git branch",          0xE0A0},
            {L"Folder icon",         0xE5FA},
            {L"Git logo",            0xE702},
            {L"File icon",           0xF15B},
        };

        bool allSupported = true;
        for (const auto& glyph : testGlyphs) {
            if (!FontSupportsCodepoint(fontFace.Get(), glyph.codepoint)) {
                result.missingIconSets.push_back(glyph.name);
                allSupported = false;
            }
        }

        result.puaSupported = allSupported;

        if (!allSupported) {
            result.errorMessage = L"Font is missing some Nerd Font glyphs. ";
            result.errorMessage += L"This may not be a complete Nerd Font patch.";
        }

        return result;
    }

    // Display user-friendly error message
    void DisplayValidationError(const ValidationResult& result) {
        if (result.fontInstalled && result.puaSupported) {
            wprintf(L"Font validation passed!\n");
            return;
        }

        wprintf(L"Nerd Font Validation Error:\n");
        wprintf(L"%s\n\n", result.errorMessage.c_str());

        if (!result.fontInstalled) {
            wprintf(L"To fix this issue:\n");
            wprintf(L"1. Download Nerd Fonts from: https://www.nerdfonts.com/\n");
            wprintf(L"2. Install the font by double-clicking the .ttf file\n");
            wprintf(L"3. Restart this application\n");
        } else if (!result.puaSupported) {
            wprintf(L"Missing icon sets:\n");
            for (const auto& iconSet : result.missingIconSets) {
                wprintf(L"  - %s\n", iconSet.c_str());
            }
            wprintf(L"\nThis font may not be a complete Nerd Font patch.\n");
            wprintf(L"Download the complete version from: https://www.nerdfonts.com/\n");
        }
    }
};

// Check all common Nerd Font variants
void CheckCommonNerdFonts(IDWriteFactory* factory) {
    const wchar_t* commonNerdFonts[] = {
        L"CaskaydiaCove Nerd Font Mono",
        L"JetBrainsMono Nerd Font Mono",
        L"FiraCode Nerd Font Mono",
        L"Hack Nerd Font Mono",
        L"Meslo LG S Nerd Font Mono",
    };

    wprintf(L"Checking for installed Nerd Fonts:\n\n");

    for (const auto* fontName : commonNerdFonts) {
        if (IsFontInstalled(factory, fontName)) {
            wprintf(L"[OK] %s\n", fontName);
        } else {
            wprintf(L"[  ] %s\n", fontName);
        }
    }
}

// Get list of all installed Nerd Fonts
std::vector<std::wstring> GetInstalledNerdFonts(IDWriteFactory* factory) {
    std::vector<std::wstring> nerdFonts;

    // Get system font collection
    ComPtr<IDWriteFontCollection> fontCollection;
    HRESULT hr = factory->GetSystemFontCollection(&fontCollection);
    if (FAILED(hr)) {
        return nerdFonts;
    }

    // Iterate through all font families
    UINT32 familyCount = fontCollection->GetFontFamilyCount();
    for (UINT32 i = 0; i < familyCount; ++i) {
        ComPtr<IDWriteFontFamily> fontFamily;
        hr = fontCollection->GetFontFamily(i, &fontFamily);
        if (FAILED(hr)) {
            continue;
        }

        // Get font family name
        ComPtr<IDWriteLocalizedStrings> familyNames;
        hr = fontFamily->GetFamilyNames(&familyNames);
        if (FAILED(hr)) {
            continue;
        }

        UINT32 index = 0;
        BOOL exists;
        hr = familyNames->FindLocaleName(L"en-us", &index, &exists);
        if (FAILED(hr) || !exists) {
            index = 0;
        }

        UINT32 length;
        hr = familyNames->GetStringLength(index, &length);
        if (FAILED(hr)) {
            continue;
        }

        std::wstring familyName(length + 1, L'\0');
        hr = familyNames->GetString(index, &familyName[0], length + 1);
        if (FAILED(hr)) {
            continue;
        }

        familyName.resize(length);

        // Check if it's a Nerd Font (contains "Nerd Font" in name)
        if (familyName.find(L"Nerd Font") != std::wstring::npos) {
            nerdFonts.push_back(familyName);
        }
    }

    return nerdFonts;
}

// Example usage
int main() {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        wprintf(L"Failed to initialize COM\n");
        return 1;
    }

    // Create DirectWrite factory
    ComPtr<IDWriteFactory> factory;
    hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(factory.GetAddressOf())
    );

    if (FAILED(hr)) {
        wprintf(L"Failed to create DirectWrite factory\n");
        CoUninitialize();
        return 1;
    }

    // Check common Nerd Fonts
    wprintf(L"=== Nerd Font Installation Check ===\n\n");
    CheckCommonNerdFonts(factory.Get());

    wprintf(L"\n=== All Installed Nerd Fonts ===\n\n");
    auto installedNerdFonts = GetInstalledNerdFonts(factory.Get());
    if (installedNerdFonts.empty()) {
        wprintf(L"No Nerd Fonts detected.\n");
        wprintf(L"Download from: https://www.nerdfonts.com/\n");
    } else {
        for (const auto& font : installedNerdFonts) {
            wprintf(L"  - %s\n", font.c_str());
        }
    }

    // Validate a specific Nerd Font
    wprintf(L"\n=== Validating Cascadia Code Nerd Font ===\n\n");
    NerdFontValidator validator;
    auto result = validator.ValidateNerdFont(
        factory.Get(),
        L"CaskaydiaCove Nerd Font Mono"
    );

    validator.DisplayValidationError(result);

    CoUninitialize();
    return 0;
}

/*
 * Expected output (with Nerd Font installed):
 *
 * === Nerd Font Installation Check ===
 *
 * [OK] CaskaydiaCove Nerd Font Mono
 * [OK] JetBrainsMono Nerd Font Mono
 * [  ] FiraCode Nerd Font Mono
 * [  ] Hack Nerd Font Mono
 * [  ] Meslo LG S Nerd Font Mono
 *
 * === All Installed Nerd Fonts ===
 *
 *   - CaskaydiaCove Nerd Font Mono
 *   - JetBrainsMono Nerd Font Mono
 *   - CaskaydiaCove Nerd Font
 *   - JetBrainsMono Nerd Font
 *
 * === Validating Cascadia Code Nerd Font ===
 *
 * Font validation passed!
 */
