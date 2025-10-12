# Bundled Fonts for Ultra-Riced Windows Terminal

This directory contains high-quality fonts bundled with the Ultra-Riced Windows Terminal build.

## Included Fonts

### 1. Spline Sans Mono
**Directory**: `spline-sans-mono/`
**Size**: 784 KB (10 variants)
**License**: SIL Open Font License 1.1
**Source**: [Google Fonts - Spline Sans Mono](https://fonts.google.com/specimen/Spline+Sans+Mono)

**Features**:
- Modern, compact monospaced design
- 50-70% smaller file size than competitors
- Excellent readability at small sizes
- Full weight range: Light, Regular, Medium, SemiBold, Bold
- Italic variants for all weights

**Best For**: Users who want a clean, modern terminal aesthetic with excellent space efficiency

### 2. CaskaydiaCove Nerd Font Mono
**Directory**: `cascadia-code-nerd-font/`
**Size**: 30 MB (12 variants)
**License**: MIT (Nerd Fonts) + SIL OFL 1.1 (Cascadia Code base)
**Source**: [Nerd Fonts - Cascadia Code](https://github.com/ryanoasis/nerd-fonts)

**Features**:
- Based on Microsoft's Cascadia Code
- **10,390+ icon glyphs** from popular icon sets:
  - Font Awesome (complete set)
  - Material Design Icons
  - Devicons (developer icons)
  - Octicons (GitHub icons)
  - Pomicons
  - PowerLine symbols (arrows, triangles)
  - Weather Icons
  - Seti UI
  - IEC Power Symbols
  - And more!

**PowerLine Support**: Full PowerLine glyph support for advanced shell prompts (oh-my-posh, Starship, etc.)

**Best For**: Power users who want icon glyphs and PowerLine symbols in their terminal

## Installation

### Automatic Installation (Recommended)

Run the PowerShell installation script from the project root:

```powershell
# From project root directory
.\install-fonts.ps1
```

**Requirements**: Administrator privileges

The script will:
1. Copy all font files to `C:\Windows\Fonts`
2. Register fonts in Windows registry
3. Verify successful installation

### Manual Installation

1. Navigate to the font directory:
   - Spline Sans Mono: `fonts/spline-sans-mono/`
   - CaskaydiaCove: `fonts/cascadia-code-nerd-font/`

2. Select all `.ttf` files

3. Right-click and choose "Install for all users" (requires admin) or "Install" (current user only)

4. Restart Windows Terminal

## Configuration

After installation, configure Windows Terminal to use these fonts:

### Method 1: GUI Settings

1. Open Windows Terminal
2. Press `Ctrl+,` to open Settings
3. Navigate to **Defaults** or a specific profile
4. In the "Appearance" section, set **Font Face** to:
   - `Spline Sans Mono` - for compact, modern look
   - `CaskaydiaCove Nerd Font Mono` - for PowerLine + icon support

### Method 2: Edit settings.json

Add to your profile or defaults:

```json
{
    "font": {
        "face": "CaskaydiaCove Nerd Font Mono",
        "size": 11
    }
}
```

## Font Comparison

| Feature | Spline Sans Mono | CaskaydiaCove Nerd Font Mono |
|---------|------------------|------------------------------|
| **Size** | 784 KB (10 fonts) | 30 MB (12 fonts) |
| **Icon Glyphs** | ✗ | ✓ (10,390+) |
| **PowerLine** | ✗ | ✓ |
| **Ligatures** | ✗ | ✓ (Cascadia Code) |
| **Compact Design** | ✓✓ | ✓ |
| **Modern Aesthetic** | ✓✓ | ✓ |
| **File Size** | Smaller | Larger |

## Using Nerd Font Icons

Once CaskaydiaCove Nerd Font Mono is installed, you can use icon glyphs in your terminal:

### Example: PowerLine Prompt

Most modern shell prompts (oh-my-posh, Starship, PowerLevel10k) will automatically use PowerLine glyphs when a Nerd Font is detected.

### Example: Manual Icon Usage

Icons are accessible via Unicode Private Use Area (U+E000 - U+F8FF):

```bash
# Example: Git branch icon
echo "\ue0a0"  # Git branch symbol

# Example: Folder icon
echo "\uf07b"  # Folder icon

# Example: Docker icon
echo "\uf308"  # Docker whale icon
```

### Icon Cheat Sheets

- [Nerd Fonts Cheat Sheet](https://www.nerdfonts.com/cheat-sheet)
- Browse and search all 10,390+ glyphs by name or code

## Troubleshooting

### Fonts not appearing in Windows Terminal

1. **Restart Windows Terminal** completely (close all instances)
2. **Log out and log back in** to Windows
3. **Verify installation**:
   - Open `C:\Windows\Fonts` in File Explorer
   - Search for "Spline" or "Caskaydia"
   - Fonts should appear in the list

### Icons showing as boxes/missing glyphs

1. Ensure you selected **CaskaydiaCove Nerd Font Mono** (not regular Cascadia Code)
2. Verify font face in settings: Must be exactly `CaskaydiaCove Nerd Font Mono`
3. Check that your shell prompt is configured for Nerd Fonts

### PowerLine symbols not working

1. Verify font installation (see above)
2. Check that your prompt theme is configured for PowerLine
3. Some themes require environment variable: `export TERM=xterm-256color`

## License Information

All fonts are open source and freely redistributable:

- **Spline Sans Mono**: SIL Open Font License 1.1
- **CaskaydiaCove Nerd Font Mono**: MIT License (Nerd Fonts) + SIL OFL 1.1 (Cascadia Code)

Complete license texts available in [NOTICE.md](../NOTICE.md)

## Additional Resources

### Spline Sans Mono
- [Google Fonts Page](https://fonts.google.com/specimen/Spline+Sans+Mono)
- [GitHub Repository](https://github.com/SorkinType/SplineSans)

### Nerd Fonts
- [Official Website](https://www.nerdfonts.com/)
- [GitHub Repository](https://github.com/ryanoasis/nerd-fonts)
- [Icon Cheat Sheet](https://www.nerdfonts.com/cheat-sheet)

### Cascadia Code (Base Font)
- [GitHub Repository](https://github.com/microsoft/cascadia-code)
- [Documentation](https://docs.microsoft.com/en-us/windows/terminal/cascadia-code)

---

**Project**: Ultra-Riced Windows Terminal
**Font Installation Script**: `install-fonts.ps1`
**See Also**: [Build Instructions](../BUILD_ULTRA_PERFORMANCE_TERMINAL.md)
