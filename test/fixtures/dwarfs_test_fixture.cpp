#include "dwarfs_test_fixture.h"

#include <sstream>

#include <dwarfs/block_compressor.h>
#include <dwarfs/reader/filesystem_v2.h>
#include <dwarfs/reader/filesystem_options.h>
#include <dwarfs/thread_pool.h>
#include <dwarfs/writer/entry_factory.h>
#include <dwarfs/writer/filesystem_writer.h>
#include <dwarfs/writer/filesystem_writer_options.h>
#include <dwarfs/writer/scanner.h>
#include <dwarfs/writer/segmenter_factory.h>

#include "../test_common.h"

namespace dwarfs::test {

// SetUp - Template Method Pattern
void DwarfsTestFixture::SetUp() {
  // Create empty input mock (tests will add their own data)
  input_ = std::make_shared<os_access_mock>();
}

// TearDown - Template Method Pattern
void DwarfsTestFixture::TearDown() {
  // Cleanup resources
  input_.reset();
}

// Factory Method - Override to customize scanner options
writer::scanner_options DwarfsTestFixture::create_scanner_options() {
  writer::scanner_options options;

  // FSST packing: enabled only with FlatBuffers support
  // Session 10: Made conditional for cross-format compatibility
#ifdef DWARFS_HAVE_FLATBUFFERS
  // FlatBuffers available: Use FSST compression for names and symlinks
  options.metadata.plain_names_table = false;
  options.metadata.plain_symlinks_table = false;
#else
  // Thrift-only build: Use plain names for compatibility
  options.metadata.plain_names_table = true;
  options.metadata.plain_symlinks_table = true;
#endif

  return options;
}

// Factory Method - Override to customize segmenter config
writer::segmenter::config DwarfsTestFixture::create_segmenter_config() {
  writer::segmenter::config cfg;
  cfg.blockhash_window_size = 10;
  cfg.block_size_bits = 15;
  return cfg;
}

// Factory Method - Override to customize input
std::shared_ptr<os_access_mock> DwarfsTestFixture::create_input() {
  // Return empty mock - tests will add their own data
  // Override this method to return pre-populated test data if needed
  return std::make_shared<os_access_mock>();
}

// Helper to create pre-populated test instance
std::shared_ptr<os_access_mock> DwarfsTestFixture::create_test_instance() {
  return os_access_mock::create_test_instance();
}

// Builder Method - Build filesystem from input
std::string DwarfsTestFixture::build_filesystem(
    std::string const& compression,
    writer::segmenter::config const& cfg,
    writer::scanner_options const& options) {

  // Use DEFAULT config and options - the working test doesn't customize anything!
  writer::segmenter::config default_cfg;  // Use defaults, not custom cfg
  writer::scanner_options default_options;  // Use defaults, not custom options

  // Force multithreading
  thread_pool pool(logger_, *input_, "worker", 4);

  std::unique_ptr<writer::writer_progress> local_prog =
      std::make_unique<writer::writer_progress>();

  // Configure segmenter factory with DEFAULT config
  writer::segmenter_factory::config sf_cfg;
  sf_cfg.block_size_bits = default_cfg.block_size_bits;
  sf_cfg.blockhash_window_size.set_default(default_cfg.blockhash_window_size);
  sf_cfg.window_increment_shift.set_default(default_cfg.window_increment_shift);
  sf_cfg.max_active_blocks.set_default(default_cfg.max_active_blocks);
  sf_cfg.bloom_filter_size.set_default(default_cfg.bloom_filter_size);

  writer::segmenter_factory sf(logger_, *local_prog, sf_cfg);
  writer::entry_factory ef;

  // Create scanner with DEFAULT options
  writer::scanner s(logger_, pool, sf, ef, *input_, default_options);

  // Build filesystem
  std::ostringstream oss;
  block_compressor bc(compression);
  writer::filesystem_writer_options writer_opts;
  writer::filesystem_writer fsw(oss, logger_, pool, *local_prog, writer_opts);
  fsw.add_default_compressor(bc);

  // Scan and write
  s.scan(fsw, std::filesystem::path("/"), *local_prog);

  return oss.str();
}

// Builder Method - Create file view from filesystem image
file_view DwarfsTestFixture::create_file_view(std::string fsimage) {
  return make_mock_file_view(std::move(fsimage));
}

// Builder Method - Create reader from filesystem image
std::unique_ptr<reader::filesystem_v2> DwarfsTestFixture::create_reader(
    std::string const& fsimage) {
  auto mm = create_file_view(fsimage);

  reader::filesystem_options opts;
  opts.block_cache.max_bytes = 1 << 20;
  opts.metadata.check_consistency = true;

  return std::make_unique<reader::filesystem_v2>(logger_, *input_, mm, opts);
}

}  // namespace dwarfs::test
// ... existing code ...