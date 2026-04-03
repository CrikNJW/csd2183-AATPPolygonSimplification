@echo off
setlocal EnableDelayedExpansion

if not defined TEST_TIMEOUT set TEST_TIMEOUT=600
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
set /a TOTAL+=1

echo Testing: %INPUT% (target=%TARGET%) -> %OUT_PATH%
del "%OUT_PATH%" tmp_test.txt tmp_in_area.txt tmp_out_area.txt tmp_area_ok.txt tmp_in_rings.txt tmp_out_rings.txt >nul 2>&1

bash -lc "timeout %TEST_TIMEOUT%s ./simplify test_cases/%INPUT% %TARGET% 2>/dev/null" > tmp_test.txt
if errorlevel 1 (
    set "RC=%errorlevel%"
    if "!RC!"=="124" (
        echo   [FAIL] simplify timed out after %TEST_TIMEOUT%s
    ) else (
        echo   [FAIL] simplify execution failed
    )
    del "%OUT_PATH%" tmp_test.txt >nul 2>&1
    exit /b 0
)

findstr /C:"Total signed area in input:" tmp_test.txt >nul || (
    echo   [FAIL] Missing input area metric
    del "%OUT_PATH%" tmp_test.txt >nul 2>&1
    exit /b 0
)
findstr /C:"Total signed area in output:" tmp_test.txt >nul || (
    echo   [FAIL] Missing output area metric
    del "%OUT_PATH%" tmp_test.txt >nul 2>&1
    exit /b 0
)
findstr /C:"Total areal displacement:" tmp_test.txt >nul || (
    echo   [FAIL] Missing displacement metric
    del "%OUT_PATH%" tmp_test.txt >nul 2>&1
    exit /b 0
)

bash -lc "grep 'Total signed area in input:' tmp_test.txt | awk '{print \$NF}'" > tmp_in_area.txt
bash -lc "grep 'Total signed area in output:' tmp_test.txt | awk '{print \$NF}'" > tmp_out_area.txt
set /p IN_AREA=<tmp_in_area.txt
set /p OUT_AREA=<tmp_out_area.txt

bash -lc "awk 'BEGIN{a=!IN_AREA!; b=!OUT_AREA!; d=a-b; if (d<0) d=-d; if (d<=1e-9) print \"YES\"; else print \"NO\"}'" > tmp_area_ok.txt
set /p AREA_OK=<tmp_area_ok.txt
if not "!AREA_OK!"=="YES" (
    echo   [FAIL] Area not preserved: !IN_AREA! != !OUT_AREA!
    del "%OUT_PATH%" tmp_test.txt tmp_in_area.txt tmp_out_area.txt tmp_area_ok.txt >nul 2>&1
    exit /b 0
)

bash -lc "awk -F, 'NR>1 && \$1 ~ /^[0-9]+$/ {print \$1}' test_cases/%INPUT% | sort -u | wc -l" > tmp_in_rings.txt
bash -lc "awk -F, 'NR>1 && \$1 ~ /^[0-9]+$/ {print \$1}' tmp_test.txt | sort -u | wc -l" > tmp_out_rings.txt
set /p IN_RINGS=<tmp_in_rings.txt
set /p OUT_RINGS=<tmp_out_rings.txt
if not "!IN_RINGS!"=="!OUT_RINGS!" (
    echo   [FAIL] Ring count changed: !IN_RINGS! -> !OUT_RINGS!
    del "%OUT_PATH%" tmp_test.txt tmp_in_area.txt tmp_out_area.txt tmp_area_ok.txt tmp_in_rings.txt tmp_out_rings.txt >nul 2>&1
    exit /b 0
)

set VERTEX_COUNT=0
for /f %%A in ('findstr /R "^[0-9][0-9]*,[0-9][0-9]*," tmp_test.txt ^| find /C ","') do set "VERTEX_COUNT=%%A"
if !VERTEX_COUNT! GTR %TARGET% (
    echo   [PASS^|WARN] Core checks passed, vertices above target: !VERTEX_COUNT! ^> %TARGET%
    set /a WARNED+=1
) else (
    echo   [PASS] Area preserved, metrics present, rings unchanged, vertices=!VERTEX_COUNT!
)

copy /Y tmp_test.txt "%OUT_PATH%" >nul

set /a PASSED+=1
del tmp_test.txt tmp_in_area.txt tmp_out_area.txt tmp_area_ok.txt tmp_in_rings.txt tmp_out_rings.txt >nul 2>&1
exit /b 0
