/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file
 * \brief Metadata domain model - root structure for all file system metadata
 * \author Marcus Holland-Moritz (github@mhxnet.de)
 * \copyright Copyright (c) Marcus Holland-Moritz
 *
 * This file is part of dwarfs.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include <cereal/cereal.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/optional.hpp>
#include <cereal/types/set.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

#include "chunk.h"
#include "directory.h"
#include "dir_entry.h"
#include "fs_options.h"
#include "history_entry.h"
#include "inode_data.h"
#include "inode_size_cache.h"
#include "string_table.h"

namespace dwarfs::metadata::domain {

/**
 * File System Metadata
 *
 * This is the root structure for all file system metadata.
 */
struct metadata {
  /**
   * Ranges of chunks that make up regular files. Identical
   * files share the same chunk range. The range of chunks
   * for a regular file are:
   *
   *   chunks[chunk_table[index]] .. chunks[chunk_table[index + 1] - 1]
   *
   * Here, `index` is either `inode - file_inode_offset` for
   * unique file inodes, or for shared file inodes:
   *
   *   shared_files[inode - file_inode_offset - unique_files] + unique_files
   *
   * Note that here `shared_files` is the unpacked version of
   * `shared_files_table`.
   */
  std::vector<chunk> chunks;

  /**
   * All directories, indexed by inode number. There's one extra
   * sentinel directory at the end that has `first_entry` point to
   * the end of `dir_entries`, so directory entry lookup work the
   * same for all directories.
   *
   * Note that this list is stored in a packed format as of v2.3
   * if `options.packed_directories` is `true` and must be unpacked
   * before use. See the documentation for the `directory` struct.
   */
  std::vector<directory> directories;

  /**
   * Inode metadata, indexed by inode number.
   *
   * Inodes are assigned strictly in the following order:
   *
   *   - directories, starting with the root dir at inode 0
   *   - symbolic links
   *   - unique regular files
   *   - shared regular files
   *   - character and block devices
   *   - named pipes and sockets
   *
   * The inode type can be determined from its mode, which makes
   * it possible to find the inode offsets for each distinct type
   * by a simple binary search. These inode offsets are required
   * to perform lookups into lists indexed by non-directory inode
   * numbers.
   *
   * The number of shared regular files can be determined from
   * `shared_files_table`.
   */
  std::vector<inode_data> inodes;

  /**
   * Chunk lookup table, indexed by `inode - file_inode_offset`.
   * There's one extra sentinel item at the end that points to the
   * end of `chunks`, so chunk lookups work the same for all inodes.
   *
   * Note that this list is stored delta-compressed as of v2.3
   * if `options.packed_chunk_table` is `true` and must be unpacked
   * before use.
   */
  std::vector<uint32_t> chunk_table;

  /**
   * =========================================================================
   * NOTE: This has been deprecated with filesystem version 2.3
   *       It is still being used to read older filesystem versions.
   */
  std::vector<uint32_t> entry_table_v2_2;
  /* =========================================================================
   */

  /// Symlink lookup table, indexed by `inode - symlink_inode_offset`
  std::vector<uint32_t> symlink_table;

  /// User ids, for lookup by `inode.owner_index`
  std::vector<uint32_t> uids;

  /// Group ids, for lookup by `inode.group_index`
  std::vector<uint32_t> gids;

  /// Inode modes, for lookup by `inode.mode_index`
  std::vector<uint32_t> modes;

  /// Directory entry names, for lookup by `dir_entry.name_index`
  std::vector<std::string> names;

  /// Symlink targets, for lookup by index from `symlink_table`
  std::vector<std::string> symlinks;

  /// Timestamp base for all inode timestamps
  uint64_t timestamp_base{0};

  /************************ DEPRECATED **********************
   *
   * These are redundant and can be determined at run-time
   * with a simple binary search. Compatibility is not
   * affected.
   *
   *   chunk_inode_offset
   *   link_inode_offset
   *
   *********************************************************/

  /// File system block size in bytes
  uint32_t block_size{0};

  /// Total file system size in bytes
  uint64_t total_fs_size{0};

  //=========================================================//
  // fields added with dwarfs-0.3.0, file system version 2.1 //
  //=========================================================//

  /// Device ids, for lookup by `inode - device_inode_offset`
  std::optional<std::vector<uint64_t>> devices;

  /// File system options
  std::optional<fs_options> options;

  //=========================================================//
  // fields added with dwarfs-0.5.0, file system version 2.3 //
  //=========================================================//

  /**
   * All directory entries
   *
   * Starting with the root directory entry at index 0, this
   * list contains ranges all directory entries of the file
   * system. Along with `directories`, this allows traversal
   * of the full file system structure.
   *
   * The ranges of entries that belong to a single directory
   * are determined by `directory.first_entry`. Within a single
   * directory, entries are ordered asciibetically by name,
   * which makes it possible to efficiently find entries using
   * binary search.
   */
  std::optional<std::vector<dir_entry>> dir_entries;

  /**
   * Shared files mapping
   *
   * Note that this list is stored in a packed format if
   * `options.packed_shared_files_table` is `true` and must be
   * unpacked before use.
   *
   * In packed format, it is stored as number of repetitions
   * per index, offset by 2 (the minimum number of repetitions),
   * so e.g. a packed list
   *
   *   [0, 3, 1, 0, 1]
   *
   * would unpack to:
   *
   *   [0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 3, 3, 4, 4, 4]
   *
   * So the packed 5-element array provides mappings for 15 shared
   * file inodes. Assuming 10 unique files and a file inode offset
   * of 10, a regular file inode 25 would be a shared file inode,
   * and the index for lookup in `chunk_table` would be `10 + 1`.
   */
  std::optional<std::vector<uint32_t>> shared_files_table;

  /// Total size of hardlinked files beyond the first link, in bytes
  /// NOTE: This is only kept for backwards compatibility, it is no
  ///       longer used in dwarfs-0.14.0 and later.
  std::optional<uint64_t> total_hardlink_size;

  /// Version string
  std::optional<std::string> dwarfs_version;

  /// Unix timestamp of metadata creation time
  std::optional<uint64_t> create_timestamp;

  std::optional<string_table> compact_names;

  std::optional<string_table> compact_symlinks;

  //=========================================================//
  // fields added with dwarfs-0.7.0, file system version 2.5 //
  //=========================================================//

  /// Preferred path separator of original file system
  std::optional<uint32_t> preferred_path_separator;

  //=========================================================//
  // fields added with dwarfs-0.7.3, file system version 2.5 //
  //=========================================================//

  /// The set of features used in this file system image. As long
  /// as an older binary supports all features, it will be able
  /// to use images created with newer versions. We use strings
  /// here instead of an enum so older versions can still output
  /// names of features used by a newer version.
  std::optional<std::set<std::string>> features;

  //=========================================================//
  // fields added with dwarfs-0.8.0, file system version 2.5 //
  //=========================================================//

  /// The set of categories used in this file system image. Used
  /// for displaying and to select compression algorithms when
  /// recompressing the image.
  std::optional<std::vector<std::string>> category_names;

  /// The category of each block in the file system image. The
  /// index into this vector is the block number and the value
  /// is an index into `category_names`.
  std::optional<std::vector<uint32_t>> block_categories;

  //==========================================================//
  // fields added with dwarfs-0.11.0, file system version 2.5 //
  //==========================================================//

  /// Size cache for highly fragmented file inodes
  std::optional<inode_size_cache> reg_file_size_cache;

  //==========================================================//
  // fields added with dwarfs-0.13.0, file system version 2.5 //
  //==========================================================//

  /// Unique block categorization metadata JSON strings. These
  /// can be used to compress a block with a metadata-dependent
  /// algorithm after having been compressed with a general
  /// purpose algorithm.
  std::optional<std::vector<std::string>> category_metadata_json;

  /// The metadata associated with each block. Maps from block
  /// number to index into `categorization_metadata_json`.
  std::optional<std::map<uint32_t, uint32_t>> block_category_metadata;

  /// Version strings for all metadata versions
  std::optional<std::vector<history_entry>> metadata_version_history;

  //==========================================================//
  // fields added with dwarfs-0.14.0, file system version 2.5 //
  //==========================================================//

  /// The block index used to encode a hole in a chunk.
  std::optional<uint32_t> hole_block_index;

  /// The size of sparse file holes that are too large to be
  /// stored efficiently in the chunk table, in bytes.
  std::optional<std::vector<uint64_t>> large_hole_size;

  /// Total allocated file system size in bytes.
  std::optional<uint64_t> total_allocated_fs_size;

  //-------------------------------------------------------------
  // This field was never released, but someone may have built
  // an image from a pre-release version, so we keep this here
  // as a reminder not to reuse this field number.
  //
  // total_allocated_hardlink_size
  //-------------------------------------------------------------

  /**
   * Cereal serialization support
   */
  template <class Archive>
  void serialize(Archive& ar, std::uint32_t const version) {
    // Base fields (version 1)
    ar(CEREAL_NVP(chunks), CEREAL_NVP(directories), CEREAL_NVP(inodes),
       CEREAL_NVP(chunk_table), CEREAL_NVP(symlink_table),
       CEREAL_NVP(uids), CEREAL_NVP(gids), CEREAL_NVP(modes),
       CEREAL_NVP(names), CEREAL_NVP(symlinks),
       CEREAL_NVP(timestamp_base), CEREAL_NVP(block_size),
       CEREAL_NVP(total_fs_size));

    // Deprecated v2.2 fields
    if (version >= 1) {
      ar(CEREAL_NVP(entry_table_v2_2));
    }

    // Fields added in dwarfs-0.3.0, file system version 2.1 (version 2)
    if (version >= 2) {
      ar(CEREAL_NVP(devices), CEREAL_NVP(options));
    }

    // Fields added in dwarfs-0.5.0, file system version 2.3 (version 3)
    if (version >= 3) {
      ar(CEREAL_NVP(dir_entries), CEREAL_NVP(shared_files_table),
         CEREAL_NVP(total_hardlink_size), CEREAL_NVP(dwarfs_version),
         CEREAL_NVP(create_timestamp), CEREAL_NVP(compact_names),
         CEREAL_NVP(compact_symlinks));
    }

    // Fields added in dwarfs-0.7.0, file system version 2.5 (version 4)
    if (version >= 4) {
      ar(CEREAL_NVP(preferred_path_separator));
    }

    // Fields added in dwarfs-0.7.3, file system version 2.5 (version 5)
    if (version >= 5) {
      ar(CEREAL_NVP(features));
    }

    // Fields added in dwarfs-0.8.0, file system version 2.5 (version 6)
    if (version >= 6) {
      ar(CEREAL_NVP(category_names), CEREAL_NVP(block_categories));
    }

    // Fields added in dwarfs-0.11.0, file system version 2.5 (version 7)
    if (version >= 7) {
      ar(CEREAL_NVP(reg_file_size_cache));
    }

    // Fields added in dwarfs-0.13.0, file system version 2.5 (version 8)
    if (version >= 8) {
      ar(CEREAL_NVP(category_metadata_json),
         CEREAL_NVP(block_category_metadata),
         CEREAL_NVP(metadata_version_history));
    }

    // Fields added in dwarfs-0.14.0, file system version 2.5 (version 9)
    if (version >= 9) {
      ar(CEREAL_NVP(hole_block_index), CEREAL_NVP(large_hole_size),
         CEREAL_NVP(total_allocated_fs_size));
    }
  }
};

} // namespace dwarfs::metadata::domain

CEREAL_CLASS_VERSION(dwarfs::metadata::domain::metadata, 9)