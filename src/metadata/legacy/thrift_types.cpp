/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Marcus Holland-Moritz (github@mhx.io)
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

#include "dwarfs/metadata/legacy/thrift_types.h"

namespace dwarfs::metadata::legacy {

const char* tag_name(Tag t) {
  switch (t) {
  case Tag::BOOL_TRUE:
    return "BOOL_TRUE";
  case Tag::BOOL_FALSE:
    return "BOOL_FALSE";
  case Tag::I16:
    return "I16";
  case Tag::I32:
    return "I32";
  case Tag::I64:
    return "I64";
  case Tag::BINARY:
    return "BINARY";
  case Tag::LIST:
    return "LIST";
  case Tag::MAP:
    return "MAP";
  case Tag::STRUCT:
    return "STRUCT";
  case Tag::STRUCT_HOMEBREW:
    return "STRUCT_HOMEBREW";
  case Tag::UNKNOWN_BOOL:
    return "UNKNOWN_BOOL";
  case Tag::INVALID:
    return "INVALID";
  }
  return "UNKNOWN";
}

} // namespace dwarfs::metadata::legacy