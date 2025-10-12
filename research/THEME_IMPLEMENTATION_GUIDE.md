# Windows Terminal Theme Implementation Guide

## Overview
This guide provides detailed instructions for integrating the popular terminal themes
into the Windows Terminal codebase.

## Implementation Approach

### 1. Direct Integration into defaults.json
The simplest and most maintainable approach is to add the new themes directly to
the existing `defaults.json` file in the schemes array.

### 2. Theme Manager Class (Future Enhancement)
For dynamic theme management, a ThemeManager class can be implemented to handle
theme loading, validation, and switching.

## Step-by-Step Implementation

### Step 1: Add Themes to defaults.json

**File**: `/src/cascadia/TerminalSettingsModel/defaults.json`

Add the following color schemes to the `"schemes"` array:

```json
{
  "name": "Catppuccin Latte",
  "cursorColor": "#DC8A78",
  "selectionBackground": "#ACB0BE",
  "background": "#EFF1F5",
  "foreground": "#4C4F69",
  "black": "#5C5F77",
  "red": "#D20F39",
  "green": "#40A02B",
  "yellow": "#DF8E1D",
  "blue": "#1E66F5",
  "purple": "#EA76CB",
  "cyan": "#179299",
  "white": "#ACB0BE",
  "brightBlack": "#ACB0BE",
  "brightRed": "#D20F39",
  "brightGreen": "#40A02B",
  "brightYellow": "#DF8E1D",
  "brightBlue": "#1E66F5",
  "brightPurple": "#EA76CB",
  "brightCyan": "#179299",
  "brightWhite": "#BCC0CC"
},
{
  "name": "Catppuccin Frappe",
  "cursorColor": "#F2D5CF",
  "selectionBackground": "#626880",
  "background": "#303446",
  "foreground": "#C6D0F5",
  "black": "#51576D",
  "red": "#E78284",
  "green": "#A6D189",
  "yellow": "#E5C890",
  "blue": "#8CAAEE",
  "purple": "#F4B8E4",
  "cyan": "#81C8BE",
  "white": "#B5BFE2",
  "brightBlack": "#626880",
  "brightRed": "#E78284",
  "brightGreen": "#A6D189",
  "brightYellow": "#E5C890",
  "brightBlue": "#8CAAEE",
  "brightPurple": "#F4B8E4",
  "brightCyan": "#81C8BE",
  "brightWhite": "#A5ADCE"
},
{
  "name": "Catppuccin Macchiato",
  "cursorColor": "#F4DBD6",
  "selectionBackground": "#5B6078",
  "background": "#24273A",
  "foreground": "#CAD3F5",
  "black": "#494D64",
  "red": "#ED8796",
  "green": "#A6DA95",
  "yellow": "#EED49F",
  "blue": "#8AADF4",
  "purple": "#F5BDE6",
  "cyan": "#8BD5CA",
  "white": "#B8C0E0",
  "brightBlack": "#5B6078",
  "brightRed": "#ED8796",
  "brightGreen": "#A6DA95",
  "brightYellow": "#EED49F",
  "brightBlue": "#8AADF4",
  "brightPurple": "#F5BDE6",
  "brightCyan": "#8BD5CA",
  "brightWhite": "#A5ADCB"
},
{
  "name": "Catppuccin Mocha",
  "cursorColor": "#F5E0DC",
  "selectionBackground": "#585B70",
  "background": "#1E1E2E",
  "foreground": "#CDD6F4",
  "black": "#45475A",
  "red": "#F38BA8",
  "green": "#A6E3A1",
  "yellow": "#F9E2AF",
  "blue": "#89B4FA",
  "purple": "#F5C2E7",
  "cyan": "#94E2D5",
  "white": "#BAC2DE",
  "brightBlack": "#585B70",
  "brightRed": "#F38BA8",
  "brightGreen": "#A6E3A1",
  "brightYellow": "#F9E2AF",
  "brightBlue": "#89B4FA",
  "brightPurple": "#F5C2E7",
  "brightCyan": "#94E2D5",
  "brightWhite": "#A6ADC8"
},
{
  "name": "Dracula",
  "cursorColor": "#F8F8F2",
  "selectionBackground": "#44475A",
  "background": "#282A36",
  "foreground": "#F8F8F2",
  "black": "#21222C",
  "blue": "#BD93F9",
  "cyan": "#8BE9FD",
  "green": "#50FA7B",
  "purple": "#FF79C6",
  "red": "#FF5555",
  "white": "#F8F8F2",
  "yellow": "#F1FA8C",
  "brightBlack": "#6272A4",
  "brightBlue": "#D6ACFF",
  "brightCyan": "#A4FFFF",
  "brightGreen": "#69FF94",
  "brightPurple": "#FF92DF",
  "brightRed": "#FF6E6E",
  "brightWhite": "#FFFFFF",
  "brightYellow": "#FFFFA5"
},
{
  "name": "Nord",
  "foreground": "#D8DEE9",
  "background": "#2E3440",
  "selectionBackground": "#434C5E",
  "cursorColor": "#81A1C1",
  "black": "#3B4252",
  "red": "#BF616A",
  "green": "#A3BE8C",
  "yellow": "#EBCB8B",
  "blue": "#81A1C1",
  "purple": "#B48EAD",
  "cyan": "#88C0D0",
  "white": "#E5E9F0",
  "brightBlack": "#4C566A",
  "brightRed": "#BF616A",
  "brightGreen": "#A3BE8C",
  "brightYellow": "#EBCB8B",
  "brightBlue": "#81A1C1",
  "brightPurple": "#B48EAD",
  "brightCyan": "#8FBCBB",
  "brightWhite": "#ECEFF4"
},
{
  "name": "Gruvbox Dark",
  "background": "#282828",
  "foreground": "#EBDBB2",
  "cursorColor": "#EBDBB2",
  "selectionBackground": "#665C54",
  "black": "#282828",
  "red": "#CC241D",
  "green": "#98971A",
  "yellow": "#D79921",
  "blue": "#458588",
  "purple": "#B16286",
  "cyan": "#689D6A",
  "white": "#A89984",
  "brightBlack": "#928374",
  "brightRed": "#FB4934",
  "brightGreen": "#B8BB26",
  "brightYellow": "#FABD2F",
  "brightBlue": "#83A598",
  "brightPurple": "#D3869B",
  "brightCyan": "#8EC07C",
  "brightWhite": "#EBDBB2"
},
{
  "name": "Tokyo Night",
  "foreground": "#a9b1dc",
  "background": "#1a1b2c",
  "cursorColor": "#c0caf5",
  "selectionBackground": "#28344a",
  "black": "#414868",
  "red": "#f7768e",
  "green": "#73daca",
  "yellow": "#e0af68",
  "blue": "#7aa2f7",
  "purple": "#bb9af7",
  "cyan": "#7dcfff",
  "white": "#c0caf5",
  "brightBlack": "#414868",
  "brightRed": "#f7768e",
  "brightGreen": "#73daca",
  "brightYellow": "#e0af68",
  "brightBlue": "#7aa2f7",
  "brightPurple": "#bb9af7",
  "brightCyan": "#7dcfff",
  "brightWhite": "#c0caf5"
},
{
  "name": "One Dark Pro",
  "foreground": "#abb2bf",
  "background": "#282c34",
  "cursorColor": "#abb2bf",
  "selectionBackground": "#3E4451",
  "black": "#3f4451",
  "red": "#e05561",
  "green": "#8cc265",
  "yellow": "#d18f52",
  "blue": "#4aa5f0",
  "purple": "#c162de",
  "cyan": "#42b3c2",
  "white": "#e6e6e6",
  "brightBlack": "#4f5666",
  "brightRed": "#ff616e",
  "brightGreen": "#a5e075",
  "brightYellow": "#f0a45d",
  "brightBlue": "#4dc4ff",
  "brightPurple": "#de73ff",
  "brightCyan": "#4cd1e0",
  "brightWhite": "#d7dae0"
}
```

### Step 2: Update NOTICE.md

**File**: `/NOTICE.md`

Add attribution for the new themes:

```markdown
## Third-Party Color Schemes

### Catppuccin
- License: MIT License
- Copyright (c) 2021 Catppuccin
- Source: https://github.com/catppuccin/catppuccin
- Windows Terminal Port: https://github.com/catppuccin/windows-terminal

### Dracula
- License: MIT License
- Copyright (c) Dracula Theme
- Source: https://github.com/dracula/dracula-theme
- Website: https://draculatheme.com

### Nord
- License: MIT License
- Copyright (c) 2016-present Arctic Ice Studio
- Source: https://github.com/nordtheme/nord
- Website: https://www.nordtheme.com

### Gruvbox
- License: MIT License
- Copyright (c) 2012 Pavel Pertsev
- Source: https://github.com/morhetz/gruvbox

### Tokyo Night
- License: Apache License 2.0
- Copyright (c) 2021 Folke Lemaitre
- Source: https://github.com/folke/tokyonight.nvim

### One Dark Pro
- License: MIT License
- Based on Atom's One Dark theme
- Copyright (c) 2014 GitHub Inc.
```

### Step 3: Build System Integration

**File**: Verify that defaults.json is included in the build

Check `/src/cascadia/TerminalSettingsModel/CMakeLists.txt` to ensure defaults.json
is properly included in the build process.

No changes should be needed as the build system already processes defaults.json.

### Step 4: Testing

Create unit tests to validate the new color schemes:

**File**: `/src/cascadia/UnitTests_SettingsModel/ColorSchemeTests.cpp`

Add test cases:

```cpp
void ColorSchemeTests::ValidateCatppuccinLatte()
{
    const auto settings = CascadiaSettings::LoadDefaults();
    const auto schemes = settings.GlobalSettings().ColorSchemes();

    VERIFY_IS_TRUE(schemes.HasKey(L"Catppuccin Latte"));
    auto scheme = schemes.Lookup(L"Catppuccin Latte");

    VERIFY_ARE_EQUAL(til::color(0xEF, 0xF1, 0xF5), scheme.Background());
    VERIFY_ARE_EQUAL(til::color(0x4C, 0x4F, 0x69), scheme.Foreground());
    VERIFY_ARE_EQUAL(til::color(0xDC, 0x8A, 0x78), scheme.CursorColor());
}

void ColorSchemeTests::ValidateDracula()
{
    const auto settings = CascadiaSettings::LoadDefaults();
    const auto schemes = settings.GlobalSettings().ColorSchemes();

    VERIFY_IS_TRUE(schemes.HasKey(L"Dracula"));
    auto scheme = schemes.Lookup(L"Dracula");

    VERIFY_ARE_EQUAL(til::color(0x28, 0x2A, 0x36), scheme.Background());
    VERIFY_ARE_EQUAL(til::color(0xF8, 0xF8, 0xF2), scheme.Foreground());
}

// Add similar tests for Nord, Gruvbox Dark, Tokyo Night, One Dark Pro
```

### Step 5: Theme Switching UI Enhancement (Future)

For implementing automatic light/dark theme switching as per spec #4066:

**Files to Modify**:
1. `/src/cascadia/TerminalSettingsModel/Profile.idl`
2. `/src/cascadia/TerminalSettingsModel/Profile.h`
3. `/src/cascadia/TerminalSettingsModel/Profile.cpp`
4. `/src/cascadia/TerminalApp/TerminalPage.cpp`

**Implementation Approach**:

```cpp
// Profile.idl - Add new properties
runtimeclass Profile
{
    // Existing properties...
    String ColorSchemeLight;
    String ColorSchemeDark;
}

// Profile.cpp - Implement theme-aware color scheme selection
winrt::hstring Profile::GetActiveColorScheme()
{
    const auto theme = _settings.GlobalSettings().Theme();

    if (!_ColorSchemeLight.empty() && !_ColorSchemeDark.empty())
    {
        if (theme == L"light")
        {
            return _ColorSchemeLight;
        }
        else if (theme == L"dark")
        {
            return _ColorSchemeDark;
        }
    }

    // Fall back to regular ColorScheme
    return _ColorScheme;
}
```

## Theme Manager Class Implementation

For advanced theme management, create a ThemeManager class:

**File**: `/src/cascadia/TerminalSettingsModel/ThemeManager.h`

```cpp
#pragma once

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    class ThemeManager
    {
    public:
        static std::vector<ColorScheme> LoadThemesFromDirectory(
            const std::filesystem::path& themePath);

        static bool ValidateTheme(const ColorScheme& scheme);

        static void ExportTheme(
            const ColorScheme& scheme,
            const std::filesystem::path& outputPath);

        static ColorScheme ImportTheme(
            const std::filesystem::path& themePath);

        static std::vector<ColorScheme> GetAllDefaultThemes();

        static ColorScheme CreateThemeVariant(
            const ColorScheme& baseTheme,
            const std::wstring& variantName);
    };
}
```

**File**: `/src/cascadia/TerminalSettingsModel/ThemeManager.cpp`

```cpp
#include "pch.h"
#include "ThemeManager.h"
#include "ColorScheme.h"
#include <fstream>

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    std::vector<ColorScheme> ThemeManager::LoadThemesFromDirectory(
        const std::filesystem::path& themePath)
    {
        std::vector<ColorScheme> themes;

        if (!std::filesystem::exists(themePath))
        {
            return themes;
        }

        for (const auto& entry : std::filesystem::directory_iterator(themePath))
        {
            if (entry.path().extension() == ".json")
            {
                try
                {
                    auto theme = ImportTheme(entry.path());
                    if (ValidateTheme(theme))
                    {
                        themes.push_back(theme);
                    }
                }
                catch (...)
                {
                    // Log error and continue
                }
            }
        }

        return themes;
    }

    bool ThemeManager::ValidateTheme(const ColorScheme& scheme)
    {
        // Validate theme has required fields
        if (scheme.Name().empty())
        {
            return false;
        }

        // Validate color table has 16 entries
        auto table = scheme.Table();
        if (table.size() != 16)
        {
            return false;
        }

        // Additional validation as needed
        return true;
    }

    void ThemeManager::ExportTheme(
        const ColorScheme& scheme,
        const std::filesystem::path& outputPath)
    {
        auto json = scheme.ToJson();

        std::ofstream file(outputPath);
        if (file.is_open())
        {
            Json::StreamWriterBuilder builder;
            builder["indentation"] = "  ";
            std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
            writer->write(json, &file);
            file.close();
        }
    }

    ColorScheme ThemeManager::ImportTheme(
        const std::filesystem::path& themePath)
    {
        std::ifstream file(themePath);
        if (!file.is_open())
        {
            throw std::runtime_error("Failed to open theme file");
        }

        Json::Value json;
        Json::CharReaderBuilder builder;
        std::string errors;

        if (!Json::parseFromStream(builder, file, &json, &errors))
        {
            throw std::runtime_error("Failed to parse theme JSON");
        }

        auto scheme = ColorScheme::FromJson(json);
        if (!scheme)
        {
            throw std::runtime_error("Invalid theme format");
        }

        return *scheme;
    }
}
```

## Usage Examples

### Setting a Profile to Use a New Theme

In `settings.json` or through the Settings UI:

```json
{
  "profiles": {
    "defaults": {
      "colorScheme": "Catppuccin Mocha"
    },
    "list": [
      {
        "guid": "{...}",
        "name": "PowerShell",
        "colorScheme": "Dracula"
      }
    ]
  }
}
```

### Using Theme-Aware Switching (Future)

```json
{
  "profiles": {
    "list": [
      {
        "guid": "{...}",
        "name": "PowerShell",
        "colorSchemeLight": "Catppuccin Latte",
        "colorSchemeDark": "Catppuccin Mocha"
      }
    ]
  }
}
```

## Build Commands

After making changes, rebuild the project:

```bash
# From the root directory
cmake --build build --config Release

# Or using MSBuild
msbuild OpenConsole.sln /p:Configuration=Release
```

## Validation Checklist

- [ ] All theme JSON files are valid JSON
- [ ] All themes have required fields (name, 16 colors, foreground, background)
- [ ] Color values are valid hex codes
- [ ] Theme names are unique
- [ ] Themes are added to defaults.json
- [ ] Attribution added to NOTICE.md
- [ ] Unit tests pass
- [ ] Visual validation completed
- [ ] Build succeeds without warnings
- [ ] Themes appear in Settings UI

## Troubleshooting

### Theme Not Appearing in Settings
- Check that defaults.json has valid JSON syntax
- Verify theme name is unique
- Rebuild the project completely

### Colors Look Wrong
- Verify hex color values match official sources
- Check color order (black, red, green, yellow, blue, purple, cyan, white)
- Ensure brightColors are properly set

### Build Errors
- Run `cmake --build build --clean-first`
- Check for JSON syntax errors in defaults.json
- Verify all required fields are present

## Additional Resources

- [Windows Terminal Settings Schema](https://aka.ms/terminal-profiles-schema)
- [Color Scheme Documentation](https://learn.microsoft.com/en-us/windows/terminal/customize-settings/color-schemes)
- [Contributing Guide](CONTRIBUTING.md)
