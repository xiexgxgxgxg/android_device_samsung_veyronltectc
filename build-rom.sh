#!/bin/bash
# Build a complete, flashable W2017 image with every device fix included.
#
# The only post-build step that the soong graph does not do for us is the
# telephony-common dex: its hiddenapi step keeps the pre-patch dex, so the
# jar is patched after the first system image is built, then the image and
# the flashable zip are rebuilt from it.
set -e

TOP=${TOP:-/data/lineage-18.1}
PRODUCT=${PRODUCT:-veyronltectc}

"$TOP/device/samsung/veyronltectc/patches/apply-patches.sh"

cd "$TOP"
source "$TOP/build-veyron.sh" eng

# 1. build the system (telephony-common.jar is still the unpatched one here)
m systemimage

# 2. put the patched RIL.java dex into the installed jar
"$TOP/device/samsung/veyronltectc/patches/merge-telephony-dex.sh" "$PRODUCT"

# 3. repack the image with the patched jar, then build the flashable zip
m systemimage
m bacon

echo
echo "flashable package:"
ls -l "$TOP/out/target/product/$PRODUCT"/lineage-18.1-*-UNOFFICIAL-*.zip | tail -1
