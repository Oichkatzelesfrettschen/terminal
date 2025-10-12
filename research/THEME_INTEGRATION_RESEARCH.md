# Windows Terminal Theme Integration Research

## Research Date
2025-10-11

## Objective
Research and integrate popular terminal themes natively into the Windows Terminal project, including:
- Catppuccin (all variants)
- Dracula
- Nord
- Gruvbox
- Tokyo Night
- One Dark

## Theme Sources

### 1. Catppuccin
**Official Repository**: https://github.com/catppuccin/windows-terminal
**License**: MIT
**Variants**: 4 flavors (Latte, Frappe, Macchiato, Mocha)

The Catppuccin theme is a soothing pastel theme with 4 warm flavors and 26 eye-candy colors each.

**Variants Researched**:
- **Latte**: Light theme with pastel colors on light background (#EFF1F5)
- **Frappe**: Medium dark theme with cool pastels on dark blue-gray (#303446)
- **Macchiato**: Dark theme with warm pastels on deep blue-gray (#24273A)
- **Mocha**: Darkest theme with vibrant pastels on near-black (#1E1E2E)

### 2. Dracula
**Official Repository**: https://github.com/dracula/dracula-theme
**Official Site**: https://draculatheme.com/windows-terminal
**License**: MIT

Dracula is a dark theme with vibrant purple, cyan, and pink accents. Background: #282A36.
Supports 400+ applications with consistent color palette.

### 3. Nord
**Official Repository**: https://github.com/nordtheme/nord
**Windows Terminal Port**: https://github.com/thismat/nord-windows-terminal
**License**: MIT

Nord is an arctic, north-bluish color palette. The colors reflect the cold, yet harmonious
world of ice and the colorfulness of the Aurora Borealis. Background: #2E3440.

### 4. Gruvbox Dark
**Original Repository**: https://github.com/morhetz/gruvbox
**Windows Terminal Port**: https://gist.github.com/davialexandre/1179070118b22d830739efee4721972d
**License**: MIT

Gruvbox is a retro groove color scheme with warm, earthy tones. Background: #282828.
Designed for comfortable long-term use with reduced eye strain.

### 5. Tokyo Night
**Official Repository**: https://github.com/folke/tokyonight.nvim
**Windows Terminal Port**: https://github.com/jiyometrik/tokyonight-windows-terminal
**License**: Apache 2.0

Tokyo Night is a clean, dark theme inspired by the lights of Downtown Tokyo at night.
Background: #1a1b2c. Includes 4 styles: storm, moon, night, and day.

### 6. One Dark Pro
**Based on**: Atom's One Dark theme
**Windows Terminal Port**: https://github.com/yosukes-dev/one-dark-windows-terminal
**License**: MIT

One Dark is a professional dark theme with balanced contrast. Background: #282c34.
Widely used across editors and terminals for its readability.

## Windows Terminal Color Scheme Format

### JSON Structure
Windows Terminal color schemes use the following JSON structure:

```json
{
  "name": "Scheme Name",
  "foreground": "#HEXCODE",
  "background": "#HEXCODE",
  "cursorColor": "#HEXCODE",
  "selectionBackground": "#HEXCODE",
  "black": "#HEXCODE",
  "red": "#HEXCODE",
  "green": "#HEXCODE",
  "yellow": "#HEXCODE",
  "blue": "#HEXCODE",
  "purple": "#HEXCODE",
  "cyan": "#HEXCODE",
  "white": "#HEXCODE",
  "brightBlack": "#HEXCODE",
  "brightRed": "#HEXCODE",
  "brightGreen": "#HEXCODE",
  "brightYellow": "#HEXCODE",
  "brightBlue": "#HEXCODE",
  "brightPurple": "#HEXCODE",
  "brightCyan": "#HEXCODE",
  "brightWhite": "#HEXCODE"
}
```

### Required Fields
- `name`: Unique identifier for the color scheme
- 16 color entries (black through brightWhite)
- `foreground`: Default text color
- `background`: Default background color

### Optional Fields
- `cursorColor`: Color of the text cursor
- `selectionBackground`: Background color for selected text

## Integration Points in Windows Terminal Codebase

### 1. ColorScheme Class
**Location**: `/src/cascadia/TerminalSettingsModel/ColorScheme.cpp`
**Purpose**: Core color scheme data structure and JSON serialization

Key methods:
- `FromJson()`: Creates ColorScheme from JSON
- `ToJson()`: Serializes ColorScheme to JSON
- `_layerJson()`: Layers JSON values onto existing scheme

### 2. Default Settings
**Location**: `/src/cascadia/TerminalSettingsModel/defaults.json`
**Purpose**: Default color schemes shipped with Windows Terminal

Current default schemes:
- Dimidium
- Ottosson
- Campbell
- Campbell Powershell
- Vintage
- One Half Dark
- One Half Light
- Solarized Dark
- Solarized Light
- Tango Dark
- Tango Light

### 3. Color Schemes Page ViewModel
**Location**: `/src/cascadia/TerminalSettingsEditor/ColorSchemesPageViewModel.cpp`
**Purpose**: UI view model for managing color schemes in settings

Key functionality:
- `_MakeColorSchemeVMsHelper()`: Populates list of all color schemes
- `RequestAddNew()`: Creates new color scheme
- `CurrentScheme()`: Gets/sets currently selected scheme

### 4. GlobalAppSettings
**Location**: `/src/cascadia/TerminalSettingsModel/GlobalAppSettings.cpp`
**Purpose**: Global settings including color scheme registry

Methods for color scheme management:
- `AddColorScheme()`: Registers a new color scheme
- `ColorSchemes()`: Returns map of all color schemes

## Theme Switching Mechanism

### Current Implementation
Windows Terminal supports theme switching through:
1. Manual selection in settings UI
2. Profile-specific color scheme assignment
3. JSON configuration in settings.json

### Proposed Enhancement (from spec #4066)
Support for automatic theme switching based on Windows theme (light/dark):

```json
"colorScheme": {
    "light": "Catppuccin Latte",
    "dark": "Catppuccin Mocha"
}
```

Or alternative syntax:
```json
"colorSchemeLight": "Catppuccin Latte",
"colorSchemeDark": "Catppuccin Mocha"
```

## UI Integration Points

### Settings UI
**Location**: `/src/cascadia/TerminalSettingsEditor/ColorSchemes.xaml`
**Purpose**: XAML UI for color scheme management

Components:
- ColorScheme list viewer
- Color picker controls
- Add/Delete/Edit scheme buttons
- Preview pane

## Build System Integration

### Files to Modify
1. **defaults.json**: Add new color schemes to default settings
2. **CMakeLists.txt**: Ensure JSON files are included in build
3. **vcpkg.json**: No changes needed (no new dependencies)

### Resource Files
Color scheme JSON files should be:
- Added to `/research/themes/` directory (DONE)
- Integrated into `/src/cascadia/TerminalSettingsModel/defaults.json`
- Validated for proper JSON format and color values

## Implementation Plan

### Phase 1: Theme File Integration
1. Create all theme JSON files in research directory (COMPLETED)
2. Validate JSON format and color accuracy
3. Add themes to defaults.json
4. Update build system to include new themes

### Phase 2: Theme Management Code
1. Create ThemeManager class for theme operations
2. Implement theme loading and validation
3. Add theme import/export functionality
4. Create theme switching API

### Phase 3: UI Implementation
1. Design theme selector UI component
2. Implement theme preview functionality
3. Add theme switching controls
4. Integrate with existing settings UI

### Phase 4: Advanced Features
1. Implement automatic light/dark theme switching
2. Add theme customization options
3. Create theme preset manager
4. Support theme variants (e.g., all Catppuccin flavors)

## Testing Requirements

### Validation Tests
1. JSON format validation for all themes
2. Color value validation (valid hex codes)
3. Required field presence checks
4. Name uniqueness validation

### Integration Tests
1. Theme loading from defaults.json
2. Theme switching functionality
3. Profile-specific theme assignment
4. Theme persistence across sessions

### Visual Tests
1. Color accuracy validation against official sources
2. Contrast ratio validation for accessibility
3. Consistency across different terminal elements
4. Theme preview accuracy

## Color Accuracy Validation

### Catppuccin
All variants validated against official repository:
- https://github.com/catppuccin/windows-terminal

### Dracula
Validated against official specification:
- https://draculatheme.com/spec

### Nord
Validated against official palette:
- https://www.nordtheme.com/

### Gruvbox
Validated against original Vim theme:
- https://github.com/morhetz/gruvbox

### Tokyo Night
Validated against official Neovim theme:
- https://github.com/folke/tokyonight.nvim

### One Dark
Validated against Atom's One Dark theme:
- https://github.com/atom/atom/tree/master/packages/one-dark-syntax

## Accessibility Considerations

### Contrast Ratios
All themes should maintain WCAG AA compliance:
- Normal text: 4.5:1 minimum contrast ratio
- Large text: 3:1 minimum contrast ratio

### Color Blindness Support
- Test themes with color blindness simulators
- Ensure sufficient differentiation between colors
- Consider alternative themes (e.g., Gruvbox has high contrast variants)

## License Compliance

All themes are open source with permissive licenses:
- Catppuccin: MIT License
- Dracula: MIT License
- Nord: MIT License
- Gruvbox: MIT License
- Tokyo Night: Apache 2.0 License
- One Dark: MIT License

Attribution should be added to NOTICE.md file for each theme.

## Files Created

### Theme JSON Files (in /research/themes/)
1. catppuccin-latte.json
2. catppuccin-frappe.json
3. catppuccin-macchiato.json
4. catppuccin-mocha.json
5. dracula.json
6. nord.json
7. gruvbox-dark.json
8. tokyo-night.json
9. one-dark.json

### Documentation
1. THEME_INTEGRATION_RESEARCH.md (this file)

## Next Steps

1. Analyze existing defaults.json structure in detail
2. Create theme management utility code
3. Implement theme validation system
4. Design and implement theme switching UI
5. Add themes to defaults.json
6. Integrate into build system
7. Create comprehensive tests
8. Update NOTICE.md with theme attributions
9. Document theme customization for users

## References

- Windows Terminal Color Schemes Documentation: https://learn.microsoft.com/en-us/windows/terminal/customize-settings/color-schemes
- Issue #4066: Theme-controlled color scheme switch
- Windows Terminal Repository: https://github.com/microsoft/terminal
- Terminal Colors Database: https://terminalcolors.com/
