/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "cros_gralloc_helpers.h"

#include <cstdlib>
#include <cutils/log.h>

const char* drmFormat2Str(int drm_format)
{
    static char buf[5];
    char *pDrmFormat = (char*) &drm_format;
    snprintf(buf, sizeof(buf), "%c%c%c%c", *pDrmFormat, *(pDrmFormat + 1),
             *(pDrmFormat + 2), *(pDrmFormat + 3));
    return buf;
}

uint32_t cros_gralloc_convert_format(int format)
{
	/*
	 * Conversion from HAL to fourcc-based DRV formats based on
	 * platform_android.c in mesa.
	 */

	switch (format) {
	case HAL_PIXEL_FORMAT_BGRA_8888:
		return DRM_FORMAT_ARGB8888;
	case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
		return DRM_FORMAT_FLEX_IMPLEMENTATION_DEFINED;
	case HAL_PIXEL_FORMAT_RGB_565:
		return DRM_FORMAT_RGB565;
	case HAL_PIXEL_FORMAT_RGB_888:
		return DRM_FORMAT_RGB888;
	case HAL_PIXEL_FORMAT_RGBA_8888:
		return DRM_FORMAT_ABGR8888;
	case HAL_PIXEL_FORMAT_RGBX_8888:
		return DRM_FORMAT_XBGR8888;
	case HAL_PIXEL_FORMAT_YCbCr_420_888:
		return DRM_FORMAT_FLEX_YCbCr_420_888;
	case HAL_PIXEL_FORMAT_YV12:
		return DRM_FORMAT_YVU420_ANDROID;
	case HAL_PIXEL_FORMAT_NV12:
		return DRM_FORMAT_NV12;
	/*
	 * Choose DRM_FORMAT_R8 because <system/graphics.h> requires the buffers
	 * with a format HAL_PIXEL_FORMAT_BLOB have a height of 1, and width
	 * equal to their size in bytes.
	 */
	case HAL_PIXEL_FORMAT_BLOB:
		return DRM_FORMAT_R8;
	}

	return DRM_FORMAT_NONE;
}

int32_t cros_gralloc_invert_format(int format)
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
        case DRM_FORMAT_NV12:
                return HAL_PIXEL_FORMAT_NV12;
	case DRM_FORMAT_FLEX_YCbCr_420_888:
		return HAL_PIXEL_FORMAT_YCbCr_420_888;
	case DRM_FORMAT_YVU420_ANDROID:
		return HAL_PIXEL_FORMAT_YV12;
        case DRM_FORMAT_R8:
                return HAL_PIXEL_FORMAT_BLOB;
        default:
                cros_gralloc_error("Unhandled DRM format %4.4s",
                                   drmFormat2Str(format));
                return 0;
        }
}

cros_gralloc_handle_t cros_gralloc_convert_handle(buffer_handle_t handle)
{
	auto hnd = reinterpret_cast<cros_gralloc_handle_t>(handle);
	if (!hnd || hnd->magic != cros_gralloc_magic)
		return nullptr;

	return hnd;
}

void cros_gralloc_log(const char *prefix, const char *file, int line, const char *format, ...)
{
	char buf[50];
	snprintf(buf, sizeof(buf), "[%s:%s(%d)]", prefix, basename(file), line);

	va_list args;
	va_start(args, format);
	__android_log_vprint(ANDROID_LOG_ERROR, buf, format, args);
	va_end(args);
}
