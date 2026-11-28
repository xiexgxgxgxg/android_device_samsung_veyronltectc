LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE          := sps_crop
LOCAL_MODULE_TAGS     := optional
LOCAL_PROPRIETARY_MODULE := true
LOCAL_SRC_FILES       := sps_crop.c
LOCAL_CFLAGS          := -Wall -Wno-unused-result
LOCAL_SHARED_LIBRARIES := libc liblog
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE          := mp4_crop_watch.sh
LOCAL_MODULE_TAGS     := optional
LOCAL_MODULE_CLASS    := EXECUTABLES
LOCAL_PROPRIETARY_MODULE := true
LOCAL_SRC_FILES       := mp4_crop_watch.sh
include $(BUILD_PREBUILT)
