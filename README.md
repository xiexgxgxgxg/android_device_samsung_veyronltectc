# LineageOS 18.1 device tree — Samsung W2017 (SM-W2017 / veyronlte CTC)

> **STATUS: BOOTS ON HARDWARE — display and input work, most peripherals do not (2026-11-18).**
>
> The eng build boots to the LineageOS setup wizard on the phone: both panels
> light up with the correct orientation, the folder hall switches between
> them, touch works and adb is up over USB. Audio, Wi-Fi, Bluetooth, camera,
> RIL/baseband, fingerprint, GNSS, the T9 keypad mapping, the screen-on
> brightness and an occasional soft reboot are still open.
>
> Item-by-item tracking (symptom / root cause / evidence / status / next step)
> is in `W2017_LineageOS18.1_问题清单.md`; the bring-up log, the environment
> and the failure analysis are in `W2017_LineageOS18.1_交接文档.md`.
>
> Boot-critical pieces that came out of that analysis:
>
> * the stock 6.0.1 SDM blobs were dropped — their C++ ABI does not match the
>   Android 11 display HAL built from source here;
> * the kernel's `mdp/caps` file now reports the pipe list the A11 SDM parses
>   (it used to make SDM believe the MDP had zero pipes);
> * the a530 GPU firmware and the Oreo-era Adreno userspace driver (from a
>   Xiaomi Note 2 MIUI 11 image) are in the vendor tree;
> * `ro.surface_flinger.primary_display_orientation=ORIENTATION_180`
>   (both panels are mounted upside down);
> * `dalvik.vm.usejit=false` (ART's JIT cache flush crashes zygote on this
>   kernel).
>
> NOTE: as committed, `system_server` still blocks on
> `android.hardware.gnss@1.0::IGnss/default` because the VINTF manifest
> declares it and nothing provides it; adding
> `android.hardware.gnss@1.0-service` (the AOSP wrapper around the legacy
> `gps.default.so` that the blob set already ships) is the next change.

## Device

| | |
|---|---|
| Model | SM-W2017 (心系天下 W2017, China Telecom / CTC) |
| SoC | Qualcomm Snapdragon 820 (msm8996), Adreno 530 |
| RAM / storage | 4 GB / 64 GB |
| Stock OS | Android 6.0.1 (builds `W2017KEU1APJ7` … `W2017KEU1ARG1`) |
| Samsung codename | `veyronlte` (`veyronlte_ctc` defconfig) |
| Lineage device name | `veyronltectc` |
| Display | dual 4.2" 1080x1920 AMOLED, switched in-kernel |
| Input | T9 keypad + touch, folder hall sensor |

### Why this device is unusual

* **Two panels, one DSI.** The stock kernel selects the active panel with
  `samsung,mdss_dsi_lcd_sel_gpio = <&tlmm 1 0>` plus a 500 ms flip delay, driven
  by the folder hall IC (`folder_hall`, PM8994 GPIO 7). Display switching happens
  entirely in `drivers/video/msm/mdss/` — Android's display stack never sees it.
* **Physical keypad.** A stock keylayout/keychars dump is required for the keypad
  to work.
* **China-only firmware.** No Treble, no vendor partition, blobs are 6.0.1-era.

## Provenance and layout

* Structure: `ivanmeler/android_device_samsung_heroqlte` (`lineage-18.1`)
* The shared msm8996 configs are **inlined under `common/`** — this tree needs no
  external `device/samsung/msm8996-common` repo.
* Kernel: dedicated fork of the Samsung W2017 source
  (`android_kernel_samsung_veyronltectc`, `lineage-18.1`,
  `veyronlte_ctc_defconfig`, Linux 3.18.140). It must stay dedicated: the stock
  W2017 kernel is the only one that carries the dual-panel switch logic
  (`drivers/video/msm/mdss/`, `samsung,mdss_dsi_lcd_sel_gpio`,
  `folder_hall`), and no upstream msm8996 kernel has it.
* Vendor: `android_vendor_samsung_veyronltectc` (standalone, no common vendor repo).
* `hardware/samsung` is required: the common config sets
  `BOARD_CUSTOM_BOOTIMG_MK := hardware/samsung/mkbootimg.mk`, and the product
  pulls `macloader` and the Samsung RIL helpers from it.

## What works

* `lunch lineage_veyronltectc-eng && m` completes: `system.img` (1.7 GiB),
  `boot.img` (with the 3.18.140 kernel and the four veyron DTBs), `dt.img`,
  `recovery.img`.
* `m bacon` produces `lineage-18.1-*-UNOFFICIAL-veyronltectc.zip`, signed with
  the test keys.
* The tree matches the hardware that was read back from the phone in TWRP:
  partition sizes, 1080x1920 panels, keypad/touch input, BCM4358 Wi-Fi.

Nothing has been booted yet.

## TODO

- [x] Vendor blobs: extracted from `W2017KEU1ARG1` (870 files) into
      `android_vendor_samsung_veyronltectc`
- [x] Kernel: build `veyronlte_ctc_defconfig` out-of-tree for LineageOS
- [x] Kernel: Android 11 userspace options (USB configfs, binder
      hwbinder/vndbinder, quota, memcg, F2FS, overlayfs)
- [x] Verify partition layout / fstab against the real partition table
- [ ] Keypad: `keylayout` / `keychars` from stock
- [ ] Dual panel: verify Android behaves when the panel switches under it
- [ ] RIL: Samsung SecRIL 6.0.1 → radio@1.4 / secbridge (hardest part)
- [ ] Camera: 6.0.1 HAL1 blobs on Pie
- [ ] Audio, sensors, fingerprint, GPS
- [ ] SELinux policy (currently permissive)
- [ ] Wi-Fi firmware: `bcmdhd_*.bin` and `nvram_net.txt` are not in the blob set
      yet
- [x] `BUILD_FINGERPRINT` set from the stock ARG1 build.prop

## Debugging a boot loop

Two independent channels survive a reset, so a ROM that never reaches the
launcher can still be diagnosed:

* **Kernel log:** `CONFIG_VEYRON_SDLOG` appends the dmesg tail to a ring in a
  raw area at the end of the microSD card every 300 ms, from early boot.  The
  ring holds about 500 records (~150 s) and a superblock carries the position
  across power cycles, so several failed boots survive -- and a TWRP boot only
  overwrites the first slot, never the whole ring.  Insert a card before
  flashing.  To read it, power off, take the card out and run
  `/data/twrp/w2_audit/sdlog_decode.py /dev/sdX` on a PC; no phone boot, TWRP
  or download mode is involved.
* **Userspace log:** `persist.logd.logpersistd=logcatd` keeps logcat in
  `/data/misc/logd/`. From TWRP, mount `/data` and
  `adb pull /data/misc/logd/`.

The recovery partition still holds TWRP, so a bad `boot.img` is restored from
TWRP without the Odin reader image.

## Local manifest

```xml
<?xml version="1.0" encoding="UTF-8"?>
<manifest>
    <project name="xiexgxgxgxg/android_device_samsung_veyronltectc"
        path="device/samsung/veyronltectc"
        revision="lineage-18.1" />
    <project name="xiexgxgxgxg/android_vendor_samsung_veyronltectc"
        path="vendor/samsung/veyronltectc"
        revision="lineage-18.1" />
    <project name="xiexgxgxgxg/android_kernel_samsung_veyronltectc"
        path="kernel/samsung/msm8996"
        revision="lineage-18.1" />
    <project name="xiexgxgxgxg/android_hardware_samsung"
        path="hardware/samsung"
        revision="lineage-18.1" />
</manifest>
```

## Build

```bash
source build/envsetup.sh
lunch lineage_veyronltectc-eng
mka bacon
```

The kernel is built with the prebuilt GCC 4.9 toolchain from
`kernel/samsung/msm8996` and the 3.18 tree needs `HOSTCFLAGS=-fcommon`,
`CONFIG_NO_ERROR_ON_MISMATCH=y` and lld for the host tools; all three are set in
`common/BoardConfigCommon.mk`.

Artifacts (eng):

```
out/target/product/veyronltectc/system.img
out/target/product/veyronltectc/boot.img
out/target/product/veyronltectc/dt.img
out/target/product/veyronltectc/lineage-18.1-*-UNOFFICIAL-veyronltectc.zip
```
