
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LIBRARY_PATH := $(LOCAL_PATH)/..

# module
LOCAL_MODULE := libuac

LOCAL_SHARED_LIBRARIES := libusb1.0

LOCAL_SRC_FILES := $(wildcard $(LIBRARY_PATH)/src/*.cpp)

LOCAL_C_INCLUDES += \
  $(LOCAL_PATH) \
  $(LIBRARY_PATH)/include \
  $(LIBRARY_PATH)/src

LOCAL_EXPORT_C_INCLUDES := \
  $(LIBRARY_PATH)/include

LOCAL_CPP_FEATURES := exceptions

LOCAL_CFLAGS := -pthread

ifeq ($(APP_DEBUG),true)
LOCAL_CFLAGS += -DUAC_ENABLE_LOGGING
endif

LOCAL_LDLIBS := -llog

include $(BUILD_STATIC_LIBRARY)
