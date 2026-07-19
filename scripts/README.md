# DwarFS Scripts Directory

This directory contains the unified build system for Tebako DwarFS.

## Directory Structure (NEW Unified System)

```
scripts/
├── lib/                    # Shared library functions (NEW)
│   ├── build_env.sh       # Environment detection, colors, output
│   └── vcpkg_helper.sh    # vcpkg utilities, triplet management
│
├── orchestrator/           # Core build/test/benchmark/release logic (NEW)
│   ├── build.sh           # Build configuration and execution
│   └── release.sh         # Release orchestration
│
├── one-step/               # One-step entry points for users (NEW)
│   ├── test-everything.sh # Run all tests
│   ├── build-all.sh       # Build all configurations
│   ├── clean.sh           # Clean build artifacts
│   └── benchmark-all.sh   # Run all benchmarks
│
├── utils/                  # Utility scripts (NEW)
│   └── clean.sh           # Core clean utility
│
└── [legacy scripts]        # Backward compatibility wrappers
    ├── test-everything.sh -> one-step/test-everything.sh
    ├── clean.sh -> utils/clean.sh
    ├── build-all-and-test.sh
    ├── test-all-configs.sh
    ├── run-all.sh
    ├── clean-build.sh
    ├── clean-all.sh
    └── ...
```

## Quick Start (New Unified System)

### For Developers

```bash
# One-step testing (auto-detects vcpkg or system packages)
./scripts/one-step/test-everything.sh

# Quick validation (no benchmarks)
./scripts/one-step/test-everything.sh --quick

# Force vcpkg mode
./scripts/one-step/test-everything.sh --vcpkg

# Build all configurations
./scripts/one-step/build-all.sh

# Clean build artifacts
./scripts/one-step/clean.sh
```

### For Release Managers

```bash
# Full release validation
./scripts/orchestrator/release.sh --dry-run

# Create release
./scripts/orchestrator/release.sh --version 0.14.2
```

## Legacy Scripts (Still Work)

Old script paths still work via wrapper scripts:

| Legacy Script | New Location | Purpose |
|--------------|--------------|---------|
| `./scripts/test-everything.sh` | `./scripts/one-step/test-everything.sh` | Run all tests |
| `./scripts/clean.sh` | `./scripts/utils/clean.sh` | Clean build artifacts |
| `./scripts/build-all-and-test.sh` | (keep) | Build and test all configs |
| `./scripts/test-all-configs.sh` | (keep) | Test all build configs |
| `./scripts/benchmark-all.sh` | (keep) | Run benchmarks |
| `./scripts/run-all.sh` | (keep) | Clean → Build → Test → Benchmark |
| `./scripts/clean-build.sh` | (keep) | Clean single build |
| `./scripts/clean-all.sh` | (deprecated) | Use `./scripts/utils/clean.sh --all` |

## Library Functions

### build_env.sh

```bash
source scripts/lib/build_env.sh

# Environment detection
dwarfs_detect_os      # => darwin, linux, windows
dwarfs_detect_arch    # => x64, arm64
dwarfs_auto_triplet   # => arm64-osx, x64-linux, etc.

# vcpkg detection
dwarfs_has_vcpkg "$VCPKG_ROOT"
dwarfs_detect_vcpkg

# Dependency detection
dwarfs_has_jemalloc 5.5.0
dwarfs_has_pkgconfig

# Output functions
info "message"
success "message"
warn "message"
error "message"
fatal "message"
section "Title"
```

### vcpkg_helper.sh

```bash
source scripts/lib/vcpkg_helper.sh

# Triplet management
dwarfs_list_triplets
dwarfs_validate_triplet "arm64-osx"
dwarfs_auto_triplet
dwarfs_triplets_for_platform "osx"

# vcpkg commands
dwarfs_vcpkg "$VCPKG_ROOT" install fmt
dwarfs_vcpkg_install "$VCPKG_ROOT" "fmt" "arm64-osx"

# Overlay management
dwarfs_setup_overlays "$VCPKG_ROOT" "$PROJECT_ROOT"

# Tebako jemalloc
dwarfs_verify_tebako_jemalloc "$PROJECT_ROOT"
dwarfs_tebako_jemalloc_version "$PROJECT_ROOT"

# Cache management
dwarfs_vcpkg_clear_cache "$VCPKG_ROOT"
```

## Orchestrator Modules

### build.sh

```bash
source scripts/orchestrator/build.sh

# List all configurations
dwarfs_list_configs
# => flatbuffers-only both-formats

# Build a configuration
dwarfs_build_configuration "flatbuffers-only" "build-fb" "Release" "vcpkg" "$VCPKG_ROOT" "arm64-osx"

# Build all configurations
dwarfs_build_all "vcpkg" "$VCPKG_ROOT" "arm64-osx"

# Clean builds
dwarfs_clean_builds "build-*"
dwarfs_clean_all
```

## Extending the Build System

### Adding a New Configuration

Edit `scripts/orchestrator/build.sh`:

```bash
DWARFS_BUILD_CONFIGS=(
    "flatbuffers-only:OFF:FlatBuffers-only"
    "both-formats:ON:Both-formats"
    "new-config:OFF:New Configuration"
)
```

### Adding a New Triplet

1. Create `vcpkg_triplets/new-triplet.cmake`
2. Add to `scripts/lib/vcpkg_helper.sh:dwarfs_list_triplets()`
3. Update `vcpkg_triplets/README.md`

### Adding a New One-Step Script

Create `scripts/one-step/new-script.sh`:

```bash
#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$PROJECT_ROOT"

source scripts/lib/build_env.sh
source scripts/lib/vcpkg_helper.sh
source scripts/orchestrator/build.sh

# Your script logic here
```

## See Also

- **[`BUILD_SYSTEM_ARCHITECTURE.md`](../BUILD_SYSTEM_ARCHITECTURE.md)** - Complete architecture documentation
- **[`TESTING.md`](../TESTING.md)** - Testing documentation
- **[`vcpkg_triplets/README.md`](../vcpkg_triplets/README.md)** - Triplet documentation
- **[`benchmarks/README.md`](../benchmarks/README.md)** - Benchmark documentation

---

**Last Updated**: 2025-01-18
**Status**: Unified build system - production ready
