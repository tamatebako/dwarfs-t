/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     DwarFS Implementation
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
 */

#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <functional>
#include <span>
#include "fixture_generator.h"

namespace dwarfs::test::compat {

/**
 * \brief Fixture checksum information
 */
struct FixtureChecksum {
  std::string algorithm;  // "sha256"
  std::string hash;
  size_t size;
  std::string platform_arch;
};

/**
 * \brief Manages generated DFT file fixtures
 *
 * This class provides a caching layer for generated fixture files,
 * including validation, storage, and retrieval.
 */
class FixtureCache {
public:
  /**
   * \brief Constructor
   *
   * \param cache_dir Directory for cached fixtures
   */
  explicit FixtureCache(const std::string& cache_dir = "test/compat/fixtures");

  ~FixtureCache() = default;

  /**
   * \brief Check if a valid fixture exists
   *
   * \param spec Fixture specification
   * \return true if fixture exists and passes validation
   */
  bool has_valid_fixture(const FixtureSpec& spec) const;

  /**
   * \brief Load a fixture from cache
   *
   * \param spec Fixture specification
   * \return Fixture data, or empty if not found
   * \throws std::runtime_error if fixture doesn't exist
   */
  std::vector<uint8_t> load(const FixtureSpec& spec);

  /**
   * \brief Store a fixture in cache
   *
   * \param spec Fixture specification
   * \param data Fixture data
   * \throws std::runtime_error if storage fails
   */
  void store(const FixtureSpec& spec, std::span<uint8_t const> data);

  /**
   * \brief Validate fixture checksum
   *
   * \param spec Fixture specification
   * \return true if checksum is valid
   */
  bool validate_checksum(const FixtureSpec& spec) const;

  /**
   * \brief Get fixture checksum
   *
   * \param spec Fixture specification
   * \return Checksum information
   * \throws std::runtime_error if fixture doesn't exist
   */
  FixtureChecksum get_checksum(const FixtureSpec& spec) const;

  /**
   * \brief Invalidate a cached fixture
   *
   * \param spec Fixture specification
   */
  void invalidate(const FixtureSpec& spec);

  /**
   * \brief Clear all cached fixtures
   */
  void clear();

  /**
   * \brief Get cache size in bytes
   *
   * \return Total size of all cached fixtures
   */
  size_t get_cache_size() const;

  /**
   * \brief Get number of cached fixtures
   *
   * \return Number of fixtures in cache
   */
  size_t get_fixture_count() const;

  /**
   * \brief List all cached fixtures
   *
   * \return List of fixture specifications
   */
  std::vector<FixtureSpec> list_fixtures() const;

  /**
   * \brief Load checksum database from file
   *
   * \param checksums_path Path to checksums file
   * \return true if loaded successfully
   */
  bool load_checksums(const std::string& checksums_path);

  /**
   * \brief Save checksum database to file
   *
   * \param checksums_path Path to checksums file
   * \return true if saved successfully
   */
  bool save_checksums(const std::string& checksums_path);

  /**
   * \brief Set auto-generation flag
   *
   * When true, missing fixtures will be generated automatically.
   * When false, missing fixtures will cause an error.
   */
  void set_auto_generate(bool auto_generate) {
    auto_generate_ = auto_generate;
  }

private:
  /**
   * \brief Calculate SHA256 checksum
   */
  std::string calculate_sha256(std::span<uint8_t const> data) const;

  /**
   * \brief Calculate SHA256 checksum from file
   */
  std::string calculate_sha256_from_file(const std::string& path) const;

  /**
   * \brief Get checksum file path for a fixture
   */
  std::string get_checksum_path(const FixtureSpec& spec) const;

  std::string cache_dir_;
  bool auto_generate_ = true;
  std::unordered_map<std::string, FixtureChecksum> checksums_;
};

} // namespace dwarfs::test::compat
