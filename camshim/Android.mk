LOCAL_PATH := $(call my-dir)

# For the vendor camera provider (preloaded via its rc).
include $(CLEAR_VARS)
LOCAL_SRC_FILES := camera_parameters_shim.cpp
LOCAL_MODULE := libcamparams_shim
LOCAL_MODULE_TAGS := optional
LOCAL_VENDOR_MODULE := true
include $(BUILD_SHARED_LIBRARY)

# Same library for cameraserver, which loads the HAL in-process.
include $(CLEAR_VARS)
LOCAL_SRC_FILES := camera_parameters_shim.cpp
LOCAL_MODULE := libcamparams_shim_sys
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)

# Android 6.0.1 blob compatibility: mm-qcamera-daemon's ISP module destroys
# a mutex twice; Android 11's bionic aborts the process on that.  Loaded via
# LD_PRELOAD by the mm-qcamera-daemon service.
include $(CLEAR_VARS)
LOCAL_SRC_FILES := mutex_shim.c
LOCAL_MODULE := libmmcamera_mutex_shim
LOCAL_MODULE_TAGS := optional
LOCAL_VENDOR_MODULE := true
include $(BUILD_SHARED_LIBRARY)

# android::CameraMetadata ABI: 6.0.1 has only mBuffer, Android 11 added
# mLocked.  camera.msm8996.so is rewritten (tools/patch_camera_metadata_abi.py)
# to reference android::LegacyMetadata, which this library implements with the
# 6.0.1 layout.  Vendor copy is preloaded by the camera provider, system copy
# by cameraserver, because both processes dlopen the 6.0.1 HAL.
include $(CLEAR_VARS)
LOCAL_SRC_FILES := camera_metadata_legacy.cpp
LOCAL_MODULE := libcammetadata_legacy
LOCAL_MODULE_TAGS := optional
LOCAL_VENDOR_MODULE := true
LOCAL_MULTILIB := 32
LOCAL_SHARED_LIBRARIES := liblog libutils libcamera_metadata
LOCAL_CFLAGS := -Wall -Werror
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := camera_metadata_legacy.cpp
LOCAL_MODULE := libcammetadata_legacy_sys
LOCAL_MODULE_TAGS := optional
LOCAL_MULTILIB := 32
LOCAL_SHARED_LIBRARIES := liblog libutils libcamera_metadata
LOCAL_CFLAGS := -Wall -Werror
include $(BUILD_SHARED_LIBRARY)
