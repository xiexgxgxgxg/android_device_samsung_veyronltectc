# LineageOS 18.1 device tree — Samsung W2017 (SM-W2017 / veyronlte CTC)

> **STATUS: WORK IN PROGRESS — BUILDS, NOT YET BOOTED.**
>
> The lineage-18.1 **eng** build completes: a signed flashable package and
> `system.img` / `boot.img` / `dt.img` are produced. The tree started life as a
> copy of the [heroqlte (Galaxy S7 QCOM)](https://github.com/ivanmeler/android_device_samsung_heroqlte)
> lineage-18.1 tree; the hardware-specific pieces were then checked against the
> real phone (partition table, display, input) and the kernel/UAPI gaps were
> fixed. Nothing has been booted on hardware yet — expect missing HALs and
> broken RIL/camera/audio.

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
      (configfs, quota, memcg and the Android 11 userspace bits are still
      pending; the tree builds and boots TWRP today)
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
