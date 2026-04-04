#!/usr/bin/env bash
set -u -o pipefail

: "${TEST_TIMEOUT:=600}"
: "${VALIDATION_TOL:=1e-2}"

OUTPUT_DIR="outputs"
TOTAL=0
PASSED=0
WARNED=0

rm -rf "$OUTPUT_DIR"
mkdir -p "$OUTPUT_DIR"

echo "========================================"
echo "Building simplify and validate_output..."
echo "========================================"
if ! make simplify validate_output; then
    echo "Build failed!"
    exit 1
fi
echo "Build successful!"
echo "Outputs will be stored in: ${OUTPUT_DIR}/"
echo "Validation tolerance: ${VALIDATION_TOL}"
echo

TEST_INPUTS=(
    "input_rectangle_with_two_holes.csv"
    "input_cushion_with_hexagonal_hole.csv"
    "input_blob_with_two_holes.csv"
    "input_wavy_with_three_holes.csv"
    "input_lake_with_two_islands.csv"
    "input_original_01.csv"
    "input_original_02.csv"
    "input_original_03.csv"
    "input_original_04.csv"
    "input_original_05.csv"
    "input_original_06.csv"
    "input_original_07.csv"
    "input_original_08.csv"
    "input_original_09.csv"
    "input_original_10.csv"
)

TEST_TARGETS=(7 13 17 21 17 99 99 99 99 99 99 99 99 99 99)

for idx in "${!TEST_INPUTS[@]}"; do
    input="${TEST_INPUTS[$idx]}"
    target="${TEST_TARGETS[$idx]}"
    out_name="${input/input_/output_}"
    out_name="${out_name%.csv}.txt"
    out_path="${OUTPUT_DIR}/${out_name}"

    TOTAL=$((TOTAL + 1))

    timeout "${TEST_TIMEOUT}s" ./simplify "test_cases/${input}" "${target}" >"${out_path}" 2>/dev/null
    run_status=$?
    if [ "${run_status}" -ne 0 ]; then
        if [ "${run_status}" -eq 124 ]; then
            echo "[FAIL] ${input} - simplify timed out after ${TEST_TIMEOUT}s"
        else
            echo "[FAIL] ${input} - simplify execution failed"
        fi
        continue
    fi

    summary="$(./validate_output --input "test_cases/${input}" --output "${out_path}" --target "${target}" --tol "${VALIDATION_TOL}" 2>&1)"
    validate_status=$?

    if [ "${validate_status}" -eq 0 ]; then
        echo "[PASS] ${input} - ${summary}"
        PASSED=$((PASSED + 1))
    elif [ "${validate_status}" -eq 2 ]; then
        echo "[PASS|WARN] ${input} - ${summary}"
        PASSED=$((PASSED + 1))
        WARNED=$((WARNED + 1))
    else
        echo "[FAIL] ${input} - ${summary}"
    fi
done

echo
echo "========================================"
echo "Summary: ${PASSED}/${TOTAL} tests passed validation, ${WARNED} warnings"
echo "========================================"

if [ "${PASSED}" -ne "${TOTAL}" ]; then
    exit 1
fi
