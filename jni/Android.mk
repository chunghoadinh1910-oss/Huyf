LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := zygisk_hider
LOCAL_SRC_FILES := module.cpp
LOCAL_CPPFLAGS := -std=c++17 -fvisibility=hidden -fno-exceptions
LOCAL_LDLIBS := -llog
include $(BUILD_SHARED_LIBRARY)
