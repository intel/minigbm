/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "cros_gralloc_buffer.h"

#include <assert.h>
#include <sys/mman.h>

cros_gralloc_buffer::cros_gralloc_buffer(uint32_t id, struct bo *acquire_bo,
					 struct cros_gralloc_handle *acquire_handle)
    : _id(id), _bo(acquire_bo), _hnd(acquire_handle), _refcount(1), _lockcount(0)
{
	assert(_bo);
	_num_planes = drv_bo_get_num_planes(_bo);
	for (uint32_t i = 0; i < _num_planes; i++)
		_map_data[i] = nullptr;
}

cros_gralloc_buffer::~cros_gralloc_buffer()
{
	drv_bo_destroy(_bo);
	if (_hnd) {
		native_handle_close(&_hnd->base);
		delete _hnd;
	}
}

uint32_t cros_gralloc_buffer::get_id() const
{
	return _id;
}

int32_t cros_gralloc_buffer::increase_refcount()
{
	return ++_refcount;
}

int32_t cros_gralloc_buffer::decrease_refcount()
{
	return --_refcount;
}

int32_t cros_gralloc_buffer::map(uint64_t flags, void *addr[DRV_MAX_PLANES])
{
	if (flags) {
		for (uint32_t plane = 0; plane < _num_planes; plane++) {
			addr[plane] =
			    drv_bo_map(_bo, 0, 0, drv_bo_get_width(_bo), drv_bo_get_height(_bo), 0,
				       &_map_data[plane], plane);

			if (addr[plane] == MAP_FAILED) {
				cros_gralloc_error("Mapping failed.");
				return CROS_GRALLOC_ERROR_UNSUPPORTED;
			}
		}
	}

	_lockcount++;
	return CROS_GRALLOC_ERROR_NONE;
}

int32_t cros_gralloc_buffer::unmap()
{
	if (!--_lockcount) {
		for (uint32_t plane = 0; plane < _num_planes; plane++) {
			if (_map_data[plane]) {
				drv_bo_unmap(_bo, _map_data[plane]);
				_map_data[plane] = nullptr;
			}
		}
	}

	return CROS_GRALLOC_ERROR_NONE;
}
