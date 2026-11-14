# LineageOS 18.1 device tree — Samsung W2017 (SM-W2017 / veyronlte CTC)

> **STATUS: WORK IN PROGRESS — DOES NOT BOOT, DO NOT FLASH.**
>
> This tree is a skeleton. It was generated from the
> [heroqlte (Galaxy S7 QCOM)](https://github.com/ivanmeler/android_device_samsung_heroqlte)
> lineage-18.1 tree and renamed for the W2017. Nothing here has been verified on
> real hardware. Expect boot loops, missing HALs and broken RIL/camera/audio.

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

## Provenance

* Structure: `ivanmeler/android_device_samsung_heroqlte` (`lineage-18.1`)
* Common tree: `device/samsung/msm8996-common` (same org)
* Kernel: Samsung official msm8996 source (`veyronlte_ctc_defconfig`, Linux 3.18.20)

## What works

Nothing. This is an untested skeleton.

## TODO

- [ ] Vendor blobs: extract from the device / stock firmware, regenerate
      `proprietary-files.txt` (the current list is inherited from heroqlte and is
      **wrong** for this device)
- [ ] Kernel: build `veyronlte_ctc_defconfig`, backport 18.1 requirements
      (binder, sdcardfs/FUSE, SELinux, quota, wifi driver)
- [ ] Verify partition layout / fstab against the real PIT
- [ ] Keypad: `keylayout` / `keychars` from stock
- [ ] Dual panel: verify Android behaves when the panel switches under it
- [ ] RIL: Samsung SecRIL 6.0.1 → radio@1.4 / secbridge (hardest part)
- [ ] Camera: 6.0.1 HAL1 blobs on Pie
- [ ] Audio, sensors, fingerprint, GPS
- [ ] SELinux policy
- [ ] `BUILD_FINGERPRINT` placeholder → real value from device

## Local manifest

```xml
<?xml version="1.0" encoding="UTF-8"?>
<manifest>
    <project name="xiexgxgxgxg/android_device_samsung_msm8996-common"
        path="device/samsung/msm8996-common"
        revision="lineage-18.1" />
    <project name="xiexgxgxgxg/android_device_samsung_veyronltectc"
        path="device/samsung/veyronltectc"
        revision="lineage-18.1" />
    <project name="xiexgxgxgxg/android_vendor_samsung_veyronltectc"
        path="vendor/samsung/veyronltectc"
        revision="lineage-18.1" />
    <project name="xiexgxgxgxg/android_kernel_samsung_veyronltectc"
        path="kernel/samsung/msm8996"
        revision="lineage-18.1" />
</manifest>
```

## Build

```bash
source build/envsetup.sh
lunch lineage_veyronltectc-userdebug
mka bacon
```

It will not build yet — the vendor tree has no blobs.
