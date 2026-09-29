#!/usr/bin/env bash
set -e

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
QMK_DIR="${REPO_DIR}/../qmk_firmware"

export PATH="$HOME/.local/bin:$HOME/Library/Application Support/qmk/bin:$PATH"

echo "=== Syncing keymap and test files to QMK tree ==="
mkdir -p "${QMK_DIR}/keyboards/zsa/moonlander/keymaps/AWDDG"
cp -r "${REPO_DIR}/AWDDG/"* "${QMK_DIR}/keyboards/zsa/moonlander/keymaps/AWDDG/"

mkdir -p "${QMK_DIR}/tests/hrm"
cp -r "${REPO_DIR}/tests/hrm/"* "${QMK_DIR}/tests/hrm/"
# Copy hrm module files to test directory so test compiles standalone
cp "${REPO_DIR}/AWDDG/hrm.h" "${REPO_DIR}/AWDDG/hrm.c" "${QMK_DIR}/tests/hrm/"

echo "=== Running HRM Unit Tests ==="
cd "${QMK_DIR}"
make test:hrm "$@"
