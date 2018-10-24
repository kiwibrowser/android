#!/bin/bash
#
# Copyright (c) 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script is used to generate .gni files and files in the
# config/platform directories needed to build libaom.
#
# Every time the upstream source code is updated this script must be run.
#
# Usage:
# $ ./cmake_update.sh
# Requirements:
# - cmake3
# - yasm or nasm
# Toolchain for armv7:
#  -gcc-arm-linux-gnueabihf
#  -g++-arm-linux-gnueabihf
# Toolchain for arm64:
#  -gcc-aarch64-linux-gnu
#  -g++-aarch64-linux-gnu
# 32bit build environment for cmake. Including but potentially not limited to:
#  -lib32gcc-7-dev
#  -lib32stdc++-7-dev
# Alternatively: treat 32bit builds like Windows and manually tweak aom_config.h

set -e

# sort() consistently.
export LC_ALL=C

BASE=$(pwd)
SRC="${BASE}/source/libaom"
CFG="${BASE}/source/config"

function clean {
  rm -rf "${TMP}"
}

# Create empty temp and config directories.
# $1 - Header file directory.
function reset_dirs {
  cd ..
  rm -rf "${TMP}"
  mkdir "${TMP}"
  cd "${TMP}"

  echo "Generate ${1} config files."
  rm -fr "${CFG}/${1}"
  mkdir -p "${CFG}/${1}/config"
}

if [ $# -ne 0 ]; then
  echo "Unknown option(s): ${@}"
  exit 1
fi

# Missing function:
# find_duplicates
# We may have enough targets to avoid re-implementing this.

# Generate libaom.config file for rtcd.pl.
# $1 - platform/arch
function print_config {
  combined_config="$(cat ${CFG}/${1}/config/aom_config.h | grep -E ' +[01] *$')"
  combined_config="$(echo "$combined_config" | grep -v DO1STROUNDING)"
  combined_config="$(echo "$combined_config" | sed 's/[ \t]//g')"
  combined_config="$(echo "$combined_config" | sed 's/.*define//')"
  combined_config="$(echo "$combined_config" | sed 's/0$/=no/')"
  combined_config="$(echo "$combined_config" | sed 's/1$/=yes/')"
  echo "$combined_config" | sort | uniq
}

# Generate *_rtcd.h files.
# $1 - Header file directory.
# $2 - Architecture.
# $3 - Optional - additional arguments to pass through to rtcd.pl.
function gen_rtcd_header {
  echo "Generate ${1} RTCD files."

  print_config ${1} > "libaom.config"

  ${SRC}/build/make/rtcd.pl \
    --arch=${2} \
    --sym=av1_rtcd ${3} \
    --config=libaom.config \
    "${SRC}/av1/common/av1_rtcd_defs.pl" \
    > "${CFG}/${1}/config/av1_rtcd.h"

  clang-format -i "${CFG}/${1}/config/av1_rtcd.h"

  ${SRC}/build/make/rtcd.pl \
    --arch=${2} \
    --sym=aom_scale_rtcd ${3} \
    --config=libaom.config \
    "${SRC}/aom_scale/aom_scale_rtcd.pl" \
    > "${CFG}/${1}/config/aom_scale_rtcd.h"

  clang-format -i "${CFG}/${1}/config/aom_scale_rtcd.h"

  ${SRC}/build/make/rtcd.pl \
    --arch=${2} \
    --sym=aom_dsp_rtcd ${3} \
    --config=libaom.config \
    "${SRC}/aom_dsp/aom_dsp_rtcd_defs.pl" \
    > "${CFG}/${1}/config/aom_dsp_rtcd.h"

  clang-format -i "${CFG}/${1}/config/aom_dsp_rtcd.h"
}

# Generate Config files.
# $1 - Header file directory.
# $2 - cmake options.
function gen_config_files {
  cmake "${SRC}" ${2} &> /dev/null

  case "${1}" in
    *x64*|*ia32*)
      egrep "#define [A-Z0-9_]+ [01]" config/aom_config.h | \
        awk '{print "%define " $2 " " $3}' > config/aom_config.asm
      ;;
  esac

  cp config/aom_config.{h,c,asm} "${CFG}/${1}/config/"
}

function update_readme {
  local IFS=$'\n'
  # Split git log output '<date>\n<commit hash>' on the newline to produce 2
  # array entries.
  local vals=($(git --no-pager log -1 --format="%cd%n%H" \
    --date=format:"%A %B %d %Y"))
  sed -E -i.bak \
    -e "s/^(Date:)[[:space:]]+.*$/\1 ${vals[0]}/" \
    -e "s/^(Commit:)[[:space:]]+[a-f0-9]{40}/\1 ${vals[1]}/" \
    ${BASE}/README.chromium
  rm ${BASE}/README.chromium.bak
  cat <<EOF

README.chromium updated with:
Date: ${vals[0]}
Commit: ${vals[1]}
EOF
}

TMP=$(mktemp -d "${BASE}/build.XXXX")
cd "${TMP}"

all_platforms="-DCONFIG_SIZE_LIMIT=1"
all_platforms+=" -DDECODE_HEIGHT_LIMIT=16384 -DDECODE_WIDTH_LIMIT=16384"
all_platforms+=" -DCONFIG_AV1_ENCODER=0"
all_platforms+=" -DCONFIG_LOWBITDEPTH=1"
# avx2 optimizations account for ~0.3mb of the decoder.
#all_platforms+=" -DENABLE_AVX2=0"
toolchain="-DCMAKE_TOOLCHAIN_FILE=${SRC}/build/cmake/toolchains"

reset_dirs linux/generic
gen_config_files linux/generic "-DAOM_TARGET_CPU=generic ${all_platforms}"
# libaom_srcs.gni and aom_version.h are shared.
cp libaom_srcs.gni "${BASE}"
cp config/aom_version.h "${CFG}/config/"
gen_rtcd_header linux/generic generic

reset_dirs linux/ia32
gen_config_files linux/ia32 "${toolchain}/x86-linux.cmake ${all_platforms}"
gen_rtcd_header linux/ia32 x86 #--disable-avx2

reset_dirs linux/x64
gen_config_files linux/x64 "${all_platforms}"
gen_rtcd_header linux/x64 x86_64 #--disable-avx2

# Windows looks like linux but with some minor tweaks. Cmake doesn't generate VS
# project files on linux otherwise we would not resort to these hacks.

reset_dirs win/ia32
cp "${CFG}/linux/ia32/config"/* "${CFG}/win/ia32/config/"
sed -i.bak \
  -e 's/\(#define[[:space:]]INLINE[[:space:]]*\)inline/#define INLINE __inline/' \
  -e 's/\(#define[[:space:]]HAVE_PTHREAD_H[[:space:]]*\)1/#define HAVE_PTHREAD_H 0/' \
  -e 's/\(#define[[:space:]]HAVE_UNISTD_H[[:space:]]*\)1/#define HAVE_UNISTD_H 0/' \
  -e 's/\(#define[[:space:]]CONFIG_GCC[[:space:]]*\)1/#define CONFIG_GCC 0/' \
  -e 's/\(#define[[:space:]]CONFIG_MSVS[[:space:]]*\)0/#define CONFIG_MSVS 1/' \
  "${CFG}/win/ia32/config/aom_config.h"
rm "${CFG}/win/ia32/config/aom_config.h.bak"
egrep "#define [A-Z0-9_]+ [01]" "${CFG}/win/ia32/config/aom_config.h" \
  | awk '{print "%define " $2 " " $3}' > "${CFG}/win/ia32/config/aom_config.asm"

reset_dirs win/x64
cp "${CFG}/linux/x64/config"/* "${CFG}/win/x64/config/"
sed -i.bak \
  -e 's/\(#define[[:space:]]INLINE[[:space:]]*\)inline/#define INLINE __inline/' \
  -e 's/\(#define[[:space:]]HAVE_PTHREAD_H[[:space:]]*\)1/#define HAVE_PTHREAD_H 0/' \
  -e 's/\(#define[[:space:]]HAVE_UNISTD_H[[:space:]]*\)1/#define HAVE_UNISTD_H 0/' \
  -e 's/\(#define[[:space:]]CONFIG_GCC[[:space:]]*\)1/#define CONFIG_GCC 0/' \
  -e 's/\(#define[[:space:]]CONFIG_MSVS[[:space:]]*\)0/#define CONFIG_MSVS 1/' \
  "${CFG}/win/x64/config/aom_config.h"
rm "${CFG}/win/x64/config/aom_config.h.bak"
egrep "#define [A-Z0-9_]+ [01]" "${CFG}/win/x64/config/aom_config.h" \
  | awk '{print "%define " $2 " " $3}' > "${CFG}/win/x64/config/aom_config.asm"

reset_dirs linux/arm
gen_config_files linux/arm \
  "${toolchain}/armv7-linux-gcc.cmake -DENABLE_NEON=0 ${all_platforms}"
gen_rtcd_header linux/arm armv7 --disable-neon

reset_dirs linux/arm-neon
gen_config_files linux/arm-neon "${toolchain}/armv7-linux-gcc.cmake ${all_platforms}"
gen_rtcd_header linux/arm-neon armv7

reset_dirs linux/arm-neon-cpu-detect
gen_config_files linux/arm-neon-cpu-detect \
  "${toolchain}/armv7-linux-gcc.cmake -DCONFIG_RUNTIME_CPU_DETECT=1 ${all_platforms}"
gen_rtcd_header linux/arm-neon-cpu-detect armv7

reset_dirs linux/arm64
gen_config_files linux/arm64 "${toolchain}/arm64-linux-gcc.cmake ${all_platforms}"
gen_rtcd_header linux/arm64 arm64

cd "${SRC}"
update_readme

clean
