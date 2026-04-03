@echo off
setlocal EnableDelayedExpansion

echo ========================================
echo Building simplify...
echo ========================================
bash -lc "make simplify"
if errorlevel 1 (
    echo Build failed!
    exit /b 1
)
echo Build successful!
echo.

set "TEST_DIR=test_cases"
set "TMP_DIR=tmp_test_outputs"

if exist "%TMP_DIR%" rmdir /s /q "%TMP_DIR%"
mkdir "%TMP_DIR%"

set TOTAL=0
set PASSED=0

echo ========================================
echo Running Test Cases...
echo ========================================
echo.
echo Test Case                                      Target   Status   Metrics
echo ============================================================================

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
echo Summary: %PASSED%/%TOTAL% tests passed
echo ========================================

rmdir /s /q "%TMP_DIR%"

if not %PASSED%==%TOTAL% exit /b 1
exit /b 0

:test
set "INPUT=%~1"
set "TARGET=%~2"
set /a TOTAL+=1

set "INPUT_UNIX=test_cases/%INPUT%"
set "OUTPUT=%INPUT:input_=output_%"
set "OUTPUT=%OUTPUT:.csv=.txt%"
set "EXPECTED_PATH=%TEST_DIR%\%OUTPUT%"
set "ACTUAL_PATH=%TMP_DIR%\%OUTPUT%"

bash -lc "./simplify '%INPUT_UNIX%' %TARGET% 2>/dev/null" > "%ACTUAL_PATH%"

set "STATUS=FAIL"
set "EXACT=NO"
set "METRICS=NO"

fc /b "%EXPECTED_PATH%" "%ACTUAL_PATH%" >nul 2>&1
if not errorlevel 1 (
    set "EXACT=YES"
    set "STATUS=PASS"
    set /a PASSED+=1
)

findstr /C:"Total signed area" "%ACTUAL_PATH%" >nul 2>&1
if not errorlevel 1 (
    set "METRICS=YES"
)

echo !INPUT!                                !TARGET!       !STATUS!     !METRICS!

exit /b 0
