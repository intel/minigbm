# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

ifeq ($(strip $(BOARD_USES_MINIGBM)), true)

MINIGBM_GRALLOC_MK := $(call my-dir)/Android.gralloc.mk
LOCAL_PATH := $(call my-dir)
intel_drivers := i915 i965 iris
include $(CLEAR_VARS)

SUBDIRS := cros_gralloc

LOCAL_SHARED_LIBRARIES := \
	libcutils             \
	liblog                \
	libdrm_pri            \
	libsync

LOCAL_SRC_FILES :=  \
    amdgpu.c        \
    drv.c           \
    evdi.c          \
    exynos.c        \
    helpers_array.c \
    helpers.c       \
    i915.c          \
    marvell.c       \
    mediatek.c      \
    meson.c         \
    msm.c           \
    nouveau.c       \
    radeon.c        \
    rockchip.c      \
    tegra.c         \
    udl.c           \
    vc4.c           \
    vgem.c          \
    virtio_gpu.c


include $(MINIGBM_GRALLOC_MK)

LOCAL_CPPFLAGS += -std=c++14

LOCAL_CFLAGS += -D_GNU_SOURCE=1 -D_FILE_OFFSET_BITS=64         \
                -Wall -Wsign-compare -Wpointer-arith           \
                -Wcast-qual -Wcast-align -Wno-unused-parameter

LOCAL_C_INCLUDES += frameworks/native/libs/nativebase/include   \
                    frameworks/native/libs/nativewindow/include \
                    frameworks/native/libs/arect/include

ifneq ($(filter $(intel_drivers), $(BOARD_GPU_DRIVERS)),)
LOCAL_CPPFLAGS += -DDRV_I915
LOCAL_CFLAGS += -DDRV_I915
LOCAL_SHARED_LIBRARIES += libdrm_intel_pri
endif

ifeq ($(shell test $(PLATFORM_SDK_VERSION) -ge 27; echo $$?), 0)
LOCAL_SHARED_LIBRARIES += libnativewindow
LOCAL_STATIC_LIBRARIES += libarect
LOCAL_HEADER_LIBRARIES += libnativebase_headers libsystem_headers libhardware_headers libutils_headers
LOCAL_CFLAGS += -DUSE_VNDK
endif

LOCAL_CFLAGS += -Wno-error
LOCAL_MODULE := gralloc.$(TARGET_BOARD_PLATFORM)
LOCAL_MODULE_TAGS := optional
# The preferred path for vendor HALs is /vendor/lib/hw
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_SUFFIX := $(TARGET_SHLIB_SUFFIX)
include $(BUILD_SHARED_LIBRARY)

endif
