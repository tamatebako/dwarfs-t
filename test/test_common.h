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
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <dwarfs/writer/filesystem_writer_options.h>
#include <dwarfs/writer/scanner_options.h>
#include <dwarfs/writer/segmenter_factory.h>

namespace dwarfs {
class logger;

namespace writer {
class entry_filter;
struct writer_progress;
} // namespace writer
} // namespace dwarfs

namespace dwarfs::test {

class filter_transformer_data;
class os_access_mock;

// Default file hash algorithm used in tests
extern std::string const default_file_hash_algo;

// Compression algorithms available for testing
extern std::vector<std::string> const compressions;

/**
 * Build a DwarFS filesystem image from mock input
 *
 * This is the main helper function for creating filesystem images in tests.
 * It handles scanner setup, segmentation, and compression.
 *
 * @param lgr Logger instance
 * @param input Mock OS access with test data
 * @param compression Compression algorithm to use
 * @param cfg Segmenter configuration
 * @param options Scanner options
 * @param writer_opts Filesystem writer options
 * @param prog Writer progress tracker (optional)
 * @param ftd Filter transformer data (optional)
 * @param input_list Optional list of specific paths to include
 * @param filter Entry filter to apply (optional)
 * @return Filesystem image as string
 */
std::string
build_dwarfs(logger& lgr, std::shared_ptr<os_access_mock> input,
             std::string const& compression,
             writer::segmenter::config const& cfg = writer::segmenter::config(),
             writer::scanner_options const& options = writer::scanner_options(),
             writer::filesystem_writer_options const& writer_opts =
                 writer::filesystem_writer_options(),
             writer::writer_progress* prog = nullptr,
             std::shared_ptr<filter_transformer_data> ftd = nullptr,
             std::optional<std::span<std::filesystem::path const>> input_list =
                 std::nullopt,
             std::unique_ptr<writer::entry_filter> filter = nullptr);

} // namespace dwarfs::test