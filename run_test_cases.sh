#!/usr/bin/env bash
set -euo pipefail

SIMPLIFY_PATH="${SIMPLIFY_PATH:-}"
TEST_CASES_DIR="${TEST_CASES_DIR:-test_cases}"
KEEP_OUTPUTS=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --simplify)
      SIMPLIFY_PATH="${2:-}"
      shift 2
      ;;
    --test-cases-dir)
      TEST_CASES_DIR="${2:-}"
      shift 2
      ;;
    --keep-outputs)
      KEEP_OUTPUTS=1
      shift
      ;;
    *)
      echo "Unknown argument: $1" >&2
      exit 2
      ;;
  esac
done

resolve_simplify_path() {
  if [[ -n "$SIMPLIFY_PATH" ]]; then
    [[ -x "$SIMPLIFY_PATH" ]] || { echo "Simplify executable not found/executable: $SIMPLIFY_PATH" >&2; exit 1; }
    echo "$SIMPLIFY_PATH"
    return
  fi

  if [[ -x "./simplify" ]]; then
    echo "./simplify"
    return
  fi

  if [[ -x "./simplify.exe" ]]; then
    echo "./simplify.exe"
    return
  fi

  echo "Could not find simplify executable. Build first or pass --simplify <path>." >&2
  exit 1
}

get_metric_line() {
  local file="$1"
  local key="$2"
  grep -F "$key" "$file" | head -n 1 || true
}

declare -a CASES=(
  "input_rectangle_with_two_holes.csv:7"
  "input_cushion_with_hexagonal_hole.csv:13"
  "input_blob_with_two_holes.csv:17"
  "input_wavy_with_three_holes.csv:21"
  "input_lake_with_two_islands.csv:17"
  "input_original_01.csv:99"
  "input_original_02.csv:99"
  "input_original_03.csv:99"
  "input_original_04.csv:99"
  "input_original_05.csv:99"
  "input_original_06.csv:99"
  "input_original_07.csv:99"
  "input_original_08.csv:99"
  "input_original_09.csv:99"
  "input_original_10.csv:99"
)

SIMPLIFY_EXE="$(resolve_simplify_path)"
TMP_DIR="./tmp_test_outputs"
rm -rf "$TMP_DIR"
mkdir -p "$TMP_DIR"

total=0
exact_matches=0

printf "%-40s %-7s %-7s %-7s\n" "Case" "Target" "Exact" "Metrics"
printf "%-40s %-7s %-7s %-7s\n" "----" "------" "-----" "-------"

for case_entry in "${CASES[@]}"; do
  IFS=':' read -r input_file target <<< "$case_entry"
  input_path="$TEST_CASES_DIR/$input_file"
  expected_name="${input_file/input_/output_}"
  expected_name="${expected_name/.csv/.txt}"
  expected_path="$TEST_CASES_DIR/$expected_name"
  actual_path="$TMP_DIR/$expected_name"

  [[ -f "$input_path" ]] || { echo "Missing input file: $input_path" >&2; exit 1; }
  [[ -f "$expected_path" ]] || { echo "Missing expected output file: $expected_path" >&2; exit 1; }

  "$SIMPLIFY_EXE" "$input_path" "$target" > "$actual_path" 2>/dev/null

  exact="PASS"
  if ! diff -u "$expected_path" "$actual_path" >/dev/null; then
    exact="FAIL"
  fi

  metric_keys=(
    "Total signed area in input:"
    "Total signed area in output:"
    "Total areal displacement:"
  )
  metrics="PASS"
  for key in "${metric_keys[@]}"; do
    expected_metric="$(get_metric_line "$expected_path" "$key")"
    actual_metric="$(get_metric_line "$actual_path" "$key")"
    if [[ "$expected_metric" != "$actual_metric" ]]; then
      metrics="FAIL"
      break
    fi
  done

  printf "%-40s %-7s %-7s %-7s\n" "$input_file" "$target" "$exact" "$metrics"

  total=$((total + 1))
  if [[ "$exact" == "PASS" ]]; then
    exact_matches=$((exact_matches + 1))
  fi
done

echo
echo "Summary: ${exact_matches}/${total} exact matches"

if [[ "$KEEP_OUTPUTS" -eq 0 ]]; then
  rm -rf "$TMP_DIR"
else
  echo "Kept generated outputs at: $TMP_DIR"
fi

if [[ "$exact_matches" -ne "$total" ]]; then
  exit 1
fi
