# Session 61: vcpkg Port Architecture Plan

**Purpose**: Design a comprehensive, maintainable vcpkg port strategy for Folly/fbthrift/wangle dependencies
**Status**: 📋 **PLANNING** - Ready for implementation
**Created**: 2025-12-31 18:35 HKT

---

## Problem Statement

DwarFS requires Thrift support for backward compatibility with v0.14.1, which depends on:
- Folly (core library)
- fizz (TLS library)
- wangle (async C++ framework)
- fbthrift (Thrift implementation)

These dependencies have complex version compatibility requirements that current vcpkg overlay ports don't satisfy.

---

## Architecture Principles

### 1. Version Pinning Strategy

**PRINCIPLE**: All dependencies MUST use **stable, tagged releases** on the same date

**Rationale**:
- Facebook releases Folly/fizz/wangle on coordinated dates
- Same-date releases are tested for compatibility
- Avoids cascading version issues

**Implementation**:
```cmake
# All components use v2025.12.29.00
folly:   facebook/folly   v2025.12.29.00
fizz:    facebook/fizz    v2025.12.29.00
wangle:  facebook/wangle  v2025.12.29.00
fbthrift: facebook/fbthrift v2025.12.29.00
```

### 2. Official Sources Over Forks

**PRINCIPLE**: Use official Facebook repositories, not mhx forks (unless necessary)

**Rationale**:
- Official repos have better maintenance
- Forks may lag behind or diverge
- Community support for official versions

**Exception**: If mhx fork has critical patches, document why

### 3. Minimal Patching

**PRINCIPLE**: Patches should only address:
- Build system issues (CMake, install paths)
- Unnecessary components (tests, benchmarks)
- Platform-specific quirks

**Anti-pattern**: Patching source code for version compatibility (use correct versions instead)

### 4. Testable Ports

**PRINCIPLE**: Each port should be independently testable

**Implementation**:
- Separate build test for each port
- Version compatibility tests
- Automated CI validation

---

## Proposed Architecture

### Layer 1: Foundation (Folly)

```
┌─────────────────────────────────────┐
│          folly-port                 │
│  facebook/folly v2025.12.29.00     │
│                                     │
│  Features:                          │
│  - Header-only parts (no jemalloc) │
│  - Static linking ready             │
│  - CMake config fixes               │
│                                     │
│  Dependencies:                      │
│  - boost, glog, gflags, etc.        │
└─────────────────────────────────────┘
```

**Portfile**: `vcpkg_ports/folly/portfile.cmake`
**Status**: ✅ WORKING (Session 60 v15)
**Build Time**: ~60 seconds

### Layer 2: Network Libraries (fizz, wangle)

```
┌──────────────────┐  ┌──────────────────┐
│    fizz-port     │  │   wangle-port    │
│  v2025.12.29.00  │  │  v2025.12.29.00  │
│                  │  │                  │
│  Depends on:     │  │  Depends on:     │
│  - folly         │  │  - folly         │
│  - OpenSSL       │  │  - fizz          │
└──────────────────┘  └──────────────────┘
```

**fizz**:
- Status: ✅ WORKING (auto-upgraded when Folly upgraded)
- Build Time: ~18 seconds
- No patches needed

**wangle**:
- Status: ✅ WORKING (Session 60 v5)
- Build Time: ~46 seconds
- Patches: fix-tfo-typo, fix-cmake-config

### Layer 3: Thrift (fbthrift)

```
┌─────────────────────────────────────┐
│        fbthrift-port                │
│  facebook/fbthrift v2025.12.29.00  │
│                                     │
│  Features:                          │
│  - Compiler only (no Python)        │
│  - Library (no benchmarks/tests)    │
│  - Static linking                   │
│                                     │
│  Depends on:                        │
│  - folly, fizz, wangle             │
│  - glog, gflags, boost             │
└─────────────────────────────────────┘
```

**Status**: 🔴 BLOCKED (Session 60 v15)
**Issue**: Using mhx/fbthrift main (rolling) instead of stable release
**Fix**: Switch to facebook/fbthrift v2025.12.29.00

---

## Implementation Plan

### Phase 1: Research Compatible Versions (30 min)

**Task 1.1**: Check facebook/fbthrift releases
```bash
curl -s https://api.github.com/repos/facebook/fbthrift/tags | \
  jq -r '.[].name' | head -20
```

**Task 1.2**: Verify Homebrew versions
```bash
brew info folly fbthrift wangle | grep -E "stable|Revision"
```

**Task 1.3**: Check fizz/wangle dates
```bash
# fizz
curl -s https://api.github.com/repos/facebookincubator/fizz/tags | \
  jq -r '.[].name' | grep "2025.12.29"

# wangle
curl -s https://api.github.com/repos/facebook/wangle/tags | \
  jq -r '.[].name' | grep "2025.12.29"
```

**Deliverable**: Version compatibility matrix

| Component | Latest Release | Homebrew Version | Recommended |
|-----------|----------------|------------------|-------------|
| folly | v2025.12.29.00 | ? | v2025.12.29.00 |
| fizz | ? | ? | (match folly date) |
| wangle | v2025.05.19.00 | ? | (match folly date) |
| fbthrift | ? | ? | (match folly date) |

### Phase 2: Update fbthrift Port (1 hour)

**Task 2.1**: Update to official facebook/fbthrift release

**Current**:
```cmake
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO mhx/fbthrift
    REF main  # ❌ Rolling branch
    ...
)
```

**Proposed**:
```cmake
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO facebook/fbthrift
    REF v2025.12.29.00  # ✅ Stable tag
    SHA512 <TO_BE_CALCULATED>
    HEAD_REF main
    PATCHES
        no-benchmarks.patch
        disable-python.patch  # May need this
)
```

**Task 2.2**: Calculate SHA512
```bash
curl -sL https://github.com/facebook/fbthrift/archive/refs/tags/v2025.12.29.00.tar.gz | \
  shasum -a 512
```

**Task 2.3**: Test compatibility
```bash
cmake -B build-test-fbthrift -GNinja \
  -DCMAKE_TOOLCHAIN_FILE=<vcpkg> \
  -DVCPKG_OVERLAY_PORTS=<ports> \
  -DDWARFS_WITH_THRIFT=ON
```

**Deliverable**: Working fbthrift port with stable release

### Phase 3: Align All Versions (30 min)

**Task 3.1**: If v2025.12.29.00 doesn't exist for all, find common date

**Strategy**:
1. List all tags for each component
2. Find intersection of release dates
3. Pick most recent common date
4. Update all ports to that date

**Task 3.2**: Update version matrix in documentation

**Deliverable**: All 4 components on compatible versions

### Phase 4: Test Suite (1 hour)

**Task 4.1**: Create port test script
```bash
#!/bin/bash
# test-vcpkg-ports.sh

for PORT in folly fizz wangle fbthrift; do
  echo "Testing $PORT..."
  vcpkg remove $PORT:arm64-osx --recurse
  vcpkg install $PORT:arm64-osx \
    --overlay-ports=vcpkg_ports \
    --triplet=arm64-osx

  if [ $? -eq 0 ]; then
    echo "✅ $PORT built successfully"
  else
    echo "❌ $PORT failed"
    exit 1
  fi
done
```

**Task 4.2**: Integrate into CI/CD (future)

**Deliverable**: Automated port testing

---

## Success Criteria

### Phase 1-3: vcpkg Ports
- [ ] All 4 components use stable, tagged releases
- [ ] All components built successfully
- [ ] Version compatibility documented
- [ ] No macro compatibility errors

### Phase 4: Homebrew Testing
- [ ] dwarfs builds with Homebrew Folly/Thrift
- [ ] All converter round-trip tests pass
- [ ] Homebrew v0.14.1 compatibility verified
- [ ] Bugs in `cpp_thrift_converter.cpp` fixed

### Documentation
- [ ] Version compatibility matrix complete
- [ ] Build instructions for both paths
- [ ] Migration plan from Homebrew to vcpkg
- [ ] Update memory bank with findings

---

## Risk Assessment

### High Risk
- **fbthrift may not have v2025.12.29.00 tag**
  - Mitigation: Use most recent tag, downgrade other components if needed

- **Official fbthrift may have different API than mhx fork**
  - Mitigation: Compare mhx patches, port to official if needed

### Medium Risk
- **Latest versions may have new requirements**
  - Mitigation: Test incrementally, rollback if needed

### Low Risk
- **Build time increases with latest versions**
  - Acceptable tradeoff for compatibility

---

## Timeline Estimate

| Phase | Task | Time | Dependencies |
|-------|------|------|--------------|
| **Phase 1** | Research versions | 30 min | Internet access |
| **Phase 2** | Update fbthrift | 1 hour | Phase 1 complete |
| **Phase 3** | Align versions | 30 min | Phase 2 complete |
| **Phase 4** | Test suite | 1 hour | Phase 3 complete |
| **Homebrew** | Build & test | 30 min | Can run in parallel |
| **Documentation** | Finalize docs | 30 min | All phases done |
| **TOTAL** | | **4.5 hours** | |

**Critical Path**: Phases 1-3 (2 hours) can run while Homebrew testing proceeds in parallel

---

## Decision Points

### At Phase 1 Completion

**If v2025.12.29.00 exists for all components**:
→ Proceed with Phase 2 (update fbthrift)

**If v2025.12.29.00 missing for some components**:
→ Find most recent common date
→ May need to downgrade Folly

**If no common dates in recent 6 months**:
→ Escalate to user for guidance
→ Consider Option C (match old versions)

### At Phase 2 Completion

**If fbthrift builds successfully**:
→ Proceed with Phase 3 (finalize versions)

**If fbthrift still has errors**:
→ Check if it's the mhx fork issue
→ Try official facebook/fbthrift
→ Document any API differences

### At Phase 4 Completion

**If all ports working**:
→ Full dwarfs build test
→ Update RULE 4 to allow vcpkg
→ Document as primary build method

**If Homebrew testing succeeded but vcpkg still blocked**:
→ Keep Homebrew as temporary solution
→ vcpkg ports marked "experimental"
→ Future session to complete vcpkg work

---

## File Structure

```
vcpkg_ports/
├── folly/
│   ├── portfile.cmake        # v2025.12.29.00 ✅
│   └── vcpkg.json            # Dependencies
├── fizz/                     # (may need if versions change)
│   ├── portfile.cmake
│   └── vcpkg.json
├── wangle/
│   ├── portfile.cmake        # v2025.05.19.00 → v2025.12.29.00?
│   ├── fix-tfo-typo.patch    # ✅
│   ├── fix-cmake-config.patch # ✅
│   └── vcpkg.json
├── fbthrift/
│   ├── portfile.cmake        # To be updated
│   ├── no-benchmarks.patch   # ✅
│   ├── disable-python.patch  # May need
│   └── vcpkg.json
└── README.md                 # Port documentation
```

---

## Testing Strategy

### Unit Tests (Per Port)

**Test**: Each port builds independently
```bash
vcpkg install folly:arm64-osx --overlay-ports=vcpkg_ports
vcpkg install fizz:arm64-osx --overlay-ports=vcpkg_ports
vcpkg install wangle:arm64-osx --overlay-ports=vcpkg_ports
vcpkg install fbthrift:arm64-osx --overlay-ports=vcpkg_ports
```

### Integration Tests (Full Stack)

**Test**: All ports together in dwarfs build
```bash
cmake -B build-vcpkg-full -GNinja \
  -DCMAKE_TOOLCHAIN_FILE=<vcpkg> \
  -DVCPKG_OVERLAY_PORTS=vcpkg_ports \
  -DDWARFS_WITH_THRIFT=ON

ninja -C build-vcpkg-full
```

### Compatibility Tests (dwarfs Functionality)

**Test**: dwarfs tools work correctly
```bash
# Create test image
./build-vcpkg-full/mkdwarfs -i /tmp/test-data -o /tmp/test.dft --format=thrift

# Verify
./build-vcpkg-full/dwarfsck -i /tmp/test.dft

# Extract
./build-vcpkg-full/dwarfsextract -i /tmp/test.dft -o /tmp/extracted

# Compare
diff -r /tmp/test-data /tmp/extracted
```

### Regression Tests (Backward Compatibility)

**Test**: Homebrew v0.14.1 can read our files
```bash
# Create with vcpkg build
./build-vcpkg-full/mkdwarfs -i /tmp/test-data -o /tmp/vcpkg.dft --format=thrift -l1

# Read with Homebrew v0.14.1
/opt/homebrew/bin/dwarfsck -i /tmp/vcpkg.dft
/opt/homebrew/bin/dwarfsextract -i /tmp/vcpkg.dft -o /tmp/hb-extracted

# Verify
diff -r /tmp/test-data /tmp/hb-extracted
```

---

## Migration Path

### Current State
```
Folly:    mhx/folly main (2024.01.15.00)
fizz:     v2025.05.19.00
wangle:   v2025.05.19.00 + patches
fbthrift: mhx/fbthrift main ❌ BROKEN
```

### Target State
```
Folly:    facebook/folly v2025.12.29.00 ✅
fizz:     facebook/fizz v2025.12.29.00
wangle:   facebook/wangle v2025.12.29.00 + patches
fbthrift: facebook/fbthrift v2025.12.29.00 + patches
```

### Migration Steps

1. **Keep current Folly** (v2025.12.29.00) ✅
2. **Check if fizz/wangle have v2025.12.29.00 tags**
   - If yes: Update to match Folly
   - If no: Find common date for all 4
3. **Switch fbthrift to official repo + stable tag**
4. **Test incrementally**: folly → fizz → wangle → fbthrift

---

## Rollback Plan

If vcpkg approach continues to fail after Phase 3:

### Fallback A: Use Homebre w (Temporary)
- Build with Homebrew for testing
- Document as exception
- Complete converter work
- Return to vcpkg in future session

### Fallback B: Submodules (Git)
- Add Folly/fbthrift as git submodules
- Build from source (not vcpkg)
- More control, but slower CI builds

### Fallback C: System Libraries
- Require user to install Folly/fbthrift
- Document installation per platform
- Simpler build, but harder for users

---

## Documentation Updates

### Critical Rules Update

Add to `.kilocode/rules/memory-bank/critical-rules.md`:

```markdown
## RULE 5: vcpkg Version Pinning

**CRITICAL**: vcpkg overlay ports MUST use stable, tagged releases

**NEVER**:
- Use rolling branches (main, master)
- Mix versions from different dates
- Use forks without justification

**ALWAYS**:
- Pin to specific version tags (vYYYY.MM.DD.NN)
- Use official Facebook repos when possible
- Document why any fork is used
- Test version compatibility before committing
```

### Memory Bank Update

Update `.kilocode/rules/memory-bank/context.md`:

```markdown
## Current Status: 🟡 VCPKG ARCHITECTURE IN PROGRESS

### Session 60: vcpkg Build Research (2025-12-31)
- Discovered fundamental version incompatibilities
- Fixed wangle (2 patches working)
- Upgraded Folly to v2025.12.29.00 (working)
- fbthrift blocked on version mismatch

**Decision**: Architect comprehensive vcpkg port strategy

### Session 61: Implementation (Next)
- Research stable fbthrift releases
- Update all ports to compatible versions
- Parallel: Use Homebrew for converter testing
```

---

## API Stability Considerations

### Folly API Changes

**Between 2024.01.15.00 and 2025.12.29.00**:
- Added: Various `FOLLY_*` macros
- Changed: Some deprecation warnings
- Impact: Requires newer C++ standard support

### fbthrift API Changes

**Risk**: mhx fork may have removed features

**Mitigation**:
1. Compare mhx/fbthrift with facebook/fbthrift
2. Identify minimal feature set needed by dwarfs
3. Ensure official version has those features
4. Port any critical mhx patches to official

### dwarfs Compatibility

**Requirement**: Must maintain backward compatibility with Homebrew dwarfs v0.14.1

**Test**:
```bash
# Homebrew → vcpkg
hb_mkdwarfs → vcpkg_dwarfsck ✅
hb_mkdwarfs → vcpkg_dwarfsextract ✅

# vcpkg → Homebrew
vcpkg_mkdwarfs → hb_dwarfsck ✅
vcpkg_mkdwarfs → hb_dwarfsextract ✅
```

---

## Long-term Maintenance

### Quarterly Version Updates

**Schedule**: Every 3 months, check for new stable releases

**Process**:
1. Check for new Folly/fizz/wangle/fbthrift tags
2. Find latest common date
3. Update all ports
4. Run full test suite
5. Update documentation

### CI Integration

**Goal**: Automate vcpkg port testing in GitHub Actions

**Workflow**:
```yaml
name: vcpkg-ports-test
on:
  push:
    paths:
      - 'vcpkg_ports/**'

jobs:
  test-ports:
    runs-on: ${{ matrix.os }}
    strategy:
      matrix:
        os: [ubuntu-latest, macos-latest, windows-latest]
    steps:
      - uses: lukka/run-vcpkg@v11
      - run: vcpkg install fbthrift --overlay-ports=vcpkg_ports
      - run: cmake -B build -DDWARFS_WITH_THRIFT=ON
      - run: ninja -C build && ctest --test-dir build
```

---

## Recommended Solution Strategy

### Option A: Use Stable fbthrift Release (REQUIRED) ⭐

**Approach**: Find a stable, tagged fbthrift release compatible with latest Folly

**Steps**:
1. Check facebook/fbthrift releases: https://github.com/facebook/fbthrift/tags
2. Find latest stable tag (e.g., v2025.12.29.00 if it exists)
3. Update `vcpkg_ports/fbthrift/portfile.cmake` to use facebook/fbthrift with tag
4. Test build compatibility

**Pros**:
- Stable, versioned dependencies
- Predictable behavior
- Better for CI/CD
- Matches fizz/wangle pattern
- **Required for Tebako static linking**

**Cons**:
- Official fbthrift may have features mhx/fbthrift removed
- May need additional patches

**Estimated Time**: 2-3 hours

### Option B: Fix Macro Compatibility (NOT RECOMMENDED)

**Approach**: Add all missing macros via patches/compiler flags

**Required Fixes**:
1. FOLLY_NODISCARD ✅ (done via compiler flag)
2. FOLLY_DETAIL_LITE_TUPLE_ADJUST_WARNINGS ❌ (pending)
3. Unknown additional macros (likely more)

**Pros**:
- Keeps mhx/fbthrift fork
- Minimal changes to fbthrift

**Cons**:
- Whack-a-mole with macros
- Fragile, breaks on updates
- Not sustainable
- **Will likely hit more missing macros**

**Estimated Time**: 4-5 hours (uncertain)

### Option C: Match All Versions to Common Date (FALLBACK)

**Approach**: Find a single date where Folly/fizz/wangle/fbthrift all have compatible releases

**Example**: Use all packages from a common release date
- Folly: vYYYY.MM.DD.NN
- fizz: vYYYY.MM.DD.NN (same date)
- wangle: vYYYY.MM.DD.NN (same date)
- fbthrift: vYYYY.MM.DD.NN (same date)

**Pros**:
- Guaranteed version compatibility (tested together by Facebook)
- Stable snapshot
- **Works with static linking**

**Cons**:
- May not be latest versions
- Need to find common date

**Estimated Time**: 2-3 hours

---

## Recommendation

**PRIMARY**: Option A (stable fbthrift release from facebook/fbthrift)
**FALLBACK**: Option C (find common release date for all 4 components)

**Rationale**:
1. Option A gives us latest stable versions
2. Option C guaranteed to work (Facebook tests compatibility)
3. Both satisfy Tebako static linking requirements
4. No violations of RULE 4

**CRITICAL**: Homebrew is NOT an option - it provides dynamic libraries incompatible with Tebako static builds

---

## Conclusion

**Immediate Action**: Proceed with **Option D** (Homebrew testing) to unblock converter work

**Next Session Goals**:
1. Complete converter bug fixes using Homebrew build
2. Verify Homebrew v0.14.1 compatibility
3. Research and implement **Option A** (stable vcpkg releases) in parallel

**Long-term Vision**: Maintain both build paths:
- **Homebrew**: Mac developer workflow, quick iteration
- **vcpkg**: CI/CD, cross-platform, static linking

---

**Created**: 2025-12-31 18:35 HKT
**Review Required**: Before implementation
**Estimated Total Time**: 4.5 hours (or 30 min if Homebrew-only)

---

## Next Session Starter

**CRITICAL**: Only vcpkg approach is valid due to Tebako static linking requirements

```bash
# Phase 1: Research compatible versions
curl -s https://api.github.com/repos/facebook/fbthrift/tags | jq -r '.[].name' | head -20
curl -s https://api.github.com/repos/facebookincubator/fizz/tags | jq -r '.[].name' | head -20
curl -s https://api.github.com/repos/facebook/wangle/tags | jq -r '.[].name' | head -20

# Find common release date
# Update all vcpkg_ports/*/portfile.cmake to use that date

# Phase 2: Test build
cmake -B build-vcpkg-stable -GNinja \
  -DCMAKE_TOOLCHAIN_FILE=/Users/mulgogi/src/external/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_OVERLAY_PORTS=/Users/mulgogi/src/external/dwarfs/vcpkg_ports \
  -DDWARFS_WITH_THRIFT=ON \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DWITH_TESTS=ON

# Phase 3: Integration test
ninja -C build-vcpkg-stable dwarfs_unit_tests mkdwarfs dwarfsck
./build-vcpkg-stable/dwarfs_unit_tests --gtest_filter="*ConverterRoundTrip*"
```

---

**Created**: 2025-12-31 18:38 HKT
**Author**: Kilo Code (Architect Mode)
**Next Session**: Research stable releases, update ports, complete vcpkg build
**Duration**: 4-5 hours phased over multiple sessions