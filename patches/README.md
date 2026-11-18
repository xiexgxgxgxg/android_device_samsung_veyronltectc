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
