Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$buildDir = Join-Path $root "build"
$objDir = Join-Path $buildDir "obj"
$binDir = Join-Path $buildDir "bin"
$generatedInclude = Join-Path $buildDir "generated\include"

New-Item -ItemType Directory -Force -Path $objDir, $binDir, $generatedInclude | Out-Null

function Invoke-Native {
    param(
        [Parameter(Mandatory = $true)]
        [string] $FilePath,

        [Parameter(Mandatory = $true)]
        [string[]] $Arguments
    )

    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$FilePath failed with exit code $LASTEXITCODE"
    }
}

$msvcRoots = Get-ChildItem -Directory "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC" -ErrorAction SilentlyContinue |
    Sort-Object Name -Descending
if (-not $msvcRoots) {
    throw "MSVC was not found under Visual Studio 18 Community."
}
$msvcRoot = $msvcRoots[0].FullName
$cl = Join-Path $msvcRoot "bin\Hostx64\x64\cl.exe"
if (-not (Test-Path $cl)) {
    $cl = Join-Path $msvcRoot "bin\HostX64\x64\cl.exe"
}
if (-not (Test-Path $cl)) {
    throw "cl.exe was not found under $msvcRoot."
}

$sdkRoots = Get-ChildItem -Directory "C:\Program Files (x86)\Windows Kits\10\Include" -ErrorAction SilentlyContinue |
    Sort-Object Name -Descending
if (-not $sdkRoots) {
    throw "Windows 10 SDK include directory was not found."
}
$sdkVersion = $sdkRoots[0].Name
$sdkInclude = Join-Path "C:\Program Files (x86)\Windows Kits\10\Include" $sdkVersion
$sdkLib = Join-Path "C:\Program Files (x86)\Windows Kits\10\Lib" $sdkVersion

$template = Get-Content -LiteralPath (Join-Path $root "third_party\igsc\include\igsc_lib.h.in") -Raw
$generated = $template.
    Replace("@GSC_VERSION_MAJOR@", "1").
    Replace("@GSC_VERSION_MINOR@", "3").
    Replace("@GSC_VERSION_PATCH@", "1").
    Replace("@GSC_VERSION_BUILD@", "0")
Set-Content -LiteralPath (Join-Path $generatedInclude "igsc_lib.h") -Value $generated -Encoding ascii

$includeArgs = @(
    "/I$generatedInclude",
    "/I$(Join-Path $root 'third_party\igsc\include')",
    "/I$(Join-Path $root 'third_party\igsc\lib')",
    "/I$(Join-Path $root 'third_party\metee\include')",
    "/I$(Join-Path $root 'third_party\metee\src\Windows')",
    "/I$(Join-Path $sdkInclude 'ucrt')",
    "/I$(Join-Path $sdkInclude 'shared')",
    "/I$(Join-Path $sdkInclude 'um')",
    "/I$(Join-Path $sdkInclude 'winrt')",
    "/I$(Join-Path $msvcRoot 'include')"
)

$commonDefines = @(
    "/DWIN32",
    "/D_WINDOWS",
    "/DUNICODE",
    "/D_UNICODE",
    "/DSYSLOG",
    "/D_CRT_SECURE_NO_WARNINGS"
)

$sources = @(
    "third_party\metee\src\Windows\metee_win.c",
    "third_party\metee\src\Windows\metee_winhelpers.c",
    "third_party\igsc\lib\igsc_lib.c",
    "third_party\igsc\lib\igsc_log.c",
    "third_party\igsc\lib\ifr.c",
    "third_party\igsc\lib\oprom.c",
    "third_party\igsc\lib\oprom_parser.c",
    "third_party\igsc\lib\fw_data_parser.c",
    "third_party\igsc\lib\enum\igsc_enum_windows.c",
    "third_party\igsc\lib\power\igsc_power_windows.c",
    "src\main.cpp"
)

$objects = @()
foreach ($relativeSource in $sources) {
    $source = Join-Path $root $relativeSource
    $objectName = ($relativeSource -replace "[:\\/]", "_") -replace "\.(c|cpp)$", ".obj"
    $object = Join-Path $objDir $objectName
    $objects += $object

    $languageArgs = if ($relativeSource.EndsWith(".cpp")) {
        @("/TP", "/EHsc", "/std:c++17")
    } else {
        @("/TC")
    }

    $compileArgs = @("/nologo", "/O2", "/MT", "/W3", "/FIstdarg.h") +
        $languageArgs +
        $commonDefines +
        $includeArgs +
        @("/Fo$object", "/c", $source)
    Invoke-Native $cl $compileArgs
}

$libPaths = @(
    "/LIBPATH:$(Join-Path $msvcRoot 'lib\x64')",
    "/LIBPATH:$(Join-Path $sdkLib 'ucrt\x64')",
    "/LIBPATH:$(Join-Path $sdkLib 'um\x64')"
)

$exe = Join-Path $binDir "B70Pcie.exe"
$linkArgs = @("/nologo", "/Fe$exe") + $objects + @("/link") + $libPaths +
    @("CfgMgr32.lib", "SetupAPI.lib", "Ole32.lib", "Advapi32.lib")
Invoke-Native $cl $linkArgs

Write-Host "Built $exe"
