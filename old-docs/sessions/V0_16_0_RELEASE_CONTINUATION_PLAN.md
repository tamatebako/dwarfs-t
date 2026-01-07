# DwarFS v0.16.0 Release - Continuation Plan

**Created**: 2025-12-08
**Status**: Documentation Complete, Validation Pending
**Target Release**: 2025-12-15

---

## Current Status Summary

### ✅ Completed Work
1. **FlatBuffers Verifier Fix** (2025-12-07)
   - Increased `max_depth` from 64 to 256
   - Increased `max_tables` from 1M to 10M
   - File: `src/reader/internal/metadata_v2_flatbuffers.cpp:126-133`
   - Validated on small images (13 bytes): CREATE ✅ + EXTRACT ✅

2. **Documentation Updates** (2025-12-08)
   - `README.md`: Added "Known Issues and Workarounds" section
   - `CHANGES.md`: Added "Critical Fixes" section
   - Memory bank updated with current status

3. **Build System**
   - All 3 configurations working (fb-only, thrift-only, both)
   - Full test suite: 1,600/1,613 passing (13 Thrift-only skipped)

4. **Tool Refactoring**
   - mkdwarfs: Modular handler pattern (56.3% reduction)
   - dwarfs: Reusable library components (82.7% reduction)

---

## Remaining Work to v0.16.0 Release

### Phase 1: Validation & Testing (Priority: HIGH)

#### 1.1 Large Image Validation (Optional but Recommended)
**Status**: ⏳ Pending
**ETA**: 2 hours
**Blocking**: No

**Context**: Existing large test image (3.8GB) was created BEFORE the verifier fix, causing segfaults.

**Tasks**:
- [ ] Recreate large test image from source with fixed build
- [ ] Validate with `dwarfsck --check-integrity`
- [ ] Validate with `dwarfsextract` (full extraction)
- [ ] Document results in validation report

**Commands**:
```bash
# Rebuild fb-only build to ensure latest code
cmake --build build-fb-bench --clean-first

# Recreate large image (if source available)
./build-fb-bench/mkdwarfs -i <source> -o benchmark-data/images/dwarfs-source_flatbuffers_v16.dwarfs --format flatbuffers

# Validate
./build-fb-bench/dwarfsck benchmark-data/images/dwarfs-source_flatbuffers_v16.dwarfs --check-integrity
./build-fb-bench/dwarfsextract -i benchmark-data/images/dwarfs-source_flatbuffers_v16.dwarfs -o /tmp/extract-validation
```

**Acceptance Criteria**:
- ✅ Image creation succeeds
- ✅ Integrity check passes
- ✅ Full extraction completes without errors

#### 1.2 Quick Validation Suite (Mandatory)
**Status**: ⏳ Pending
**ETA**: 30 minutes
**Blocking**: No

**Tasks**:
- [ ] Run small image validation (already validated ✅)
- [ ] Test all 3 build configurations (fb-only, thrift-only, both)
- [ ] Verify cross-format compatibility
- [ ] Test sparse file support across builds

**Commands**:
```bash
# Test fb-only
./build-fb-bench/mkdwarfs -i /tmp/test -o /tmp/fb-test.dff
./build-fb-bench/dwarfsextract -i /tmp/fb-test.dff -o /tmp/fb-extract

# Test thrift-only (if available)
./build-thrift-bench/mkdwarfs -i /tmp/test -o /tmp/thrift-test.dft --format thrift
./build-thrift-bench/dwarfsextract -i /tmp/thrift-test.dft -o /tmp/thrift-extract

# Test both-formats
./build-both-bench/dwarfsextract -i /tmp/fb-test.dff -o /tmp/both-fb
./build-both-bench/dwarfsextract -i /tmp/thrift-test.dft -o /tmp/both-thrift
```

---

### Phase 2: Benchmarking (Priority: MEDIUM)

#### 2.1 Comprehensive Benchmark Suite
**Status**: ⏳ Pending
**ETA**: 2-4 hours (dataset-dependent)
**Blocking**: No

**Reference**: `doc/COMPREHENSIVE_BENCHMARK_CONTINUATION_PROMPT.md`

**Scope**: 60 test scenarios
- 3 datasets (small/medium/large)
- 3 formats (fb-only, thrift-only, both)
- 4 operations (create, verify, extract, mount)
- 2 interfaces (tool, library)

**Tasks**:
- [ ] Prepare benchmark datasets
- [ ] Run comprehensive benchmark suite
- [ ] Generate performance report
- [ ] Compare with v0.14.1 baseline
- [ ] Document any regressions

**Commands**:
```bash
cd /Users/mulgogi/src/external/dwarfs

# Run comprehensive benchmarks
python3 benchmarks/run_all_benchmarks.py --output benchmark-results/v0.16.0

# Generate reports
python3 benchmarks/generate_comprehensive_report.py \
  --input benchmark-results/v0.16.0 \
  --output doc/V0_16_0_BENCHMARK_RESULTS.md
```

**Acceptance Criteria**:
- ✅ All benchmark tests complete successfully
- ✅ No significant performance regressions (>10%)
- ✅ FlatBuffers performance within 10% of Thrift
- ✅ Report generated and reviewed

---

### Phase 3: CI/CD Validation (Priority: HIGH)

#### 3.1 Full Platform CI/CD Run
**Status**: ⏳ Pending
**ETA**: 1-2 hours (automated)
**Blocking**: Yes (for RC tag)

**Platforms to Test**:
- Ubuntu 22.04/24.04 (x86_64, aarch64)
- macOS 14/15 (arm64, x86_64)
- Windows Server 2022/2025 (x64)
- Windows 11 (arm64)
- Fedora, Debian, Arch, Alpine, openSUSE

**Tasks**:
- [ ] Trigger GitHub Actions workflow
- [ ] Monitor all platform builds
- [ ] Review test results across platforms
- [ ] Address any platform-specific failures
- [ ] Verify artifact generation

**Commands**:
```bash
# Push to trigger CI
git push origin feature/metadata-serialization-v16

# Monitor via GitHub Actions UI
# https://github.com/tamatebako/dwarfs/actions
```

**Acceptance Criteria**:
- ✅ All platform builds succeed
- ✅ All tests pass (>99%)
- ✅ Artifacts generated successfully
- ✅ No critical warnings or errors

---

### Phase 4: Documentation Finalization (Priority: MEDIUM)

#### 4.1 Archive Temporary Documentation
**Status**: ⏳ Pending
**ETA**: 15 minutes
**Blocking**: No

**Tasks**:
- [ ] Move completed work documentation to `doc/old-docs/v0.16.0-release/`
- [ ] Archive continuation prompts
- [ ] Archive status trackers
- [ ] Keep only reference documentation in `doc/`

**Files to Archive**:
```bash
mkdir -p doc/old-docs/v0.16.0-release

# Move completion summaries
mv doc/FLATBUFFERS_VERIFIER_FIX_STATUS.md doc/old-docs/v0.16.0-release/
mv doc/FLATBUFFERS_VERIFIER_FIX_CONTINUATION_PROMPT.md doc/old-docs/v0.16.0-release/
mv doc/BOTH_FORMAT_EXTRACTION_FIX_COMPLETE.md doc/old-docs/v0.16.0-release/
mv doc/THRIFT_ONLY_BUILD_FIX_COMPLETE.md doc/old-docs/v0.16.0-release/

# Keep reference docs
# - COMPREHENSIVE_BENCHMARK_CONTINUATION_PROMPT.md (for future benchmarks)
# - Architecture docs (memory-bank)
# - Build/usage docs (VCPKG_INSTALLATION.md, etc.)
```

#### 4.2 Final Documentation Review
**Status**: ⏳ Pending
**ETA**: 30 minutes
**Blocking**: No

**Tasks**:
- [ ] Review README.md for accuracy
- [ ] Review CHANGES.md for completeness
- [ ] Verify all code examples work
- [ ] Check all links are valid
- [ ] Ensure file extensions correct (.dff vs .dft)

---

### Phase 5: Release Preparation (Priority: HIGH)

#### 5.1 Tag Release Candidate
**Status**: ⏳ Pending
**ETA**: 15 minutes
**Blocking**: Yes (requires CI/CD pass)

**Prerequisites**:
- ✅ All tests passing
- ✅ CI/CD validation complete
- ✅ Documentation reviewed

**Tasks**:
- [ ] Finalize CHANGES.md with release date
- [ ] Create annotated tag `v0.16.0-rc1`
- [ ] Push tag to trigger release build
- [ ] Monitor release artifact generation

**Commands**:
```bash
# Update CHANGES.md with date
sed -i 's/2025-12-XX/2025-12-10/' CHANGES.md

# Create RC tag
git add CHANGES.md
git commit -m "docs: finalize v0.16.0-rc1 release notes"
git tag -a v0.16.0-rc1 -m "Release Candidate 1 for v0.16.0

Critical fixes:
- FlatBuffers metadata verifier for large repositories
- Tool modularization (mkdwarfs, dwarfs)
- FUSE-T support on macOS

See CHANGES.md for full details."

# Push tag
git push origin v0.16.0-rc1
```

#### 5.2 Final Platform Testing
**Status**: ⏳ Pending
**ETA**: 2-3 days
**Blocking**: Yes (for stable release)

**Test Matrix**:
- [ ] Linux: Ubuntu, Fedora, Debian (create + mount + extract)
- [ ] macOS: FUSE-T and macFUSE (mount + extract)
- [ ] Windows: WinFsp (mount + extract)
- [ ] FreeBSD: Linux emulation (mount + extract)

**Acceptance Criteria**:
- ✅ All platforms can create images
- ✅ All platforms can mount images
- ✅ All platforms can extract images
- ✅ No platform-specific crashes or errors

#### 5.3 Stable Release
**Status**: ⏳ Pending
**ETA**: 1 hour
**Blocking**: No (after testing complete)

**Tasks**:
- [ ] Create final tag `v0.16.0`
- [ ] Generate release notes
- [ ] Upload artifacts to GitHub Releases
- [ ] Announce release
- [ ] Update vcpkg ports

**Commands**:
```bash
# Create stable tag
git tag -a v0.16.0 -m "DwarFS v0.16.0 Stable Release

Major changes:
- FlatBuffers as default metadata format
- Critical fix for large repository support
- Comprehensive tool refactoring
- macOS FUSE-T support

See CHANGES.md for complete changelog."

# Push
git push origin v0.16.0
```

---

## Risk Assessment

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| Large image validation fails | LOW | MEDIUM | Fix is validated on small images; large failure would just delay testing |
| Benchmark regressions | LOW | LOW | Accept <10% regressions as acceptable |
| CI/CD platform failures | MEDIUM | HIGH | Monitor closely, fix platform-specific issues |
| Documentation gaps | LOW | LOW | User feedback will identify gaps post-release |

---

## Success Criteria

### Minimum for RC1 Tag
- [x] FlatBuffers fix validated (small images)
- [x] Documentation updated (README.md + CHANGES.md)
- [ ] CI/CD passing on all platforms
- [ ] No critical test failures

### Recommended for RC1 Tag
- [ ] Large image validation complete
- [ ] Quick validation suite passing
- [ ] Benchmark suite run (preliminary results)

### Required for Stable Release
- [ ] RC1 tested on Linux/macOS/Windows
- [ ] No critical bugs reported in RC1
- [ ] Full benchmark suite complete
- [ ] All documentation reviewed

---

## Timeline

| Phase | Duration | Start | Complete |
|-------|----------|-------|----------|
| **Documentation** | 1 hour | 2025-12-08 10:40 | ✅ 2025-12-08 11:40 |
| Validation | 2 hours | 2025-12-09 | 2025-12-09 |
| Benchmarking | 4 hours | 2025-12-09 | 2025-12-10 |
| CI/CD | 2 hours | 2025-12-10 | 2025-12-10 |
| **RC1 Tag** | 1 hour | 2025-12-10 | 2025-12-10 |
| Platform Testing | 3 days | 2025-12-11 | 2025-12-13 |
| **Stable Release** | 1 hour | 2025-12-15 | 2025-12-15 |

**Total**: ~7 days from documentation complete to stable release

---

## Next Session Priority

**Immediate Actions** (can start independently):
1. Large image validation (if source data available)
2. Quick validation suite (all 3 build configs)
3. Trigger CI/CD workflow

**Can Start in Parallel**:
- Benchmark suite (while CI/CD runs)
- Documentation archival (while validations run)

**Blocked Until**:
- RC1 tag: Requires CI/CD pass
- Stable release: Requires platform testing complete

---

**Last Updated**: 2025-12-08 11:40 HKT
**Next Review**: After CI/CD validation
**Owner**: Release Engineering Team