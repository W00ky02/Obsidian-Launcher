param(
    [Parameter(Mandatory=$true)][string]$QtPath,
    [string]$BuildDir = 'build',
    [string]$PackageDir = 'dist'
)
$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot
$qtRoot = (Resolve-Path -LiteralPath $QtPath).Path
if (-not (Test-Path -LiteralPath (Join-Path $qtRoot 'lib\cmake\Qt6\Qt6Config.cmake'))) {
    throw 'QtPath muss auf das msvc2022_64-Verzeichnis einer Qt-6-Installation zeigen.'
}
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw 'CMake fehlt im PATH. Bitte in Developer PowerShell for VS 2022 ausführen.'
}
function Check-Result { if ($LASTEXITCODE -ne 0) { throw "Build-Schritt fehlgeschlagen (Exitcode $LASTEXITCODE)." } }
cmake -S . -B $BuildDir -G 'Visual Studio 17 2022' -A x64 "-DCMAKE_PREFIX_PATH=$qtRoot" -DBUILD_TESTING=ON
Check-Result
cmake --build $BuildDir --config Release --parallel
Check-Result
$env:PATH = (Join-Path $qtRoot 'bin') + [IO.Path]::PathSeparator + $env:PATH
ctest --test-dir $BuildDir -C Release --output-on-failure
Check-Result
cmake --install $BuildDir --config Release --prefix $PackageDir
Check-Result
Write-Host "Fertig: $(Join-Path $PackageDir 'ObsidianLauncher.exe')"
