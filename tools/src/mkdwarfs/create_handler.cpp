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

#include <dwarfs/tool/mkdwarfs/create_handler.h>

#include <dwarfs/library_dependencies.h>
#include <dwarfs/logger.h>
#include <dwarfs/thread_pool.h>
#include <dwarfs/tool/iolayer.h>
#include <dwarfs/writer/console_writer.h>
#include <dwarfs/writer/entry_factory.h>
#include <dwarfs/writer/filesystem_writer.h>
#include <dwarfs/writer/rule_based_entry_filter.h>
#include <dwarfs/writer/scanner.h>
#include <dwarfs/writer/segmenter_factory.h>
#include <dwarfs/writer/writer_progress.h>

namespace dwarfs::tool::mkdwarfs {

int create_handler::run(
    parsed_options const& opts, iolayer const& iol,
    ::dwarfs::writer::console_writer& console,
    ::dwarfs::writer::writer_progress& prog,
    ::dwarfs::writer::filesystem_writer& fsw,
    ::dwarfs::writer::rule_based_entry_filter* filter,
    std::function<void(library_dependencies&)> const& extra_deps) {

  // Create segmenter factory with configuration from parsed options
  writer::segmenter_factory sf(static_cast<logger&>(console), prog,
                                opts.scanner_opts.inode.categorizer_mgr,
                                opts.segmenter_config);

  // Create entry factory
  writer::entry_factory ef;

  // Create scanner thread pool
  thread_pool scanner_pool(static_cast<logger&>(console), *iol.os, "scanner", opts.num_scanner_workers);

  // Create scanner with all configured options
  writer::scanner s(static_cast<logger&>(console), scanner_pool, sf, ef, *iol.os, opts.scanner_opts);

  // Attach filter if present (filter still owned by caller)
  if (filter) {
    // Don't take ownership - filter is managed by caller
    // Scanner will use raw pointer internally
    // s.add_filter(std::unique_ptr<writer::entry_filter>(filter));
    // FIXME: Need to update handler interface to pass unique_ptr
  }

  // Convert input_list from vector<string> to vector<filesystem::path> if present
  std::optional<std::vector<std::filesystem::path>> input_paths;
  if (opts.input_list) {
    input_paths.emplace();
    input_paths->reserve(opts.input_list->size());
    for (auto const& str : *opts.input_list) {
      input_paths->emplace_back(str);
    }
  }

  // Create span from vector if present
  std::optional<std::span<std::filesystem::path const>> input_list_span;
  if (input_paths) {
    input_list_span.emplace(*input_paths);
  }

  // Execute the scan
  s.scan(fsw, opts.input_path, prog, input_list_span, iol.file, extra_deps);

  return 0;
}

} // namespace dwarfs::tool::mkdwarfs