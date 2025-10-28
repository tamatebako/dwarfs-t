/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file
 * \brief Compilation test for domain model headers
 * \author Marcus Holland-Moritz (github@mhxnet.de)
 * \copyright Copyright (c) Marcus Holland-Moritz
 *
 * This file is part of dwarfs.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * SPDX-License-Identifier: MIT
 */

// This file is used to verify that all domain model headers compile correctly
// It should NOT be built as part of the library

#include "dwarfs/metadata/domain/chunk.h"
#include "dwarfs/metadata/domain/dir_entry.h"
#include "dwarfs/metadata/domain/directory.h"
#include "dwarfs/metadata/domain/feature.h"
#include "dwarfs/metadata/domain/flac_block_header.h"
#include "dwarfs/metadata/domain/fs_options.h"
#include "dwarfs/metadata/domain/history_entry.h"
#include "dwarfs/metadata/domain/inode_data.h"
#include "dwarfs/metadata/domain/inode_size_cache.h"
#include "dwarfs/metadata/domain/metadata.h"
#include "dwarfs/metadata/domain/ricepp_block_header.h"
#include "dwarfs/metadata/domain/string_table.h"

#include <cereal/archives/binary.hpp>
#include <sstream>

namespace {

// Verify that each domain model can be instantiated and serialized
void test_compilation() {
  using namespace dwarfs::metadata::domain;

  // Test basic structs
  chunk c;
  dir_entry de;
  directory d;
  feature f = feature::sparsefiles;
  flac_block_header fbh;
  fs_options fso;
  history_entry he;
  inode_data id;
  inode_size_cache isc;
  metadata m;
  ricepp_block_header rbh;
  string_table st;

  // Test serialization
  std::stringstream ss;
  {
    cereal::BinaryOutputArchive oarchive(ss);
    oarchive(c, de, d, fbh, fso, he, id, isc, m, rbh, st);
  }

  // Test deserialization
  {
    cereal::BinaryInputArchive iarchive(ss);
    chunk c2;
    dir_entry de2;
    directory d2;
    flac_block_header fbh2;
    fs_options fso2;
    history_entry he2;
    inode_data id2;
    inode_size_cache isc2;
    metadata m2;
    ricepp_block_header rbh2;
    string_table st2;

    iarchive(c2, de2, d2, fbh2, fso2, he2, id2, isc2, m2, rbh2, st2);
  }
}

} // anonymous namespace