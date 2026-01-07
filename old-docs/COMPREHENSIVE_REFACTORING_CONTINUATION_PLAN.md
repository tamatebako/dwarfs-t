# Comprehensive CLI Tools Refactoring Plan

**Date**: 2025-11-25 19:40 HKT
**Branch**: refactor/mkdwarfs-phase1
**Status**: mkdwarfs refactoring 93% complete (Phase 7.5-7.6 remaining)

---

## Executive Summary

The mkdwarfs refactoring has successfully demonstrated the handler pattern approach, reducing main() from 1578 to 689 lines (56.3% reduction). This plan extends the same architectural principles to **all DwarFS CLI tools** for consistency, testability, and maintainability.

**Current Tool Sizes**:
- `dwarfs_main.cpp`: **2041 lines** ← LARGEST, HIGHEST PRIORITY
- `mkdwarfs_main.cpp`: 689 lines (refactored from 1578)
- `dwarfsck_main.cpp`: 391 lines ← MEDIUM PRIORITY
- `dwarfsextract_main.cpp`: 280 lines ← LOW PRIORITY
- `universal.cpp`: 134 lines (minimal refactoring needed)

---

## Part 1: Complete mkdwarfs Refactoring

### Phase 7.5: Final Code Review (IN PROGRESS)

**Status**: TODO
**Estimated Time**: 30-60 minutes

#### Build Verification

```bash
# Test with Thrift
rm -rf build-with-thrift
cmake -B build-with-thrift -GNinja \
  -DDWARFS_WITH_THRIFT=ON \
  -DWITH_TESTS=ON \
  -DWITH_FUSE_DRIVER=OFF
ninja -C build-with-thrift
ctest --test-dir build-with-thrift -R mkdwarfs --output-on-failure

# Test without Thrift  
rm -rf build-no-thrift
cmake -B build-no-thrift -GNinja \
  -DDWARFS_WITH_THRIFT=OFF \
  -DWITH_TESTS=ON \
  -DWITH_FUSE_DRIVER=OFF
ninja -C build-no-thrift
ctest --test-dir build-no-thrift -R mkdwarfs --output-on-failure
```

### Phase 7.6: Prepare Merge Plan

**Status**: TODO
**Estimated Time**: 30 minutes

**Commit Strategy**: Keep separate phase commits for reviewability

---

## Part 2: Extend Pattern to All CLI Tools

### Tool Refactoring Priority Matrix

| Tool | Lines | Complexity | Priority | Est. Time |
|------|-------|------------|----------|-----------|
| **dwarfs** | 2041 | VERY HIGH | **1 - CRITICAL** | 8-12 hours |
| **dwarfsck** | 391 | MEDIUM | **2 - HIGH** | 3-4 hours |
| **dwarfsextract** | 280 | LOW-MEDIUM | **3 - MEDIUM** | 2-3 hours |
| **universal** | 134 | LOW | **4 - LOW** | 1 hour |

---

## Tool 1: dwarfs (FUSE Driver) - CRITICAL PRIORITY

### Current Situation

**File**: `tools/src/dwarfs_main.cpp` (2041 lines)
**Complexity**: VERY HIGH
- FUSE callback implementations (20+ operations)
- Platform-specific code (Linux/macOS/Windows/FreeBSD)
- Conditional compilation for FUSE2/FUSE3/WinFsp/FUSE-T
- Performance monitoring integration
- Complex option parsing

### Proposed Architecture

```
dwarfs_main.cpp (target: <500 lines)
├── options_parser (~200 lines extracted)
│   ├── Parse FUSE options
│   ├── Validate configuration
│   └── Return parsed_options
├── filesystem_loader (~150 lines extracted)
│   ├── Load DwarFS image
│   ├── Initialize filesystem_v2
│   └── Setup cache and workers
├── fuse_operations_handler (~800 lines extracted)
│   ├── op_init, op_lookup, op_getattr
│   ├── op_read, op_readdir, op_statfs
│   └── Platform-specific implementations
└── handler_factory (~100 lines)
    ├── Select FUSE mode (lowlevel/high-level)
    └── Platform-specific setup
```

### Expected Outcome

- `dwarfs_main.cpp`: 2041 → ~450 lines (**78% reduction**)
- 8 new modules with clean separation
- Testable FUSE operations
- Platform-specific code properly isolated

---

## Tool 2: dwarfsck (Check/Inspect) - HIGH PRIORITY

### Current Situation  

**File**: `tools/src/dwarfsck_main.cpp` (391 lines)
**Operations**: list files, checksum, integrity check, JSON export

### Proposed Architecture

```
dwarfsck_main.cpp (target: <150 lines)
├── options_parser (~100 lines extracted)
├── list_files_handler
├── checksum_handler  
├── integrity_check_handler
└── handler_factory
```

### Expected Outcome

- `dwarfsck_main.cpp`: 391 → ~150 lines (**62% reduction**)
- 11 new modules
- Each operation independently testable

---

## Tool 3: dwarfsextract (Extract) - MEDIUM PRIORITY

### Current Situation

**File**: `tools/src/dwarfsextract_main.cpp` (280 lines)
**Operations**: filesystem extraction, archive conversion, pattern matching

### Proposed Architecture

```
dwarfsextract_main.cpp (target: <100 lines)
├── options_parser (~80 lines extracted)
└── extract_handler (~100 lines extracted)
```

### Expected Outcome

- `dwarfsextract_main.cpp`: 280 → ~100 lines (**64% reduction**)
- 4 new modules
- Clean extraction workflow

---

## Implementation Timeline

### Week 1: Complete mkdwarfs
- **Days 1-2**: Integration tests ✅ DONE
- **Day 3**: Final review and documentation

### Weeks 2-3: Refactor dwarfs  
- **Days 1-2**: Analysis and planning
- **Days 3-5**: Options parser extraction
- **Days 6-8**: FUSE operations handler
- **Days 9-10**: Filesystem loader
- **Days 11-12**: Handler factory and integration tests

### Week 4: Refactor dwarfsck
- **Days 1-2**: Options parser
- **Days 3-4**: Operation handlers  
- **Day 5**: Integration tests

### Week 5: Refactor dwarfsextract
- **Days 1-2**: Options parser and extract handler
- **Day 3**: Integration tests

---

## Testing Strategy

### Integration Tests for Each Tool

**dwarfs tests**:
- Mount/unmount workflows
- FUSE operation correctness
- Platform-specific functionality
- Performance monitoring

**dwarfsck tests**:
- List files functionality
- Checksum computation
- Integrity checking
- JSON export

**dwarfsextract tests**:
- Directory extraction
- Archive format conversion
- Pattern matching

### Build Matrix

```
Tool         | FlatBuffers | Thrift | FUSE2 | FUSE3 | WinFsp |
-------------|-------------|--------|-------|-------|--------|
mkdwarfs     | ✓           | ±      | N/A   | N/A   | N/A    |
dwarfs       | ✓           | ±      | ±     | ±     | ±      |
dwarfsck     | ✓           | ±      | N/A   | N/A   | N/A    |
dwarfsextract| ✓           | ±      | N/A   | N/A   | N/A    |
```

---

## Success Metrics

### Code Metrics

| Metric | Before | Target | Improvement |
|--------|--------|--------|-------------|
| Total main() lines | 3391 | <1400 | **58.7%** |
| mkdwarfs_main | 1578 | 689 | **56.3%** ✓ |
| dwarfs_main | 2041 | <500 | **75.5%** |
| dwarfsck_main | 391 | <150 | **61.6%** |
| dwarfsextract_main | 280 | <100 | **64.3%** |

### Quality Metrics

- ✓ All tools use consistent handler pattern
- ✓ Each tool has integration tests
- ✓ Build with all feature combinations
- ✓ No God functions (max 200 lines per function)
- ✓ Clear separation of concerns

---

## Documentation Updates

### New Documents

1. `doc/TOOL_REFACTORING_GUIDE.md` - Common patterns and guidelines
2. `doc/ARCHITECTURE.md` - Updated with CLI tools architecture

### Updated Documents

1. `.kilocode/rules/memory-bank/architecture.md` - Add tool sections
2. `CHANGES.md` - Comprehensive v0.16.0 entry

---

## Next Actions

### Immediate (mkdwarfs completion)
1. Run build verification tests
2. Prepare merge commit message
3. Tag release (optional)

### Short-term (dwarfs refactoring)
1. Study dwarfs_main.cpp structure
2. Design module boundaries  
3. Begin options_parser extraction

### Long-term (all tools)
1. Apply pattern to dwarfsck
2. Apply pattern to dwarfsextract
3. Update all documentation

---

**Last Updated**: 2025-11-25 19:52 HKT
**Status**: mkdwarfs 93% complete, comprehensive plan ready
**Total Estimated Time**: 3-4 weeks for all tools