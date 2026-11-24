$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$files = @(
    (Join-Path $repoRoot 'build-x86/src/patch_wcx/Release/patch_wcx.wcx')
    (Join-Path $repoRoot 'build/src/patch_wcx/Release/patch_wcx.wcx64')
    (Join-Path $PSScriptRoot 'pluginst.inf')
    (Join-Path $repoRoot 'LICENSE')
    (Join-Path $repoRoot 'README.md')
)

foreach ($file in $files) {
    if (-not (Test-Path -LiteralPath $file -PathType Leaf)) {
        throw "Missing package input: $file"
    }
}

$archive = Join-Path $repoRoot 'build/patch-wcx.zip'
Compress-Archive -LiteralPath $files -DestinationPath $archive -Force
Write-Output $archive
