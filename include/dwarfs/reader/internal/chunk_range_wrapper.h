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

#include <memory>
#include <dwarfs/reader/internal/metadata_view_interface.h>

namespace dwarfs::reader::internal {

/**
 * Value-semantic wrapper for chunk_range_interface
 * 
 * This class provides value semantics (copy/move) for the abstract
 * chunk_range_interface, enabling it to be returned by value from
 * functions in dual-format builds.
 * 
 * Single-format builds use the concrete backend type directly (zero overhead).
 * Dual-format builds use this wrapper to enable polymorphism.
 */
class chunk_range_wrapper {
 public:
  // Type alias for iterator (dual-format needs this)
  using iterator = chunk_range_interface::iterator;
  
  chunk_range_wrapper() = default;
  
  // Construct from interface pointer (takes ownership)
  explicit chunk_range_wrapper(std::shared_ptr<chunk_range_interface const> impl)
      : impl_{std::move(impl)} {}
  
  // Value semantics: copyable and movable
  chunk_range_wrapper(chunk_range_wrapper const&) = default;
  chunk_range_wrapper& operator=(chunk_range_wrapper const&) = default;
  chunk_range_wrapper(chunk_range_wrapper&&) noexcept = default;
  chunk_range_wrapper& operator=(chunk_range_wrapper&&) noexcept = default;
  
  // Delegate to interface
  size_t size() const { return impl_ ? impl_->size() : 0; }
  bool empty() const { return impl_ ? impl_->empty() : true; }
  
  std::shared_ptr<chunk_view_interface const> at(size_t index) const {
    return impl_ ? impl_->at(index) : nullptr;
  }
  
  // Iterator support (forward to interface)
  chunk_range_interface::iterator begin() const {
    return impl_ ? impl_->begin() : chunk_range_interface::iterator{};
  }

  chunk_range_interface::iterator end() const {
    return impl_ ? impl_->end() : chunk_range_interface::iterator{};
  }
  
  // Access to underlying interface (for advanced use)
  chunk_range_interface const* get() const { return impl_.get(); }
  chunk_range_interface const& operator*() const { return *impl_; }
  chunk_range_interface const* operator->() const { return impl_.get(); }
  explicit operator bool() const { return impl_ != nullptr; }
  
 private:
  std::shared_ptr<chunk_range_interface const> impl_;
};

} // namespace dwarfs::reader::internal