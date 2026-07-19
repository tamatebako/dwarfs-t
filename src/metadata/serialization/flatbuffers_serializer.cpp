//
// Copyright (c) Marcus Holland-Moritz
//
// This file is part of dwarfs.
//
// dwarfs is free software: you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later
// version.
//
// dwarfs is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
// A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with
// dwarfs.  If not, see <https://www.gnu.org/licenses/>.
//

#include "dwarfs/config.h"

#ifdef DWARFS_HAVE_FLATBUFFERS

// Include generated FlatBuffers header to verify build system integration
#include <dwarfs/gen-flatbuffers/metadata.h>

#include "dwarfs/metadata/serialization/flatbuffers_serializer.h"
#include "dwarfs/metadata/serialization/serializer_registry.h"
#include "dwarfs/metadata/domain/metadata.h"
#include "dwarfs/metadata/domain/inode_size_cache.h"
#include "dwarfs/internal/fsst.h"

#include <flatbuffers/flatbuffers.h>
#include <iostream>
#include <iomanip>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

namespace dwarfs::metadata::serialization {

// Helper: Convert domain::chunk to FlatBuffers
static ::flatbuffers::Offset<dwarfs::flatbuffers::Chunk>
create_chunk(::flatbuffers::FlatBufferBuilder& builder, const domain::chunk& c) {
  return dwarfs::flatbuffers::CreateChunk(builder, c.block(), c.offset(), c.size());
}

// Helper: Convert domain::directory to FlatBuffers
static ::flatbuffers::Offset<dwarfs::flatbuffers::Directory>
create_directory(::flatbuffers::FlatBufferBuilder& builder, const domain::directory& d) {
  return dwarfs::flatbuffers::CreateDirectory(builder, d.parent_entry(), d.first_entry(), d.self_entry());
}

// Helper: Convert domain::inode_data to FlatBuffers
static ::flatbuffers::Offset<dwarfs::flatbuffers::InodeData>
create_inode_data(::flatbuffers::FlatBufferBuilder& builder, const domain::inode_data& inode) {
  return dwarfs::flatbuffers::CreateInodeData(
    builder,
    inode.mode_index,
    inode.owner_index,
    inode.group_index,
    inode.atime_offset,
    inode.mtime_offset,
    inode.ctime_offset,
    inode.btime_offset,
    inode.atime_subsec,
    inode.mtime_subsec,
    inode.ctime_subsec,
    inode.btime_subsec,
    inode.nlink_minus_one,
    inode.name_index_v2_2.value_or(0),
    inode.inode_v2_2.value_or(0)
  );
}

// Helper: Convert domain::dir_entry to FlatBuffers
static ::flatbuffers::Offset<dwarfs::flatbuffers::DirEntry>
create_dir_entry(::flatbuffers::FlatBufferBuilder& builder, const domain::dir_entry& entry) {
  return dwarfs::flatbuffers::CreateDirEntry(builder, entry.name_index(), entry.inode_num());
}

// Helper: Convert domain::fs_options to FlatBuffers
static ::flatbuffers::Offset<dwarfs::flatbuffers::FsOptions>
create_fs_options(::flatbuffers::FlatBufferBuilder& builder, const domain::fs_options& opts) {
  return dwarfs::flatbuffers::CreateFsOptions(
    builder,
    opts.mtime_only,
    opts.time_resolution_sec.value_or(1),
    opts.subsecond_resolution_nsec_multiplier.value_or(1),
    opts.packed_chunk_table,
    opts.packed_directories,
    opts.packed_shared_files_table,
    opts.has_btime,
    opts.inodes_have_nlink
  );
}

// Helper: Convert domain::string_table to FlatBuffers
static ::flatbuffers::Offset<dwarfs::flatbuffers::StringTable>
create_string_table(::flatbuffers::FlatBufferBuilder& builder, const domain::string_table& st) {
  // Use CreateVector for binary data (not CreateString)
  auto buffer_offset = builder.CreateVector(
      reinterpret_cast<const uint8_t*>(st.buffer.data()),
      st.buffer.size());

  // The domain model's compact_names contains the original data from the file.
  // We preserve the index as-is without unpacking.
  auto index_offset = builder.CreateVector(st.index);

  // Create symtab offset if compression is enabled (also binary data)
  ::flatbuffers::Offset<::flatbuffers::Vector<uint8_t>> symtab_offset = 0;
  if (st.symtab) {
    symtab_offset = builder.CreateVector(
        reinterpret_cast<const uint8_t*>(st.symtab->data()),
        st.symtab->size());
  }

  return dwarfs::flatbuffers::CreateStringTable(
    builder,
    buffer_offset,
    symtab_offset,
    index_offset,
    st.packed_index
  );
}

std::vector<uint8_t> FlatBuffersSerializer::serialize(const void* metadata) const {
  if (metadata == nullptr) {
    throw std::invalid_argument("Cannot serialize null metadata");
  }

  auto* domain_meta = static_cast<const domain::metadata*>(metadata);
  ::flatbuffers::FlatBufferBuilder builder(1024 * 1024); // 1MB initial

  // Build all vectors
  std::vector<::flatbuffers::Offset<dwarfs::flatbuffers::Chunk>> chunks;
  for (const auto& c : domain_meta->chunks) {
    chunks.push_back(create_chunk(builder, c));
  }
  auto chunks_vector = builder.CreateVector(chunks);

  std::vector<::flatbuffers::Offset<dwarfs::flatbuffers::Directory>> directories;
  for (const auto& d : domain_meta->directories) {
    directories.push_back(create_directory(builder, d));
  }
  auto directories_vector = builder.CreateVector(directories);

  std::vector<::flatbuffers::Offset<dwarfs::flatbuffers::InodeData>> inodes;
  for (const auto& i : domain_meta->inodes) {
    inodes.push_back(create_inode_data(builder, i));
  }
  auto inodes_vector = builder.CreateVector(inodes);

  // Simple vectors
  auto chunk_table_vector = builder.CreateVector(domain_meta->chunk_table);
  auto entry_table_v2_2_vector = builder.CreateVector(domain_meta->entry_table_v2_2);
  auto symlink_table_vector = builder.CreateVector(domain_meta->symlink_table);
  auto uids_vector = builder.CreateVector(domain_meta->uids);
  auto gids_vector = builder.CreateVector(domain_meta->gids);
  auto modes_vector = builder.CreateVector(domain_meta->modes);

  // String vectors
  std::vector<::flatbuffers::Offset<::flatbuffers::String>> names;
  for (const auto& name : domain_meta->names) {
    names.push_back(builder.CreateString(name));
  }
  auto names_vector = builder.CreateVector(names);

  std::vector<::flatbuffers::Offset<::flatbuffers::String>> symlinks;
  for (const auto& symlink : domain_meta->symlinks) {
    symlinks.push_back(builder.CreateString(symlink));
  }
  auto symlinks_vector = builder.CreateVector(symlinks);

  // Optional fields
  ::flatbuffers::Offset<::flatbuffers::Vector<uint64_t>> devices_vector = 0;
  if (domain_meta->devices) {
    devices_vector = builder.CreateVector(*domain_meta->devices);
  }

  ::flatbuffers::Offset<dwarfs::flatbuffers::FsOptions> options_offset = 0;
  if (domain_meta->options) {
    options_offset = create_fs_options(builder, *domain_meta->options);
  }

  ::flatbuffers::Offset<::flatbuffers::Vector<::flatbuffers::Offset<dwarfs::flatbuffers::DirEntry>>> dir_entries_vector = 0;
  if (domain_meta->dir_entries) {
    std::vector<::flatbuffers::Offset<dwarfs::flatbuffers::DirEntry>> entries;
    for (auto const& e : *domain_meta->dir_entries) {
      entries.push_back(create_dir_entry(builder, e));
    }
    dir_entries_vector = builder.CreateVector(entries);
  }

  ::flatbuffers::Offset<::flatbuffers::Vector<uint32_t>> shared_files_vector = 0;
  if (domain_meta->shared_files_table) {
    shared_files_vector = builder.CreateVector(*domain_meta->shared_files_table);
  }

  ::flatbuffers::Offset<::flatbuffers::String> dwarfs_version_offset = 0;
  if (domain_meta->dwarfs_version) {
    dwarfs_version_offset = builder.CreateString(*domain_meta->dwarfs_version);
  }

  ::flatbuffers::Offset<dwarfs::flatbuffers::StringTable> compact_names_offset = 0;
  if (domain_meta->compact_names) {
    compact_names_offset = create_string_table(builder, *domain_meta->compact_names);
  }

  ::flatbuffers::Offset<dwarfs::flatbuffers::StringTable> compact_symlinks_offset = 0;
  if (domain_meta->compact_symlinks) {
    compact_symlinks_offset = create_string_table(builder, *domain_meta->compact_symlinks);
  }

  ::flatbuffers::Offset<::flatbuffers::Vector<::flatbuffers::Offset<::flatbuffers::String>>> features_vector = 0;
  if (domain_meta->features) {
    std::vector<::flatbuffers::Offset<::flatbuffers::String>> feat;
    for (const auto& f : *domain_meta->features) {
      feat.push_back(builder.CreateString(f));
    }
    features_vector = builder.CreateVector(feat);
  }

  ::flatbuffers::Offset<::flatbuffers::Vector<::flatbuffers::Offset<::flatbuffers::String>>> category_names_vector = 0;
  if (domain_meta->category_names) {
    std::vector<::flatbuffers::Offset<::flatbuffers::String>> cats;
    for (const auto& c : *domain_meta->category_names) {
      cats.push_back(builder.CreateString(c));
    }
    category_names_vector = builder.CreateVector(cats);
  }

  ::flatbuffers::Offset<::flatbuffers::Vector<uint32_t>> block_categories_vector = 0;
  if (domain_meta->block_categories) {
    block_categories_vector = builder.CreateVector(*domain_meta->block_categories);
  }

  ::flatbuffers::Offset<::flatbuffers::Vector<uint64_t>> large_hole_size_vector = 0;
  if (domain_meta->large_hole_size) {
    large_hole_size_vector = builder.CreateVector(*domain_meta->large_hole_size);
  }

  // NEW: reg_file_size_cache
  ::flatbuffers::Offset<dwarfs::flatbuffers::InodeSizeCache> reg_file_size_cache_offset = 0;
  if (domain_meta->reg_file_size_cache) {
    auto& cache = *domain_meta->reg_file_size_cache;

    // Convert maps to parallel arrays
    std::vector<uint32_t> size_keys, alloc_keys;
    std::vector<uint64_t> size_vals, alloc_vals;

    for (auto const& [k, v] : cache.size_lookup) {
      size_keys.push_back(k);
      size_vals.push_back(v);
    }

    for (auto const& [k, v] : cache.allocated_size_lookup) {
      alloc_keys.push_back(k);
      alloc_vals.push_back(v);
    }

    auto size_keys_vec = size_keys.empty() ? 0 : builder.CreateVector(size_keys);
    auto size_vals_vec = size_vals.empty() ? 0 : builder.CreateVector(size_vals);
    auto alloc_keys_vec = alloc_keys.empty() ? 0 : builder.CreateVector(alloc_keys);
    auto alloc_vals_vec = alloc_vals.empty() ? 0 : builder.CreateVector(alloc_vals);

    dwarfs::flatbuffers::InodeSizeCacheBuilder cache_builder(builder);
    cache_builder.add_size_lookup_keys(size_keys_vec);
    cache_builder.add_size_lookup_values(size_vals_vec);
    cache_builder.add_allocated_size_lookup_keys(alloc_keys_vec);
    cache_builder.add_allocated_size_lookup_values(alloc_vals_vec);
    cache_builder.add_min_chunk_count(cache.min_chunk_count);

    reg_file_size_cache_offset = cache_builder.Finish();
  }

  // NEW: category_metadata_json
  ::flatbuffers::Offset<::flatbuffers::Vector<::flatbuffers::Offset<::flatbuffers::String>>> category_metadata_json_vector = 0;
  if (domain_meta->category_metadata_json) {
    std::vector<::flatbuffers::Offset<::flatbuffers::String>> json_strings;
    for (const auto& j : *domain_meta->category_metadata_json) {
      json_strings.push_back(builder.CreateString(j));
    }
    category_metadata_json_vector = builder.CreateVector(json_strings);
  }

  // NEW: block_category_metadata (as parallel arrays)
  ::flatbuffers::Offset<::flatbuffers::Vector<uint32_t>> block_category_metadata_keys_vector = 0;
  ::flatbuffers::Offset<::flatbuffers::Vector<uint32_t>> block_category_metadata_values_vector = 0;
  if (domain_meta->block_category_metadata) {
    std::vector<uint32_t> keys, values;
    for (auto const& [k, v] : *domain_meta->block_category_metadata) {
      keys.push_back(k);
      values.push_back(v);
    }
    block_category_metadata_keys_vector = builder.CreateVector(keys);
    block_category_metadata_values_vector = builder.CreateVector(values);
  }

  // NEW: metadata_version_history
  ::flatbuffers::Offset<::flatbuffers::Vector<::flatbuffers::Offset<dwarfs::flatbuffers::HistoryEntry>>> metadata_version_history_vector = 0;
  if (domain_meta->metadata_version_history) {
    std::vector<::flatbuffers::Offset<dwarfs::flatbuffers::HistoryEntry>> history;
    for (const auto& h : *domain_meta->metadata_version_history) {
      auto dv_offset = h.dwarfs_version ? builder.CreateString(*h.dwarfs_version) : 0;
      auto opts_offset = h.options ? create_fs_options(builder, *h.options) : 0;

      history.push_back(dwarfs::flatbuffers::CreateHistoryEntry(
        builder,
        h.major,
        h.minor,
        dv_offset,
        h.block_size,
        opts_offset
      ));
    }
    metadata_version_history_vector = builder.CreateVector(history);
  }

  // Build the root Metadata table
  auto metadata_offset = dwarfs::flatbuffers::CreateMetadata(
    builder,
    chunks_vector,
    directories_vector,
    inodes_vector,
    chunk_table_vector,
    entry_table_v2_2_vector,
    symlink_table_vector,
    uids_vector,
    gids_vector,
    modes_vector,
    names_vector,
    symlinks_vector,
    domain_meta->timestamp_base,
    domain_meta->block_size,
    domain_meta->total_fs_size,
    devices_vector,
    options_offset,
    dir_entries_vector,
    shared_files_vector,
    domain_meta->total_hardlink_size.value_or(0),
    dwarfs_version_offset,
    domain_meta->create_timestamp.value_or(0),
    compact_names_offset,
    compact_symlinks_offset,
    domain_meta->preferred_path_separator.value_or(0),
    features_vector,
    category_names_vector,
    block_categories_vector,
    reg_file_size_cache_offset,
    category_metadata_json_vector,
    block_category_metadata_keys_vector,
    block_category_metadata_values_vector,
    metadata_version_history_vector,
    domain_meta->hole_block_index.value_or(0),
    large_hole_size_vector,
    domain_meta->total_allocated_fs_size.value_or(0)
  );

  // Finish with file identifier "DFBF"
  builder.FinishSizePrefixed(metadata_offset, "DFBF");

  // Return the buffer as vector
  uint8_t* buf = builder.GetBufferPointer();
  size_t size = builder.GetSize();
  return std::vector<uint8_t>(buf, buf + size);
}

std::unique_ptr<void, void(*)(void*)> FlatBuffersSerializer::deserialize(
    const std::vector<uint8_t>& data) const {

  // Verify size-prefixed buffer
  if (data.size() < 8) { // Need at least size prefix + file identifier
    throw std::invalid_argument("Data too short for FlatBuffers format");
  }

  // Verify the FULL buffer (includes size prefix)
  ::flatbuffers::Verifier verifier(data.data(), data.size());
  if (!verifier.VerifySizePrefixedBuffer<dwarfs::flatbuffers::Metadata>("DFBF")) {
    throw std::runtime_error("Invalid FlatBuffers metadata buffer");
  }

  // Get the metadata root (from size-prefixed buffer)
  auto* fb_meta = ::flatbuffers::GetSizePrefixedRoot<dwarfs::flatbuffers::Metadata>(data.data());

  // Convert to domain model
  auto domain_meta = std::make_unique<domain::metadata>();

  // Core structures
  if (fb_meta->chunks()) {
    for (const auto* chunk : *fb_meta->chunks()) {
      domain::chunk c(chunk->block(), chunk->offset(), chunk->size());
      domain_meta->chunks.push_back(c);
    }
  }

  if (fb_meta->directories()) {
    for (const auto* dir : *fb_meta->directories()) {
      domain::directory d(dir->parent_entry(), dir->first_entry(), dir->self_entry());
      domain_meta->directories.push_back(d);
    }
  }

  if (fb_meta->inodes()) {
    for (const auto* inode : *fb_meta->inodes()) {
      domain::inode_data i;
      i.mode_index = inode->mode_index();
      i.owner_index = inode->owner_index();
      i.group_index = inode->group_index();
      i.atime_offset = inode->atime_offset();
      i.mtime_offset = inode->mtime_offset();
      i.ctime_offset = inode->ctime_offset();
      if (inode->btime_offset() > 0) i.btime_offset = inode->btime_offset();
      if (inode->atime_subsec() > 0) i.atime_subsec = inode->atime_subsec();
      if (inode->mtime_subsec() > 0) i.mtime_subsec = inode->mtime_subsec();
      if (inode->ctime_subsec() > 0) i.ctime_subsec = inode->ctime_subsec();
      if (inode->btime_subsec() > 0) i.btime_subsec = inode->btime_subsec();
      if (inode->nlink_minus_one() > 0) i.nlink_minus_one = inode->nlink_minus_one();
      if (inode->name_index_v2_2() > 0) i.name_index_v2_2 = inode->name_index_v2_2();
      if (inode->inode_v2_2() > 0) i.inode_v2_2 = inode->inode_v2_2();
      domain_meta->inodes.push_back(i);
    }
  }

  // Simple vectors
  if (fb_meta->chunk_table()) {
    bool is_packed = fb_meta->options() && fb_meta->options()->packed_chunk_table();

    if (is_packed) {
      // Unpack delta-encoded chunk_table using prefix sum
      std::vector<uint32_t> packed;
      packed.reserve(fb_meta->chunk_table()->size());
      for (auto val : *fb_meta->chunk_table()) {
        packed.push_back(val);
      }

      domain_meta->chunk_table.reserve(packed.size());
      std::partial_sum(packed.begin(), packed.end(),
                       std::back_inserter(domain_meta->chunk_table));
    } else {
      domain_meta->chunk_table.assign(
        fb_meta->chunk_table()->begin(),
        fb_meta->chunk_table()->end());
    }
  }

  if (fb_meta->entry_table_v2_2()) {
    domain_meta->entry_table_v2_2.assign(
      fb_meta->entry_table_v2_2()->begin(),
      fb_meta->entry_table_v2_2()->end());
  }

  if (fb_meta->symlink_table()) {
    domain_meta->symlink_table.assign(
      fb_meta->symlink_table()->begin(),
      fb_meta->symlink_table()->end());
  }

  if (fb_meta->uids()) {
    domain_meta->uids.assign(fb_meta->uids()->begin(), fb_meta->uids()->end());
  }

  if (fb_meta->gids()) {
    domain_meta->gids.assign(fb_meta->gids()->begin(), fb_meta->gids()->end());
  }

  if (fb_meta->modes()) {
    domain_meta->modes.assign(fb_meta->modes()->begin(), fb_meta->modes()->end());
  }

  // String vectors
  if (fb_meta->names()) {
    for (const auto* name : *fb_meta->names()) {
      domain_meta->names.emplace_back(name->c_str());
    }
  } else {
  }

  if (fb_meta->symlinks()) {
    for (const auto* symlink : *fb_meta->symlinks()) {
      domain_meta->symlinks.emplace_back(symlink->c_str());
    }
  }

  // Scalars
  domain_meta->timestamp_base = fb_meta->timestamp_base();
  domain_meta->block_size = fb_meta->block_size();
  domain_meta->total_fs_size = fb_meta->total_fs_size();

  // Optional fields
  if (fb_meta->devices() && fb_meta->devices()->size() > 0) {
    domain_meta->devices = std::vector<uint64_t>(
      fb_meta->devices()->begin(),
      fb_meta->devices()->end());
  }

  if (fb_meta->options()) {
    domain::fs_options opts;
    opts.mtime_only = fb_meta->options()->mtime_only();
    opts.time_resolution_sec = fb_meta->options()->time_resolution_sec();
    opts.subsecond_resolution_nsec_multiplier = fb_meta->options()->subsecond_resolution_nsec_multiplier();
    opts.packed_chunk_table = fb_meta->options()->packed_chunk_table();
    opts.packed_directories = fb_meta->options()->packed_directories();
    opts.packed_shared_files_table = fb_meta->options()->packed_shared_files_table();
    opts.has_btime = fb_meta->options()->has_btime();
    opts.inodes_have_nlink = fb_meta->options()->inodes_have_nlink();
    domain_meta->options = opts;
  }

  if (fb_meta->dir_entries()) {
    std::vector<domain::dir_entry> entries;
    for (size_t i = 0; i < fb_meta->dir_entries()->size(); ++i) {
      auto const* entry = fb_meta->dir_entries()->Get(i);
      domain::dir_entry e(entry->name_index(), entry->inode_num());
      entries.push_back(e);
    }
    domain_meta->dir_entries = entries;
  }

  if (fb_meta->shared_files_table() && fb_meta->shared_files_table()->size() > 0) {
    domain_meta->shared_files_table = std::vector<uint32_t>(
      fb_meta->shared_files_table()->begin(),
      fb_meta->shared_files_table()->end());
  }

  if (fb_meta->total_hardlink_size() > 0) {
    domain_meta->total_hardlink_size = fb_meta->total_hardlink_size();
  }

  if (fb_meta->dwarfs_version()) {
    domain_meta->dwarfs_version = fb_meta->dwarfs_version()->c_str();
  }

  if (fb_meta->create_timestamp() > 0) {
    domain_meta->create_timestamp = fb_meta->create_timestamp();
  }

  // compact_names and compact_symlinks deserialization was missing!
  if (fb_meta->compact_names()) {
    domain::string_table st;
    auto* fb_st = fb_meta->compact_names();

    if (fb_st->buffer()) {
      // Convert Vector<uint8_t> to std::string
      std::string compressed_buffer(
          reinterpret_cast<const char*>(fb_st->buffer()->data()),
          fb_st->buffer()->size());

      // First, unpack the index if packed_index is true
      if (fb_st->index()) {
        st.packed_index = fb_st->packed_index();

        // CRITICAL FIX: The index format depends on whether compression is enabled.
        // When packed_index=true and we have symtab (compression), the index contains
        // SIZES of compressed chunks. Otherwise, it contains delta-encoded offsets.
        if (fb_st->packed_index() && fb_st->index() && fb_st->index()->size() > 0) {
          // Store the index values as-is (could be sizes or deltas)
          st.index.assign(fb_st->index()->begin(), fb_st->index()->end());
          st.packed_index = fb_st->packed_index();
        } else if (fb_st->index() && fb_st->index()->size() > 0) {
          // Index is already absolute offsets (not packed)
          st.index.assign(fb_st->index()->begin(), fb_st->index()->end());
        }
      }

      // If we have symtab, decompress the buffer using dwarfs-rs algorithm
      if (fb_st->symtab() && fb_st->symtab()->size() > 0) {
        // Convert Vector<uint8_t> to std::string
        std::string symtab_data(
            reinterpret_cast<const char*>(fb_st->symtab()->data()),
            fb_st->symtab()->size());

        internal::fsst_decoder decoder(symtab_data);

        // With FSST compression, st.index contains SIZES of each compressed chunk.
        // We need to calculate offsets into the compressed buffer from these sizes.
        std::vector<uint32_t> compressed_offsets;

        // When packed_index=true and we have symtab, the index contains SIZES of
        // compressed chunks. Otherwise, it contains OFFSETS into the compressed buffer.
        bool needs_conversion = st.packed_index;

        if (needs_conversion) {
          // Index contains sizes followed by buffer_size marker
          // Format: [size0, size1, ..., sizeN-1, buffer_size]
          // For 14 strings: [s0, s1, ..., s13, buffer_size]
          // After partial_sum, we want offsets for chunk boundaries:
          // [0, s0, s0+s1, ..., sum(s0..s12), buffer_size]
          // Where each entry i is the START offset of chunk i
          // (Entry N (buffer_size) is the end offset of the last chunk)

          size_t num_entries = st.index.size();


          compressed_offsets.assign(st.index.begin(), st.index.end());

          if (num_entries > 2) {
            uint32_t sum = 0;
            for (size_t i = 0; i < num_entries - 1; ++i) {
              uint32_t size = st.index[i];
              compressed_offsets[i] = sum;
              sum += size;
            }
            // Last entry is the buffer size (end offset of last chunk)
            compressed_offsets[num_entries - 1] = st.index[num_entries - 1];
          }
        } else {
          // Index already contains offsets (packed_index was false)
          compressed_offsets = st.index;
        }

        // dwarfs-rs algorithm:
        // - Calculate offsets into COMPRESSED buffer (from sizes or use existing offsets)
        // - Each consecutive pair [offsets[i], offsets[i+1]] defines one COMPRESSED chunk
        // - Decompress each chunk separately into a SINGLE output buffer
        // - Build a NEW index with cumulative offsets into the DECOMPRESSED buffer

        std::string decompressed_buffer;
        decompressed_buffer.reserve(compressed_buffer.size() * 2);

        std::vector<uint32_t> decompressed_index;
        decompressed_index.reserve(st.index.size());
        decompressed_index.push_back(0);  // First offset is always 0

        size_t out_len = 0;
        size_t num_chunks = compressed_offsets.size() > 0 ? compressed_offsets.size() - 1 : 0;

        for (size_t i = 0; i < num_chunks; ++i) {
          uint32_t chunk_start = compressed_offsets[i];
          uint32_t chunk_end = compressed_offsets[i + 1];

          // Clamp chunk_end to buffer size
          if (chunk_end > compressed_buffer.size()) {
            chunk_end = compressed_buffer.size();
          }

          // Skip chunks that start past the buffer (after clamping)
          if (chunk_start > compressed_buffer.size()) {
            break;
          }

          // Skip empty chunks
          if (chunk_start == chunk_end) {
            decompressed_index.push_back(out_len);
            continue;
          }

          std::string_view chunk(compressed_buffer.data() + chunk_start, chunk_end - chunk_start);
          std::string decompressed = decoder.decompress(chunk);

          decompressed_buffer.append(decompressed);
          out_len += decompressed.size();
          decompressed_index.push_back(out_len);
        }

        // Add the final buffer size marker if not already present
        // For N strings, we need N+1 index entries (N offsets + 1 buffer size marker)
        if (decompressed_index.empty() || decompressed_index.back() != decompressed_buffer.size()) {
          decompressed_index.push_back(decompressed_buffer.size());
        }

        st.buffer = decompressed_buffer;
        st.index = decompressed_index;
        st.packed_index = false;  // Index now contains OFFSETS, not SIZES
      } else {
        // Buffer is not compressed
        st.buffer = compressed_buffer;
      }
    }

    domain_meta->compact_names = st;
  } else {
  }

  if (fb_meta->compact_symlinks()) {
    domain::string_table st;
    auto* fb_st = fb_meta->compact_symlinks();

    if (fb_st->buffer()) {
      // Convert Vector<uint8_t> to std::string
      std::string compressed_buffer(
          reinterpret_cast<const char*>(fb_st->buffer()->data()),
          fb_st->buffer()->size());

      // First, unpack the index if packed_index is true
      if (fb_st->index()) {
        st.packed_index = fb_st->packed_index();

        if (st.packed_index) {
          // CRITICAL FIX: Index contains SIZES, not delta-encoded offsets!
          // We need to convert sizes to offsets using partial_sum
          std::vector<uint32_t> packed;
          packed.reserve(fb_st->index()->size());
          for (auto val : *fb_st->index()) {
            packed.push_back(val);
          }

          // Convert sizes to offsets for chunk boundaries
          std::vector<uint32_t> offset_index;
          offset_index.reserve(packed.size());
          offset_index.push_back(0);  // First offset is always 0
          for (size_t i = 0; i < packed.size() - 1; ++i) {
            offset_index.push_back(offset_index.back() + packed[i]);
          }
          st.index = offset_index;
        } else {
          st.index.assign(fb_st->index()->begin(), fb_st->index()->end());
        }
      }

      // If we have symtab, decompress the buffer using dwarfs-rs algorithm
      if (fb_st->symtab() && fb_st->symtab()->size() > 0) {
        // Convert Vector<uint8_t> to std::string
        std::string symtab_data(
            reinterpret_cast<const char*>(fb_st->symtab()->data()),
            fb_st->symtab()->size());
        internal::fsst_decoder decoder(symtab_data);

        std::string decompressed_buffer;
        decompressed_buffer.reserve(compressed_buffer.size() * 2);

        std::vector<uint32_t> decompressed_index;
        decompressed_index.reserve(st.index.size());
        decompressed_index.push_back(0);  // First offset is always 0

        size_t out_len = 0;
        size_t num_chunks = st.index.size() > 0 ? st.index.size() - 1 : 0;


        for (size_t i = 0; i < num_chunks; ++i) {
          uint32_t chunk_start = st.index[i];
          uint32_t chunk_end = st.index[i + 1];


          if (chunk_start > chunk_end || chunk_end > compressed_buffer.size()) {
            break;
          }

          std::string_view chunk(compressed_buffer.data() + chunk_start, chunk_end - chunk_start);
          std::string decompressed = decoder.decompress(chunk);


          decompressed_buffer.append(decompressed);
          out_len += decompressed.size();
          decompressed_index.push_back(out_len);
        }

        // Add the final buffer size marker if not already present
        // For N strings, we need N+1 index entries (N offsets + 1 buffer size marker)
        if (decompressed_index.empty() || decompressed_index.back() != decompressed_buffer.size()) {
          decompressed_index.push_back(decompressed_buffer.size());
        }

        st.buffer = decompressed_buffer;
        st.index = decompressed_index;
        st.packed_index = false;  // Index now contains OFFSETS, not SIZES
      } else {
        // Buffer is not compressed
        st.buffer = compressed_buffer;
      }
    }

    domain_meta->compact_symlinks = st;
  }

  if (fb_meta->preferred_path_separator() > 0) {
    domain_meta->preferred_path_separator = fb_meta->preferred_path_separator();
  }

  if (fb_meta->features()) {
    std::set<std::string> features;
    for (const auto* f : *fb_meta->features()) {
      features.insert(f->c_str());
    }
    domain_meta->features = features;
  }

  if (fb_meta->category_names()) {
    std::vector<std::string> cats;
    for (const auto* c : *fb_meta->category_names()) {
      cats.emplace_back(c->c_str());
    }
    domain_meta->category_names = cats;
  }

  if (fb_meta->block_categories() && fb_meta->block_categories()->size() > 0) {
    domain_meta->block_categories = std::vector<uint32_t>(
      fb_meta->block_categories()->begin(),
      fb_meta->block_categories()->end());
  }

  if (fb_meta->hole_block_index() > 0) {
    domain_meta->hole_block_index = fb_meta->hole_block_index();
  }

  if (fb_meta->large_hole_size() && fb_meta->large_hole_size()->size() > 0) {
    domain_meta->large_hole_size = std::vector<uint64_t>(
      fb_meta->large_hole_size()->begin(),
      fb_meta->large_hole_size()->end());
  }

  if (fb_meta->total_allocated_fs_size() > 0) {
    domain_meta->total_allocated_fs_size = fb_meta->total_allocated_fs_size();
  }

  // Deserialize InodeSizeCache (reg_file_size_cache)
  if (fb_meta->reg_file_size_cache()) {
    auto* fb_cache = fb_meta->reg_file_size_cache();
    domain::inode_size_cache cache;

    cache.min_chunk_count = fb_cache->min_chunk_count();

    // Deserialize size_lookup: parallel arrays -> map
    if (fb_cache->size_lookup_keys() && fb_cache->size_lookup_values()) {
      auto* keys = fb_cache->size_lookup_keys();
      auto* values = fb_cache->size_lookup_values();
      size_t count = std::min(keys->size(), values->size());
      for (size_t i = 0; i < count; ++i) {
        cache.size_lookup[(*keys)[i]] = (*values)[i];
      }
    }

    // Deserialize allocated_size_lookup: parallel arrays -> map
    if (fb_cache->allocated_size_lookup_keys() && fb_cache->allocated_size_lookup_values()) {
      auto* keys = fb_cache->allocated_size_lookup_keys();
      auto* values = fb_cache->allocated_size_lookup_values();
      size_t count = std::min(keys->size(), values->size());
      for (size_t i = 0; i < count; ++i) {
        cache.allocated_size_lookup[(*keys)[i]] = (*values)[i];
      }
    }

    domain_meta->reg_file_size_cache = std::move(cache);
  }

  // Return with custom deleter
  return std::unique_ptr<void, void(*)(void*)>(
      domain_meta.release(),
      [](void* ptr) { delete static_cast<domain::metadata*>(ptr); }
  );
}

// Registration function called by init_serializers()
void register_flatbuffers_serializer() {
  static SerializerRegistration<FlatBuffersSerializer> registration{
    "FlatBuffers",
    {'D', 'F', 'B', 'F'},  // Magic bytes: "DFBF" file identifier
    120,  // Highest priority (modern default format)
    SerializationFormat::FLATBUFFERS
  };
}

} // namespace dwarfs::metadata::serialization

#endif // DWARFS_HAVE_FLATBUFFERS