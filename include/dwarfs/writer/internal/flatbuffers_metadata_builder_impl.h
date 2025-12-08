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

// Domain model types
#include <dwarfs/metadata/domain/metadata.h>

// Processor interfaces (needed for unique_ptr members)
#include <dwarfs/writer/internal/metadata_chunk_processor.h>
#include <dwarfs/writer/internal/metadata_entry_processor.h>
#include <dwarfs/writer/internal/metadata_packing_processor.h>
#include <dwarfs/writer/internal/metadata_upgrade_processor.h>

// Forward declarations (outside namespace to avoid pollution)
namespace dwarfs::writer {
struct metadata_options;
}

namespace dwarfs::writer::internal {
class time_resolution_converter;
class dir;
class global_entry_data;
class inode_manager;
class block_manager;
struct block_mapping;
}

namespace dwarfs::writer::internal {

/**
 * FlatBuffers-specific metadata builder implementation.
 * 
 * This template class implements the Strategy pattern for FlatBuffers
 * metadata format. It uses composition to delegate work to specialized
 * processor objects:
 * - chunk_processor: Handles chunk gathering and block remapping
 * - entry_processor: Handles directory entry processing
 * - packing_processor: Handles metadata packing/compression
 * - upgrade_processor: Handles metadata version upgrades
 * 
 * KEY ADVANTAGE: Works directly with domain model - no conversion needed!
 * 
 * Template Parameters:
 * - LoggerPolicy: Logging policy (debug_logger_policy or prod_logger_policy)
 */
template <typename LoggerPolicy>
class flatbuffers_metadata_builder final : public metadata_builder::impl {
 public:
  using uid_type = file_stat::uid_type;
  using gid_type = file_stat::gid_type;

  /**
   * Constructor for creating new filesystem.
   * 
   * @param lgr Logger instance
   * @param options Metadata options
   */
  flatbuffers_metadata_builder(logger& lgr, metadata_options const& options);

  /**
   * Constructor for recompressing existing filesystem (const metadata).
   * 
   * @param lgr Logger instance
   * @param md Existing metadata (copied)
   * @param orig_fs_options Original filesystem options
   * @param orig_fs_version Original filesystem version
   * @param options New metadata options
   */
  flatbuffers_metadata_builder(logger& lgr,
                               ::dwarfs::metadata::domain::metadata const& md,
                               ::dwarfs::metadata::domain::fs_options const* orig_fs_options,
                               filesystem_version const& orig_fs_version,
                               metadata_options const& options);

  /**
   * Constructor for recompressing existing filesystem (move metadata).
   *
   * @param lgr Logger instance
   * @param md Existing metadata (moved)
   * @param orig_fs_options Original filesystem options
   * @param orig_fs_version Original filesystem version
   * @param options New metadata options
   */
  flatbuffers_metadata_builder(logger& lgr,
                               ::dwarfs::metadata::domain::metadata&& md,
                               ::dwarfs::metadata::domain::fs_options const* orig_fs_options,
                               filesystem_version const& orig_fs_version,
                               metadata_options const& options);

  /**
   * Destructor.
   * Must be explicitly declared/defined in .cpp due to Pimpl with unique_ptr.
   */
  ~flatbuffers_metadata_builder();

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
   * Build final metadata domain model.
   *
   * KEY ADVANTAGE: Returns domain model directly - no conversion needed!
   *
   * @return Completed metadata domain model
   */
  ::dwarfs::metadata::domain::metadata build() override;

 private:
  /**
   * Initialize processors (called by constructors).
   * Creates processor instances that will be used to handle
   * the various metadata operations.
   */
  void initialize_processors();

  /**
   * Common initialization for recompression constructors.
   * Sets up feature set, validates options, upgrades metadata.
   */
  void initialize_for_recompression(
      ::dwarfs::metadata::domain::fs_options const* orig_fs_options,
      filesystem_version const& orig_fs_version);

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

  LOG_PROXY_DECL(LoggerPolicy);
  
  // Domain model (built directly!)
  ::dwarfs::metadata::domain::metadata md_;
  
  // Processors (composition pattern)
  std::unique_ptr<metadata_chunk_processor> chunk_proc_;
  std::unique_ptr<metadata_entry_processor> entry_proc_;
  std::unique_ptr<metadata_packing_processor> pack_proc_;
  std::unique_ptr<metadata_upgrade_processor> upgrade_proc_;
  
  // Configuration
  dwarfs::internal::feature_set features_;
  metadata_options const& options_;
  std::optional<uint32_t> old_block_size_;
  std::unique_ptr<time_resolution_converter> timeres_;
};

} // namespace dwarfs::writer::internal