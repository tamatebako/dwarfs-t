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

#include <algorithm>
#include <iostream>
#include <numeric>
#include <vector>

#include <fmt/format.h>

#include <dwarfs/error.h>
#include <dwarfs/logger.h>

#include <dwarfs/internal/fsst.h>
#include <dwarfs/internal/string_table.h>

namespace dwarfs::internal {

#ifdef DWARFS_HAVE_EXPERIMENTAL_THRIFT

// ============================================================================
// Thrift-based implementation (existing code)
// ============================================================================

class legacy_string_table : public string_table::impl {
 public:
  explicit legacy_string_table(string_table::LegacyTableView v)
      : v_{v} {}

  std::string lookup(size_t index) const override {
    return std::string(v_[index]);
  }

  std::vector<std::string> unpack() const override {
    throw std::runtime_error("cannot unpack legacy string table");
  }

  bool is_packed() const override { return false; }

  size_t size() const override {
    return v_.size();
  }

  size_t unpacked_size() const override {
    return std::accumulate(v_.begin(), v_.end(), 0,
                           [](auto n, auto s) { return n + s.size(); });
  }

 private:
  string_table::LegacyTableView v_;
};

template <bool PackedData, bool PackedIndex>
class packed_string_table : public string_table::impl {
 public:
  packed_string_table(logger& lgr, [[maybe_unused]] std::string_view name,
                      string_table::PackedTableView v)
      : v_{v}
      , buffer_{v_.buffer().data()} {
    LOG_PROXY(debug_logger_policy, lgr);

    if constexpr (PackedData) {
      auto ti = LOG_TIMED_DEBUG;

      auto st = v_.symtab();
      DWARFS_CHECK(st, "symtab unexpectedly unset");

      dec_.emplace(st.value());

      ti << "imported dictionary for " << name << " string table";
    }

    if constexpr (PackedIndex) {
      auto ti = LOG_TIMED_DEBUG;

      DWARFS_CHECK(v_.packed_index(), "index unexpectedly not packed");
      index_.resize(v_.index().size() + 1);
      std::partial_sum(v_.index().begin(), v_.index().end(),
                       index_.begin() + 1);

      ti << "unpacked index for " << name << " string table ("
         << sizeof(index_.front()) * index_.capacity() << " bytes)";
    }
  }

  std::string lookup(size_t index) const override {
    auto beg = buffer_;
    auto end = buffer_;

    if constexpr (PackedIndex) {
      beg += index_[index];
      end += index_[index + 1];
    } else {
      beg += v_.index()[index];
      end += v_.index()[index + 1];
    }

    if constexpr (PackedData) {
      return dec_->decompress(std::string_view{beg, end});
    }

    return {beg, end};
  }

  std::vector<std::string> unpack() const override {
    std::vector<std::string> v;
    auto size = PackedIndex ? index_.size() : v_.index().size();
    if (size > 0) {
      v.reserve(size - 1);
      for (size_t i = 0; i < size - 1; ++i) {
        v.emplace_back(lookup(i));
      }
    }
    return v;
  }

  bool is_packed() const override { return true; }

  size_t size() const override {
    auto index_size = PackedIndex ? index_.size() : v_.index().size();
    return index_size > 0 ? index_size - 1 : 0;
  }

  size_t unpacked_size() const override {
    size_t unpacked = 0;
    auto size = PackedIndex ? index_.size() : v_.index().size();
    for (size_t i = 0; i < size - 1; ++i) {
      unpacked += lookup(i).size();
    }
    return unpacked;
  }

 private:
  string_table::PackedTableView v_;
  char const* const buffer_;
  std::vector<uint32_t> index_;
  std::optional<fsst_decoder> dec_;
};

string_table::string_table(LegacyTableView v)
    : impl_{std::make_unique<legacy_string_table>(v)} {}

namespace {

std::unique_ptr<string_table::impl>
build_string_table(logger& lgr, std::string_view name,
                   string_table::PackedTableView v) {
  if (v.symtab()) {
    if (v.packed_index()) {
      return std::make_unique<packed_string_table<true, true>>(lgr, name, v);
    }
    return std::make_unique<packed_string_table<true, false>>(lgr, name, v);
  }
  if (v.packed_index()) {
    return std::make_unique<packed_string_table<false, true>>(lgr, name, v);
  }
  return std::make_unique<packed_string_table<false, false>>(lgr, name, v);
}

} // namespace

string_table::string_table(logger& lgr, std::string_view name,
                           PackedTableView v)
    : impl_{build_string_table(lgr, name, v)} {}

#else
#endif

// ============================================================================
// Plain C++ implementation (available in all builds for FlatBuffers)
// ============================================================================

class legacy_string_table_cpp : public string_table::impl {
 public:
  explicit legacy_string_table_cpp(std::span<std::string const> v)
      : v_{v} {}

  // NEW: Constructor that accepts vector by value and owns the data
  explicit legacy_string_table_cpp(std::vector<std::string> v)
      : owned_v_{std::move(v)}
      , v_{owned_v_} {}

  std::string operator[](size_t i) const {
    return std::string{v_[i]};
  }

  std::string lookup(size_t index) const override {
    return v_[index];
  }

  std::vector<std::string> unpack() const override {
    throw std::runtime_error("cannot unpack legacy string table");
  }

  bool is_packed() const override { return false; }

  size_t size() const override { return v_.size(); }

  size_t unpacked_size() const override {
    return std::accumulate(v_.begin(), v_.end(), size_t{0},
                           [](auto sum, auto const& s) {
                             return sum + s.size();
                           });
  }

 private:
  std::vector<std::string> owned_v_;  // NEW: Owned data (empty if using span)
  std::span<std::string const> v_;
};

string_table::string_table(std::span<std::string const> v)
    : impl_{std::make_unique<legacy_string_table_cpp>(v)} {}

// NEW: Constructor that owns the vector data (FlatBuffers-only)
#if !defined(DWARFS_HAVE_EXPERIMENTAL_THRIFT) && defined(DWARFS_HAVE_FLATBUFFERS)
string_table::string_table(std::vector<std::string> v)
    : impl_{std::make_unique<legacy_string_table_cpp>(std::move(v))} {}
#endif

// C++ implementation - always available when FLATBUFFERS is enabled
#ifdef DWARFS_HAVE_FLATBUFFERS

template <bool PackedData, bool PackedIndex>
class packed_string_table_cpp : public string_table::impl {
 public:
  packed_string_table_cpp(logger& lgr, [[maybe_unused]] std::string_view name,
                          metadata::domain::string_table const& v)
      : v_{v}  // COPY the domain object, don't just reference it!
      , buffer_{v_.buffer.data()} {
    LOG_PROXY(debug_logger_policy, lgr);

    if constexpr (PackedData) {
      auto ti = LOG_TIMED_DEBUG;

      DWARFS_CHECK(v_.symtab.has_value(), "symtab unexpectedly unset");

      dec_.emplace(v_.symtab.value());

      ti << "imported dictionary for " << name << " string table";
    }

    if constexpr (PackedIndex) {
      auto ti = LOG_TIMED_DEBUG;

      DWARFS_CHECK(v_.packed_index, "index unexpectedly not packed");
      index_.resize(v_.index.size() + 1);
      std::partial_sum(v_.index.begin(), v_.index.end(),
                       index_.begin() + 1);

      ti << "unpacked index for " << name << " string table ("
         << sizeof(index_.front()) * index_.capacity() << " bytes)";
    }
  }

  std::string lookup(size_t index) const override {
    // Bounds check: with N+1 index entries, we have N valid string indices (0..N-1)
    auto size = PackedIndex ? index_.size() : v_.index.size();
    if (size == 0) {
      throw std::out_of_range(fmt::format(
          "string_table::lookup({}): index is empty", index));
    }

    if (index >= size - 1) {
      throw std::out_of_range(fmt::format(
          "string_table::lookup({}): index out of range (max={})",
          index, size - 2));
    }

    auto beg = buffer_;
    auto end = buffer_;

    if constexpr (PackedIndex) {
      beg += index_[index];
      end += index_[index + 1];
    } else {
      beg += v_.index[index];
      end += v_.index[index + 1];
    }

    if constexpr (PackedData) {
      return dec_->decompress(std::string_view{beg, end});
    }

    return {beg, end};
  }

  std::vector<std::string> unpack() const override {
    std::vector<std::string> v;
    auto size = PackedIndex ? index_.size() : v_.index.size();
    if (size > 0) {
      v.reserve(size - 1);
      for (size_t i = 0; i < size - 1; ++i) {
        v.emplace_back(lookup(i));
      }
    }
    return v;
  }

  bool is_packed() const override { return true; }

  size_t size() const override {
    auto index_size = PackedIndex ? index_.size() : v_.index.size();
    return index_size > 0 ? index_size - 1 : 0;
  }

  size_t unpacked_size() const override {
    size_t unpacked = 0;
    auto size = PackedIndex ? index_.size() : v_.index.size();
    for (size_t i = 0; i < size - 1; ++i) {
      unpacked += lookup(i).size();
    }
    return unpacked;
  }

 private:
  metadata::domain::string_table v_;  // Store by VALUE (not reference!) to avoid dangling reference
  char const* const buffer_;
  std::vector<uint32_t> index_;
  std::optional<fsst_decoder> dec_;
};

namespace {

std::unique_ptr<string_table::impl>
build_string_table_cpp(logger& lgr, std::string_view name,
                       metadata::domain::string_table const& v) {
  if (v.symtab.has_value()) {
    if (v.packed_index) {
      return std::make_unique<packed_string_table_cpp<true, true>>(lgr, name, v);
    }
    return std::make_unique<packed_string_table_cpp<true, false>>(lgr, name, v);
  }
  if (v.packed_index) {
    return std::make_unique<packed_string_table_cpp<false, true>>(lgr, name, v);
  }
  return std::make_unique<packed_string_table_cpp<false, false>>(lgr, name, v);
}

} // namespace

string_table::string_table(logger& lgr, std::string_view name,
                           metadata::domain::string_table const& v)
    : impl_{build_string_table_cpp(lgr, name, v)} {}

#endif // DWARFS_HAVE_FLATBUFFERS

// ============================================================================
// Common pack implementation (used by both Thrift and plain C++)
// ============================================================================

template <typename T>
string_table::packed_table_type
string_table::pack_generic(std::span<T const> input,
                           pack_options const& options) {
  auto const size = input.size();
  std::optional<fsst_encoder::bulk_compression_result> res;

  if (options.pack_data) {
    res = fsst_encoder::compress(input, options.force_pack_data);
  }

#ifdef DWARFS_HAVE_EXPERIMENTAL_THRIFT
  thrift::metadata::string_table output;

  if (res.has_value()) {
    // store compressed
    output.buffer() = std::move(res->buffer);
    output.symtab() = std::move(res->dictionary);
    // For N strings:
    // - If pack_index: N deltas (differences between consecutive offsets)
    // - If !pack_index: N+1 offsets (N string offsets + 1 buffer size marker)
    output.index()->resize(size + (options.pack_index ? 0 : 1));
    for (size_t i = 0; i < size; ++i) {
      output.index()[i] = res->compressed_data[i].size();
    }
    // Only add buffer size marker if NOT pack_index
    if (!options.pack_index) {
      output.index()[size] = output.buffer().value().size();
    }
  } else {
    // store uncompressed
    auto const total_input_size =
        std::accumulate(input.begin(), input.end(), size_t{0},
                        [](size_t n, auto const& s) { return n + s.size(); });
    output.buffer()->reserve(total_input_size);
    output.index()->reserve(size + (options.pack_index ? 0 : 1));  // N or N+1 entries
    for (auto const& s : input) {
      output.buffer().value() += s;
      output.index()->emplace_back(s.size());
    }
    // Only add buffer size marker if NOT pack_index
    if (!options.pack_index) {
      output.index()->emplace_back(output.buffer().value().size());
    }
  }

  output.packed_index() = options.pack_index;

  if (!options.pack_index) {
    // Convert sizes to offsets
    output.index()->insert(output.index()->begin(), 0);
    std::partial_sum(output.index()->begin(), output.index()->end() - 1,
                     output.index()->begin());
    output.index()->back() = output.buffer().value().size();
  }

  return output;
#else
  metadata::domain::string_table output;

  if (res.has_value()) {
    // store compressed
    output.buffer = std::move(res->buffer);
    output.symtab = std::move(res->dictionary);
    // For N strings, we need N+1 index entries
    output.index.resize(size + 1);
    for (size_t i = 0; i < size; ++i) {
      output.index[i] = res->compressed_data[i].size();
    }
    // The last entry is the buffer size
    output.index[size] = output.buffer.size();
  } else {
    // store uncompressed
    auto const total_input_size =
        std::accumulate(input.begin(), input.end(), size_t{0},
                        [](size_t n, auto const& s) { return n + s.size(); });
    output.buffer.reserve(total_input_size);
    output.index.reserve(size + 1);  // N+1 entries
    for (auto const& s : input) {
      output.buffer += s;
      output.index.emplace_back(s.size());
    }
    // The last entry is the buffer size
    output.index.emplace_back(output.buffer.size());
  }

  output.packed_index = options.pack_index;

  if (!options.pack_index) {
    // Convert sizes to offsets
    output.index.insert(output.index.begin(), 0);
    std::partial_sum(output.index.begin(), output.index.end() - 1,
                     output.index.begin());
    output.index.back() = output.buffer.size();
  }

  return output;
#endif
}

string_table::packed_table_type
string_table::pack(std::span<std::string const> input,
                   pack_options const& options) {
  return pack_generic(input, options);
}

string_table::packed_table_type
string_table::pack(std::span<std::string_view const> input,
                   pack_options const& options) {
  return pack_generic(input, options);
}

#ifdef DWARFS_HAVE_FLATBUFFERS
// FlatBuffers-specific pack that always returns domain types
// Separate implementation to avoid Thrift type conflicts
template <typename T>
static metadata::domain::string_table
pack_domain_impl(std::span<T const> input, string_table::pack_options const& options) {
  auto const size = input.size();
  std::optional<fsst_encoder::bulk_compression_result> res;

  if (options.pack_data) {
    res = fsst_encoder::compress(input, options.force_pack_data);
  }

  metadata::domain::string_table output;

  if (res.has_value()) {
    // store compressed
    output.buffer = std::move(res->buffer);
    output.symtab = std::move(res->dictionary);
    // For N strings, we need N+1 index entries (N sizes/offsets + buffer size marker)
    output.index.resize(size + 1);
    for (size_t i = 0; i < size; ++i) {
      output.index[i] = res->compressed_data[i].size();
    }
    // The last entry is the buffer size (marker for end of last string)
    output.index[size] = output.buffer.size();
  } else {
    // store uncompressed
    auto const total_input_size =
        std::accumulate(input.begin(), input.end(), size_t{0},
                        [](size_t n, auto const& s) { return n + s.size(); });
    output.buffer.reserve(total_input_size);
    output.index.reserve(size + 1);  // N+1 entries for N strings
    for (auto const& s : input) {
      output.buffer += s;
      output.index.emplace_back(s.size());
    }
    // The last entry is the buffer size (marker for end of last string)
    output.index.emplace_back(output.buffer.size());
  }

  output.packed_index = options.pack_index;

  if (!options.pack_index) {
    // Convert sizes to offsets
    // Index currently has N+1 entries: [s0, s1, ..., sN-1, buffer_size]
    // We need to convert to offsets: [0, o1, o2, ..., oN-1, buffer_size]
    output.index.insert(output.index.begin(), 0);
    std::partial_sum(output.index.begin(), output.index.end() - 1,
                     output.index.begin());
    // Remove the extra element that partial_sum created
    // After insert+partial_sum: [0, s0, s0+s1, ..., sum(s0..sN-1), sum(s0..sN-1)+buffer_size]
    // We want: [0, o1, o2, ..., oN-1, buffer_size]
    output.index.back() = output.buffer.size();
  }

  return output;
}

metadata::domain::string_table
string_table::pack_domain(std::span<std::string const> input,
                          pack_options const& options) {
  return pack_domain_impl(input, options);
}

metadata::domain::string_table
string_table::pack_domain(std::span<std::string_view const> input,
                          pack_options const& options) {
  return pack_domain_impl(input, options);
}
#endif

} // namespace dwarfs::internal
