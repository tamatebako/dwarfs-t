# Tebako DwarFS v0.17.0 Pre-Release Checklist

**Last Updated:** 2026-01-14
**Target Release:** v0.17.0 (First Tebako DwarFS Release)
**Status:** 🟡 In Progress

---

## Overview

This checklist ensures v0.17.0 is production-ready across all dimensions: technical completeness, user experience, distribution, and post-release support.

**Release Blockers:** Critical items that MUST pass before release
**Release Warnings:** Important items that should pass but can be documented as known issues

---

## Section 1: Technical Validation

### 1.1 Metadata Format Compatibility ✅

**Status:** Partially Complete

- [x] **FlatBuffers** (Default)
  - [x] Serialization/deserialization working
  - [x] Format detection via magic bytes (`DFBF`)
  - [x] Test coverage: 100% pass rate
  - [ ] Document performance characteristics in README

- [ ] **Legacy Thrift** (Hand-coded, always available)
  - [ ] **CRITICAL:** Write test suite for encoding/decoding Legacy Thrift format
  - [ ] **CRITICAL:** Test interoperability with Homebrew `dwarfs` v0.14.1_3
    - [ ] Read Homebrew-generated DFT files
    - [ ] Write DFT files readable by Homebrew `dwarfs`
  - [ ] Create test fixtures using Homebrew tool at `/opt/homebrew/Cellar/dwarfs/0.14.1_3`
  - [ ] Add tests to existing test framework

- [x] **Modern Thrift** (fbthrift, optional)
  - [x] Serialization/deserialization working
  - [x] Format detection via magic bytes (`{0x82, 0x21}`)
  - [x] Test coverage: 15/15 tests PASSED
  - [x] vcpkg overlay ports complete

**Validation Commands:**
```bash
# Test Legacy Thrift interoperability
./build/dwarfsck -i /path/to/homebrew/image.dft
./build/mkdwarfs --format=thrift -i /test/data -o test-output.dft
/opt/homebrew/Cellar/dwarfs/0.14.1_3/bin/dwarfsck -i test-output.dft
```

---

### 1.2 Build Configurations

**Status:** Needs Testing

Test both build configurations on all supported platforms:

- [ ] **Default Build** (FlatBuffers + Legacy Thrift)
  ```bash
  cmake -B build -GNinja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
  ninja -C build
  ```

- [ ] **Thrift-Enabled Build** (FlatBuffers + Legacy Thrift + Modern Thrift)
  ```bash
  cmake -B build-thrift -GNinja \
    -DCMAKE_BUILD_TYPE=Release \
    -DDWARFS_WITH_THRIFT=ON \
    -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
  ninja -C build-thrift
  ```

- [ ] Verify both build configurations produce working executables
- [ ] Test all four tools: `mkdwarfs`, `dwarfs`, `dwarfsck`, `dwarfsextract`

---

### 1.3 Benchmarking

**Status:** Needs Execution

Run comprehensive benchmarks and update README with results:

- [ ] **Execute benchmark suite:**
  ```bash
  cd scripts
  ./benchmark-all.sh
  ```

- [ ] **Benchmark all 3 metadata formats:**
  - FlatBuffers (default)
  - Legacy Thrift
  - Modern Thrift

- [ ] **Benchmark operations:**
  - Creation (`mkdwarfs`)
  - Reading/mounting (`dwarfs`)
  - Extraction (`dwarfsextract`)
  - Listing (`dwarfsck --list`)
  - Random access (single file, multiple files)
  - Sequential access (whole archive)

- [ ] **Document results:**
  - [ ] Update `README.md` with benchmark numbers
  - [ ] Add comparison tables to documentation
  - [ ] Document performance trade-offs between formats

**Acceptance Criteria:** All benchmarks complete successfully with reasonable performance

---

### 1.4 Example Application

**Status:** Needs Testing

- [ ] **Test static-site-server example:**
  ```bash
  cd example/static-site-server
  ./build.sh
  ./test.sh
  ```

- [ ] Verify HTTP server serves files correctly
- [ ] Test with all 3 metadata formats
- [ ] Document any known issues

**Acceptance Criteria:** Example builds and runs without errors

---

### 1.5 Build Scripts

**Status:** Needs Validation

- [ ] **Test build-all-and-test.sh:**
  ```bash
  ./scripts/build-all-and-test.sh --vcpkg
  ```

- [ ] **Clean up scripts/ and benchmark/ directories:**
  - [ ] Ensure scripts are MECE (mutually exclusive, collectively exhaustive)
  - [ ] Remove redundant or obsolete scripts
  - [ ] Document script dependencies and requirements
  - [ ] Add usage examples to script headers

**Acceptance Criteria:** All scripts work, well-documented, no redundancy

---

### 1.6 Platform Triplets

**Status:** Needs CI Validation

Verify support for all platforms (test on CI):

**Windows:**
- [ ] `arm64-windows-static`
- [ ] `arm64-windows-dynamic`
- [ ] `x64-windows-static`
- [ ] `x64-windows-dynamic`
- [ ] `arm64-mingw-static`
- [ ] `arm64-mingw-dynamic`
- [ ] `x64-mingw-static`
- [ ] `x64-mingw-dynamic`

**macOS:**
- [ ] `arm64-osx-static`
- [ ] `arm64-osx-dynamic`
- [ ] `x64-osx-static`
- [ ] `x64-osx-dynamic`

**Linux:**
- [ ] `arm64-linux-static`
- [ ] `arm64-linux-dynamic`
- [ ] `x64-linux-static`
- [ ] `x64-linux-dynamic`

**Acceptance Criteria:** All platforms build and pass tests on CI

---

### 1.7 Code Organization

**Status:** Needs Work

- [ ] **Clean up directory structure** (reference: `/Users/mulgogi/src/external/xz`)
  - [ ] Remove obsolete files and directories
  - [ ] Organize headers logically
  - [ ] Consolidate scattered documentation
  - [ ] Ensure consistent naming conventions

- [ ] **GitHub Workflows DRY:**
  - [ ] Convert to reusable workflows
  - [ ] Create composite actions for common operations
  - [ ] Ensure all workflows use CMake + vcpkg overlay ports consistently
  - [ ] Document workflow architecture

**Acceptance Criteria:** Clean structure, DRY workflows, easy to maintain

---

## Section 2: Release Artifacts

### 2.1 Version Management

- [ ] **Finalize version number:** Confirm `0.17.0`
- [ ] **Update version in all locations:**
  - [ ] `CMakeLists.txt` (project version)
  - [ ] `CHANGES.md` (version header)
  - [ ] `README.md` (version references)
  - [ ] Man pages (version strings)
- [ ] **Create Git tag:** `v0.17.0`
- [ ] **Sign release:** GPG signature for release tarball

---

### 2.2 Changelog Finalization

- [ ] **Review `CHANGES.md` for completeness:**
  - [ ] All new features documented
  - [ ] All bug fixes listed
  - [ ] Breaking changes highlighted
  - [ ] Migration notes included

- [ ] **Add release date**
- [ ] **Add download links/locations**
- [ ] **Verify upgrade path from upstream DwarFS documented**

---

### 2.3 Release Packaging

- [ ] **Create release tarball:**
  - [ ] Source tarball (`.tar.gz`, `.tar.xz`)
  - [ ] Include all necessary files
  - [ ] Exclude build artifacts, `.git`, test data

- [ ] **Build release binaries:**
  - [ ] macOS (universal binary: arm64 + x64)
  - [ ] Windows (x64 + arm64)
  - [ ] Linux (x64_64, arm64)

- [ ] **Generate checksums:**
  - [ ] SHA256 checksums for all artifacts
  - [ ] Publish in checksums.txt

---

## Section 3: User-Facing Documentation

### 3.1 README Updates

- [ ] **Update main README.md:**
  - [ ] Add v0.17.0 release announcement banner
  - [ ] Update benchmark numbers (from Section 1.3)
  - [ ] Clarify metadata format options
  - [ ] Add quick start guide for each format
  - [ ] Document performance trade-offs
  - [ ] Update installation instructions

---

### 3.2 Migration Guide

- [ ] **Create MIGRATION.md:**
  - [ ] Upgrading from upstream DwarFS
  - [ ] Switching between metadata formats
  - [ ] Homebrew → Tebako DwarFS transition
  - [ ] Breaking changes and workarounds
  - [ ] Configuration file changes (if any)

---

### 3.3 Breaking Changes Documentation

- [ ] **Document breaking changes (if any):**
  - [ ] CLI flag changes
  - [ ] Metadata format differences
  - [ ] API changes (libdwarfs)
  - [ ] Behavior changes from upstream

- [ ] **Provide migration paths** for each breaking change
- [ ] **Add deprecation warnings** where applicable

---

### 3.4 Known Issues

- [ ] **Create KNOWN_ISSUES.md:**
  - [ ] List all known issues with workarounds
  - [ ] Categorize by severity (critical, major, minor)
  - [ ] Link to tracking issues
  - [ ] Document which platforms/features are affected

- [ ] **Review STATUS_REPORT.md** and incorporate issues

---

### 3.5 API/ABI Documentation

- [ ] **Update libdwarfs API documentation:**
  - [ ] Document all public headers
  - [ ] Add Doxygen comments if missing
  - [ ] Generate API documentation
  - [ ] Document ABI stability guarantees

- [ ] **Document shared library versioning**
- [ ] **Provide example code** for common use cases

---

## Section 4: Distribution Channels

### 4.1 GitHub Release

- [ ] **Create GitHub release:**
  - [ ] Draft release notes
  - [ ] Attach all artifacts (tarballs, binaries, checksums)
  - [ ] Link to changelog
  - [ ] Add installation instructions
  - [ ] Include upgrade guide

- [ ] **Verify release page completeness**
- [ ] **Test download links**

---

### 4.2 Homebrew Formula

- [ ] **Update Homebrew formula:**
  - [ ] Fork or create tap for Tebako DwarFS
  - [ ] Write formula file (`.rb` or `brew` style)
  - [ ] Test formula installation
  - [ ] Submit to homebrew-core or maintain in tap

- [ ] **Document Homebrew installation** in README
- [ ] **Test upgrade from upstream dwarfs**

---

### 4.3 vcpkg Port

- [ ] **Update vcpkg port:**
  - [ ] Submit to vcpkg repository
  - [ ] Or maintain in overlay ports
  - [ ] Test all triplets (from Section 1.6)
  - [ ] Document vcpkg installation

- [ ] **Verify versioning matches** v0.17.0
- [ ] **Test binary caching** if applicable

---

### 4.4 Package Manager Updates

- [ ] **Consider other package managers:**
  - [ ] AUR (Arch Linux)
  - [ ] Scoop (Windows)
  - [ ] Chocolatey (Windows)
  - [ ] Conda (cross-platform)

- [ ] **Document community packaging options**

---

## Section 5: Release Announcement

### 5.1 Release Notes

- [ ] **Write comprehensive release notes:**
  - [ ] Executive summary (what, why, who should care)
  - [ ] Key features overview
  - [ ] Performance benchmarks
  - [ ] Breaking changes
  - [ ] Known limitations
  - [ ] Future roadmap

- [ ] **Keep notes user-focused** (not developer-focused)
- [ ] **Include diagrams/screenshots** where helpful

---

### 5.2 Announcement Channels

- [ ] **Prepare announcements for:**
  - [ ] GitHub Discussions
  - [ ] Reddit (r/cpp, r/linux, r/programming)
  - [ ] Hacker News
  - [ ] Twitter/X
  - [ ] LinkedIn
  - [ ] Project website/blog

- [ ] **Coordinate announcement timing** across channels
- [ ] **Prepare FAQ** for common questions

---

### 5.3 Demo/Examples

- [ ] **Create demo content:**
  - [ ] Video tutorial (optional)
  - [ ] Example use cases
  - [ ] Performance comparison charts
  - [ ] Before/after compression examples

- [ ] **Link demos from README**

---

## Section 6: Security & Compliance

### 6.1 Security Audit

- [ ] **Run security checks:**
  - [ ] Static analysis (cppcheck, clang-tidy)
  - [ ] Dynamic analysis (AddressSanitizer, UndefinedBehaviorSanitizer)
  - [ ] Dependency vulnerability scan
  - [ ] Fuzz testing (if applicable)

- [ ] **Document security posture**
- [ ] **Create SECURITY.md** (vulnerability reporting)

---

### 6.2 License Compliance

- [ ] **Verify all dependencies:**
  - [ ] List all third-party libraries
  - [ ] Verify licenses are compatible
  - [ ] Add license attributions where required
  - [ ] Update LICENSE files

- [ ] **Check for proprietary code** (should be none)
- [ ] **Verify SPDX identifiers** where applicable

---

### 6.3 Data Privacy

- [ ] **Verify no telemetry/phoning home** (unless documented)
- [ ] **Check for hardcoded credentials/keys**
- [ ] **Document data collection** (if any)

---

## Section 7: Post-Release Planning

### 7.1 Monitoring Setup

- [ ] **Set up issue tracking:**
  - [ ] Create GitHub labels for v0.17.0 issues
  - [ ] Create issue templates
  - [ ] Set up triage process

- [ ] **Prepare for bug reports:**
  - [ ] Document debug output collection
  - [ ] Create issue checklist template
  - [ ] Prepare common troubleshooting steps

---

### 7.2 Regression Detection

- [ ] **Set up continuous benchmarking:**
  - [ ] Track performance over time
  - [ ] Alert on regressions
  - [ ] Compare against v0.17.0 baseline

- [ ] **Track test coverage trends**
- [ ] **Monitor crash reports** (if telemetry enabled)

---

### 7.3 Documentation Maintenance

- [ ] **Create documentation maintenance schedule:**
  - [ ] Regular review cycle
  - [ ] Update process for new releases
  - [ ] Archive outdated docs

- [ ] **Assign documentation owners**

---

### 7.4 Roadmap Planning

- [ ] **Document v0.18.0 roadmap:**
  - [ ] Prioritized feature list
  - [ ] Technical debt items
  - [ ] Platform expansion goals

- [ ] **Create timeline estimates**
- [ ] **Link to roadmap from README**

---

## Section 8: Sign-Off Checklist

### 8.1 Technical Sign-Off

- [ ] **All tests pass** on all platforms (Section 1.6)
- [ ] **Benchmarks complete** and documented (Section 1.3)
- [ ] **No critical bugs** remaining
- [ ] **No memory leaks** detected
- [ ] **No security vulnerabilities** known

### 8.2 Documentation Sign-Off

- [ ] **All docs updated** for v0.17.0
- [ ] **README tested** by fresh user
- [ ] **Migration guide reviewed** by external user
- [ ] **API docs generated** successfully

### 8.3 Release Sign-Off

- [ ] **Release artifacts prepared** (Section 2.3)
- [ ] **Changelog finalized** (Section 2.2)
- [ ] **Announcement drafted** (Section 5.1)
- [ ] **Distribution channels ready** (Section 4)

### 8.4 Final Release Approval

- [ ] **Project lead approval:** _______________
- [ ] **Tech lead approval:** _______________
- [ ] **Documentation approval:** _______________
- [ ] **Security approval:** _______________
- [ ] **Release date:** _______________

---

## Progress Tracking

**Overall Progress:** 15% (estimate)

| Section | Progress | Blockers |
|---------|----------|----------|
| 1. Technical Validation | 40% | Legacy Thrift tests |
| 2. Release Artifacts | 0% | Pending technical validation |
| 3. Documentation | 20% | Benchmark results, migration guide |
| 4. Distribution | 0% | Pending release artifacts |
| 5. Announcement | 0% | Pending all above |
| 6. Security | 0% | Pending technical validation |
| 7. Post-Release | 0% | Pending release |
| 8. Sign-Off | 0% | Pending all above |

---

## Quick Reference Commands

```bash
# Full validation (run all sections)
./scripts/validate-release.sh

# Generate release artifacts
./scripts/generate-release.sh 0.17.0

# Run full test suite
./scripts/build-all-and-test.sh --vcpkg

# Benchmark all formats
./scripts/benchmark-all.sh

# Test Homebrew interoperability
./scripts/test-homebrew-compat.sh
```

---

## Notes

- This checklist is a living document - update as items are completed
- Add new items as they're discovered
- Mark critical blockers clearly
- Keep progress tracking updated

**Next Review:** After completing Section 1 (Technical Validation)
