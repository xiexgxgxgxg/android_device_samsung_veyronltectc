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
