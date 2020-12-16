/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef I915_PRIVATE_ANDROID
#define I915_PRIVATE_ANDROID

#include <stdint.h>

#include "i915_private_types.h"

uint32_t i915_private_convert_format(int format);

int32_t i915_private_invert_format(int format);

bool i915_private_supported_yuv_format(uint32_t droid_format);

#endif
