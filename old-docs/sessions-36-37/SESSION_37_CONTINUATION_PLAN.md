# Session 37: Static Build Testing & Documentation

**Previous Session**: Session 36 - Static Build Infrastructure Implementation
**Status**: Ready to Start
**Estimated Time**: 2-3 hours
**Priority**: HIGH - Blocking production use

---

## Prerequisites

Session 36 must be complete with all 32 files created:
- ✅ vcpkg.json + vcpkg-configuration.json
- ✅ 6 triplet files
- ✅ 6 overlay port files
- ✅ 15 cmake/vcpkg modules
- ✅ CMakeLists.txt updated
- ✅ build-all-and-test.sh updated

---

## Phase 1: vcpkg Installation & Setup (30 min)

### Task 1.1: Install vcpkg
```bash
# Clone vcpkg
git clone https://github.com/microsoft/vcpkg.git ~/vcpkg

# Bootstrap
~/vcpkg/bootstrap-vcpkg.sh

# Set environment
export VCPKG_ROOT="$HOME/vcpkg"
echo 'export VCPKG_ROOT="$HOME/vcpkg"' >> ~/.bashrc
```

### Task 1.2: Verify Overlay Ports
```bash
cd /Users/mulgogi/src/external/dwarfs

# Check overlay port structure
ls -la vcpkg_ports/jemalloc/
ls -la vcpkg_ports/folly/
ls -la vcpkg_ports/fbthrift/

# Verify triplets
ls -la vcpkg_triplets/
```

---

## Phase 2: First Build Attempt (30 min)

### Task 2.1: Initial Build (Expect Failures)
```bash
cd /Users/mulgogi/src/external/dwarfs
export VCPKG_ROOT="$HOME/vcpkg"

# Build with vcpkg
./scripts/build-all-and-test.sh --vcpkg 2>&1 | tee build-vcpkg-first.log
```

**Expected Failures**:
- jemalloc SHA512 mismatch
- folly SHA512 mismatch
- fbthrift SHA512 mismatch

vcpkg will show messages like:
```
Expected hash: <actual_hash>
Actual hash:   0000000000000000...
```

### Task 2.2: Extract Correct Hashes
From build log, extract the 3 correct SHA512 hashes:
```bash
grep -A 2 "Expected hash" build-vcpkg-first.log
```

---

## Phase 3: Fix SHA512 Hashes (15 min)

### Task 3.1: Update jemalloc Hash
Edit `vcpkg_ports/jemalloc/portfile.cmake:6`:
```cmake
SHA512 <paste_actual_hash_here>
```

### Task 3.2: Update folly Hash
Edit `vcpkg_ports/folly/portfile.cmake:6`:
```cmake
SHA512 <paste_actual_hash_here>
```

### Task 3.3: Update fbthrift Hash
Edit `vcpkg_ports/fbthrift/portfile.cmake:6`:
```cmake
SHA512 <paste_actual_hash_here>
```

---

## Phase 4: Second Build Attempt (60 min)

### Task 4.1: Clean and Rebuild
```bash
# Clean previous failed build
rm -rf build-*

# Rebuild with correct hashes
./scripts/build-all-and-test.sh --vcpkg 2>&1 | tee build-vcpkg-second.log
```

**Expected Duration**: 30-60 minutes (vcpkg dependency installation)

### Task 4.2: Monitor Build Progress
Watch for:
- vcpkg installing packages
- CMake configuration messages
- Compilation progress
- Linking phase (may use 4-8 GB RAM)

---

## Phase 5: Verification (20 min)

### Task 5.1: Verify Static Linking

**macOS**:
```bash
# Check mkdwarfs
otool -L build-fb-only/mkdwarfs | grep -v "/usr/lib/system"
# Expected: Empty (or only libSystem.B.dylib)

# Check dwarfsck
otool -L build-fb-only/dwarfsck | grep -v "/usr/lib/system"

# Check dwarfsextract
otool -L build-fb-only/dwarfsextract | grep -v "/usr/lib/system"
```

**Linux**:
```bash
# Check mkdwarfs
ldd build-fb-only/mkdwarfs
# Expected: Only libc, libm, libdl, libpthread, linux-vdso, ld-linux

# Check all binaries
for bin in build-fb-only/{mkdwarfs,dwarfsck,dwarfsextract}; do
  echo "=== $bin ==="
  ldd "$bin"
done
```

### Task 5.2: Verify Binary Sizes
```bash
ls -lh build-fb-only/{mkdwarfs,dwarfsck,dwarfsextract}

# Expected sizes (static):
# mkdwarfs: ~15-25 MB
# dwarfsck: ~10-20 MB
# dwarfsextract: ~10-20 MB
```

### Task 5.3: Run Tests
```bash
cd build-fb-only
ctest --output-on-failure -j8
```

---

## Phase 6: Documentation Updates (45 min)

### Task 6.1: Update README.adoc
Add new section "Building with vcpkg (Static Builds)":

```adoc
== Building with vcpkg (Static Builds)

=== Prerequisites

* vcpkg package manager
* CMake ≥3.28
* Ninja or Make

=== Installation

[source,bash]
----
# Install vcpkg
git clone https://github.com/microsoft/vcpkg.git ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh
export VCPKG_ROOT="$HOME/vcpkg"

# Clone DwarFS
git clone https://github.com/tamatebako/dwarfs.git
cd dwarfs

# Build with vcpkg (auto-detects platform)
./scripts/build-all-and-test.sh --vcpkg

# Or specify triplet manually
export VCPKG_DEFAULT_TRIPLET="arm64-osx-static"
./scripts/build-all-and-test.sh --vcpkg
----

=== Supported Platforms

* Linux: x64-linux-static, arm64-linux-static
* macOS: x64-osx-static, arm64-osx-static  
* Windows: x64-windows-static, arm64-windows-static

=== Benefits

* Fully static binaries (no shared library dependencies)
* Reproducible builds across platforms
* No complex Thrift/Folly build requirements
* Uses Tebako-specific dependency forks (jemalloc, folly, fbthrift)
----
```

### Task 6.2: Create vcpkg Build Guide
Create `doc/vcpkg-build-guide.md` with:
- Detailed vcpkg setup instructions
- Platform-specific notes
- Troubleshooting guide
- SHA512 hash update procedure

### Task 6.3: Move Temporary Documentation
```bash
mkdir -p old-docs/session-36
mv doc/SESSION_36_*.md old-docs/session-36/
```

Keep only:
- `doc/SESSION_36_COMPLETION_SUMMARY.md` (as reference)
- `doc/SESSION_37_CONTINUATION_PLAN.md` (this file)

---

## Success Criteria

✅ **Phase 1**: vcpkg installed and $VCPKG_ROOT set
✅ **Phase 2**: First build attempted, SHA512 errors captured
✅ **Phase 3**: All 3 portfile.cmake updated with correct hashes
✅ **Phase 4**: Build completes successfully
✅ **Phase 5**: Static linking verified (ldd/otool shows only system libs)
✅ **Phase 6**: Documentation updated and temporary files moved

---

## Troubleshooting

### Issue: vcpkg install hangs
**Solution**: Check network connectivity, vcpkg may be downloading packages

### Issue: Out of memory during link
**Solution**: Reduce parallel jobs: `export JOBS=4`

### Issue: SHA512 mismatch persists
**Solution**: 
1. Delete vcpkg download cache: `rm -rf ~/vcpkg/downloads`
2. Retry build

### Issue: Cannot find jemalloc
**Solution**: Verify overlay port is referenced in vcpkg-configuration.json

---

## Next Session After Completion

After Session 37 completes:
1. Commit all changes with proper message
2. Test on additional platforms (if available)
3. Update CI/CD to use vcpkg builds
4. Document platform-specific quirks

---

**Created**: 2025-12-24
**Status**: Ready to Execute
**Next**: Install vcpkg and start Phase 1