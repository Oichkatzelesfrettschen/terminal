# Font Patcher Comprehensive Guide

**Research Date:** 2025-10-11
**Tool:** font-patcher (Nerd Fonts)
**Version:** Compatible with Nerd Fonts v3.3.0

---

## Overview

The **font-patcher** is a Python script using FontForge that adds Nerd Fonts icon glyphs to any font file. It enables you to patch custom fonts with 10,390+ icons from 13 icon sets.

---

## Requirements

### System Requirements

**Operating System:**
- Windows 10/11
- macOS 10.14+
- Linux (any modern distribution)

**Dependencies:**
1. **Python 3.7+**
2. **FontForge with Python bindings**
3. **Nerd Fonts source files** (glyphs and scripts)

---

## Installation

### Windows

#### 1. Install Python 3

```powershell
# Download from python.org
# https://www.python.org/downloads/

# Verify installation
python --version
# Output: Python 3.11.x or higher
```

#### 2. Install FontForge

**Option A: Chocolatey**
```powershell
choco install fontforge
```

**Option B: Scoop**
```powershell
scoop bucket add extras
scoop install fontforge
```

**Option C: Manual Download**
- Download from: https://fontforge.org/en-US/downloads/windows/
- Install to default location
- Add to PATH: `C:\Program Files (x86)\FontForgeBuilds\bin`

#### 3. Install Python Dependencies

```powershell
pip install fontforge
pip install argparse
```

#### 4. Download Font Patcher

```powershell
# Clone Nerd Fonts repository
git clone --depth 1 https://github.com/ryanoasis/nerd-fonts.git

# Or download specific files
$baseUrl = "https://raw.githubusercontent.com/ryanoasis/nerd-fonts/master"
Invoke-WebRequest "$baseUrl/font-patcher" -OutFile font-patcher
Invoke-WebRequest "$baseUrl/src/glyphs/*" -OutFile src/glyphs/
```

---

### macOS

#### 1. Install Python 3 (via Homebrew)

```bash
brew install python3
python3 --version
```

#### 2. Install FontForge

```bash
brew install fontforge
```

#### 3. Install Python Dependencies

```bash
pip3 install fontforge
```

#### 4. Download Font Patcher

```bash
git clone --depth 1 https://github.com/ryanoasis/nerd-fonts.git
cd nerd-fonts
```

---

### Linux

#### Debian/Ubuntu

```bash
# Install Python 3
sudo apt update
sudo apt install python3 python3-pip

# Install FontForge
sudo apt install fontforge python3-fontforge

# Install dependencies
pip3 install argparse

# Download Font Patcher
git clone --depth 1 https://github.com/ryanoasis/nerd-fonts.git
cd nerd-fonts
```

#### Arch Linux

```bash
# Install dependencies
sudo pacman -S python python-pip fontforge

# Install Python FontForge bindings
pip install fontforge

# Download Font Patcher
git clone --depth 1 https://github.com/ryanoasis/nerd-fonts.git
cd nerd-fonts
```

#### Fedora/RHEL

```bash
# Install dependencies
sudo dnf install python3 python3-pip fontforge

# Install Python FontForge bindings
pip3 install fontforge

# Download Font Patcher
git clone --depth 1 https://github.com/ryanoasis/nerd-fonts.git
cd nerd-fonts
```

---

## Directory Structure

After cloning, you should have:

```
nerd-fonts/
  font-patcher           # Main Python script
  src/
    glyphs/
      font-awesome.ttf   # Font Awesome icons
      devicons.ttf       # Devicons
      materialdesign.ttf # Material Design Icons
      octicons.ttf       # Octicons
      powerline.ttf      # Powerline symbols
      codicons.ttf       # Codicons
      # ... other icon fonts
  bin/
    scripts/
      gotta-patch-em-all-font-patcher! # Batch patching script
```

**Note:** You MUST have the `src/glyphs/` directory with icon fonts for patching to work.

---

## Basic Usage

### Command Syntax

```bash
fontforge -script font-patcher [OPTIONS] FontFile.ttf
```

**Or with Python directly:**
```bash
python font-patcher [OPTIONS] FontFile.ttf
```

### Simple Example

```bash
# Patch a font with all icon sets
fontforge -script font-patcher --complete MyFont.ttf
```

**Output:**
```
MyFont Nerd Font Complete.ttf
```

---

## Command-Line Options

### Essential Options

| Option | Short | Description |
|--------|-------|-------------|
| `--complete` | `-c` | Add all available icon sets |
| `--mono` | `-s` | Force monospace (single-width glyphs) |
| `--outputdir DIR` | None | Output directory for patched font |
| `--quiet` | `-q` | Suppress output messages |
| `--careful` | `-l` | Do not overwrite existing glyphs |

### Icon Set Selection

| Option | Short | Icon Set |
|--------|-------|----------|
| `--fontawesome` | None | Font Awesome |
| `--fontawesomeextension` | None | Font Awesome Extension |
| `--fontlogos` | None | Font Logos |
| `--octicons` | None | Octicons (GitHub) |
| `--codicons` | None | Codicons (VS Code) |
| `--powersymbols` | None | Powerline symbols |
| `--pomicons` | None | Pomicons |
| `--powerline` | None | Powerline glyphs |
| `--powerlineextra` | None | Powerline Extra Symbols |
| `--material` | None | Material Design Icons |
| `--weather` | None | Weather Icons |

### Font Metrics Options

| Option | Short | Description |
|--------|-------|-------------|
| `--adjust-line-height` | None | Adjust line height to prevent clipping |
| `--name NAME` | `-n` | Override font name |
| `--postprocess SCRIPT` | None | Run script after patching |
| `--removeligs` | None | Remove ligatures |
| `--makegroups` | None | Create glyph groups |

---

## Common Use Cases

### 1. Patch with All Icons (Recommended)

```bash
fontforge -script font-patcher --complete --mono MyFont.ttf
```

**Explanation:**
- `--complete`: Includes all 13 icon sets
- `--mono`: Forces single-width glyphs (recommended for terminals)

---

### 2. Patch with Specific Icon Sets

```bash
fontforge -script font-patcher \
  --fontawesome \
  --octicons \
  --powersymbols \
  --mono \
  MyFont.ttf
```

**Use Case:** Only need specific icon sets to reduce font size.

---

### 3. Patch Multiple Fonts

```bash
#!/bin/bash
for font in *.ttf; do
  echo "Patching $font..."
  fontforge -script font-patcher --complete --mono --quiet "$font"
done
```

**Use Case:** Batch patch all fonts in a directory.

---

### 4. Patch with Custom Output Directory

```bash
fontforge -script font-patcher \
  --complete \
  --mono \
  --outputdir ./patched-fonts \
  MyFont.ttf
```

**Use Case:** Organize patched fonts in separate directory.

---

### 5. Patch with Custom Font Name

```bash
fontforge -script font-patcher \
  --complete \
  --mono \
  --name "MyCustomFont Nerd Font Mono" \
  MyFont.ttf
```

**Use Case:** Control exact font family name.

---

### 6. Patch and Fix Line Height

```bash
fontforge -script font-patcher \
  --complete \
  --mono \
  --adjust-line-height \
  MyFont.ttf
```

**Use Case:** Prevent icon clipping in terminals with tight line spacing.

---

## Advanced Usage

### Custom Icon Selection Script

```python
#!/usr/bin/env python3
# patch-custom-icons.py

import subprocess
import sys

def patch_font(input_font, output_dir, icon_sets):
    """Patch font with specified icon sets."""
    cmd = [
        "fontforge",
        "-script",
        "font-patcher",
        "--mono",
        "--outputdir", output_dir
    ]

    # Add icon set flags
    icon_flags = {
        "powerline": "--powersymbols",
        "fontawesome": "--fontawesome",
        "devicons": "--devicons",
        "octicons": "--octicons",
        "codicons": "--codicons",
        "material": "--material",
    }

    for icon_set in icon_sets:
        if icon_set in icon_flags:
            cmd.append(icon_flags[icon_set])

    cmd.append(input_font)

    # Execute
    result = subprocess.run(cmd, capture_output=True, text=True)

    if result.returncode == 0:
        print(f"Successfully patched: {input_font}")
    else:
        print(f"Error patching {input_font}: {result.stderr}")

if __name__ == "__main__":
    input_font = sys.argv[1]
    output_dir = sys.argv[2]
    icon_sets = sys.argv[3].split(",")

    patch_font(input_font, output_dir, icon_sets)
```

**Usage:**
```bash
python patch-custom-icons.py MyFont.ttf ./output powerline,fontawesome,octicons
```

---

### Batch Patching with Progress

```bash
#!/bin/bash
# batch-patch.sh

FONT_DIR="./original-fonts"
OUTPUT_DIR="./patched-fonts"
TOTAL=$(ls -1 "$FONT_DIR"/*.ttf | wc -l)
CURRENT=0

mkdir -p "$OUTPUT_DIR"

for font in "$FONT_DIR"/*.ttf; do
  CURRENT=$((CURRENT + 1))
  FONT_NAME=$(basename "$font")

  echo "[$CURRENT/$TOTAL] Patching $FONT_NAME..."

  fontforge -script font-patcher \
    --complete \
    --mono \
    --adjust-line-height \
    --outputdir "$OUTPUT_DIR" \
    --quiet \
    "$font"

  if [ $? -eq 0 ]; then
    echo "  Success"
  else
    echo "  Failed"
  fi
done

echo "Batch patching complete: $CURRENT/$TOTAL fonts processed"
```

---

### Docker-Based Patching

```dockerfile
# Dockerfile for font-patcher
FROM ubuntu:22.04

# Install dependencies
RUN apt-get update && apt-get install -y \
    python3 \
    python3-pip \
    fontforge \
    python3-fontforge \
    git \
    && rm -rf /var/lib/apt/lists/*

# Clone Nerd Fonts
RUN git clone --depth 1 https://github.com/ryanoasis/nerd-fonts.git /nerd-fonts

WORKDIR /nerd-fonts

# Set entrypoint
ENTRYPOINT ["fontforge", "-script", "font-patcher"]
CMD ["--help"]
```

**Build and Use:**
```bash
# Build Docker image
docker build -t font-patcher .

# Patch font using Docker
docker run --rm \
  -v "$(pwd)/fonts:/in:ro" \
  -v "$(pwd)/output:/out" \
  font-patcher \
  --complete \
  --mono \
  --outputdir /out \
  /in/MyFont.ttf
```

**Benefits:**
- No local dependencies required
- Reproducible patching environment
- Easy CI/CD integration

---

## Troubleshooting

### Issue 1: `fontforge: command not found`

**Solution:**
```bash
# Verify FontForge installation
which fontforge

# If not found, install:
# macOS:
brew install fontforge

# Ubuntu/Debian:
sudo apt install fontforge

# Windows:
# Add to PATH: C:\Program Files (x86)\FontForgeBuilds\bin
```

---

### Issue 2: `ModuleNotFoundError: No module named 'fontforge'`

**Solution:**
```bash
# Install Python FontForge bindings
pip3 install fontforge

# If that fails, install system package:
# Ubuntu/Debian:
sudo apt install python3-fontforge

# macOS (use system Python):
brew install fontforge
# Use: /usr/local/bin/fontforge (not system python)
```

---

### Issue 3: `FileNotFoundError: src/glyphs/*.ttf`

**Cause:** Missing icon font files.

**Solution:**
```bash
# Clone full Nerd Fonts repository (not just font-patcher)
git clone --depth 1 https://github.com/ryanoasis/nerd-fonts.git

# Run font-patcher from nerd-fonts directory
cd nerd-fonts
fontforge -script font-patcher --complete MyFont.ttf
```

---

### Issue 4: Font Name Conflicts

**Cause:** Patched font has same name as original.

**Solution:**
```bash
# Use --name to specify custom name
fontforge -script font-patcher \
  --complete \
  --mono \
  --name "MyFont Nerd Font Mono Custom" \
  MyFont.ttf
```

---

### Issue 5: Glyph Overlap Warnings

**Cause:** Icon glyphs overlap with existing font glyphs.

**Solution:**
```bash
# Use --careful flag to skip overlapping glyphs
fontforge -script font-patcher \
  --complete \
  --mono \
  --careful \
  MyFont.ttf
```

---

### Issue 6: Patching Takes Too Long

**Cause:** Processing all 10,390+ glyphs is slow.

**Solution:**
```bash
# Only patch needed icon sets
fontforge -script font-patcher \
  --powersymbols \
  --fontawesome \
  --octicons \
  --mono \
  MyFont.ttf

# Typical times:
# - Powerline only: 10-20 seconds
# - Font Awesome + Octicons: 30-45 seconds
# - Complete (--complete): 60-120 seconds
```

---

## Font Patcher Output

### Output File Naming

**Standard Naming:**
```
Original: MyFont.ttf
Patched:  MyFont Nerd Font Complete.ttf          (--complete)
          MyFont Nerd Font Complete Mono.ttf     (--complete --mono)
          MyFont Nerd Font.ttf                   (without --complete)
          MyFont Nerd Font Mono.ttf              (--mono)
```

**Custom Naming:**
```bash
fontforge -script font-patcher --name "CustomName" MyFont.ttf
# Output: CustomName.ttf
```

---

### Font Metadata

Patched fonts include updated metadata:

```
Family Name:       MyFont Nerd Font Mono
Full Name:         MyFont Nerd Font Mono Regular
PostScript Name:   MyFontNerdFontMono-Regular
Version:           [Original Version] + Nerd Fonts v3.3.0
Copyright:         [Original Copyright] + Nerd Fonts
```

---

## Performance Optimization

### Parallel Patching

```bash
#!/bin/bash
# parallel-patch.sh

FONT_DIR="./fonts"
OUTPUT_DIR="./output"
PARALLEL_JOBS=4

export FONT_DIR OUTPUT_DIR

# Function to patch single font
patch_font() {
  local font="$1"
  fontforge -script font-patcher \
    --complete --mono --quiet \
    --outputdir "$OUTPUT_DIR" \
    "$font"
}

export -f patch_font

# Use GNU parallel
find "$FONT_DIR" -name "*.ttf" | parallel -j $PARALLEL_JOBS patch_font

# Or use xargs
find "$FONT_DIR" -name "*.ttf" | xargs -P $PARALLEL_JOBS -I {} fontforge -script font-patcher --complete --mono --outputdir "$OUTPUT_DIR" {}
```

**Speed Improvement:** 3-4x faster on multi-core systems.

---

### Precompiled Icon Cache

```python
# cache-glyphs.py
import fontforge

def precompile_icon_cache():
    """Precompile icon fonts to speed up patching."""
    icon_fonts = [
        "src/glyphs/font-awesome.ttf",
        "src/glyphs/devicons.ttf",
        "src/glyphs/octicons.ttf",
        # ... others
    ]

    cache = {}

    for icon_font_path in icon_fonts:
        font = fontforge.open(icon_font_path)

        for glyph in font.glyphs():
            if glyph.unicode != -1:
                cache[glyph.unicode] = {
                    "width": glyph.width,
                    "vwidth": glyph.vwidth,
                    "boundingBox": glyph.boundingBox(),
                }

        font.close()

    return cache

# Use cache in font-patcher to skip loading icon fonts repeatedly
```

---

## Integration with CI/CD

### GitHub Actions Example

```yaml
# .github/workflows/patch-fonts.yml
name: Patch Fonts with Nerd Fonts

on:
  push:
    paths:
      - 'fonts/**'

jobs:
  patch:
    runs-on: ubuntu-latest

    steps:
      - name: Checkout repository
        uses: actions/checkout@v3

      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y fontforge python3-fontforge

      - name: Clone Nerd Fonts
        run: |
          git clone --depth 1 https://github.com/ryanoasis/nerd-fonts.git

      - name: Patch fonts
        run: |
          cd nerd-fonts
          for font in ../fonts/*.ttf; do
            fontforge -script font-patcher --complete --mono --outputdir ../patched "$font"
          done

      - name: Upload patched fonts
        uses: actions/upload-artifact@v3
        with:
          name: patched-fonts
          path: patched/*.ttf
```

---

## Best Practices

### 1. Always Use `--mono` for Terminal Fonts

```bash
# Good: Monospace glyphs for terminals
fontforge -script font-patcher --complete --mono MyFont.ttf

# Bad: Variable width glyphs cause alignment issues
fontforge -script font-patcher --complete MyFont.ttf
```

---

### 2. Test Patched Fonts Before Distribution

```bash
# Install patched font
cp "MyFont Nerd Font Complete Mono.ttf" ~/.local/share/fonts/
fc-cache -fv

# Test in terminal
echo ""

# Verify glyphs render correctly
```

---

### 3. Version Control Patched Fonts

```bash
# Don't commit patched fonts to Git (large files)
# Instead, store original fonts and patch on CI/CD

# .gitignore
fonts/patched/
*.nerd-font.ttf
*Nerd Font*.ttf
```

---

### 4. Document Patching Process

```markdown
## Font Patching

This project uses Nerd Fonts for icon support. To patch fonts:

1. Install dependencies: `sudo apt install fontforge python3-fontforge`
2. Clone Nerd Fonts: `git clone https://github.com/ryanoasis/nerd-fonts.git`
3. Patch: `fontforge -script font-patcher --complete --mono fonts/MyFont.ttf`
4. Install: `cp "MyFont Nerd Font Complete Mono.ttf" ~/.local/share/fonts/`
```

---

## References

- **Font Patcher Script:** https://github.com/ryanoasis/nerd-fonts/blob/master/font-patcher
- **Nerd Fonts Wiki:** https://github.com/ryanoasis/nerd-fonts/wiki
- **FontForge Python Scripting:** https://fontforge.org/docs/scripting/python.html
- **Nerd Fonts Releases:** https://github.com/ryanoasis/nerd-fonts/releases

---

**Document Created:** 2025-10-11
**Last Updated:** 2025-10-11
**For:** Windows Terminal Optimization Project
