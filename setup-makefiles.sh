#!/bin/bash
#
# Regenerate vendor/samsung/veyronltectc makefiles from proprietary-files.txt.
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

setup_vendor "veyronltectc" "samsung" "${ANDROID_ROOT}" false
write_headers "veyronltectc"
write_makefiles "${MY_DIR}/proprietary-files.txt" true
write_footers
