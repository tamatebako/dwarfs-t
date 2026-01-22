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

} // namespace dwarfs::reader::internal

// Forward declarations for Thrift types in dwarfs::thrift::metadata namespace
namespace dwarfs {
namespace thrift {
namespace metadata {
class fs_options;
class metadata;
}
}
}

namespace dwarfs::reader::internal {

/**
 * Thrift-specific metadata export functionality
 *
 * This class provides methods to convert domain metadata to Thrift format.
 * The class is always available, but methods throw when Thrift support
 * was not compiled into the binary.
 *
 * Design:
 * - Single Responsibility: Thrift export operations only
 * - Runtime availability check via is_available() static method
 * - Methods throw std::runtime_error when Thrift is not available
 * - Pure OOP: No preprocessor guards in client code
 */
class metadata_v2_thrift_export {
 public:
  /**
   * Check if Thrift support is available at runtime
   *
   * @return true if Modern Thrift (fbthrift/folly) support is compiled in
   */
  static bool is_available();

  /**
   * Construct from metadata_v2
   *
   * @param meta The metadata object to export
   * @throws std::runtime_error if Thrift support is not available
   * @throws std::runtime_error if the implementation is not domain_metadata_impl
   */
  explicit metadata_v2_thrift_export(metadata_v2 const& meta);

  /**
   * Convert domain metadata to Thrift format (frozen/compact)
   *
   * @return Thrift metadata object
   * @throws std::runtime_error if Thrift support is not available
   */
  std::unique_ptr<thrift::metadata::metadata> thaw() const;

  /**
   * Convert domain metadata to Thrift format (unpacked/expanded)
   *
   * @return Thrift metadata object with expanded tables
   * @throws std::runtime_error if Thrift support is not available
   */
  std::unique_ptr<thrift::metadata::metadata> unpack() const;

  /**
   * Extract filesystem options from domain metadata
   *
   * @return Thrift fs_options object, or nullptr if not available
   * @throws std::runtime_error if Thrift support is not available
   */
  std::unique_ptr<thrift::metadata::fs_options> thaw_fs_options() const;

 private:
  domain_metadata_impl const* impl_;  // Non-owning pointer
#ifdef DWARFS_HAVE_EXPERIMENTAL_THRIFT
  bool const thrift_available_;
#endif
};

} // namespace dwarfs::reader::internal
