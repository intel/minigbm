/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifdef DRV_I915

#include <stdint.h>

#include "i915_private_types.h"

struct driver;

int i915_private_init(struct driver *drv, uint64_t *cursor_width, uint64_t *cursor_height);

int i915_private_add_combinations(struct driver *drv);

void i915_private_align_dimensions(uint32_t format, uint32_t *vertical_alignment);

uint32_t i915_private_bpp_from_format(uint32_t format, size_t plane);

void i915_private_vertical_subsampling_from_format(uint32_t *vertical_subsampling, uint32_t format,
						   size_t plane);

size_t i915_private_num_planes_from_format(uint32_t format);

uint32_t i915_private_resolve_format(uint32_t format, uint64_t usage, uint32_t *resolved_format);

#endif
