#!/usr/bin/env python3
"""Rewrite android::CameraMetadata references in the 6.0.1 camera HAL blob.

Why
---
camera.msm8996.so (Samsung/Qualcomm 6.0.1) is linked against
libcamera_client.so for android::CameraMetadata.  That class grew a
`mutable bool mLocked` member in Android 8 (for getAndLock()/unlock());
on Android 11 the out-of-line methods live in libcamera_client.so and
read/write mLocked at offset 4 of an object that the 6.0.1 HAL allocated
with the old layout (mBuffer only, 4 bytes).  At runtime the stray byte
reads as "locked", so every update() fails and release() returns NULL;
the A11 camera provider then dies in CameraModule::getCameraInfo() ->
CameraMetadata::append(NULL).

What
----
The mangled names are rewritten from `14CameraMetadata` to
`14LegacyMetadata`, which keeps the string length identical, so the
.dynstr section can be patched in place without moving anything.  The
HAL then imports android::LegacyMetadata, which libcammetadata_legacy.so
implements with the 6.0.1 layout (see camshim/camera_metadata_legacy.cpp).
Because the class name is unique, preloading that library cannot disturb
the real android::CameraMetadata used by cameraserver or the provider.

Usage
-----
    tools/patch_camera_metadata_abi.py <camera.msm8996.so> [--check]

The vendor tree keeps the patched blob; re-run this script whenever the
blob is re-imported from the stock image.
"""

import argparse
import hashlib
import os
import sys

# Only the *imported* android::CameraMetadata symbols are renamed:
#   _ZN7android14CameraMetadata...   (non-const member functions)
#   _ZNK7android14CameraMetadata...  (const member functions)
# Those are undefined symbols and they all live before the .gnu.hash symndx
# (undefined symbols are not part of the GNU hash table), so rewriting their
# names cannot invalidate the blob's hash table.
#
# Symbols *defined* by the blob that merely mention the class in their
# signature (QCamera3HardwareInterface::translateFromHalMetadata(...
# const android::CameraMetadata& ...) and friends) must keep their names:
# they live in the hashed range and are resolved through the hash table.
PREFIXES = (
    (b"_ZN7android14CameraMetadata", b"_ZN7android14LegacyMetadata"),
    (b"_ZNK7android14CameraMetadata", b"_ZNK7android14LegacyMetadata"),
)


def scan(data):
    offsets = []
    for old, _new in PREFIXES:
        start = 0
        while True:
            idx = data.find(old, start)
            if idx < 0:
                break
            offsets.append((idx, old))
            start = idx + 1
    return sorted(offsets)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("blob")
    ap.add_argument("--check", action="store_true",
                    help="only verify the patch state, do not write")
    args = ap.parse_args()

    with open(args.blob, "rb") as f:
        data = f.read()

    found = scan(data)
    if not found:
        print("%s: no imported android::CameraMetadata symbols "
              "(already patched?)" % args.blob)
        return 0 if args.check else 1

    print("%s: %d imported android::CameraMetadata symbols"
          % (args.blob, len(found)))
    if args.check:
        print("NOT patched")
        return 1

    patched = bytearray(data)
    for off, old in found:
        new = dict(PREFIXES)[old]
        patched[off:off + len(old)] = new
    patched = bytes(patched)
    assert len(patched) == len(data)

    tmp = args.blob + ".tmp"
    with open(tmp, "wb") as f:
        f.write(patched)
    os.chmod(tmp, os.stat(args.blob).st_mode & 0o7777)
    os.replace(tmp, args.blob)

    print("patched -> android::LegacyMetadata")
    print("sha1 %s" % hashlib.sha1(patched).hexdigest())
    return 0


if __name__ == "__main__":
    sys.exit(main())
