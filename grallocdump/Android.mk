LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_SRC_FILES := grallocdump.cpp
LOCAL_MODULE := grallocdump
LOCAL_MODULE_TAGS := optional
LOCAL_VENDOR_MODULE := true
LOCAL_MULTILIB := 32
LOCAL_SHARED_LIBRARIES := libui libgui libutils liblog libcutils
include $(BUILD_EXECUTABLE)
