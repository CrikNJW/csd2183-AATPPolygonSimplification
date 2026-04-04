@echo off
setlocal EnableExtensions

if not defined TEST_TIMEOUT set "TEST_TIMEOUT=600"
if not defined VALIDATION_TOL set "VALIDATION_TOL=1e-2"

where wsl.exe >nul 2>&1
if errorlevel 1 (
    echo [FAIL] WSL is required but wsl.exe was not found.
    exit /b 1
)

set "REPO_WIN=%~dp0"
for %%I in ("%REPO_WIN%\.") do set "REPO_WIN=%%~fI"

wsl.exe bash -lc "set -u -o pipefail; repo=$(wslpath -a \"%REPO_WIN%\"); cd \"$repo\" || exit 1; TEST_TIMEOUT=\"%TEST_TIMEOUT%\" VALIDATION_TOL=\"%VALIDATION_TOL%\" bash scripts/build_and_test_wsl.sh"
set "EXIT_CODE=%ERRORLEVEL%"
exit /b %EXIT_CODE%
