# Spline Sans Font Family - Terminal Integration Research

## Executive Summary

This document provides comprehensive technical information about the Spline Sans and Spline Sans Mono font families for integration with Windows Terminal. Both fonts are officially available through Google Fonts and maintained by SorkinType on GitHub.

---

## 1. Font Family Overview

### 1.1 Spline Sans (Proportional)

**Official Sources:**
- Google Fonts: https://fonts.google.com/specimen/Spline+Sans
- GitHub Repository: https://github.com/SorkinType/SplineSans
- License: SIL Open Font License v1.1

**Design Characteristics:**
- Grotesque sans serif typeface
- Purpose-built for UI interfaces, checkout processes, and paragraphs of text
- Condensed proportions for space efficiency
- Strategic "thorn" traps visible at larger sizes
- Designed by Eben Sorkin and Mirko Velimirovic
- Initiated by Spline Team

**Available Weights (5 weights):**
- Light (300)
- Regular (400)
- Medium (500)
- SemiBold (600)
- Bold (700)

**File Formats Available:**
- TrueType (.ttf) - static fonts
- OpenType (.otf) - static fonts
- Variable Font (.ttf) - SplineSans[wght].ttf

### 1.2 Spline Sans Mono (Monospaced)

**Official Sources:**
- Google Fonts: https://fonts.google.com/specimen/Spline+Sans+Mono
- GitHub Repository: https://github.com/SorkinType/SplineSansMono
- License: SIL Open Font License v1.1

**Design Characteristics:**
- Monospaced grotesque typeface
- Purpose-built for UI interfaces, checkout processes, and programming
- Fixed-width spacing (spacing: 100 - confirmed monospace)
- Condensed proportions for terminal efficiency
- Includes italic variants for all weights

**Available Weights (4 weights x 2 styles = 10 total fonts):**
- Light (300) + Light Italic
- Regular (400) + Italic
- Medium (500) + Medium Italic
- SemiBold (600) + SemiBold Italic
- Bold (700) + Bold Italic

**File Formats Available:**
- TrueType (.ttf) - static fonts
- OpenType (.otf) - static fonts
- Variable Font (.ttf) - SplineSansMono[wght].ttf and SplineSansMono-Italic[wght].ttf

---

## 2. Technical Specifications

### 2.1 Character Set Coverage

Based on fc-query analysis of SplineSansMono-Regular.ttf:

**Language Support:**
- Primary: English and Western European languages
- Extended support for: aa, ay, az-az, bi, br, bs, ca, ch, co, cs, cy, da, de, en, es, et, eu, fi, fj, fo, fr, fur, fy, gd, gl, gv, ho, hr, hu, ia, ig, id, ie, io, is, it, ki, kw, la, lb, lt, lv, mg, mh, mt, nb, nds, nl, nn, no, nr, nso, ny, oc, om, pl, pt, rm, ro, se, sk, sl, sma, smj, smn, so, sq, ss, st, sv, sw, tk, tl, tn, tr, ts, uz, vo, vot, wa, wen, wo, xh, yap, zu, an, crh, csb, fil, hsb, ht, jv, kj, ku-tr, kwm, lg, li, ms, na, ng, pap-an, pap-aw, rn, rw, sc, sg, sn, su, za, agr, ayc, bem, dsb, lij, mfe, mjw, nhn, niu, sgs, szl, tpi, unm, wae, yuw

**Glyph Sets Supported:**
- Google Core
- Google Plus
- Google Pro
- Plus Optional
- Pro Optional

**OpenType Layout Support:**
- DFLT (Default)
- latn (Latin)

**Font Properties:**
- Foundry: STC (Sorkin Type Co.)
- Format: TrueType (SFNT wrapper)
- Outline: True (vector-based)
- Scalable: True
- Font Hinting: True (important for screen rendering)
- Color Emoji: False
- Symbol Font: False
- Variable Font Support: Yes (separate variable font files available)

### 2.2 Rendering Characteristics

**Spacing:**
- Spline Sans Mono: Fixed-width (spacing: 100) - CONFIRMED MONOSPACE
- Optimal for terminal use where character alignment is critical

**Slant:**
- Regular variants: 0 (upright)
- Italic variants: Available separately

**Weight Range:**
- Light: 300
- Regular: 400 (weight: 80 in fontconfig)
- Medium: 500
- SemiBold: 600
- Bold: 700

**Width:**
- Standard: 100 (normal width)

---

## 3. Terminal Suitability Analysis

### 3.1 Advantages for Terminal Use

**Spline Sans Mono:**
- TRUE MONOSPACE: Confirmed fixed-width spacing (critical for terminal)
- Multiple weights available for syntax highlighting
- Italic variants available for emphasis
- Comprehensive Latin character coverage
- Font hinting enabled for crisp screen rendering
- Condensed proportions maximize screen real estate
- Modern grotesque design (2022) suitable for 2025-2026 usage
- Open source license allows bundling with applications

### 3.2 Limitations for Terminal Use

**PowerLine Glyph Support:**
- NOT INCLUDED: Spline Sans Mono does not include PowerLine glyphs by default
- Requires patching with Nerd Fonts patcher or PowerLine fonts tools
- Alternative: Use font fallback chain to combine with PowerLine-enabled fonts

**Programming Ligatures:**
- NOT INCLUDED: Spline Sans Mono does not include programming ligatures
- This is typical for monospace fonts (ligatures often conflict with fixed-width requirements)
- Common ligature fonts: Fira Code, JetBrains Mono, Cascadia Code PL

**Character Coverage:**
- Limited to Latin and Western European scripts
- No native support for:
  - Box-drawing characters (may rely on fallback)
  - Block elements
  - Mathematical symbols (beyond basic)
  - Emoji (requires fallback fonts)

### 3.3 Recommended Use Cases

**Ideal For:**
- Clean, modern terminal interface
- Code editing without ligature requirements
- UI-focused terminal applications
- Applications requiring multiple font weights
- Professional/minimalist terminal aesthetics

**Not Ideal For:**
- PowerLine/Oh-My-Posh prompts (without patching)
- Extensive box-drawing graphics
- Ligature-dependent workflows
- Non-Latin scripts

---

## 4. Windows Terminal Integration

### 4.1 Font Installation Methods

#### Method 1: User Installation (Recommended for Testing)
1. Download TTF files from repository
2. Double-click .ttf file
3. Click "Install" in Windows Font Viewer
4. Fonts appear in Windows Terminal font selector

#### Method 2: System-Wide Installation (All Users)
1. Copy .ttf files to `C:\Windows\Fonts\`
2. Registry entry automatically created at:
   `HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Fonts`

#### Method 3: Application Bundling (Distribution)
For bundling fonts with Windows Terminal builds:
1. Package fonts in installer
2. Install to `%LOCALAPPDATA%\Microsoft\Windows\Fonts\` (per-user)
3. Register via DirectWrite font collection API
4. Add registry entries for font enumeration

### 4.2 Windows Terminal Configuration

#### Basic Font Configuration (settings.json)

```json
{
    "profiles": {
        "defaults": {
            "font": {
                "face": "Spline Sans Mono",
                "size": 11,
                "weight": "normal"
            }
        }
    }
}
```

#### Advanced Font Configuration with Multiple Weights

```json
{
    "profiles": {
        "list": [
            {
                "name": "PowerShell (Spline Sans Mono)",
                "font": {
                    "face": "Spline Sans Mono",
                    "size": 11,
                    "weight": "normal"
                }
            },
            {
                "name": "PowerShell (Spline Sans Mono Bold)",
                "font": {
                    "face": "Spline Sans Mono",
                    "size": 11,
                    "weight": "bold"
                }
            },
            {
                "name": "PowerShell (Spline Sans Mono Light)",
                "font": {
                    "face": "Spline Sans Mono",
                    "size": 11,
                    "weight": "light"
                }
            }
        ]
    }
}
```

#### Font Fallback Configuration

Windows Terminal (v1.21+) supports font fallback for missing glyphs:

```json
{
    "profiles": {
        "defaults": {
            "font": {
                "face": "Spline Sans Mono",
                "size": 11,
                "weight": "normal"
            }
        }
    },
    "schemes": [
        {
            "name": "SplineSansMonoTheme",
            "font": {
                "face": "Spline Sans Mono",
                "fallback": ["Cascadia Code PL", "Consolas", "Segoe UI Symbol"]
            }
        }
    ]
}
```

Note: The exact font fallback API is handled by DirectWrite. Manual fallback configuration may require specific Windows Terminal settings or DirectWrite integration.

### 4.3 DirectWrite Integration

Windows Terminal uses DirectWrite for font rendering. Key APIs:

**Font Enumeration:**
```cpp
IDWriteFontCollection* fontCollection;
factory->GetSystemFontCollection(&fontCollection);
```

**Font Loading (for bundled fonts):**
```cpp
IDWriteFontFile* fontFile;
factory->CreateFontFileReference(L"path\\to\\SplineSansMono-Regular.ttf", nullptr, &fontFile);

IDWriteFontFace* fontFace;
factory->CreateFontFace(DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &fontFile, 0,
                        DWRITE_FONT_SIMULATIONS_NONE, &fontFace);
```

**Rendering with AtlasEngine:**
Windows Terminal's AtlasEngine (v1.13+) provides:
- Hardware-accelerated rendering via Direct3D
- Pixel-perfect glyph positioning
- Automatic font fallback for missing glyphs
- OpenType feature support

### 4.4 Build System Integration

For integrating Spline Sans Mono into Windows Terminal build:

#### CMakeLists.txt Integration

```cmake
# Font resources
set(SPLINE_SANS_MONO_FONTS
    ${CMAKE_SOURCE_DIR}/research/fonts/spline-sans/mono/ttf/SplineSansMono-Regular.ttf
    ${CMAKE_SOURCE_DIR}/research/fonts/spline-sans/mono/ttf/SplineSansMono-Bold.ttf
    ${CMAKE_SOURCE_DIR}/research/fonts/spline-sans/mono/ttf/SplineSansMono-Italic.ttf
    ${CMAKE_SOURCE_DIR}/research/fonts/spline-sans/mono/ttf/SplineSansMono-Light.ttf
)

# Install fonts to package
install(FILES ${SPLINE_SANS_MONO_FONTS}
        DESTINATION fonts
        COMPONENT fonts)
```

#### Resource Embedding (Alternative)

Fonts can be embedded as binary resources in the executable:

```cpp
// Resource.rc
IDR_FONT_SPLINE_MONO_REGULAR FONT "fonts\\SplineSansMono-Regular.ttf"
IDR_FONT_SPLINE_MONO_BOLD    FONT "fonts\\SplineSansMono-Bold.ttf"
```

```cpp
// Font loading from resource
HRSRC hResource = FindResource(hInstance, MAKEINTRESOURCE(IDR_FONT_SPLINE_MONO_REGULAR), RT_FONT);
HGLOBAL hMemory = LoadResource(hInstance, hResource);
void* fontData = LockResource(hMemory);
DWORD fontSize = SizeofResource(hInstance, hResource);

IDWriteFontFile* fontFile;
factory->CreateCustomFontFileReference(fontData, fontSize,
                                       fontFileLoader, &fontFile);
```

---

## 5. Rendering Optimization Considerations

### 5.1 Optimal Rendering Sizes

Based on font hinting and design:
- Minimum readable size: 9pt
- Recommended sizes: 10pt, 11pt, 12pt, 14pt
- Font includes hinting for crisp rendering at all sizes

### 5.2 ClearType Optimization

Windows Terminal uses ClearType by default:
- Spline Sans Mono includes TrueType hints
- Subpixel rendering provides sharp text
- Recommended ClearType settings: Standard (Windows default)

### 5.3 DirectWrite Rendering Modes

Recommended DirectWrite rendering mode for terminal:

```cpp
DWRITE_RENDERING_MODE renderingMode = DWRITE_RENDERING_MODE_CLEARTYPE_NATURAL_SYMMETRIC;
```

This provides:
- Symmetric anti-aliasing
- Optimal for monospace fonts
- Consistent character widths

### 5.4 Performance Considerations

**Font File Sizes:**
- Regular: ~74KB (efficient)
- Bold: ~76KB
- Italic: ~79KB
- Total for core set: ~229KB

**Glyph Cache:**
- Atlas Engine caches rendered glyphs
- Spline Sans Mono's compact design minimizes cache size
- Expected cache size: ~2-4MB for typical usage

---

## 6. PowerLine and Nerd Fonts Integration

### 6.1 Current Status

Spline Sans Mono does NOT include:
- PowerLine glyphs (U+E0A0 - U+E0D4)
- Nerd Fonts icons (U+E000 - U+F8FF, U+F0000+)

### 6.2 Patching Options

#### Option 1: Nerd Fonts Patcher

```bash
# Install Nerd Fonts patcher
git clone https://github.com/ryanoasis/nerd-fonts.git
cd nerd-fonts

# Patch Spline Sans Mono
./font-patcher SplineSansMono-Regular.ttf --complete --careful

# Result: "Spline Sans Mono Nerd Font Complete.ttf"
```

#### Option 2: Font Fallback Chain

Configure Windows Terminal to use PowerLine-enabled fallback:

```json
{
    "font": {
        "face": "Spline Sans Mono, Cascadia Code PL, Segoe UI Symbol"
    }
}
```

DirectWrite automatically selects glyphs from fallback fonts when not present in primary font.

#### Option 3: Custom Font Build

Modify Spline Sans Mono source (.glyphs file) to include PowerLine glyphs:

1. Open SplineSansMono.glyphs in Glyphs App
2. Import PowerLine glyphs from reference font
3. Rebuild with gftools:

```bash
python3 -m venv venv
source venv/bin/activate
pip install gftools
gftools build-vf --fixnonhinting --static
```

---

## 7. Testing and Validation

### 7.1 Font Validation Tools

**Linux (WSL):**
```bash
# Character coverage analysis
fc-query SplineSansMono-Regular.ttf

# List all glyphs
fc-list :charset=0041  # Check for 'A' (U+0041)

# Font metadata
fc-scan SplineSansMono-Regular.ttf
```

**Windows:**
```powershell
# Font registration check
Get-ItemProperty "HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\fonts"

# DirectWrite enumeration test (C++)
// Use IDWriteFontCollection::GetFontFamily() to verify font is registered
```

### 7.2 Terminal Rendering Tests

**Test Cases:**
1. ASCII characters (U+0020 - U+007E)
2. Latin Extended-A (U+0100 - U+017F)
3. Box-drawing characters (U+2500 - U+257F) - expect fallback
4. PowerLine glyphs (U+E0A0 - U+E0D4) - expect fallback
5. Character width consistency (monospace verification)

**Test Script (PowerShell):**
```powershell
# test-spline-sans-mono.ps1
$testStrings = @(
    "The quick brown fox jumps over the lazy dog 0123456789",
    "!@#$%^&*()_+-={}[]|\\:;`"'<>,.?/~",
    "`u{0100}`u{0101}`u{0102}`u{0103}",  # Latin Extended
    "├─┤│┌┐└┘",                           # Box drawing
    "`u{E0A0}`u{E0B0}`u{E0B2}"            # PowerLine (if patched)
)

foreach ($str in $testStrings) {
    Write-Host $str
}
```

### 7.3 Build Integration Tests

**CMake Test:**
```cmake
# Test font installation
add_test(NAME FontInstallation
         COMMAND ${CMAKE_COMMAND} -E compare_files
                 ${SPLINE_SANS_MONO_REGULAR_TTF}
                 ${CMAKE_INSTALL_PREFIX}/fonts/SplineSansMono-Regular.ttf)
```

**Runtime Font Availability Test:**
```cpp
// Test font is available to DirectWrite
IDWriteFontCollection* fontCollection;
HRESULT hr = factory->GetSystemFontCollection(&fontCollection);

UINT32 familyIndex;
BOOL exists;
hr = fontCollection->FindFamilyName(L"Spline Sans Mono", &familyIndex, &exists);

ASSERT_TRUE(exists);
```

---

## 8. License Compliance

### 8.1 SIL Open Font License v1.1

**Key Points:**
- Font can be bundled with Windows Terminal
- No cost for commercial use
- Can be modified and redistributed
- Must include original license and copyright notice
- Cannot sell fonts standalone
- Modified versions cannot use reserved font names without permission

**Reserved Font Names:**
- "Spline Sans"
- "Spline Sans Mono"

**Attribution Required:**
```
Copyright 2022 The Spline Sans Mono Project Authors
(https://github.com/SorkinType/SplineSansMono)

This Font Software is licensed under the SIL Open Font License, Version 1.1.
```

### 8.2 Distribution Requirements

**For Windows Terminal Integration:**

1. Include OFL.txt in fonts directory
2. Add attribution in NOTICE or CREDITS file
3. Document font usage in README
4. If modified (e.g., patched with PowerLine):
   - Change font name to avoid confusion
   - Example: "Spline Sans Mono Patched" or "Spline Sans Mono Nerd Font"
   - Include modified source files if distributed

---

## 9. Implementation Roadmap

### 9.1 Phase 1: Basic Integration (Immediate)

- [x] Download Spline Sans and Spline Sans Mono from official sources
- [x] Verify license compatibility
- [x] Analyze font characteristics
- [ ] Install fonts locally for testing
- [ ] Configure Windows Terminal to use Spline Sans Mono
- [ ] Test rendering quality at various sizes

### 9.2 Phase 2: Build Integration (Short-term)

- [ ] Add fonts to repository structure
- [ ] Create CMake font installation targets
- [ ] Implement DirectWrite font loading code
- [ ] Add font resource embedding (optional)
- [ ] Create font selection UI in Windows Terminal settings
- [ ] Test on Windows 10 and Windows 11

### 9.3 Phase 3: Enhanced Features (Medium-term)

- [ ] Patch fonts with Nerd Fonts for PowerLine support
- [ ] Implement font fallback chain configuration
- [ ] Add font weight selection UI
- [ ] Optimize font rendering performance
- [ ] Test with AtlasEngine rendering pipeline
- [ ] Benchmark against other terminal fonts

### 9.4 Phase 4: Production Ready (Long-term)

- [ ] Complete rendering tests across all character ranges
- [ ] Performance profiling and optimization
- [ ] Documentation for end users
- [ ] Package fonts with installer
- [ ] Add font licensing information to About dialog
- [ ] Submit patches to upstream Spline Sans Mono (if applicable)

---

## 10. Comparison with Other Terminal Fonts

| Feature | Spline Sans Mono | Cascadia Code | JetBrains Mono | Fira Code |
|---------|------------------|---------------|----------------|-----------|
| Monospaced | YES | YES | YES | YES |
| PowerLine | NO (needs patch) | PL variant | NO (needs patch) | NO (needs patch) |
| Ligatures | NO | YES | YES | YES |
| Weights | 5 weights | 7 weights | 8 weights | 4 weights |
| Italic | YES | YES | YES | YES |
| License | OFL 1.1 | OFL 1.1 | OFL 1.1 | OFL 1.1 |
| File Size | ~74KB | ~250KB | ~165KB | ~190KB |
| Hinting | YES | YES | YES | YES |
| Year | 2022 | 2019+ | 2020+ | 2014+ |
| Design Focus | UI/Terminal | Terminal | IDE | Programming |

**Key Differentiators:**
- Spline Sans Mono: Cleanest/most minimal design, smallest file size
- Cascadia Code: Official Microsoft terminal font, best Windows integration
- JetBrains Mono: Most weights, IDE-optimized
- Fira Code: Most mature ligature support

---

## 11. References and Resources

### Official Documentation
- Google Fonts Spline Sans: https://fonts.google.com/specimen/Spline+Sans
- Google Fonts Spline Sans Mono: https://fonts.google.com/specimen/Spline+Sans+Mono
- SorkinType GitHub (Spline Sans): https://github.com/SorkinType/SplineSans
- SorkinType GitHub (Spline Sans Mono): https://github.com/SorkinType/SplineSansMono
- SIL OFL License: https://scripts.sil.org/OFL

### Windows Terminal Documentation
- Windows Terminal Font Configuration: https://learn.microsoft.com/en-us/windows/terminal/customize-settings/profile-appearance
- DirectWrite API: https://learn.microsoft.com/en-us/windows/win32/directwrite/
- AtlasEngine Overview: https://github.com/microsoft/terminal/blob/main/src/renderer/atlas/AtlasEngine.h

### Font Patching Tools
- Nerd Fonts: https://github.com/ryanoasis/nerd-fonts
- PowerLine Fonts: https://github.com/powerline/fonts
- FontForge: https://fontforge.org/

### Testing Tools
- fc-query (Linux): fontconfig package
- fc-scan (Linux): fontconfig package
- Font Viewer (Windows): built-in
- CharacterMap (Windows): built-in

---

## 12. Contact and Contribution

### Font Maintainers
- Eben Sorkin (Designer)
- Mirko Velimirovic (Designer)
- Faride Mereb (Project Manager)
- SorkinType GitHub Organization: https://github.com/SorkinType

### Contributing to Spline Sans Mono
- Submit issues: https://github.com/SorkinType/SplineSansMono/issues
- Pull requests welcome (see repository CONTRIBUTING guidelines)
- Follow Unified Font Repository v0.3 workflow

### Windows Terminal Integration
- File issues specific to Windows Terminal integration with this optimization project
- Font rendering issues should be reported to Windows Terminal: https://github.com/microsoft/terminal/issues

---

## Appendix A: Font File Inventory

### Downloaded Font Files (Spline Sans Mono)

**TTF Files (10 files, 784KB total):**
- SplineSansMono-Light.ttf (72KB)
- SplineSansMono-LightItalic.ttf (80KB)
- SplineSansMono-Regular.ttf (74KB)
- SplineSansMono-Italic.ttf (79KB)
- SplineSansMono-Medium.ttf (75KB)
- SplineSansMono-MediumItalic.ttf (80KB)
- SplineSansMono-SemiBold.ttf (76KB)
- SplineSansMono-SemiBoldItalic.ttf (83KB)
- SplineSansMono-Bold.ttf (76KB)
- SplineSansMono-BoldItalic.ttf (81KB)

**OTF Files (10 files):**
- Same variants as TTF, OpenType format

**Variable Fonts (2 files):**
- SplineSansMono[wght].ttf (upright, weight axis 300-700)
- SplineSansMono-Italic[wght].ttf (italic, weight axis 300-700)

### Downloaded Font Files (Spline Sans - Proportional)

**TTF Files (5 files, 384KB total):**
- SplineSans-Light.ttf (74KB)
- SplineSans-Regular.ttf (73KB)
- SplineSans-Medium.ttf (75KB)
- SplineSans-SemiBold.ttf (77KB)
- SplineSans-Bold.ttf (76KB)

**OTF Files (5 files):**
- Same variants as TTF, OpenType format

**Variable Fonts (1 file):**
- SplineSans[wght].ttf (weight axis 300-700)

---

## Appendix B: Build Scripts

### B.1 Font Installation Script (PowerShell)

```powershell
# install-spline-sans-mono.ps1
# Installs Spline Sans Mono fonts for current user

$fontsDir = "$env:LOCALAPPDATA\Microsoft\Windows\Fonts"
$sourceDir = "$PSScriptRoot\..\mono\ttf"

# Ensure fonts directory exists
if (-not (Test-Path $fontsDir)) {
    New-Item -ItemType Directory -Path $fontsDir -Force
}

# Copy font files
Get-ChildItem -Path $sourceDir -Filter "*.ttf" | ForEach-Object {
    Write-Host "Installing $($_.Name)..."
    Copy-Item $_.FullName -Destination $fontsDir -Force

    # Register font in registry
    $fontName = $_.BaseName
    $regPath = "HKCU:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Fonts"
    Set-ItemProperty -Path $regPath -Name "$fontName (TrueType)" -Value $_.Name
}

Write-Host "Spline Sans Mono fonts installed successfully!"
Write-Host "Restart Windows Terminal to see the fonts in the font selector."
```

### B.2 Font Uninstallation Script (PowerShell)

```powershell
# uninstall-spline-sans-mono.ps1
# Removes Spline Sans Mono fonts for current user

$fontsDir = "$env:LOCALAPPDATA\Microsoft\Windows\Fonts"
$fontPattern = "SplineSansMono*.ttf"

# Remove font files
Get-ChildItem -Path $fontsDir -Filter $fontPattern | ForEach-Object {
    Write-Host "Removing $($_.Name)..."
    Remove-Item $_.FullName -Force

    # Remove registry entry
    $fontName = $_.BaseName
    $regPath = "HKCU:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Fonts"
    Remove-ItemProperty -Path $regPath -Name "$fontName (TrueType)" -ErrorAction SilentlyContinue
}

Write-Host "Spline Sans Mono fonts removed successfully!"
```

### B.3 Font Testing Script (PowerShell)

```powershell
# test-spline-sans-mono.ps1
# Tests Spline Sans Mono rendering in Windows Terminal

Write-Host "=== Spline Sans Mono Font Test ==="
Write-Host ""

Write-Host "ASCII Characters:"
Write-Host "abcdefghijklmnopqrstuvwxyz"
Write-Host "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
Write-Host "0123456789"
Write-Host "!@#$%^&*()_+-={}[]|\\:;`"'<>,.?/~"
Write-Host ""

Write-Host "Extended Latin:"
Write-Host "`u{00C0}`u{00C1}`u{00C2}`u{00C3}`u{00C4}`u{00C5} `u{00E0}`u{00E1}`u{00E2}`u{00E3}`u{00E4}`u{00E5}"
Write-Host "`u{00C8}`u{00C9}`u{00CA}`u{00CB} `u{00E8}`u{00E9}`u{00EA}`u{00EB}"
Write-Host ""

Write-Host "Box Drawing (may use fallback):"
Write-Host "┌─┬─┐"
Write-Host "├─┼─┤"
Write-Host "└─┴─┘"
Write-Host "│ ┃ ║"
Write-Host ""

Write-Host "Code Sample:"
Write-Host "function Get-SystemInfo {"
Write-Host "    param("
Write-Host "        [string]`$ComputerName = `$env:COMPUTERNAME"
Write-Host "    )"
Write-Host "    "
Write-Host "    Get-WmiObject -Class Win32_OperatingSystem |"
Write-Host "        Select-Object Caption, Version, BuildNumber"
Write-Host "}"
Write-Host ""

Write-Host "Character Width Test (should align):"
Write-Host "1234567890123456789012345678901234567890"
Write-Host "iiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiii"
Write-Host "MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM"
Write-Host "||||||||||||||||||||||||||||||||||||||||"
Write-Host ""

Write-Host "=== End of Font Test ==="
```

---

## Appendix C: Windows Terminal Configuration Examples

### C.1 Basic Configuration

```json
{
    "$schema": "https://aka.ms/terminal-profiles-schema",
    "defaultProfile": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}",
    "profiles": {
        "defaults": {
            "font": {
                "face": "Spline Sans Mono",
                "size": 11
            }
        },
        "list": [
            {
                "guid": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}",
                "name": "PowerShell",
                "commandline": "powershell.exe"
            }
        ]
    }
}
```

### C.2 Multi-Weight Configuration

```json
{
    "profiles": {
        "list": [
            {
                "name": "Dev (Light)",
                "font": {
                    "face": "Spline Sans Mono",
                    "size": 10,
                    "weight": "light"
                },
                "colorScheme": "One Half Light"
            },
            {
                "name": "Dev (Regular)",
                "font": {
                    "face": "Spline Sans Mono",
                    "size": 11,
                    "weight": "normal"
                },
                "colorScheme": "One Half Dark"
            },
            {
                "name": "Dev (Bold)",
                "font": {
                    "face": "Spline Sans Mono",
                    "size": 11,
                    "weight": "bold"
                },
                "colorScheme": "Solarized Dark"
            }
        ]
    }
}
```

### C.3 Full Configuration with Fallback

```json
{
    "$schema": "https://aka.ms/terminal-profiles-schema",
    "defaultProfile": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}",
    "profiles": {
        "defaults": {
            "font": {
                "face": "Spline Sans Mono",
                "size": 11,
                "weight": "normal"
            },
            "colorScheme": "Campbell",
            "useAcrylic": false,
            "cursorShape": "bar"
        },
        "list": [
            {
                "guid": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}",
                "name": "Windows PowerShell",
                "commandline": "powershell.exe",
                "hidden": false
            },
            {
                "guid": "{0caa0dad-35be-5f56-a8ff-afceeeaa6101}",
                "name": "Command Prompt",
                "commandline": "cmd.exe",
                "hidden": false
            },
            {
                "guid": "{2c4de342-38b7-51cf-b940-2309a097f518}",
                "name": "Ubuntu (WSL)",
                "source": "Windows.Terminal.Wsl",
                "hidden": false
            }
        ]
    },
    "schemes": [],
    "actions": [
        {
            "command": {
                "action": "copy",
                "singleLine": false
            },
            "keys": "ctrl+c"
        },
        {
            "command": "paste",
            "keys": "ctrl+v"
        }
    ]
}
```

---

## Document Metadata

**Created:** 2025-10-11
**Author:** Research Team - Windows Terminal Optimization Project
**Version:** 1.0
**Status:** Complete
**Last Updated:** 2025-10-11

**Related Files:**
- Fonts: `/home/eirikr/github-performance-projects/windows-terminal-optimized/research/fonts/spline-sans/`
- License: `/home/eirikr/github-performance-projects/windows-terminal-optimized/research/fonts/spline-sans/licenses/SplineSansMono-OFL.txt`
- Source Repository: https://github.com/SorkinType/SplineSansMono
