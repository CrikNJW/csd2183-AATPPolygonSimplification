@echo off
setlocal EnableDelayedExpansion

if not defined TEST_TIMEOUT set TEST_TIMEOUT=600
if not defined VALIDATION_TOL set VALIDATION_TOL=1e-2
set "OUTPUT_DIR=outputs"
set TOTAL=0
set PASSED=0
set WARNED=0

if exist "%OUTPUT_DIR%" rmdir /s /q "%OUTPUT_DIR%"
mkdir "%OUTPUT_DIR%"

echo ========================================
echo Building simplify...
echo ========================================
bash -lc "make simplify"
if errorlevel 1 (
    echo Build failed!
    exit /b 1
)
echo Build successful!
echo Outputs will be stored in: %OUTPUT_DIR%\
echo Validation tolerance: %VALIDATION_TOL%
echo.

call :test "input_rectangle_with_two_holes.csv" 7
call :test "input_cushion_with_hexagonal_hole.csv" 13
call :test "input_blob_with_two_holes.csv" 17
call :test "input_wavy_with_three_holes.csv" 21
call :test "input_lake_with_two_islands.csv" 17
call :test "input_original_01.csv" 99
call :test "input_original_02.csv" 99
call :test "input_original_03.csv" 99
call :test "input_original_04.csv" 99
call :test "input_original_05.csv" 99
call :test "input_original_06.csv" 99
call :test "input_original_07.csv" 99
call :test "input_original_08.csv" 99
call :test "input_original_09.csv" 99
call :test "input_original_10.csv" 99

echo.
echo ========================================
echo Summary: %PASSED%/%TOTAL% tests passed validation, %WARNED% warnings
echo ========================================
if not %PASSED%==%TOTAL% exit /b 1
exit /b 0

:test
set "INPUT=%~1"
set "TARGET=%~2"
set "OUT_NAME=%INPUT:input_=output_%"
set "OUT_NAME=%OUT_NAME:.csv=.txt%"
set "OUT_PATH=%OUTPUT_DIR%\%OUT_NAME%"
set "TMP_OUT=tmp_%RANDOM%_test.txt"
set "TMP_JSON=tmp_%RANDOM%_validate.json"
set "TMP_LOG=tmp_%RANDOM%_validate.log"
set /a TOTAL+=1

del "%OUT_PATH%" >nul 2>&1

bash -lc "timeout %TEST_TIMEOUT%s ./simplify test_cases/%INPUT% %TARGET% 2>/dev/null" > "%TMP_OUT%"
if errorlevel 1 (
    if "%errorlevel%"=="124" (
        echo [FAIL] %INPUT% - simplify timed out after %TEST_TIMEOUT%s
    ) else (
        echo [FAIL] %INPUT% - simplify execution failed
    )
    del "%TMP_OUT%" >nul 2>&1
    exit /b 0
)

copy /Y "%TMP_OUT%" "%OUT_PATH%" >nul

python scripts\validate_output.py ^
  --input "test_cases\%INPUT%" ^
  --output "%OUT_PATH%" ^
  --target %TARGET% ^
  --tol %VALIDATION_TOL% ^
  --json "%TMP_JSON%" > "%TMP_LOG%" 2>&1

if errorlevel 1 (
    echo [FAIL] %INPUT% - validation script failed
    type "%TMP_LOG%"
    del "%TMP_OUT%" "%TMP_JSON%" "%TMP_LOG%" >nul 2>&1
    exit /b 0
)

for /f "usebackq delims=" %%L in (`python -c "import json;d=json.load(open('%TMP_JSON%'));print('PASS' if d['ok'] else 'FAIL');print(d['summary']);print('WARN' if d['vertex_warn'] else 'NOWARN')"`) do (
    if not defined _line1 (
        set "_line1=%%L"
    ) else if not defined _line2 (
        set "_line2=%%L"
    ) else (
        set "_line3=%%L"
    )
)

if "!_line1!"=="PASS" (
    if "!_line3!"=="WARN" (
        echo [PASS^|WARN] %INPUT% - !_line2!
        set /a WARNED+=1
    ) else (
        echo [PASS] %INPUT% - !_line2!
    )
    set /a PASSED+=1
) else (
    echo [FAIL] %INPUT% - !_line2!
)

set "_line1="
set "_line2="
set "_line3="
del "%TMP_OUT%" "%TMP_JSON%" "%TMP_LOG%" >nul 2>&1
exit /b 0
