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

#include <algorithm>
#include <numeric>

#include <fmt/format.h>

#include <dwarfs/error.h>
#include <dwarfs/logger.h>

#include <dwarfs/internal/fsst.h>
#include <dwarfs/internal/string_table.h>

namespace dwarfs::internal {

class legacy_string_table : public string_table::impl {
 public:
  explicit legacy_string_table(std::vector<std::string> const& v)
      : v_{v} {}

  std::string lookup(size_t index) const override {
    return v_[index];
  }

  std::vector<std::string> unpack() const override {
    throw std::runtime_error("cannot unpack legacy string table");
  }

  bool is_packed() const override { return false; }

  size_t unpacked_size() const override {
    return std::accumulate(v_.begin(), v_.end(), size_t{0},
                           [](auto n, auto const& s) { return n + s.size(); });
  }

 private:
  std::vector<std::string> const& v_;
};

template <bool PackedData, bool PackedIndex>
class packed_string_table : public string_table::impl {
 public:
  packed_string_table(logger& lgr, [[maybe_unused]] std::string_view name,
                      metadata::domain::string_table const& st)
      : st_{st}
      , buffer_{st_.buffer.data()} {
    LOG_PROXY(debug_logger_policy, lgr);

    if constexpr (PackedData) {
      auto ti = LOG_TIMED_DEBUG;

      DWARFS_CHECK(st_.symtab.has_value(), "symtab unexpectedly unset");

      dec_.emplace(st_.symtab.value());

      ti << "imported dictionary for " << name << " string table";
    }

    if constexpr (PackedIndex) {
      auto ti = LOG_TIMED_DEBUG;

      DWARFS_CHECK(st_.packed_index, "index unexpectedly not packed");
      index_.resize(st_.index.size() + 1);
      std::partial_sum(st_.index.begin(), st_.index.end(),
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
      beg += st_.index[index];
      end += st_.index[index + 1];
    }

    if constexpr (PackedData) {
      return dec_->decompress(std::string_view{beg, end});
    }

    return {beg, end};
  }

  std::vector<std::string> unpack() const override {
    std::vector<std::string> v;
    auto size = PackedIndex ? index_.size() : st_.index.size();
    if (size > 0) {
      v.reserve(size - 1);
      for (size_t i = 0; i < size - 1; ++i) {
        v.emplace_back(lookup(i));
      }
    }
    return v;
  }

  bool is_packed() const override { return true; }

  size_t unpacked_size() const override {
    size_t unpacked = 0;
    auto size = PackedIndex ? index_.size() : st_.index.size();
    for (size_t i = 0; i < size - 1; ++i) {
      unpacked += lookup(i).size();
    }
    return unpacked;
  }

 private:
  metadata::domain::string_table const& st_;
  char const* const buffer_;
  std::vector<uint32_t> index_;
  std::optional<fsst_decoder> dec_;
};

string_table::string_table(std::vector<std::string> const& legacy)
    : impl_{std::make_unique<legacy_string_table>(legacy)} {}

namespace {

std::unique_ptr<string_table::impl>
build_string_table(logger& lgr, std::string_view name,
                   metadata::domain::string_table const& st) {
  if (st.symtab.has_value()) {
    if (st.packed_index) {
      return std::make_unique<packed_string_table<true, true>>(lgr, name, st);
    }
    return std::make_unique<packed_string_table<true, false>>(lgr, name, st);
  }
  if (st.packed_index) {
    return std::make_unique<packed_string_table<false, true>>(lgr, name, st);
  }
  return std::make_unique<packed_string_table<false, false>>(lgr, name, st);
}

} // namespace

string_table::string_table(logger& lgr, std::string_view name,
                           metadata::domain::string_table const& st)
    : impl_{build_string_table(lgr, name, st)} {}

template <typename T>
metadata::domain::string_table
string_table::pack_generic(std::span<T const> input,
                           pack_options const& options) {
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
    output.index.resize(size);
    for (size_t i = 0; i < size; ++i) {
      output.index[i] = res->compressed_data[i].size();
    }
  } else {
    // store uncompressed
    auto const total_input_size =
        std::accumulate(input.begin(), input.end(), size_t{0},
                        [](size_t n, auto const& s) { return n + s.size(); });
    output.buffer.reserve(total_input_size);
    output.index.reserve(size);
    for (auto const& s : input) {
      output.buffer += s;
      output.index.emplace_back(s.size());
    }
  }

  output.packed_index = options.pack_index;

  if (!options.pack_index) {
    output.index.insert(output.index.begin(), 0);
    std::partial_sum(output.index.begin(), output.index.end(),
                     output.index.begin());
  }

  return output;
}

metadata::domain::string_table
string_table::pack(std::span<std::string const> input,
                   pack_options const& options) {
  return pack_generic(input, options);
}

metadata::domain::string_table
string_table::pack(std::span<std::string_view const> input,
                   pack_options const& options) {
  return pack_generic(input, options);
}

} // namespace dwarfs::internal
