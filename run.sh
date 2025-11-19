#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
BIN_PATH="${BUILD_DIR}/ui/neoguvc"

if [[ ! -x "${BIN_PATH}" ]]; then
  echo "Binário não encontrado em ${BIN_PATH}."
  echo "Execute: cmake --build build"
  exit 1
fi

LIB_PATHS=(
  "${BUILD_DIR}/gview_audio"
  "${BUILD_DIR}/gview_encoder"
  "${BUILD_DIR}/gview_render"
  "${BUILD_DIR}/gview_v4l2core"
)

export LD_LIBRARY_PATH="$(IFS=:; echo "${LIB_PATHS[*]}")${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"

exec "${BIN_PATH}" "$@"
