# Homebrew Compatibility Testing - Implementation Roadmap

## Overview

This roadmap outlines the implementation of Homebrew compatibility testing for the Legacy Thrift metadata format.

## Phases

### Phase 1: Foundation (Foundation)

**Goal:** Create the core infrastructure and configuration system.

#### 1.1 Configuration System
- [ ] Create `test/compat/config.yaml` with all configuration options
- [ ] Document configuration options in README
- [ ] Create YAML parser for config file

**Deliverables:**
- `test/compat/config.yaml`
- Configuration parser in C++ or shell

**Dependencies:** None

---

#### 1.2 Directory Structure
- [ ] Create `test/compat/fixtures/` (gitignore'd)
- [ ] Create `test/compat/scripts/`
- [ ] Create `test/compat/lib/`
- [ ] Create `test/compat/tests/`
- [ ] Create `test/compat/expected/v0.14.1/`
- [ ] Add `.gitignore` for fixtures directory

**Deliverables:**
- Complete directory structure
- `.gitignore` entry

**Dependencies:** None

---

#### 1.3 Homebrew Detector Class
- [ ] Create `test/compat/lib/homebrew_detector.h`
- [ ] Implement `test/compat/lib/homebrew_detector.cpp`
- [ ] Detect platform (macOS/Linux)
- [ ] Detect architecture (arm64/x86_64)
- [ ] Find Homebrew installation path
- [ ] Find mkdwarfs/dwarfs binaries
- [ ] Determine dwarfs version
- [ ] Write unit tests for detection logic

**Deliverables:**
- `test/compat/lib/homebrew_detector.h`
- `test/compat/lib/homebrew_detector.cpp`
- Unit tests

**Dependencies:** None

---

#### 1.4 Makefile Integration
- [ ] Add `test-compat` target to Makefile
- [ ] Add `check-homebrew` target
- [ ] Add `generate-fixtures` target
- [ ] Add `clean-fixtures` target
- [ ] Add `test-compat-read` target
- [ ] Add `test-compat-write` target
- [ ] Add `test-compat-full` target
- [ ] Add `verify-compatibility` target

**Deliverables:**
- Updated Makefile

**Dependencies:** 1.2, 1.3

---

### Phase 2: Core Components

**Goal:** Implement the core C++ classes for fixture management.

#### 2.1 Fixture Generator Class
- [ ] Create `test/compat/lib/fixture_generator.h`
- [ ] Implement `test/compat/lib/fixture_generator.cpp`
- [ ] Define `FixtureSpec` structure
- [ ] Implement test metadata generation
- [ ] Implement mkdwarfs invocation
- [ ] Implement fixture naming
- [ ] Handle platform-specific paths
- [ ] Write unit tests

**Deliverables:**
- `test/compat/lib/fixture_generator.h`
- `test/compat/lib/fixture_generator.cpp`
- Unit tests

**Dependencies:** 1.1, 1.3

---

#### 2.2 Fixture Cache Class
- [ ] Create `test/compat/lib/fixture_cache.h`
- [ ] Implement `test/compat/lib/fixture_cache.cpp`
- [ ] Implement `has_valid_fixture()` method
- [ ] Implement `load()` method
- [ ] Implement `store()` method
- [ ] Implement checksum validation
- [ ] Implement cache invalidation
- [ ] Write unit tests

**Deliverables:**
- `test/compat/lib/fixture_cache.h`
- `test/compat/lib/fixture_cache.cpp`
- Unit tests

**Dependencies:** 1.1, 2.1

---

#### 2.3 Compatibility Tester Class
- [ ] Create `test/compat/lib/compatibility_tester.h`
- [ ] Implement `test/compat/lib/compatibility_tester.cpp`
- [ ] Implement `test_read_homebrew_file()` method
- [ ] Implement `test_write_compatible_file()` method
- [ ] Implement `test_round_trip()` method
- [ ] Add detailed error reporting
- [ ] Write unit tests

**Deliverables:**
- `test/compat/lib/compatibility_tester.h`
- `test/compat/lib/compatibility_tester.cpp`
- Unit tests

**Dependencies:** 1.1, 2.1, 2.2

---

### Phase 3: Scripts and Orchestration

**Goal:** Create the orchestration scripts that tie everything together.

#### 3.1 Check Homebrew Script
- [ ] Create `test/compat/scripts/check_homebrew.sh`
- [ ] Detect if Homebrew is installed
- [ ] Detect if dwarfs is installed
- [ ] Prompt to install if missing
- [ ] Support both macOS and Linux
- [ ] Return appropriate exit codes

**Deliverables:**
- `test/compat/scripts/check_homebrew.sh`

**Dependencies:** 1.3

---

#### 3.2 Generate Fixtures Script
- [ ] Create `test/compat/scripts/generate_fixtures.sh`
- [ ] Read config.yaml for versions to test
- [ ] Call FixtureGenerator for each version
- [ ] Validate generated fixtures
- [ ] Store fixtures with correct naming
- [ ] Report progress
- [ ] Handle errors gracefully

**Deliverables:**
- `test/compat/scripts/generate_fixtures.sh`

**Dependencies:** 1.1, 2.1, 3.1

---

#### 3.3 Run Tests Script
- [ ] Create `test/compat/scripts/run_tests.sh`
- [ ] Check if fixtures exist, generate if not
- [ ] Run all configured tests
- [ ] Collect results
- [ ] Report results in table format
- [ ] Support individual test types
- [ ] Support verbose mode

**Deliverables:**
- `test/compat/scripts/run_tests.sh`

**Dependencies:** 1.1, 2.3, 3.2

---

#### 3.4 Validate Fixtures Script
- [ ] Create `test/compat/scripts/validate_fixtures.sh`
- [ ] Check fixture checksums
- [ ] Validate fixture format
- [ ] Detect corrupted fixtures
- [ ] Report invalid fixtures

**Deliverables:**
- `test/compat/scripts/validate_fixtures.sh`

**Dependencies:** 2.2

---

### Phase 4: Test Implementation

**Goal:** Write the actual GTest compatibility tests.

#### 4.1 Read Homebrew Files Test
- [ ] Create `test/compat/tests/read_homebrew_files_test.cpp`
- [ ] Test reading DFT files from Homebrew 0.14.1
- [ ] Test reading DFT files from Homebrew latest
- [ ] Verify metadata integrity
- [ ] Verify all file types (regular, dir, symlink)
- [ ] Test error handling for corrupted files

**Deliverables:**
- `test/compat/tests/read_homebrew_files_test.cpp`

**Dependencies:** 2.3, 3.2

---

#### 4.2 Write Compatible Files Test
- [ ] Create `test/compat/tests/write_compatible_files_test.cpp`
- [ ] Test writing DFT files
- [ ] Verify Homebrew can read our DFT files
- [ ] Test all metadata types
- [ ] Test edge cases (empty files, large files)
- [ ] Verify format compatibility

**Deliverables:**
- `test/compat/tests/write_compatible_files_test.cpp`

**Dependencies:** 2.3, 3.2

---

#### 4.3 Round Trip Test
- [ ] Create `test/compat/tests/round_trip_test.cpp`
- [ ] Test serialize -> Homebrew read
- [ ] Test Homebrew write -> deserialize
- [ ] Test full round trip
- [ ] Verify data integrity
- [ ] Test with complex metadata

**Deliverables:**
- `test/compat/tests/round_trip_test.cpp`

**Dependencies:** 2.3, 3.2

---

#### 4.4 CMake Integration
- [ ] Add compat test to CMakeLists.txt
- [ ] Create `compat_tests` target
- [ ] Link with required libraries
- [ ] Set test fixtures path
- [ ] Configure test discovery

**Deliverables:**
- Updated CMakeLists.txt

**Dependencies:** 4.1, 4.2, 4.3

---

### Phase 5: CI/CD Integration

**Goal:** Integrate with GitHub Actions for automated testing.

#### 5.1 GitHub Actions Workflow
- [ ] Create `.github/workflows/compat-test.yml`
- [ ] Set up matrix strategy (platform x version)
- [ ] Configure macOS build
- [ ] Configure Linux build
- [ ] Add artifact upload
- [ ] Add test result reporting
- [ ] Add manual trigger option
- [ ] Add scheduled runs

**Deliverables:**
- `.github/workflows/compat-test.yml`

**Dependencies:** 4.4

---

#### 5.2 Setup Homebrew dwarfs Action
- [ ] Create `.github/actions/setup-homebrew-dwarfs/action.yaml`
- [ ] Create `.github/actions/setup-homebrew-dwarfs/install.sh`
- [ ] Support macOS installation
- [ ] Support Linuxbrew installation
- [ ] Support specific versions
- [ ] Return mkdwarfs path as output
- [ ] Handle errors gracefully

**Deliverables:**
- `.github/actions/setup-homebrew-dwarfs/action.yaml`
- `.github/actions/setup-homebrew-dwarfs/install.sh`

**Dependencies:** None

---

#### 5.3 CI Test Verification
- [ ] Run workflow on macOS
- [ ] Run workflow on Ubuntu
- [ ] Verify all tests pass
- [ ] Verify artifacts are uploaded
- [ ] Verify manual trigger works
- [ ] Verify scheduled runs work

**Deliverables:**
- Working CI workflows

**Dependencies:** 5.1, 5.2

---

### Phase 6: Documentation and Polish

**Goal:** Complete documentation and user experience.

#### 6.1 Documentation
- [ ] Complete README.md
- [ ] Document configuration options
- [ ] Document troubleshooting
- [ ] Document expected test results
- [ ] Add examples

**Deliverables:**
- Complete README.md

**Dependencies:** All previous phases

---

#### 6.2 Compatibility Report Generation
- [ ] Create `scripts/generate_compat_report.sh`
- [ ] Generate markdown table
- [ ] Generate JSON results
- [ ] Generate fixture checksums
- [ ] Add to Makefile

**Deliverables:**
- `scripts/generate_compat_report.sh`
- Updated Makefile

**Dependencies:** 5.3

---

#### 6.3 Final Verification
- [ ] Run tests locally on macOS arm64
- [ ] Run tests locally on macOS x86_64
- [ ] Run tests locally on Linux (if available)
- [ ] Verify CI passes on all platforms
- [ ] Fix any remaining issues
- [ ] Update documentation

**Deliverables:**
- All tests passing
- Complete documentation

**Dependencies:** All previous phases

---

## Dependencies Graph

```
Phase 1: Foundation
├── 1.1 Configuration ────────┐
├── 1.2 Directory Structure  ├──> Phase 4: CMake
├── 1.3 HomebrewDetector ─────┤
└── 1.4 Makefile ─────────────┘

Phase 2: Core Components
├── 2.1 FixtureGenerator ──> 3.2 Generate Fixtures
├── 2.2 FixtureCache ───────> 3.4 Validate Fixtures
└── 2.3 CompatibilityTester ─> 3.3 Run Tests

Phase 3: Scripts
├── 3.1 Check Homebrew ──────> 3.2 Generate Fixtures
├── 3.2 Generate Fixtures ───> 3.3 Run Tests
├── 3.3 Run Tests ───────────> Phase 4: Tests
└── 3.4 Validate Fixtures ───> (standalone)

Phase 4: Tests
├── 4.1 Read Tests ────> 4.4 CMake Integration
├── 4.2 Write Tests ───> 4.4 CMake Integration
├── 4.3 Round Trip ────> 4.4 CMake Integration
└── 4.4 CMake ──────────> Phase 5: CI

Phase 5: CI/CD
├── 5.1 Workflow ────────> 5.3 Verification
├── 5.2 Composite Action > 5.3 Verification
└── 5.3 Verification ───> Phase 6: Polish

Phase 6: Documentation
├── 6.1 Documentation ────> Final
├── 6.2 Reports ──────────> Final
└── 6.3 Verification ─────> Final
```

---

## Timeline Estimate

- **Phase 1 (Foundation):** 1-2 days
- **Phase 2 (Core Components):** 2-3 days
- **Phase 3 (Scripts):** 1-2 days
- **Phase 4 (Tests):** 2-3 days
- **Phase 5 (CI/CD):** 1-2 days
- **Phase 6 (Documentation):** 1 day

**Total:** 8-13 days

---

## Success Criteria

A phase is considered complete when:
- [ ] All checkboxes are checked
- [ ] All tests pass
- [ ] Code is reviewed
- [ ] Documentation is updated

The overall project is complete when all 6 phases are complete and all success criteria are met.
