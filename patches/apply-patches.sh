#!/bin/bash
# Apply the device-specific AOSP patches (0001-0007) to a fresh Lineage tree.
# Idempotent: a patch that is already applied is skipped.
set -e

TOP=${TOP:-/data/lineage-18.1}
P=$TOP/device/samsung/veyronltectc/patches

apply() {
    local repo=$1 patch=$2
    cd "$TOP/$repo"
    if git apply --reverse --check "$patch" >/dev/null 2>&1; then
        echo "already applied: $(basename "$patch")"
    else
        git apply "$patch"
        echo "applied: $(basename "$patch") ($repo)"
    fi
}

apply hardware/interfaces          "$P/0001-hal-services-run-as-root-on-3.18.patch"
apply system/netd                  "$P/0002-netd-run-as-root-on-3.18.patch"
apply hardware/interfaces          "$P/0003-audio-legacy-hal-metadata.patch"
apply hardware/interfaces          "$P/0004-fingerprint-32bit-service.patch"
apply system/libhidl               "$P/0005-libhidlbase-drop-getservice-sleep.patch"
apply external/wpa_supplicant_8    "$P/0006-hostapd-run-as-root-on-3.18.patch"
apply frameworks/opt/telephony     "$P/0007-telephony-probe-radio-hal-without-blocking.patch"
apply vendor/apn                  "$P/0008-apn-ctc-ims-on-modem-profile.patch"
apply hardware/interfaces          "$P/0009-audio-in-capture-position-abi.patch"
apply hardware/interfaces          "$P/0010-fingerprint-use-64bit-samsung-service.patch"
apply frameworks/base              "$P/0011-keyguard-null-fingerprint-message.patch"
apply frameworks/base              "$P/0012-autobrightness-responsive.patch"

echo "all patches applied"
apply frameworks/base               "$P/0013-global-actions-keep-power-menu-on.patch"
apply frameworks/base               "$P/0014-generic-kl-map-key-recent-to-app-switch.patch"
apply hardware/interfaces          "$P/0015-camera-provider-preload-metadata-shim.patch"
apply frameworks/av                "$P/0016-cameraserver-preload-metadata-shim.patch"
apply hardware/interfaces          "$P/0017-camera-provider-wait-for-daemon.patch"
