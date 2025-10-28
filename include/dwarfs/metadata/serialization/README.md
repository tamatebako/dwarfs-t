# DwarFS Metadata Serialization Abstraction Layer

This directory contains the serialization abstraction layer for DwarFS metadata, implementing Phase 3 of Track B (Thrift/Folly Removal).

## Overview

The serialization layer provides a clean, extensible interface for serializing and deserializing metadata in multiple formats, following SOLID principles and design patterns.

## Design Principles

### Open/Closed Principle
- **Open for extension**: New serialization formats can be added without modifying existing code
- **Closed for modification**: Adding support for new formats doesn't require changes to the abstraction layer

### Dependency Inversion Principle
- High-level code depends on [`IMetadataSerializer`](metadata_serializer.h) abstraction
- Concrete implementations (Cereal, Thrift) depend on the same abstraction
- No direct coupling to specific serialization libraries

### Strategy Pattern
- Different serializers are interchangeable strategies
- Client code can switch formats at runtime
- Each serializer handles exactly ONE format (Single Responsibility)

### Abstract Factory Pattern
- [`SerializerFactory`](serializer_factory.h) encapsulates creation logic
- Returns serializers through the interface, not concrete types
- Enables format-based serializer selection

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    Client Code                              │
│                (MetadataReader/Writer)                      │
└────────────────────┬────────────────────────────────────────┘
                     │ depends on
                     ▼
┌─────────────────────────────────────────────────────────────┐
│              IMetadataSerializer (Interface)                │
│  ┌──────────────────────────────────────────────────────┐  │
│  │ + serialize(metadata) → bytes                        │  │
│  │ + deserialize(bytes) → metadata                      │  │
│  │ + get_format_name() → string                         │  │
│  │ + get_format() → SerializationFormat                 │  │
│  └──────────────────────────────────────────────────────┘  │
└────────────┬────────────────────────────┬───────────────────┘
             │                            │
    implements                       implements
             │                            │
             ▼                            ▼
┌────────────────────────┐  ┌─────────────────────────────┐
│ CerealBinarySerializer │  │ ThriftCompactSerializer     │
│  (New Format)          │  │ (Legacy - Phase 4)          │
└────────────────────────┘  └─────────────────────────────┘
```

## Components

### 1. Serialization Format ([`serialization_format.h`](serialization_format.h))

Defines available serialization formats and magic byte constants:

```cpp
enum class SerializationFormat {
  THRIFT_COMPACT,  // Legacy Apache Thrift format (0x82 0x21)
  CEREAL_BINARY,   // New Cereal format (0xCE 0xEA 0x01)
  AUTO_DETECT      // Automatic format detection
};
```

**Magic Bytes:**
- **Cereal Binary**: `0xCE 0xEA 0x01` (custom header)
- **Thrift Compact**: `0x82 0x21` (Thrift protocol header)

### 2. Serializer Interface ([`metadata_serializer.h`](metadata_serializer.h))

Pure virtual interface defining the serialization contract:

```cpp
class IMetadataSerializer {
public:
  virtual std::vector<uint8_t> serialize(const metadata&) const = 0;
  virtual std::unique_ptr<metadata> deserialize(const std::vector<uint8_t>&) const = 0;
  virtual std::string_view get_format_name() const noexcept = 0;
  virtual SerializationFormat get_format() const noexcept = 0;
};
```

**Key Features:**
- Pure virtual interface (Strategy pattern)
- Smart pointer usage for memory safety
- Exception-based error handling
- Format identification methods

### 3. Format Detector ([`format_detector.h`](format_detector.h))

Detects serialization format from magic bytes:

```cpp
class FormatDetector {
public:
  static SerializationFormat detect_format(const std::vector<uint8_t>& data);
  static bool is_cereal_binary(const std::vector<uint8_t>& data);
  static bool is_thrift_compact(const std::vector<uint8_t>& data);
  static std::string get_format_info(const std::vector<uint8_t>& data);
};
```

**Detection Strategy:**
1. Check minimum data size (3 bytes)
2. Check for Cereal magic bytes (priority: new format first)
3. Check for Thrift magic bytes (legacy fallback)
4. Throw exception if unrecognized

### 4. Serializer Factory ([`serializer_factory.h`](serializer_factory.h))

Creates serializer instances (Abstract Factory pattern):

```cpp
class SerializerFactory {
public:
  static std::unique_ptr<IMetadataSerializer> create(SerializationFormat);
  static std::unique_ptr<IMetadataSerializer> create_from_data(const std::vector<uint8_t>&);
  static std::unique_ptr<IMetadataSerializer> create_default();
  static bool is_supported(SerializationFormat) noexcept;
};
```

**Factory Methods:**
- `create()`: Create by explicit format
- `create_from_data()`: Create by detecting format
- `create_default()`: Create with recommended format (Cereal)
- `is_supported()`: Check format availability

### 5. Cereal Binary Serializer ([`cereal_binary_serializer.h`](cereal_binary_serializer.h))

Concrete implementation using Cereal library:

```cpp
class CerealBinarySerializer : public IMetadataSerializer {
public:
  std::vector<uint8_t> serialize(const metadata& meta) const override;
  std::unique_ptr<metadata> deserialize(const std::vector<uint8_t>& data) const override;
  std::string_view get_format_name() const noexcept override;
  SerializationFormat get_format() const noexcept override;
};
```

**Format Structure:**
1. Magic bytes (3 bytes): `0xCE 0xEA 0x01`
2. Cereal binary archive data (variable length)

**Features:**
- Header-only for template instantiation
- Complete implementation (not just declaration)
- Exception-safe error handling
- Magic byte validation on deserialization

## High-Level APIs

### MetadataReader ([`../reader.h`](../reader.h))

Simplified API for reading metadata:

```cpp
// Auto-detect format
MetadataReader reader;
auto meta = reader.read(data);

// Explicit format
MetadataReader cereal_reader(SerializationFormat::CEREAL_BINARY);
auto meta2 = cereal_reader.read(cereal_data);

// Format detection
auto format = reader.detect_format(data);
auto info = reader.get_format_info(data);
```

### MetadataWriter ([`../writer.h`](../writer.h))

Simplified API for writing metadata:

```cpp
// Default format (Cereal Binary)
MetadataWriter writer;
auto data = writer.write(meta);

// Explicit format
MetadataWriter cereal_writer(SerializationFormat::CEREAL_BINARY);
auto data2 = cereal_writer.write(meta);
```

## Usage Examples

### Basic Serialization

```cpp
#include <dwarfs/metadata/writer.h>
#include <dwarfs/metadata/reader.h>

using namespace dwarfs::metadata;

// Create metadata
domain::metadata meta;
meta.block_size = 131072;
meta.total_fs_size = 1024 * 1024 * 1024;

// Write to binary
MetadataWriter writer;
std::vector<uint8_t> data = writer.write(meta);

// Read from binary
MetadataReader reader;
auto meta2 = reader.read(data);

assert(meta2->block_size == meta.block_size);
```

### Format Detection

```cpp
#include <dwarfs/metadata/reader.h>

// Read file with unknown format
std::vector<uint8_t> data = read_file("metadata.bin");

MetadataReader reader;
auto format = reader.detect_format(data);

std::cout << "Detected: " << reader.get_format_info(data) << "\n";
// Output: "Detected: Cereal Binary (version 1)"

auto meta = reader.read(data);
```

### Using the Factory

```cpp
#include <dwarfs/metadata/serialization/serializer_factory.h>

using namespace dwarfs::metadata::serialization;

// Create by format
auto cereal = SerializerFactory::create(SerializationFormat::CEREAL_BINARY);
auto data = cereal->serialize(meta);

// Create by detection
auto detected = SerializerFactory::create_from_data(data);
auto meta2 = detected->deserialize(data);

// Check support
if (SerializerFactory::is_supported(SerializationFormat::CEREAL_BINARY)) {
  std::cout << "Cereal format is available\n";
}
```

## Migration Path

### Phase 3 (Current): Cereal Implementation
- ✅ Serialization abstraction layer
- ✅ Cereal binary serializer
- ✅ Format detection
- ✅ High-level reader/writer APIs

### Phase 4 (Next): Thrift Adapter
- ⏳ `ThriftCompactSerializer` implementation
- ⏳ Adapter between Thrift and domain models
- ⏳ Legacy format support for backward compatibility

### Phase 5 (Future): Migration
- ⏳ Update existing code to use new APIs
- ⏳ Gradual replacement of Thrift calls
- ⏳ Eventually deprecate Thrift format

## Benefits

### Extensibility
- Add new formats without modifying existing code
- Each format is independent and self-contained
- Factory pattern centralizes creation logic

### Maintainability
- Clear separation of concerns
- Single Responsibility: each class has one job
- Well-documented interfaces and implementations

### Testability
- Interface-based design enables mocking
- Format detection is independent and testable
- Each serializer can be tested in isolation

### Backward Compatibility
- Supports multiple formats simultaneously
- Auto-detection ensures old files can be read
- Gradual migration path for existing code

## Error Handling

All classes use exception-based error handling:

```cpp
try {
  MetadataReader reader;
  auto meta = reader.read(data);
} catch (const std::invalid_argument& e) {
  // Invalid data or unsupported format
  std::cerr << "Invalid data: " << e.what() << "\n";
} catch (const std::runtime_error& e) {
  // Serialization/deserialization failed
  std::cerr << "Failed to read: " << e.what() << "\n";
}
```

## Thread Safety

- All classes are stateless and thread-safe for reads
- Serializers can be used concurrently from multiple threads
- Factory methods are static and thread-safe
- Format detection is thread-safe

## Performance Considerations

- **Header-only design**: Enables compiler optimizations
- **Smart pointers**: Minimal overhead with move semantics
- **Cereal format**: Efficient binary serialization
- **Magic bytes**: Fast format detection (3-byte comparison)

## Next Steps

1. **Phase 4**: Implement `ThriftCompactSerializer`
2. **Integration**: Use in existing codebase
3. **Testing**: Comprehensive unit and integration tests
4. **Migration**: Gradually replace Thrift usage
5. **Deprecation**: Eventually remove Thrift dependency

## References

- [Domain Models README](../domain/README.md)
- [Cereal Documentation](https://uscilab.github.io/cereal/)
- [Track B Architecture](../../../../TRACKB_ARCHITECTURE.md)
- [Apache Thrift](https://thrift.apache.org/)