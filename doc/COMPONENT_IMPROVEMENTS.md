# DwarFS Component Improvement Designs

## Overview

This document provides detailed designs for specific component improvements identified in the architectural review. Each improvement includes rationale, design specifications, implementation guidance, and impact assessment.

---

## Table of Contents

1. [CMake Build System Refactoring](#1-cmake-build-system-refactoring)
2. [Configuration Management Framework](#2-configuration-management-framework)
3. [Serialization Registry Pattern](#3-serialization-registry-pattern)
4. [Platform Abstraction Layer](#4-platform-abstraction-layer)
5. [Build Profile System](#5-build-profile-system)
6. [Compression Configuration Enhancement](#6-compression-configuration-enhancement)

---

## 1. CMake Build System Refactoring

### Priority: HIGH
### Effort: 4-6 weeks
### Impact: MAJOR

### Current Issues
- 1412-line monolithic [`CMakeLists.txt`](../CMakeLists.txt)
- Hardcoded dependency versions
- Platform-specific paths embedded
- Complex conditional compilation logic
- Difficult to maintain and test

### Design Solution

#### 1.1 Modular CMake Structure

```
cmake/
├── config/
│   ├── dependencies.yaml        # Dependency specifications
│   ├── platforms.yaml           # Platform-specific settings
│   ├── profiles.yaml            # Build profile definitions
│   └── parse_config.py          # Configuration parser
│
├── modules/
│   ├── DwarfsCompression.cmake  # Compression subsystem
│   ├── DwarfsSerialization.cmake # Serialization subsystem
│   ├── DwarfsFUSE.cmake         # FUSE driver configuration
│   ├── DwarfsTools.cmake        # Tool binaries
│   └── DwarfsTests.cmake        # Testing framework
│
├── functions/
│   ├── load_configuration.cmake # Config loading utilities
│   ├── find_dependencies.cmake  # Dependency resolution
│   ├── setup_compiler.cmake     # Compiler configuration
│   └── create_targets.cmake     # Target creation helpers
│
└── (existing modules remain)
    ├── need_fmt.cmake
    ├── need_fuse.cmake
    └── ...
```

#### 1.2 Dependency Configuration File

**File:** `cmake/config/dependencies.yaml`

```yaml
# See CONFIGURATION_ARCHITECTURE.md for full specification
dependencies:
  required:
    boost:
      min_version: "1.67.0"
      find_package:
        components: [chrono, program_options]
        optional_components: [process]

  optional:
    brotli:
      min_version: "1.0.9"
      pkg_config: [libbrotlienc, libbrotlidec]
      cmake_flag: DWARFS_HAVE_LIBBROTLI
      try_option: TRY_ENABLE_BROTLI
```

#### 1.3 Refactored Main CMakeLists.txt

**Target Structure:**
```cmake
cmake_minimum_required(VERSION 3.28.0)
project(dwarfs LANGUAGES CXX C)

# Load configuration framework
include(cmake/functions/load_configuration.cmake)
dwarfs_load_configuration(cmake/config/dependencies.yaml)

# Setup compiler and platform
include(cmake/functions/setup_compiler.cmake)
dwarfs_setup_compiler()

# Find dependencies
include(cmake/functions/find_dependencies.cmake)
dwarfs_find_required_dependencies()
dwarfs_find_optional_dependencies()

# Configure subsystems
include(cmake/modules/DwarfsCompression.cmake)
include(cmake/modules/DwarfsSerialization.cmake)
include(cmake/modules/DwarfsFUSE.cmake)
include(cmake/modules/DwarfsTools.cmake)
include(cmake/modules/DwarfsTests.cmake)

# Create targets
include(cmake/functions/create_targets.cmake)
dwarfs_create_library_targets()
dwarfs_create_binary_targets()

# Installation
include(cmake/dwarfs_install.cmake)
```

**Expected Result:**
- Main CMakeLists.txt: ~200 lines (from 1412)
- Clear module boundaries
- Easier to understand and maintain
- Better testability

### Implementation Steps

1. **Week 1: Extract Dependency Configuration**
   - Create `cmake/config/dependencies.yaml`
   - Implement Python parser
   - Create CMake loading functions
   - Test with current build

2. **Week 2: Create Module Structure**
   - Extract compression configuration to `DwarfsCompression.cmake`
   - Extract serialization to `DwarfsSerialization.cmake`
   - Test module isolation

3. **Week 3: Platform Abstraction**
   - Create `cmake/config/platforms.yaml`
   - Implement platform detection
   - Extract platform-specific code

4. **Week 4: Refactor Main CMakeLists.txt**
   - Integrate all modules
   - Remove duplicated code
   - Comprehensive testing

5. **Week 5-6: Testing & Validation**
   - Test on all platforms (Windows, macOS, Linux)
   - Verify all build configurations
   - Update CI/CD workflows
   - Update documentation

### Testing Strategy

```bash
# Test matrix
for profile in minimal standard full developer; do
  for platform in linux macos windows; do
    for compiler in gcc clang msvc; do
      cmake -B build-$profile-$platform-$compiler \
            -DDWARFS_BUILD_PROFILE=$profile
      cmake --build build-$profile-$platform-$compiler
      ctest --test-dir build-$profile-$platform-$compiler
    done
  done
done
```

### Success Metrics
- ✓ CMakeLists.txt < 300 lines
- ✓ All existing builds still work
- ✓ No performance regression
- ✓ Easier to add new dependencies
- ✓ Clear separation of concerns

---

## 2. Configuration Management Framework

### Priority: HIGH
### Effort: 6-8 weeks
### Impact: MAJOR

### Design Solution

#### 2.1 Configuration Library Structure

```cpp
namespace dwarfs::config {

// Core configuration manager
class ConfigurationManager {
public:
  static ConfigurationManager& instance();

  void load(std::filesystem::path const& config_file);
  void validate();

  BuildConfig const& build() const;
  DependencyConfig const& dependencies() const;
  CompressionConfig const& compression() const;
  SerializationConfig const& serialization() const;
  FeatureConfig const& features() const;

private:
  ConfigurationManager() = default;
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

// Build configuration accessor
class BuildConfig {
public:
  int cxx_standard() const;
  std::string default_build_type() const;
  BuildProfile profile(std::string_view name) const;

private:
  friend class ConfigurationManager;
  std::shared_ptr<BuildConfigData> data_;
};

// Compression configuration accessor
class CompressionConfig {
public:
  struct AlgorithmInfo {
    std::string name;
    std::string description;
    compression_type type;
    bool enabled;
    std::vector<std::string> options;
    std::optional<std::string> build_flag;
  };

  std::vector<AlgorithmInfo> enabled_algorithms() const;
  AlgorithmInfo algorithm(std::string_view name) const;
  bool is_enabled(std::string_view name) const;

private:
  friend class ConfigurationManager;
  std::shared_ptr<CompressionConfigData> data_;
};

} // namespace dwarfs::config
```

#### 2.2 YAML Configuration Parser

**Implementation:** Using yaml-cpp library

```cpp
namespace dwarfs::config::detail {

class YAMLConfigParser {
public:
  explicit YAMLConfigParser(std::filesystem::path const& file);

  ConfigData parse();

private:
  void parse_build_section(YAML::Node const& node);
  void parse_dependencies_section(YAML::Node const& node);
  void parse_compression_section(YAML::Node const& node);
  void parse_serialization_section(YAML::Node const& node);
  void parse_features_section(YAML::Node const& node);

  void validate_schema();

  YAML::Node root_;
  ConfigData data_;
};

} // namespace dwarfs::config::detail
```

#### 2.3 Configuration Validation Framework

```cpp
namespace dwarfs::config {

class ValidationResult {
public:
  bool is_valid() const;
  std::vector<std::string> const& errors() const;
  std::vector<std::string> const& warnings() const;

  void add_error(std::string message);
  void add_warning(std::string message);

private:
  std::vector<std::string> errors_;
  std::vector<std::string> warnings_;
};

class ConfigValidator {
public:
  explicit ConfigValidator(ConfigData const& config);

  ValidationResult validate();

private:
  void check_mutex_rules();
  void check_dependency_rules();
  void check_platform_restrictions();
  void check_consistency_rules();

  ConfigData const& config_;
  ValidationResult result_;
};

} // namespace dwarfs::config
```

### Implementation Steps

1. **Week 1-2: Core Infrastructure**
   - Create config namespace and basic classes
   - Implement YAML parser
   - Add basic validation
   - Unit tests for parser

2. **Week 3-4: Configuration Accessors**
   - Implement BuildConfig class
   - Implement DependencyConfig class
   - Implement CompressionConfig class
   - Implement SerializationConfig class

3. **Week 5-6: Integration**
   - Integrate with compression registry
   - Integrate with serialization system
   - Update CMake to generate embedded config
   - Add runtime config file support

4. **Week 7-8: Testing & Documentation**
   - Comprehensive unit tests
   - Integration tests
   - Update all documentation
   - Migration guide

### Usage Example

```cpp
#include <dwarfs/config/configuration_manager.h>

int main() {
  using namespace dwarfs::config;

  // Initialize configuration
  auto& config = ConfigurationManager::instance();
  config.load("config/dwarfs-config.yaml");

  // Validate
  auto validation = config.validate();
  if (!validation.is_valid()) {
    for (auto const& error : validation.errors()) {
      std::cerr << "Config error: " << error << "\n";
    }
    return 1;
  }

  // Query configuration
  auto const& compression = config.compression();
  for (auto const& algo : compression.enabled_algorithms()) {
    std::cout << "Available: " << algo.name << "\n";
  }

  return 0;
}
```

---

## 3. Serialization Registry Pattern

### Priority: MEDIUM
### Effort: 3-4 weeks
### Impact: MODERATE

### Current State
- Config file exists but not fully utilized
- No runtime format registration
- Magic byte detection hardcoded
- Format capabilities not queryable

### Design Solution

#### 3.1 Serialization Registry Architecture

```cpp
namespace dwarfs::serialization {

// Format descriptor
struct FormatDescriptor {
  std::string name;
  std::string description;
  std::vector<uint8_t> magic_bytes;
  int priority;
  bool read_only;
  bool legacy;
  std::optional<std::string> build_flag;

  std::unique_ptr<Serializer> create_serializer() const;
};

// Registry pattern similar to compression
class SerializationRegistry {
public:
  static SerializationRegistry& instance();

  void register_format(FormatDescriptor descriptor);

  FormatDescriptor const* detect_format(
      std::span<uint8_t const> data) const;

  FormatDescriptor const* get_format(
      std::string_view name) const;

  FormatDescriptor const* default_format() const;

  std::vector<FormatDescriptor const*> available_formats() const;

private:
  SerializationRegistry();

  std::vector<FormatDescriptor> formats_;
  std::string default_format_name_;
};

// Format registration helper (similar to compression)
template<typename SerializerT>
struct FormatRegistrar {
  static std::unique_ptr<FormatDescriptor> reg();
};

} // namespace dwarfs::serialization
```

#### 3.2 Configuration-Driven Registration

```cpp
// During initialization, load from config
void initialize_serialization_from_config() {
  using namespace dwarfs::serialization;
  using namespace dwarfs::config;

  auto& config = ConfigurationManager::instance();
  auto& registry = SerializationRegistry::instance();

  auto const& serialization = config.serialization();

  for (auto const& format : serialization.formats()) {
    if (!format.enabled) continue;

    // Check build flag if required
    if (format.build_flag && !is_build_flag_set(*format.build_flag)) {
      continue;
    }

    // Register format
    FormatDescriptor desc{
      .name = format.name,
      .description = format.description,
      .magic_bytes = format.magic_bytes,
      .priority = format.priority,
      .read_only = format.read_only,
      .legacy = format.legacy,
      .build_flag = format.build_flag
    };

    registry.register_format(std::move(desc));
  }
}
```

### Implementation Steps

1. **Week 1: Registry Infrastructure**
   - Create SerializationRegistry class
   - Implement format registration
   - Add magic byte detection

2. **Week 2: Configuration Integration**
   - Load formats from config file
   - Implement format factories
   - Add validation

3. **Week 3: Serializer Integration**
   - Update CerealBinarySerializer integration
   - Update ThriftCompactSerializer integration
   - Test format detection

4. **Week 4: Testing & Documentation**
   - Unit tests for registry
   - Integration tests
   - Update documentation

---

## 4. Platform Abstraction Layer

### Priority: MEDIUM
### Effort: 2-3 weeks
### Impact: MODERATE

### Design Solution

#### 4.1 Platform Configuration Interface

```cpp
namespace dwarfs::platform {

enum class OS {
  Windows,
  MacOS,
  Linux,
  BSD,
  Unknown
};

enum class Architecture {
  X86_64,
  ARM64,
  X86,
  ARM,
  Unknown
};

class PlatformInfo {
public:
  static PlatformInfo const& instance();

  OS operating_system() const;
  Architecture architecture() const;
  std::string os_name() const;
  std::string arch_name() const;

  // Platform-specific path queries
  std::filesystem::path tool_search_path(
      std::string_view tool_name) const;

  std::vector<std::filesystem::path> library_search_paths() const;

  // Feature availability
  bool has_fuse() const;
  bool has_winfsp() const;
  bool supports_mmap() const;
  bool supports_sparse_files() const;

private:
  PlatformInfo();

  OS os_;
  Architecture arch_;
};

// Configuration-driven path resolution
class PathResolver {
public:
  explicit PathResolver(PlatformInfo const& platform);

  std::optional<std::filesystem::path> find_tool(
      std::string_view name) const;

  std::optional<std::filesystem::path> find_library(
      std::string_view name) const;

private:
  PlatformInfo const& platform_;
  std::vector<std::filesystem::path> search_paths_;
};

} // namespace dwarfs::platform
```

#### 4.2 CMake Platform Configuration

```cmake
# cmake/functions/detect_platform.cmake

function(dwarfs_detect_platform)
  # Detect OS
  if(WIN32)
    set(DWARFS_PLATFORM_OS "Windows" PARENT_SCOPE)
  elseif(APPLE)
    set(DWARFS_PLATFORM_OS "MacOS" PARENT_SCOPE)
  elseif(UNIX)
    set(DWARFS_PLATFORM_OS "Linux" PARENT_SCOPE)
  endif()

  # Load platform-specific configuration
  include(cmake/config/platforms/${DWARFS_PLATFORM_OS}.cmake OPTIONAL)

  # Set default paths if not configured
  dwarfs_set_platform_defaults()
endfunction()
```

### Implementation Steps

1. **Week 1: Platform Detection**
   - Create PlatformInfo class
   - Implement OS/arch detection
   - Add feature detection

2. **Week 2: Path Resolution**
   - Create PathResolver class
   - Implement configuration-driven search
   - Update CMake integration

3. **Week 3: Integration & Testing**
   - Update existing code to use platform layer
   - Test on all platforms
   - Documentation

---

## 5. Build Profile System

### Priority: MEDIUM
### Effort: 2 weeks
### Impact: MODERATE

### Design Solution

#### 5.1 Profile Definition

```yaml
# cmake/config/profiles.yaml

profiles:
  minimal:
    description: "Minimal build with core features only"
    features:
      compression: [null, zstd]
      fuse_driver: false
      tools: true
      tests: false

  developer:
    inherits: full  # Inherit from another profile
    description: "Development build with debugging"
    features:
      sanitizers:
        address: true
        undefined: true
      coverage: true
      dev_tools: true
```

#### 5.2 Profile Selection

```bash
# Command line
cmake -B build -DDWARFS_BUILD_PROFILE=developer

# Environment variable
export DWARFS_BUILD_PROFILE=minimal
cmake -B build

# CMake cache
cmake -B build -DDWARFS_BUILD_PROFILE=full
```

### Implementation Steps

1. **Week 1: Profile Infrastructure**
   - Create profile YAML schema
   - Implement profile parser
   - Add CMake integration

2. **Week 2: Testing & Documentation**
   - Test all profiles
   - Document profile system
   - Add profile selection guide

---

## 6. Compression Configuration Enhancement

### Priority: LOW
### Effort: 1-2 weeks
### Impact: MINOR

### Design Solution

#### 6.1 Enhanced Algorithm Metadata

```yaml
compression:
  algorithms:
    zstd:
      # Existing fields...

      # Performance characteristics
      performance:
        compression_speed: "very fast"
        decompression_speed: "very fast"
        compression_ratio: "excellent"
        memory_usage: "moderate"

      # Use case recommendations
      recommended_for:
        - "General purpose"
        - "Fast compression needed"
        - "Network transmission"

      # Benchmarking data (optional)
      benchmarks:
        compression_mbps: 500
        decompression_mbps: 1500
        typical_ratio: 2.5
```

#### 6.2 Algorithm Recommendation System

```cpp
namespace dwarfs::compression {

class AlgorithmRecommender {
public:
  struct UseCase {
    enum Priority { Speed, Ratio, Memory };
    Priority priority;
    size_t typical_size;
    std::optional<std::string> data_type;
  };

  std::vector<std::string> recommend(UseCase const& use_case) const;

private:
  CompressionConfig const& config_;
};

} // namespace dwarfs::compression
```

---

## Summary

All component improvements are designed to:
- ✓ Follow OOP principles
- ✓ Maintain MECE boundaries
- ✓ Enhance separation of concerns
- ✓ Support Open/Closed principle
- ✓ Enable configuration-driven architecture

See [`IMPROVEMENT_ROADMAP.md`](IMPROVEMENT_ROADMAP.md) for prioritized implementation plan.

---

**Document Version:** 1.0
**Date:** 2025-10-28
**Status:** DESIGN PROPOSAL