/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "cros_gralloc_driver.h"
#include "../util.h"

#include <cstdlib>
#include <fcntl.h>
#include <xf86drm.h>

cros_gralloc_driver::cros_gralloc_driver() : _drv(nullptr)
{
}

cros_gralloc_driver::~cros_gralloc_driver()
{
	_buffers.clear();
	_handles.clear();

	if (_drv) {
		drv_destroy(_drv);
		_drv = nullptr;
	}
}

int32_t cros_gralloc_driver::init()
{
	/*
	 * Create a driver from rendernode while filtering out
	 * the specified undesired driver.
	 *
	 * TODO(gsingh): Enable render nodes on udl/evdi.
	 */

	int fd;
	drmVersionPtr version;
	char const *str = "%s/renderD%d";
	const char *undesired[2] = { "vgem", nullptr };
	uint32_t num_nodes = 63;
	uint32_t min_node = 128;
	uint32_t max_node = (min_node + num_nodes);

	for (uint32_t i = 0; i < ARRAY_SIZE(undesired); i++) {
		for (uint32_t j = min_node; j < max_node; j++) {
			char *node;
			if (asprintf(&node, str, DRM_DIR_NAME, j) < 0)
				continue;

			fd = open(node, O_RDWR, 0);
			free(node);

			if (fd < 0)
				continue;

			version = drmGetVersion(fd);
			if (!version)
				continue;

			if (undesired[i] && !strcmp(version->name, undesired[i])) {
				drmFreeVersion(version);
				continue;
			}

			drmFreeVersion(version);
			_drv = drv_create(fd);
			if (_drv)
				return CROS_GRALLOC_ERROR_NONE;
		}
	}

	return CROS_GRALLOC_ERROR_NO_RESOURCES;
}

bool cros_gralloc_driver::is_supported(const struct cros_gralloc_buffer_descriptor *descriptor)
{
	struct combination *combo;
	combo = drv_get_combination(_drv, drv_resolve_format(_drv, descriptor->drm_format),
				    descriptor->drv_usage);
	return (combo != nullptr);
}

int32_t cros_gralloc_driver::allocate(const struct cros_gralloc_buffer_descriptor *descriptor,
				      buffer_handle_t *out_handle)
{
	uint32_t id;
	uint64_t mod;
	size_t num_planes;

	struct bo *bo;
	struct cros_gralloc_handle *hnd;

	bo = drv_bo_create(_drv, descriptor->width, descriptor->height,
			   drv_resolve_format(_drv, descriptor->drm_format), descriptor->drv_usage);
	if (!bo) {
		cros_gralloc_error("Failed to create bo.");
		return CROS_GRALLOC_ERROR_NO_RESOURCES;
	}

	/*
	 * If there is a desire for more than one kernel buffer, this can be
	 * removed once the ArcCodec and Wayland service have the ability to
	 * send more than one fd. GL/Vulkan drivers may also have to modified.
	 */
	if (drv_num_buffers_per_bo(bo) != 1) {
		drv_bo_destroy(bo);
		cros_gralloc_error("Can only support one buffer per bo.");
		return CROS_GRALLOC_ERROR_NO_RESOURCES;
	}

	hnd = new cros_gralloc_handle();
	num_planes = drv_bo_get_num_planes(bo);

	hnd->base.version = sizeof(hnd->base);
	hnd->base.numFds = num_planes;
	hnd->base.numInts = handle_data_size - num_planes;

	for (size_t p = 0; p < num_planes; p++) {
		hnd->fds[p] = drv_bo_get_plane_fd(bo, p);
		hnd->strides[p] = drv_bo_get_plane_stride(bo, p);
		hnd->offsets[p] = drv_bo_get_plane_offset(bo, p);
		hnd->sizes[p] = drv_bo_get_plane_size(bo, p);

		mod = drv_bo_get_plane_format_modifier(bo, p);
		hnd->format_modifiers[2 * p] = static_cast<uint32_t>(mod >> 32);
		hnd->format_modifiers[2 * p + 1] = static_cast<uint32_t>(mod);
	}

	hnd->width = drv_bo_get_width(bo);
	hnd->height = drv_bo_get_height(bo);
	hnd->format = drv_bo_get_format(bo);
	hnd->pixel_stride = drv_bo_get_stride_in_pixels(bo);
	hnd->magic = cros_gralloc_magic;
	hnd->droid_format = cros_gralloc_invert_format(hnd->format);
	hnd->usage = descriptor->producer_usage;

	id = drv_bo_get_plane_handle(bo, 0).u32;
	auto buffer = new cros_gralloc_buffer(id, bo, hnd);

	std::lock_guard<std::mutex> lock(_mutex);
	_buffers[id] = buffer;
	_handles[hnd] = std::make_pair(buffer, 1);
	*out_handle = &hnd->base;
	return CROS_GRALLOC_ERROR_NONE;
}

int32_t cros_gralloc_driver::retain(buffer_handle_t handle)
{
	uint32_t id;
	std::lock_guard<std::mutex> lock(_mutex);

	auto hnd = cros_gralloc_convert_handle(handle);
	if (!hnd) {
		cros_gralloc_error("Invalid handle.");
		return CROS_GRALLOC_ERROR_BAD_HANDLE;
	}

	auto buffer = get_buffer(hnd);
	if (buffer) {
		_handles[hnd].second++;
		buffer->increase_refcount();
		return CROS_GRALLOC_ERROR_NONE;
	}

	if (drmPrimeFDToHandle(drv_get_fd(_drv), hnd->fds[0], &id)) {
		cros_gralloc_error("drmPrimeFDToHandle failed.");
		return CROS_GRALLOC_ERROR_BAD_HANDLE;
	}

	if (_buffers.count(id)) {
		buffer = _buffers[id];
		buffer->increase_refcount();
	} else {
		struct bo *bo;
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

		bo = drv_bo_import(_drv, &data);
		if (!bo)
			return CROS_GRALLOC_ERROR_NO_RESOURCES;

		id = drv_bo_get_plane_handle(bo, 0).u32;

		buffer = new cros_gralloc_buffer(id, bo, nullptr);
		_buffers[id] = buffer;
	}

	_handles[hnd] = std::make_pair(buffer, 1);
	return CROS_GRALLOC_ERROR_NONE;
}

int32_t cros_gralloc_driver::release(buffer_handle_t handle)
{
	std::lock_guard<std::mutex> lock(_mutex);

	auto hnd = cros_gralloc_convert_handle(handle);
	if (!hnd) {
		cros_gralloc_error("Invalid handle.");
		return CROS_GRALLOC_ERROR_BAD_HANDLE;
	}

	auto buffer = get_buffer(hnd);
	if (!buffer) {
		cros_gralloc_error("Invalid Reference.");
		return CROS_GRALLOC_ERROR_BAD_HANDLE;
	}

	if (!--_handles[hnd].second)
		_handles.erase(hnd);

	if (buffer->decrease_refcount() == 0) {
		_buffers.erase(buffer->get_id());
		delete buffer;
	}

	return CROS_GRALLOC_ERROR_NONE;
}

int32_t cros_gralloc_driver::map(buffer_handle_t handle, int32_t acquire_fence, uint64_t flags,
				 void *addr[DRV_MAX_PLANES])
{
	std::lock_guard<std::mutex> lock(_mutex);

	auto hnd = cros_gralloc_convert_handle(handle);
	if (!hnd) {
		cros_gralloc_error("Invalid handle.");
		return CROS_GRALLOC_ERROR_BAD_HANDLE;
	}

	auto buffer = get_buffer(hnd);
	if (!buffer) {
		cros_gralloc_error("Invalid Reference.");
		return CROS_GRALLOC_ERROR_BAD_HANDLE;
	}

	if (acquire_fence >= 0) {
		cros_gralloc_error("Sync wait not yet supported.");
		return CROS_GRALLOC_ERROR_UNSUPPORTED;
	}

	return buffer->map(flags, addr);
}

int32_t cros_gralloc_driver::unmap(buffer_handle_t handle, int32_t *release_fence)
{
	std::lock_guard<std::mutex> lock(_mutex);

	auto hnd = cros_gralloc_convert_handle(handle);
	if (!hnd) {
		cros_gralloc_error("Invalid handle.");
		return CROS_GRALLOC_ERROR_BAD_HANDLE;
	}

	auto buffer = get_buffer(hnd);
	if (!buffer) {
		cros_gralloc_error("Invalid Reference.");
		return CROS_GRALLOC_ERROR_BAD_HANDLE;
	}

	*release_fence = -1;
	return buffer->unmap();
}

int32_t cros_gralloc_driver::get_backing_store(buffer_handle_t handle, uint64_t *out_store)
{
	std::lock_guard<std::mutex> lock(_mutex);

	auto hnd = cros_gralloc_convert_handle(handle);
	if (!hnd) {
		cros_gralloc_error("Invalid handle.");
		return CROS_GRALLOC_ERROR_BAD_HANDLE;
	}

	auto buffer = get_buffer(hnd);
	if (!buffer) {
		cros_gralloc_error("Invalid Reference.");
		return CROS_GRALLOC_ERROR_BAD_HANDLE;
	}

	*out_store = static_cast<uint64_t>(buffer->get_id());
	return CROS_GRALLOC_ERROR_NONE;
}

cros_gralloc_buffer *cros_gralloc_driver::get_buffer(cros_gralloc_handle_t hnd)
{
	/* Assumes driver mutex is held. */
	if (_handles.count(hnd))
		return _handles[hnd].first;

	return nullptr;
}
