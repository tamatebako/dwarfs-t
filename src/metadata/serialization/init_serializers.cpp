/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Ribose (@riboseinc @tamatebako)
 * \copyright  Copyright (c) Ribose
 */

#include "dwarfs/metadata/serialization/init_serializers.h"

// Include all serializer headers to force linking
#ifdef DWARFS_HAVE_THRIFT
#include "dwarfs/metadata/serialization/thrift_compact_serializer.h"
#endif

#ifdef DWARFS_HAVE_FLATBUFFERS
#include "dwarfs/metadata/serialization/flatbuffers_serializer.h"
#endif

// Legacy Thrift is always available (no external dependencies)
#include "dwarfs/metadata/serialization/legacy_thrift_serializer.h"

namespace dwarfs::metadata::serialization {

void init_serializers() {
  // Call registration functions to ensure serializers are registered
  // This ensures static initializers run even when linking from static libraries

  // Legacy Thrift (always available, hand-coded implementation)
  register_legacy_thrift_serializer();

#ifdef DWARFS_HAVE_THRIFT
  #ifdef DWARFS_USE_THRIFT_COMPACT
  // Modern Thrift Compact (requires full Facebook stack)
  register_thrift_compact_serializer();
  #else
  // Modern Thrift Binary (requires full Facebook stack)
  register_thrift_compact_serializer();
  #endif
#endif

#ifdef DWARFS_HAVE_FLATBUFFERS
  register_flatbuffers_serializer();
#endif
}

} // namespace dwarfs::metadata::serialization