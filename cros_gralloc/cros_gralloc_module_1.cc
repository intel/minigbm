/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "cros_gralloc.h"

#include <hardware/hardware.h>
#include <hardware/gralloc1.h>
#include <sync/sync.h>

#include <sys/mman.h>
#include <xf86drm.h>
#include <assert.h>

int cros_gralloc_validate_reference(struct cros_gralloc_module *mod,
				    struct cros_gralloc_handle *hnd, struct cros_gralloc_bo **bo)
{
	if (!mod->handles.count(hnd))
		return CROS_GRALLOC_ERROR_BAD_HANDLE;

	*bo = mod->handles[hnd].bo;
	return CROS_GRALLOC_ERROR_NONE;
}

int cros_gralloc_decrement_reference_count(struct cros_gralloc_module *mod,
					   struct cros_gralloc_bo *bo)
{
	if (bo->refcount <= 0) {
		cros_gralloc_error("The reference count is <= 0.");
		assert(0);
	}

	if (!--bo->refcount) {
		mod->buffers.erase(drv_bo_get_plane_handle(bo->bo, 0).u32);
		drv_bo_destroy(bo->bo);

		if (bo->hnd) {
			mod->handles.erase(bo->hnd);
			native_handle_close(&bo->hnd->base);
			delete bo->hnd;
		}

		delete bo;
	}

	return CROS_GRALLOC_ERROR_NONE;
}

static int cros_gralloc_register_buffer(gralloc1_device_t* device,
					buffer_handle_t handle)
{
	uint32_t id;
	struct cros_gralloc_bo *bo;
	auto hnd = (struct cros_gralloc_handle *)handle;
	auto mod = (struct cros_gralloc_module *)device->common.module;
	ScopedSpinLock lock(mod->lock);

	if (cros_gralloc_validate_handle(hnd)) {
		cros_gralloc_error("Invalid handle.");
		return CROS_GRALLOC_ERROR_BAD_HANDLE;
	}

	if (!mod->drv) {
		if (cros_gralloc_rendernode_open(&mod->drv)) {
			cros_gralloc_error("Failed to open render node.");
			return CROS_GRALLOC_ERROR_NO_RESOURCES;
		}
	}

	if (!cros_gralloc_validate_reference(mod, hnd, &bo)) {
		bo->refcount++;
		mod->handles[hnd].registrations++;
		return CROS_GRALLOC_ERROR_NONE;
	}

	if (drmPrimeFDToHandle(drv_get_fd(mod->drv), hnd->fds[0], &id)) {
		cros_gralloc_error("drmPrimeFDToHandle failed.");
		return CROS_GRALLOC_ERROR_BAD_HANDLE;
	}

	if (mod->buffers.count(id)) {
		bo = mod->buffers[id];
		bo->refcount++;
	} else {
		struct drv_import_fd_data data;
		data.format = hnd->format;
		data.width = hnd->width;
		data.height = hnd->height;

		memcpy(data.fds, hnd->fds, sizeof(data.fds));
		memcpy(data.strides, hnd->strides, sizeof(data.strides));
		memcpy(data.offsets, hnd->offsets, sizeof(data.offsets));
		memcpy(data.sizes, hnd->sizes, sizeof(data.sizes));
		for (uint32_t p = 0; p < DRV_MAX_PLANES; p++) {
			data.format_modifiers[p] =
			    static_cast<uint64_t>(hnd->format_modifiers[2 * p]) << 32;
			data.format_modifiers[p] |= hnd->format_modifiers[2 * p + 1];
		}

		bo = new cros_gralloc_bo();
		bo->bo = drv_bo_import(mod->drv, &data);
		if (!bo->bo) {
			delete bo;
			return CROS_GRALLOC_ERROR_NO_RESOURCES;
		}

		id = drv_bo_get_plane_handle(bo->bo, 0).u32;
		mod->buffers[id] = bo;

		bo->refcount = 1;
	}

	mod->handles[hnd].bo = bo;
	mod->handles[hnd].registrations = 1;

	return CROS_GRALLOC_ERROR_NONE;
}

static int cros_gralloc_unregister_buffer(gralloc1_device_t* device,
					  buffer_handle_t handle)
{
	struct cros_gralloc_bo *bo;
	auto hnd = (struct cros_gralloc_handle *)handle;
	auto mod = (struct cros_gralloc_module *)device->common.module;
	ScopedSpinLock lock(mod->lock);

	if (cros_gralloc_validate_handle(hnd)) {
		cros_gralloc_error("Invalid handle.");
		return CROS_GRALLOC_ERROR_BAD_HANDLE;
	}

	if (cros_gralloc_validate_reference(mod, hnd, &bo)) {
		cros_gralloc_error("Invalid Reference.");
		return CROS_GRALLOC_ERROR_BAD_HANDLE;
	}

	if (mod->handles[hnd].registrations <= 0) {
		cros_gralloc_error("Handle not registered.");
		return CROS_GRALLOC_ERROR_BAD_HANDLE;
	}

	mod->handles[hnd].registrations--;

	if (!mod->handles[hnd].registrations)
		mod->handles.erase(hnd);

	return cros_gralloc_decrement_reference_count(mod, bo);
}

static int cros_gralloc_lock(gralloc1_device_t* device, buffer_handle_t handle,
			     int l, int t, int w, int h, void **vaddr)
{
	struct cros_gralloc_bo *bo;
	auto mod = (struct cros_gralloc_module *)device->common.module;
	auto hnd = (struct cros_gralloc_handle *)handle;
	ScopedSpinLock lock(mod->lock);

	if (cros_gralloc_validate_handle(hnd)) {
		cros_gralloc_error("Invalid handle.");
		return CROS_GRALLOC_ERROR_BAD_HANDLE;
	}

	if (cros_gralloc_validate_reference(mod, hnd, &bo)) {
		cros_gralloc_error("Invalid Reference.");
		return CROS_GRALLOC_ERROR_BAD_HANDLE;
	}

	if (hnd->droid_format == HAL_PIXEL_FORMAT_YCbCr_420_888) {
		cros_gralloc_error("HAL_PIXEL_FORMAT_YCbCr_*_888 format not compatible.");
		return CROS_GRALLOC_ERROR_BAD_HANDLE;
	}

	if (bo->map_data) {
		*vaddr = bo->map_data->addr;
	} else {
		*vaddr = drv_bo_map(bo->bo, 0, 0, drv_bo_get_width(bo->bo),
				    drv_bo_get_height(bo->bo), 0, &bo->map_data, 0);
	}

	if (*vaddr == MAP_FAILED) {
		cros_gralloc_error("Mapping failed.");
		return CROS_GRALLOC_ERROR_UNSUPPORTED;
	}

	bo->lockcount++;

	return CROS_GRALLOC_ERROR_NONE;
}

static int cros_gralloc_unlock(struct gralloc_module_t const *module, buffer_handle_t handle)
{
	struct cros_gralloc_bo *bo;
	auto hnd = (struct cros_gralloc_handle *)handle;
	auto mod = (struct cros_gralloc_module *)module;
	ScopedSpinLock lock(mod->lock);

	if (cros_gralloc_validate_handle(hnd)) {
		cros_gralloc_error("Invalid handle.");
		return CROS_GRALLOC_ERROR_BAD_HANDLE;
	}

	if (cros_gralloc_validate_reference(mod, hnd, &bo)) {
		cros_gralloc_error("Invalid Reference.");
		return CROS_GRALLOC_ERROR_BAD_HANDLE;
	}

	if (!--bo->lockcount && bo->map_data) {
		drv_bo_unmap(bo->bo, bo->map_data);
		bo->map_data = NULL;
	}

	return CROS_GRALLOC_ERROR_NONE;
}

static int cros_gralloc_lock_ycbcr(struct gralloc_module_t const *module, buffer_handle_t handle,
				   int usage, int l, int t, int w, int h,
				   struct android_ycbcr *ycbcr)
{
	uint8_t *addr = NULL;
	size_t offsets[DRV_MAX_PLANES];
	struct cros_gralloc_bo *bo;
	auto hnd = (struct cros_gralloc_handle *)handle;
	auto mod = (struct cros_gralloc_module *)module;
	ScopedSpinLock lock(mod->lock);

	if (cros_gralloc_validate_handle(hnd)) {
		cros_gralloc_error("Invalid handle.");
		return CROS_GRALLOC_ERROR_BAD_HANDLE;
	}

	if (cros_gralloc_validate_reference(mod, hnd, &bo)) {
		cros_gralloc_error("Invalid Reference.");
		return CROS_GRALLOC_ERROR_BAD_HANDLE;
	}

	if ((hnd->droid_format != HAL_PIXEL_FORMAT_YCbCr_420_888) &&
	    (hnd->droid_format != HAL_PIXEL_FORMAT_YV12)) {
		cros_gralloc_error("Non-YUV format not compatible.");
		return CROS_GRALLOC_ERROR_BAD_HANDLE;
	}

	if (sw_access & usage) {
		void *vaddr;
		if (bo->map_data) {
			vaddr = bo->map_data->addr;
		} else {
			vaddr = drv_bo_map(bo->bo, 0, 0, drv_bo_get_width(bo->bo),
					   drv_bo_get_height(bo->bo), 0, &bo->map_data, 0);
		}

		if (vaddr == MAP_FAILED) {
			cros_gralloc_error("Mapping failed.");
			return CROS_GRALLOC_ERROR_UNSUPPORTED;
		}

		addr = static_cast<uint8_t *>(vaddr);
	}

	for (size_t p = 0; p < drv_bo_get_num_planes(bo->bo); p++)
		offsets[p] = drv_bo_get_plane_offset(bo->bo, p);

	switch (hnd->format) {
	case DRM_FORMAT_NV12:
		ycbcr->y = addr;
		ycbcr->cb = addr + offsets[1];
		ycbcr->cr = addr + offsets[1] + 1;
		ycbcr->ystride = drv_bo_get_plane_stride(bo->bo, 0);
		ycbcr->cstride = drv_bo_get_plane_stride(bo->bo, 1);
		ycbcr->chroma_step = 2;
		break;
	case DRM_FORMAT_YVU420_ANDROID:
		ycbcr->y = addr;
		ycbcr->cb = addr + offsets[2];
		ycbcr->cr = addr + offsets[1];
		ycbcr->ystride = drv_bo_get_plane_stride(bo->bo, 0);
		ycbcr->cstride = drv_bo_get_plane_stride(bo->bo, 1);
		ycbcr->chroma_step = 1;
		break;
	case DRM_FORMAT_UYVY:
		ycbcr->y = addr + 1;
		ycbcr->cb = addr;
		ycbcr->cr = addr + 2;
		ycbcr->ystride = drv_bo_get_plane_stride(bo->bo, 0);
		ycbcr->cstride = drv_bo_get_plane_stride(bo->bo, 0);
		ycbcr->chroma_step = 2;
		break;
	default:
		return CROS_GRALLOC_ERROR_UNSUPPORTED;
	}

	bo->lockcount++;

	return CROS_GRALLOC_ERROR_NONE;
}

static void cros_gralloc1_dump(gralloc1_device_t* /*device*/,
	uint32_t* /*outSize*/, char* /*outBuffer*/)
{
}

static int32_t cros_gralloc1_create_descriptor(gralloc1_device_t* /*device*/,
	gralloc1_buffer_descriptor_t* outDescriptor)
{
    if (!outDescriptor)
	    return GRALLOC1_ERROR_BAD_DESCRIPTOR;

    struct cros_gralloc_descriptor *hnd = new cros_gralloc_descriptor();
    *outDescriptor = (gralloc1_buffer_descriptor_t)hnd;
    return GRALLOC1_ERROR_NONE;
}

static int32_t cros_gralloc1_destroy_descriptor(gralloc1_device_t* /*device*/,
	gralloc1_buffer_descriptor_t descriptor)
{
	auto hnd = (struct cros_gralloc_descriptor *)descriptor;
	delete hnd;
	return GRALLOC1_ERROR_NONE;
}

static int32_t cros_gralloc1_set_consumer_usage(gralloc1_device_t* /*device*/,
	gralloc1_buffer_descriptor_t descriptor, uint64_t usage)
{
	auto hnd = (struct cros_gralloc_descriptor *)descriptor;
	hnd->consumer_usage = usage;
	return GRALLOC1_ERROR_NONE;
}

static int32_t cros_gralloc1_set_dimensions(gralloc1_device_t* /*device*/,
	gralloc1_buffer_descriptor_t descriptor, uint32_t width, uint32_t height)
{
	auto hnd = (struct cros_gralloc_descriptor *)descriptor;
	hnd->width = width;
	hnd->height = height;
	return GRALLOC1_ERROR_NONE;
}

static int32_t cros_gralloc1_set_format(gralloc1_device_t* /*device*/,
	gralloc1_buffer_descriptor_t descriptor, int32_t /*android_pixel_format_t*/ format)
{
	auto hnd = (struct cros_gralloc_descriptor *)descriptor;
	hnd->droid_format = format;
	return GRALLOC1_ERROR_NONE;
}

static int32_t cros_gralloc1_set_producer_usage(gralloc1_device_t* device,
	gralloc1_buffer_descriptor_t descriptor, uint64_t /*gralloc1_producer_usage_t*/ usage)
{
	auto hnd = (struct cros_gralloc_descriptor *)descriptor;
	hnd->producer_usage = usage;
	return GRALLOC1_ERROR_NONE;
}

static struct cros_gralloc_handle * get_gralloc_handle(gralloc1_device_t* device, buffer_handle_t buffer)
{
	struct cros_gralloc_bo *bo;
	auto mod = (struct cros_gralloc_module *)device->common.module;
	auto hnd = (struct cros_gralloc_handle *)buffer;
	if (cros_gralloc_validate_handle(hnd)) {
	    cros_gralloc_error("Invalid handle.");
	    return nullptr;
	}

	if (cros_gralloc_validate_reference(mod, hnd, &bo)) {
	    cros_gralloc_error("Invalid Reference.");
	    return nullptr;
	}

	return hnd;
}

static int32_t cros_gralloc1_get_backing_store(gralloc1_device_t* device,
	buffer_handle_t buffer, gralloc1_backing_store_t* outStore)
{
	struct cros_gralloc_bo *bo;
	auto mod = (struct cros_gralloc_module *)device->common.module;
	auto hnd = (struct cros_gralloc_handle *)buffer;
	if (cros_gralloc_validate_handle(hnd)) {
	    cros_gralloc_error("Invalid handle.");
	    return GRALLOC1_ERROR_BAD_HANDLE;
	}

	if (cros_gralloc_validate_reference(mod, hnd, &bo)) {
	    cros_gralloc_error("Invalid Reference.");
	    return GRALLOC1_ERROR_BAD_HANDLE;
	}

	*outStore = drv_bo_get_plane_handle(bo->bo, 0).u64;

	return GRALLOC1_ERROR_NONE;
}

static int32_t cros_gralloc1_get_consumer_usage(gralloc1_device_t* device,
	buffer_handle_t buffer, uint64_t* /*gralloc1_consumer_usage_t*/ outUsage)
{
	auto hnd = get_gralloc_handle(device, buffer);
	if (!hnd) {
	    return GRALLOC1_ERROR_BAD_HANDLE;
	}

	*outUsage = hnd->consumer_usage;
	return GRALLOC1_ERROR_NONE;
}

static int32_t cros_gralloc1_get_dimensions(gralloc1_device_t* device,
	buffer_handle_t buffer, uint32_t* outWidth, uint32_t* outHeight)
{
	auto hnd = get_gralloc_handle(device, buffer);
	if (!hnd) {
	    return GRALLOC1_ERROR_BAD_HANDLE;
	}

	*outWidth = hnd->width;
	*outHeight = hnd->height;
	return GRALLOC1_ERROR_NONE;
}

static int32_t cros_gralloc1_get_format(gralloc1_device_t* device,
	buffer_handle_t buffer, int32_t* outFormat)
{
	auto hnd = get_gralloc_handle(device, buffer);
	if (!hnd) {
	    return GRALLOC1_ERROR_BAD_HANDLE;
	}

	*outFormat = hnd->droid_format;
	return GRALLOC1_ERROR_NONE;
}

static int32_t cros_gralloc1_get_producer_usage(gralloc1_device_t* device,
	buffer_handle_t buffer, uint64_t* /*gralloc1_producer_usage_t*/ outUsage)
{
	auto hnd = get_gralloc_handle(device, buffer);
	*outUsage = hnd->producer_usage;
	if (!hnd) {
	    return GRALLOC1_ERROR_BAD_HANDLE;
	}

	return GRALLOC1_ERROR_NONE;
}

static int32_t cros_gralloc1_get_stride(gralloc1_device_t* device,
	buffer_handle_t buffer, uint32_t* outStride)
{
	auto hnd = get_gralloc_handle(device, buffer);
	if (!hnd) {
	    return GRALLOC1_ERROR_BAD_HANDLE;
	}

	*outStride = hnd->pixel_stride;
	return GRALLOC1_ERROR_NONE;
}

static int32_t cros_gralloc1_allocate(gralloc1_device_t* device,
	uint32_t numDescriptors, const gralloc1_buffer_descriptor_t* descriptors,
	buffer_handle_t* outBuffers)
{
	if (numDescriptors !=  1) {
	    return GRALLOC1_ERROR_UNSUPPORTED;
	}

	auto hnd = (struct cros_gralloc_descriptor *)*descriptors;
	auto mod = (struct cros_gralloc_module *)device->common.module;
	uint64_t drv_usage = cros_gralloc1_convert_flags(hnd->producer_usage, hnd->consumer_usage);

	int err = cros_gralloc1_alloc(mod, hnd->width, hnd->height,
				      hnd->droid_format,
				      drv_usage,
				      hnd->consumer_usage & GRALLOC1_CONSUMER_USAGE_HWCOMPOSER,
				      outBuffers);
	if (err == CROS_GRALLOC_ERROR_NONE) {
	    struct cros_gralloc_handle* handle = (struct cros_gralloc_handle*)outBuffers;
	    handle->consumer_usage = hnd->consumer_usage;
	    handle->producer_usage = hnd->producer_usage;
	    handle->usage = 0;
	    return GRALLOC1_ERROR_NONE;
	}

	return GRALLOC1_ERROR_UNSUPPORTED;
}

static int32_t cros_gralloc1_retain(gralloc1_device_t* device,
	buffer_handle_t buffer)
{
	if (cros_gralloc_register_buffer(device, buffer))
	{
	    return GRALLOC1_ERROR_BAD_HANDLE;
	}

	return GRALLOC1_ERROR_NONE;
}

static int32_t cros_gralloc1_release(gralloc1_device_t* device,
	buffer_handle_t buffer)
{
	if (cros_gralloc_unregister_buffer(device, buffer))
	{
	    return GRALLOC1_ERROR_BAD_HANDLE;
	}

	return GRALLOC1_ERROR_NONE;
}

static int32_t cros_gralloc1_get_num_flex_planes(gralloc1_device_t* device,
	buffer_handle_t buffer, uint32_t* outNumPlanes)
{
	struct cros_gralloc_bo *bo;
	auto mod = (struct cros_gralloc_module *)device->common.module;
	auto hnd = (struct cros_gralloc_handle *)buffer;
	if (cros_gralloc_validate_handle(hnd)) {
	    cros_gralloc_error("Invalid handle.");
	    return GRALLOC1_ERROR_BAD_HANDLE;
	}

	if (cros_gralloc_validate_reference(mod, hnd, &bo)) {
	    cros_gralloc_error("Invalid Reference.");
	    return GRALLOC1_ERROR_BAD_HANDLE;
	}

	*outNumPlanes = drv_bo_get_num_planes(bo->bo);
	return GRALLOC1_ERROR_NONE;
}

static int32_t validate_lock(gralloc1_device_t* device,
	buffer_handle_t buffer,
	uint64_t /*gralloc1_producer_usage_t*/ producerUsage,
	uint64_t /*gralloc1_consumer_usage_t*/ consumerUsage,
	int32_t acquireFence)
{
	if (producerUsage == GRALLOC1_PRODUCER_USAGE_NONE &&
		(consumerUsage == GRALLOC1_PRODUCER_USAGE_NONE)) {
	    return GRALLOC1_ERROR_BAD_VALUE;
	}

	if (producerUsage != GRALLOC1_PRODUCER_USAGE_NONE &&
		(consumerUsage != GRALLOC1_PRODUCER_USAGE_NONE)) {
            cros_gralloc_error("Buffer usage set for both producer and consumer");
	    //return GRALLOC1_ERROR_BAD_VALUE;
	}

	auto hnd = get_gralloc_handle(device, buffer);
	if (!hnd) {
	    return GRALLOC1_ERROR_BAD_HANDLE;
	}

	uint64_t usage = producerUsage | consumerUsage;
	bool cpu_usage = false;

	if (producerUsage == GRALLOC1_PRODUCER_USAGE_NONE) {
	    if (usage & GRALLOC1_CONSUMER_USAGE_CPU_READ) {
		cpu_usage = true;
	    } else if (usage & GRALLOC1_CONSUMER_USAGE_CPU_READ_OFTEN) {
		cpu_usage = true;
	    }
	} else {
	    if (usage & GRALLOC1_PRODUCER_USAGE_CPU_READ) {
		cpu_usage = true;
	    } else if (usage & GRALLOC1_PRODUCER_USAGE_CPU_READ_OFTEN) {
		cpu_usage = true;
	    } else if (usage & GRALLOC1_PRODUCER_USAGE_CPU_WRITE) {
		cpu_usage = true;
	    } else if (usage & GRALLOC1_PRODUCER_USAGE_CPU_WRITE_OFTEN) {
		cpu_usage = true;
	    }
	}

	if (!cpu_usage) {
	    return GRALLOC1_ERROR_BAD_VALUE;
	}

	if (acquireFence >= 0) {
	    sync_wait(acquireFence, -1);
	    close(acquireFence);
	}

	return GRALLOC1_ERROR_NONE;
}

static int32_t cros_gralloc1_lock(gralloc1_device_t* device,
	buffer_handle_t buffer,
	uint64_t /*gralloc1_producer_usage_t*/ producerUsage,
	uint64_t /*gralloc1_consumer_usage_t*/ consumerUsage,
	const gralloc1_rect_t* accessRegion, void** outData,
	int32_t acquireFence)
{
    int32_t ret = validate_lock(device, buffer,
				producerUsage, consumerUsage,
				acquireFence);

    if (ret != GRALLOC1_ERROR_NONE)
	return ret;

    if (cros_gralloc_lock(device, buffer, accessRegion->left,
			  accessRegion->top, accessRegion->width,
			  accessRegion->height,
			  outData)) {
	return GRALLOC1_ERROR_UNSUPPORTED;
    }

    return GRALLOC1_ERROR_NONE;
}

static int32_t cros_gralloc1_lock_flex(gralloc1_device_t* device,
	buffer_handle_t buffer,
	uint64_t /*gralloc1_producer_usage_t*/ producerUsage,
	uint64_t /*gralloc1_consumer_usage_t*/ consumerUsage,
	const gralloc1_rect_t* accessRegion,
	struct android_flex_layout* outFlexLayout, int32_t acquireFence)
{
	int32_t ret = validate_lock(device, buffer,
				    producerUsage, consumerUsage,
				    acquireFence);

	if (ret != GRALLOC1_ERROR_NONE)
	    return ret;

	return GRALLOC1_ERROR_NONE;
}

static int32_t cros_gralloc1_unlock(gralloc1_device_t* /*device*/,
	buffer_handle_t /*buffer*/, int32_t* /*outReleaseFence*/)
{
	return GRALLOC1_ERROR_NONE;
}

static int cros_gralloc1_close(struct hw_device_t *dev)
{
	auto mod = (struct cros_gralloc_module *)dev->module;
	auto device = (struct gralloc1_device *)dev;
	ScopedSpinLock lock(mod->lock);

	if (mod->drv) {
		drv_destroy(mod->drv);
		mod->drv = NULL;
	}

	mod->buffers.clear();
	mod->handles.clear();

	delete device;

	return CROS_GRALLOC_ERROR_NONE;
}

static void cros_gralloc1_getCapabilities(gralloc1_device_t */*dev*/, uint32_t *outCount,
					int32_t */*outCapabilities*/)
{
	if (outCount != NULL)
	{
		*outCount = 0;
	}
}

static gralloc1_function_pointer_t cros_gralloc1_getFunction(gralloc1_device_t */*dev*/, int32_t descriptor)
{
	auto func = static_cast<gralloc1_function_descriptor_t>(descriptor);
	switch (func) {
	    case GRALLOC1_FUNCTION_DUMP:
		return (gralloc1_function_pointer_t)cros_gralloc1_dump;

	    case GRALLOC1_FUNCTION_CREATE_DESCRIPTOR:
		return (gralloc1_function_pointer_t)cros_gralloc1_create_descriptor;

	    case GRALLOC1_FUNCTION_DESTROY_DESCRIPTOR:
		return (gralloc1_function_pointer_t)cros_gralloc1_destroy_descriptor;

	    case GRALLOC1_FUNCTION_SET_CONSUMER_USAGE:
		return (gralloc1_function_pointer_t)cros_gralloc1_set_consumer_usage;

	    case GRALLOC1_FUNCTION_SET_DIMENSIONS:
		return (gralloc1_function_pointer_t)cros_gralloc1_set_dimensions;

	    case GRALLOC1_FUNCTION_SET_FORMAT:
		return (gralloc1_function_pointer_t)cros_gralloc1_set_format;

	    case GRALLOC1_FUNCTION_SET_PRODUCER_USAGE:
		return (gralloc1_function_pointer_t)cros_gralloc1_set_producer_usage;

	    case GRALLOC1_FUNCTION_GET_BACKING_STORE:
		return (gralloc1_function_pointer_t)cros_gralloc1_get_backing_store;

	    case GRALLOC1_FUNCTION_GET_CONSUMER_USAGE:
		return (gralloc1_function_pointer_t)cros_gralloc1_get_consumer_usage;

	    case GRALLOC1_FUNCTION_GET_DIMENSIONS:
		return (gralloc1_function_pointer_t)cros_gralloc1_get_dimensions;

	    case GRALLOC1_FUNCTION_GET_FORMAT:
		return (gralloc1_function_pointer_t)cros_gralloc1_get_format;

	    case GRALLOC1_FUNCTION_GET_PRODUCER_USAGE:
		return (gralloc1_function_pointer_t)cros_gralloc1_get_producer_usage;

	    case GRALLOC1_FUNCTION_GET_STRIDE:
		return (gralloc1_function_pointer_t)cros_gralloc1_get_stride;

	    case GRALLOC1_FUNCTION_ALLOCATE:
		return (gralloc1_function_pointer_t)cros_gralloc1_allocate;

	    case GRALLOC1_FUNCTION_RETAIN:
		return (gralloc1_function_pointer_t)cros_gralloc1_retain;

	    case GRALLOC1_FUNCTION_RELEASE:
		return (gralloc1_function_pointer_t)cros_gralloc1_release;

	    case GRALLOC1_FUNCTION_GET_NUM_FLEX_PLANES:
		return (gralloc1_function_pointer_t)cros_gralloc1_get_num_flex_planes;

	    case GRALLOC1_FUNCTION_LOCK:
		return (gralloc1_function_pointer_t)cros_gralloc1_lock;

	    case GRALLOC1_FUNCTION_LOCK_FLEX:
		return (gralloc1_function_pointer_t)cros_gralloc1_lock_flex;

	    case GRALLOC1_FUNCTION_UNLOCK:
		return (gralloc1_function_pointer_t)cros_gralloc1_unlock;
	    default:
		return nullptr;
	}

	return nullptr;
}

static int cros_gralloc1_open(const struct hw_module_t *mod, const char *name, struct hw_device_t **device)
{
	auto module = (struct cros_gralloc_module *)mod;
	ScopedSpinLock lock(module->lock);

	if (!module->drv) {

		if (strcmp(name, GRALLOC_HARDWARE_MODULE_ID)) {
			cros_gralloc_error("Incorrect device name - %s.", name);
			return CROS_GRALLOC_ERROR_UNSUPPORTED;
		}

		if (cros_gralloc_rendernode_open(&module->drv)) {
			cros_gralloc_error("Failed to open render node.");
			return CROS_GRALLOC_ERROR_NO_RESOURCES;
		}
	}
	gralloc1_device_t *dev = new gralloc1_device_t();
	dev->common.tag = HARDWARE_DEVICE_TAG;
	dev->common.version = 0;
	dev->common.module = (hw_module_t *)mod;
	dev->common.close = cros_gralloc1_close;
	dev->getCapabilities = cros_gralloc1_getCapabilities;
	dev->getFunction = cros_gralloc1_getFunction;

	*device = &dev->common;

	return CROS_GRALLOC_ERROR_NONE;
}

static struct hw_module_methods_t cros_gralloc_module_methods = {.open = cros_gralloc1_open };

struct cros_gralloc_module HAL_MODULE_INFO_SYM = {
	.base =
	    {
		.common =
		    {
			.tag = HARDWARE_MODULE_TAG,
			.module_api_version = HARDWARE_MODULE_API_VERSION(1, 0),
			.hal_api_version = 0,
			.id = GRALLOC_HARDWARE_MODULE_ID,
			.name = "Gralloc module",
			.author = "Chrome OS",
			.methods = &cros_gralloc_module_methods,
		    },
	    },

	.drv = NULL,
};
