/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef CROS_GRALLOC_BUFFER_H
#define CROS_GRALLOC_BUFFER_H

#include "../drv.h"
#include "cros_gralloc_helpers.h"

class cros_gralloc_buffer
{
      public:
	cros_gralloc_buffer(uint32_t id, struct bo *acquire_bo,
			    struct cros_gralloc_handle *acquire_handle);
	~cros_gralloc_buffer();

	uint32_t get_id() const;

	/* The new reference count is returned by both these functions. */
	int32_t increase_refcount();
	int32_t decrease_refcount();

	int32_t map(uint64_t flags, void *addr[DRV_MAX_PLANES]);
	int32_t unmap();

      private:
	cros_gralloc_buffer(cros_gralloc_buffer const &);
	cros_gralloc_buffer operator=(cros_gralloc_buffer const &);

	uint32_t _id;
	struct bo *_bo;
	struct cros_gralloc_handle *_hnd;

	int32_t _refcount;
	int32_t _lockcount;
	uint32_t _num_planes;

	struct map_info *_map_data[DRV_MAX_PLANES];
};

#endif
