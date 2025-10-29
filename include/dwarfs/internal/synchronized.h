/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Marcus Holland-Moritz (github@mhxnet.de)
 * \copyright  Copyright (c) Marcus Holland-Moritz
 *
 * This file is part of dwarfs.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

/**
 * \file synchronized.h
 * \brief Thread-safe wrapper (folly::Synchronized replacement)
 *
 * Provides RAII-based synchronized access to data using mutexes.
 */

#include <mutex>
#include <shared_mutex>
#include <utility>

namespace dwarfs::compat {

/**
 * \brief Thread-safe wrapper for data
 *
 * Replacement for folly::Synchronized providing thread-safe access to wrapped data.
 * Uses std::shared_mutex for reader-writer locking.
 *
 * \tparam T Type of data to protect
 */
template <typename T, typename Mutex = std::shared_mutex>
class Synchronized {
 private:
  mutable Mutex mutex_;
  T data_;

 public:
  /**
   * \brief RAII lock guard for write access
   */
  class WriteLock {
   private:
    std::unique_lock<Mutex> lock_;
    T* data_;

   public:
    WriteLock(Mutex& mutex, T& data)
        : lock_(mutex), data_(&data) {}

    T& operator*() { return *data_; }
    T* operator->() { return data_; }
    const T& operator*() const { return *data_; }
    const T* operator->() const { return data_; }
  };

  /**
   * \brief RAII lock guard for read access
   */
  class ReadLock {
   private:
    std::shared_lock<Mutex> lock_;
    const T* data_;

   public:
    ReadLock(Mutex& mutex, const T& data)
        : lock_(mutex), data_(&data) {}

    const T& operator*() const { return *data_; }
    const T* operator->() const { return data_; }
  };

  // Constructors
  Synchronized() = default;

  explicit Synchronized(const T& value) : data_(value) {}

  explicit Synchronized(T&& value) : data_(std::move(value)) {}

  // Acquire write lock (folly::Synchronized::wlock())
  WriteLock wlock() {
    return WriteLock(mutex_, data_);
  }

  // Acquire read lock (folly::Synchronized::rlock())
  ReadLock rlock() const {
    return ReadLock(mutex_, data_);
  }

  // Execute function with write lock
  template <typename F>
  auto with_wlock(F&& func) -> decltype(func(std::declval<T&>())) {
    std::unique_lock lock(mutex_);
    return func(data_);
  }

  // Execute function with read lock
  template <typename F>
  auto with_rlock(F&& func) const -> decltype(func(std::declval<const T&>())) {
    std::shared_lock lock(mutex_);
    return func(data_);
  }

  // Copy/move operations with locking
  Synchronized(const Synchronized& other) {
    std::shared_lock lock(other.mutex_);
    data_ = other.data_;
  }

  Synchronized(Synchronized&& other) {
    std::unique_lock lock(other.mutex_);
    data_ = std::move(other.data_);
  }

  Synchronized& operator=(const Synchronized& other) {
    if (this != &other) {
      std::scoped_lock lock(mutex_, other.mutex_);
      data_ = other.data_;
    }
    return *this;
  }

  Synchronized& operator=(Synchronized&& other) {
    if (this != &other) {
      std::scoped_lock lock(mutex_, other.mutex_);
      data_ = std::move(other.data_);
    }
    return *this;
  }
};

} // namespace dwarfs::compat