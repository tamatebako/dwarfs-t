/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Marcus Holland-Moritz (github@mhxnet.de)
 * \copyright  Copyright (c) Marcus Holland-Moritz
 *
 * This file is part of dwarfs.
 *
 * SPDX-License-Identifier: MIT
 */

#include <dwarfs/reader/internal/metadata_v2_thrift_export.h>
#include <dwarfs/reader/internal/domain_metadata_impl.h>

#ifdef DWARFS_HAVE_EXPERIMENTAL_THRIFT
#include <dwarfs/gen-cpp2/metadata_types.h>
#endif

namespace dwarfs::reader::internal {

bool metadata_v2_thrift_export::is_available() {
#ifdef DWARFS_HAVE_EXPERIMENTAL_THRIFT
  return true;
#else
  return false;
#endif
}

metadata_v2_thrift_export::metadata_v2_thrift_export(metadata_v2 const& meta)
#ifdef DWARFS_HAVE_EXPERIMENTAL_THRIFT
    : thrift_available_(true) {
#else
    : thrift_available_(false) {
#endif
  // Access the concrete domain_metadata_impl implementation
  // This is safe because impl_ is always a domain_metadata_impl
  impl_ = dynamic_cast<domain_metadata_impl const*>(meta.impl_.get());
  if (!impl_) {
    throw std::runtime_error("metadata_v2::impl is not a domain_metadata_impl");
  }

#ifdef DWARFS_HAVE_EXPERIMENTAL_THRIFT
  // Thrift support is available - no additional checks needed
#else
  // Thrift support is not compiled in - warn but allow object construction
  // The methods will throw when called
#endif
}

std::unique_ptr<thrift::metadata::metadata> metadata_v2_thrift_export::thaw() const {
#ifdef DWARFS_HAVE_EXPERIMENTAL_THRIFT
  return impl_->thaw();
#else
  throw std::runtime_error("Thrift support not available (build without DWARFS_WITH_THRIFT)");
#endif
}

std::unique_ptr<thrift::metadata::metadata> metadata_v2_thrift_export::unpack() const {
#ifdef DWARFS_HAVE_EXPERIMENTAL_THRIFT
  return impl_->unpack();
#else
  throw std::runtime_error("Thrift support not available (build without DWARFS_WITH_THRIFT)");
#endif
}

std::unique_ptr<thrift::metadata::fs_options>
metadata_v2_thrift_export::thaw_fs_options() const {
#ifdef DWARFS_HAVE_EXPERIMENTAL_THRIFT
  return impl_->thaw_fs_options();
#else
  throw std::runtime_error("Thrift support not available (build without DWARFS_WITH_THRIFT)");
#endif
}

} // namespace dwarfs::reader::internal
