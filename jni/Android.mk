LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

GIT_VERSION := " $(shell git rev-parse --short HEAD || echo unknown)"
ifneq ($(GIT_VERSION)," unknown")
	LOCAL_CFLAGS += -DGIT_VERSION=\"$(GIT_VERSION)\"
endif

ifeq ($(TARGET_ARCH),arm)
LOCAL_CFLAGS += -DANDROID_ARM -fsigned-char
LOCAL_ARM_MODE := arm
endif

ifeq ($(TARGET_ARCH),x86)
LOCAL_CFLAGS +=  -DANDROID_X86
endif

ifeq ($(TARGET_ARCH),mips)
LOCAL_CFLAGS += -DANDROID_MIPS
endif

CORE_DIR := ../core

LOCAL_MODULE    := libretro

include ../Makefile.common

LOCAL_SRC_FILES    =  $(SOURCES_CXX) $(SOURCES_C)

LOCAL_CXXFLAGS = -DANDROID -D__LIBRETRO__ $(INCFLAGS)
LOCAL_CFLAGS = -DANDROID -D__LIBRETRO__ $(INCFLAGS)
LOCAL_LDLIBS   += -lz
LOCAL_C_INCLUDES = $(INCFLAGS)

include $(BUILD_SHARED_LIBRARY)
