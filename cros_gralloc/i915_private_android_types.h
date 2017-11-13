/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Android graphics.h defines the formats and leaves 0x100 - 0x1FF
 * range available for HAL implementation specific formats.
 */

#ifdef USE_GRALLOC1
#include <hardware/gralloc1.h>
#endif

enum { HAL_PIXEL_FORMAT_NV12_Y_TILED_INTEL = 0x100,
       HAL_PIXEL_FORMAT_P010_INTEL = 0x110,
       HAL_PIXEL_FORMAT_A2R10G10B10_INTEL = 0x113,
       HAL_PIXEL_FORMAT_X2R10G10B10_INTEL = 0x119,
       HAL_PIXEL_FORMAT_NV12 = 0x10F,
};

#ifdef USE_GRALLOC1
enum { GRALLOC1_FUNCTION_SET_MODIFIER = 101 };

typedef int32_t /*gralloc1_error_t*/ (*GRALLOC1_PFN_SET_MODIFIER)(
    gralloc1_device_t *device, gralloc1_buffer_descriptor_t descriptor, uint64_t modifier);
#endif
