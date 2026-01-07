# Session 31L Continuation Prompt

**Read First**: [`doc/SESSION_31L_CONTINUATION_PLAN.md`](SESSION_31L_CONTINUATION_PLAN.md)

## Quick Context

Session 31K implemented timestamp handling successfully. All domain-based architecture is complete and working. Now we need to **safely delete old backend files** that are no longer being compiled.

## Critical Understanding

**7 files to delete (337KB total)** - These are NOT being compiled anymore:
1. `metadata_v2_flatbuffers.cpp` (83KB) - Replaced by flatbuffers_metadata_adapter.cpp
2. `metadata_v2_thrift.cpp` (81KB) - Replaced by thrift_metadata_adapter.cpp
3. `metadata_v2_thrift_part1.cpp` (48KB) - Old split file
4. `metadata_v2_thrift_part2.cpp` (28KB) - Old split file
5. `metadata_v2_thrift_getters.cpp` (3KB) - Old split file
6. `metadata_v2_flatbuffers_factory.cpp` (2.6KB) - Old factory
7. `metadata_v2_thrift_upstream.cpp` (84KB) - Upstream copy

## Start Here

### Phase 1: Remove from CMake FIRST ⚠️ (20 min)

**CRITICAL**: Remove file references from CMake BEFORE moving files.

**What to do**:
1. Open [`cmake/libdwarfs.cmake:179-186`](../cmake/libdwarfs.cmake)
2. Find the `set_source_files_properties` block with `metadata_v2_flatbuffers.cpp`
3. Remove the line `src/reader/internal/metadata_v2_flatbuffers.cpp` (it's NOT in source list)
4. Keep only `metadata_types_flatbuffers.cpp` (it IS being compiled)
5. Verify no other references: `grep -r "metadata_v2_flatbuffers\|metadata_v2_thrift" cmake/`
6. Test build: `cmake --build build-fb-clean --target mkdwarfs dwarfsck -j8`

**Success**: Build completes without errors.

### Phase 2: Move Files to Backup (10 min)

**What to do**:
```bash
mkdir -p .backup/session31-obsolete-backend

# Move the 7 files
mv src/reader/internal/metadata_v2_flatbuffers.cpp .backup/session31-obsolete-backend/
mv src/reader/internal/metadata_v2_thrift.cpp .backup/session31-obsolete-backend/
mv src/reader/internal/metadata_v2_thrift_part1.cpp .backup/session31-obsolete-backend/
mv src/reader/internal/metadata_v2_thrift_part2.cpp .backup/session31-obsolete-backend/
mv src/reader/internal/metadata_v2_thrift_getters.cpp .backup/session31-obsolete-backend/
mv src/reader/internal/metadata_v2_flatbuffers_factory.cpp .backup/session31-obsolete-backend/
mv src/reader/internal/metadata_v2_thrift_upstream.cpp .backup/session31-obsolete-backend/

# Verify moved
ls -lh .backup/session31-obsolete-backend/
```

**Success**: 7 files moved, total ~337KB.

### Phase 3: Verify Builds (30 min)

**What to do**:
1. **FlatBuffers-only build**:
   ```bash
   rm -rf build-fb-verify
   cmake -B build-fb-verify -DCMAKE_BUILD_TYPE=Release -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=OFF
   cmake --build build-fb-verify --target mkdwarfs dwarfsck -j8
   ./build-fb-verify/mkdwarfs -i example/pg11339-h -o /tmp/verify-fb.dff
   ./build-fb-verify/dwarfsck /tmp/verify-fb.dff
   ```

2. **Both-formats build**:
   ```bash
   rm -rf build-both-verify
   cmake -B build-both-verify -DCMAKE_BUILD_TYPE=Release -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=ON
   cmake --build build-both-verify --target mkdwarfs dwarfsck -j8
   ./build-both-verify/mkdwarfs -i example/pg11339-h -o /tmp/verify-both.dff
   ./build-both-verify/dwarfsck /tmp/verify-both.dff
   ```

**Success**: Both builds work, filesystems created and validated.

### Phase 4: Git Commit (10 min)

**What to do**:
```bash
git add cmake/libdwarfs.cmake
git add src/reader/internal/metadata_v2*.cpp

git commit -m "refactor(metadata): remove obsolete backend implementation files

Removed 7 obsolete files (337KB) replaced by domain-based architecture.

Files deleted:
- metadata_v2_flatbuffers.cpp - replaced by flatbuffers_metadata_adapter.cpp
- metadata_v2_thrift.cpp - replaced by thrift_metadata_adapter.cpp
- Legacy split files (part1, part2, getters)
- Obsolete factory and upstream copy

Moved to .backup/session31-obsolete-backend/ for reference.
"
```

### Phase 5: Update Documentation (50 min)

**What to do**:
1. **Update README.adoc**: Add architecture section (see plan for content)
2. **Create doc/dwarfs-metadata-architecture.md**: Full architecture docs
3. **Move old session docs**:
   ```bash
   mkdir -p doc/old-sessions
   mv doc/SESSION_28_*.md doc/old-sessions/
   mv doc/SESSION_29_*.md doc/old-sessions/
   mv doc/SESSION_30_*.md doc/old-sessions/
   mv doc/SESSION_31[A-I]_*.md doc/old-sessions/
   # Keep: SESSION_31J, SESSION_31K, SESSION_31L
   ```

## Key Files

- **Plan**: [`SESSION_31L_CONTINUATION_PLAN.md`](SESSION_31L_CONTINUATION_PLAN.md) - Full details
- **CMake**: [`cmake/libdwarfs.cmake`](../cmake/libdwarfs.cmake) - What needs changing
- **Previous**: [`SESSION_31K_CONTINUATION_PROMPT.md`](SESSION_31K_CONTINUATION_PROMPT.md) - What was done

## Expected Outcome

After Session 31L:
- ✅ CMake cleaned (no dead references)
- ✅ 7 files removed (337KB freed)
- ✅ All builds work (FB-only, both-formats)
- ✅ Git history clean
- ✅ Documentation updated
- ✅ Domain migration 100% complete

## Timeline

- Phase 1: 20 min
- Phase 2: 10 min
- Phase 3: 30 min
- Phase 4: 10 min
- Phase 5: 50 min
- **Total**: 2 hours

---

**Read the full plan**: [`SESSION_31L_CONTINUATION_PLAN.md`](SESSION_31L_CONTINUATION_PLAN.md)

**Last Updated**: 2025-12-23 16:07 HKT