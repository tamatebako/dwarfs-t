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

namespace dwarfs::writer::internal {

/**
 * Abstract interface for metadata packing operations.
 * 
 * Handles packing/compression of various metadata components to
 * reduce image size. This interface defines operations for:
 * - Packing directories (delta compression)
 * - Packing chunk table (delta compression)
 * - Packing shared files table (run-length encoding)
 * - Packing string tables (FSST compression)
 * - Managing category names and metadata
 * 
 * Implementations must be format-specific (FlatBuffers or Thrift)
 * as they work directly with the underlying metadata representation.
 */
class metadata_packing_processor {
 public:
  virtual ~metadata_packing_processor() = default;

  /**
   * Pack metadata components according to options.
   * 
   * This method applies various packing optimizations:
   * 
   * 1. Directory packing: Delta-compress first_entry values
   *    to reduce metadata size when many directories exist.
   * 
   * 2. Chunk table packing: Use std::adjacent_difference to
   *    convert absolute indices to deltas, improving compression.
   * 
   * 3. Shared files packing: Run-length encode the shared files
   *    vector to reduce size when many files share data.
   * 
   * 4. String table packing: Apply FSST compression to names
   *    and symlinks tables for significant size reduction.
   * 
   * 5. Category cleanup: Remove category names and metadata
   *    if requested via options.
   * 
   * All packing is controlled by metadata_options flags and
   * only applied if explicitly enabled.
   */
  virtual void pack_metadata() = 0;
};

} // namespace dwarfs::writer::internal