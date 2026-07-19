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
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include <dwarfs/metadata/serialization/serialization_format.h>
#include <dwarfs/writer/scanner_options.h>
#include <dwarfs/writer/segmenter_factory.h>

namespace dwarfs::test {

class os_access_mock;
class logger;

/**
 * Cached test fixture manager using Singleton pattern.
 *
 * This class manages cached filesystem images for tests, avoiding the need
 * to rebuild images on every test run. Images are cached on disk and reused
 * across test executions.
 *
 * Key design principles:
 * - Single Responsibility: Each image has one purpose
 * - Open/Closed: Easy to add new images without modifying core
 * - Strategy Pattern: Generator functions create images on demand
 * - Thread-safe: Mutex protects concurrent access
 */
class CachedTestFixtures {
 public:
  using generator_func = std::function<std::filesystem::path(
      metadata::serialization::SerializationFormat)>;

  /**
   * Get the singleton instance
   */
  static CachedTestFixtures& instance();

  /**
   * Get or create a cached filesystem image.
   *
   * @param name The name of the test fixture (e.g., "basic_test_data")
   * @param format The metadata serialization format to use
   * @return Path to the cached filesystem image
   */
  std::filesystem::path get_image(
      std::string const& name,
      metadata::serialization::SerializationFormat format =
          metadata::serialization::SerializationFormat::FLATBUFFERS);

  /**
   * Force regeneration of all cached images.
   * Useful for test suite re-runs or when test data changes.
   */
  void regenerate_all();

  /**
   * Force regeneration of a specific image.
   *
   * @param name The name of the fixture to regenerate
   * @param format The format of the image to regenerate
   */
  void regenerate(std::string const& name,
                  metadata::serialization::SerializationFormat format);

  /**
   * Register a generator func for a named fixture.
   *
   * @param name The fixture name
   * @param generator Function that creates the filesystem image
   */
  void register_generator(std::string name, generator_func generator);

  /**
   * Get the cache directory path
   */
  std::filesystem::path cache_dir() const { return cache_dir_; }

  /**
   * Get format suffix for filenames (public utility)
   */
  static std::string format_suffix(
      metadata::serialization::SerializationFormat format);

  // Delete copy/move constructors and assignment operators
  CachedTestFixtures(CachedTestFixtures const&) = delete;
  CachedTestFixtures& operator=(CachedTestFixtures const&) = delete;
  CachedTestFixtures(CachedTestFixtures&&) = delete;
  CachedTestFixtures& operator=(CachedTestFixtures&&) = delete;

 private:
  CachedTestFixtures();
  ~CachedTestFixtures();

  /**
   * Check if a cached image exists and is valid
   */
  bool is_cached(std::string const& name,
                 metadata::serialization::SerializationFormat format) const;

  /**
   * Get the cache file path for a specific image
   */
  std::filesystem::path get_cache_path(
      std::string const& name,
      metadata::serialization::SerializationFormat format) const;

  std::filesystem::path cache_dir_;
  std::mutex mutex_;
  std::unordered_map<std::string, generator_func> generators_;
};

/**
 * Helper macro for registering test fixtures at static initialization.
 *
 * Example usage:
 *   REGISTER_TEST_FIXTURE(basic_test_data, generate_basic_test_data);
 */
#define REGISTER_TEST_FIXTURE(name, generator)                                 \
  namespace {                                                                  \
  struct fixture_registrar_##name {                                            \
    fixture_registrar_##name() {                                               \
      ::dwarfs::test::CachedTestFixtures::instance().register_generator(       \
          #name, generator);                                                   \
    }                                                                          \
  };                                                                           \
  static fixture_registrar_##name fixture_registrar_##name##_instance;         \
  }

/**
 * Standard fixture generators.
 * These create common test filesystem images.
 */

/**
 * Generate the basic test data fixture (same as create_test_instance()).
 * This is the most commonly used test data in dwarfs_test.cpp
 */
std::filesystem::path generate_basic_test_data(
    metadata::serialization::SerializationFormat format);

/**
 * Generate a fixture for testing UID/GID edge cases
 */
std::filesystem::path generate_uid_gid_test(
    metadata::serialization::SerializationFormat format);

/**
 * Generate a fixture for testing symlink structures
 */
std::filesystem::path generate_symlink_test(
    metadata::serialization::SerializationFormat format);

/**
 * Generate an empty filesystem (for regression tests)
 */
std::filesystem::path generate_empty_fs(
    metadata::serialization::SerializationFormat format);

} // namespace dwarfs::test