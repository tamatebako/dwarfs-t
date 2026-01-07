/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file thrift_metadata_reader.cpp
 *
 * Thrift Metadata Reader Implementation
 *
 * NO PREPROCESSOR GUARDS: This file is ONLY compiled when
 * DWARFS_WITH_THRIFT=ON via CMake configuration.
 *
 * \author Ribose Inc.
 * \date 2025-12-22
 * \copyright See LICENSE file
 */

#include "dwarfs/reader/metadata_reader_interface.h"
#include "dwarfs/metadata/converters/domain_thrift_converter.h"
#include <dwarfs/gen-cpp2/metadata_types.h>
#include <stdexcept>

namespace dwarfs::reader {

class thrift_metadata_reader : public metadata_reader_interface {
public:
  explicit thrift_metadata_reader(const dwarfs::thrift::metadata::metadata& thrift_meta)
    : thrift_metadata_(thrift_meta) {
  }

  metadata::domain::metadata read() override {
    return metadata::converters::from_thrift(thrift_metadata_);
  }

  metadata::domain::chunk get_chunk(size_t index) override {
    if (!thrift_metadata_.chunks().has_value()) {
      throw std::out_of_range("No chunks in metadata");
    }
    const auto& chunks = thrift_metadata_.chunks().value();
    if (index >= chunks.size()) {
      throw std::out_of_range("Chunk index out of range");
    }
    return metadata::converters::from_thrift(chunks[index]);
  }

  metadata::domain::directory get_directory(size_t index) override {
    if (!thrift_metadata_.directories().has_value()) {
      throw std::out_of_range("No directories in metadata");
    }
    const auto& dirs = thrift_metadata_.directories().value();
    if (index >= dirs.size()) {
      throw std::out_of_range("Directory index out of range");
    }
    return metadata::converters::from_thrift(dirs[index]);
  }

  metadata::domain::inode_data get_inode(size_t index) override {
    if (!thrift_metadata_.inodes().has_value()) {
      throw std::out_of_range("No inodes in metadata");
    }
    const auto& inodes = thrift_metadata_.inodes().value();
    if (index >= inodes.size()) {
      throw std::out_of_range("Inode index out of range");
    }
    return metadata::converters::from_thrift(inodes[index]);
  }

  metadata::domain::dir_entry get_dir_entry(size_t index) override {
    if (!thrift_metadata_.dir_entries().has_value()) {
      throw std::out_of_range("No directory entries in metadata");
    }
    const auto& entries = thrift_metadata_.dir_entries().value();
    if (index >= entries.size()) {
      throw std::out_of_range("Directory entry index out of range");
    }
    return metadata::converters::from_thrift(entries[index]);
  }

  std::string_view get_format_name() const override {
    return "Thrift";
  }

private:
  const dwarfs::thrift::metadata::metadata& thrift_metadata_;
};

// Factory function (will be called by factory)
std::unique_ptr<metadata_reader_interface>
create_thrift_metadata_reader(const dwarfs::thrift::metadata::metadata& thrift_meta) {
  return std::make_unique<thrift_metadata_reader>(thrift_meta);
}

} // namespace dwarfs::reader