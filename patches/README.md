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
