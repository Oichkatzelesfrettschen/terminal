# Unicode Private Use Area (PUA) Code Points

**Nerd Fonts Version:** 3.3.0
**Research Date:** 2025-10-11

---

## Overview

Nerd Fonts uses the **Unicode Private Use Area (PUA)** to map icon glyphs without conflicting with standard Unicode characters. PUA code points are reserved for private use and will never be assigned official meanings by the Unicode Consortium.

---

## Unicode Private Use Area Ranges

The Unicode Standard defines three Private Use Areas:

| Range | Name | Total Code Points |
|-------|------|-------------------|
| `U+E000` - `U+F8FF` | Basic Multilingual Plane (BMP) PUA | 6,400 |
| `U+F0000` - `U+FFFFD` | Supplementary Private Use Area-A | 65,534 |
| `U+100000` - `U+10FFFD` | Supplementary Private Use Area-B | 65,534 |

**Nerd Fonts primarily uses:** `U+E000` - `U+F8FF` and `U+F0000` - `U+FFFFD`

---

## Nerd Fonts Glyph Set Mappings (v3.3.0)

### Complete Code Point Ranges by Icon Set

| Icon Set | Code Point Range | Glyph Count | Notes |
|----------|------------------|-------------|-------|
| **Pomicons** | `U+E000` - `U+E00A` | 11 | Original Powerline-Extra icons |
| **Powerline Symbols** | `U+E0A0` - `U+E0A2`, `U+E0B0` - `U+E0B3` | 7 | Core Powerline separators |
| **Powerline Extra Symbols** | `U+E0A3`, `U+E0B4` - `U+E0C8`, `U+E0CA`, `U+E0CC` - `U+E0D7` | 38 | Extended Powerline glyphs |
| **Font Awesome Extension** | `U+E200` - `U+E2A9` | 170 | Additional Font Awesome icons |
| **Weather Icons** | `U+E300` - `U+E3E3` | 228 | Weather condition symbols |
| **Seti-UI + Custom** | `U+E5FA` - `U+E6B7` | 190 | File type icons |
| **Devicons** | `U+E700` - `U+E8EF` | 496 | Development tool icons |
| **Codicons** | `U+EA60` - `U+EC1E` | 447 | VS Code icon set |
| **Font Awesome** | `U+ED00` - `U+F2FF` | 1,536 | Main Font Awesome collection (with gaps) |
| **Font Logos** | `U+F300` - `U+F381` | 130 | Brand/technology logos |
| **Octicons** | `U+F400` - `U+F533`, `U+2665`, `U+26A1` | 308 | GitHub icons |
| **Material Design Icons** | `U+F0001` - `U+F1AF0` | 7,000+ | Google Material Design (Supplementary PUA-A) |
| **IEC Power Symbols** | `U+23FB` - `U+23FE`, `U+2B58` | 5 | Power state indicators (Standard Unicode) |

**Total Icon Glyphs:** 10,390+

---

## Code Point Range Details

### Basic Multilingual Plane (BMP) PUA: `U+E000` - `U+F8FF`

This is the most commonly used PUA range by Nerd Fonts.

#### Powerline and Powerline Extra (`U+E0A0` - `U+E0D7`)
Used for terminal prompt separators and indicators:

**Core Powerline Symbols:**
- `U+E0A0` ():  Branch symbol
- `U+E0A1` (): Line number
- `U+E0A2` (): Closed lock
- `U+E0B0` (): Right-pointing triangle (separator)
- `U+E0B1` (): Right-pointing angle (separator)
- `U+E0B2` (): Left-pointing triangle (separator)
- `U+E0B3` (): Left-pointing angle (separator)

**Powerline Extra Symbols:**
- `U+E0B4` - `U+E0C8`: Additional separators and shapes
- `U+E0CA`: Flame icon
- `U+E0CC` - `U+E0D7`: Branch variants and symbols

#### Font Awesome Extension (`U+E200` - `U+E2A9`)
Supplementary Font Awesome icons not in the main collection.

#### Weather Icons (`U+E300` - `U+E3E3`)
Weather condition symbols (sun, cloud, rain, snow, etc.).

#### Seti-UI + Custom (`U+E5FA` - `U+E6B7`)
File type icons for common programming languages and file extensions.

#### Devicons (`U+E700` - `U+E8EF`)
Development tool and language logos (Git, Docker, Python, JavaScript, etc.).

#### Codicons (`U+EA60` - `U+EC1E`)
Visual Studio Code's icon set for UI elements and file types.

#### Font Awesome (`U+ED00` - `U+F2FF`)
The main Font Awesome icon collection (note: contains gaps for deprecated glyphs).

#### Font Logos (`U+F300` - `U+F381`)
Technology and brand logos (AWS, Linux, Windows, etc.).

#### Octicons (`U+F400` - `U+F533`)
GitHub's icon set for version control and collaboration symbols.

---

### Supplementary Private Use Area-A: `U+F0000` - `U+FFFFD`

#### Material Design Icons (`U+F0001` - `U+F1AF0`)
Google's extensive Material Design icon collection (7,000+ glyphs).

**Note:** This range requires fonts to support Supplementary Planes (beyond BMP), which may have compatibility issues with older rendering systems.

---

## Version Compatibility: v2.x vs v3.x

### Breaking Changes in v3.0

Nerd Fonts v3.0 **significantly reorganized PUA mappings**, making it incompatible with v2.x:

| Icon Set | v2.x Range | v3.x Range | Status |
|----------|-----------|-----------|--------|
| Font Awesome | `U+F000` - `U+F2FF` | `U+ED00` - `U+F2FF` | **Changed** |
| Seti-UI | `U+E5FA` - `U+E62B` | `U+E5FA` - `U+E6B7` | **Extended** |
| Material Design | `U+F500` - `U+FD46` | `U+F0001` - `U+F1AF0` | **Moved to Supplementary PUA** |

**Impact:**
- Applications using v2.x fonts with v3.x code point expectations will display incorrect icons
- Terminal prompt themes designed for v2.x may break with v3.x fonts
- Always verify Nerd Fonts version compatibility

**Recommendation:** Use v3.3.0 (latest) for all new projects in 2025.

---

## Code Point Lookup Tools

### 1. Nerd Fonts Cheat Sheet
**URL:** https://www.nerdfonts.com/cheat-sheet

- Browse all 10,390+ icons visually
- Search by icon name or category
- Copy Unicode code points directly
- Filter by icon set

### 2. Font Viewer Tools

**Windows:**
- Character Map (`charmap.exe`)
- Microsoft Font Viewer

**macOS:**
- Font Book (Command + T for character viewer)
- Character Viewer (Control + Command + Space)

**Linux:**
- GNOME Character Map (`gucharmap`)
- KDE Character Selector (`kcharselect`)

### 3. Programmatic Lookup

**Python (using `fonttools`):**
```python
from fontTools.ttLib import TTFont

font = TTFont('JetBrainsMonoNerdFont-Regular.ttf')
cmap = font.getBestCmap()

# Find all glyphs in PUA range
pua_glyphs = {code: name for code, name in cmap.items() if 0xE000 <= code <= 0xF8FF}
print(f"Found {len(pua_glyphs)} PUA glyphs in BMP")
```

---

## DirectWrite / Windows API Considerations

### Font Fallback for PUA

Windows Terminal and DirectWrite-based applications handle PUA characters differently:

1. **Primary Font Check**: DirectWrite first checks if the primary font contains the glyph
2. **Fallback Chain**: If not found, DirectWrite uses system fallback (Segoe UI Symbol, etc.)
3. **PUA Not in Fallback Data**: System fallback does NOT include PUA mappings by default

**Solution:** Explicitly specify Nerd Font as the primary font to ensure PUA glyphs render correctly.

### Font Linking (Legacy)

Older Windows applications used font linking via registry:
```
HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows NT\CurrentVersion\FontLink\SystemLink
```

**Not recommended** for modern applications. Use DirectWrite font fallback APIs instead.

---

## Rendering Considerations

### Glyph Width Variations

Nerd Fonts provides multiple variants for different use cases:

| Variant | Glyph Width Handling | Best For |
|---------|---------------------|----------|
| **NerdFont** | Variable width (some double-width) | Text editors, IDEs |
| **NerdFontMono** | Strictly single-width | Terminals, code editors requiring perfect alignment |
| **NerdFontPropo** | Proportional width | UI text, non-monospaced contexts |

**Terminal Recommendation:** Use `NerdFontMono` to prevent alignment issues.

### Font Metrics and Line Height

Nerd Fonts may alter original font metrics to accommodate icon glyphs:

- **Ascent/Descent**: Adjusted to fit larger icon glyphs
- **Line Height**: May increase to prevent icon clipping
- **Baseline Alignment**: Icons centered on baseline for consistent rendering

**Impact:** Line spacing may differ slightly from unpatched font versions.

---

## Integration Code Samples

### C++ (DirectWrite)

```cpp
#include <dwrite.h>

// Create font collection with Nerd Font
IDWriteFontCollection* fontCollection = nullptr;
IDWriteFontFamily* fontFamily = nullptr;
IDWriteFont* font = nullptr;

// Get font for PUA glyph range
UINT32 index;
BOOL exists;
fontFamily->GetFirstMatchingFont(DWRITE_FONT_WEIGHT_NORMAL,
                                  DWRITE_FONT_STRETCH_NORMAL,
                                  DWRITE_FONT_STYLE_NORMAL,
                                  &font);

// Check if font supports PUA code point
BOOL hasGlyph;
font->HasCharacter(0xE0B0, &hasGlyph);  // Check for Powerline separator
```

### C# (.NET)

```csharp
using System.Windows.Media;

// Create Nerd Font family
FontFamily nerdFont = new FontFamily("JetBrainsMono Nerd Font");

// Render PUA glyph
string powerlineIcon = "\uE0B0";  // Right-pointing triangle
FormattedText text = new FormattedText(
    powerlineIcon,
    CultureInfo.CurrentCulture,
    FlowDirection.LeftToRight,
    new Typeface(nerdFont, FontStyles.Normal, FontWeights.Normal, FontStretches.Normal),
    12,
    Brushes.White,
    VisualTreeHelper.GetDpi(this).PixelsPerDip
);
```

### Web (CSS)

```css
@font-face {
  font-family: 'JetBrainsMono Nerd Font';
  src: url('JetBrainsMonoNerdFont-Regular.ttf') format('truetype');
}

.icon::before {
  font-family: 'JetBrainsMono Nerd Font';
  content: '\e0b0';  /* Powerline separator */
}
```

---

## Testing PUA Glyph Support

### Test String for Verification

Use this test string to verify Nerd Font installation and PUA support:

```
            (Powerline)
            (Devicons)
            (Font Awesome)
            (Octicons)
            (Material Design)
```

**Expected Rendering:** All symbols should display as distinct icons, not as placeholder boxes.

---

## References

- **Unicode PUA Specification:** https://www.unicode.org/faq/private_use.html
- **Nerd Fonts Glyph Sets:** https://github.com/ryanoasis/nerd-fonts/wiki/Glyph-Sets-and-Code-Points
- **DirectWrite Font Fallback:** https://learn.microsoft.com/en-us/windows/win32/directwrite/custom-font-sets-win10

---

**Document Created:** 2025-10-11
**Last Updated:** 2025-10-11
**For:** Windows Terminal Optimization Project
