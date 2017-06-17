/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef CROS_GRALLOC_SPINLOCK_H
#define CROS_GRALLOC_SPINLOCK_H

#include <atomic>

#ifndef DISABLE_LOCK
class SpinLock {
 public:
  void lock() {
    while (atomic_lock_.test_and_set(std::memory_order_acquire)) {
    }
  }

  void unlock() {
    atomic_lock_.clear(std::memory_order_release);
  }

 private:
  std::atomic_flag atomic_lock_ = ATOMIC_FLAG_INIT;
};

class ScopedSpinLock {
 public:
  explicit ScopedSpinLock(SpinLock& lock) : lock_(lock) {
    lock_.lock();
    locked_ = true;
  }

  ~ScopedSpinLock() {
    if (locked_) {
      lock_.unlock();
      locked_ = false;
    }
  }

 private:
  SpinLock& lock_;
  bool locked_;
};

#define SCOPED_SPIN_LOCK(X) \
ScopedSpinLock lock(X);
#else
class SpinLock {
 public:
  void lock() {
  }

  void unlock() {
  }
};

#define SCOPED_SPIN_LOCK(X) ((void)0)
#endif

#endif  // PUBLIC_SPINLOCK_H_
