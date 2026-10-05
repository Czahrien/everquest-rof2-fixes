@echo off
rem Runs zig from %ZIG%, the repo-local venv created by tools\setup-tools.ps1, or PATH.
if defined ZIG (
  "%ZIG%" %*
  exit /b %ERRORLEVEL%
)
if exist "%~dp0..\.tools\venv\Scripts\python.exe" (
  "%~dp0..\.tools\venv\Scripts\python.exe" -m ziglang %*
  exit /b %ERRORLEVEL%
)
where zig >nul 2>nul || (
  echo zig not found: run tools\setup-tools.ps1, put zig on PATH, or set ZIG 1>&2
  exit /b 1
)
zig %*
