#!/bin/bash -eu

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC="${SRC:-$(cd "${SCRIPT_DIR}/.." && pwd)}"
OUT="${OUT:-${SCRIPT_DIR}/out}"
mkdir -p "${OUT}"

SRC_FILES=()
for cpp in "${SRC}"/src/*.cpp; do
  base="$(basename "${cpp}")"
  if [[ "${base}" == "main.cpp" ]]; then
    continue
  fi
  SRC_FILES+=("${cpp}")
done
SRC_FILES+=("${SRC}/fuzz/replay_fuzzer.cc")

"${CXX}" ${CXXFLAGS} "${SRC_FILES[@]}" -I"${SRC}/include" \
  ${LIB_FUZZING_ENGINE:+"${LIB_FUZZING_ENGINE}"} -o "${OUT}/replay_fuzzer"
