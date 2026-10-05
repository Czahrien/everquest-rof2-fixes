# Installs the pinned build tools (zig, cmake, ninja) into .tools\venv. Needs Python 3.
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$venv = Join-Path $root '.tools\venv'
$requirements = Join-Path $PSScriptRoot 'requirements.txt'

if (Get-Command uv -ErrorAction SilentlyContinue) {
  uv venv -q --allow-existing $venv
  if ($LASTEXITCODE) { exit $LASTEXITCODE }
  uv pip install -q -p $venv -r $requirements
} else {
  $python = if (Get-Command py -ErrorAction SilentlyContinue) { 'py' } else { 'python' }
  & $python -m venv $venv
  if ($LASTEXITCODE) { exit $LASTEXITCODE }
  & (Join-Path $venv 'Scripts\python.exe') -m pip install -q -r $requirements
}
if ($LASTEXITCODE) { exit $LASTEXITCODE }
& (Join-Path $PSScriptRoot 'zig.cmd') version
