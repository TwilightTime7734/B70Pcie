# Build the real executable, then test the CLI with simulated IGSC devices.
. (Join-Path $PSScriptRoot '..\build.ps1')

$testExe = Join-Path $binDir 'B70Pcie-status-tests.exe'
$testObject = Join-Path $objDir 'status_tests.obj'
$testSource = Join-Path $root 'tests\status_tests.cpp'
$testArgs = @('/nologo', '/O2', '/MT', '/W3', '/TP', '/EHsc', '/std:c++17') +
    $commonDefines + $includeArgs + @("/Fo$testObject", "/Fe$testExe", $testSource, '/link') +
    $libPaths + @('Advapi32.lib')
Invoke-Native $cl $testArgs
& $testExe
if ($LASTEXITCODE -ne 0) {
    throw "CLI tests failed with exit code $LASTEXITCODE"
}
