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

#include <memory>

namespace dwarfs::tool::mkdwarfs {

class parsed_options;
class handler_interface;

/**
 * Factory for creating mkdwarfs operation handlers
 *
 * Selects the appropriate handler based on parsed options:
 * - create_handler for normal filesystem creation
 * - recompress_handler for recompressing existing images (requires Thrift)
 *
 * This factory pattern eliminates conditional branching in main() and
 * provides a clean extension point for future operation modes.
 */
class handler_factory {
 public:
  /**
   * Create appropriate handler based on options
   *
   * @param opts Parsed command-line options
   * @return Handler instance ready to execute
   * @throws std::runtime_error if invalid state (e.g., recompress without Thrift)
   */
  static std::unique_ptr<handler_interface> create(parsed_options const& opts);
};

} // namespace dwarfs::tool::mkdwarfs