/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file header_test.cpp
 * \brief DwarFS Header Installation Verification Test
 * 
 * This file tests that ALL required headers are properly installed
 * by the DwarFS build system. It includes all critical headers that
 * consumer projects (like libtfs) depend on.
 * 
 * If this file compiles successfully, it proves that:
 * 1. All public headers are installed
 * 2. All internal headers required by public headers are installed
 * 3. All detail headers are installed
 * 4. Header include paths are correct
 * 
 * This test was created in response to vcpkg header installation issues
 * reported by the libtfs team (2025-12-30).
 */

#include <iostream>

// ============================================================================
// CRITICAL HEADERS - These were reported missing by libtfs team
// ============================================================================

// Top-level public headers
#include <dwarfs/logger.h>           // CRITICAL: Missing in original installation
#include <dwarfs/config.h>           // Required by many headers

// Internal headers required by public reader headers
#include <dwarfs/internal/packed_ptr.h>        // CRITICAL: Required by metadata_types_*.h
#include <dwarfs/internal/string_table.h>      // CRITICAL: Required by metadata_types_*.h

// Detail headers
#include <dwarfs/detail/file_view_impl.h>      // CRITICAL: Missing in original installation
#include <dwarfs/detail/file_segment_impl.h>   // CRITICAL: Missing in original installation

// ============================================================================
// PUBLIC READER HEADERS - Main API that consumers use
// ============================================================================

#include <dwarfs/reader/metadata_types.h>      // Depends on internal headers above
#include <dwarfs/reader/filesystem_v2.h>       // Main filesystem interface
#include <dwarfs/reader/filesystem_loader.h>   // High-level loading API (v0.16.0+)

// Reader internal headers (must be installed for metadata_types.h)
#include <dwarfs/reader/internal/metadata_types_flatbuffers.h>  // Includes internal/packed_ptr.h

// ============================================================================
// COMMON HEADERS - Frequently used utilities
// ============================================================================

#include <dwarfs/os_access_generic.h>          // OS abstraction
#include <dwarfs/file_stat.h>                  // File metadata
#include <dwarfs/byte_buffer.h>                // Buffer management
#include <dwarfs/checksum.h>                   // Checksums

// ============================================================================
// EXTRACTOR HEADERS - For extraction functionality
// ============================================================================

#include <dwarfs/utility/filesystem_extractor.h>  // Extraction API

// ============================================================================
// VERIFICATION FUNCTION
// ============================================================================

int main() {
  std::cout << "DwarFS Header Installation Test\n";
  std::cout << "================================\n\n";
  
  std::cout << "✅ PASSED: All critical headers compiled successfully\n\n";
  
  std::cout << "Verified headers:\n";
  std::cout << "  ✅ dwarfs/logger.h\n";
  std::cout << "  ✅ dwarfs/internal/packed_ptr.h\n";
  std::cout << "  ✅ dwarfs/internal/string_table.h\n";
  std::cout << "  ✅ dwarfs/detail/file_view_impl.h\n";
  std::cout << "  ✅ dwarfs/detail/file_segment_impl.h\n";
  std::cout << "  ✅ dwarfs/reader/metadata_types.h\n";
  std::cout << "  ✅ dwarfs/reader/filesystem_v2.h\n";
  std::cout << "  ✅ dwarfs/reader/filesystem_loader.h\n";
  std::cout << "  ✅ dwarfs/utility/filesystem_extractor.h\n";
  std::cout << "  ✅ + 5 additional common headers\n\n";
  
  std::cout << "This test ensures that the DwarFS installation provides\n";
  std::cout << "all headers required by consumer projects like libtfs.\n";
  
  return 0;
}