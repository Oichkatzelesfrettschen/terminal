[CmdletBinding()]
param(
    [ValidateNotNullOrEmpty()]
    [string]$Configuration = "UltraPerformance",

    [ValidateNotNullOrEmpty()]
    [string]$DxcPath = "dxc.exe",

    [ValidateNotNullOrEmpty()]
    [string]$ProjectRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot ".."))
)

$ErrorActionPreference = 'Stop'

function Invoke-DxcShader {
    param(
        [string]$Input,
        [string]$Profile,
        [string]$EntryPoint,
        [string[]]$Includes,
        [hashtable]$Defines,
        [string]$OutputBinary,
        [string]$OutputHeader
    )

    $args = @(
        "-T", $Profile,
        "-E", $EntryPoint,
        "-Zi",
        "-Qstrip_debug",
        "-Qstrip_reflect",
        "-WX"
    )

    foreach ($inc in $Includes) {
        $args += @("-I", $inc)
    }

    if ($Defines) {
        foreach ($key in $Defines.Keys) {
            $args += @("-D", "$key=$($Defines[$key])")
        }
    }

    if ($OutputHeader) {
        $args += @("-Fre", $OutputHeader)
    }

    if ($OutputBinary) {
        $args += @("-Fo", $OutputBinary)
    }

    $args += $Input

    Write-Host "[DXC]" (Split-Path $Input -Leaf) "=>" (Split-Path $OutputBinary -Leaf)
    $proc = Start-Process -FilePath $DxcPath -ArgumentList $args -NoNewWindow -Wait -PassThru
    if ($proc.ExitCode -ne 0) {
        throw "DXC compilation failed for $Input"
    }
}

$shaderRoot = Join-Path $ProjectRoot "src/renderer/atlas"
$outRoot = Join-Path $ProjectRoot "build/shaders/$Configuration"

New-Item -ItemType Directory -Force -Path $outRoot | Out-Null

$includePaths = @(
    $shaderRoot,
    (Join-Path $shaderRoot "shaders"),
    (Join-Path $shaderRoot "shaders/hlsl"),
    (Join-Path $shaderRoot "shaders/hlsl/common")
)

$shaders = @(
    @{ Input = Join-Path $shaderRoot "shader_d3d12_vs.hlsl"; Profile = "vs_6_0"; Entry = "main"; Defines = @{}; Suffix = "shader_d3d12_vs" },
    @{ Input = Join-Path $shaderRoot "shader_d3d12_ps.hlsl"; Profile = "ps_6_0"; Entry = "main"; Defines = @{}; Suffix = "shader_d3d12_ps" },
    @{ Input = Join-Path $shaderRoot "shader_vs.hlsl"; Profile = "vs_5_1"; Entry = "main"; Defines = @{}; Suffix = "shader_vs" },
    @{ Input = Join-Path $shaderRoot "shader_ps.hlsl"; Profile = "ps_5_1"; Entry = "main"; Defines = @{}; Suffix = "shader_ps" },
    @{ Input = Join-Path $shaderRoot "grid_generate_cs.hlsl"; Profile = "cs_6_0"; Entry = "main"; Defines = @{}; Suffix = "grid_generate_cs" },
    @{ Input = Join-Path $shaderRoot "glyph_rasterize_cs.hlsl"; Profile = "cs_6_0"; Entry = "main"; Defines = @{}; Suffix = "glyph_rasterize_cs" }
)

foreach ($shader in $shaders) {
    $outputBinary = Join-Path $outRoot ($shader.Suffix + ".cso")
    $outputHeader = Join-Path $outRoot ($shader.Suffix + ".h")

    Invoke-DxcShader -Input $shader.Input `
                      -Profile $shader.Profile `
                      -EntryPoint $shader.Entry `
                      -Includes $includePaths `
                      -Defines $shader.Defines `
                      -OutputBinary $outputBinary `
                      -OutputHeader $outputHeader
}

Write-Host "DXC build complete. Artifacts placed in" $outRoot
