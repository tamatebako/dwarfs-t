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

#ifdef DWARFS_HAVE_THRIFT

#include <cstdint>
#include <string>

#include <nlohmann/json.hpp>

#include <dwarfs/gen-cpp2/metadata_layouts.h>

// Forward declare FlatBuffers types for dual-format support
#ifdef DWARFS_HAVE_FLATBUFFERS
namespace dwarfs::flatbuffers {
struct Metadata;
struct HistoryEntry;
}
#endif

namespace dwarfs {

class file_stat;

namespace reader::internal {

// Forward declarations for backend-specific types
// Note: In single-format builds, metadata_types_fwd.h provides type aliases
// that bring backend types into the internal:: namespace
namespace thrift_backend {
class inode_view_impl;
}

#ifdef DWARFS_HAVE_FLATBUFFERS
namespace flatbuffers_backend {
class inode_view_impl;
}
#endif

class time_resolution_handler {
 public:
  // Thrift constructors
  explicit time_resolution_handler(
      ::apache::thrift::frozen::View<thrift::metadata::metadata> meta);
  explicit time_resolution_handler(
      ::apache::thrift::frozen::View<thrift::metadata::history_entry> hist);

#ifdef DWARFS_HAVE_FLATBUFFERS
  // FlatBuffers constructors (for dual-format builds)
  explicit time_resolution_handler(::dwarfs::flatbuffers::Metadata const* meta);
  explicit time_resolution_handler(::dwarfs::flatbuffers::HistoryEntry const* hist);
#endif

#if defined(DWARFS_HAVE_FLATBUFFERS) && defined(DWARFS_HAVE_THRIFT)
  // Dual-format: use namespace-qualified type
  void fill_stat_timevals(file_stat& st, thrift_backend::inode_view_impl const& ivr) const;
#else
  // Single-format: use simple name
  void fill_stat_timevals(file_stat& st, inode_view_impl const& ivr) const;
#endif

#ifdef DWARFS_HAVE_FLATBUFFERS
  void fill_stat_timevals(file_stat& st, flatbuffers_backend::inode_view_impl const& ivr) const;
#endif

  void add_time_resolution_to(nlohmann::json& j) const;
  std::string get_time_resolution_string() const;

 private:
  template <typename T>
  time_resolution_handler(T const& obj, uint64_t timebase);

#ifdef DWARFS_HAVE_FLATBUFFERS
  template <typename T>
  time_resolution_handler(T const* obj, uint64_t timebase);
#endif

  uint64_t const timebase_{0};
  uint32_t const resolution_{1};
  uint32_t const nsec_multiplier_{0};
  bool const mtime_only_{false};
};

} // namespace reader::internal
} // namespace dwarfs

#endif // DWARFS_HAVE_THRIFT

//==============================================================================
// FlatBuffers-native time resolution handler (ONLY when Thrift not available)
//==============================================================================

#if defined(DWARFS_HAVE_FLATBUFFERS) && !defined(DWARFS_HAVE_THRIFT)

#include <cstdint>
#include <string>

#include <nlohmann/json.hpp>

namespace dwarfs {

class file_stat;

// Forward declare FlatBuffers types
namespace flatbuffers {
struct Metadata;
struct HistoryEntry;
}

namespace reader::internal {

// Forward declare FlatBuffers backend types
namespace flatbuffers_backend {
class inode_view_impl;
}

class time_resolution_handler {
 public:
  explicit time_resolution_handler(flatbuffers::Metadata const* meta);
  explicit time_resolution_handler(flatbuffers::HistoryEntry const* hist);

  void fill_stat_timevals(file_stat& st, flatbuffers_backend::inode_view_impl const& ivr) const;
  void add_time_resolution_to(nlohmann::json& j) const;
  std::string get_time_resolution_string() const;

 private:
  template <typename T>
  time_resolution_handler(T const* obj, uint64_t timebase);

  uint64_t const timebase_{0};
  uint32_t const resolution_{1};
  uint32_t const nsec_multiplier_{0};
  bool const mtime_only_{false};
};

} // namespace reader::internal
} // namespace dwarfs

#endif // DWARFS_HAVE_FLATBUFFERS && !DWARFS_HAVE_THRIFT
