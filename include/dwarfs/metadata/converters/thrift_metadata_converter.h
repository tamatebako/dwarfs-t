#pragma once

#include "dwarfs/metadata/converters/metadata_converter.h"
#include "dwarfs/metadata/domain/metadata.h"

// Forward declare Thrift types to avoid heavy includes in header
namespace dwarfs::thrift::metadata {
  struct metadata;
  struct chunk;
  struct directory;
  struct inode_data;
  struct dir_entry;
  struct fs_options;
  struct string_table;
  struct inode_size_cache;
  struct history_entry;
}

namespace dwarfs::metadata::converters {

/**
 * Converts between Thrift metadata and domain metadata.
 *
 * Single Responsibility: Thrift ↔ Domain conversion only
 * MECE: All Thrift fields mapped to domain fields exactly once
 * Dependencies: Both Thrift and domain types
 *
 * This converter implements bidirectional transformation between
 * the Thrift frozen format and the pure domain model. Each field
 * is converted exactly once, following MECE principles.
 *
 * Thread-safety: This converter is stateless and therefore thread-safe.
 */
class ThriftMetadataConverter : public IMetadataConverter {
public:
  /**
   * Default constructor
   */
  ThriftMetadataConverter() = default;

  // IMetadataConverter interface implementation

  /**
   * Convert Thrift metadata to domain metadata.
   *
   * @param format_specific Pointer to thrift::metadata::metadata
   * @return Domain metadata with all fields converted
   * @throws std::invalid_argument if format_specific is null
   * @throws std::runtime_error if conversion fails
   */
  domain::metadata to_domain(const void* format_specific) const override;

  /**
   * Convert domain metadata to Thrift metadata.
   *
   * @param domain_meta Domain metadata object
   * @return Unique pointer to thrift::metadata::metadata
   * @throws std::runtime_error if conversion fails
   */
  std::unique_ptr<void, void(*)(void*)> from_domain(
      const domain::metadata& domain_meta) const override;

  /**
   * Get converter name for debugging.
   *
   * @return "Thrift Metadata Converter"
   */
  std::string_view get_name() const noexcept override {
    return "Thrift Metadata Converter";
  }

private:
  // ===================================================================
  // Helper methods for Thrift → Domain conversion
  // ===================================================================

  /**
   * Convert Thrift chunk to domain chunk.
   * @param t_chunk Thrift chunk
   * @return Domain chunk
   */
  domain::chunk convert_chunk_to_domain(
      const ::dwarfs::thrift::metadata::chunk& t_chunk) const;

  /**
   * Convert Thrift directory to domain directory.
   * @param t_dir Thrift directory
   * @return Domain directory
   */
  domain::directory convert_directory_to_domain(
      const ::dwarfs::thrift::metadata::directory& t_dir) const;

  /**
   * Convert Thrift inode_data to domain inode_data.
   * @param t_inode Thrift inode data
   * @return Domain inode data
   */
  domain::inode_data convert_inode_to_domain(
      const ::dwarfs::thrift::metadata::inode_data& t_inode) const;

  /**
   * Convert Thrift dir_entry to domain dir_entry.
   * @param t_entry Thrift directory entry
   * @return Domain directory entry
   */
  domain::dir_entry convert_dir_entry_to_domain(
      const ::dwarfs::thrift::metadata::dir_entry& t_entry) const;

  /**
   * Convert Thrift fs_options to domain fs_options.
   * @param t_opts Thrift filesystem options
   * @return Domain filesystem options
   */
  domain::fs_options convert_fs_options_to_domain(
      const ::dwarfs::thrift::metadata::fs_options& t_opts) const;

  /**
   * Convert Thrift string_table to domain string_table.
   * @param t_table Thrift string table
   * @return Domain string table
   */
  domain::string_table convert_string_table_to_domain(
      const ::dwarfs::thrift::metadata::string_table& t_table) const;

  /**
   * Convert Thrift inode_size_cache to domain inode_size_cache.
   * @param t_cache Thrift inode size cache
   * @return Domain inode size cache
   */
  domain::inode_size_cache convert_inode_size_cache_to_domain(
      const ::dwarfs::thrift::metadata::inode_size_cache& t_cache) const;

  /**
   * Convert Thrift history_entry to domain history_entry.
   * @param t_entry Thrift history entry
   * @return Domain history entry
   */
  domain::history_entry convert_history_entry_to_domain(
      const ::dwarfs::thrift::metadata::history_entry& t_entry) const;

  // ===================================================================
  // Helper methods for Domain → Thrift conversion
  // ===================================================================

  /**
   * Convert domain chunk to Thrift chunk.
   * @param d_chunk Domain chunk
   * @return Thrift chunk
   */
  ::dwarfs::thrift::metadata::chunk convert_chunk_from_domain(
      const domain::chunk& d_chunk) const;

  /**
   * Convert domain directory to Thrift directory.
   * @param d_dir Domain directory
   * @return Thrift directory
   */
  ::dwarfs::thrift::metadata::directory convert_directory_from_domain(
      const domain::directory& d_dir) const;

  /**
   * Convert domain inode_data to Thrift inode_data.
   * @param d_inode Domain inode data
   * @return Thrift inode data
   */
  ::dwarfs::thrift::metadata::inode_data convert_inode_from_domain(
      const domain::inode_data& d_inode) const;

  /**
   * Convert domain dir_entry to Thrift dir_entry.
   * @param d_entry Domain directory entry
   * @return Thrift directory entry
   */
  ::dwarfs::thrift::metadata::dir_entry convert_dir_entry_from_domain(
      const domain::dir_entry& d_entry) const;

  /**
   * Convert domain fs_options to Thrift fs_options.
   * @param d_opts Domain filesystem options
   * @return Thrift filesystem options
   */
  ::dwarfs::thrift::metadata::fs_options convert_fs_options_from_domain(
      const domain::fs_options& d_opts) const;

  /**
   * Convert domain string_table to Thrift string_table.
   * @param d_table Domain string table
   * @return Thrift string table
   */
  ::dwarfs::thrift::metadata::string_table convert_string_table_from_domain(
      const domain::string_table& d_table) const;

  /**
   * Convert domain inode_size_cache to Thrift inode_size_cache.
   * @param d_cache Domain inode size cache
   * @return Thrift inode size cache
   */
  ::dwarfs::thrift::metadata::inode_size_cache convert_inode_size_cache_from_domain(
      const domain::inode_size_cache& d_cache) const;

  /**
   * Convert domain history_entry to Thrift history_entry.
   * @param d_entry Domain history entry
   * @return Thrift history entry
   */
  ::dwarfs::thrift::metadata::history_entry convert_history_entry_from_domain(
      const domain::history_entry& d_entry) const;
};

} // namespace dwarfs::metadata::converters