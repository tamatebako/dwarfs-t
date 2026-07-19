/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file domain_to_thrift.h
 * \brief Convert domain model to Modern Thrift types
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
 * Convert domain::metadata to thrift::modern::cpp2::Metadata
 *
 * Single Responsibility: Transform domain model → Thrift types
 * Dependencies: domain::metadata, thrift::modern::cpp2::Metadata
 *
 * This function performs a complete conversion from the format-agnostic
 * domain model to Modern Thrift CompactProtocol-ready types. All fields
 * are converted, including optional fields.
 *
 * @param domain_meta Domain model metadata
 * @return Thrift metadata ready for CompactProtocol serialization
 */
dwarfs::thrift::modern::cpp2::Metadata domain_to_thrift(
    domain::metadata const& domain_meta);

} // namespace dwarfs::metadata::modern

#endif // DWARFS_HAVE_MODERN_THRIFT
