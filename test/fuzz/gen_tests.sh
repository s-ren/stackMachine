#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "Usage: $0 <max_length> <n>"
    echo "  max_length  max number of instructions per generated file"
    echo "  n           number of files to generate"
    exit 1
}

[[ $# -ne 2 ]] && usage

MAX_LENGTH=$1
N=$2

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT_DIR="${SCRIPT_DIR}/corpus/gen_tests_${MAX_LENGTH}"

mkdir -p "${OUT_DIR}"

for i in $(seq 1 "${N}"); do
    # Random instruction count between 1 and max_length
    num_instructions=$(( (RANDOM % MAX_LENGTH) + 1 ))
    out_file="${OUT_DIR}/test_$(printf '%04d' "${i}").bin"
    python3 "${SCRIPT_DIR}/fuzzer.py" -n "${num_instructions}" -o "${out_file}"
done

echo "Generated ${N} files in ${OUT_DIR}"
