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
#include <dwarfs/gen-cpp2/metadata_types.h>

namespace dwarfs::reader::internal {

metadata_v2_thrift_export::metadata_v2_thrift_export(metadata_v2 const& meta) {
  // Access the concrete domain_metadata_impl implementation
  // This is safe because impl_ is always a domain_metadata_impl
  impl_ = dynamic_cast<domain_metadata_impl const*>(meta.impl_.get());
  if (!impl_) {
    throw std::runtime_error("metadata_v2::impl is not a domain_metadata_impl");
  }
}

std::unique_ptr<thrift::metadata::metadata> metadata_v2_thrift_export::thaw() const {
  return impl_->thaw();
}

std::unique_ptr<thrift::metadata::metadata> metadata_v2_thrift_export::unpack() const {
  return impl_->unpack();
}

std::unique_ptr<thrift::metadata::fs_options>
metadata_v2_thrift_export::thaw_fs_options() const {
  return impl_->thaw_fs_options();
}

} // namespace dwarfs::reader::internal
