# Popular Terminal Themes for Windows Terminal

This directory contains JSON color scheme files for popular terminal themes,
ready for integration into Windows Terminal.

## Themes Included

### Linux Humanity (Ubuntu Tango)
**Source**: https://github.com/luigifab/human-theme
**License**: GNU GPLv3+

File: `linux-humanity.json`

Ubuntu's classic terminal theme based on the Tango Desktop Project palette.
Background: #300A24 (Ubuntu's signature aubergine purple)

Features Ubuntu's distinctive purple background with high-contrast white text
and the warm, earthy Tango color palette. Used in GNOME Terminal since Ubuntu 12.04.

Best for: Ubuntu users, classic Linux aesthetics, high contrast preference

### CachyOS Nord
**Source**: https://www.nordtheme.com / https://github.com/CachyOS/CachyOS-Nord-KDE
**License**: MIT

File: `cachyos-nord.json`

Arctic-themed terminal colors based on the Nord palette, used by CachyOS Linux.
Background: #2E3440 (dark gray-blue)

Features cool, north-bluish pastel colors designed for eye comfort during
extended terminal sessions. Part of the popular Nord theme with 400+ app ports.

Best for: CachyOS/Arch users, modern minimalist aesthetics, cool color preference

### Catppuccin (4 variants)
**Source**: https://github.com/catppuccin/windows-terminal
**License**: MIT

A soothing pastel theme with 4 warm flavors:

1. **Latte** (`catppuccin-latte.json`) - Light theme
   - Background: #EFF1F5 (light gray-white)
   - Best for: Daytime use, bright environments

2. **Frappe** (`catppuccin-frappe.json`) - Medium dark
   - Background: #303446 (dark blue-gray)
   - Best for: All-day use, balanced contrast

3. **Macchiato** (`catppuccin-macchiato.json`) - Dark
   - Background: #24273A (deep blue-gray)
   - Best for: Evening use, reduced eye strain

4. **Mocha** (`catppuccin-mocha.json`) - Darkest
   - Background: #1E1E2E (near-black)
   - Best for: Night use, OLED displays

### Dracula
**Source**: https://draculatheme.com
**License**: MIT

File: `dracula.json`

A dark theme with vibrant purple, cyan, and pink accents.
Background: #282A36

One of the most popular themes with 400+ app ports.
Best for: General use, vibrant color preference

### Nord
**Source**: https://www.nordtheme.com
**License**: MIT

File: `nord.json`

An arctic, north-bluish color palette inspired by the Aurora Borealis.
Background: #2E3440

Best for: Cool color preference, Scandinavian aesthetic

### Gruvbox Dark
**Source**: https://github.com/morhetz/gruvbox
**License**: MIT

File: `gruvbox-dark.json`

Retro groove color scheme with warm, earthy tones.
Background: #282828

Best for: Long coding sessions, reduced eye strain, warm color preference

### Tokyo Night
**Source**: https://github.com/folke/tokyonight.nvim
**License**: Apache 2.0

File: `tokyo-night.json`

A clean, dark theme inspired by Tokyo at night.
Background: #1a1b2c

Best for: Modern aesthetic, balanced contrast

### One Dark Pro
**Source**: Based on Atom's One Dark
**License**: MIT

File: `one-dark.json`

Professional dark theme with balanced contrast.
Background: #282c34

Best for: General development, VSCode users

## Installation

### Method 1: Manual Installation (User Settings)

1. Open Windows Terminal
2. Press `Ctrl+,` to open Settings
3. Click "Open JSON file" at bottom-left
4. Copy the contents of any theme JSON file
5. Paste into the `"schemes"` array in your `settings.json`
6. Save the file
7. Go back to Settings UI and select the new theme for your profile

### Method 2: Integration into Defaults (Development)

For developers integrating into Windows Terminal source:

1. Copy theme JSON content
2. Add to `/src/cascadia/TerminalSettingsModel/defaults.json`
3. Add to the `"schemes"` array
4. Rebuild the project
5. Theme will be available to all users in default installation

See `../THEME_IMPLEMENTATION_GUIDE.md` for detailed integration instructions.

## Usage

After installation, set a theme for your profile:

```json
{
  "profiles": {
    "defaults": {
      "colorScheme": "Catppuccin Mocha"
    }
  }
}
```

Or set per-profile:

```json
{
  "profiles": {
    "list": [
      {
        "name": "PowerShell",
        "colorScheme": "Dracula"
      },
      {
        "name": "Ubuntu",
        "colorScheme": "Nord"
      }
    ]
  }
}
```

## Theme Comparison

| Theme | Type | Background | Best Use Case |
|-------|------|------------|---------------|
| Linux Humanity | Dark | #300A24 | Ubuntu users, high contrast |
| CachyOS Nord | Dark | #2E3440 | Arch/CachyOS users, eye comfort |
| Catppuccin Latte | Light | #EFF1F5 | Bright environments |
| Catppuccin Frappe | Dark | #303446 | All-day coding |
| Catppuccin Macchiato | Dark | #24273A | Evening work |
| Catppuccin Mocha | Dark | #1E1E2E | Night coding, OLED |
| Dracula | Dark | #282A36 | Vibrant colors |
| Nord | Dark | #2E3440 | Cool aesthetics |
| Gruvbox Dark | Dark | #282828 | Warm, retro feel |
| Tokyo Night | Dark | #1a1b2c | Modern, clean |
| One Dark Pro | Dark | #282c34 | Professional work |

## Color Format

All themes follow the Windows Terminal color scheme format:

- `name`: Unique theme identifier
- `foreground`: Default text color
- `background`: Default background color
- `cursorColor`: Text cursor color
- `selectionBackground`: Selected text background
- 16 ANSI colors: black, red, green, yellow, blue, purple, cyan, white
- 8 bright variants: brightBlack through brightWhite

## Validation

All themes have been validated against:
- Official source repositories
- JSON format correctness
- Required field presence
- Valid hex color codes
- Unique naming

## Accessibility

Themes have been selected to provide good readability and contrast.
For specific accessibility needs:

- **High Contrast**: Use Gruvbox Dark or Windows High Contrast themes
- **Color Blind Friendly**: Nord, Gruvbox have good differentiation
- **Light Theme**: Catppuccin Latte is the only light theme included

## Contributing

To add new themes:

1. Create JSON file with proper format
2. Validate against official sources
3. Test in Windows Terminal
4. Document theme characteristics
5. Update this README

## License Information

All themes are open source with permissive licenses:

- Linux Humanity: GNU GPLv3+ (c) Kenneth Wimer, Conn O'Griofa, Luigi Fab
- CachyOS Nord: MIT License (c) 2016 Arctic Ice Studio
- Catppuccin: MIT License (c) 2021 Catppuccin
- Dracula: MIT License (c) Dracula Theme
- Nord: MIT License (c) 2016 Arctic Ice Studio
- Gruvbox: MIT License (c) 2012 Pavel Pertsev
- Tokyo Night: Apache 2.0 (c) 2021 Folke Lemaitre
- One Dark Pro: MIT License (c) 2014 GitHub Inc.

See individual theme repositories for full license details.

**Note**: For detailed information about Linux Humanity and CachyOS Nord themes,
see `LINUX_THEMES_RESEARCH.md` in this directory.

## Support

For issues with:
- Theme colors: Contact original theme maintainers
- Windows Terminal integration: File issue on Windows Terminal GitHub
- This package: See `../THEME_INTEGRATION_RESEARCH.md`

## Additional Resources

- [Windows Terminal Documentation](https://learn.microsoft.com/en-us/windows/terminal/)
- [Color Schemes Guide](https://learn.microsoft.com/en-us/windows/terminal/customize-settings/color-schemes)
- [Theme Integration Guide](../THEME_IMPLEMENTATION_GUIDE.md)
- [Terminal Colors Database](https://terminalcolors.com/)
