# Phase C & D: Documentation Update and Testing - Continuation Plan

**Date**: 2025-11-30  
**Current Status**: Phase B Complete ✅  
**Next Phases**: C (Documentation) → D (Test Matrix & Benchmarks)

---

## Quick Start for Next Session

```bash
cd /Users/mulgogi/src/external/dwarfs
git branch --show-current  # Should be: refactor/dwarfs-mkdwarfs-complete

# Read this plan
cat doc/PHASE_C_D_CONTINUATION_PLAN.md

# Read implementation status
cat doc/PHASE_C_D_IMPLEMENTATION_STATUS.md
```

---

## Phase C: Documentation Update

**Priority**: HIGH  
**Estimated Time**: 1-2 hours  
**Status**: Ready to start

### Objectives

1. Create new `README.md` for Tebako fork
2. Preserve original as `README.DWARFS.md`
3. Update documentation to reflect FlatBuffers as default
4. Move temporary documentation to `doc/old-docs/`

### Tasks

#### C1: Preserve Original README (5 min)
```bash
mv README.md README.DWARFS.md
```

#### C2: Create New Tebako Fork README (45 min)

**File**: `README.md`

**Required Sections**:
1. **Project Title & Badges**
   - "DwarFS (Tebako Fork)"
   - Build status badges
   - License badge

2. **Introduction**
   - What is this fork
   - Key differences from upstream
   - Link to original: `README.DWARFS.md`

3. **Key Features of This Fork**
   - FlatBuffers metadata (modern default)
   - Optional Thrift support (backward compatibility)
   - Extensive CI/CD coverage
   - Library form support (static/shared)
   - FUSE-T support (macOS)
   - Platform support matrix

4. **Quick Start**
   - Installation instructions
   - Basic usage examples
   - Building from source

5. **Metadata Serialization**
   - FlatBuffers (default) vs Thrift (optional)
   - Size comparison results (Phase B)
   - Build options

6. **Platform Support**
   - Linux, macOS, Windows, FreeBSD
   - Architectures: x86_64, aarch64, etc.
   - CI/CD matrix coverage

7. **Build Options**
   - Core options (WITH_LIBDWARFS, WITH_TOOLS, etc.)
   - Format options (WITH_FLATBUFFERS, WITH_THRIFT)
   - Platform-specific options

8. **Documentation**
   - Links to detailed docs
   - Original DwarFS docs reference
   - Tebako-specific docs

9. **Contributing**
   - Fork-specific guidelines
   - Link to upstream

10. **License**
    - GPL 3.0 as per original

#### C3: Move Temporary Documentation (15 min)

```bash
mkdir -p doc/old-docs/phase-b-size-optimization
mv doc/FLATBUFFERS_METADATA_*.md doc/old-docs/phase-b-size-optimization/
mv doc/CRITICAL_FIXES_*.md doc/old-docs/phase-b-size-optimization/
mv doc/METADATA_DUAL_FORMAT_*.md doc/old-docs/phase-b-size-optimization/

# Keep PHASE_B_SIZE_ANALYSIS.md as official reference
```

#### C4: Update Memory Bank (15 min)

Update `.kilocode/rules/memory-bank/context.md`:
- Mark Phase B complete
- Document Phase C progress
- Note new README.md structure

---

## Phase D: Test Matrix & Benchmarks

**Priority**: HIGH  
**Estimated Time**: 2-3 hours  
**Status**: Ready after Phase C

### Objectives

1. Run test matrix across configurations
2. Execute benchmarks comparing formats
3. Validate all functionality
4. Document results

### Test Matrix

| Configuration | Build | Create | Verify | Extract | Mount |
|---------------|-------|--------|--------|---------|-------|
| FlatBuffers-only | ✅ | ⏸️ | ⏸️ | ⏸️ | ⏸️ |
| Thrift-only | ✅ | ⏸️ | ⏸️ | ⏸️ | ⏸️ |

### D1: Build Verification (30 min)

**Already Complete**:
- ✅ FlatBuffers-only: `build-fb/`
- ✅ Thrift-only: `build-tb/`

**Verify Tools**:
```bash
# FlatBuffers-only
./build-fb/mkdwarfs --version
./build-fb/dwarfsck --version
./build-fb/dwarfsextract --version

# Thrift-only
./build-tb/mkdwarfs --version
./build-tb/dwarfsck --version
./build-tb/dwarfsextract --version
```

### D2: Functional Test Matrix (45 min)

**Test Dataset**: `/tmp/size-test` (already created)

**For Each Configuration**:

1. **Create Image**
   ```bash
   ./{build}/mkdwarfs -i /tmp/size-test -o /tmp/test-{format}.dwarfs
   ```

2. **Verify Image**
   ```bash
   ./{build}/dwarfsck /tmp/test-{format}.dwarfs
   ```

3. **Extract Image**
   ```bash
   ./{build}/dwarfsextract -i /tmp/test-{format}.dwarfs -o /tmp/extract-{format}
   diff -r /tmp/size-test /tmp/extract-{format}
   ```

4. **Check Metadata**
   ```bash
   ./{build}/dwarfsck -j /tmp/test-{format}.dwarfs > /tmp/meta-{format}.json
   ```

### D3: Benchmark Execution (60 min)

**Use Existing Benchmark Framework**: `benchmarks/`

#### D3.1: Prepare Dataset
```bash
python3 benchmarks/download_datasets.py --download perl
```

#### D3.2: Run Benchmark
```bash
python3 benchmarks/run_metadata_format_benchmark.py \
  --mkdwarfs-fb ./build-fb/mkdwarfs \
  --mkdwarfs-tb ./build-tb/mkdwarfs \
  --dwarfsextract-fb ./build-fb/dwarfsextract \
  --dwarfsextract-tb ./build-tb/dwarfsextract \
  --perl-dataset benchmark-files/perl-5.43.3 \
  --output benchmark-results/phase-d-results.json \
  --runs 3
```

#### D3.3: Generate Report
```bash
python3 benchmarks/generate_metadata_report.py \
  benchmark-results/phase-d-results.json \
  benchmark-results/PHASE_D_REPORT.md
```

### D4: Document Results (30 min)

**File**: `doc/PHASE_D_TEST_RESULTS.md`

**Contents**:
- Test matrix results
- Benchmark comparison
- Performance analysis
- Conclusions

---

## Success Criteria

### Phase C Complete When:
- [ ] New `README.md` created for Tebako fork
- [ ] Original preserved as `README.DWARFS.md`
- [ ] Temporary docs moved to `doc/old-docs/`
- [ ] Memory bank updated

### Phase D Complete When:
- [ ] All test matrix items pass
- [ ] Benchmarks executed successfully
- [ ] Results documented
- [ ] No regressions identified

---

## Architecture Principles for Phases C & D

### Documentation (Phase C)

1. **Clarity**: New README clearly explains fork differences
2. **Completeness**: All key features documented
3. **Navigation**: Easy to find information
4. **Maintainability**: Structure supports future updates

### Testing (Phase D)

1. **Comprehensive**: All configurations tested
2. **Reproducible**: Clear steps to repeat tests
3. **Measurable**: Quantitative results captured
4. **Actionable**: Issues clearly identified

---

## Rollback Plan

If issues arise:

```bash
# Phase C rollback
git checkout HEAD -- README.md
rm README.DWARFS.md

# Phase D rollback
# No code changes, just remove test artifacts
rm -rf /tmp/test-* /tmp/extract-*
```

---

## Timeline

### Phase C: 1-2 hours
- C1: 5 min (preserve original)
- C2: 45 min (create new README)
- C3: 15 min (move temp docs)
- C4: 15 min (update memory bank)

### Phase D: 2-3 hours
- D1: 30 min (build verification)
- D2: 45 min (functional tests)
- D3: 60 min (benchmarks)
- D4: 30 min (documentation)

**Total**: 3-5 hours for both phases

---

## Files to Create/Modify

### Phase C
- **NEW**: `README.md` (Tebako fork)
- **RENAME**: `README.md` → `README.DWARFS.md`
- **MOVE**: Temp docs to `doc/old-docs/`
- **UPDATE**: `.kilocode/rules/memory-bank/context.md`

### Phase D
- **NEW**: `doc/PHASE_D_TEST_RESULTS.md`
- **NEW**: `benchmark-results/PHASE_D_REPORT.md`
- **NEW**: `benchmark-results/phase-d-results.json`

---

## Next Session Commands

### Start Phase C:
```bash
# 1. Preserve original
mv README.md README.DWARFS.md

# 2. Create new README
# (Use editor to create README.md)

# 3. Move temp docs
mkdir -p doc/old-docs/phase-b-size-optimization
mv doc/FLATBUFFERS_METADATA_*.md doc/old-docs/phase-b-size-optimization/
```

### Start Phase D:
```bash
# 1. Verify builds
./build-fb/mkdwarfs --version
./build-tb/mkdwarfs --version

# 2. Run tests
# (Follow D2 test matrix)

# 3. Run benchmarks
python3 benchmarks/download_datasets.py --download perl
python3 benchmarks/run_metadata_format_benchmark.py ...
```

---

**Status**: Ready to execute  
**Next Phase**: C1 - Preserve original README