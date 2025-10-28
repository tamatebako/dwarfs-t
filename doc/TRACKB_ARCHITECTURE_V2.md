# Track B Architecture V2: Registry and Configuration System

## Overview

This document describes the enhanced configuration-driven serialization architecture for DwarFS metadata. Version 2 eliminates all hardcoding through a registry pattern and external YAML configuration.

## Design Principles

1. **Configuration > Hardcoding** - All metadata in external configuration files
2. **Registry Pattern** - Auto-discovery, no hardcoded lists
3. **Single Responsibility** - Each component serves one purpose
4. **Open/Closed Principle** - Extensible without modifying existing code
5. **Dependency Inversion** - High-level code depends on abstractions

## Architecture Components

### 1. Configuration Manager (`serialization_config.h`)

**Purpose**: Thread-safe singleton that loads and manages serialization configuration from YAML files.

**Key Features**:
- Loads format metadata from YAML
- Validates configuration on load
- Thread-safe access via mutex
- Type-safe configuration access

**Configuration File** (`config/serialization_config.yaml`):
```yaml
serialization:
  default_format: cereal_binary
  formats:
    cereal_binary:
      enabled: true
      name: "Cereal Binary"
      magic_bytes: [0xCE, 0xEA, 0x01]
      class: CerealBinarySerializer
      read_write: true
      priority: 100

    thrift_compact:
      enabled: true
      name: "Thrift Compact"
      magic_bytes: [0x82, 0x21]
      class: ThriftCompactSerializer
      read_only: true
      priority: 50
      requires_build_flag: DWARFS_LEGACY_THRIFT_SUPPORT
```

### 2. Serializer Registry (`serializer_registry.h`)

**Purpose**: Thread-safe singleton registry for serializer auto-discovery and creation.

**Key Features**:
- Stores serializer factory functions
- Provides format detection by magic bytes
- Priority-based format detection
- Thread-safe registration and lookup

**Public API**:
```cpp
class SerializerRegistry {
  // Register a serializer (called by auto-registration)
  void register_serializer(name, creator, magic_bytes, priority, format);

  // Create serializer instances
  std::unique_ptr<IMetadataSerializer> create_serializer(format);
  std::unique_ptr<IMetadataSerializer> create_serializer(name);

  // Format detection
  SerializationFormat detect_format(data);

  // Query available formats
  std::vector<std::string> get_available_formats();
  bool is_registered(format);
};
```

### 3. Auto-Registration Helper (`auto_register.h`)

**Purpose**: Template class for automatic serializer registration during static initialization.

**Usage**:
```cpp
// In cereal_binary_serializer.cpp:
namespace {
  SerializerAutoRegister<CerealBinarySerializer> registration(
      "cereal_binary",
      {0xCE, 0xEA, 0x01},  // Magic bytes
      100,                  // Priority
      SerializationFormat::CEREAL_BINARY
  );
}
```

**Convenience Macro**:
```cpp
REGISTER_SERIALIZER(CerealBinarySerializer,
                    "cereal_binary",
                    {0xCE, 0xEA, 0x01},
                    100,
                    SerializationFormat::CEREAL_BINARY);
```

### 4. Updated SerializerFactory

**Changes**:
- Removed hardcoded switch statements
- Delegates all creation to registry
- Simplified to thin wrapper around registry

**Before**:
```cpp
switch (format) {
  case CEREAL_BINARY:
    return std::make_unique<CerealBinarySerializer>();
  case THRIFT_COMPACT:
    #ifdef DWARFS_LEGACY_THRIFT_SUPPORT
    return std::make_unique<ThriftCompactSerializer>();
    #else
    throw std::runtime_error("Not available");
    #endif
}
```

**After**:
```cpp
return SerializerRegistry::instance().create_serializer(format);
```

### 5. Updated FormatDetector

**Changes**:
- Removed hardcoded magic byte checks
- Delegates detection to registry
- Configuration-driven priority ordering

**Before**:
```cpp
if (is_cereal_binary(data)) return CEREAL_BINARY;
if (is_thrift_compact(data)) return THRIFT_COMPACT;
throw std::runtime_error("Unknown format");
```

**After**:
```cpp
return SerializerRegistry::instance().detect_format(data);
```

## Architecture Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                    Application Layer                        │
│  ┌──────────────────┐      ┌──────────────────┐            │
│  │ SerializerFactory│      │  FormatDetector  │            │
│  └────────┬─────────┘      └────────┬─────────┘            │
│           │                         │                       │
│           └────────────┬────────────┘                       │
│                        │                                    │
└────────────────────────┼────────────────────────────────────┘
                         │
┌────────────────────────┼────────────────────────────────────┐
│              Registry & Configuration Layer                 │
│                        ▼                                    │
│  ┌──────────────────────────────────────┐                  │
│  │      SerializerRegistry              │                  │
│  │  ┌────────────────────────────────┐  │                  │
│  │  │ Map<Format, Creator Function>  │  │                  │
│  │  └────────────────────────────────┘  │                  │
│  │  ┌────────────────────────────────┐  │                  │
│  │  │ Magic Bytes → Format           │  │                  │
│  │  └────────────────────────────────┘  │                  │
│  └──────────────────────────────────────┘                  │
│                        ▲                                    │
│                        │                                    │
│  ┌─────────────────────┴─────────┐                         │
│  │   SerializationConfig         │                         │
│  │  (loads config/serialization  │                         │
│  │   _config.yaml)                │                         │
│  └───────────────────────────────┘                         │
└─────────────────────────────────────────────────────────────┘
                         ▲
┌────────────────────────┼────────────────────────────────────┐
│               Serializer Implementations                    │
│                        │                                    │
│  ┌────────────────────────────┐                            │
│  │ CerealBinarySerializer     │                            │
│  │  + Auto-registration       │                            │
│  │    (via static init)       │                            │
│  └────────────────────────────┘                            │
│                                                             │
│  ┌────────────────────────────┐                            │
│  │ ThriftCompactSerializer    │                            │
│  │  + Auto-registration       │                            │
│  │    (via static init)       │                            │
│  └────────────────────────────┘                            │
└─────────────────────────────────────────────────────────────┘
```

## Data Flow

### Serialization (Write)
1. User calls `SerializerFactory::create_default()`
2. Factory delegates to `SerializerRegistry::create_serializer()`
3. Registry looks up format in registered serializers
4. Returns appropriate serializer instance
5. User calls `serializer->serialize(metadata)`

### Deserialization (Read)
1. User calls `FormatDetector::detect_format(data)`
2. Detector delegates to `SerializerRegistry::detect_format()`
3. Registry checks magic bytes against all registered formats (by priority)
4. Returns detected format
5. User calls `SerializerFactory::create(format)`
6. Registry creates appropriate serializer
7. User calls `serializer->deserialize(data)`

## Benefits

### 1. No Hardcoding
- All format metadata in external YAML
- Magic bytes configurable
- Priority ordering configurable
- Easy to add new formats without code changes

### 2. Auto-Discovery
- Serializers self-register at startup
- No manual registration needed
- Registry discovers all available formats automatically

### 3. Extensibility
- Adding new format: Create serializer + add YAML config
- No modifications to factory or detector
- Follows Open/Closed Principle

### 4. Thread Safety
- All registry operations protected by mutex
- Safe for concurrent access
- Singleton ensures single source of truth

### 5. Type Safety
- Compile-time type checking via templates
- No runtime type errors
- Clear factory function signatures

## Adding a New Serializer

### Step 1: Create Serializer Class
```cpp
// include/dwarfs/metadata/serialization/my_serializer.h
class MySerializer : public IMetadataSerializer {
public:
  std::vector<uint8_t> serialize(const domain::metadata&) const override;
  std::unique_ptr<domain::metadata> deserialize(const std::vector<uint8_t>&) const override;
  // ...
};
```

### Step 2: Create Registration File
```cpp
// src/metadata/serialization/my_serializer.cpp
#include "dwarfs/metadata/serialization/auto_register.h"
#include "dwarfs/metadata/serialization/my_serializer.h"

namespace {
  SerializerAutoRegister<MySerializer> registration(
      "my_format",
      {0xMY, 0xMG, 0x01},
      75,  // Priority
      SerializationFormat::MY_FORMAT
  );
}
```

### Step 3: Add to Configuration
```yaml
# config/serialization_config.yaml
serialization:
  formats:
    my_format:
      enabled: true
      name: "My Format"
      magic_bytes: [0xMY, 0xMG, 0x01]
      class: MySerializer
      read_write: true
      priority: 75
```

### Step 4: Add to Build System
```cmake
# cmake/libdwarfs.cmake
add_library(dwarfs_common
  ...
  src/metadata/serialization/my_serializer.cpp
)
```

That's it! No changes needed to factory, detector, or any other code.

## Migration Path

The architecture is backwards compatible:

1. **Existing Code**: Works unchanged
2. **Gradual Migration**: Can migrate to config-driven approach incrementally
3. **No Breaking Changes**: All existing APIs remain functional

## Future Enhancements

1. **Runtime Configuration Reload**: Support hot-reloading configuration
2. **Plugin Architecture**: Load serializers from shared libraries
3. **Format Versioning**: Support multiple versions of same format
4. **Custom Factories**: Allow user-defined factory functions
5. **Format Negotiation**: Automatic format selection based on capabilities

## Implementation Files

- `config/serialization_config.yaml` - Configuration file
- `include/dwarfs/metadata/config/serialization_config.h` - Config manager
- `include/dwarfs/metadata/serialization/serializer_registry.h` - Registry
- `include/dwarfs/metadata/serialization/auto_register.h` - Auto-registration
- `src/metadata/serialization/cereal_binary_serializer.cpp` - Cereal registration
- `src/metadata/serialization/thrift_compact_serializer.cpp` - Thrift registration
- `include/dwarfs/metadata/serialization/serializer_factory.h` - Updated factory
- `include/dwarfs/metadata/serialization/format_detector.h` - Updated detector

## Testing

Key test scenarios:

1. **Auto-Registration**: Verify serializers register at startup
2. **Format Detection**: Verify correct format detection by magic bytes
3. **Priority Ordering**: Verify higher priority formats checked first
4. **Factory Creation**: Verify correct serializer instances created
5. **Thread Safety**: Verify concurrent access to registry
6. **Configuration Loading**: Verify YAML parsing and validation
7. **Error Handling**: Verify appropriate errors for unknown formats

## Conclusion

Track B Architecture V2 provides a fully configuration-driven, extensible serialization system that eliminates hardcoding and supports easy addition of new formats without modifying core code.