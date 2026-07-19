/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file flatbuffers_metadata_reader.cpp
 *
 * FlatBuffers Metadata Reader Implementation
 *
 * This file is part of the FlatBuffers metadata support, which is
 * always enabled as the default metadata format.
 *
 * \author Ribose Inc.
 * \date 2025-12-22
 * \copyright See LICENSE file
 */

#include "dwarfs/reader/metadata_reader_interface.h"
#include "dwarfs/metadata/converters/domain_flatbuffers_converter.h"
#include <dwarfs/gen-flatbuffers/metadata.h>
#include <flatbuffers/flatbuffers.h>
#include <stdexcept>

namespace dwarfs::reader {

class flatbuffers_metadata_reader : public metadata_reader_interface {
public:
  explicit flatbuffers_metadata_reader(const uint8_t* data, size_t size)
    : fb_metadata_(nullptr) {
    // Verify FlatBuffers data (use :: prefix to avoid namespace collision)
    ::flatbuffers::Verifier verifier(data, size);
    if (!verifier.VerifySizePrefixedBuffer<dwarfs::flatbuffers::Metadata>(nullptr)) {
      throw std::runtime_error("Invalid FlatBuffers metadata");
    }

    // Get root (size-prefixed, use :: prefix)
    fb_metadata_ = ::flatbuffers::GetSizePrefixedRoot<dwarfs::flatbuffers::Metadata>(data);
    if (!fb_metadata_) {
      throw std::runtime_error("Failed to parse FlatBuffers metadata");
    }
  }

  metadata::domain::metadata read() override {
    if (!fb_metadata_) {
      throw std::runtime_error("FlatBuffers metadata not initialized");
    }
    return metadata::converters::from_flatbuffers(*fb_metadata_);
  }

  metadata::domain::chunk get_chunk(size_t index) override {
    if (!fb_metadata_ || !fb_metadata_->chunks()) {
      throw std::out_of_range("Invalid chunk index");
    }
    if (index >= fb_metadata_->chunks()->size()) {
      throw std::out_of_range("Chunk index out of range");
    }
    return metadata::converters::from_flatbuffers(*fb_metadata_->chunks()->Get(index));
  }

  metadata::domain::directory get_directory(size_t index) override {
    if (!fb_metadata_ || !fb_metadata_->directories()) {
      throw std::out_of_range("Invalid directory index");
    }
    if (index >= fb_metadata_->directories()->size()) {
      throw std::out_of_range("Directory index out of range");
    }
    return metadata::converters::from_flatbuffers(*fb_metadata_->directories()->Get(index));
  }

  metadata::domain::inode_data get_inode(size_t index) override {
    if (!fb_metadata_ || !fb_metadata_->inodes()) {
      throw std::out_of_range("Invalid inode index");
    }
    if (index >= fb_metadata_->inodes()->size()) {
      throw std::out_of_range("Inode index out of range");
    }
    return metadata::converters::from_flatbuffers(*fb_metadata_->inodes()->Get(index));
  }

  metadata::domain::dir_entry get_dir_entry(size_t index) override {
    if (!fb_metadata_ || !fb_metadata_->dir_entries()) {
      throw std::out_of_range("Invalid dir_entry index");
    }
    if (index >= fb_metadata_->dir_entries()->size()) {
      throw std::out_of_range("Directory entry index out of range");
    }
    return metadata::converters::from_flatbuffers(*fb_metadata_->dir_entries()->Get(index));
  }

  std::string_view get_format_name() const override {
    return "FlatBuffers";
  }

private:
  const dwarfs::flatbuffers::Metadata* fb_metadata_;
};

// Factory function (will be called by factory)
std::unique_ptr<metadata_reader_interface>
create_flatbuffers_metadata_reader(const uint8_t* data, size_t size) {
  return std::make_unique<flatbuffers_metadata_reader>(data, size);
}

} // namespace dwarfs::reader