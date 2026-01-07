/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file thrift_to_domain.h
 * \brief Convert Modern Thrift types to domain model
 *
 * \author     Marcus Holland-Moritz (github@mhxnet.de)
 * \copyright  Copyright (c) Marcus Holland-Moritz
 *
 * This file is part of dwarfs.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "dwarfs/metadata/domain/metadata.h"

#ifdef DWARFS_HAVE_MODERN_THRIFT
// Include generated Thrift types (modern fbthrift generates in cpp2 namespace)
#include <dwarfs/gen-cpp2/metadata_modern_types.h>

namespace dwarfs::metadata::modern {

/**
 * Convert thrift::modern::cpp2::Metadata to domain::metadata
 *
 * Single Responsibility: Transform Thrift types → domain model
 * Dependencies: thrift::modern::cpp2::Metadata, domain::metadata
 *
 * This function performs a complete conversion from Modern Thrift
 * CompactProtocol types back to the format-agnostic domain model.
 * All fields are converted, including optional fields.
 *
 * @param thrift_meta Thrift metadata from CompactProtocol deserialization
 * @return Domain model metadata
 */
domain::metadata thrift_to_domain(
    dwarfs::thrift::modern::cpp2::Metadata const& thrift_meta);

} // namespace dwarfs::metadata::modern

#endif // DWARFS_HAVE_MODERN_THRIFT
