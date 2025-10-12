# Spline Sans Font Research

This directory contains comprehensive research and resources for integrating Spline Sans and Spline Sans Mono fonts into Windows Terminal.

## Directory Structure

```
spline-sans/
├── README.md                                    # This file
├── sans/                                        # Spline Sans (proportional)
│   ├── ttf/                                    # TrueType fonts (5 weights)
│   ├── otf/                                    # OpenType fonts (5 weights)
│   └── variable/                               # Variable font
├── mono/                                        # Spline Sans Mono (monospaced)
│   ├── ttf/                                    # TrueType fonts (10 variants)
│   ├── otf/                                    # OpenType fonts (10 variants)
│   └── variable/                               # Variable fonts (2 files)
├── licenses/                                    # Font licenses
│   ├── SplineSans-OFL.txt                      # SIL OFL 1.1 license
│   └── SplineSansMono-OFL.txt                  # SIL OFL 1.1 license
└── docs/                                        # Documentation
    ├── SPLINE_SANS_TERMINAL_INTEGRATION.md     # Complete integration guide
    ├── directwrite_integration_example.cpp     # DirectWrite code examples
    ├── SplineSans-README.md                    # Original README
    ├── SplineSansMono-README.md                # Original README
    ├── SplineSans-Regular-analysis.txt         # Font metadata
    └── SplineSansMono-Regular-analysis.txt     # Font metadata
```

## Quick Start

### 1. Font Installation (Windows)

**User Installation:**
```powershell
# Navigate to font directory
cd mono/ttf/

# Install desired weights (double-click .ttf files)
# Or use PowerShell:
$fonts = Get-ChildItem -Filter "*.ttf"
foreach ($font in $fonts) {
    Start-Process $font.FullName
    # Click "Install" in the font viewer
}
```

**System-Wide Installation:**
```powershell
# Run as Administrator
Copy-Item mono/ttf/*.ttf C:\Windows\Fonts\
```

### 2. Windows Terminal Configuration

Add to `settings.json`:

```json
{
    "profiles": {
        "defaults": {
            "font": {
                "face": "Spline Sans Mono",
                "size": 11
            }
        }
    }
}
```

## Font Information

### Spline Sans Mono (Recommended for Terminal)

- **Type:** Monospaced grotesque sans serif
- **Weights:** Light, Regular, Medium, SemiBold, Bold
- **Styles:** Upright + Italic for each weight (10 total fonts)
- **License:** SIL Open Font License 1.1
- **Character Set:** Latin, Latin Extended, Western European
- **Features:** True monospace, font hinting, scalable
- **File Size:** ~74KB per font (very compact)
- **Year:** 2022

**Suitability for Terminal:**
- TRUE MONOSPACE: Verified fixed-width spacing
- Multiple weights for syntax highlighting
- Italic variants for emphasis
- Modern, clean design
- Efficient screen rendering

**Limitations:**
- NO PowerLine glyphs (requires patching)
- NO programming ligatures
- Limited to Latin scripts

### Spline Sans (Proportional - Not for Terminal)

- **Type:** Proportional grotesque sans serif
- **Weights:** Light, Regular, Medium, SemiBold, Bold
- **Use Case:** UI elements, text content (NOT terminal text)

## Documentation

### Primary Documentation

**[SPLINE_SANS_TERMINAL_INTEGRATION.md](docs/SPLINE_SANS_TERMINAL_INTEGRATION.md)**

Complete technical guide covering:
1. Font family overview and specifications
2. Character set coverage analysis
3. Terminal suitability evaluation
4. Windows Terminal integration methods
5. DirectWrite API integration
6. Build system integration
7. PowerLine/Nerd Fonts patching
8. Testing and validation
9. License compliance
10. Implementation roadmap
11. Comparison with other terminal fonts
12. Configuration examples

### Code Examples

**[directwrite_integration_example.cpp](docs/directwrite_integration_example.cpp)**

Comprehensive C++ examples demonstrating:
- Font enumeration via DirectWrite
- Font face creation (system, file, resource)
- Rendering configuration
- Monospace validation
- Glyph metrics analysis
- Optimal rendering parameters

## Font Files

### Spline Sans Mono (Terminal Use)

**Static Fonts - TTF (10 files, 784KB total):**
```
SplineSansMono-Light.ttf         (72KB)  Weight: 300
SplineSansMono-LightItalic.ttf   (80KB)  Weight: 300, Italic
SplineSansMono-Regular.ttf       (74KB)  Weight: 400
SplineSansMono-Italic.ttf        (79KB)  Weight: 400, Italic
SplineSansMono-Medium.ttf        (75KB)  Weight: 500
SplineSansMono-MediumItalic.ttf  (80KB)  Weight: 500, Italic
SplineSansMono-SemiBold.ttf      (76KB)  Weight: 600
SplineSansMono-SemiBoldItalic.ttf(83KB)  Weight: 600, Italic
SplineSansMono-Bold.ttf          (76KB)  Weight: 700
SplineSansMono-BoldItalic.ttf    (81KB)  Weight: 700, Italic
```

**Variable Fonts (2 files):**
```
SplineSansMono[wght].ttf         Variable weight 300-700 (upright)
SplineSansMono-Italic[wght].ttf  Variable weight 300-700 (italic)
```

**Static Fonts - OTF (10 files):**
Same variants as TTF, OpenType format

### Spline Sans (UI Use Only)

**Static Fonts - TTF (5 files, 384KB total):**
```
SplineSans-Light.ttf      (74KB)  Weight: 300
SplineSans-Regular.ttf    (73KB)  Weight: 400
SplineSans-Medium.ttf     (75KB)  Weight: 500
SplineSans-SemiBold.ttf   (77KB)  Weight: 600
SplineSans-Bold.ttf       (76KB)  Weight: 700
```

## License

Both Spline Sans and Spline Sans Mono are licensed under the **SIL Open Font License v1.1**.

**Key Points:**
- Free for commercial and personal use
- Can be bundled with applications
- Can be modified and redistributed
- Must include license and copyright notice
- Cannot sell fonts standalone
- Modified versions cannot use reserved font names

**License Files:**
- `licenses/SplineSans-OFL.txt`
- `licenses/SplineSansMono-OFL.txt`

**Attribution:**
```
Copyright 2022 The Spline Sans Mono Project Authors
(https://github.com/SorkinType/SplineSansMono)

This Font Software is licensed under the SIL Open Font License, Version 1.1.
```

## Official Sources

### Spline Sans Mono
- **Google Fonts:** https://fonts.google.com/specimen/Spline+Sans+Mono
- **GitHub:** https://github.com/SorkinType/SplineSansMono
- **Designers:** Eben Sorkin, Mirko Velimirovic
- **Maintainer:** SorkinType

### Spline Sans
- **Google Fonts:** https://fonts.google.com/specimen/Spline+Sans
- **GitHub:** https://github.com/SorkinType/SplineSans
- **Designers:** Eben Sorkin, Mirko Velimirovic
- **Maintainer:** SorkinType

## Testing

### Character Coverage Test (PowerShell)

```powershell
# Test Spline Sans Mono rendering
Write-Host "ASCII: abcdefghijklmnopqrstuvwxyz ABCDEFGHIJKLMNOPQRSTUVWXYZ 0123456789"
Write-Host "Symbols: !@#$%^&*()_+-={}[]|\\:;`"'<>,.?/~"
Write-Host "Extended: àáâãäå èéêë ìíîï òóôõö ùúûü"
Write-Host "Box (fallback): ┌─┬─┐ ├─┼─┤ └─┴─┘"
```

### Font Validation (Linux/WSL)

```bash
# Character set analysis
fc-query mono/ttf/SplineSansMono-Regular.ttf

# List supported languages
fc-query mono/ttf/SplineSansMono-Regular.ttf | grep lang

# Check monospace property
fc-query mono/ttf/SplineSansMono-Regular.ttf | grep spacing
# Expected: spacing: 100(i)(s)  <- confirms monospace
```

## Integration Recommendations

### For Windows Terminal

1. **Font Choice:** Spline Sans Mono (monospaced variant)
2. **Recommended Weight:** Regular (400) or Medium (500)
3. **Recommended Size:** 11pt (optimal readability)
4. **ClearType:** Enabled (Windows default)
5. **Rendering Mode:** ClearType Natural Symmetric

### For PowerLine/Oh-My-Posh Users

Spline Sans Mono does NOT include PowerLine glyphs. Options:

1. **Patch with Nerd Fonts:**
   ```bash
   git clone https://github.com/ryanoasis/nerd-fonts
   cd nerd-fonts
   ./font-patcher ../SplineSansMono-Regular.ttf --complete
   ```

2. **Use Font Fallback:**
   ```json
   {
       "font": {
           "face": "Spline Sans Mono, Cascadia Code PL"
       }
   }
   ```

3. **Switch to PowerLine-enabled font:**
   - Cascadia Code PL
   - JetBrains Mono Nerd Font
   - Fira Code Nerd Font

## Comparison with Other Fonts

| Feature           | Spline Sans Mono | Cascadia Code | JetBrains Mono | Fira Code |
|-------------------|------------------|---------------|----------------|-----------|
| Monospaced        | YES              | YES           | YES            | YES       |
| PowerLine         | NO               | PL variant    | NO             | NO        |
| Ligatures         | NO               | YES           | YES            | YES       |
| Weights           | 5                | 7             | 8              | 4         |
| Italic            | YES (all)        | YES           | YES            | YES       |
| File Size (Reg)   | 74KB             | 250KB         | 165KB          | 190KB     |
| Design            | Minimalist       | Modern        | Rounded        | Code-focus|
| Year              | 2022             | 2019+         | 2020+          | 2014+     |

**Choose Spline Sans Mono if you want:**
- Cleanest, most minimal terminal aesthetic
- Smallest font file sizes
- Modern design (2025-ready)
- No ligature requirements
- Pure monospace without distractions

## Next Steps

1. **Read Full Documentation:**
   - [docs/SPLINE_SANS_TERMINAL_INTEGRATION.md](docs/SPLINE_SANS_TERMINAL_INTEGRATION.md)

2. **Review Code Examples:**
   - [docs/directwrite_integration_example.cpp](docs/directwrite_integration_example.cpp)

3. **Install and Test:**
   - Install fonts from `mono/ttf/`
   - Configure Windows Terminal
   - Test rendering quality

4. **Integration:**
   - Add fonts to build system
   - Implement DirectWrite loading
   - Create font selection UI
   - Test across Windows versions

## Contact

### Font Issues
- Report to SorkinType: https://github.com/SorkinType/SplineSansMono/issues

### Integration Issues
- File in Windows Terminal optimization project

---

**Research Date:** 2025-10-11
**Status:** Complete
**Fonts Downloaded:** Spline Sans (5 weights), Spline Sans Mono (10 variants)
**Documentation:** Complete integration guide with code examples
