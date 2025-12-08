/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file metadata_accessor.h
 *
 * DwarFS Metadata Accessor Interface
 *
 * This file provides a unified interface for accessing DwarFS metadata
 * regardless of the underlying representation (Thrift frozen layout or
 * plain C++ structures).
 *
 * Design Philosophy:
 * - Concept-based interface for compile-time polymorphism
 * - Zero-overhead abstraction (no virtual functions in hot path)
 * - Uniform API for frozen and structured metadata
 * - Conditional compilation for Thrift dependency
 * - Type-safe optional field access
 *
 * Architecture:
 * ┌─────────────────────────────────────┐
 * │   Application Code (metadata_v2)   │
 * └─────────────────┬───────────────────┘
 *                   │
 *                   ▼
 *         ┌─────────────────────┐
 *         │ MetadataAccessor    │  ◄── Concept
 *         │    (interface)      │
 *         └──────────┬──────────┘
 *                    │
 *         ┌──────────┴──────────┐
 *         │                     │
 *         ▼                     ▼
 * ┌────────────────┐    ┌────────────────────┐
 * │frozen_accessor │    │structured_accessor │
 * │(MappedFrozen)  │    │ (plain structs)    │
 * └────────────────┘    └────────────────────┘
 *
 * Usage:
 * ```cpp
 * // Generic code using concept
 * template <MetadataAccessor Accessor>
 * void process(Accessor const& acc) {
 *   auto size = acc.block_size();
 *   for (auto const& chunk : acc.chunks()) {
 *     // process chunk
 *   }
 * }
 *
 * // Specific accessor usage
 * #ifdef DWARFS_HAVE_THRIFT
 *   auto frozen = map_frozen<thrift::metadata::metadata>(schema, data);
 *   frozen_metadata_accessor accessor(std::move(frozen));
 * #else
 *   metadata meta = deserialize(data);
 *   structured_metadata_accessor accessor(meta);
 * #endif
 * ```
 *
 * \author Ribose
 * \date 2025-11-12
 * \copyright See LICENSE file
 */

#pragma once

#include <dwarfs/internal/metadata_structures.h>

#include <concepts>
#include <ranges>
#include <span>
#include <stdexcept>
#include <variant>

#ifdef DWARFS_HAVE_THRIFT
#include <thrift/lib/cpp2/frozen/FrozenUtil.h>
#include <dwarfs/gen-cpp2/metadata_types.h>
#endif

namespace dwarfs::internal {

// ============================================================================
// Metadata Accessor Concept
// ============================================================================

/**
 * MetadataAccessor concept - Defines the interface for metadata access
 *
 * This concept specifies what operations a metadata accessor must provide.
 * Both frozen_metadata_accessor and structured_metadata_accessor satisfy
 * this concept, enabling generic programming without runtime overhead.
 *
 * Requirements:
 * - All scalar fields must be accessible via getter methods
 * - All collection fields must return range-like objects
 * - Optional fields must return optional-like objects (has_value() check)
 */
template <typename T>
concept MetadataAccessor = requires(T const& t) {
  // Core scalar accessors
  { t.block_size() } -> std::convertible_to<uint32_t>;
  { t.timestamp_base() } -> std::convertible_to<uint64_t>;
  { t.total_fs_size() } -> std::convertible_to<uint64_t>;

  // Collection accessors - return range-like views
  { t.chunks() } -> std::ranges::input_range;
  { t.directories() } -> std::ranges::input_range;
  { t.inodes() } -> std::ranges::input_range;
  { t.chunk_table() } -> std::ranges::input_range;
  { t.symlink_table() } -> std::ranges::input_range;
  { t.uids() } -> std::ranges::input_range;
  { t.gids() } -> std::ranges::input_range;
  { t.modes() } -> std::ranges::input_range;
  { t.names() } -> std::ranges::input_range;
  { t.symlinks() } -> std::ranges::input_range;
};

// ============================================================================
// Structured Metadata Accessor (Always Available)
// ============================================================================

/**
 * structured_metadata_accessor - Accesses plain C++ metadata structures
 *
 * This accessor wraps a `metadata` structure and provides a uniform
 * interface for accessing its fields. It always compiles with or without
 * Thrift, making it the default choice for Thrift-optional builds.
 *
 * Performance Characteristics:
 * - Direct member access (zero overhead)
 * - Returns std::span for collections (no allocation)
 * - Returns std::optional references for optional fields
 *
 * Thread Safety:
 * - Safe for concurrent reads if metadata is immutable
 * - Not safe for concurrent read/write
 */
class structured_metadata_accessor {
public:
  /**
   * Constructor - Wraps a metadata structure
   *
   * \param meta Reference to metadata structure (must outlive this accessor)
   */
  explicit structured_metadata_accessor(metadata const& meta)
    : meta_(meta) {}

  // ── Core Scalar Accessors ────────────────────────────────────────────────

  uint32_t block_size() const { return meta_.block_size; }
  uint64_t timestamp_base() const { return meta_.timestamp_base; }
  uint64_t total_fs_size() const { return meta_.total_fs_size; }

  // ── Core Collection Accessors ────────────────────────────────────────────

  auto chunks() const { return std::span{meta_.chunks}; }
  auto directories() const { return std::span{meta_.directories}; }
  auto inodes() const { return std::span{meta_.inodes}; }
  auto chunk_table() const { return std::span{meta_.chunk_table}; }
  auto entry_table_v2_2() const { return std::span{meta_.entry_table_v2_2}; }
  auto symlink_table() const { return std::span{meta_.symlink_table}; }
  auto uids() const { return std::span{meta_.uids}; }
  auto gids() const { return std::span{meta_.gids}; }
  auto modes() const { return std::span{meta_.modes}; }
  auto names() const { return std::span{meta_.names}; }
  auto symlinks() const { return std::span{meta_.symlinks}; }

  // ── Optional Field Accessors (v2.1+) ─────────────────────────────────────

  auto devices() const { return meta_.devices; }
  auto options() const { return meta_.options; }

  // ── Optional Field Accessors (v2.3+) ─────────────────────────────────────

  auto dir_entries() const { return meta_.dir_entries; }
  auto shared_files_table() const { return meta_.shared_files_table; }
  auto total_hardlink_size() const { return meta_.total_hardlink_size; }
  auto dwarfs_version() const { return meta_.dwarfs_version; }
  auto create_timestamp() const { return meta_.create_timestamp; }
  auto compact_names() const { return meta_.compact_names; }
  auto compact_symlinks() const { return meta_.compact_symlinks; }

  // ── Optional Field Accessors (v2.5+) ─────────────────────────────────────

  auto preferred_path_separator() const { return meta_.preferred_path_separator; }
  auto features() const { return meta_.features; }
  auto category_names() const { return meta_.category_names; }
  auto block_categories() const { return meta_.block_categories; }
  auto reg_file_size_cache() const { return meta_.reg_file_size_cache; }
  auto category_metadata_json() const { return meta_.category_metadata_json; }
  auto block_category_metadata() const { return meta_.block_category_metadata; }
  auto metadata_version_history() const { return meta_.metadata_version_history; }
  auto hole_block_index() const { return meta_.hole_block_index; }
  auto large_hole_size() const { return meta_.large_hole_size; }
  auto total_allocated_fs_size() const { return meta_.total_allocated_fs_size; }

  // ── Convenience Methods ──────────────────────────────────────────────────

  /**
   * Check if symlinks are present in the filesystem
   *
   * \return true if symlink_table is not empty
   */
  bool has_symlinks() const {
    return !meta_.symlink_table.empty();
  }

  /**
   * Check if a specific optional field is present
   *
   * \return true if the field has a value
   */
  bool has_devices() const { return meta_.devices.has_value(); }
  bool has_options() const { return meta_.options.has_value(); }
  bool has_dir_entries() const { return meta_.dir_entries.has_value(); }
  bool has_shared_files_table() const { return meta_.shared_files_table.has_value(); }
  bool has_features() const { return meta_.features.has_value(); }
  bool has_compact_names() const { return meta_.compact_names.has_value(); }
  bool has_compact_symlinks() const { return meta_.compact_symlinks.has_value(); }
  bool has_reg_file_size_cache() const { return meta_.reg_file_size_cache.has_value(); }
  bool has_hole_block_index() const { return meta_.hole_block_index.has_value(); }

private:
  metadata const& meta_; ///< Reference to underlying metadata structure
};

// ============================================================================
// Frozen Metadata Accessor (Conditional on Thrift)
// ============================================================================

#ifdef DWARFS_HAVE_THRIFT

/**
 * frozen_metadata_accessor - Accesses Thrift frozen metadata layout
 *
 * This accessor wraps a `MappedFrozen<thrift::metadata::metadata>` and
 * provides the same interface as structured_metadata_accessor. It enables
 * zero-copy access to memory-mapped frozen layouts.
 *
 * Performance Characteristics:
 * - Zero-copy memory access (optimal for read-only)
 * - Returns frozen views (no allocation)
 * - Memory-mapped (no deserialization overhead)
 *
 * Thread Safety:
 * - Safe for concurrent reads (frozen layout is immutable)
 *
 * Note: Only available when DWARFS_HAVE_THRIFT is defined at compile time.
 */
class frozen_metadata_accessor {
public:
  /**
   * Constructor - Wraps a frozen metadata layout
   *
   * \param meta Frozen metadata layout (moved into accessor)
   */
  explicit frozen_metadata_accessor(
    apache::thrift::frozen::MappedFrozen<thrift::metadata::metadata> meta)
    : meta_(std::move(meta)) {}

  // ── Core Scalar Accessors ────────────────────────────────────────────────

  uint32_t block_size() const { return meta_.block_size(); }
  uint64_t timestamp_base() const { return meta_.timestamp_base(); }
  uint64_t total_fs_size() const { return meta_.total_fs_size(); }

  // ── Core Collection Accessors ────────────────────────────────────────────

  auto chunks() const { return meta_.chunks(); }
  auto directories() const { return meta_.directories(); }
  auto inodes() const { return meta_.inodes(); }
  auto chunk_table() const { return meta_.chunk_table(); }
  auto entry_table_v2_2() const { return meta_.entry_table_v2_2(); }
  auto symlink_table() const { return meta_.symlink_table(); }
  auto uids() const { return meta_.uids(); }
  auto gids() const { return meta_.gids(); }
  auto modes() const { return meta_.modes(); }
  auto names() const { return meta_.names(); }
  auto symlinks() const { return meta_.symlinks(); }

  // ── Optional Field Accessors (v2.1+) ─────────────────────────────────────

  auto devices() const { return meta_.devices(); }
  auto options() const { return meta_.options(); }

  // ── Optional Field Accessors (v2.3+) ─────────────────────────────────────

  auto dir_entries() const { return meta_.dir_entries(); }
  auto shared_files_table() const { return meta_.shared_files_table(); }
  auto total_hardlink_size() const { return meta_.total_hardlink_size(); }
  auto dwarfs_version() const { return meta_.dwarfs_version(); }
  auto create_timestamp() const { return meta_.create_timestamp(); }
  auto compact_names() const { return meta_.compact_names(); }
  auto compact_symlinks() const { return meta_.compact_symlinks(); }

  // ── Optional Field Accessors (v2.5+) ─────────────────────────────────────

  auto preferred_path_separator() const { return meta_.preferred_path_separator(); }
  auto features() const { return meta_.features(); }
  auto category_names() const { return meta_.category_names(); }
  auto block_categories() const { return meta_.block_categories(); }
  auto reg_file_size_cache() const { return meta_.reg_file_size_cache(); }
  auto category_metadata_json() const { return meta_.category_metadata_json(); }
  auto block_category_metadata() const { return meta_.block_category_metadata(); }
  auto metadata_version_history() const { return meta_.metadata_version_history(); }
  auto hole_block_index() const { return meta_.hole_block_index(); }
  auto large_hole_size() const { return meta_.large_hole_size(); }
  auto total_allocated_fs_size() const { return meta_.total_allocated_fs_size(); }

  // ── Convenience Methods ──────────────────────────────────────────────────

  /**
   * Check if symlinks are present in the filesystem
   *
   * \return true if symlink_table is not empty
   */
  bool has_symlinks() const {
    return !meta_.symlink_table().empty();
  }

  /**
   * Check if a specific optional field is present
   *
   * \return true if the field has a value
   */
  bool has_devices() const { return meta_.devices().has_value(); }
  bool has_options() const { return meta_.options().has_value(); }
  bool has_dir_entries() const { return meta_.dir_entries().has_value(); }
  bool has_shared_files_table() const { return meta_.shared_files_table().has_value(); }
  bool has_features() const { return meta_.features().has_value(); }
  bool has_compact_names() const { return meta_.compact_names().has_value(); }
  bool has_compact_symlinks() const { return meta_.compact_symlinks().has_value(); }
  bool has_reg_file_size_cache() const { return meta_.reg_file_size_cache().has_value(); }
  bool has_hole_block_index() const { return meta_.hole_block_index().has_value(); }

private:
  apache::thrift::frozen::MappedFrozen<thrift::metadata::metadata> meta_;
};

#endif // DWARFS_HAVE_THRIFT

// ============================================================================
// Type-Erased Variant (Runtime Polymorphism)
// ============================================================================

/**
 * metadata_accessor_variant - Type-erased accessor for runtime dispatch
 *
 * Provides runtime polymorphism when the accessor type is not known at
 * compile time. Use std::visit to access the underlying accessor.
 *
 * Example:
 * \code{.cpp}
 * metadata_accessor_variant accessor = ...;
 * auto size = std::visit([](auto const& acc) {
 *   return acc.block_size();
 * }, accessor);
 * \endcode
 */
#ifdef DWARFS_HAVE_THRIFT
using metadata_accessor_variant = std::variant<
  frozen_metadata_accessor,
  structured_metadata_accessor
>;
#else
using metadata_accessor_variant = std::variant<
  structured_metadata_accessor
>;
#endif

/**
 * Helper function for visiting metadata accessor variants
 *
 * \param accessor The variant to visit
 * \param visitor Callable invoked with the concrete accessor type
 * \return Result of visitor invocation
 */
template <typename Visitor>
auto visit_metadata(metadata_accessor_variant const& accessor, Visitor&& visitor) {
  return std::visit(std::forward<Visitor>(visitor), accessor);
}

// ============================================================================
// Static Assertions (Verify Concept Satisfaction)
// ============================================================================

// Verify that our accessor implementations satisfy the MetadataAccessor concept
static_assert(MetadataAccessor<structured_metadata_accessor>,
              "structured_metadata_accessor must satisfy MetadataAccessor concept");

#ifdef DWARFS_HAVE_THRIFT
static_assert(MetadataAccessor<frozen_metadata_accessor>,
              "frozen_metadata_accessor must satisfy MetadataAccessor concept");
#endif

} // namespace dwarfs::internal