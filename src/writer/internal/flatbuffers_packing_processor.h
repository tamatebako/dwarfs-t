/* vim:set ts=2 sw=2 sts=2 et: */
/**
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

#include <dwarfs/writer/internal/metadata_packing_processor.h>

namespace dwarfs {
class logger;
}

namespace dwarfs::metadata::domain {
class metadata;
}

namespace dwarfs::writer {
struct metadata_options;
}

namespace dwarfs::writer::internal {

/**
 * FlatBuffers implementation of packing processor.
 * 
 * Handles:
 * - Directory packing (delta compression)
 * - Chunk table packing (delta compression)
 * - Shared files table packing (run-length encoding)
 * - String table packing (FSST compression)
 * - Category cleanup
 */
class flatbuffers_packing_processor final : public metadata_packing_processor {
 public:
  /**
   * Construct packing processor.
   * 
   * @param lgr Logger instance
   * @param md Metadata to pack
   * @param options Metadata options
   */
  flatbuffers_packing_processor(logger& lgr,
                                metadata::domain::metadata& md,
                                metadata_options const& options);

  // Implement interface
  void pack_metadata() override;

 private:
  logger& lgr_;
  metadata::domain::metadata& md_;
  metadata_options const& options_;
};

} // namespace dwarfs::writer::internal