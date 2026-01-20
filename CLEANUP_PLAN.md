# Scripts and Benchmarks Cleanup Plan

This document outlines the cleanup of `scripts/` and `benchmarks/` directories to follow MECE principles (Mutually Exclusive, Collectively Exhaustive).

## Current Issues

1. **Duplicate functionality**: Multiple scripts do similar things
2. **Unclear naming**: Script purposes are not immediately obvious
3. **Scattered locations**: Mix of scripts in `scripts/` and `benchmarks/`
4. **Missing hierarchy**: No clear entry points for different use cases

## Principles

- **MECE**: Each script has a unique, non-overlapping purpose
- **Clear naming**: Script names clearly indicate their function
- **Logical grouping**: Related scripts are grouped together
- **Single responsibility**: Each script does one thing well
- **Clear entry points**: Obvious main scripts for each use case

## Proposed Structure

### scripts/ (Build and Development Scripts)

```
scripts/
├── build/
│   ├── clean-build.sh       # Clean and rebuild specific configuration
│   ├── build-all.sh         # Build all configurations
│   └── test-all.sh          # Build and test all configurations
├── vcpkg/
│   ├── setup-vcpkg.sh       # Initial vcpkg setup
│   ├── test-vcpkg.sh        # Test vcpkg installation
│   └── verify-ports.sh       # Verify vcpkg overlay ports
├── benchmark/
│   └── run-benchmark.sh     # Entry point for all benchmarks
└── utils/
    ├── clean-all.sh         # Clean all build artifacts
    └── verify-setup.sh       # Verify development environment
```

### benchmarks/ (Benchmark Implementation)

```
benchmarks/
├── run_all.sh               # Main entry point (delegates to individual)
├── lib/
│   ├── common.sh            # Shared benchmark utilities
│   └── runner.sh            # Benchmark execution framework
├── formats/                 # Metadata format benchmarks
│   ├── metadata_format.sh
│   └── flatbuffers.sh
├── compression/            # Compression algorithm benchmarks
│   └── algorithms.sh
├── operations/              # Filesystem operation benchmarks
│   ├── creation.sh
│   ├── read.sh
│   └── extraction.sh
└── reports/                 # Report generation
    ├── generate_report.py
    └── templates/
        └── summary.md
```

## File Mapping (Old → New)

### scripts/

| Old File | New File | Notes |
|----------|----------|-------|
| `benchmark-all.sh` | `benchmark/run-benchmark.sh` | Move to benchmark/ |
| `build-all-and-test.sh` | `build/test-all.sh` | Rename for clarity |
| `clean-all.sh` | `utils/clean-all.sh` | Keep in utils |
| `clean-build.sh` | `build/clean-build.sh` | Move to build/ |
| `clean.sh` | **DELETE** | Redundant with clean-all.sh |
| `run-all.sh` | **DELETE** | Unclear purpose |
| `test_vcpkg_install.sh` | `vcpkg/test-vcpkg.sh` | Move to vcpkg/ |
| `test-all-configs.sh` | `build/test-all.sh` | Merge with test-all.sh |
| `verify_benchmark_setup.sh` | `utils/verify-setup.sh` | Move to utils |

### benchmarks/

| Old File | New File | Notes |
|----------|----------|-------|
| `run_all_benchmarks.sh` | **DELETE** | Use run_all.sh instead |
| `run_comprehensive_benchmark.sh` | **DELETE** | Use run_all.sh --comprehensive |
| `run_flatbuffers_benchmark.sh` | `formats/flatbuffers.sh` | Move to formats/ |
| `run_libdwarfs_benchmark.sh` | `operations/library.sh` | Move to operations/ |
| `run_quick_comprehensive_test.sh` | **DELETE** | Use run_all.sh --quick |

### benchmarks/*.py (Python scripts - keep, organize)

| Old File | New File | Notes |
|----------|----------|-------|
| `build_and_test_all.py` | **DELETE** | Use scripts/build/test-all.sh |
| `comprehensive_benchmark.py` | `lib/comprehensive.py` | Move to lib/ |
| `compression_algorithm_benchmark.py` | `compression/algorithms.py` | Move to compression/ |
| `download_datasets.py` | `lib/download_datasets.py` | Move to lib/ |
| `extraction_benchmark.py` | `operations/extraction.py` | Move to operations/ |
| `generate_comprehensive_report.py` | `reports/comprehensive.py` | Move to reports/ |
| `generate_compression_report.py` | `reports/compression.py` | Move to reports/ |
| `generate_metadata_report.py` | `reports/metadata.py` | Move to reports/ |
| `metadata_format_benchmark.py` | `formats/metadata.py` | Move to formats/ |
| `run_all_benchmarks.py` | **DELETE** | Duplicate of run_all.sh |
| `run_metadata_format_benchmark.py` | **DELETE** | Use run_all.sh --metadata |

## Implementation Steps

1. Create new directory structure
2. Move/rename files according to mapping
3. Update imports and references
4. Delete redundant files
5. Update documentation (README.md)
6. Update CI/CD workflows to use new paths

## Usage Examples After Cleanup

### Building

```bash
# Clean and rebuild
./scripts/build/clean-build.sh

# Build all configurations
./scripts/build/build-all.sh

# Build and test all
./scripts/build/test-all.sh
```

### Benchmarking

```bash
# Run all benchmarks
./benchmarks/run_all.sh

# Run specific benchmark type
./benchmarks/run_all.sh --formats
./benchmarks/run_all.sh --compression
./benchmarks/run_all.sh --operations

# Run with specific options
./benchmarks/run_all.sh --quick
./benchmarks/run_all.sh --comprehensive
```

### vcpkg

```bash
# Setup vcpkg
./scripts/vcpkg/setup-vcpkg.sh

# Test vcpkg installation
./scripts/vcpkg/test-vcpkg.sh

# Verify overlay ports
./scripts/vcpkg/verify-ports.sh
```

### Utilities

```bash
# Clean all build artifacts
./scripts/utils/clean-all.sh

# Verify development environment
./scripts/utils/verify-setup.sh
```

## Notes

- The `benchmarks/README.md` should be updated with new structure
- All Python scripts should import from the new locations
- CI/CD workflows need path updates
- Consider adding a `scripts/` entry point that delegates to subdirectories
