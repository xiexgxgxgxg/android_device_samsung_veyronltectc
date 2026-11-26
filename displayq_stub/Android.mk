LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := displayq_stub.cpp
LOCAL_MODULE := display_qservice_stub
LOCAL_MODULE_TAGS := optional
LOCAL_VENDOR_MODULE := true
LOCAL_MULTILIB := 32
LOCAL_SHARED_LIBRARIES := libbinder libutils liblog
LOCAL_INIT_RC := display_qservice_stub.rc
LOCAL_CFLAGS := -Wall -Werror
include $(BUILD_EXECUTABLE)
