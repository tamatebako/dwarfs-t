# DwarFS Configuration Architecture Design

## Overview

This document defines a comprehensive configuration architecture for DwarFS that moves the project toward a fully configuration-driven design. This architecture eliminates hardcoding, provides flexibility for different deployment scenarios, and maintains backward compatibility.

---

## Design Principles

1. **Configuration Over Hardcoding**: All configurable aspects externalized
2. **Single Source of Truth**: Each value defined in exactly one place
3. **Layered Configuration**: Support for defaults, environment-specific, and runtime overrides
4. **Type Safety**: Strong typing with validation
5. **Backward Compatibility**: Existing behavior preserved by default
6. **Build-Time + Runtime**: Configuration available at both stages

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                   DwarFS Configuration System                   │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌────────────────┐      ┌────────────────┐                    │
│  │ YAML Config    │──────│ Schema         │                    │
│  │ Files          │      │ Validator      │                    │
│  └────────────────┘      └────────────────┘                    │
│         │                        │                              │
│         ▼                        ▼                              │
│  ┌──────────────────────────────────────────┐                  │
│  │     Configuration Loader (C++)           │                  │
│  │  - Parse YAML                            │                  │
│  │  - Validate against schema               │                  │
│  │  - Merge layers (default + env + user)   │                  │
│  │  - Provide type-safe access              │                  │
│  └──────────────────────────────────────────┘                  │
│         │                        │                              │
│         ▼                        ▼                              │
│  ┌─────────────┐        ┌──────────────────┐                  │
│  │ Build-Time  │        │ Runtime          │                  │
│  │ Config Gen  │        │ Config Access    │                  │
│  │ (CMake)     │        │ (Library API)    │                  │
│  └─────────────┘        └──────────────────┘                  │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## Configuration File Structure

### Master Configuration: `config/dwarfs-config.yaml`

```yaml
# DwarFS Master Configuration
# This file defines all configurable aspects of the DwarFS system

metadata:
  version: "1.0"
  schema_version: "1.0"
  description: "DwarFS configuration schema"

# ============================================================================
# BUILD CONFIGURATION
# ============================================================================
build:
  # C++ Standard
  cxx_standard: 20

  # Default build type
  default_build_type: "Release"

  # Build profiles (quick selection of common configurations)
  profiles:
    minimal:
      description: "Minimal build with only essential features"
      features:
        compression: [null, zstd]
        fuse_driver: false
        tools: true
        tests: false

    standard:
      description: "Standard build with common features"
      features:
        compression: [null, zstd, lz4, lzma]
        fuse_driver: true
        tools: true
        tests: false

    full:
      description: "Full build with all optional features"
      features:
        compression: [null, brotli, flac, lz4, lzma, ricepp, zstd]
        fuse_driver: true
        tools: true
        benchmarks: true
        tests: true

    developer:
      description: "Development build with all features and sanitizers"
      features:
        compression: [null, brotli, flac, lz4, lzma, ricepp, zstd]
        fuse_driver: true
        tools: true
        benchmarks: true
        tests: true
        dev_tools: true
      sanitizers:
        address: true
        thread: false
        undefined: true
      coverage: true

# ============================================================================
# DEPENDENCY CONFIGURATION
# ============================================================================
dependencies:
  # Required dependencies (must be present)
  required:
    boost:
      min_version: "1.67.0"
      components:
        - chrono
        - program_options
      optional_components:
        - process

    libcrypto:
      min_version: "3.0.0"
      pkg_config: "libcrypto"

    libarchive:
      min_version: "3.6.0"
      pkg_config: "libarchive"

    xxhash:
      min_version: "0.8.1"
      pkg_config: "libxxhash"

    zstd:
      min_version: "1.4.8"
      pkg_config: "libzstd"

    yaml-cpp:
      min_version: "0.6.0"
      pkg_config: "yaml-cpp"

  # Optional dependencies (enabled if available and requested)
  optional:
    brotli:
      min_version: "1.0.9"
      pkg_config:
        - libbrotlienc
        - libbrotlidec
      cmake_flag: "DWARFS_HAVE_LIBBROTLI"
      cmake_option: "TRY_ENABLE_BROTLI"
      default: true
      description: "Brotli compression support"

    lz4:
      min_version: "1.9.3"
      pkg_config: "liblz4"
      cmake_flag: "DWARFS_HAVE_LIBLZ4"
      cmake_option: "TRY_ENABLE_LZ4"
      default: true
      description: "LZ4 compression support"

    lzma:
      min_version: "5.2.5"
      pkg_config: "liblzma"
      cmake_flag: "DWARFS_HAVE_LIBLZMA"
      cmake_option: "TRY_ENABLE_LZMA"
      default: true
      description: "LZMA compression support"

    flac:
      min_version: "1.4.2"
      pkg_config: "flac++"
      cmake_flag: "DWARFS_HAVE_FLAC"
      cmake_option: "TRY_ENABLE_FLAC"
      default: true
      description: "FLAC compression support"

    jemalloc:
      min_version: "5.2.1"
      pkg_config: "jemalloc"
      cmake_flag: "JEMALLOC_FOUND"
      cmake_option: "USE_JEMALLOC"
      default: true
      description: "jemalloc memory allocator"
      conflicts: [mimalloc]

    mimalloc:
      min_version: "2.0.0"
      find_package: true
      cmake_flag: "mimalloc_FOUND"
      cmake_option: "USE_MIMALLOC"
      default: false
      description: "mimalloc memory allocator"
      conflicts: [jemalloc]

  # Header-only libraries (fetch if needed)
  header_only:
    fmt:
      required_version: "10.0"
      preferred_version: "12.0.0"
      git_repo: "https://github.com/fmtlib/fmt.git"
      local_var: "DWARFS_LOCAL_REPO_PATH"

    googletest:
      required_version: "1.13.0"
      preferred_version: "1.17.0"
      git_repo: "https://github.com/google/googletest.git"
      required_for: [tests]

    range-v3:
      required_version: "0.12.0"
      preferred_version: "0.12.0"
      git_repo: "https://github.com/ericniebler/range-v3.git"

    parallel-hashmap:
      required_version: "1.3.8"
      preferred_version: "2.0.0"
      git_repo: "https://github.com/greg7mdp/parallel-hashmap.git"

# ============================================================================
# PLATFORM CONFIGURATION
# ============================================================================
platform:
  # Platform-specific paths and settings
  windows:
    winfsp:
      default_path: "$ENV{PROGRAMFILES(x86)}/WinFsp"
      search_paths:
        - "C:/Program Files (x86)/WinFsp"
        - "C:/Program Files/WinFsp"

    tool_paths:
      ccache: ["c:/bin"]
      upx: ["c:/bin"]

  macos:
    homebrew:
      prefix: "/opt/homebrew"
      use_for_libarchive: true

  linux:
    # Linux-specific settings
    use_mold_linker: true  # if available
    prefer_system_libs: true

# ============================================================================
# COMPRESSION CONFIGURATION
# ============================================================================
compression:
  # Registry of available compression algorithms
  algorithms:
    null:
      enabled: true
      type: NONE
      description: "No compression (passthrough)"
      library_dependencies: []
      memory_efficient: true
      build_flag: null  # Always available

    brotli:
      enabled: true
      type: BROTLI
      description: "Brotli compression"
      library_dependencies:
        - name: "brotli"
          version_function: "BrotliEncoderVersion"
          version_format: "hex"
      requires_dependency: brotli
      build_flag: "DWARFS_HAVE_LIBBROTLI"

      # Algorithm-specific options
      options:
        - "level=<compression-level>"
        - "quality=<1-11>"
        - "window=<10-24>"

      # Memory usage table (for optimization)
      memory_usage_table: true

    zstd:
      enabled: true
      type: ZSTD
      description: "Zstandard compression"
      library_dependencies:
        - name: "zstd"
          version_function: "ZSTD_versionString"
      requires_dependency: zstd
      build_flag: "DWARFS_HAVE_LIBZSTD"

      options:
        - "level=<compression-level>"

      memory_usage_table: true

    lz4:
      enabled: true
      type: LZ4
      description: "LZ4 fast compression"
      library_dependencies:
        - name: "lz4"
          version_function: "LZ4_versionString"
      requires_dependency: lz4
      build_flag: "DWARFS_HAVE_LIBLZ4"

      variants:
        - name: "lz4"
          type: LZ4
          description: "LZ4 fast compression"
        - name: "lz4hc"
          type: LZ4HC
          description: "LZ4 high compression"
          options:
            - "level=<1-12>"

    lzma:
      enabled: true
      type: LZMA
      description: "LZMA compression"
      library_dependencies:
        - name: "lzma"
          version_function: "lzma_version_string"
      requires_dependency: lzma
      build_flag: "DWARFS_HAVE_LIBLZMA"

      options:
        - "level=<0-9>"
        - "extreme"
        - "binary=<auto|x86|powerpc|ia64|arm|armthumb|sparc>"
        - "dict_size=<size>"

    flac:
      enabled: true
      type: FLAC
      description: "FLAC audio compression"
      library_dependencies:
        - name: "FLAC++"
          version_function: "FLAC__VERSION_STRING"
      requires_dependency: flac
      build_flag: "DWARFS_HAVE_FLAC"

      # Requires specific metadata
      metadata_requirements:
        sample_rate: required
        bits_per_sample: required
        num_channels: required

    ricepp:
      enabled: true
      type: RICEPP
      description: "Rice++ compression (FITS/astronomical data)"
      library_dependencies: []
      build_flag: "DWARFS_HAVE_RICEPP"
      cmake_option: "ENABLE_RICEPP"

      # Builtin library (no external dependency)
      builtin: true
      source_dir: "ricepp/"

# ============================================================================
# SERIALIZATION CONFIGURATION
# ============================================================================
serialization:
  # Default format for new filesystems
  default_format: cereal_binary

  # Registry of serialization formats
  formats:
    cereal_binary:
      enabled: true
      priority: 100
      name: "Cereal Binary"
      description: "Modern binary serialization using Cereal library"

      # Format identification
      magic_bytes: [0xCE, 0xEA, 0x01]
      version: 1

      # Implementation
      implementation_class: "CerealBinarySerializer"
      header_only: true

      # Capabilities
      read_write: true
      features:
        - "Fast serialization"
        - "Compact binary format"
        - "Version tolerant"

    thrift_compact:
      enabled: true
      priority: 50
      name: "Thrift Compact"
      description: "Legacy Apache Thrift format (backward compatibility)"

      # Format identification
      magic_bytes: [0x82, 0x21]

      # Implementation
      implementation_class: "ThriftCompactSerializer"

      # Capabilities
      read_only: true
      legacy: true

      # Build requirements
      requires_build_flag: "DWARFS_LEGACY_THRIFT_SUPPORT"
      cmake_option: "DWARFS_LEGACY_THRIFT_SUPPORT"
      default_enabled: true

      library_dependencies:
        - "FBThrift"
        - "Folly"

      features:
        - "Read old .dwarfs files"
        - "No new files in this format"

# ============================================================================
# FEATURE FLAGS
# ============================================================================
features:
  # Core features
  libdwarfs:
    description: "Build libdwarfs library"
    cmake_option: "WITH_LIBDWARFS"
    default: true

  tools:
    description: "Build command-line tools"
    cmake_option: "WITH_TOOLS"
    default: true
    requires: [libdwarfs]

  fuse_driver:
    description: "Build FUSE driver"
    cmake_option: "WITH_FUSE_DRIVER"
    default: true
    requires: [libdwarfs]
    platform_specific:
      windows: "WinFSP"
      macos: "FUSE for macOS"
      linux: "FUSE3 or FUSE2"

  # Optional features
  tests:
    description: "Build test suite"
    cmake_option: "WITH_TESTS"
    default: false
    requires: [googletest]

  benchmarks:
    description: "Build benchmarks"
    cmake_option: "WITH_BENCHMARKS"
    default: false
    requires: [benchmark]

  man_pages:
    description: "Build man pages using ronn"
    cmake_option: "WITH_MAN_PAGES"
    default: true
    platform: [linux, macos]

  man_option:
    description: "Enable --man option in tools"
    cmake_option: "WITH_MAN_OPTION"
    default: true

  desktop_integration:
    description: "Install desktop integration files"
    cmake_option: "WITH_DESKTOP_INTEGRATION"
    default: true
    platform: [linux, macos]

  universal_binary:
    description: "Build universal dwarfs binary"
    cmake_option: "WITH_UNIVERSAL_BINARY"
    default: false

  # Developer features
  dev_tools:
    description: "Build developer tools"
    cmake_option: "WITH_DEV_TOOLS"
    default: false

  perfmon:
    description: "Enable performance monitoring"
    cmake_option: "ENABLE_PERFMON"
    default: true

  stacktrace:
    description: "Enable stack traces"
    cmake_option: "ENABLE_STACKTRACE"
    default: false
    requires: [cpptrace]

  # Debugging/analysis features
  sanitizers:
    address:
      cmake_option: "ENABLE_ASAN"
      default: false
      conflicts: [thread, memory]

    thread:
      cmake_option: "ENABLE_TSAN"
      default: false
      conflicts: [address, memory]

    undefined:
      cmake_option: "ENABLE_UBSAN"
      default: false

    memory:
      cmake_option: "ENABLE_MSAN"
      default: false
      conflicts: [address, thread]

  coverage:
    cmake_option: "ENABLE_COVERAGE"
    default: false

# ============================================================================
# COMPILER CONFIGURATION
# ============================================================================
compiler:
  # Default flags for different build types
  flags:
    common:
      gnu_clang:
        - "-Wall"
        - "-Wextra"
        - "-pedantic"

      gnu_only:
        - "-Wno-stringop-overflow"

      clang_only:
        - "-Wnull-dereference"

      msvc:
        - "/W4"
        - "/w14254"
        - "/w14263"

    cxx_specific:
      gnu_clang:
        - "-Wold-style-cast"
        - "-Wnon-virtual-dtor"
        - "-Wsuggest-override"
        - "-Wpessimizing-move"

  # Build type specific settings
  build_types:
    Release:
      optimization: "-O3"
      defines: [NDEBUG]

    Debug:
      optimization: "-O0"
      defines: [DEBUG]
      flags: ["-g"]

    RelWithDebInfo:
      optimization: "-O2"
      defines: [NDEBUG]
      flags: ["-g"]

    MinSizeRel:
      optimization: "-Os"
      defines: [NDEBUG]

# ============================================================================
# INSTALLATION CONFIGURATION
# ============================================================================
installation:
  # Installation paths (can be overridden)
  paths:
    bash_completion: "bash-completion/completions"
    zsh_completion: "zsh/site-functions"

  # What to install in each configuration
  components:
    runtime:
      - binaries
      - libraries

    development:
      - headers
      - cmake_config
      - pkg_config

    documentation:
      - man_pages
      - readme
      - license

    desktop:
      - mime_types
      - desktop_files

# ============================================================================
# VALIDATION RULES
# ============================================================================
validation:
  # Configuration validation rules
  rules:
    # Mutual exclusion
    mutex:
      - [jemalloc, mimalloc]
      - [sanitizers.address, sanitizers.thread]
      - [sanitizers.address, sanitizers.memory]
      - [sanitizers.thread, sanitizers.memory]

    # Dependencies
    requires:
      tools: [libdwarfs]
      fuse_driver: [libdwarfs]
      tests: [libdwarfs]
      benchmarks: [libdwarfs]

    # Platform restrictions
    platform_only:
      man_pages: [linux, macos]
      desktop_integration: [linux, macos]

    # Build flags consistency
    consistency:
      - name: "compression_algorithm_available"
        check: "If compression.algorithms.X.enabled, then dependencies.optional.X must be satisfied"
```

---

## Implementation Components

### 1. Configuration Loader Library

**Location:** `include/dwarfs/config/` and `src/config/`

**Key Classes:**

#### `ConfigurationManager`
```cpp
namespace dwarfs::config {

class ConfigurationManager {
public:
  // Load configuration from file
  static ConfigurationManager load(std::filesystem::path const& config_file);

  // Get build configuration
  BuildConfig const& build() const;

  // Get dependency configuration
  DependencyConfig const& dependencies() const;

  // Get compression configuration
  CompressionConfig const& compression() const;

  // Get serialization configuration
  SerializationConfig const& serialization() const;

  // Get feature flags
  FeatureConfig const& features() const;

  // Validate configuration
  ValidationResult validate() const;

private:
  std::shared_ptr<ConfigData> data_;
};

} // namespace dwarfs::config
```

#### `BuildConfig`
```cpp
class BuildConfig {
public:
  int cxx_standard() const;
  std::string default_build_type() const;

  // Check if profile exists
  bool has_profile(std::string_view name) const;

  // Get profile configuration
  BuildProfile get_profile(std::string_view name) const;
};
```

#### `DependencyConfig`
```cpp
class DependencyConfig {
public:
  // Query required dependencies
  std::vector<Dependency> required_dependencies() const;

  // Query optional dependencies
  std::vector<Dependency> optional_dependencies() const;

  // Check if dependency is enabled
  bool is_enabled(std::string_view name) const;

  // Get dependency info
  Dependency get_dependency(std::string_view name) const;
};
```

---

### 2. CMake Integration

**Location:** `cmake/config/`

#### `load_configuration.cmake`
```cmake
# Load DwarFS configuration from YAML
function(dwarfs_load_configuration CONFIG_FILE)
  # Parse YAML using Python helper
  find_package(Python3 REQUIRED)

  execute_process(
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/cmake/config/parse_config.py
            ${CONFIG_FILE}
    OUTPUT_VARIABLE CONFIG_JSON
    RESULT_VARIABLE PARSE_RESULT
  )

  if(NOT PARSE_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to parse configuration file: ${CONFIG_FILE}")
  endif()

  # Process JSON and set CMake variables
  # ...
endfunction()
```

#### `configure_dependencies.cmake`
```cmake
# Configure dependencies based on loaded configuration
function(dwarfs_configure_dependencies)
  # Iterate through required dependencies
  dwarfs_config_get(REQUIRED_DEPS "dependencies.required")
  foreach(dep_name ${REQUIRED_DEPS})
    # Get dependency info from config
    dwarfs_config_get(dep_version "dependencies.required.${dep_name}.min_version")
    dwarfs_config_get(dep_pkgconfig "dependencies.required.${dep_name}.pkg_config")

    # Find package
    if(dep_pkgconfig)
      pkg_check_modules(${dep_name} REQUIRED IMPORTED_TARGET
                       ${dep_pkgconfig}>=${dep_version})
    endif()
  endforeach()

  # Similar for optional dependencies...
endfunction()
```

---

### 3. Python Configuration Parser

**Location:** `cmake/config/parse_config.py`

```python
#!/usr/bin/env python3
"""
DwarFS Configuration Parser

Parses YAML configuration and generates CMake-compatible output.
"""

import sys
import yaml
import json

def load_config(config_file):
    """Load and validate configuration file."""
    with open(config_file, 'r') as f:
        config = yaml.safe_load(f)

    # Validate schema
    validate_schema(config)

    return config

def validate_schema(config):
    """Validate configuration against schema."""
    required_sections = ['build', 'dependencies', 'compression', 'serialization']

    for section in required_sections:
        if section not in config:
            raise ValueError(f"Missing required section: {section}")

    # Additional validation...

def generate_cmake_vars(config):
    """Generate CMake variable definitions."""
    cmake_vars = {}

    # Extract key values
    cmake_vars['DWARFS_CXX_STANDARD'] = config['build']['cxx_standard']
    cmake_vars['DWARFS_DEFAULT_BUILD_TYPE'] = config['build']['default_build_type']

    # Dependencies
    for dep_name, dep_info in config['dependencies']['required'].items():
        cmake_vars[f'DWARFS_REQ_DEP_{dep_name.upper()}_VERSION'] = dep_info['min_version']

    return cmake_vars

def main():
    if len(sys.argv) != 2:
        print("Usage: parse_config.py <config_file>", file=sys.stderr)
        sys.exit(1)

    config_file = sys.argv[1]

    try:
        config = load_config(config_file)
        cmake_vars = generate_cmake_vars(config)

        # Output as JSON for CMake to parse
        print(json.dumps(cmake_vars, indent=2))

    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)

if __name__ == '__main__':
    main()
```

---

### 4. Runtime Configuration Access

**Example Usage:**

```cpp
#include <dwarfs/config/configuration_manager.h>

int main() {
  using namespace dwarfs::config;

  // Load configuration
  auto config = ConfigurationManager::load("config/dwarfs-config.yaml");

  // Validate
  auto validation = config.validate();
  if (!validation.is_valid()) {
    std::cerr << "Configuration errors:\n";
    for (auto const& error : validation.errors()) {
      std::cerr << "  - " << error << "\n";
    }
    return 1;
  }

  // Query compression algorithms
  auto const& compression = config.compression();
  for (auto const& algo : compression.enabled_algorithms()) {
    std::cout << "Available: " << algo.name()
              << " (" << algo.description() << ")\n";
  }

  return 0;
}
```

---

## Migration Strategy

### Phase 1: Foundation (Week 1-2)
1. Create configuration file structure
2. Implement basic YAML parser
3. Create CMake integration functions
4. Test with existing build

### Phase 2: Dependency Management (Week 3-4)
1. Move dependency specifications to config
2. Update CMakeLists.txt to use config
3. Test across platforms
4. Validate backward compatibility

### Phase 3: Feature Configuration (Week 5-6)
1. Externalize feature flags
2. Implement build profiles
3. Update documentation
4. Add validation framework

### Phase 4: Runtime Integration (Week 7-8)
1. Create C++ configuration library
2. Implement runtime config access
3. Update compression/serialization registries
4. Add configuration query APIs

### Phase 5: Polish & Documentation (Week 9-10)
1. Complete validation rules
2. Add configuration examples
3. Create migration guide
4. Update all documentation

---

## Environment-Specific Configuration

### Development Override: `config/dwarfs-config.dev.yaml`
```yaml
# Override defaults for development
build:
  default_build_type: "Debug"

features:
  tests:
    default: true
  dev_tools:
    default: true

  sanitizers:
    undefined:
      default: true

compiler:
  flags:
    common:
      gnu_clang:
        - "-Werror"  # Treat warnings as errors in dev
```

### CI Override: `config/dwarfs-config.ci.yaml`
```yaml
# Override defaults for CI builds
features:
  tests:
    default: true
  coverage:
    default: true

# Strict validation
validation:
  strict: true
```

---

## Benefits

1. **Eliminates Hardcoding**
   - All configuration in one place
   - Easy to update versions
   - Clear dependencies

2. **Improves Maintainability**
   - Self-documenting configuration
   - Centralized validation
   - Consistent patterns

3. **Enables Flexibility**
   - Build profiles for different use cases
   - Environment-specific overrides
   - Platform-specific settings

4. **Enhances Testing**
   - Easy to test different configurations
   - Reproducible builds
   - Clear dependency matrix

5. **Better User Experience**
   - Simple profile selection
   - Clear feature documentation
   - Predictable behavior

---

## Future Extensions

1. **Plugin System**
   - External compression algorithms
   - Custom serialization formats
   - Third-party categorizers

2. **Dynamic Configuration**
   - Runtime configuration reloading
   - Configuration hot-swap
   - Live feature toggling

3. **Configuration GUI**
   - Visual configuration editor
   - Dependency visualization
   - Validation feedback

---

**Document Version:** 1.0
**Date:** 2025-10-28
**Status:** DESIGN PROPOSAL