# Linux Terminal Themes - Quick Start Guide

## Overview

Two Linux distribution themes are now available for Windows Terminal:

1. **Linux Humanity (Ubuntu Tango)** - Ubuntu's classic purple terminal
2. **CachyOS Nord** - Modern Arctic-themed terminal from CachyOS

## Quick Installation

### Step 1: Copy Theme JSON

Choose one or both themes:

**Linux Humanity**
```json
{
  "name": "Linux Humanity (Ubuntu Tango)",
  "background": "#300A24",
  "foreground": "#FFFFFF",
  "cursorColor": "#FFFFFF",
  "selectionBackground": "#B5D5FF",
  "black": "#2E3436",
  "red": "#CC0000",
  "green": "#4E9A06",
  "yellow": "#C4A000",
  "blue": "#3465A4",
  "purple": "#75507B",
  "cyan": "#06989A",
  "white": "#D3D7CF",
  "brightBlack": "#555753",
  "brightRed": "#EF2929",
  "brightGreen": "#8AE234",
  "brightYellow": "#FCE94F",
  "brightBlue": "#729FCF",
  "brightPurple": "#AD7FA8",
  "brightCyan": "#34E2E2",
  "brightWhite": "#EEEEEC"
}
```

**CachyOS Nord**
```json
{
  "name": "CachyOS Nord",
  "background": "#2E3440",
  "foreground": "#D8DEE9",
  "cursorColor": "#D8DEE9",
  "selectionBackground": "#4C566A",
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
}
```

### Step 2: Add to Windows Terminal

1. Open Windows Terminal
2. Press `Ctrl+Shift+,` (or Settings > Open JSON file)
3. Find the `"schemes": []` array
4. Paste the theme JSON inside the array
5. Save the file

### Step 3: Apply Theme

1. Go to Settings > Profiles > [Your Profile] > Appearance
2. Under "Color scheme", select your theme
3. Click Save

## Visual Comparison

### Linux Humanity (Ubuntu Tango)
```
Background: #300A24 (Deep aubergine purple)
Foreground: #FFFFFF (Pure white)

Characteristics:
- High contrast (white text on purple)
- Warm color palette
- Ubuntu's signature look
- Excellent readability
- Traditional Linux feel
```

### CachyOS Nord
```
Background: #2E3440 (Dark gray-blue)
Foreground: #D8DEE9 (Light gray)

Characteristics:
- Medium-high contrast
- Cool color palette
- Modern minimalist
- Reduced eye strain
- Scandinavian aesthetic
```

## Color Palette At-A-Glance

| Color Type | Linux Humanity | CachyOS Nord |
|------------|----------------|--------------|
| **Background** | #300A24 Purple | #2E3440 Gray-blue |
| **Foreground** | #FFFFFF White | #D8DEE9 Light gray |
| **Red** | #CC0000 Vibrant | #BF616A Muted |
| **Green** | #4E9A06 Vivid | #A3BE8C Soft |
| **Yellow** | #C4A000 Brown-yellow | #EBCB8B Warm |
| **Blue** | #3465A4 Sky blue | #81A1C1 Light blue |
| **Purple** | #75507B Plum | #B48EAD Lavender |
| **Cyan** | #06989A Teal | #88C0D0 Bright cyan |

## Recommended Fonts

### For Linux Humanity
- **Ubuntu Mono** (most authentic)
- DejaVu Sans Mono
- Liberation Mono
- Cascadia Code

### For CachyOS Nord
- **JetBrains Mono** (modern)
- Fira Code (with ligatures)
- Cascadia Code
- IBM Plex Mono

## Use Cases

### Choose Linux Humanity If:
- You use Ubuntu or Debian
- You prefer high contrast
- You like warm color schemes
- You want traditional Linux aesthetics
- You need maximum readability

### Choose CachyOS Nord If:
- You use CachyOS or Arch Linux
- You prefer cool, pastel colors
- You like modern minimalism
- You code for extended periods
- You want reduced eye strain

## Testing Your Theme

After installation, test the theme with this command:

**Bash/PowerShell**
```bash
echo -e "\e[31mRed \e[32mGreen \e[33mYellow \e[34mBlue \e[35mPurple \e[36mCyan \e[0m"
echo -e "\e[91mBright Red \e[92mBright Green \e[93mBright Yellow \e[0m"
echo -e "\e[94mBright Blue \e[95mBright Purple \e[96mBright Cyan \e[0m"
```

**PowerShell Alternative**
```powershell
Write-Host "Red " -ForegroundColor Red -NoNewline
Write-Host "Green " -ForegroundColor Green -NoNewline
Write-Host "Yellow " -ForegroundColor Yellow -NoNewline
Write-Host "Blue " -ForegroundColor Blue -NoNewline
Write-Host "Magenta " -ForegroundColor Magenta -NoNewline
Write-Host "Cyan" -ForegroundColor Cyan
```

## File Locations

Theme files are located at:
```
research/themes/linux-humanity.json
research/themes/cachyos-nord.json
```

Detailed documentation:
```
research/themes/LINUX_THEMES_RESEARCH.md
research/themes/README.md
```

## Troubleshooting

**Theme doesn't appear in list**
- Check JSON syntax (no trailing commas)
- Ensure theme is inside `"schemes": []` array
- Restart Windows Terminal

**Colors look wrong**
- Verify hex codes match exactly
- Check display color profile
- Try different profile

**Want to modify colors**
- Edit hex values in JSON
- Use a color picker tool
- Test changes immediately

## Next Steps

1. **Explore other themes**: Check `research/themes/` for more options
2. **Read full documentation**: See `LINUX_THEMES_RESEARCH.md`
3. **Customize further**: Adjust colors to your preference
4. **Share feedback**: Test and report any issues

## Quick Links

- **Full Documentation**: `LINUX_THEMES_RESEARCH.md`
- **All Themes**: `research/themes/`
- **Theme Integration Guide**: `../THEME_IMPLEMENTATION_GUIDE.md`
- **Windows Terminal Docs**: https://learn.microsoft.com/en-us/windows/terminal/

## Licenses

- **Linux Humanity**: GNU GPLv3+ (c) Kenneth Wimer, Conn O'Griofa, Luigi Fab
- **CachyOS Nord**: MIT License (c) 2016 Arctic Ice Studio

See `LINUX_THEMES_RESEARCH.md` for full license details.

---

**Created**: 2025-10-11
**Status**: Production Ready
**Version**: 1.0
