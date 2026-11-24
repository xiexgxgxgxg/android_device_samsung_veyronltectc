
#
# Copyright (C) 2020-2021 The LineageOS Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# WARNING: WIP port for Samsung W2017 (SM-W2017, veyronlte/CTC).
# Derived from the heroqlte (Galaxy S7 QCOM) tree; nothing here is verified
# on hardware yet. Kernel config name comes from the official Samsung
# msm8996 kernel source (arch/arm64/configs/veyronlte_ctc_defconfig).

DEVICE_PATH := device/samsung/veyronltectc

# Shared msm8996 board config (inlined in $(DEVICE_PATH)/common, no external common repo)
-include $(DEVICE_PATH)/common/BoardConfigCommon.mk

# Assert
TARGET_OTA_ASSERT_DEVICE := veyronltectc,veyronlte,SM-W2017

# HIDL
DEVICE_FRAMEWORK_MANIFEST_FILE := $(DEVICE_PATH)/framework_manifest.xml
DEVICE_MANIFEST_FILE += $(DEVICE_PATH)/manifest.xml

# Kernel
TARGET_KERNEL_CONFIG := veyronlte_ctc_defconfig

# Properties
TARGET_VENDOR_PROP += $(DEVICE_PATH)/vendor.prop

# RIL
#
# The stock 6.0.1 vendor RIL (libsec-ril.so) only implements the legacy
# RIL_RadioFunctions API, so rild has to be built from AOSP's hardware/ril:
# A11's libril carries the IRadio HIDL service (libril/ril_service.cpp) that
# the framework talks to, and it loads the vendor library through
# rild.libpath.  Do not set BOARD_PROVIDES_LIBRIL (it would skip building the
# AOSP libril) or ENABLE_VENDOR_RIL_SERVICE (it would skip rild and leave the
# device without any radio stack, which is what happened before).
#BOARD_PROVIDES_LIBRIL := true
#ENABLE_VENDOR_RIL_SERVICE := true

# inherit from the proprietary version
include vendor/samsung/veyronltectc/BoardConfigVendor.mk
