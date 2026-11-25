#!/bin/bash
# Inject the patched RIL.java dex into the installed telephony-common.jar.
#
# The soong hiddenapi step for this boot-classpath jar keeps using the
# pre-patch dex in this tree, so `m telephony-common` produces a jar whose
# classes.dex still probes the missing radio@1.2 HAL with retry=true and
# com.android.phone hangs.  The javac/dex output under
# out/soong/.intermediates/.../dex/telephony-common.jar *does* contain the
# patch (patches/0007).  Run this after building the ROM, then push the jar
# (or include it in the image) before booting.
#
# Usage: patches/merge-telephony-dex.sh [product]
set -e

PRODUCT=${1:-veyronltectc}
TOP=${TOP:-/data/lineage-18.1}
JAR=$TOP/out/target/product/$PRODUCT/system/framework/telephony-common.jar
DEXJAR=$TOP/out/soong/.intermediates/frameworks/opt/telephony/telephony-common/android_common/dex/telephony-common.jar

[ -f "$JAR" ] || { echo "missing $JAR"; exit 1; }
[ -f "$DEXJAR" ] || { echo "missing $DEXJAR"; exit 1; }

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
unzip -o -q "$DEXJAR" classes.dex -d "$tmp"
cp "$JAR" "$tmp/telephony-common.jar"
( cd "$tmp" && zip -X -0 -q telephony-common.jar classes.dex )
cp "$tmp/telephony-common.jar" "$JAR"

echo "patched: $JAR"
unzip -p "$JAR" classes.dex | md5sum
echo "push with: adb push $JAR /system/framework/telephony-common.jar"
