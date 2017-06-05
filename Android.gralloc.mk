# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

LOCAL_CPP_EXTENSION := .cc

LOCAL_SRC_FILES += \
	cros_gralloc/cros_gralloc_buffer.cc \
	cros_gralloc/cros_gralloc_driver.cc \
        cros_gralloc/cros_gralloc_helpers.cc

ifeq ($(strip $(BOARD_USES_GRALLOC1)), true)
LOCAL_SRC_FILES += cros_gralloc/gralloc1/cros_gralloc1_module.cc
else
LOCAL_SRC_FILES += cros_gralloc/gralloc0/cros_gralloc_module.cc
endif
