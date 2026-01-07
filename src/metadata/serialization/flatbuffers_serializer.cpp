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

#include <flatbuffers/flatbuffers.h>
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
  auto buffer_offset = builder.CreateString(st.buffer);
  auto symtab_offset = st.symtab ? builder.CreateString(*st.symtab) : 0;
  auto index_offset = builder.CreateVector(st.index);

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
    for (const auto& e : *domain_meta->dir_entries) {
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
    domain_meta->chunk_table.assign(
      fb_meta->chunk_table()->begin(),
      fb_meta->chunk_table()->end());
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
    for (const auto* entry : *fb_meta->dir_entries()) {
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
      st.buffer = fb_st->buffer()->c_str();
    }
    if (fb_st->index()) {
      st.index.assign(fb_st->index()->begin(), fb_st->index()->end());
    }
    st.packed_index = fb_st->packed_index();
    if (fb_st->symtab() && fb_st->symtab()->size() > 0) {
      // CRITICAL: symtab is binary FSST data, not null-terminated string!
      // Use data() + size() to preserve all bytes including nulls
      st.symtab = std::string(fb_st->symtab()->data(), fb_st->symtab()->size());
    }
    domain_meta->compact_names = st;
  }

  if (fb_meta->compact_symlinks()) {
    domain::string_table st;
    auto* fb_st = fb_meta->compact_symlinks();

    if (fb_st->buffer()) {
      st.buffer = fb_st->buffer()->c_str();
    }
    if (fb_st->index()) {
      st.index.assign(fb_st->index()->begin(), fb_st->index()->end());
    }
    st.packed_index = fb_st->packed_index();
    if (fb_st->symtab() && fb_st->symtab()->size() > 0) {
      // CRITICAL: symtab is binary FSST data, not null-terminated string!
      st.symtab = std::string(fb_st->symtab()->data(), fb_st->symtab()->size());
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