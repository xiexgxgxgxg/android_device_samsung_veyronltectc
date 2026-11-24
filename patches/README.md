# Local patches for projects outside the device/vendor/kernel repos

LineageOS checkouts are read-only for us; anything that has to change inside
an AOSP project is kept here as a patch and applied manually before a build.

## 0001-hal-services-run-as-root-on-3.18.patch

`hardware/interfaces/{wifi,bluetooth}`: the WiFi and Bluetooth HAL services
declare `capabilities NET_ADMIN ...`, but this 3.18 kernel has no ambient
capability support, so init cannot hand those capabilities across the exec
(it logs "does not support ambient capabilities" and continues).  The
services then run without CAP_NET_ADMIN and fail on the first privileged
operation:

    E bt_upio : set_bluetooth_power : write(/sys/class/rfkill/rfkill0/state)
                failed: Operation not permitted
    E android.hardware.wifi@1.0-service: Failed to open socket to set up/down
                state (Permission denied)

Run them as root **and drop the `capabilities` line**: on this kernel init's
capability handling ends up clearing the set (`CapEff` was 0 even though the
process was uid 0), which made the failures worse.  With `user root` and no
`capabilities` line the services keep the full set:

    # before                       # after
    CapEff: 0000000000000000        CapEff: 0000003fffffffff

Verified: WiFi reaches ConnectModeState with wlan0 UP (MAC a0:cc:2b:19:65:a1)
and Bluetooth reaches state ON with address FC:42:03:F1:33:08.

Apply:
    (cd hardware/interfaces && git apply ../../device/samsung/veyronltectc/patches/0001-hal-services-run-as-root-on-3.18.patch)

## 0002-netd-run-as-root-on-3.18.patch

`system/netd`: same 3.18 ambient-capability problem, but netd is the one that
actually configures the WiFi interface.  Its rc declared

    capabilities CHOWN DAC_OVERRIDE ... NET_ADMIN NET_RAW ...

so init ended up with `CapEff: 0000000000000000` / `CapBnd: 0` even though
netd runs as uid 0.  The visible failure is:

    E WifiNative: Unable to get interface config
    E WifiNative: java.lang.IllegalStateException:
        android.os.ServiceSpecificException: [Permission denied] : socket() failed (code 13)
    E IpClient.wlan0: ERROR Setting IPv4 address to 10.x.x.x/24 failed:
        android.os.ServiceSpecificException: [Permission denied] : socket() failed (code 13)

Association, the 4-way handshake and DHCP all succeed; only the netd netlink
operations fail, so the connection is torn down immediately.  Fix is the same
as 0001: run as root and remove the `capabilities` line.

Apply:
    (cd system/netd && git apply ../../device/samsung/veyronltectc/patches/0002-netd-run-as-root-on-3.18.patch)

## 0003-audio-legacy-hal-metadata.patch

`hardware/interfaces/audio/core/all-versions/default`: after the W2017 stock
`mixer_paths.xml` was restored, the 6.0.1 `audio.primary.msm8996.so` opened
successfully, but `android.hardware.audio.service` immediately crashed:

    Fatal signal 11 (SIGSEGV), fault addr 0xbb80
    #01 ... StreamOut::updateSourceMetadata(...)+222
    #02 ... Device::openOutputStream(...)
    #03 ... PrimaryDevice::openOutputStream(...)

`update_source_metadata` (and `update_sink_metadata`) were added to
`audio_stream_out` / `audio_stream_in` in AUDIO_DEVICE_API_VERSION_3_0
(Android 8.0).  The 6.0.1 HAL's structs end before those fields, so even the
`mStream->update_source_metadata == nullptr` check reads past the struct and
sometimes calls a garbage pointer (0xbb80 = 48000, a sample rate value).

Guard both calls with `mDevice->version() >= AUDIO_DEVICE_API_VERSION_3_0`
before touching the metadata fields.  Old HALs simply skip metadata, which is
the correct behaviour for a pre-8.0 HAL.

Apply:
    (cd hardware/interfaces && git apply ../../device/samsung/veyronltectc/patches/0003-audio-legacy-hal-metadata.patch)

## 0004-fingerprint-32bit-service.patch

`hardware/interfaces/biometrics/fingerprint/2.1/default`: the W2017's real
fingerprint HAL is the stock 32-bit Egis `fingerprint.default.so`; the
64-bit HAL in the vendor tree is a demo that returns
`FINGERPRINT_ERROR_UNABLE_TO_PROCESS` from enumerate.  Build the AOSP 2.1
wrapper 32-bit (`compile_multilib: "32"`) and accept the stock HAL's
fingerprint API 2.0 (512) in addition to 2.1 (513), otherwise `openHal`
logs:

    E/...fingerprint@2.1-service: Wrong fp version. Expected 513, got 512
    E/...fingerprint@2.1-service: Can't open HAL module

The 32-bit deps (`libbauthserver`, `libbauthtzcommon`,
`libegis_fp_normal_sensor_test`, `libsynaFpSensorTestNwd`, `libMcRegistry`,
`libstdc++`) must be installed from the stock AP into `/vendor/lib`.

As of this writing the service registers the HIDL interface and reaches the
HAL, but the Samsung bauth layer still fails:

    E/bauth_FPDLBAuthSensorControl: BAuthDeviceOpen sys call failed rv : 209
    E/bauth_FPBAuthService: Sensor Device Open Fail
    E/bauth_TLC_Communicator: Trustlet Session is not opened

so the kernel bauth/TEE driver/trustlet is the remaining blocker; the
service/version part is ready.

Apply:
    (cd hardware/interfaces && git apply ../../device/samsung/veyronltectc/patches/0004-fingerprint-32bit-service.patch)

## 0005-libhidlbase-drop-getservice-sleep.patch

`system/libhidl`: `getRawServiceInternal()` does

    if (!kEnforceVintfManifest) {
        ALOGE("getService: Potential race detected. ...");
        sleep(1);
    }

before it even asks hwservicemanager, so **every** HIDL `getService()` on a
device that does not enforce its VINTF manifest costs a full second.  This
port cannot enforce it: it runs on stock 6.0.1 vendor blobs and 51 of the 78
registered HAL interfaces are not declared (wifi, supplicant, drm/clearkey,
codec2 / `media.swcodec`, display config/color, cas, health, ...).  Turning
enforcement on would make all of them unreachable.

The sleep is supposed to cover the race where a HAL has not registered yet,
but the retry loop right below already waits for that notification
(`Waiter`), so it only costs time.  Drop the sleep, keep the warning once
(`std::call_once`).

This is the root cause of the "delayed playback" that was reported for the
ringtone preview.  A single preview resolves 5 HALs:

    NuPlayerDriver created -> GenericSource: FileSource   2.02 s (2 HALs)
    Ringtone: Successfully created local player          +0.02 s
    OMXClient: IOmx service obtained                      1.00 s
    codec setup (IOmxStore + codec2)                      +2.4 s
    ------------------------------------------------------------------
    tap -> audible preview                                ~5.4 s

After the patch the same path measures 26 ms from "NuPlayerDriver created"
to "Successfully created local player" and 12 ms to "IOmx obtained", i.e.
the preview starts in well under 150 ms.  It also removes the same penalty
from the display, power and radio HAL lookups (the radio HAL is declared but
never registers, so `getService` used to block 1 s per poll).

Apply:
    (cd system/libhidl && patch -p1 < ../../device/samsung/veyronltectc/patches/0005-libhidlbase-drop-getservice-sleep.patch)

The patch is kept relative to the `system/libhidl` project root
(`a/transport/ServiceManagement.cpp`), because that is the repository that
owns the file.

## 0006-hostapd-run-as-root-on-3.18.patch

`external/wpa_supplicant_8`: the same 3.18 ambient-capability problem as
0001/0002, but for the SoftAP.  `hostapd.android.rc` declared

    capabilities NET_ADMIN NET_RAW
    user wifi

so hostapd ran as uid wifi with the capability set cleared and could not open
its nl80211 socket:

    E/hostapd: nl80211: socket(PF_INET,SOCK_DGRAM) failed: Permission denied
    E/hostapd: Failed to initialize driver 'nl80211'
    E/hostapd: Enabling interface wlan0 failed
    E/HostapdHal: IHostapd.addAccessPoint failed: 1
    E/SoftApManager: Soft AP start failed        -> the hotspot switches off

Run it as root and drop the `capabilities` line; hostapd itself calls
`capset()` and drops to uid wifi afterwards (`src/utils/os_unix.c`), so the
privileges are only held while it opens the sockets.

A second, device-side half of the same bug is fixed in
`common/rootdir/etc/init.qcom.rc`: the stock `hostapd_fst` service used to
claim `android.hardware.wifi.hostapd@1.0::IHostapd/default` before this rc was
parsed, so the AOSP hostapd service was never started at all.

Verified: `cmd wifi start-softap ...` -> `Soft AP started successfully`,
`dumpsys wifi` -> `SUCCESS: 1`, `numSetupSoftApInterfaceFailureDueToHostapd=0`,
`iw dev` -> `wlan0 type AP`.

Apply:
    (cd external/wpa_supplicant_8 && patch -p1 < ../../device/samsung/veyronltectc/patches/0006-hostapd-run-as-root-on-3.18.patch)
