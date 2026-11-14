#!/bin/bash
#
# Pull the blobs listed in proprietary-files.txt into vendor/samsung/veyronltectc.
# Usage: ./extract-files.sh [adb|/path/to/system_dump]
#
set -e

MY_DIR="${BASH_SOURCE%/*}"
ANDROID_ROOT="${MY_DIR}/../../.."
HELPER="${ANDROID_ROOT}/tools/extract-utils/extract_utils.sh"

if [ ! -f "${HELPER}" ]; then
    echo "Unable to find ${HELPER} — run this from a LineageOS 18.1 tree"
    exit 1
fi
source "${HELPER}"

CLEAN_VENDOR=true
SECTION=
SRC=

while [ "${#}" -gt 0 ]; do
    case "${1}" in
        -n | --no-cleanup ) CLEAN_VENDOR=false ;;
        -s | --section ) SECTION="${2}"; shift ;;
        * ) SRC="${1}" ;;
    esac
    shift
done
[ -z "${SRC}" ] && SRC="adb"

setup_vendor "veyronltectc" "samsung" "${ANDROID_ROOT}" false

if [ "${CLEAN_VENDOR}" = true ]; then
    rm -rf "${ANDROID_ROOT}/vendor/samsung/veyronltectc/proprietary"
fi

extract "${MY_DIR}/proprietary-files.txt" "${SRC}"
