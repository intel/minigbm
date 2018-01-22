/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifdef DRV_I915

#include <assert.h>
#include <errno.h>
#include <i915_drm.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <drm_fourcc.h>
#include <xf86drm.h>

#include "drv_priv.h"
#include "helpers.h"
#include "util.h"
#include "i915_private.h"

static const uint32_t private_linear_source_formats[] = { DRM_FORMAT_R16,    DRM_FORMAT_NV16,
							  DRM_FORMAT_YUV420, DRM_FORMAT_YUV422,
							  DRM_FORMAT_YUV444, DRM_FORMAT_NV21,
							  DRM_FORMAT_P010, DRM_FORMAT_RGB888, DRM_FORMAT_BGR888,
							  DRM_FORMAT_XRGB161616, DRM_FORMAT_XBGR161616 };

static const uint32_t private_source_formats[] = { DRM_FORMAT_P010, DRM_FORMAT_NV12_Y_TILED_INTEL };

#if !defined(DRM_CAP_CURSOR_WIDTH)
#define DRM_CAP_CURSOR_WIDTH 0x8
#endif

#if !defined(DRM_CAP_CURSOR_HEIGHT)
#define DRM_CAP_CURSOR_HEIGHT 0x9
#endif

static const uint32_t kDefaultCursorWidth = 64;
static const uint32_t kDefaultCursorHeight = 64;

#define BO_USE_CAMERA_MASK BO_USE_CAMERA_READ | BO_USE_SCANOUT | BO_USE_CAMERA_WRITE

static void get_preferred_cursor_attributes(uint32_t drm_fd, uint64_t *cursor_width,
					    uint64_t *cursor_height)
{
	uint64_t width = 0, height = 0;
	if (drmGetCap(drm_fd, DRM_CAP_CURSOR_WIDTH, &width)) {
		fprintf(stderr, "cannot get cursor width. \n");
	} else if (drmGetCap(drm_fd, DRM_CAP_CURSOR_HEIGHT, &height)) {
		fprintf(stderr, "cannot get cursor height. \n");
	}

	if (!width)
		width = kDefaultCursorWidth;

	*cursor_width = width;

	if (!height)
		height = kDefaultCursorHeight;

	*cursor_height = height;
}

int i915_private_init(struct driver *drv, uint64_t *cursor_width, uint64_t *cursor_height)
{
	get_preferred_cursor_attributes(drv->fd, cursor_width, cursor_height);
	return 0;
}

int i915_private_add_combinations(struct driver *drv)
{
	struct format_metadata metadata;
	uint64_t render_flags, texture_flags;

	render_flags = BO_USE_RENDER_MASK;
	texture_flags = BO_USE_TEXTURE_MASK;

	metadata.tiling = I915_TILING_NONE;
	metadata.priority = 1;
	metadata.modifier = DRM_FORMAT_MOD_NONE;

	drv_modify_combination(drv, DRM_FORMAT_ABGR8888, &metadata, BO_USE_CURSOR | BO_USE_SCANOUT);
	drv_modify_combination(drv, DRM_FORMAT_NV12, &metadata,
			       BO_USE_RENDERING | BO_USE_TEXTURE | BO_USE_CAMERA_MASK);
	drv_modify_combination(drv, DRM_FORMAT_YUYV, &metadata,
			       BO_USE_TEXTURE | BO_USE_CAMERA_MASK | BO_USE_RENDERING);
	drv_modify_combination(drv, DRM_FORMAT_VYUY, &metadata,
			       BO_USE_TEXTURE | BO_USE_CAMERA_MASK | BO_USE_RENDERING);
	drv_modify_combination(drv, DRM_FORMAT_UYVY, &metadata,
			       BO_USE_TEXTURE | BO_USE_CAMERA_MASK | BO_USE_RENDERING);
	drv_modify_combination(drv, DRM_FORMAT_YVYU, &metadata,
			       BO_USE_TEXTURE | BO_USE_CAMERA_MASK | BO_USE_RENDERING);
	drv_modify_combination(drv, DRM_FORMAT_YVU420_ANDROID, &metadata,
			       BO_USE_TEXTURE | BO_USE_CAMERA_MASK);

	/* Media/Camera expect these formats support. */
	metadata.tiling = I915_TILING_NONE;
	metadata.priority = 1;
	metadata.modifier = DRM_FORMAT_MOD_NONE;
	drv_add_combinations(drv, private_linear_source_formats,
			     ARRAY_SIZE(private_linear_source_formats), &metadata,
			     texture_flags | BO_USE_CAMERA_MASK);

	metadata.tiling = I915_TILING_Y;
	metadata.priority = 3;
	metadata.modifier = I915_FORMAT_MOD_Y_TILED;
	drv_add_combinations(drv, private_source_formats, ARRAY_SIZE(private_source_formats),
			     &metadata, texture_flags | BO_USE_CAMERA_MASK);

	texture_flags &= ~BO_USE_RENDERSCRIPT;
	texture_flags &= ~BO_USE_SW_WRITE_OFTEN;
	texture_flags &= ~BO_USE_SW_READ_OFTEN;
	texture_flags &= ~BO_USE_LINEAR;

	metadata.tiling = I915_TILING_X;
	metadata.priority = 2;
	metadata.modifier = I915_FORMAT_MOD_X_TILED;

	int ret = drv_add_combinations(drv, private_linear_source_formats,
				       ARRAY_SIZE(private_linear_source_formats), &metadata,
				       texture_flags | BO_USE_CAMERA_MASK);
	if (ret)
		return ret;

	return 0;
}

void i915_private_align_dimensions(uint32_t format, uint32_t *vertical_alignment)
{
	switch (format) {
	case DRM_FORMAT_NV12_Y_TILED_INTEL:
		*vertical_alignment = 64;
		break;
	}
}

uint32_t i915_private_bpp_from_format(uint32_t format, size_t plane)
{
	assert(plane < drv_num_planes_from_format(format));

	switch (format) {
	case DRM_FORMAT_NV12_Y_TILED_INTEL:
		return (plane == 0) ? 8 : 4;
	case DRM_FORMAT_P010:
		return (plane == 0) ? 16 : 8;
	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_YUV422:
	case DRM_FORMAT_YUV444:
	case DRM_FORMAT_NV16:
		return 8;
	case DRM_FORMAT_R16:
		return 16;
        case DRM_FORMAT_XRGB161616:
        case DRM_FORMAT_XBGR161616:
                return 64;
	}

	fprintf(stderr, "drv: UNKNOWN FORMAT %d\n", format);
	return 0;
}

void i915_private_vertical_subsampling_from_format(uint32_t *vertical_subsampling, uint32_t format,
						   size_t plane)
{
	switch (format) {
	case DRM_FORMAT_NV12_Y_TILED_INTEL:
	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_P010:
		*vertical_subsampling = (plane == 0) ? 1 : 2;
		break;
	default:
		*vertical_subsampling = 1;
	}
}

size_t i915_private_num_planes_from_format(uint32_t format)
{
	switch (format) {
	case DRM_FORMAT_R16:
        case DRM_FORMAT_XRGB161616:
        case DRM_FORMAT_XBGR161616:
		return 1;
	case DRM_FORMAT_NV12_Y_TILED_INTEL:
	case DRM_FORMAT_NV16:
	case DRM_FORMAT_P010:
		return 2;
	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_YUV422:
	case DRM_FORMAT_YUV444:
		return 3;
	}

	fprintf(stderr, "drv: UNKNOWN FORMAT %d\n", format);
	return 0;
}

uint32_t i915_private_resolve_format(uint32_t format, uint64_t usage, uint32_t *resolved_format)
{
	switch (format) {
	case DRM_FORMAT_FLEX_IMPLEMENTATION_DEFINED:
		/* KBL camera subsystem requires NV12. */
		if (usage & (BO_USE_CAMERA_READ | BO_USE_CAMERA_WRITE)) {
			*resolved_format = DRM_FORMAT_NV12;
                        return 1;
                }

		if (usage & BO_USE_TEXTURE) {
			*resolved_format = DRM_FORMAT_ABGR8888;
			return 1;
		}
	}

	return 0;
}

#endif
