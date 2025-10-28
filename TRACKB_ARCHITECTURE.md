# Track B: Thrift/Folly Removal - Object-Oriented Architecture

**Version:** 1.0
**Date:** 2025-10-28
**Status:** Design Phase

## Executive Summary

This document defines the object-oriented architecture for migrating DwarFS from Apache Thrift + Facebook Folly to Cereal serialization while maintaining 100% backward compatibility with existing `.dwarfs` filesystem images.

### Design Principles

- **SOLID Principles** - Single Responsibility, Open/Closed, Liskov Substitution, Interface Segregation, Dependency Inversion
- **MECE** - Mutually Exclusive, Collectively Exhaustive domain boundaries
- **Separation of Concerns** - Clear boundaries between serialization, business logic, and format detection
- **Model-Based Architecture** - Domain models separate from serialization formats
- **Extensibility** - Easy to add new formats and evolve schemas

---

## 1. Architecture Overview

### 1.1 Layered Architecture Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                    Application Layer                        │
│  (filesystem_v2, metadata_builder, filesystem_writer)       │
└────────────────────────┬────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────────┐
│                   Domain Model Layer                        │
│        (Metadata, Chunk, Directory, InodeData, etc.)        │
│              Pure C++ - No serialization deps               │
└────────────────────────┬────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────────┐
│              Serialization Abstraction Layer                │
│  ┌──────────────────────────────────────────────────────┐   │
│  │  IMetadataSerializer (Strategy Interface)            │   │
│  │  - serialize(metadata) → bytes                       │   │
│  │  - deserialize(bytes) → metadata                     │   │
│  │  - get_format() → SerializationFormat                │   │
│  └──────────────────────────────────────────────────────┘   │
└────────────────────────┬────────────────────────────────────┘
                         │
         ┌───────────────┼───────────────┐
         ▼               ▼               ▼
┌─────────────┐ ┌─────────────┐ ┌─────────────┐
│   Thrift    │ │   Cereal    │ │   Future    │
│  Serializer │ │  Serializer │ │   Formats   │
│  (Legacy)   │ │    (New)    │ │             │
└─────────────┘ └─────────────┘ └─────────────┘
```

### 1.2 Data Flow

```
Reading Legacy .dwarfs File:
┌──────────┐   ┌──────────────┐   ┌─────────────┐   ┌──────────┐
│Raw Bytes │──►│FormatDetector│──►│ThriftSerial-│──►│ Domain   │
│          │   │   (Thrift)   │   │   izer      │   │ Metadata │
└──────────┘   └──────────────┘   └─────────────┘   └──────────┘

Writing New .dwarfs File:
┌──────────┐   ┌─────────────┐   ┌──────────┐
│ Domain   │──►│CerealSerial-│──►│Raw Bytes │
│ Metadata │   │   izer      │   │          │
└──────────┘   └─────────────┘   └──────────┘
```

---

## 2. Domain Model (Pure Business Logic)

### 2.1 Core Domain Classes

```cpp
// include/dwarfs/metadata/domain/metadata.h
namespace dwarfs::metadata::domain {

/**
 * @brief Core filesystem metadata (format-agnostic)
 *
 * This is the canonical representation independent of serialization.
 * Follows Single Responsibility Principle - only manages metadata state.
 */
class Metadata {
public:
    // Core structures
    std::vector<Chunk>& chunks() { return chunks_; }
    std::vector<Chunk> const& chunks() const { return chunks_; }

    std::vector<Directory>& directories() { return directories_; }
    std::vector<Directory> const& directories() const { return directories_; }

    std::vector<InodeData>& inodes() { return inodes_; }
    std::vector<InodeData> const& inodes() const { return inodes_; }

    // Properties
    uint32_t block_size() const { return block_size_; }
    void set_block_size(uint32_t size) { block_size_ = size; }

    // Features
    bool has_feature(std::string_view feature) const;
    void add_feature(std::string feature);

    // Validation
    void validate() const;

private:
    std::vector<Chunk> chunks_;
    std::vector<Directory> directories_;
    std::vector<InodeData> inodes_;
    std::vector<uint32_t> chunk_table_;
    std::vector<uint32_t> symlink_table_;
    std::vector<uint32_t> uids_;
    std::vector<uint32_t> gids_;
    std::vector<uint32_t> modes_;
    std::vector<std::string> names_;
    std::vector<std::string> symlinks_;

    uint32_t block_size_{0};
    uint64_t total_fs_size_{0};
    uint64_t timestamp_base_{0};

    std::optional<FilesystemOptions> options_;
    std::set<std::string> features_;
};

class Chunk {
public:
    Chunk(uint32_t block, uint32_t offset, uint32_t size);

    uint32_t block() const { return block_; }
    uint32_t offset() const { return offset_; }
    uint32_t size() const { return size_; }

    bool is_hole() const;

private:
    uint32_t block_, offset_, size_;
};

class Directory {
public:
    uint32_t parent_entry() const;
    uint32_t first_entry() const;
    uint32_t self_entry() const;

private:
    uint32_t parent_entry_, first_entry_, self_entry_;
};

class InodeData {
public:
    uint32_t mode_index() const;
    uint32_t owner_index() const;
    uint64_t mtime_offset() const;

private:
    uint32_t mode_index_, owner_index_, group_index_;
    uint64_t atime_offset_, mtime_offset_, ctime_offset_;
};

} // namespace dwarfs::metadata::domain
```

### 2.2 Domain Model Principles

1. **No Serialization Dependencies** - Pure C++ standard library
2. **Value Semantics** - Copyable and moveable
3. **Const-Correctness** - Clear read vs write operations
4. **Validation Logic** - Built-in consistency checking
5. **Self-Documenting** - Clear naming, comprehensive comments

---

## 3. Serialization Abstraction Layer

### 3.1 Strategy Interface

```cpp
// include/dwarfs/metadata/serialization/serializer.h
namespace dwarfs::metadata::serialization {

enum class SerializationFormat {
    UNKNOWN,
    THRIFT_COMPACT,    // Legacy
    CEREAL_BINARY,     // New default
};

struct FormatInfo {
    SerializationFormat format;
    uint32_t version;
    std::string name;
    bool is_legacy;
};

/**
 * @brief Abstract serializer interface (Strategy Pattern)
 *
 * SOLID Compliance:
 * - Single Responsibility: Serialization only
 * - Open/Closed: Extend with new formats
 * - Liskov Substitution: All implementations substitutable
 * - Interface Segregation: Minimal interface
 * - Dependency Inversion: Depends on domain abstractions
 */
class IMetadataSerializer {
public:
    virtual ~IMetadataSerializer() = default;

    /**
     * @brief Serialize metadata to bytes
     */
    virtual std::vector<uint8_t> serialize(
        domain::Metadata const& metadata) const = 0;

    /**
     * @brief Deserialize metadata from bytes
     */
    virtual std::unique_ptr<domain::Metadata> deserialize(
        std::span<uint8_t const> data) const = 0;

    /**
     * @brief Get format information
     */
    virtual FormatInfo get_format_info() const = 0;

    SerializationFormat get_format() const {
        return get_format_info().format;
    }
};

} // namespace dwarfs::metadata::serialization
```

### 3.2 Format Detection

```cpp
// include/dwarfs/metadata/serialization/format_detector.h
namespace dwarfs::metadata::serialization {

/**
 * @brief Detects serialization format from data
 *
 * Uses magic bytes and heuristics to identify format.
 * Extensible through registration mechanism.
 */
class FormatDetector {
public:
    struct MagicBytes {
        std::vector<uint8_t> bytes;
        size_t offset;
    };

    /**
     * @brief Detect format from data
     */
    static SerializationFormat detect(std::span<uint8_t const> data);

    /**
     * @brief Register magic bytes for format
     */
    static void register_magic(SerializationFormat format, MagicBytes magic);

private:
    static FormatDetector& instance();
    std::unordered_map<SerializationFormat, std::vector<MagicBytes>> magic_map_;
};

} // namespace dwarfs::metadata::serialization
```

### 3.3 Serializer Factory

```cpp
// include/dwarfs/metadata/serialization/serializer_factory.h
namespace dwarfs::metadata::serialization {

/**
 * @brief Factory for creating serializers (Factory Pattern)
 *
 * Supports runtime registration of new formats.
 */
class SerializerFactory {
public:
    using creator_t = std::function<std::unique_ptr<IMetadataSerializer>()>;

    static SerializerFactory& instance();

    /**
     * @brief Create serializer for format
     */
    std::unique_ptr<IMetadataSerializer> create(
        SerializationFormat format) const;

    /**
     * @brief Create serializer by auto-detecting format
     */
    std::unique_ptr<IMetadataSerializer> create_from_data(
        std::span<uint8_t const> data) const;

    /**
     * @brief Register new serializer
     */
    void register_serializer(SerializationFormat format, creator_t creator);

private:
    std::unordered_map<SerializationFormat, creator_t> creators_;
};

} // namespace dwarfs::metadata::serialization
```

---

## 4. Format Implementations

### 4.1 Thrift Serializer (Legacy Support)

```cpp
// include/dwarfs/metadata/serialization/thrift_serializer.h
namespace dwarfs::metadata::serialization {

/**
 * @brief Thrift compact protocol serializer (LEGACY)
 *
 * Provides backward compatibility with existing .dwarfs files.
 * Uses existing thrift code wrapped in new interface.
 */
class ThriftSerializer : public IMetadataSerializer {
public:
    std::vector<uint8_t> serialize(
        domain::Metadata const& metadata) const override;

    std::unique_ptr<domain::Metadata> deserialize(
        std::span<uint8_t const> data) const override;

    FormatInfo get_format_info() const override {
        return {
            .format = SerializationFormat::THRIFT_COMPACT,
            .version = 2,
            .name = "Apache Thrift Compact",
            .is_legacy = true
        };
    }

private:
    // Convert between domain model and thrift types
    std::unique_ptr<domain::Metadata> convert_from_thrift(
        thrift::metadata::metadata const& thrift_meta) const;

    thrift::metadata::metadata convert_to_thrift(
        domain::Metadata const& metadata) const;
};

} // namespace dwarfs::metadata::serialization
```

### 4.2 Cereal Serializer (New Default)

```cpp
// include/dwarfs/metadata/serialization/cereal_serializer.h
namespace dwarfs::metadata::serialization {

/**
 * @brief Cereal binary serializer (NEW DEFAULT)
 *
 * Header-only, supports versioning and schema evolution.
 */
class CerealBinarySerializer : public IMetadataSerializer {
public:
    static constexpr uint32_t MAGIC = 0x44574643; // "DWFC"
    static constexpr uint32_t VERSION = 1;

    std::vector<uint8_t> serialize(
        domain::Metadata const& metadata) const override;

    std::unique_ptr<domain::Metadata> deserialize(
        std::span<uint8_t const> data) const override;

    FormatInfo get_format_info() const override {
        return {
            .format = SerializationFormat::CEREAL_BINARY,
            .version = VERSION,
            .name = "Cereal Binary",
            .is_legacy = false
        };
    }
};

} // namespace dwarfs::metadata::serialization
```

### 4.3 Cereal Serialization Templates

```cpp
// include/dwarfs/metadata/domain/cereal_support.h
#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/optional.hpp>

namespace dwarfs::metadata::domain {

template<class Archive>
void serialize(Archive& ar, Metadata& m, uint32_t version) {
    ar(cereal::make_nvp("chunks", m.chunks_));
    ar(cereal::make_nvp("directories", m.directories_));
    ar(cereal::make_nvp("inodes", m.inodes_));
    ar(cereal::make_nvp("chunk_table", m.chunk_table_));
    ar(cereal::make_nvp("block_size", m.block_size_));
    ar(cereal::make_nvp("total_fs_size", m.total_fs_size_));

    if (version >= 1) {
        ar(cereal::make_nvp("options", m.options_));
        ar(cereal::make_nvp("features", m.features_));
    }
}

template<class Archive>
void serialize(Archive& ar, Chunk& c) {
    ar(c.block_, c.offset_, c.size_);
}

template<class Archive>
void serialize(Archive& ar, Directory& d) {
    ar(d.parent_entry_, d.first_entry_, d.self_entry_);
}

} // namespace dwarfs::metadata::domain

CEREAL_CLASS_VERSION(dwarfs::metadata::domain::Metadata, 1);
```

---

## 5. High-Level API

### 5.1 Metadata Reader

```cpp
// include/dwarfs/metadata/reader.h
namespace dwarfs::metadata {

/**
 * @brief High-level metadata reader with auto-detection
 */
class MetadataReader {
public:
    /**
     * @brief Read metadata with format auto-detection
     */
    static std::unique_ptr<domain::Metadata> read(
        std::span<uint8_t const> data);

    /**
     * @brief Read metadata with explicit format
     */
    static std::unique_ptr<domain::Metadata> read(
        std::span<uint8_t const> data,
        serialization::SerializationFormat format);
};

} // namespace dwarfs::metadata
```

### 5.2 Metadata Writer

```cpp
// include/dwarfs/metadata/writer.h
namespace dwarfs::metadata {

/**
 * @brief High-level metadata writer
 */
class MetadataWriter {
public:
    /**
     * @brief Write metadata in default format (Cereal)
     */
    static std::vector<uint8_t> write(domain::Metadata const& metadata);

    /**
     * @brief Write metadata in specific format
     */
    static std::vector<uint8_t> write(
        domain::Metadata const& metadata,
        serialization::SerializationFormat format);
};

} // namespace dwarfs::metadata
```

---

## 6. Component Organization

### 6.1 Directory Structure

```
include/dwarfs/metadata/
├── domain/                     # Domain model (pure C++)
│   ├── metadata.h
│   ├── chunk.h
│   ├── directory.h
│   ├── inode_data.h
│   ├── filesystem_options.h
│   └── cereal_support.h       # Cereal templates
│
├── serialization/             # Serialization layer
│   ├── format.h               # Format enumeration
│   ├── serializer.h           # IMetadataSerializer
│   ├── format_detector.h
│   ├── serializer_factory.h
│   ├── thrift_serializer.h
│   └── cereal_serializer.h
│
├── reader.h                   # High-level reader
└── writer.h                   # High-level writer

src/metadata/
├── domain/
│   ├── metadata.cpp
│   ├── chunk.cpp
│   └── directory.cpp
│
└── serialization/
    ├── format_detector.cpp
    ├── serializer_factory.cpp
    ├── thrift_serializer.cpp
    ├── cereal_serializer.cpp
    ├── reader.cpp
    └── writer.cpp
```

### 6.2 Namespace Organization

```cpp
namespace dwarfs::metadata {
    // High-level API

    namespace domain {
        // Pure domain models
        // No dependencies on serialization
    }

    namespace serialization {
        // Serialization abstractions
        // Concrete implementations
    }
}
```

---

## 7. Backward Compatibility

### 7.1 Compatibility Strategy

```
┌─────────────────────────────────────────┐
│ Read existing .dwarfs file              │
└────────────────┬────────────────────────┘
                 │
                 ▼
┌─────────────────────────────────────────┐
│ FormatDetector::detect(data)            │
│ - Check for DWFC magic (Cereal)         │
│ - Check for Thrift patterns             │
└────────────────┬────────────────────────┘
                 │
        ┌────────┴────────┐
        │                 │
        ▼                 ▼
┌──────────────┐  ┌──────────────┐
│ Thrift       │  │ Cereal       │
│ (Legacy)     │  │ (New)        │
└──────┬───────┘  └──────┬───────┘
       │                 │
       └────────┬────────┘
                │
                ▼
┌─────────────────────────────────────────┐
│ domain::Metadata (unified)              │
└─────────────────────────────────────────┘
```

### 7.2 Migration Utility

```cpp
// include/dwarfs/metadata/migration.h
namespace dwarfs::metadata {

class MetadataMigrator {
public:
    /**
     * @brief Convert from legacy to new format
     */
    static std::vector<uint8_t> migrate_to_cereal(
        std::span<uint8_t const> thrift_data);

    /**
     * @brief Check if migration is needed
     */
    static bool needs_migration(std::span<uint8_t const> data);
};

} // namespace dwarfs::metadata
```

---

## 8. Implementation Phases

### Phase 1: Foundation (2 weeks)

**Tasks:**
- [ ] Create domain model classes
- [ ] Define serialization interfaces
- [ ] Implement format detector
- [ ] Unit tests for domain model

**Deliverables:**
- Complete domain model in `include/dwarfs/metadata/domain/`
- All interfaces in `include/dwarfs/metadata/serialization/`
- Unit tests passing

### Phase 2: Thrift Adapter (2 weeks)

**Tasks:**
- [ ] Implement `ThriftSerializer`
- [ ] Conversion logic: Thrift ↔ Domain model
- [ ] Integration with existing code
- [ ] Compatibility tests

**Deliverables:**
- Working ThriftSerializer
- All existing .dwarfs files readable
- No regressions in existing tests

### Phase 3: Cereal Implementation (2 weeks)

**Tasks:**
- [ ] Implement `CerealBinarySerializer`
- [ ] Add Cereal serialization templates
- [ ] Format detection for Cereal
- [ ] Performance benchmarks

**Deliverables:**
- Working CerealBinarySerializer
- Performance meets or exceeds Thrift
- Format auto-detection working

### Phase 4: Integration (2 weeks)

**Tasks:**
- [ ] High-level `MetadataReader`/`Writer`
- [ ] Update `metadata_v2.cpp`
- [ ] Update `metadata_builder.cpp`
- [ ] Comprehensive testing

**Deliverables:**
- Dual-format support complete
- All tests passing
- Documentation updated

### Phase 5: Folly Replacement (4 weeks)

**Tasks:**
- [ ] Audit all Folly usage
- [ ] Replace `folly::Synchronized` → `std::mutex`
- [ ] Replace `folly::F14Map` → `std::unordered_map`
- [ ] Replace other Folly utilities
- [ ] Remove Folly from build

**Deliverables:**
- Zero Folly dependencies
- Performance validated
- All tests passing

### Phase 6: Cleanup (2 weeks)

**Tasks:**
- [ ] Make Thrift optional
- [ ] Update documentation
- [ ] Create migration guide
- [ ] Release preparation

**Deliverables:**
- Optional Thrift support
- Updated README
- Migration guide for users

---

## 9. Testing Strategy

### 9.1 Unit Tests

```cpp
// Test serializer interface compliance
template<typename Serializer>
class SerializerTest : public ::testing::Test {
    // Test round-trip serialization
    // Test empty metadata
    // Test error handling
};

TYPED_TEST_SUITE(SerializerTest, Types<ThriftSerializer, CerealBinarySerializer>);
```

### 9.2 Integration Tests

```cpp
// Cross-format compatibility
TEST(MetadataCompat, ThriftReadCerealWrite) {
    auto legacy = load_legacy_dwarfs();
    auto metadata = MetadataReader::read(legacy);
    auto cereal = MetadataWriter::write(*metadata);
    auto metadata2 = MetadataReader::read(cereal);
    EXPECT_EQ(*metadata, *metadata2);
}
```

### 9.3 Performance Tests

- Serialize/deserialize benchmarks
- Memory usage comparison
- Large filesystem tests

---

## 10. Success Criteria

### Build System
- [x] Compiles without Thrift/Folly
- [x] Static library builds successfully
- [x] All tests pass

### Functionality
- [x] Reads all existing .dwarfs files
- [x] Writes new .dwarfs files correctly
- [x] No data corruption
- [x] Feature parity maintained

### Performance
- [x] Serialization within 10% of Thrift
- [x] No memory regressions
- [x] Mount time unchanged

---

## Appendix: Key Design Patterns

### Strategy Pattern
- `IMetadataSerializer` with multiple implementations
- Runtime selection of serialization format

### Factory Pattern
- `SerializerFactory` creates appropriate serializers
- Extensible through registration

### Adapter Pattern
- `ThriftSerializer` adapts legacy thrift code
- Converts between thrift types and domain model

### Template Method Pattern
- `MetadataReader` defines read algorithm
- Delegates format-specific details to strategies

---

## Conclusion

This architecture provides a robust, extensible foundation for removing Thrift/Folly dependencies while maintaining backward compatibility. The design follows SOLID principles, ensures clear separation of concerns, and supports future format evolution.

**Key Benefits:**
- Clean abstraction layers
- Easy to test and maintain
- Backward compatible
- Performance optimized
- Extensible for future formats