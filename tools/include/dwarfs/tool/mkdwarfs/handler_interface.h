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
#include <memory>

namespace dwarfs {

class library_dependencies;

namespace writer {
class console_writer;
class writer_progress;
class filesystem_writer;
class rule_based_entry_filter;
} // namespace writer

namespace tool {

struct iolayer;

namespace mkdwarfs {

struct parsed_options;

/**
 * Abstract interface for mkdwarfs operation handlers
 *
 * Each handler implements a specific operation mode:
 * - create_handler: Creates new filesystem from directory
 * - recompress_handler: Recompresses existing filesystem (requires Thrift)
 *
 * This interface enables the factory pattern for clean handler selection
 * and makes it easy to add new operation modes in the future.
 */
class handler_interface {
 public:
  virtual ~handler_interface() = default;

  /**
   * Execute the handler operation
   *
   * @param opts Parsed command-line options
   * @param iol I/O layer for file access
   * @param console Console writer for progress display and logging
   * @param prog Writer progress tracker
   * @param fsw Filesystem writer for output
   * @param entry_filter Optional entry filter for create operations (can be nullptr)
   * @param extra_deps Callback to add extra library dependencies
   * @return 0 on success, error code otherwise
   */
  virtual int run(parsed_options const& opts, iolayer const& iol,
                  ::dwarfs::writer::console_writer& console,
                  ::dwarfs::writer::writer_progress& prog,
                  ::dwarfs::writer::filesystem_writer& fsw,
                  ::dwarfs::writer::rule_based_entry_filter* entry_filter,
                  std::function<void(library_dependencies&)> const& extra_deps) = 0;
};

} // namespace mkdwarfs
} // namespace tool
} // namespace dwarfs