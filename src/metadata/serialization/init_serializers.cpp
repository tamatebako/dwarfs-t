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

namespace dwarfs::metadata::serialization {

void init_serializers() {
  // Call registration functions to ensure serializers are registered
  // This ensures static initializers run even when linking from static libraries

#ifdef DWARFS_HAVE_THRIFT
  register_thrift_serializer();
#endif

#ifdef DWARFS_HAVE_FLATBUFFERS
  register_flatbuffers_serializer();
#endif
}

} // namespace dwarfs::metadata::serialization