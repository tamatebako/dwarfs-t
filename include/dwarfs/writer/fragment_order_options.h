/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Marcus Holland-Moritz (github@mhxnet.de)
 * \copyright  Copyright (c) Marcus Holland-Moritz
 *
 * This file is part of dwarfs.
 *
 * dwarfs is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * dwarfs is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with dwarfs.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <filesystem>
#include <iosfwd>
#include <string>
#include <unordered_map>

namespace dwarfs::writer {

enum class fragment_order_mode {
  NONE,
  PATH,
  REVPATH,
  SIMILARITY,
  NILSIMSA,
  EXPLICIT
};

// std::hash<std::filesystem::path> is unavailable with some standard
// library versions (e.g. older libc++), so use the portable
// std::filesystem::hash_value explicitly
struct fs_path_hash {
  auto operator()(std::filesystem::path const& p) const noexcept {
    return std::filesystem::hash_value(p);
  }
};

struct fragment_order_options {
  static constexpr int const kDefaultNilsimsaMaxChildren{16384};
  static constexpr int const kDefaultNilsimsaMaxClusterSize{16384};

  fragment_order_mode mode{fragment_order_mode::NONE};
  int nilsimsa_max_children{kDefaultNilsimsaMaxChildren};
  int nilsimsa_max_cluster_size{kDefaultNilsimsaMaxClusterSize};
  std::string explicit_order_file{};
  std::unordered_map<std::filesystem::path, size_t, fs_path_hash>
      explicit_order{};
};

std::ostream& operator<<(std::ostream& os, fragment_order_mode mode);

} // namespace dwarfs::writer
