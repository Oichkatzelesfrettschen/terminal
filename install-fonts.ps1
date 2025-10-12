# ============================================================================
# Install Bundled Fonts for Ultra-Riced Windows Terminal
# ============================================================================
#
# This script installs the following fonts to your Windows system:
# - Spline Sans Mono (10 variants, 784 KB)
# - CaskaydiaCove Nerd Font Mono (12 variants, 30 MB)
#
# Requires: Administrator privileges
# ============================================================================

#Requires -RunAsAdministrator

Write-Host "============================================================================" -ForegroundColor Cyan
Write-Host "Ultra-Riced Windows Terminal - Font Installer" -ForegroundColor Cyan
Write-Host "============================================================================" -ForegroundColor Cyan
Write-Host ""

$ErrorActionPreference = "Stop"

# Determine script location and font directories
$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$SplineFontDir = Join-Path $ScriptRoot "fonts\spline-sans-mono"
$CascadiaFontDir = Join-Path $ScriptRoot "fonts\cascadia-code-nerd-font"

# Windows Fonts directory
$FontsDir = "$env:windir\Fonts"
$FontsRegistryPath = "HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Fonts"

# Function to install a font
function Install-Font {
    param(
        [string]$FontPath
    )

    $FontFile = Get-Item $FontPath
    $FontName = $FontFile.Name

    Write-Host "  Installing: $FontName" -ForegroundColor Yellow

    try {
        # Copy font file to Windows Fonts directory
        $DestPath = Join-Path $FontsDir $FontName

        if (Test-Path $DestPath) {
            Write-Host "    Font already exists, skipping..." -ForegroundColor Gray
            return $true
        }

        Copy-Item $FontPath $DestPath -Force

        # Register font in registry
        # Get font family name from file metadata
        $Shell = New-Object -ComObject Shell.Application
        $Folder = $Shell.Namespace((Get-Item $FontPath).DirectoryName)
        $Item = $Folder.ParseName($FontName)
        $FontDisplayName = $Folder.GetDetailsOf($Item, 21)  # 21 = Name property

        if ([string]::IsNullOrWhiteSpace($FontDisplayName)) {
            # Fallback: use filename without extension
            $FontDisplayName = [System.IO.Path]::GetFileNameWithoutExtension($FontName)
        }

        # Add registry entry
        $RegName = "$FontDisplayName (TrueType)"
        Set-ItemProperty -Path $FontsRegistryPath -Name $RegName -Value $FontName -Type String -Force

        Write-Host "    SUCCESS: Font installed" -ForegroundColor Green
        return $true

    } catch {
        Write-Host "    ERROR: Failed to install font" -ForegroundColor Red
        Write-Host "    $($_.Exception.Message)" -ForegroundColor Red
        return $false
    }
}

# Install Spline Sans Mono fonts
Write-Host "[1/2] Installing Spline Sans Mono..." -ForegroundColor Cyan
Write-Host ""

if (-not (Test-Path $SplineFontDir)) {
    Write-Host "ERROR: Spline Sans Mono font directory not found!" -ForegroundColor Red
    Write-Host "Expected path: $SplineFontDir" -ForegroundColor Red
    exit 1
}

$SplineFonts = Get-ChildItem -Path $SplineFontDir -Filter "*.ttf"
$SplineSuccess = 0
$SplineTotal = $SplineFonts.Count

foreach ($Font in $SplineFonts) {
    if (Install-Font -FontPath $Font.FullName) {
        $SplineSuccess++
    }
}

Write-Host ""
Write-Host "  Installed $SplineSuccess of $SplineTotal Spline Sans Mono variants" -ForegroundColor $(if ($SplineSuccess -eq $SplineTotal) { "Green" } else { "Yellow" })
Write-Host ""

# Install Cascadia Code Nerd Font (CaskaydiaCove Mono)
Write-Host "[2/2] Installing CaskaydiaCove Nerd Font Mono..." -ForegroundColor Cyan
Write-Host ""

if (-not (Test-Path $CascadiaFontDir)) {
    Write-Host "ERROR: Cascadia Code Nerd Font directory not found!" -ForegroundColor Red
    Write-Host "Expected path: $CascadiaFontDir" -ForegroundColor Red
    exit 1
}

$CascadiaFonts = Get-ChildItem -Path $CascadiaFontDir -Filter "*.ttf"
$CascadiaSuccess = 0
$CascadiaTotal = $CascadiaFonts.Count

foreach ($Font in $CascadiaFonts) {
    if (Install-Font -FontPath $Font.FullName) {
        $CascadiaSuccess++
    }
}

Write-Host ""
Write-Host "  Installed $CascadiaSuccess of $CascadiaTotal CaskaydiaCove variants" -ForegroundColor $(if ($CascadiaSuccess -eq $CascadiaTotal) { "Green" } else { "Yellow" })
Write-Host ""

# Summary
Write-Host "============================================================================" -ForegroundColor Cyan
Write-Host "Font Installation Complete!" -ForegroundColor Green
Write-Host "============================================================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Installed Fonts:" -ForegroundColor Yellow
Write-Host "  - Spline Sans Mono: $SplineSuccess/$SplineTotal variants" -ForegroundColor White
Write-Host "  - CaskaydiaCove Nerd Font Mono: $CascadiaSuccess/$CascadiaTotal variants" -ForegroundColor White
Write-Host ""
Write-Host "How to use:" -ForegroundColor Yellow
Write-Host "  1. Restart Windows Terminal (close all instances)" -ForegroundColor White
Write-Host "  2. Open Settings (Ctrl+,)" -ForegroundColor White
Write-Host "  3. Go to Defaults or Profile settings" -ForegroundColor White
Write-Host "  4. Set Font Face to:" -ForegroundColor White
Write-Host "     - 'Spline Sans Mono' for compact, modern look" -ForegroundColor Cyan
Write-Host "     - 'CaskaydiaCove Nerd Font Mono' for PowerLine + icon glyphs" -ForegroundColor Cyan
Write-Host ""
Write-Host "PowerLine/Nerd Fonts Features:" -ForegroundColor Yellow
Write-Host "  CaskaydiaCove Nerd Font Mono includes 10,390+ icon glyphs:" -ForegroundColor White
Write-Host "  - PowerLine symbols (arrows, triangles)" -ForegroundColor Gray
Write-Host "  - Font Awesome icons" -ForegroundColor Gray
Write-Host "  - Material Design icons" -ForegroundColor Gray
Write-Host "  - Devicons, Octicons, Pomicons, and more" -ForegroundColor Gray
Write-Host ""
Write-Host "Note: You may need to log out and log back in for all applications" -ForegroundColor Gray
Write-Host "to recognize the newly installed fonts." -ForegroundColor Gray
Write-Host ""
