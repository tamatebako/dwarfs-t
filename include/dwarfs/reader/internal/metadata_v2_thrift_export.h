/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Marcus Holland-Moritz (github@mhxnet.de)
 * \copyright  Copyright (c) Marcus Holland-Moritz
 *
 * This file is part of dwarfs.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <memory>
#include <stdexcept>

namespace dwarfs::reader::internal {

class metadata_v2;
class domain_metadata_impl;

namespace thrift::metadata {
class fs_options;
class metadata;
}

/**
 * Thrift-specific metadata export functionality
 *
 * This class provides methods to convert domain metadata to Thrift format.
 * It is ONLY compiled when Modern Thrift (fbthrift/folly) is available.
 *
 * Design:
 * - Single Responsibility: Thrift export operations only
 * - This entire class is conditionally compiled via CMake
 * - No preprocessor guards in the code - the class simply doesn't exist
 *   when Thrift is not available
 */
class metadata_v2_thrift_export {
 public:
  /**
   * Construct from metadata_v2
   *
   * @param meta The metadata object to export
   * @throws std::runtime_error if the implementation is not domain_metadata_impl
   */
  explicit metadata_v2_thrift_export(metadata_v2 const& meta);

  /**
   * Convert domain metadata to Thrift format (frozen/compact)
   *
   * @return Thrift metadata object
   */
  std::unique_ptr<::thrift::metadata::metadata> thaw() const;

  /**
   * Convert domain metadata to Thrift format (unpacked/expanded)
   *
   * @return Thrift metadata object with expanded tables
   */
  std::unique_ptr<::thrift::metadata::metadata> unpack() const;

  /**
   * Extract filesystem options from domain metadata
   *
   * @return Thrift fs_options object, or nullptr if not available
   */
  std::unique_ptr<::thrift::metadata::fs_options> thaw_fs_options() const;

 private:
  domain_metadata_impl const* impl_;  // Non-owning pointer
};

} // namespace dwarfs::reader::internal
