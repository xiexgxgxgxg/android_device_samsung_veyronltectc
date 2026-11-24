# Development helper used to debug SIM detection; not part of the shipping
# image (see qmiuim.c).  Build with:  m qmiuim

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := qmiuim.c

LOCAL_MODULE := qmiuim
LOCAL_MODULE_TAGS := optional
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MULTILIB := 64

# The QMI libraries only exist as prebuilt vendor blobs, so they are opened
# with dlopen() at runtime instead of being linked against.
LOCAL_SHARED_LIBRARIES := liblog libdl

LOCAL_CFLAGS := -Wall

include $(BUILD_EXECUTABLE)
