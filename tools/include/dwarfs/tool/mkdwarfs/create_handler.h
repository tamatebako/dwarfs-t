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

#include <functional>
#include <optional>
#include <vector>

#include <dwarfs/tool/mkdwarfs/parsed_options.h>
#include <dwarfs/tool/mkdwarfs/handler_interface.h>

namespace dwarfs {

class library_dependencies;

namespace writer {
class console_writer;
class filesystem_writer;
class writer_progress;
class rule_based_entry_filter;
} // namespace writer

namespace tool {

class iolayer;

namespace mkdwarfs {

/**
 * Handler for filesystem creation operations
 *
 * Extracts scanner setup and scan execution logic from the main function.
 * Takes parsed options and runtime objects as parameters, not creating them.
 */
class create_handler : public handler_interface {
 public:
  create_handler() = default;

  /**
   * Execute filesystem creation (scan and write)
   *
   * @param opts Parsed command-line options
   * @param iol I/O layer for file access
   * @param console Console writer for progress and diagnostics
   * @param prog Writer progress tracker (runtime object from main)
   * @param fsw Filesystem writer (runtime object from main)
   * @param filter Optional entry filter for selective scanning
   * @param extra_deps Library dependencies callback
   * @return 0 on success, error code otherwise
   */
  int run(parsed_options const& opts, iolayer const& iol,
          ::dwarfs::writer::console_writer& console,
          ::dwarfs::writer::writer_progress& prog,
          ::dwarfs::writer::filesystem_writer& fsw,
          ::dwarfs::writer::rule_based_entry_filter* filter,
          std::function<void(library_dependencies&)> const& extra_deps) override;

 private:
  // Future: Add helper methods if needed for complex setup
};

} // namespace mkdwarfs
} // namespace tool
} // namespace dwarfs