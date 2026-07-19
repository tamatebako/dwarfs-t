/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Marcus Holland-Moritz (github@mhx.io)
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

#include "dwarfs/metadata/legacy/legacy_metadata_serializer.h"

#include <stdexcept>

#include "dwarfs/metadata/domain/metadata.h"
#include "dwarfs/metadata/legacy/thrift_compact_reader.h"
#include "dwarfs/metadata/legacy/thrift_compact_writer.h"

namespace dwarfs::metadata::legacy {

namespace {

// Helper: Write vector of structs (chunk, directory, inode_data)
template <typename T, typename WriteFunc>
void write_struct_list(ThriftCompactWriter& w, uint16_t field_id,
                      std::vector<T> const& vec, WriteFunc write_item) {
  if (vec.empty()) {
    return; // Skip empty lists
  }

  // Field header for list
  w.write_field_header(field_id, Tag::LIST);

  // List header: size + element type
  w.write_varint(static_cast<uint32_t>(vec.size()));
  w.write_byte(static_cast<uint8_t>(Tag::STRUCT));

  // Write each struct
  for (auto const& item : vec) {
    write_item(w, item);
  }
}

// Helper: Write vector of primitives (u32, string)
template <typename T, typename WriteFunc>
void write_primitive_list(ThriftCompactWriter& w, uint16_t field_id,
                         std::vector<T> const& vec, Tag element_tag,
                         WriteFunc write_item) {
  if (vec.empty()) {
    return; // Skip empty lists
  }

  w.write_field_header(field_id, Tag::LIST);
  w.write_varint(static_cast<uint32_t>(vec.size()));
  w.write_byte(static_cast<uint8_t>(element_tag));

  for (auto const& item : vec) {
    write_item(w, item);
  }
}

// Helper: Read list header
struct ListHeader {
  uint32_t size;
  Tag element_type;
};

ListHeader read_list_header(ThriftCompactReader& r) {
  uint32_t size = r.read_varint();
  Tag element_type = Tag::INVALID;

  if (size > 0) {
    uint8_t type_byte = r.read_byte();
    element_type = static_cast<Tag>(type_byte);
  }

  return {size, element_type};
}

} // anonymous namespace

void LegacyMetadataSerializer::serialize(domain::metadata const& meta,
                                        std::vector<uint8_t>& output) {
  output.clear();
  ThriftCompactWriter w(output);

  w.begin_struct();

  // Field 1: chunks (list<Chunk>)
  write_struct_list(w, 1, meta.chunks, [](ThriftCompactWriter& w, auto const& c) {
    w.begin_struct();
    w.write_field_header(1, Tag::I32);
    w.write_i32(static_cast<int32_t>(c.block()));
    w.write_field_header(2, Tag::I32);
    w.write_i32(static_cast<int32_t>(c.offset()));
    w.write_field_header(3, Tag::I32);
    w.write_i32(static_cast<int32_t>(c.size()));
    w.end_struct();
  });

  // Field 2: directories (list<Directory>)
  write_struct_list(w, 2, meta.directories, [](ThriftCompactWriter& w, auto const& d) {
    w.begin_struct();
    w.write_field_header(1, Tag::I32);
    w.write_i32(static_cast<int32_t>(d.parent_entry()));
    w.write_field_header(2, Tag::I32);
    w.write_i32(static_cast<int32_t>(d.first_entry()));
    w.write_field_header(3, Tag::I32);
    w.write_i32(static_cast<int32_t>(d.self_entry()));
    w.end_struct();
  });

  // Field 3: inodes (list<InodeData>)
  write_struct_list(w, 3, meta.inodes, [](ThriftCompactWriter& w, auto const& i) {
    w.begin_struct();
    w.write_field_header(1, Tag::I32);
    w.write_i32(static_cast<int32_t>(i.mode_index));
    w.write_field_header(2, Tag::I32);
    w.write_i32(static_cast<int32_t>(i.owner_index));
    w.write_field_header(3, Tag::I32);
    w.write_i32(static_cast<int32_t>(i.group_index));
    w.write_field_header(4, Tag::I32);
    w.write_i32(static_cast<int32_t>(i.atime_offset));
    w.write_field_header(5, Tag::I32);
    w.write_i32(static_cast<int32_t>(i.mtime_offset));
    w.write_field_header(6, Tag::I32);
    w.write_i32(static_cast<int32_t>(i.ctime_offset));
    w.end_struct();
  });

  // Field 4: chunk_table (list<i32>)
  write_primitive_list(w, 4, meta.chunk_table, Tag::I32,
                      [](ThriftCompactWriter& w, uint32_t val) {
                        w.write_i32(static_cast<int32_t>(val));
                      });

  // Field 5: entry_table_v2_2 (list<i32>) - deprecated but include if present
  write_primitive_list(w, 5, meta.entry_table_v2_2, Tag::I32,
                      [](ThriftCompactWriter& w, uint32_t val) {
                        w.write_i32(static_cast<int32_t>(val));
                      });

  // Field 6: symlink_table (list<i32>)
  write_primitive_list(w, 6, meta.symlink_table, Tag::I32,
                      [](ThriftCompactWriter& w, uint32_t val) {
                        w.write_i32(static_cast<int32_t>(val));
                      });

  // Field 7: uids (list<i32>)
  write_primitive_list(w, 7, meta.uids, Tag::I32,
                      [](ThriftCompactWriter& w, uint32_t val) {
                        w.write_i32(static_cast<int32_t>(val));
                      });

  // Field 8: gids (list<i32>)
  write_primitive_list(w, 8, meta.gids, Tag::I32,
                      [](ThriftCompactWriter& w, uint32_t val) {
                        w.write_i32(static_cast<int32_t>(val));
                      });

  // Field 9: modes (list<i32>)
  write_primitive_list(w, 9, meta.modes, Tag::I32,
                      [](ThriftCompactWriter& w, uint32_t val) {
                        w.write_i32(static_cast<int32_t>(val));
                      });

  // Field 10: names (list<string>)
  write_primitive_list(w, 10, meta.names, Tag::BINARY,
                      [](ThriftCompactWriter& w, std::string const& val) {
                        w.write_string(val);
                      });

  // Field 11: symlinks (list<string>)
  write_primitive_list(w, 11, meta.symlinks, Tag::BINARY,
                      [](ThriftCompactWriter& w, std::string const& val) {
                        w.write_string(val);
                      });

  // Field 12: timestamp_base (i64) - Full 64-bit support
  w.write_field_header(12, Tag::I64);
  w.write_i64(static_cast<int64_t>(meta.timestamp_base));

  // Field 15: block_size (i32)
  w.write_field_header(15, Tag::I32);
  w.write_i32(static_cast<int32_t>(meta.block_size));

  // Field 16: total_fs_size (i64) - Full 64-bit support
  w.write_field_header(16, Tag::I64);
  w.write_i64(static_cast<int64_t>(meta.total_fs_size));

  w.end_struct();
}

void LegacyMetadataSerializer::deserialize(std::span<uint8_t const> data,
                                          domain::metadata& meta) {
  ThriftCompactReader r(data);

  r.begin_struct();

  // Read fields in order
  while (auto hdr = r.read_field_header()) {
    switch (hdr->field_id) {
      case 1: { // chunks
        if (hdr->type != Tag::LIST) {
          throw std::runtime_error("Expected LIST for chunks");
        }
        auto list_hdr = read_list_header(r);
        meta.chunks.reserve(list_hdr.size);
        for (uint32_t i = 0; i < list_hdr.size; ++i) {
          r.begin_struct();
          domain::chunk c;
          while (auto field = r.read_field_header()) {
            switch (field->field_id) {
              case 1: c.set_block(static_cast<uint32_t>(r.read_i32())); break;
              case 2: c.set_offset(static_cast<uint32_t>(r.read_i32())); break;
              case 3: c.set_size(static_cast<uint32_t>(r.read_i32())); break;
              default: throw std::runtime_error("Unknown chunk field");
            }
          }
          r.end_struct();
          meta.chunks.push_back(c);
        }
        break;
      }

      case 2: { // directories
        if (hdr->type != Tag::LIST) {
          throw std::runtime_error("Expected LIST for directories");
        }
        auto list_hdr = read_list_header(r);
        meta.directories.reserve(list_hdr.size);
        for (uint32_t i = 0; i < list_hdr.size; ++i) {
          r.begin_struct();
          domain::directory d;
          while (auto field = r.read_field_header()) {
            switch (field->field_id) {
              case 1: d.set_parent_entry(static_cast<uint32_t>(r.read_i32())); break;
              case 2: d.set_first_entry(static_cast<uint32_t>(r.read_i32())); break;
              case 3: d.set_self_entry(static_cast<uint32_t>(r.read_i32())); break;
              default: throw std::runtime_error("Unknown directory field");
            }
          }
          r.end_struct();
          meta.directories.push_back(d);
        }
        break;
      }

      case 3: { // inodes
        if (hdr->type != Tag::LIST) {
          throw std::runtime_error("Expected LIST for inodes");
        }
        auto list_hdr = read_list_header(r);
        meta.inodes.reserve(list_hdr.size);
        for (uint32_t i = 0; i < list_hdr.size; ++i) {
          r.begin_struct();
          domain::inode_data inode;
          while (auto field = r.read_field_header()) {
            switch (field->field_id) {
              case 1: inode.mode_index = static_cast<uint32_t>(r.read_i32()); break;
              case 2: inode.owner_index = static_cast<uint32_t>(r.read_i32()); break;
              case 3: inode.group_index = static_cast<uint32_t>(r.read_i32()); break;
              case 4: inode.atime_offset = static_cast<uint64_t>(r.read_i32()); break;
              case 5: inode.mtime_offset = static_cast<uint64_t>(r.read_i32()); break;
              case 6: inode.ctime_offset = static_cast<uint64_t>(r.read_i32()); break;
              default:
                // Skip unknown fields for forward compatibility
                break;
            }
          }
          r.end_struct();
          meta.inodes.push_back(inode);
        }
        break;
      }

      case 4: { // chunk_table
        if (hdr->type != Tag::LIST) {
          throw std::runtime_error("Expected LIST for chunk_table");
        }
        auto list_hdr = read_list_header(r);
        meta.chunk_table.reserve(list_hdr.size);
        for (uint32_t i = 0; i < list_hdr.size; ++i) {
          meta.chunk_table.push_back(static_cast<uint32_t>(r.read_i32()));
        }
        break;
      }

      case 5: { // entry_table_v2_2
        if (hdr->type != Tag::LIST) {
          throw std::runtime_error("Expected LIST for entry_table_v2_2");
        }
        auto list_hdr = read_list_header(r);
        meta.entry_table_v2_2.reserve(list_hdr.size);
        for (uint32_t i = 0; i < list_hdr.size; ++i) {
          meta.entry_table_v2_2.push_back(static_cast<uint32_t>(r.read_i32()));
        }
        break;
      }

      case 6: { // symlink_table
        if (hdr->type != Tag::LIST) {
          throw std::runtime_error("Expected LIST for symlink_table");
        }
        auto list_hdr = read_list_header(r);
        meta.symlink_table.reserve(list_hdr.size);
        for (uint32_t i = 0; i < list_hdr.size; ++i) {
          meta.symlink_table.push_back(static_cast<uint32_t>(r.read_i32()));
        }
        break;
      }

      case 7: { // uids
        if (hdr->type != Tag::LIST) {
          throw std::runtime_error("Expected LIST for uids");
        }
        auto list_hdr = read_list_header(r);
        meta.uids.reserve(list_hdr.size);
        for (uint32_t i = 0; i < list_hdr.size; ++i) {
          meta.uids.push_back(static_cast<uint32_t>(r.read_i32()));
        }
        break;
      }

      case 8: { // gids
        if (hdr->type != Tag::LIST) {
          throw std::runtime_error("Expected LIST for gids");
        }
        auto list_hdr = read_list_header(r);
        meta.gids.reserve(list_hdr.size);
        for (uint32_t i = 0; i < list_hdr.size; ++i) {
          meta.gids.push_back(static_cast<uint32_t>(r.read_i32()));
        }
        break;
      }

      case 9: { // modes
        if (hdr->type != Tag::LIST) {
          throw std::runtime_error("Expected LIST for modes");
        }
        auto list_hdr = read_list_header(r);
        meta.modes.reserve(list_hdr.size);
        for (uint32_t i = 0; i < list_hdr.size; ++i) {
          meta.modes.push_back(static_cast<uint32_t>(r.read_i32()));
        }
        break;
      }

      case 10: { // names
        if (hdr->type != Tag::LIST) {
          throw std::runtime_error("Expected LIST for names");
        }
        auto list_hdr = read_list_header(r);
        meta.names.reserve(list_hdr.size);
        for (uint32_t i = 0; i < list_hdr.size; ++i) {
          auto sv = r.read_string();
          meta.names.emplace_back(sv.begin(), sv.end());
        }
        break;
      }

      case 11: { // symlinks
        if (hdr->type != Tag::LIST) {
          throw std::runtime_error("Expected LIST for symlinks");
        }
        auto list_hdr = read_list_header(r);
        meta.symlinks.reserve(list_hdr.size);
        for (uint32_t i = 0; i < list_hdr.size; ++i) {
          auto sv = r.read_string();
          meta.symlinks.emplace_back(sv.begin(), sv.end());
        }
        break;
      }

      case 12: // timestamp_base (i64)
        if (hdr->type != Tag::I64) {
          throw std::runtime_error("Expected I64 for timestamp_base");
        }
        meta.timestamp_base = static_cast<uint64_t>(r.read_i64());
        break;

      case 15: // block_size
        if (hdr->type != Tag::I32) {
          throw std::runtime_error("Expected I32 for block_size");
        }
        meta.block_size = static_cast<uint32_t>(r.read_i32());
        break;

      case 16: // total_fs_size (i64)
        if (hdr->type != Tag::I64) {
          throw std::runtime_error("Expected I64 for total_fs_size");
        }
        meta.total_fs_size = static_cast<uint64_t>(r.read_i64());
        break;

      default:
        // Skip unknown fields for forward compatibility
        break;
    }
  }

  r.end_struct();
}

} // namespace dwarfs::metadata::legacy