LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := mkbootimg
LOCAL_C_INCLUDES := $(LOCAL_PATH)
LOCAL_SRC_FILES := mkbootimg.c mincrypt/sha.c
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := unpackbootimg
LOCAL_C_INCLUDES := $(LOCAL_PATH)
LOCAL_SRC_FILES := unpackbootimg.c mincrypt/sha.c
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)
