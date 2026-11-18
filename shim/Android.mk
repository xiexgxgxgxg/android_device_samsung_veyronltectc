LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_SRC_FILES := nh_shim.c
LOCAL_MODULE := libnh_shim
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)
