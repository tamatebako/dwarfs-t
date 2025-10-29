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

#include <cassert>
#include <string_view>

#include <nlohmann/json.hpp>

#include <dwarfs/file_stat.h>
#include <dwarfs/metadata/domain/metadata.h>
#include <dwarfs/metadata/domain/history_entry.h>

#include <dwarfs/reader/internal/metadata_types.h>
#include <dwarfs/reader/internal/time_resolution_handler.h>

namespace dwarfs::reader::internal {

namespace {

uint32_t get_resolution(auto const& meta) {
  if (meta.options) {
    if (meta.options->time_resolution_sec) {
      assert(*meta.options->time_resolution_sec > 0);
      return *meta.options->time_resolution_sec;
    }
  }

  return 1;
}

uint32_t
get_nsec_multiplier(auto const& meta, uint32_t resolution [[maybe_unused]]) {
  if (meta.options) {
    if (meta.options->subsecond_resolution_nsec_multiplier) {
      assert(resolution == 1);
      assert(*meta.options->subsecond_resolution_nsec_multiplier > 0 &&
             *meta.options->subsecond_resolution_nsec_multiplier < 1'000'000'000);
      return *meta.options->subsecond_resolution_nsec_multiplier;
    }
  }

  return 0;
}

} // namespace

template <typename T>
time_resolution_handler::time_resolution_handler(T const& obj,
                                                 uint64_t const timebase)
    : timebase_{timebase}
    , resolution_{get_resolution(obj)}
    , nsec_multiplier_{get_nsec_multiplier(obj, resolution_)}
    , mtime_only_{obj.options && obj.options->mtime_only} {}

time_resolution_handler::time_resolution_handler(
    metadata::domain::metadata const& meta)
    : time_resolution_handler(meta, meta.timestamp_base) {}

time_resolution_handler::time_resolution_handler(
    metadata::domain::history_entry const& hist)
    : time_resolution_handler(hist, 0) {}

void time_resolution_handler::fill_stat_timevals(
    file_stat& st, inode_view_impl const& ivr) const {
  auto const mtime_nsec =
      nsec_multiplier_ > 0 ? ivr.mtime_subsec() * nsec_multiplier_ : 0;
  st.set_mtimespec(resolution_ * (timebase_ + ivr.mtime_offset()), mtime_nsec);

  if (mtime_only_) {
    st.set_atimespec(st.mtimespec_unchecked());
    st.set_ctimespec(st.mtimespec_unchecked());
  } else {
    auto const atime_nsec =
        nsec_multiplier_ > 0 ? ivr.atime_subsec() * nsec_multiplier_ : 0;
    auto const ctime_nsec =
        nsec_multiplier_ > 0 ? ivr.ctime_subsec() * nsec_multiplier_ : 0;
    st.set_atimespec(resolution_ * (timebase_ + ivr.atime_offset()),
                     atime_nsec);
    st.set_ctimespec(resolution_ * (timebase_ + ivr.ctime_offset()),
                     ctime_nsec);
  }
}

void time_resolution_handler::add_time_resolution_to(nlohmann::json& j) const {
  static constexpr auto kTimeResKey{"time_resolution"};
  if (nsec_multiplier_ > 0) {
    // emit as float
    j[kTimeResKey] = 1e-9 * nsec_multiplier_;
  } else {
    // emit as integer
    j[kTimeResKey] = resolution_;
  }
}

std::string time_resolution_handler::get_time_resolution_string() const {
  if (nsec_multiplier_ > 0) {
    if (nsec_multiplier_ % 1'000'000 == 0) {
      return std::to_string(nsec_multiplier_ / 1'000'000) + " ms";
    }

    if (nsec_multiplier_ % 1'000 == 0) {
      return std::to_string(nsec_multiplier_ / 1'000) + " µs";
    }

    return std::to_string(nsec_multiplier_) + " ns";
  }

  return std::to_string(resolution_) + " seconds";
}

} // namespace dwarfs::reader::internal
