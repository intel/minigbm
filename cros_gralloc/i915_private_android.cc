/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifdef DRV_I915

#include "i915_private_android.h"
#include "i915_private_android_types.h"

#include "cros_gralloc_helpers.h"

#include <hardware/gralloc.h>

uint32_t i915_private_convert_format(int format)
{
	switch (format) {
	case HAL_PIXEL_FORMAT_NV12:
		return DRM_FORMAT_NV12;
	case HAL_PIXEL_FORMAT_NV12_Y_TILED_INTEL:
		return DRM_FORMAT_NV12_Y_TILED_INTEL;
	case HAL_PIXEL_FORMAT_YCbCr_422_I:
		return DRM_FORMAT_YUYV;
	case HAL_PIXEL_FORMAT_Y16:
		return DRM_FORMAT_R16;
	case HAL_PIXEL_FORMAT_YCbCr_444_888:
		return DRM_FORMAT_YUV444;
	case HAL_PIXEL_FORMAT_YCrCb_420_SP:
		return DRM_FORMAT_NV21;
	case HAL_PIXEL_FORMAT_YCbCr_422_SP:
		return DRM_FORMAT_NV16;
	case HAL_PIXEL_FORMAT_YCbCr_422_888:
		return DRM_FORMAT_YUV422;
	case HAL_PIXEL_FORMAT_P010_INTEL:
		return DRM_FORMAT_P010_INTEL;
	}

	return DRM_FORMAT_NONE;
}

int32_t i915_private_invert_format(int format)
{
	/* Convert the DRM FourCC into the most specific HAL pixel format. */
	switch (format) {
	case DRM_FORMAT_ARGB8888:
		return HAL_PIXEL_FORMAT_BGRA_8888;
	case DRM_FORMAT_RGB565:
		return HAL_PIXEL_FORMAT_RGB_565;
	case DRM_FORMAT_RGB888:
		return HAL_PIXEL_FORMAT_RGB_888;
	case DRM_FORMAT_ABGR8888:
		return HAL_PIXEL_FORMAT_RGBA_8888;
	case DRM_FORMAT_XBGR8888:
		return HAL_PIXEL_FORMAT_RGBX_8888;
	case DRM_FORMAT_FLEX_YCbCr_420_888:
		return HAL_PIXEL_FORMAT_YCbCr_420_888;
	case DRM_FORMAT_YVU420_ANDROID:
		return HAL_PIXEL_FORMAT_YV12;
	case DRM_FORMAT_R8:
		return HAL_PIXEL_FORMAT_BLOB;
	case DRM_FORMAT_NV12:
		return HAL_PIXEL_FORMAT_NV12;
	case DRM_FORMAT_NV12_Y_TILED_INTEL:
		return HAL_PIXEL_FORMAT_NV12_Y_TILED_INTEL;
	case DRM_FORMAT_YUYV:
		return HAL_PIXEL_FORMAT_YCbCr_422_I;
	case DRM_FORMAT_R16:
		return HAL_PIXEL_FORMAT_Y16;
	case DRM_FORMAT_P010_INTEL:
		return HAL_PIXEL_FORMAT_P010_INTEL;
	case DRM_FORMAT_YUV444:
		return HAL_PIXEL_FORMAT_YCbCr_444_888;
	case DRM_FORMAT_NV21:
		return HAL_PIXEL_FORMAT_YCrCb_420_SP;
	case DRM_FORMAT_NV16:
		return HAL_PIXEL_FORMAT_YCbCr_422_SP;
	case DRM_FORMAT_YUV422:
		return HAL_PIXEL_FORMAT_YCbCr_422_888;
	default:
		cros_gralloc_error("Unhandled DRM format %4.4s", drmFormat2Str(format));
	}

	return 0;
}

bool i915_private_supported_yuv_format(uint32_t droid_format)
{
	switch (droid_format) {
	case HAL_PIXEL_FORMAT_NV12:
	case HAL_PIXEL_FORMAT_NV12_Y_TILED_INTEL:
	case HAL_PIXEL_FORMAT_YCbCr_422_I:
	case HAL_PIXEL_FORMAT_YCbCr_422_888:
	case HAL_PIXEL_FORMAT_YCbCr_444_888:
	case HAL_PIXEL_FORMAT_YCrCb_420_SP:
	case HAL_PIXEL_FORMAT_Y16:
	case HAL_PIXEL_FORMAT_P010_INTEL:
		return true;
	default:
		return false;
	}

	return false;
}

#endif
