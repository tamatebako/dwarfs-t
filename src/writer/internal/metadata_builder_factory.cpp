// Create factory implementation
/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Ribose Inc.
 * \copyright  Copyright (c) Ribose Inc.
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

// FACTORY PATTERN: Creates appropriate metadata builder strategy

#include <dwarfs/logger.h>
#include <dwarfs/writer/internal/metadata_builder.h>
#include <dwarfs/metadata/serialization/serialization_format.h>
#include <dwarfs/error.h>

#ifdef DWARFS_HAVE_THRIFT
#include <dwarfs/metadata/converters/domain_thrift_converter.h>
#include <dwarfs/gen-cpp2/metadata_types.h>
#endif

#include <fmt/format.h>

// Include builder implementations (Strategy pattern headers)
#ifdef DWARFS_HAVE_FLATBUFFERS
#include <dwarfs/writer/internal/flatbuffers_metadata_builder_impl.h>
#endif

#ifdef DWARFS_HAVE_THRIFT
#include <dwarfs/writer/internal/thrift_metadata_builder_impl.h>
#endif

namespace dwarfs::writer::internal {

// Factory method: create new builder with specific format
std::unique_ptr<metadata_builder::impl> metadata_builder::impl::create(
    logger& lgr,
    metadata_options const& options,
    metadata::serialization::SerializationFormat format) {

  using namespace metadata::serialization;

  switch (format) {
#ifdef DWARFS_HAVE_THRIFT
    case SerializationFormat::THRIFT_COMPACT:
      return make_unique_logging_object<
          impl,
          thrift_metadata_builder,
          logger_policies>(lgr, options);
#endif

#ifdef DWARFS_HAVE_FLATBUFFERS
    case SerializationFormat::FLATBUFFERS:
      return make_unique_logging_object<
          impl,
          flatbuffers_metadata_builder,
          logger_policies>(lgr, options);
#endif

    default:
      DWARFS_THROW(runtime_error,
          fmt::format("Unsupported metadata format: {}",
                      static_cast<int>(format)));
  }
}

// Factory method: create builder from existing metadata
std::unique_ptr<metadata_builder::impl> metadata_builder::impl::create_from_existing(
    logger& lgr,
    metadata::domain::metadata const& existing_md,
    metadata::domain::fs_options const* orig_fs_options,
    filesystem_version const& orig_fs_version,
    metadata_options const& options,
    metadata::serialization::SerializationFormat format) {

  using namespace metadata::serialization;

  switch (format) {
#ifdef DWARFS_HAVE_THRIFT
    case SerializationFormat::THRIFT_COMPACT: {
      // Convert domain → Thrift for Thrift strategy
      auto thrift_md = metadata::converters::to_thrift(existing_md);

      // Convert fs_options if present
      thrift::metadata::fs_options const* thrift_opts = nullptr;
      std::unique_ptr<thrift::metadata::fs_options> thrift_opts_storage;
      if (orig_fs_options) {
        thrift_opts_storage = std::make_unique<thrift::metadata::fs_options>(
            metadata::converters::to_thrift(*orig_fs_options));
        thrift_opts = thrift_opts_storage.get();
      }

      return make_unique_logging_object<
          impl,
          thrift_metadata_builder,
          logger_policies>(lgr, std::move(thrift_md), thrift_opts,
                          orig_fs_version, options);
    }
#endif

#ifdef DWARFS_HAVE_FLATBUFFERS
    case SerializationFormat::FLATBUFFERS:
      return make_unique_logging_object<
          impl,
          flatbuffers_metadata_builder,
          logger_policies>(lgr, existing_md, orig_fs_options,
                          orig_fs_version, options);
#endif

    default:
      DWARFS_THROW(runtime_error,
          fmt::format("Unsupported metadata format: {}",
                      static_cast<int>(format)));
  }
}

// Factory method: create builder from existing metadata (move version)
std::unique_ptr<metadata_builder::impl> metadata_builder::impl::create_from_existing(
    logger& lgr,
    metadata::domain::metadata&& existing_md,
    metadata::domain::fs_options const* orig_fs_options,
    filesystem_version const& orig_fs_version,
    metadata_options const& options,
    metadata::serialization::SerializationFormat format) {

  using namespace metadata::serialization;

  switch (format) {
#ifdef DWARFS_HAVE_THRIFT
    case SerializationFormat::THRIFT_COMPACT: {
      // Convert domain → Thrift for Thrift strategy
      auto thrift_md = metadata::converters::to_thrift(existing_md);

      // Convert fs_options if present
      thrift::metadata::fs_options const* thrift_opts = nullptr;
      std::unique_ptr<thrift::metadata::fs_options> thrift_opts_storage;
      if (orig_fs_options) {
        thrift_opts_storage = std::make_unique<thrift::metadata::fs_options>(
            metadata::converters::to_thrift(*orig_fs_options));
        thrift_opts = thrift_opts_storage.get();
      }

      return make_unique_logging_object<
          impl,
          thrift_metadata_builder,
          logger_policies>(lgr, std::move(thrift_md), thrift_opts,
                          orig_fs_version, options);
    }
#endif

#ifdef DWARFS_HAVE_FLATBUFFERS
    case SerializationFormat::FLATBUFFERS:
      return make_unique_logging_object<
          impl,
          flatbuffers_metadata_builder,
          logger_policies>(lgr, std::move(existing_md), orig_fs_options,
                          orig_fs_version, options);
#endif

    default:
      DWARFS_THROW(runtime_error,
          fmt::format("Unsupported metadata format: {}",
                      static_cast<int>(format)));
  }
}

} // namespace dwarfs::writer::internal