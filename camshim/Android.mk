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
