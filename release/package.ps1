param(
    [string] $BinaryPath = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$packageRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($BinaryPath)) {
    $BinaryPath = Join-Path $packageRoot 'build\bin\B70Pcie.exe'
}
$BinaryPath = (Resolve-Path -LiteralPath $BinaryPath).Path
$outputDir = Join-Path $packageRoot 'build\release'
$stagingDir = Join-Path $outputDir ('staging-' + [guid]::NewGuid().ToString('N'))
$packageName = 'B70Pcie-v1.1-win64'
$packageDir = Join-Path $stagingDir $packageName
New-Item -ItemType Directory -Force -Path $packageDir | Out-Null

$packageFiles = [ordered]@{
    'B70Pcie.exe' = $BinaryPath
    'README.txt' = Join-Path $packageRoot 'release\README-WINDOWS.txt'
    'LICENSE.txt' = Join-Path $packageRoot 'LICENSE'
    'THIRD_PARTY_NOTICES.md' = Join-Path $packageRoot 'THIRD_PARTY_NOTICES.md'
    'LICENSE-IGSC.txt' = Join-Path $packageRoot 'third_party\igsc\LICENSE.txt'
    'LICENSE-MeTee.txt' = Join-Path $packageRoot 'third_party\metee\COPYING'
    'LICENSE-MeTee-Windows-WDK.txt' = Join-Path $packageRoot 'third_party\metee\src\Windows\LICENSE.WDK.Ms-LPL'
}
foreach ($entry in $packageFiles.GetEnumerator()) {
    Copy-Item -LiteralPath $entry.Value -Destination (Join-Path $packageDir $entry.Key)
}

$checksums = foreach ($name in ($packageFiles.Keys | Sort-Object)) {
    $hash = (Get-FileHash -LiteralPath (Join-Path $packageDir $name) -Algorithm SHA256).Hash.ToLowerInvariant()
    "$hash  $name"
}
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[IO.File]::WriteAllText((Join-Path $packageDir 'SHA256SUMS.txt'), (($checksums -join "`n") + "`n"), $utf8NoBom)

$zipPath = Join-Path $outputDir ($packageName + '.zip')
Compress-Archive -LiteralPath $packageDir -DestinationPath $zipPath -Force
$zipHash = (Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText(($zipPath + '.sha256'), "$zipHash  $packageName.zip`n", $utf8NoBom)
Write-Host "Packaged $zipPath"
Write-Host "ZIP SHA-256: $zipHash"
Write-Host ('EXE SHA-256: ' + (Get-FileHash -LiteralPath $BinaryPath -Algorithm SHA256).Hash.ToLowerInvariant())
