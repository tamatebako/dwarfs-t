# Session 45: Tool Support Library - Complete Integration

**Date**: 2025-12-27+
**Previous Session**: Session 44 (Phases 1-2 complete)
**Estimated Total Time**: 2-3 hours
**Status**: Ready to start

---

## Context from Session 44

### Completed ✅
- ✅ Phase 1: Modular CMake structure (`cmake/tool_support.cmake`)
- ✅ Phase 2: vcpkg port cleanup

### Foundation Established
- ✅ Library target properly configured
- ✅ Absolute paths for vcpkg compatibility
- ✅ Clean modular architecture
- ✅ Installation rules defined

### Known Issue
- ❌ parallel_hashmap dependency missing in `dwarfs_common` (pre-existing, unrelated)
- **Impact**: Blocks full build until resolved
- **Solution**: Must be fixed before proceeding with phases 3-6

---

## Remaining Work - Compressed Timeline

### Phase 3: Fix Dependencies & Full vcpkg Build (45 min)

**Goal**: Resolve blocking dependency and verify library builds/installs

**Tasks**:

1. **Fix parallel_hashmap dependency** (15 min)
   - Check if `cmake/vcpkg/phmap.cmake` is being included
   - Verify FetchContent or pkg-config setup
   - Ensure headers accessible to `library_dependencies.cpp`

2. **Full build test** (15 min)
   ```bash
   rm -rf build-quick
   cmake -B build-quick -DWITH_LIBDWARFS=ON -DWITH_TOOLS=OFF
   cmake --build build-quick -j8
   ls build-quick/libdwarfs_tool_support.a  # MUST exist
   ```

3. **vcpkg workflow test** (15 min)
   ```bash
   rm -rf vcpkg_installed example/static-site-server/build
   cd example/static-site-server
   ./build.sh
   ls build/vcpkg_installed/*/lib/libdwarfs_tool_support.a  # MUST exist
   ls build/vcpkg_installed/*/include/dwarfs/tool/  # Headers present
   ```

**Validation**:
- [ ] Library file exists: `libdwarfs_tool_support.a`
- [ ] vcpkg installation successful
- [ ] Headers installed correctly
- [ ] CMake config exports tool_support target

---

### Phase 4: Build Tools Separately (30 min)

**Goal**: Verify tools can build using installed libraries

**Setup** (10 min):
```bash
# Create minimal tools CMakeLists.txt
cat > tools/CMakeLists.txt << 'EOF'
cmake_minimum_required(VERSION 3.24)
project(dwarfs_tools)

find_package(dwarfs REQUIRED CONFIG)

# mkdwarfs
add_executable(mkdwarfs src/mkdwarfs_main.cpp)
target_link_libraries(mkdwarfs PRIVATE dwarfs::dwarfs_tool_support)

# dwarfsck
add_executable(dwarfsck src/dwarfsck_main.cpp)
target_link_libraries(dwarfsck PRIVATE dwarfs::dwarfs_tool_support)

# dwarfsextract
add_executable(dwarfsextract src/dwarfsextract_main.cpp)
target_link_libraries(dwarfsextract PRIVATE dwarfs::dwarfs_tool_support)

install(TARGETS mkdwarfs dwarfsck dwarfsextract
        RUNTIME DESTINATION bin)
EOF
```

**Build Test** (20 min):
```bash
cmake -B build-tools -S tools \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=arm64-osx-static

cmake --build build-tools --parallel

# Verify binaries
ls build-tools/mkdwarfs
ls build-tools/dwarfsck
ls build-tools/dwarfsextract
```

**Validation**:
- [ ] All 3 tools compile successfully
- [ ] Binaries link against installed libraries
- [ ] find_package(dwarfs) successful
- [ ] dwarfs::dwarfs_tool_support target available

---

### Phase 5: Functional Integration Test (30 min)

**Goal**: Verify tools work end-to-end

**Test Workflow** (30 min):
```bash
# Create test filesystem
./build-tools/mkdwarfs -i /usr/share/dict -o test.dff -l1

# Verify integrity
./build-tools/dwarfsck test.dff

# Extract
./build-tools/dwarfsextract -i test.dff -o test-output

# Compare
diff -r /usr/share/dict test-output
echo $?  # Should be 0

# Cleanup
rm -rf test.dff test-output
```

**Advanced Tests**:
```bash
# Test with different compression levels
./build-tools/mkdwarfs -i /usr/share/dict -o test-l9.dff -l9

# Test selective extraction
./build-tools/dwarfsextract -i test.dff -o partial/ -f "words"

# Test JSON output
./build-tools/dwarfsck test.dff --json > metadata.json
cat metadata.json | jq .
```

**Validation**:
- [ ] Filesystem creation successful
- [ ] Integrity check passes
- [ ] Extraction produces identical files
- [ ] All command-line options work
- [ ] No crashes or errors

---

### Phase 6: Documentation & Cleanup (45 min)

**Goal**: Update official docs and archive session materials

#### 6.1 Update README.md (15 min)

Add section after "Building":

```markdown
### Building Tools with vcpkg

DwarFS libraries can be built and installed via vcpkg for use in other projects:

\`\`\`bash
# Install DwarFS libraries
vcpkg install dwarfs

# Build tools using installed libraries
cmake -B build-tools -S tools \\
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake

cmake --build build-tools
\`\`\`

This approach is useful for:
- Embedding DwarFS in C++ applications
- Building custom tools using DwarFS libraries
- Static linking scenarios
- Cross-platform development

See \`example/static-site-server/\` for a complete example.
```

#### 6.2 Create doc/vcpkg-integration.md (20 min)

Comprehensive guide covering:
- **Overview**: Why use vcpkg for DwarFS
- **Installation**: Step-by-step vcpkg setup
- **Library Structure**: 7 libraries explained
- **Tool Building**: Separate tool builds
- **Dependencies**: What's required and why
- **Troubleshooting**: Common issues and solutions
- **Examples**: Code snippets for each use case

#### 6.3 Archive Session Docs (10 min)

```bash
mkdir -p old-docs/sessions-41-44
mv doc/SESSION_41_*.md old-docs/sessions-41-44/
mv doc/SESSION_42_*.md old-docs/sessions-41-44/
mv doc/SESSION_43_*.md old-docs/sessions-41-44/
mv doc/SESSION_44_*.md old-docs/sessions-41-44/

# Keep only the summary for reference
cp old-docs/sessions-41-44/SESSION_44_COMPLETION_SUMMARY.md \
   doc/TOOL_SUPPORT_LIBRARY_IMPLEMENTATION.md
```

**Validation**:
- [ ] README.md updated with vcpkg section
- [ ] vcpkg-integration.md created
- [ ] Session docs archived
- [ ] Summary doc created for future reference

---

## Critical Success Factors

### 1. Dependency Resolution
**Must Fix**: parallel_hashmap include issue before proceeding
**Action**: Verify `cmake/vcpkg/phmap.cmake` included and working

### 2. Reproducible Builds
**Must Verify**: Clean builds work from scratch
**Test**: `rm -rf build* vcpkg_installed && ./example/static-site-server/build.sh`

### 3. Tool Separation
**Must Achieve**: Tools build independently using installed libraries
**Test**: Build tools in clean directory with only vcpkg-installed deps

### 4. End-to-End Validation
**Must Pass**: Create → Verify → Extract → Compare workflow
**Test**: Full functional test with sample data

---

## Quick Start Guide

**To continue from Session 44**:

1. Read this plan
2. Read `doc/SESSION_45_IMPLEMENTATION_STATUS.md`
3. **FIX BLOCKING ISSUE**: Resolve parallel_hashmap dependency
4. Start with Phase 3: Full vcpkg build test
5. Proceed through phases 4-6 sequentially

**Critical First Step**:
```bash
# Must succeed before proceeding
cmake --build build-quick -j8
ls build-quick/libdwarfs_tool_support.a
```

If this fails with parallel_hashmap error, **fix that first**.

---

## Timeline

| Phase | Task | Est. Time | Dependency |
|-------|------|-----------|------------|
| 3.1 | Fix parallel_hashmap | 15 min | None |
| 3.2 | Full build test | 15 min | 3.1 |
| 3.3 | vcpkg workflow test | 15 min | 3.2 |
| 4.1 | Setup tools CMakeLists | 10 min | 3.3 |
| 4.2 | Build tools separately | 20 min | 4.1 |
| 5 | Functional integration | 30 min | 4.2 |
| 6.1 | Update README | 15 min | 5 |
| 6.2 | Create vcpkg guide | 20 min | 6.1 |
| 6.3 | Archive session docs | 10 min | 6.2 |

**Total**: ~2.5 hours (compressed from original 3 hours)

---

## Deliverables

### Code
- [x] `cmake/tool_support.cmake` (Session 44)
- [ ] Fixed dependency in `cmake/vcpkg/phmap.cmake`
- [ ] Minimal `tools/CMakeLists.txt` for separate builds
- [ ] Working example in `example/static-site-server/`

### Documentation
- [ ] Updated `README.md` with vcpkg section
- [ ] New `doc/vcpkg-integration.md`
- [ ] Archived session docs in `old-docs/sessions-41-44/`
- [ ] Summary doc `doc/TOOL_SUPPORT_LIBRARY_IMPLEMENTATION.md`

### Validation
- [ ] Library builds successfully
- [ ] vcpkg installation works
- [ ] Tools build separately
- [ ] End-to-end functional tests pass
- [ ] Clean builds reproducible

---

## Known Risks & Mitigations

### Risk 1: Dependency Hell
**Risk**: Other missing dependencies surface during build
**Mitigation**: Address each systematically, document solutions
**Fallback**: Use existing working build, test incrementally

### Risk 2: vcpkg Configuration Issues
**Risk**: vcpkg toolchain doesn't find libraries
**Mitigation**: Verify CMAKE_PREFIX_PATH, use verbose output
**Fallback**: Manual library path configuration

### Risk 3: Tool Linkage Failures
**Risk**: Tools don't link against installed libraries
**Mitigation**: Check dwarfs-targets.cmake exports, verify CMake config
**Fallback**: Static library approach with explicit linking

---

## Success Criteria

**Session 45 Complete When**:
- [x] All dependencies resolved
- [x] Library builds and installs via vcpkg
- [x] Tools build separately using installed libraries
- [x] End-to-end functional tests pass
- [x] Documentation updated and archived
- [x] Clean builds reproducible

**Definition of Done**:
```bash
# This entire workflow must succeed:
rm -rf build* vcpkg_installed
cd example/static-site-server && ./build.sh
cd ../.. && cmake -B build-tools -S tools -DCMAKE_TOOLCHAIN_FILE=...
cmake --build build-tools
./build-tools/mkdwarfs -i /usr/share/dict -o test.dff
./build-tools/dwarfsck test.dff
./build-tools/dwarfsextract -i test.dff -o test-out
diff -r /usr/share/dict test-out
```

---

**Focus**: Fix dependencies, validate end-to-end, document thoroughly.
**Deadline**: Complete all phases in single session (~2.5 hours).