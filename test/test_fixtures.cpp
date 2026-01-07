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

#include "test_fixtures.h"

#include <cstdlib>
#include <fstream>
#include <sstream>

#include <dwarfs/block_compressor.h>
#include <dwarfs/logger.h>
#include <dwarfs/thread_pool.h>
#include <dwarfs/writer/entry_factory.h>
#include <dwarfs/writer/filesystem_writer.h>
#include <dwarfs/writer/filesystem_writer_options.h>
#include <dwarfs/writer/scanner.h>
#include <dwarfs/writer/writer_progress.h>

#include "test_helpers.h"
#include "test_logger.h"

namespace dwarfs::test {

namespace {

// Helper function to build a dwarfs image and write to disk
std::filesystem::path build_and_save_dwarfs(
    std::filesystem::path const& output_path,
    std::shared_ptr<os_access_mock> input,
    metadata::serialization::SerializationFormat format,
    writer::segmenter::config const& cfg = writer::segmenter::config(),
    writer::scanner_options const& options = writer::scanner_options(),
    writer::filesystem_writer_options const& writer_opts =
        writer::filesystem_writer_options()) {

  test_logger lgr;
  thread_pool pool(lgr, *input, "worker", 4);

  writer::writer_progress wprog;

  // Set metadata format
  writer::scanner_options opts = options;
  opts.metadata_format = format;

  // Configure segmenter
  writer::segmenter_factory::config sf_cfg;
  sf_cfg.block_size_bits = cfg.block_size_bits;
  sf_cfg.blockhash_window_size.set_default(cfg.blockhash_window_size);
  sf_cfg.window_increment_shift.set_default(cfg.window_increment_shift);
  sf_cfg.max_active_blocks.set_default(cfg.max_active_blocks);
  sf_cfg.bloom_filter_size.set_default(cfg.bloom_filter_size);

  writer::segmenter_factory sf(lgr, wprog, sf_cfg);
  writer::entry_factory ef;
  writer::scanner s(lgr, pool, sf, ef, *input, opts);

  // Create output file
  std::ofstream ofs(output_path, std::ios::binary);
  if (!ofs) {
    throw std::runtime_error("Failed to create output file: " +
                             output_path.string());
  }

  block_compressor bc("null"); // Use null compression for faster tests
  writer::filesystem_writer fsw(ofs, lgr, pool, wprog, writer_opts);
  fsw.add_default_compressor(bc);

  s.scan(fsw, std::filesystem::path("/"), wprog);

  ofs.close();

  return output_path;
}

} // anonymous namespace

// Singleton instance
CachedTestFixtures& CachedTestFixtures::instance() {
  static CachedTestFixtures instance;
  return instance;
}

CachedTestFixtures::CachedTestFixtures() {
  // Determine cache directory
  if (auto env_cache = std::getenv("DWARFS_TEST_CACHE_DIR")) {
    cache_dir_ = env_cache;
  } else {
    // Default to build directory
    cache_dir_ = std::filesystem::current_path() / "test_fixtures_cache";
  }

  // Create cache directory if it doesn't exist
  std::filesystem::create_directories(cache_dir_);
}

CachedTestFixtures::~CachedTestFixtures() = default;

std::filesystem::path CachedTestFixtures::get_image(
    std::string const& name,
    metadata::serialization::SerializationFormat format) {

  std::lock_guard<std::mutex> lock(mutex_);

  // Check if image is cached
  if (is_cached(name, format)) {
    return get_cache_path(name, format);
  }

  // Find generator
  auto it = generators_.find(name);
  if (it == generators_.end()) {
    throw std::runtime_error("No generator registered for fixture: " + name);
  }

  // Generate image
  return it->second(format);
}

void CachedTestFixtures::regenerate_all() {
  std::lock_guard<std::mutex> lock(mutex_);

  // Remove all cached files
  if (std::filesystem::exists(cache_dir_)) {
    for (auto const& entry : std::filesystem::directory_iterator(cache_dir_)) {
      if (entry.is_regular_file() && entry.path().extension() == ".dwarfs") {
        std::filesystem::remove(entry.path());
      }
    }
  }
}

void CachedTestFixtures::regenerate(
    std::string const& name,
    metadata::serialization::SerializationFormat format) {

  std::lock_guard<std::mutex> lock(mutex_);

  auto cache_path = get_cache_path(name, format);
  if (std::filesystem::exists(cache_path)) {
    std::filesystem::remove(cache_path);
  }
}

void CachedTestFixtures::register_generator(std::string name,
                                            generator_func generator) {
  std::lock_guard<std::mutex> lock(mutex_);
  generators_[std::move(name)] = std::move(generator);
}

bool CachedTestFixtures::is_cached(
    std::string const& name,
    metadata::serialization::SerializationFormat format) const {

  auto cache_path = get_cache_path(name, format);
  return std::filesystem::exists(cache_path) &&
         std::filesystem::is_regular_file(cache_path);
}

std::filesystem::path CachedTestFixtures::get_cache_path(
    std::string const& name,
    metadata::serialization::SerializationFormat format) const {

  return cache_dir_ / (name + format_suffix(format) + ".dwarfs");
}

std::string CachedTestFixtures::format_suffix(
    metadata::serialization::SerializationFormat format) {
  switch (format) {
    case metadata::serialization::SerializationFormat::FLATBUFFERS:
      return ".fb";
    case metadata::serialization::SerializationFormat::MODERN_THRIFT:
      return ".thrift";
    default:
      return "";
  }
}

// Standard fixture generators

std::filesystem::path generate_basic_test_data(
    metadata::serialization::SerializationFormat format) {

  auto input = os_access_mock::create_test_instance();
  auto& fixtures = CachedTestFixtures::instance();
  auto output_path = fixtures.cache_dir() /
                     ("basic_test_data" +
                      CachedTestFixtures::format_suffix(format) + ".dwarfs");

  writer::segmenter::config cfg;
  cfg.blockhash_window_size = 10;
  cfg.block_size_bits = 15;

  return build_and_save_dwarfs(output_path, input, format, cfg);
}

std::filesystem::path generate_uid_gid_test(
    metadata::serialization::SerializationFormat format) {

  auto input = std::make_shared<os_access_mock>();

  input->add("", {1, 040755, 1, 0, 0, 10, 42, 0, 0, 0});
  input->add("foo16.txt", {2, 0100755, 1, 60000, 65535, 5, 42, 0, 0, 0},
             "hello");
  input->add("foo32.txt", {3, 0100755, 1, 65536, 4294967295, 5, 42, 0, 0, 0},
             "world");

  auto& fixtures = CachedTestFixtures::instance();
  auto output_path = fixtures.cache_dir() /
                     ("uid_gid_test" +
                      CachedTestFixtures::format_suffix(format) + ".dwarfs");

  return build_and_save_dwarfs(output_path, input, format);
}

std::filesystem::path generate_symlink_test(
    metadata::serialization::SerializationFormat format) {

  auto input = std::make_shared<os_access_mock>();

  input->add_dir("");
  input->add_dir("dir1");
  input->add_file("dir1/file1.txt", "content1");
  input->add("link1", {10, 0120777, 1, 1000, 100, 11, 42, 0, 0, 0},
             "dir1/file1.txt");
  input->add("link2", {11, 0120777, 1, 1000, 100, 6, 42, 0, 0, 0},
             "../dir1");

  auto& fixtures = CachedTestFixtures::instance();
  auto output_path = fixtures.cache_dir() /
                     ("symlink_test" +
                      CachedTestFixtures::format_suffix(format) + ".dwarfs");

  return build_and_save_dwarfs(output_path, input, format);
}

std::filesystem::path generate_empty_fs(
    metadata::serialization::SerializationFormat format) {

  auto input = std::make_shared<os_access_mock>();
  input->add_dir("");

  auto& fixtures = CachedTestFixtures::instance();
  auto output_path = fixtures.cache_dir() /
                     ("empty_fs" +
                      CachedTestFixtures::format_suffix(format) + ".dwarfs");

  writer::segmenter::config cfg;
  cfg.blockhash_window_size = 8;
  cfg.block_size_bits = 10;

  writer::scanner_options opts;
  opts.metadata.pack_chunk_table = true;
  opts.metadata.pack_directories = true;
  opts.metadata.pack_shared_files_table = true;
  opts.metadata.pack_names = true;
  opts.metadata.pack_names_index = true;
  opts.metadata.pack_symlinks = true;
  opts.metadata.pack_symlinks_index = true;
  opts.metadata.force_pack_string_tables = true;

  return build_and_save_dwarfs(output_path, input, format, cfg, opts);
}

// Auto-register standard fixtures
REGISTER_TEST_FIXTURE(basic_test_data, generate_basic_test_data)
REGISTER_TEST_FIXTURE(uid_gid_test, generate_uid_gid_test)
REGISTER_TEST_FIXTURE(symlink_test, generate_symlink_test)
REGISTER_TEST_FIXTURE(empty_fs, generate_empty_fs)

} // namespace dwarfs::test