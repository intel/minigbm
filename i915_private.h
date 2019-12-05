/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifdef DRV_I915

#include <stdint.h>

struct driver;

/*
 * 2 plane YCbCr MSB aligned
 * index 0 = Y plane, [15:0] Y:x [10:6] little endian
 * index 1 = Cr:Cb plane, [31:0] Cr:x:Cb:x [10:6:10:6] little endian
 */
#define DRM_FORMAT_P010		fourcc_code('P', '0', '1', '0') /* 2x2 subsampled Cr:Cb plane 10 bits per channel */

/*
 * 2 plane YCbCr MSB aligned
 * index 0 = Y plane, [15:0] Y:x [12:4] little endian
 * index 1 = Cr:Cb plane, [31:0] Cr:x:Cb:x [12:4:12:4] little endian
 */
#define DRM_FORMAT_P012		fourcc_code('P', '0', '1', '2') /* 2x2 subsampled Cr:Cb plane 12 bits per channel */

/*
 * 2 plane YCbCr MSB aligned
 * index 0 = Y plane, [15:0] Y little endian
 * index 1 = Cr:Cb plane, [31:0] Cr:Cb [16:16] little endian
 */
#define DRM_FORMAT_P016		fourcc_code('P', '0', '1', '6') /* 2x2 subsampled Cr:Cb plane 16 bits per channel */

/* 64 bpp RGB */
#define DRM_FORMAT_XRGB161616  fourcc_code('X', 'R', '4', '8') /* [63:0] x:R:G:B 16:16:16:16 little endian */
#define DRM_FORMAT_XBGR161616  fourcc_code('X', 'B', '4', '8') /* [63:0] x:B:G:R 16:16:16:16 little endian */
#define DRM_FORMAT_ABGR16161616F  fourcc_code('A', 'B', '4', 'H') /* [63:0] x:B:G:R 16:16:16:16 little endian */

#define DRM_FORMAT_NV12_Y_TILED_INTEL fourcc_code('9', '9', '9', '6')

int i915_private_init(struct driver *drv, uint64_t *cursor_width, uint64_t *cursor_height);

int i915_private_add_combinations(struct driver *drv);

void i915_private_align_dimensions(uint32_t format, uint32_t *vertical_alignment);

uint32_t i915_private_bpp_from_format(uint32_t format, size_t plane);

void i915_private_vertical_subsampling_from_format(uint32_t *vertical_subsampling, uint32_t format,
						   size_t plane);

size_t i915_private_num_planes_from_format(uint32_t format);

uint32_t i915_private_resolve_format(uint32_t format, uint64_t usage, uint32_t *resolved_format);

#endif
