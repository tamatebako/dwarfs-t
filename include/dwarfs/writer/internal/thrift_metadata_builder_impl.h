/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Marcus Holland-Moritz (github@mhxnet.de)
 * \author     Ribose Inc. (OOP refactoring)
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

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <dwarfs/file_stat.h>
#include <dwarfs/logger.h>
#include <dwarfs/version.h>
#include <dwarfs/writer/internal/metadata_builder.h>

#include <dwarfs/internal/features.h>

// Thrift types
#include <dwarfs/gen-cpp2/metadata_types.h>

// Forward declarations (outside namespace to avoid pollution)
namespace dwarfs::writer {
struct metadata_options;
}

namespace dwarfs::writer::internal {
class dir;
class global_entry_data;
class inode_manager;
class block_manager;
struct block_mapping;
}

// Include complete type for unique_ptr member
#include <dwarfs/writer/internal/time_resolution_converter.h>

namespace dwarfs::writer::internal {

/**
 * Thrift-specific metadata builder implementation.
 * 
 * This template class implements the Strategy pattern for Apache Thrift
 * Compact metadata format. This is the legacy format, maintained for
 * backward compatibility.
 * 
 * KEY DIFFERENCE: Works with Thrift types internally, converts to domain
 * model at the end via converters.
 * 
 * Template Parameters:
 * - LoggerPolicy: Logging policy (debug_logger_policy or prod_logger_policy)
 * 
 * @see flatbuffers_metadata_builder for the modern default format
 */
template <typename LoggerPolicy>
class thrift_metadata_builder final : public metadata_builder::impl {
 public:
  using uid_type = file_stat::uid_type;
  using gid_type = file_stat::gid_type;

  /**
   * Constructor for creating new filesystem.
   * 
   * @param lgr Logger instance
   * @param options Metadata options
   */
  thrift_metadata_builder(logger& lgr, metadata_options const& options);

  /**
   * Constructor for recompressing existing filesystem (from Thrift metadata).
   *
   * Uses perfect forwarding to accept both const& and && versions of
   * Thrift metadata.
   *
   * @param lgr Logger instance
   * @param md Existing Thrift metadata (forwarded)
   * @param orig_fs_options Original filesystem options (Thrift)
   * @param orig_fs_version Original filesystem version
   * @param options New metadata options
   */
  template <typename T>
    requires(std::same_as<std::decay_t<T>, thrift::metadata::metadata>)
  thrift_metadata_builder(logger& lgr, T&& md,
                         thrift::metadata::fs_options const* orig_fs_options,
                         filesystem_version const& orig_fs_version,
                         metadata_options const& options);

  /**
   * Constructor for recompressing from domain model (const version).
   * Converts domain model to Thrift format internally.
   *
   * @param lgr Logger instance
   * @param md Existing domain metadata
   * @param orig_fs_options Original filesystem options (domain)
   * @param orig_fs_version Original filesystem version
   * @param options New metadata options
   */
  thrift_metadata_builder(logger& lgr,
                         ::dwarfs::metadata::domain::metadata const& md,
                         ::dwarfs::metadata::domain::fs_options const* orig_fs_options,
                         filesystem_version const& orig_fs_version,
                         metadata_options const& options);

  /**
   * Constructor for recompressing from domain model (move version).
   * Converts domain model to Thrift format internally.
   *
   * @param lgr Logger instance
   * @param md Existing domain metadata (moved)
   * @param orig_fs_options Original filesystem options (domain)
   * @param orig_fs_version Original filesystem version
   * @param options New metadata options
   */
  thrift_metadata_builder(logger& lgr,
                         ::dwarfs::metadata::domain::metadata&& md,
                         ::dwarfs::metadata::domain::fs_options const* orig_fs_options,
                         filesystem_version const& orig_fs_version,
                         metadata_options const& options);

  /**
   * Destructor.
   */
  ~thrift_metadata_builder() = default;

  // Implement metadata_builder::impl interface
  void set_devices(std::vector<uint64_t> devices) override;
  void set_symlink_table_size(size_t size) override;
  void set_block_size(uint32_t block_size) override;
  void set_shared_files_table(std::vector<uint32_t> shared_files) override;
  void set_category_names(std::vector<std::string> category_names) override;
  void set_block_categories(std::vector<uint32_t> block_categories) override;
  void set_category_metadata_json(std::vector<std::string> metadata_json) override;
  void set_block_category_metadata(std::map<uint32_t, uint32_t> block_metadata) override;
  void add_symlink_table_entry(size_t index, uint32_t entry) override;

  void gather_chunks(::dwarfs::writer::internal::inode_manager const& im,
                     ::dwarfs::writer::internal::block_manager const& bm,
                     size_t chunk_count) override;
  void gather_entries(std::span<::dwarfs::writer::internal::dir*> dirs,
                      ::dwarfs::writer::internal::global_entry_data const& ge_data,
                      uint32_t num_inodes) override;
  void gather_global_entry_data(::dwarfs::writer::internal::global_entry_data const& ge_data) override;
  void remap_blocks(std::span<::dwarfs::writer::internal::block_mapping const> mapping,
                    size_t new_block_count) override;

  /**
   * Build final metadata.
   * 
   * KEY DIFFERENCE: Builds Thrift metadata internally, then converts to
   * domain model before returning.
   *
   * @return Completed metadata domain model (converted from Thrift)
   */
  ::dwarfs::metadata::domain::metadata build() override;

 private:
  // Type aliases for Thrift types
  using chunks_t = typename decltype(std::declval<thrift::metadata::metadata>()
                                         .chunks())::value_type;
  using chunk_table_t =
      typename decltype(std::declval<thrift::metadata::metadata>()
                            .chunk_table())::value_type;
  using categories_t =
      typename decltype(std::declval<thrift::metadata::metadata>()
                            .block_categories())::value_type;
  using category_metadata_t =
      typename decltype(std::declval<thrift::metadata::metadata>()
                            .block_category_metadata())::value_type;

  static constexpr auto kTmpHoleIx = std::numeric_limits<
      typename decltype(std::declval<thrift::metadata::metadata>()
                            .chunks()[0]
                            .block())::value_type>::max();

  /**
   * Build Thrift metadata (internal helper).
   * Builds the Thrift representation before conversion to domain model.
   * 
   * @return Reference to internal Thrift metadata
   */
  thrift::metadata::metadata const& build_thrift_internal();

  /**
   * Remap holes after block remapping.
   */
  void remap_holes(chunks_t& new_chunks, size_t new_hole_index,
                   size_t max_data_chunk_size);

  /**
   * Upgrade metadata from older versions.
   */
  void upgrade_metadata(thrift::metadata::fs_options const* orig_fs_options,
                        filesystem_version const& orig_fs_version);

  /**
   * Upgrade from pre-v2.2 format.
   * Converts entry_table_v2_2 to dir_entries.
   */
  void upgrade_from_pre_v2_2();

  /**
   * Update inodes according to options.
   * Handles UID/GID overrides, timestamp updates, time resolution
   * conversion, and chmod transformations.
   */
  void update_inodes();

  /**
   * Update nlink fields in inodes.
   * Calculated from dir_entries if hardlink table is enabled.
   */
  void update_nlink();

  /**
   * Update total sizes and inode size cache.
   * Calculates total_fs_size, total_allocated_fs_size,
   * total_hardlink_size, and populates reg_file_size_cache.
   */
  void update_totals_and_size_cache();

  /**
   * Apply chmod transformations to modes.
   * Uses chmod_transformer to apply chmod specifications.
   */
  void apply_chmod();

  /**
   * Get time resolution in seconds.
   */
  uint32_t get_time_resolution() const;

  /**
   * Get subsecond multiplier in nanoseconds.
   */
  uint32_t get_subsec_mult() const;

  /**
   * Get time resolution as chrono duration.
   */
  std::chrono::nanoseconds get_chrono_time_resolution() const;

  LOG_PROXY_DECL(LoggerPolicy);

  // Thrift metadata (built internally)
  thrift::metadata::metadata md_;

  // Configuration
  metadata_options const& options_;
  uint32_t old_block_size_{0};
  std::unique_ptr<time_resolution_converter> timeres_;
  dwarfs::internal::feature_set features_;
};

} // namespace dwarfs::writer::internal