# Compatibility symbols the stock 6.0.1 vendor RIL library still references.
# Loaded with LD_PRELOAD by the rild service; see ril_compat_shim.cpp.

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := ril_compat_shim.cpp

LOCAL_MODULE := libril_compat_shim
LOCAL_MODULE_TAGS := optional

# vendor process: keep it next to the RIL stack it serves
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_CLASS := SHARED_LIBRARIES

LOCAL_SHARED_LIBRARIES := liblog

# RIL_CardStatus_v6 / RIL_REQUEST_* definitions
LOCAL_C_INCLUDES := hardware/ril/include

LOCAL_CFLAGS := -Wall -Werror

include $(BUILD_SHARED_LIBRARY)
