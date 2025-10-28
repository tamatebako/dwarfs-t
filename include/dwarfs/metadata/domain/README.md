# DwarFS Metadata Domain Models

This directory contains pure C++ domain model classes for DwarFS metadata, implementing Phase 2 of Track B (Thrift/Folly Removal).

## Overview

These classes represent the core domain entities of the DwarFS file system format, with full Cereal serialization support for persistence and wire transmission.

## Design Principles

### MECE (Mutually Exclusive, Collectively Exhaustive)
Each class represents ONE distinct concept from the .dwarfs format:
- **chunk**: A view onto a file system block
- **directory**: Directory structure and entry ranges
- **inode_data**: Inode metadata (permissions, ownership, timestamps)
- **dir_entry**: Single directory entry (name + inode)
- **fs_options**: File system configuration options
- **string_table**: Packed string storage with optional compression
- **inode_size_cache**: Performance optimization for fragmented files
- **history_entry**: Metadata version tracking
- **metadata**: Root structure aggregating all file system metadata
- **feature**: File system feature flags (enum)
- **flac_block_header**: FLAC compression metadata
- **ricepp_block_header**: Rice++ compression metadata

### Separation of Concerns
- Domain models are pure data structures
- No business logic mixed with data representation
- Serialization is declarative via Cereal templates
- Each class has single, clear responsibility

### Object-Oriented Design
- Proper encapsulation with clear interfaces
- Header-only for Cereal template instantiation
- Version-aware serialization for schema evolution
- No thrift or folly dependencies

## Files

### Core Metadata Structures
- [`chunk.h`](chunk.h) - Data chunk representation
- [`directory.h`](directory.h) - Directory structure
- [`inode_data.h`](inode_data.h) - Inode metadata
- [`dir_entry.h`](dir_entry.h) - Directory entry
- [`metadata.h`](metadata.h) - Root metadata structure

### Supporting Structures
- [`fs_options.h`](fs_options.h) - File system options
- [`string_table.h`](string_table.h) - Packed string storage
- [`inode_size_cache.h`](inode_size_cache.h) - Size cache for fragmented files
- [`history_entry.h`](history_entry.h) - Version history tracking
- [`feature.h`](feature.h) - Feature enumeration

### Compression Metadata
- [`flac_block_header.h`](flac_block_header.h) - FLAC compression metadata
- [`ricepp_block_header.h`](ricepp_block_header.h) - Rice++ compression metadata

## Serialization

All structures support Cereal serialization with versioning:

```cpp
template <class Archive>
void serialize(Archive& ar, std::uint32_t const version) {
  // Base fields
  ar(CEREAL_NVP(field1), CEREAL_NVP(field2));

  // Version-specific fields
  if (version >= 2) {
    ar(CEREAL_NVP(field3));
  }
}
```

Version numbers are declared using:
```cpp
CEREAL_CLASS_VERSION(namespace::class_name, version_number)
```

### Version Evolution

Classes support schema evolution through versioned serialization:
- **Version 1**: Base implementation matching original thrift schema
- **Version 2+**: Additional fields for newer DwarFS versions
- Backward compatibility maintained through version checks

Example from [`inode_data.h`](inode_data.h):
- Version 1: Core timestamp and ownership fields
- Version 2: Birth time, subsecond timestamps, hard link counts

## Usage Example

```cpp
#include <dwarfs/metadata/domain/metadata.h>
#include <cereal/archives/binary.hpp>
#include <sstream>

using namespace dwarfs::metadata::domain;

// Create metadata
metadata meta;
meta.block_size = 131072;
meta.total_fs_size = 1024 * 1024 * 1024;

// Serialize
std::stringstream ss;
{
  cereal::BinaryOutputArchive archive(ss);
  archive(meta);
}

// Deserialize
{
  cereal::BinaryInputArchive archive(ss);
  metadata meta2;
  archive(meta2);
}
```

## Relationship to Thrift Schema

These classes are direct C++ translations of the thrift schemas:
- `thrift/metadata.thrift` → Domain model classes
- `thrift/compression.thrift` → Compression header classes
- `thrift/features.thrift` → Feature enum

Key differences:
- Pure C++ types instead of thrift types
- Cereal serialization instead of thrift serialization
- STL containers instead of folly containers
- No thrift/folly dependencies

## Next Steps (Phase 3)

Phase 3 will implement the serialization layer:
- Binary serializers/deserializers
- Format conversion utilities
- Migration from thrift to Cereal
- Integration with existing codebase

## References

- [Cereal Documentation](https://uscilab.github.io/cereal/)
- [Track B Architecture](../../../TRACKB_ARCHITECTURE.md)
- Original thrift schemas in `thrift/` directory