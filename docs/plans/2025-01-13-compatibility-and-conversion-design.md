# Compatibility Testing and Format Conversion Design

**Goal:** Ensure the Frozen2 serializer maintains compatibility with both dwarfs-rs and Homebrew dwarfs, and enable bidirectional conversion between FlatBuffers and Legacy Thrift formats.

**Date:** 2025-01-13

**Status:** Approved Design

---

## Section 1: Overall Architecture

The solution consists of three major components:

1. **Compatibility Test Suite**: Comprehensive tests to ensure we never lose compatibility with existing implementations
2. **Documentation Updates**: Covering user-facing usage, developer architecture, compatibility specification, and testing guide
3. **Format Converter**: Bidirectional conversion between FlatBuffers and Legacy Thrift using a Load → Save pipeline

The Load → Save pattern for format conversion works by:
- **Load**: Deserialize source format (FlatBuffers or Legacy Thrift) into the shared `domain::metadata` representation
- **Save**: Serialize `domain::metadata` to target format

This ensures that filesystem metadata is preserved while internal implementation state is discarded.

---

## Section 2: Compatibility Test Suite

The test suite consists of four categories:

### Round-trip Tests
- Serialize test metadata → Deserialize → Compare
- Ensures Frozen2 serializer produces valid output that can be read back
- Covers all metadata types: files, directories, symlinks, permissions, timestamps

### Schema Validation Tests
- Ensure generated schemas match dwarfs-rs schema structure exactly
- Binary comparison of schema section with reference images
- Validate all field types and encodings

### Reference Image Tests
- Test against known-good images from **dwarfs-rs** (build locally and in CI)
- Test against known-good images from **Homebrew dwarfs** (use existing installation)
- Mount images, verify file contents, metadata, permissions
- Binary comparison of metadata sections

### Regression Tests
- Golden Master Testing: Serialize known metadata, save as golden files, compare on each run
- Prevent unintended changes to serialization format
- Run on every commit in CI

**Test Data Sources:**
- **Homebrew dwarfs**: Use existing installation (works locally and in GitHub Actions)
- **dwarfs-rs**: Build locally and in CI to create reference images

---

## Section 3: Documentation Structure

### User Guide (`docs/frozen2_serializer.md`)
- How to create Legacy Thrift format images
- Command-line options for format selection
- Performance characteristics
- When to use each format

### Developer Architecture (`docs/metadata_architecture.md`)
- Domain metadata model
- Serialization framework
- Frozen2 implementation details
- Format comparison matrix

### Compatibility Specification (`docs/compatibility_spec.md`)
- Compatible dwarfs-rs versions
- Compatible Homebrew dwarfs versions
- Schema format specification
- Test coverage matrix

### Testing Guide (`docs/testing_guide.md`)
- How to run compatibility tests
- How to create test fixtures
- CI configuration
- Debugging compatibility issues

---

## Section 4: Unified CLI Design

The `dwarfs` tool will be extended with subcommands:

```bash
dwarfs mount <image> <mountpoint>    # FUSE mount (current functionality)
dwarfs create <dir> <output>         # Create image (mkdwarfs functionality)
dwarfs check <image>                 # Verify image (dwarfsck functionality)
dwarfs convert <input> <output>      # NEW: Format conversion
```

### Convert Subcommand

```bash
dwarfs convert <input> <output> [options]

Options:
  --format <flatbuffers|legacy>    # Target format (default: auto-detect)
  --validate                       # Check if conversion is possible without converting
  --verbose                        # Detailed output
```

**Format Auto-Detection:**
- Input: Read metadata section, detect format from magic bytes/schema
- Output: Use file extension (.dff for FlatBuffers, .dft for Legacy Thrift)

**Example Usage:**
```bash
# Convert FlatBuffers to Legacy Thrift
dwarfs convert image.dff image.dft --format legacy

# Convert Legacy Thrift to FlatBuffers
dwarfs convert image.dft image.dff --format flatbuffers

# Auto-detect from extension
dwarfs convert image.dff image.dft

# Validate only (dry-run)
dwarfs convert image.dff image.dft --validate
```

---

## Section 5: Implementation Plan and Order

### Phase 1: Compatibility Tests (Foundation)
**Duration:** 4-6 tasks

1. Create test fixtures using Homebrew dwarfs (run locally, commit to repo)
2. Build dwarfs-rs and create reference images (add to CI)
3. Implement round-trip tests for Frozen2 serializer
4. Implement schema validation tests
5. Implement reference image tests
6. Add regression tests with golden master files

### Phase 2: Documentation (Parallel with Phase 1)
**Duration:** 3-4 tasks

1. Write user guide for Frozen2 serializer
2. Write developer architecture documentation
3. Write compatibility specification
4. Write testing guide

### Phase 3: Format Conversion (Depends on Phase 1)
**Duration:** 5-7 tasks

1. Extend `dwarfs` CLI with subcommand infrastructure
2. Implement format auto-detection
3. Implement convert_handler for FlatBuffers → Legacy Thrift
4. Implement convert_handler for Legacy Thrift → FlatBuffers
5. Add `--validate` mode
6. Add error handling and validation
7. Integration tests for format conversion

### Phase 4: CI Integration
**Duration:** 2-3 tasks

1. Add dwarfs-rs build to GitHub Actions
2. Add compatibility test suite to CI
3. Add format conversion tests to CI

**Total Estimated Tasks:** 14-20 tasks

---

## Implementation Notes

**Key Principles:**
- Use existing `domain::metadata` as the intermediate format
- Preserve filesystem metadata (file contents, permissions, timestamps)
- Discard internal implementation state during conversion
- Follow existing handler pattern from `mkdwarfs` for consistency
- Auto-detect formats when possible, require explicit override when ambiguous

**File Locations:**
- Test suite: `test/metadata/compatibility/`
- Fixtures: `test/fixtures/dwarfs-rs/` and `test/fixtures/homebrew/`
- Convert handler: `tools/include/dwarfs/tool/dwarfs/convert_handler.h`
- CLI main: `tools/src/dwarfs.cpp`
