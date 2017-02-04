/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef HWC_BUFFER_H_
#define HWC_BUFFER_H_

#include <stdint.h>

struct HwcBuffer {
  uint32_t width;
  uint32_t height;
  uint32_t format;
  uint32_t pitches[4];
  uint32_t offsets[4];
  uint32_t gem_handles[4];
  uint32_t prime_fd;
  uint32_t usage;
};

#endif  // HWC_BUFFER_H_
