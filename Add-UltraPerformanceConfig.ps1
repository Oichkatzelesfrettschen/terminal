$slnPath = "OpenConsole.slnx"
$slnContent = Get-Content $slnPath -Raw

# Add UltraPerformance to solution configurations
$configPattern = 'Configurations'
$ultraPerfConfig = '    <BuildType Name="UltraPerformance" />'
if ($slnContent -notmatch $ultraPerfConfig) {
    $slnContent = $slnContent -replace $configPattern, "$configPattern\n$ultraPerfConfig"
    Set-Content -Path $slnPath -Value $slnContent
}

$vcxprojFiles = Get-ChildItem -Path "src" -Recurse -Filter "*.vcxproj"

foreach ($file in $vcxprojFiles) {
    $content = Get-Content $file.FullName -Raw

    # Add UltraPerformance PropertyGroup
    $postPropsImport = '<Import Project="$(SolutionDir)src\\common.build.post.props" />'
    $ultraPerfPropertyGroup = @"
  <PropertyGroup Condition="'\$(Configuration)'=='UltraPerformance'">
    <Import Project="\$(SolutionDir)src\\common.build.ultraperformance.props" />
  </PropertyGroup>
"@

    if ($content -notmatch '<PropertyGroup Condition="\'\$(Configuration)'==\'UltraPerformance\'">') {
        $content = $content -replace [regex]::Escape($postPropsImport), "$ultraPerfPropertyGroup$postPropsImport"
        Set-Content -Path $file.FullName -Value $content
    }
}
