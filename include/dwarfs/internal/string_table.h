/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Marcus Holland-Moritz (github@mhxnet.de)
 * \copyright  Copyright (c) Marcus Holland-Moritz
 *
 * This file is part of dwarfs.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the “Software”), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
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

#include <array>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#ifdef DWARFS_HAVE_THRIFT
#include <dwarfs/gen-cpp2/metadata_layouts.h>
#endif

#ifdef DWARFS_HAVE_FLATBUFFERS
#include <dwarfs/metadata/domain/string_table.h>
#endif

namespace dwarfs {

class logger;

namespace internal {

class string_table {
 public:
#ifdef DWARFS_HAVE_THRIFT
  using LegacyTableView =
      ::apache::thrift::frozen::View<std::vector<std::string>>;
  using PackedTableView =
      ::apache::thrift::frozen::View<thrift::metadata::string_table>;
  using packed_table_type = thrift::metadata::string_table;
#else
  // Plain C++ view types for non-Thrift builds
  using LegacyTableView = std::span<std::string const>;
  using PackedTableView = metadata::domain::string_table const&;
  using packed_table_type = metadata::domain::string_table;
#endif

  struct pack_options {
    pack_options(bool pack_data = true, bool pack_index = true,
                 bool force_pack_data = false)
        : pack_data{pack_data}
        , pack_index{pack_index}
        , force_pack_data{force_pack_data} {}

    bool pack_data;
    bool pack_index;
    bool force_pack_data;
  };

#ifdef DWARFS_HAVE_THRIFT
  string_table(logger& lgr, std::string_view name, PackedTableView v);
  string_table(LegacyTableView v);
  string_table(std::span<std::string const> v);
#endif

#ifdef DWARFS_HAVE_FLATBUFFERS
  // Domain model constructor - available when FlatBuffers is enabled
  string_table(logger& lgr, std::string_view name,
               ::dwarfs::metadata::domain::string_table const& v);
#endif

#if !defined(DWARFS_HAVE_THRIFT) && defined(DWARFS_HAVE_FLATBUFFERS)
  string_table(std::span<std::string const> v);
  // NEW: Constructor that owns the vector data (avoids use-after-free)
  explicit string_table(std::vector<std::string> v);
#endif

  std::string operator[](size_t index) const { return impl_->lookup(index); }

  std::vector<std::string> unpack() const { return impl_->unpack(); }

  bool is_packed() const { return impl_->is_packed(); }

  // Returns number of entries (count)
  size_t size() const { return impl_->size(); }

  // Returns total bytes of all unpacked strings (deprecated - use size() for count)
  size_t unpacked_size() const { return impl_->unpacked_size(); }

  static packed_table_type
  pack(std::span<std::string const> input,
       pack_options const& options = pack_options());

  static packed_table_type
  pack(std::span<std::string_view const> input,
       pack_options const& options = pack_options());

  static packed_table_type
  pack(std::vector<std::string> const& input,
       pack_options const& options = pack_options()) {
    return pack(std::span(input.data(), input.size()), options);
  }

  template <size_t N>
  static packed_table_type
  pack(std::array<std::string_view, N> const& input,
       pack_options const& options = pack_options()) {
    return pack(std::span(input.data(), input.size()), options);
  }

#ifdef DWARFS_HAVE_FLATBUFFERS
  // FlatBuffers-specific pack functions that always return domain types
  static metadata::domain::string_table
  pack_domain(std::span<std::string const> input,
              pack_options const& options = pack_options());

  static metadata::domain::string_table
  pack_domain(std::span<std::string_view const> input,
              pack_options const& options = pack_options());

  static metadata::domain::string_table
  pack_domain(std::vector<std::string> const& input,
              pack_options const& options = pack_options()) {
    return pack_domain(std::span(input.data(), input.size()), options);
  }
#endif

  class impl {
   public:
    virtual ~impl() = default;

    virtual std::string lookup(size_t index) const = 0;
    virtual std::vector<std::string> unpack() const = 0;
    virtual bool is_packed() const = 0;
    virtual size_t size() const = 0;  // Number of entries
    virtual size_t unpacked_size() const = 0;  // Total bytes
  };

 private:
  template <typename T>
  static packed_table_type
  pack_generic(std::span<T const> input, pack_options const& options);

  std::unique_ptr<impl const> impl_;
};

} // namespace internal

} // namespace dwarfs
