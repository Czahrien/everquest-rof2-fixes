# Builds build\dinput8.dll on Windows with zig and Ninja.
# Run tools\setup-tools.ps1 once first to get pinned zig/cmake/ninja in .tools\venv.
param([Parameter(ValueFromRemainingArguments = $true)] [string[]] $CMakeArgs)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$build = Join-Path $root 'build'
$venvScripts = Join-Path $root '.tools\venv\Scripts'
if (Test-Path $venvScripts) { $env:PATH = "$venvScripts;$env:PATH" }

foreach ($tool in 'cmake', 'ninja') {
  if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
    throw "$tool not found: run tools\setup-tools.ps1 first"
  }
}

$cache = Join-Path $build 'CMakeCache.txt'
if ((Test-Path $cache) -and -not (Select-String -Path $cache -Pattern 'CMAKE_GENERATOR:INTERNAL=Ninja' -Quiet)) {
  Remove-Item -Recurse -Force $build
}

$toolchain = Join-Path $root 'cmake\zig-x86-windows.cmake'
cmake -S $root -B $build -G Ninja "-DCMAKE_TOOLCHAIN_FILE=$toolchain" @CMakeArgs
if ($LASTEXITCODE) { exit $LASTEXITCODE }
cmake --build $build
if ($LASTEXITCODE) { exit $LASTEXITCODE }
Write-Host "built $build\dinput8.dll"
