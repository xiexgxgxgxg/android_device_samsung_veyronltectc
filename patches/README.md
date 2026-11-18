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

Run them as root instead (uid 0 keeps its capabilities across exec on 3.18).
Verified: both services show up as `root` in `ps`; the Bluetooth power write
and the WiFi "set up/down" socket no longer fail with EPERM/EACCES.

Apply:
    (cd hardware/interfaces && git apply ../../device/samsung/veyronltectc/patches/0001-hal-services-run-as-root-on-3.18.patch)
