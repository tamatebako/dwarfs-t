/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Ribose Inc.
 * \copyright  Copyright (c) Ribose Inc.
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

// Factory stub for dual-format builds
// Provides make_metadata_v2_flatbuffers() without full implementation

#if defined(DWARFS_HAVE_FLATBUFFERS) && defined(DWARFS_HAVE_THRIFT)

#include <dwarfs/error.h>
#include <dwarfs/logger.h>
#include <dwarfs/reader/internal/metadata_v2.h>
#include <dwarfs/reader/metadata_options.h>

namespace dwarfs::reader::internal {

// Factory function for dual-format builds
// In dual-format, We prefer Thrift backend, so this function is rarely called
// It's only here to satisfy the linker when metadata_v2_factory.cpp needs it
metadata_v2 make_metadata_v2_flatbuffers(
    logger& lgr, std::span<uint8_t const> /*schema*/, std::span<uint8_t const> /*data*/,
    metadata_options const& /*options*/, int /*inode_offset*/,
    bool /*force_consistency_check*/,
    std::shared_ptr<performance_monitor const> const& /*perfmon*/) {
  
  LOG_PROXY(prod_logger_policy, lgr);
  LOG_ERROR << "FlatBuffers backend not available in this dual-format build";
  LOG_ERROR << "Dual-format builds use Thrift backend for metadata";
  
  DWARFS_THROW(runtime_error, 
               "FlatBuffers backend not fully compiled in dual-format build. "
               "Use FlatBuffers-only build or Thrift format.");
}

} // namespace dwarfs::reader::internal

#endif // defined(DWARFS_HAVE_FLATBUFFERS) && defined(DWARFS_HAVE_THRIFT)