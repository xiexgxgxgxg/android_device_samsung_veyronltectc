LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_SRC_FILES := bufdump.c
LOCAL_MODULE := bufdump
LOCAL_MODULE_TAGS := optional
LOCAL_VENDOR_MODULE := true
LOCAL_MULTILIB := 32
LOCAL_CFLAGS := -Wall
include $(BUILD_EXECUTABLE)
