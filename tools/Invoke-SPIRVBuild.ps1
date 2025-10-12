[CmdletBinding()]
param(
    [ValidateNotNullOrEmpty()]
    [string]$Configuration = "UltraPerformance.GL",

    [ValidateNotNullOrEmpty()]
    [string]$DxcPath = "dxc.exe",

    [ValidateNotNullOrEmpty()]
    [string]$SpirvCrossPath = "spirv-cross.exe",

    [ValidateNotNullOrEmpty()]
    [string]$ProjectRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot ".."))
)

$ErrorActionPreference = 'Stop'

function Invoke-DxcSpirv {
    param(
        [string]$Input,
        [string]$Profile,
        [string]$EntryPoint,
        [string[]]$Includes,
        [string]$OutputSpirv
    )

    $args = @(
        "-T", $Profile,
        "-E", $EntryPoint,
        "-spirv",
        "-fspv-target-env=vulkan1.2"
    )

    foreach ($inc in $Includes) {
        $args += @("-I", $inc)
    }

    $args += @("-Fo", $OutputSpirv)
    $args += $Input

    Write-Host "[DXC-SPIRV]" (Split-Path $Input -Leaf) "=>" (Split-Path $OutputSpirv -Leaf)
    $proc = Start-Process -FilePath $DxcPath -ArgumentList $args -NoNewWindow -Wait -PassThru
    if ($proc.ExitCode -ne 0) {
        throw "DXC SPIR-V compilation failed for $Input"
    }
}

function Invoke-SpirvCrossToGLSL {
    param(
        [string]$InputSpirv,
        [string]$OutputGlsl,
        [int]$Version
    )

    $args = @(
        "--version", $Version,
        "--es",
        "--output", $OutputGlsl,
        $InputSpirv
    )

    Write-Host "[SPIRV-CROSS]" (Split-Path $InputSpirv -Leaf) "=>" (Split-Path $OutputGlsl -Leaf)
    $proc = Start-Process -FilePath $SpirvCrossPath -ArgumentList $args -NoNewWindow -Wait -PassThru
    if ($proc.ExitCode -ne 0) {
        throw "SPIRV-Cross failed for $InputSpirv"
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
    @{ Input = Join-Path $shaderRoot "shader_d3d12_vs.hlsl"; Profile = "vs_6_0"; Entry = "main"; Spirv = Join-Path $outRoot "shader_vs.spv"; Glsl = Join-Path $outRoot "shader_vs.glsl" },
    @{ Input = Join-Path $shaderRoot "shader_d3d12_ps.hlsl"; Profile = "ps_6_0"; Entry = "main"; Spirv = Join-Path $outRoot "shader_ps.spv"; Glsl = Join-Path $outRoot "shader_ps.glsl" }
)

foreach ($shader in $shaders) {
    Invoke-DxcSpirv -Input $shader.Input `
                    -Profile $shader.Profile `
                    -EntryPoint $shader.Entry `
                    -Includes $includePaths `
                    -OutputSpirv $shader.Spirv

    Invoke-SpirvCrossToGLSL -InputSpirv $shader.Spirv `
                             -OutputGlsl $shader.Glsl `
                             -Version 460
}

Write-Host "SPIR-V build complete. Artifacts placed in" $outRoot
