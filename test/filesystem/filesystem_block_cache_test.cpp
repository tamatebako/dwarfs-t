/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     DwarFS Implementation
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

/**
 * Block Cache worker_group Initialization Regression Test
 *
 * Regression test for a SIGBUS crash fixed in the dwarfs-t lineage
 * (commit 3b15ed05 "feat: static-build and tebako hardening",
 * src/reader/internal/block_cache.cpp).
 *
 * Original bug (reported downstream by libtfs/tebako, 2026-02):
 *   Reading large highly-compressible files (e.g. a 10 MiB sparse file
 *   compressing to ~100 bytes) crashed with SIGBUS on macOS ARM64.
 *   Root cause: the block_cache worker_group was initialized lazily via
 *   std::call_once on first use (enqueue_job), which raced when multiple
 *   reader threads hit the cache concurrently right after construction.
 *
 * Fix:
 *   The worker group is now initialized eagerly in the block_cache
 *   constructor (and the std::call_once / init_worker_group machinery
 *   was removed), so no first-use initialization race can occur.
 *
 * This test reproduces the reported trigger as closely as possible:
 * a 10 MiB all-zero (extremely compressible) file, read concurrently
 * from many threads immediately after the filesystem (and hence the
 * block cache) is constructed, with per-thread offset skew so reads
 * hit block boundaries at different alignments.
 *
 * Note: without the fix this manifests as a *crash* (SIGBUS), not a
 * clean assertion failure, and as a race it is timing/platform
 * dependent (observed on macOS ARM64). The test also verifies data
 * correctness of every byte read.
 */

#include <gtest/gtest.h>

#include <atomic>
#include <cstddef>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include <dwarfs/config.h>
#include <dwarfs/reader/filesystem_options.h>
#include <dwarfs/reader/filesystem_v2.h>

#include "../fixtures/filesystem_test_fixture.h"

using namespace dwarfs;

namespace dwarfs::test {

class FilesystemBlockCacheTest : public FilesystemTestFixture {
  // No additional setup needed - uses base fixture
};

TEST_F(FilesystemBlockCacheTest, ConcurrentHighlyCompressibleFileReads) {
  // Mirror the original report: 10 MiB sparse file that compresses to
  // roughly a hundred bytes.
  constexpr size_t kFileSize = 10 * 1024 * 1024;
  constexpr size_t kNumThreads = 8;
  constexpr size_t kChunkSize = 64 * 1024;

  input_->add("", {1, 040755, 1, 0, 0, 10, 42, 0, 0, 0});
  input_->add_file("sparse.bin", std::string(kFileSize, '\0'));

#if defined(DWARFS_HAVE_LIBBROTLI)
  std::string compression{"brotli:quality=0"};
#elif defined(DWARFS_HAVE_LIBLZMA)
  std::string compression{"lzma:level=0"};
#else
  std::string compression{"zstd:level=5"};
#endif

  auto fsimage = build_filesystem(compression);

  // Create the filesystem (and block cache) with an explicit worker
  // count; reads below start immediately and concurrently.
  file_view_ = create_file_view(fsimage);
  reader::filesystem_options opts;
  opts.block_cache.max_bytes = 1 << 20;
  opts.block_cache.num_workers = 4;
  filesystem_ = std::make_unique<reader::filesystem_v2>(
      logger_, *input_, file_view_, opts);

  auto dev = filesystem_->find("/sparse.bin");
  ASSERT_TRUE(dev) << "sparse.bin not found in test image";

  auto iv = dev->inode();
  auto st = filesystem_->getattr(iv);
  ASSERT_TRUE(st.is_regular_file());
  ASSERT_EQ(static_cast<size_t>(st.size()), kFileSize);

  auto const inode_num = iv.inode_num();

  std::atomic<size_t> read_errors{0};
  std::atomic<size_t> bad_bytes{0};

  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);

  for (size_t t = 0; t < kNumThreads; ++t) {
    threads.emplace_back([&, t] {
      std::vector<char> buf(kChunkSize);
      // Per-thread skew: different threads read at different alignments
      // relative to the (compressed) block boundaries.
      for (size_t off = t * 4096; off < kFileSize; off += kChunkSize) {
        std::error_code ec;
        size_t n = filesystem_->read(inode_num, buf.data(),
                                     buf.size(),
                                     static_cast<file_off_t>(off), ec);
        if (ec) {
          read_errors.fetch_add(1, std::memory_order_relaxed);
          continue;
        }
        for (size_t i = 0; i < n; ++i) {
          if (buf[i] != 0) {
            bad_bytes.fetch_add(1, std::memory_order_relaxed);
            break;
          }
        }
      }
    });
  }

  for (auto& th : threads) {
    th.join();
  }

  EXPECT_EQ(read_errors.load(), 0u);
  EXPECT_EQ(bad_bytes.load(), 0u);
}

} // namespace dwarfs::test
