/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Ribose (@riboseinc @tamatebako)
 * \copyright  Copyright (c) Ribose
 */
#pragma once

namespace dwarfs::metadata::serialization {

/**
 * Initialize all serializers
 *
 * This function ensures that all serializer static initializers
 * are executed by forcing a reference to each serializer implementation.
 * Call this before using the SerializerRegistry.
 */
void init_serializers();

} // namespace dwarfs::metadata::serialization