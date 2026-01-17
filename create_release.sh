#!/bin/bash

COMPILER_NAME=""
VERSION=""
while [[ "$#" -gt 0 ]]; do
    case "$1" in
        --compiler-name)
            COMPILER_NAME="$2"
            shift # past argument
            shift # past value
            ;;
        --version)
            VERSION="$2"
            shift # past argument
            shift # past value
            ;;
        *)
            echo "Unknown parameter passed: $1"
            exit 1
            ;;
    esac
done

if [ -z "$COMPILER_NAME" ]; then
    echo "Error: --compiler-name argument is required."
    exit 1
fi

if [ -z "$VERSION" ]; then
    echo "Error: --version argument is required."
    exit 1
fi

SECONDS=0
INSTALL_PREFIX="/mnt/d/local/LumexCore/${VERSION}"
CMAKE_ARGS="-DLUMEX_BUILD_XML=ON"

python compile.py Release --arch x86 --shared-libs --clean --install-prefix ${INSTALL_PREFIX} --cmake-args="${CMAKE_ARGS}" --std 11
python compile.py Release --arch x86 --shared-libs --clean --install-prefix ${INSTALL_PREFIX} --cmake-args="${CMAKE_ARGS}" --std 14
python compile.py Release --arch x86 --shared-libs --clean --install-prefix ${INSTALL_PREFIX} --cmake-args="${CMAKE_ARGS}" --std 17
python compile.py Release --arch x86 --shared-libs --clean --install-prefix ${INSTALL_PREFIX} --cmake-args="${CMAKE_ARGS}" --std 20

python compile.py Release --arch x64 --shared-libs --clean --install-prefix ${INSTALL_PREFIX} --cmake-args="${CMAKE_ARGS}" --std 11
python compile.py Release --arch x64 --shared-libs --clean --install-prefix ${INSTALL_PREFIX} --cmake-args="${CMAKE_ARGS}" --std 14
python compile.py Release --arch x64 --shared-libs --clean --install-prefix ${INSTALL_PREFIX} --cmake-args="${CMAKE_ARGS}" --std 17
python compile.py Release --arch x64 --shared-libs --clean --install-prefix ${INSTALL_PREFIX} --cmake-args="${CMAKE_ARGS}" --std 20

rename_unknown() {
    mv ${INSTALL_PREFIX}/${VERSION}_linux_x86_unknown_cpp11 ${INSTALL_PREFIX}/${VERSION}_linux_x86_${COMPILER_NAME}_cpp11
    mv ${INSTALL_PREFIX}/${VERSION}_linux_x86_unknown_cpp14 ${INSTALL_PREFIX}/${VERSION}_linux_x86_${COMPILER_NAME}_cpp14
    mv ${INSTALL_PREFIX}/${VERSION}_linux_x86_unknown_cpp17 ${INSTALL_PREFIX}/${VERSION}_linux_x86_${COMPILER_NAME}_cpp17
    mv ${INSTALL_PREFIX}/${VERSION}_linux_x86_unknown_cpp20 ${INSTALL_PREFIX}/${VERSION}_linux_x86_${COMPILER_NAME}_cpp20

    mv ${INSTALL_PREFIX}/${VERSION}_linux_x64_unknown_cpp11 ${INSTALL_PREFIX}/${VERSION}_linux_x64_${COMPILER_NAME}_cpp11
    mv ${INSTALL_PREFIX}/${VERSION}_linux_x64_unknown_cpp14 ${INSTALL_PREFIX}/${VERSION}_linux_x64_${COMPILER_NAME}_cpp14
    mv ${INSTALL_PREFIX}/${VERSION}_linux_x64_unknown_cpp17 ${INSTALL_PREFIX}/${VERSION}_linux_x64_${COMPILER_NAME}_cpp17
    mv ${INSTALL_PREFIX}/${VERSION}_linux_x64_unknown_cpp20 ${INSTALL_PREFIX}/${VERSION}_linux_x64_${COMPILER_NAME}_cpp20
}

# Pack results robustly (handle unknown/gcc/clang folder marker)
pack_arch() {
  local arch="$1"  # x86 | x64
  local out="${INSTALL_PREFIX}/${VERSION}_linux_${arch}_${COMPILER_NAME}.tar.gz"

  # resolve real dirs (unknown/gcc/clang), fail if not exactly one match
  local d11=(${INSTALL_PREFIX}/${VERSION}_linux_${arch}_*_cpp11)
  local d14=(${INSTALL_PREFIX}/${VERSION}_linux_${arch}_*_cpp14)
  local d17=(${INSTALL_PREFIX}/${VERSION}_linux_${arch}_*_cpp17)
  local d20=(${INSTALL_PREFIX}/${VERSION}_linux_${arch}_*_cpp20)

  for d in "${d11[@]}" "${d14[@]}" "${d17[@]}" "${d20[@]}"; do
    if [[ -z "$d" ]]; then
      echo "Error: expected folder for ${arch}, one of cpp11/14/17/20 not found."
      exit 1
    fi
  done

  # make paths relative to INSTALL_PREFIX for clean tar entries
  local r11="${d11#${INSTALL_PREFIX}/}"
  local r14="${d14#${INSTALL_PREFIX}/}"
  local r17="${d17#${INSTALL_PREFIX}/}"
  local r20="${d20#${INSTALL_PREFIX}/}"

  tar -C "${INSTALL_PREFIX}" -czvf "${out}" "${r11}" "${r14}" "${r17}" "${r20}"
}

rename_unknown

pack_arch x86
pack_arch x64

# print elapsed
h=$((SECONDS/3600)); m=$(((SECONDS%3600)/60)); s=$((SECONDS%60))
printf "Total build time: %02dh %02dm %02ds\n" "$h" "$m" "$s"
