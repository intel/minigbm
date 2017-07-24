/*
 * Copyright 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifdef DRV_I915

#include <errno.h>
#include <i915_drm.h>
#include <intel_bufmgr.h>
#include <string.h>
#include <sys/mman.h>
#include <xf86drm.h>

#include "drv_priv.h"
#include "helpers.h"
#include "util.h"

#if !defined(DRM_CAP_CURSOR_WIDTH)
#define DRM_CAP_CURSOR_WIDTH 0x8
#endif

#if !defined(DRM_CAP_CURSOR_HEIGHT)
#define DRM_CAP_CURSOR_HEIGHT 0x9
#endif

static const uint32_t kDefaultCursorWidth = 64;
static const uint32_t kDefaultCursorHeight = 64;

static const uint32_t render_target_formats[] = { DRM_FORMAT_ARGB1555, DRM_FORMAT_ABGR8888,
						  DRM_FORMAT_ARGB8888, DRM_FORMAT_RGB565,
						  DRM_FORMAT_XBGR8888, DRM_FORMAT_XRGB1555,
						  DRM_FORMAT_XRGB8888 };

static const uint32_t tileable_texture_source_formats[] = { DRM_FORMAT_GR88, DRM_FORMAT_R8,
							    DRM_FORMAT_UYVY, DRM_FORMAT_YUYV,
                                                            DRM_FORMAT_NV12 };

static const uint32_t texture_source_formats[] = { DRM_FORMAT_YVU420, DRM_FORMAT_YVU420_ANDROID };

static const uint32_t camera_formats[] = { DRM_FORMAT_R8, DRM_FORMAT_NV12 };

struct i915_device {
	int gen;
	drm_intel_bufmgr *mgr;
	uint32_t count;
	uint64_t cursor_width;
	uint64_t cursor_height;
};

struct i915_bo {
	drm_intel_bo *ibos[DRV_MAX_PLANES];
};

static int get_gen(int device_id)
{
	const uint16_t gen3_ids[] = { 0x2582, 0x2592, 0x2772, 0x27A2, 0x27AE,
				      0x29C2, 0x29B2, 0x29D2, 0xA001, 0xA011 };
	unsigned i;
	for (i = 0; i < ARRAY_SIZE(gen3_ids); i++)
		if (gen3_ids[i] == device_id)
			return 3;

	return 4;
}

static int i915_add_kms_item(struct driver *drv, const struct kms_item *item)
{
	uint32_t i;
	struct combination *combo;

	/*
	 * Older hardware can't scanout Y-tiled formats. Newer devices can, and
	 * report this functionality via format modifiers.
	 */
	for (i = 0; i < drv->backend->combos.size; i++) {
		combo = &drv->backend->combos.data[i];
		if (combo->format == item->format) {
			if ((combo->metadata.tiling == I915_TILING_Y &&
			     item->modifier == I915_FORMAT_MOD_Y_TILED) ||
			    (combo->metadata.tiling == I915_TILING_X &&
			     item->modifier == I915_FORMAT_MOD_X_TILED)) {
				combo->metadata.modifier = item->modifier;
				combo->usage |= item->usage;
			} else if (combo->metadata.tiling != I915_TILING_Y) {
				combo->usage |= item->usage;
			}
		}
	}

	return 0;
}

static int i915_add_combinations(struct driver *drv)
{
	int ret;
	uint32_t i, num_items;
	struct kms_item *items;
	struct format_metadata metadata;
	uint64_t render_flags, texture_flags, camera_flags;

	render_flags = BO_USE_RENDER_MASK;
	texture_flags = BO_USE_TEXTURE_MASK;
	camera_flags = BO_USE_CAMERA_MASK;

	metadata.tiling = I915_TILING_NONE;
	metadata.priority = 1;
	metadata.modifier = DRM_FORMAT_MOD_NONE;

	ret = drv_add_combinations(drv, render_target_formats, ARRAY_SIZE(render_target_formats),
				   &metadata, render_flags);
	if (ret)
		return ret;

	ret = drv_add_combinations(drv, texture_source_formats, ARRAY_SIZE(texture_source_formats),
				   &metadata, texture_flags);
	if (ret)
		return ret;

	ret = drv_add_combinations(drv, tileable_texture_source_formats,
				   ARRAY_SIZE(tileable_texture_source_formats), &metadata,
				   texture_flags);
	if (ret)
		return ret;

	ret = drv_add_combinations(drv, camera_formats, ARRAY_SIZE(camera_formats), &metadata,
				   camera_flags);
	if (ret)
		return ret;

	drv_modify_combination(drv, DRM_FORMAT_XRGB8888, &metadata, BO_USE_CURSOR | BO_USE_SCANOUT);
	drv_modify_combination(drv, DRM_FORMAT_ARGB8888, &metadata, BO_USE_CURSOR | BO_USE_SCANOUT);
	drv_modify_combination(drv, DRM_FORMAT_ABGR8888, &metadata, BO_USE_CURSOR | BO_USE_SCANOUT);

	render_flags &= ~BO_USE_SW_WRITE_OFTEN;
	render_flags &= ~BO_USE_SW_READ_OFTEN;
	render_flags &= ~BO_USE_LINEAR;

	texture_flags &= ~BO_USE_SW_WRITE_OFTEN;
	texture_flags &= ~BO_USE_SW_READ_OFTEN;
	texture_flags &= ~BO_USE_LINEAR;

	metadata.tiling = I915_TILING_X;
	metadata.priority = 2;

	ret = drv_add_combinations(drv, render_target_formats, ARRAY_SIZE(render_target_formats),
				   &metadata, render_flags);
	if (ret)
		return ret;

	ret = drv_add_combinations(drv, tileable_texture_source_formats,
				   ARRAY_SIZE(tileable_texture_source_formats), &metadata,
				   texture_flags);
	if (ret)
		return ret;

	metadata.tiling = I915_TILING_Y;
	metadata.priority = 3;

	ret = drv_add_combinations(drv, render_target_formats, ARRAY_SIZE(render_target_formats),
				   &metadata, render_flags);
	if (ret)
		return ret;

	ret = drv_add_combinations(drv, tileable_texture_source_formats,
				   ARRAY_SIZE(tileable_texture_source_formats), &metadata,
				   texture_flags);
	if (ret)
		return ret;

	items = drv_query_kms(drv, &num_items);
	if (!items || !num_items)
		return 0;

	for (i = 0; i < num_items; i++) {
		ret = i915_add_kms_item(drv, &items[i]);
		if (ret) {
			free(items);
			return ret;
		}
	}

	free(items);
	return 0;
}

static void get_preferred_cursor_attributes(uint32_t drm_fd,
					    uint64_t *cursor_width,
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

static int i915_align_dimensions(struct bo *bo, uint32_t tiling, uint32_t *stride,
				 uint32_t *aligned_height)
{
	struct i915_device *i915 = bo->drv->priv;
	uint32_t horizontal_alignment = 4;
	uint32_t vertical_alignment = 4;

	switch (tiling) {
	default:
	case I915_TILING_NONE:
		horizontal_alignment = 64;
		break;

	case I915_TILING_X:
		horizontal_alignment = 512;
		vertical_alignment = 8;
		break;

	case I915_TILING_Y:
		if (i915->gen == 3) {
			horizontal_alignment = 512;
			vertical_alignment = 8;
		} else {
			horizontal_alignment = 128;
			vertical_alignment = 32;
		}
		break;
	}

	*aligned_height = ALIGN(bo->height, vertical_alignment);
	if (i915->gen > 3) {
		*stride = ALIGN(*stride, horizontal_alignment);
	} else {
		while (*stride > horizontal_alignment)
			horizontal_alignment <<= 1;

		*stride = horizontal_alignment;
	}

	if (i915->gen <= 3 && *stride > 8192)
		return -EINVAL;

	return 0;
}

static int i915_init(struct driver *drv)
{
	struct i915_device *i915_dev;
	drm_i915_getparam_t get_param;
	int device_id;
	int ret;

	i915_dev = calloc(1, sizeof(*i915_dev));
	if (!i915_dev)
		return -1;

	memset(&get_param, 0, sizeof(get_param));
	get_param.param = I915_PARAM_CHIPSET_ID;
	get_param.value = &device_id;
	ret = drmIoctl(drv->fd, DRM_IOCTL_I915_GETPARAM, &get_param);
	if (ret) {
		fprintf(stderr, "drv: DRM_IOCTL_I915_GETPARAM failed\n");
		free(i915_dev);
		return -EINVAL;
	}

	i915_dev->gen = get_gen(device_id);
	i915_dev->count = 0;

	i915_dev->mgr = drm_intel_bufmgr_gem_init(drv->fd, 16 * 1024);
	if (!i915_dev->mgr) {
		fprintf(stderr, "drv: drm_intel_bufmgr_gem_init failed\n");
		free(i915_dev);
		return -EINVAL;
	}

	drv->priv = i915_dev;

	get_preferred_cursor_attributes(drv->fd, &i915_dev->cursor_width, &i915_dev->cursor_height);

	return i915_add_combinations(drv);
}

static void i915_close(struct driver *drv)
{
	struct i915_device *i915_dev = drv->priv;
	drm_intel_bufmgr_destroy(i915_dev->mgr);
	free(i915_dev);
	drv->priv = NULL;
}

static int i915_bo_create(struct bo *bo, uint32_t width, uint32_t height, uint32_t format,
			  uint32_t flags)
{
	int ret;
	size_t plane;
	char name[20];
	uint32_t stride;
	uint32_t tiling_mode;
	struct i915_bo *i915_bo;

	stride = drv_stride_from_format(format, width, 0);
	struct i915_device *i915_dev = (struct i915_device *)bo->drv->priv;

#if 0
	if (flags & (BO_USE_CURSOR | BO_USE_LINEAR | BO_USE_SW_READ_OFTEN | BO_USE_SW_WRITE_OFTEN))
		tiling_mode = I915_TILING_NONE;
	else if (flags & BO_USE_SCANOUT)
		tiling_mode = I915_TILING_X;
	else
		bo->tiling = I915_TILING_Y;
#else
	bo->tiling = I915_TILING_NONE;
	tiling_mode = I915_TILING_NONE;
#endif

	/*
	 * Align cursor width and height to values expected by Intel
	 * HW.
	 */
	if (flags & BO_USE_CURSOR) {
	    width = ALIGN(width, i915_dev->cursor_width);
	    height = ALIGN(height, i915_dev->cursor_height);
	    stride = drv_stride_from_format(format, width, 0);
	} else {
	    ret = i915_align_dimensions(bo, bo->tiling, &stride, &height);
	    if (ret)
		return ret;
	}

	/*
	 * Align the Y plane to 128 bytes so the chroma planes would be aligned
	 * to 64 byte boundaries. This is an Intel HW requirement.
	 */
	if (format == DRM_FORMAT_YVU420) {
		stride = ALIGN(stride, 128);
	}

	/*
	 * HAL_PIXEL_FORMAT_YV12 requires that the buffer's height not be aligned.
	 */
	if (format == DRM_FORMAT_YVU420_ANDROID)
		height = bo->height;

	/*
	 * Ensure we pass aligned width/height.
	 */
	bo->width = width;
	bo->height = height;

	drv_bo_from_format(bo, stride, height, format);

	snprintf(name, sizeof(name), "i915-buffer-%u", i915_dev->count);
	i915_dev->count++;

	i915_bo = calloc(1, sizeof(*i915_bo));
	if (!i915_bo)
		return -ENOMEM;

	bo->priv = i915_bo;

	i915_bo->ibos[0] = drm_intel_bo_alloc(i915_dev->mgr, name, bo->total_size, 0);
	if (!i915_bo->ibos[0]) {
		fprintf(stderr, "drv: drm_intel_bo_alloc failed");
		free(i915_bo);
		bo->priv = NULL;
		return -ENOMEM;
	}

	for (plane = 0; plane < bo->num_planes; plane++) {
		if (plane > 0)
			drm_intel_bo_reference(i915_bo->ibos[0]);

		bo->handles[plane].u32 = i915_bo->ibos[0]->handle;
		i915_bo->ibos[plane] = i915_bo->ibos[0];
	}

	bo->tiling = tiling_mode;

	ret = drm_intel_bo_set_tiling(i915_bo->ibos[0], &bo->tiling, bo->strides[0]);

	if (ret || bo->tiling != tiling_mode) {
		fprintf(stderr, "drv: drm_intel_gem_bo_set_tiling failed errno=%x, stride=%x\n",
			errno, bo->strides[0]);
		/* Calls i915 bo destroy. */
		bo->drv->backend->bo_destroy(bo);
		return -errno;
	}

	return 0;
}

static int i915_bo_destroy(struct bo *bo)
{
	size_t plane;
	struct i915_bo *i915_bo = bo->priv;

	for (plane = 0; plane < bo->num_planes; plane++)
		drm_intel_bo_unreference(i915_bo->ibos[plane]);

	free(i915_bo);
	bo->priv = NULL;

	return 0;
}

static int i915_bo_import(struct bo *bo, struct drv_import_fd_data *data)
{
	size_t plane;
	uint32_t swizzling;
	struct i915_bo *i915_bo;
	struct i915_device *i915_dev = bo->drv->priv;

	i915_bo = calloc(1, sizeof(*i915_bo));
	if (!i915_bo)
		return -ENOMEM;

	bo->priv = i915_bo;

	/*
	 * When self-importing, libdrm_intel increments the reference count
	 * on the drm_intel_bo. It also returns the same drm_intel_bo per GEM
	 * handle. Thus, we don't need to increase the reference count
	 * (i.e, drv_increment_reference_count) when importing with this
	 * backend.
	 */
	for (plane = 0; plane < bo->num_planes; plane++) {

		i915_bo->ibos[plane] = drm_intel_bo_gem_create_from_prime(
		    i915_dev->mgr, data->fds[plane], data->sizes[plane]);

		if (!i915_bo->ibos[plane]) {
			/*
			 * Need to call GEM close on planes that were opened,
			 * if any. Adjust the num_planes variable to be the
			 * plane that failed, so GEM close will be called on
			 * planes before that plane.
			 */
			bo->num_planes = plane;
			i915_bo_destroy(bo);
			fprintf(stderr, "drv: i915: failed to import failed");
			return -EINVAL;
		}

		bo->handles[plane].u32 = i915_bo->ibos[plane]->handle;
	}

	if (drm_intel_bo_get_tiling(i915_bo->ibos[0], &bo->tiling, &swizzling)) {
		fprintf(stderr, "drv: drm_intel_bo_get_tiling failed");
		i915_bo_destroy(bo);
		return -EINVAL;
	}

	return 0;
}

static void *i915_bo_map(struct bo *bo, struct map_info *data, size_t plane)
{
	int ret;
	struct i915_bo *i915_bo = bo->priv;

	if (bo->tiling == I915_TILING_NONE)
		/* TODO(gsingh): use bo_map flags to determine if we should
		 * enable writing.
		 */
		ret = drm_intel_bo_map(i915_bo->ibos[0], 1);
	else
		ret = drm_intel_gem_bo_map_gtt(i915_bo->ibos[0]);

	if (ret) {
		fprintf(stderr, "drv: i915_bo_map failed.");
		return MAP_FAILED;
	}

	return i915_bo->ibos[0]->virtual;
}

static int i915_bo_unmap(struct bo *bo, struct map_info *data)
{
	int ret;
	struct i915_bo *i915_bo = bo->priv;

	if (bo->tiling == I915_TILING_NONE)
		ret = drm_intel_bo_unmap(i915_bo->ibos[0]);
	else
		ret = drm_intel_gem_bo_unmap_gtt(i915_bo->ibos[0]);

	return ret;
}

static uint32_t i915_resolve_format(uint32_t format, uint32_t flags)
{
	switch (format) {
	case DRM_FORMAT_FLEX_IMPLEMENTATION_DEFINED:
		/* Camera on Intel platforms requires NV12. */
		if ((flags & BO_USE_HW_CAMERA_WRITE) ||
				(flags & BO_USE_HW_CAMERA_READ))
			return DRM_FORMAT_NV12;

		if (flags & BO_USE_TEXTURE)
			return DRM_FORMAT_ABGR8888;

		/*HACK: See b/28671744 */
		return DRM_FORMAT_XBGR8888;
	case DRM_FORMAT_FLEX_YCbCr_420_888:
		return DRM_FORMAT_NV12;
	default:
		return format;
	}
}

struct backend backend_i915 = {
	.name = "i915",
	.init = i915_init,
	.close = i915_close,
	.bo_create = i915_bo_create,
	.bo_destroy = i915_bo_destroy,
	.bo_import = i915_bo_import,
	.bo_map = i915_bo_map,
	.bo_unmap = i915_bo_unmap,
	.resolve_format = i915_resolve_format,
};

#endif
