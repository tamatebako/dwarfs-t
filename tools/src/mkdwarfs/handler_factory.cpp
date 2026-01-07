/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Marcus Holland-Moritz (github@mhxnet.de)
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <dwarfs/tool/mkdwarfs/handler_factory.h>
#include <dwarfs/tool/mkdwarfs/handler_interface.h>
#include <dwarfs/tool/mkdwarfs/create_handler.h>
#ifdef DWARFS_HAVE_THRIFT
#include <dwarfs/tool/mkdwarfs/recompress_handler.h>
#endif
#include <dwarfs/tool/mkdwarfs/parsed_options.h>

#include <stdexcept>

namespace dwarfs::tool::mkdwarfs {

std::unique_ptr<handler_interface>
handler_factory::create(parsed_options const& opts) {
  if (opts.is_recompress) {
#ifdef DWARFS_HAVE_THRIFT
    return std::make_unique<recompress_handler>();
#else
    throw std::runtime_error(
        "Recompress functionality requires Thrift support.\n"
        "This build was compiled without Thrift (DWARFS_WITH_THRIFT=OFF).\n"
        "Recompressing existing images requires Thrift because the rewrite\n"
        "implementation depends on Thrift-specific metadata APIs.\n\n"
        "To use recompress features, rebuild with DWARFS_WITH_THRIFT=ON");
#endif
  }

  // Default: create handler for normal filesystem creation
  return std::make_unique<create_handler>();
}

} // namespace dwarfs::tool::mkdwarfs