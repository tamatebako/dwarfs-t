# Build.yml Extension Plan for Upstream Contribution

## Executive Summary

This document outlines a strategy to extend the upstream [`build.yml`](.github/workflows/build.yml) to include Windows, MSys2, metadata testing, and benchmarking capabilities, while maintaining a clean separation for Tebako-specific fork customizations.

**Goal**: Make metadata format testing and Windows support contributions easily acceptable to upstream DwarFS.

## Current State Analysis

### Upstream build.yml Coverage
- ✅ Linux: Extensive Docker-based matrix (Ubuntu, Fedora, Arch, Alpine, Debian, SUSE)
- ✅ Cross-compilation: riscv64, i386, arm, ppc64le, ppc64, loongarch64, s390x, aarch64
- ✅ Self-hosted: Windows, macOS, FreeBSD (but Windows build is limited)
- ❌ No Windows vcpkg builds (only self-hosted custom infrastructure)
- ❌ No MSys2/MinGW builds
- ❌ No metadata format testing across platforms
- ❌ No benchmark smoke tests

### Our Additional Coverage Needed
1. **Windows vcpkg builds** - Standard GitHub runners with vcpkg
2. **MSys2 builds** - MinGW-w64 toolchain testing
3. **Metadata format testing** - All formats across all platforms
4. **Benchmark smoke tests** - Quick verification that benchmarks work

### Tebako-Specific Elements (Fork Only)
- Custom container images (`ghcr.io/.../tebako-*-dev`)
- `-DTEBAKO_BUILD=ON` and `-DTEBAKO_BUILD_SCOPE` flags
- Specific compiler versions for Tebako (gcc-10, clang-12 on Ubuntu 20.04)
- Tebako integration testing workflows

## Architecture Strategy

### Phase 1: Non-Invasive Extensions to build.yml

**Principle**: Add jobs that complement existing build.yml without modifying its core structure.

```yaml
# In build.yml, add new jobs alongside existing ones:

jobs:
  # ... existing package-source, linux, windows, macos, freebsd jobs ...

  # NEW: Windows vcpkg builds (upstream contribution)
  windows-vcpkg:
    runs-on: ${{ matrix.runner }}
    strategy:
      matrix:
        include:
          - runner: windows-latest
            arch: x64
            format: all-formats
          - runner: windows-latest
            arch: x64
            format: minimal
    # ... vcpkg setup and build ...

  # NEW: MSys2 builds (upstream contribution)
  windows-msys2:
    runs-on: windows-latest
    strategy:
      matrix:
        sys: [ucrt64, mingw64]
    # ... MSys2 setup and build ...

  # NEW: Metadata format testing (upstream contribution)
  metadata-formats:
    runs-on: ${{ matrix.runner }}
    strategy:
      matrix:
        include:
          - os: ubuntu-latest
            format: all-formats
          - os: macos-latest
            format: minimal
          - os: windows-latest
            format: all-formats
    # ... metadata format tests ...

  # NEW: Benchmark smoke tests (upstream contribution)
  benchmark-smoke:
    needs: [linux, windows-vcpkg, macos]
    runs-on: ${{ matrix.runner }}
    # ... quick benchmark validation ...
```

### Phase 2: Tebako Fork Customization

**Principle**: Use workflow includes or separate jobs triggered by repository context.

#### Option A: Conditional Jobs (Recommended)
```yaml
jobs:
  # In build.yml, add Tebako-specific jobs that only run in the fork

  tebako-ubuntu:
    # Only run in Tebako fork
    if: github.repository == 'tamatebako/dwarfs' || github.repository_owner == 'riboseinc'
    runs-on: ${{ matrix.host }}
    container:
      image: 'ghcr.io/${{ github.repository_owner }}/tebako-${{ matrix.os }}-dev'
    strategy:
      matrix:
        include:
          - os: ubuntu-20.04
            cc: gcc-10
            scope: ALL
          - os: ubuntu-20.04
            cc: clang-12
            scope: MKD
    # ... Tebako-specific build ...

  tebako-macos:
    if: github.repository == 'tamatebako/dwarfs' || github.repository_owner == 'riboseinc'
    # ... Tebako macOS builds ...

  tebako-alpine:
    if: github.repository == 'tamatebako/dwarfs' || github.repository_owner == 'riboseinc'
    # ... Tebako Alpine builds ...
```

#### Option B: Separate Workflow Files (Alternative)
Keep Tebako workflows completely separate:
- `.github/workflows/build.yml` - Upstream + our extensions
- `.github/workflows/tebako-integration.yml` - Tebako-specific (fork only)

## Detailed Extension Design

### Extension 1: Windows vcpkg Builds

**Purpose**: Add standard Windows builds using GitHub runners and vcpkg

**Integration Point**: New job `windows-vcpkg` in build.yml

**Design**:
```yaml
windows-vcpkg:
  name: Windows vcpkg - ${{ matrix.arch }} - ${{ matrix.format }}
  runs-on: ${{ matrix.runner }}

  strategy:
    fail-fast: false
    matrix:
      include:
        # x64 builds
        - runner: windows-latest
          arch: x64
          triplet: x64-windows-static
          format: all-formats
          with_thrift: ON

        - runner: windows-latest
          arch: x64
          triplet: x64-windows-static
          format: minimal
          with_thrift: OFF

        # ARM64 builds (cross-compilation)
        - runner: windows-latest
          arch: ARM64
          triplet: arm64-windows-static
          format: all-formats
          with_thrift: ON

        - runner: windows-latest
          arch: ARM64
          triplet: arm64-windows-static
          format: minimal
          with_thrift: OFF

  steps:
    - name: Checkout
      uses: actions/checkout@v4
      with:
        submodules: true

    - name: Setup vcpkg
      run: |
        vcpkg install --triplet=${{ matrix.triplet }} `
          boost-system boost-filesystem boost-program-options `
          boost-iostreams boost-context boost-fiber `
          zlib lz4 zstd xxhash bzip2 liblzma `
          libarchive libevent double-conversion jemalloc gtest

        # Install Thrift if needed
        if [ "${{ matrix.with_thrift }}" == "ON" ]; then
          vcpkg install --triplet=${{ matrix.triplet }} thrift
        fi

    - name: Configure
      run: |
        cmake -B build -G "Visual Studio 17 2022" -A ${{ matrix.arch }} `
          -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake `
          -DCMAKE_BUILD_TYPE=Release `
          -DWITH_TESTS=ON `
          -DWITH_BENCHMARKS=ON `
          -DDWARFS_WITH_THRIFT=${{ matrix.with_thrift }} `
          -DDWARFS_WITH_CEREAL=ON `
          -DDWARFS_WITH_BITSERY=ON

    - name: Build
      run: cmake --build build --config Release --parallel

    - name: Test
      if: matrix.arch != 'ARM64'
      run: ctest --test-dir build -C Release --output-on-failure

    - name: Benchmark Smoke Test
      if: matrix.arch != 'ARM64'
      continue-on-error: true
      run: |
        ./build/Release/dwarfs_benchmark.exe --benchmark_min_time=0.1s --benchmark_filter=.*Creation.*

    - name: Upload Artifacts
      uses: actions/upload-artifact@v4
      with:
        name: dwarfs-windows-${{ matrix.arch }}-${{ matrix.format }}
        path: build/Release/dwarfs*.exe
```

**Upstream Contribution Value**:
- Enables standard Windows CI without self-hosted runners
- Tests both x64 and ARM64
- Validates all metadata formats on Windows
- Easy to understand and maintain

---

### Extension 2: MSys2/MinGW Builds

**Purpose**: Test MinGW-w64 toolchain and MSys2 ecosystem

**Integration Point**: New job `windows-msys2` in build.yml

**Design**:
```yaml
windows-msys2:
  name: Windows MSys2 - ${{ matrix.sys }}
  runs-on: windows-latest

  strategy:
    fail-fast: false
    matrix:
      sys: [ucrt64, mingw64]

  defaults:
    run:
      shell: msys2 {0}

  steps:
    - name: Setup MSys2
      uses: msys2/setup-msys2@v2
      with:
        msystem: ${{ matrix.sys }}
        update: true
        install: >-
          git tar bison flex
        pacboy: >-
          toolchain:p cmake:p ninja:p boost:p
          openssl:p libevent:p double-conversion:p
          fmt:p glog:p lz4:p xz:p zstd:p xxhash:p
          bzip2:p libarchive:p gtest:p

    - name: Checkout
      uses: actions/checkout@v4
      with:
        submodules: true

    - name: Configure
      run: |
        cmake -B build -GNinja \
          -DCMAKE_BUILD_TYPE=Release \
          -DWITH_TESTS=ON \
          -DWITH_BENCHMARKS=ON \
          -DDWARFS_WITH_THRIFT=OFF \
          -DDWARFS_WITH_CEREAL=ON \
          -DDWARFS_WITH_BITSERY=ON

    - name: Build
      run: cmake --build build --parallel

    - name: Test
      run: ctest --test-dir build --output-on-failure

    - name: Benchmark Smoke Test
      continue-on-error: true
      run: |
        ./build/dwarfs_benchmark --benchmark_min_time=0.1s --benchmark_filter=.*Creation.*
```

**Upstream Contribution Value**:
- Tests MinGW-w64 compatibility
- Validates pacboy package ecosystem
- Complements vcpkg builds with different toolchain

---

### Extension 3: Metadata Format Testing

**Purpose**: Comprehensive metadata serialization format testing across platforms

**Integration Point**: New job `metadata-formats` in build.yml

**Design**:
```yaml
metadata-formats:
  name: Metadata - ${{ matrix.os }} (${{ matrix.arch }}) - ${{ matrix.format }}
  runs-on: ${{ matrix.runner }}

  strategy:
    fail-fast: false
    matrix:
      include:
        # Linux x86_64
        - os: ubuntu
          arch: x86_64
          runner: ubuntu-latest
          format: all-formats
          with_thrift: ON

        - os: ubuntu
          arch: x86_64
          runner: ubuntu-latest
          format: minimal
          with_thrift: OFF

        # Linux ARM64
        - os: ubuntu
          arch: aarch64
          runner: ubuntu-24.04-arm64
          format: all-formats
          with_thrift: ON

        # macOS Intel
        - os: macos
          arch: x86_64
          runner: macos-13
          format: all-formats
          with_thrift: ON

        # macOS Apple Silicon
        - os: macos
          arch: aarch64
          runner: macos-14
          format: minimal
          with_thrift: OFF

        # Windows x64
        - os: windows
          arch: x64
          runner: windows-latest
          format: all-formats
          with_thrift: ON

  steps:
    - name: Checkout
      uses: actions/checkout@v4
      with:
        submodules: true

    - name: Setup Dependencies
      uses: ./.github/actions/setup-deps
      with:
        os: ${{ matrix.os }}
        arch: ${{ matrix.arch }}
        with_thrift: ${{ matrix.with_thrift }}

    - name: Configure
      shell: bash
      run: |
        cmake -B build \
          -DCMAKE_BUILD_TYPE=Release \
          -DWITH_TESTS=ON \
          -DDWARFS_WITH_THRIFT=${{ matrix.with_thrift }} \
          -DDWARFS_WITH_CEREAL=ON \
          -DDWARFS_WITH_BITSERY=ON

    - name: Build
      shell: bash
      run: cmake --build build --parallel

    - name: Test All
      shell: bash
      run: ctest --test-dir build --output-on-failure

    - name: Test Metadata Serialization
      shell: bash
      run: |
        ctest --test-dir build \
          --output-on-failure \
          --tests-regex "metadata.*serial"

    - name: Verify Metadata Formats
      shell: bash
      run: |
        # Verify that the binaries support the advertised formats
        ./build/mkdwarfs --help | grep -i "metadata format"
        ./build/dwarfsck --help | grep -i format
```

**Upstream Contribution Value**:
- Validates metadata serialization across all platforms
- Tests Thrift, Cereal, and Bitsery compatibility
- Ensures backward compatibility
- Catches serialization bugs early

---

### Extension 4: Benchmark Smoke Tests

**Purpose**: Quick validation that benchmarks build and run

**Integration Point**: New job `benchmark-smoke` in build.yml

**Design**:
```yaml
benchmark-smoke:
  name: Benchmark Smoke Test - ${{ matrix.platform }}
  needs: [linux, windows-vcpkg, macos]
  runs-on: ${{ matrix.runner }}

  strategy:
    fail-fast: false
    matrix:
      include:
        - platform: linux-amd64
          runner: ubuntu-latest
          artifact: dwarfs-linux-amd64-static

        - platform: macos-arm64
          runner: macos-14
          artifact: dwarfs-macos-arm64

        - platform: windows-x64
          runner: windows-latest
          artifact: dwarfs-windows-x64-all-formats

  steps:
    - name: Download Artifact
      uses: actions/download-artifact@v4
      with:
        name: ${{ matrix.artifact }}
        path: binaries

    - name: Make Executable (Unix)
      if: runner.os != 'Windows'
      run: chmod +x binaries/dwarfs_benchmark

    - name: Run Creation Benchmark
      continue-on-error: true
      shell: bash
      run: |
        if [ "$RUNNER_OS" = "Windows" ]; then
          ./binaries/dwarfs_benchmark.exe \
            --benchmark_min_time=0.1s \
            --benchmark_filter=.*Creation.*
        else
          ./binaries/dwarfs_benchmark \
            --benchmark_min_time=0.1s \
            --benchmark_filter=.*Creation.*
        fi

    - name: Run Decompression Benchmark
      continue-on-error: true
      shell: bash
      run: |
        if [ "$RUNNER_OS" = "Windows" ]; then
          ./binaries/dwarfs_benchmark.exe \
            --benchmark_min_time=0.1s \
            --benchmark_filter=.*Decompression.*
        else
          ./binaries/dwarfs_benchmark \
            --benchmark_min_time=0.1s \
            --benchmark_filter=.*Decompression.*
        fi
```

**Upstream Contribution Value**:
- Validates benchmark suite builds correctly
- Catches benchmark regressions early
- Minimal overhead (quick smoke tests only)
- Uses artifacts from other jobs (no rebuild)

---

## Tebako Fork Separation Strategy

### Approach: Conditional Jobs in Single File

**Rationale**:
- Minimal fork divergence
- Easy to merge upstream changes
- Clear separation via conditions
- No workflow duplication

### Implementation Pattern

```yaml
# In .github/workflows/build.yml

jobs:
  # ... existing upstream jobs ...

  # ... our upstream contributions (windows-vcpkg, msys2, metadata, benchmark) ...

  # ============================================================================
  # TEBAKO-SPECIFIC JOBS (only in tamatebako/dwarfs fork)
  # ============================================================================

  tebako-ubuntu:
    name: Tebako Ubuntu - ${{ matrix.arch }} - ${{ matrix.compiler }} - ${{ matrix.scope }}
    if: |
      github.repository == 'tamatebako/dwarfs' ||
      github.repository_owner == 'riboseinc' ||
      contains(github.event.head_commit.message, '[tebako]')
    runs-on: ${{ matrix.host }}
    container:
      image: 'ghcr.io/${{ github.repository_owner }}/tebako-${{ matrix.os }}-dev'
      options: --privileged

    strategy:
      fail-fast: false
      matrix:
        include:
          - host: ubuntu-22.04
            arch: amd64
            os: ubuntu-20.04
            compiler: gcc-10
            scope: ALL
            tests: ON

          - host: ubuntu-22.04
            arch: amd64
            os: ubuntu-20.04
            compiler: clang-12
            scope: MKD
            tests: OFF

          - host: ubuntu-22.04-arm
            arch: arm64
            os: ubuntu-20.04
            compiler: gcc-10
            scope: ALL
            tests: ON

    env:
      CC: ${{ matrix.compiler }}
      CXX: ${{ matrix.compiler == 'gcc-10' && 'g++-10' || 'clang++-12' }}

    steps:
      - name: Checkout
        uses: actions/checkout@v4
        with:
          submodules: true

      - name: Install FUSE3
        if: matrix.scope == 'ALL'
        run: apt-get update && apt-get install -y libfuse3-dev fuse3

      - name: Configure
        run: |
          cmake -B build -GNinja \
            -DCMAKE_BUILD_TYPE=Release \
            -DTEBAKO_BUILD=ON \
            -DTEBAKO_BUILD_SCOPE=${{ matrix.scope }} \
            -DWITH_TESTS=${{ matrix.tests }}

      - name: Build
        run: cmake --build build --parallel

      - name: Test
        if: matrix.tests == 'ON'
        run: ctest --test-dir build --output-on-failure

  tebako-macos:
    name: Tebako macOS - ${{ matrix.xcode }} - ${{ matrix.scope }}
    if: |
      github.repository == 'tamatebako/dwarfs' ||
      github.repository_owner == 'riboseinc' ||
      contains(github.event.head_commit.message, '[tebako]')
    runs-on: ${{ matrix.os }}

    strategy:
      matrix:
        include:
          - os: macos-14
            xcode: '15.0.1'
            scope: ALL
            tests: ON

          - os: macos-14
            xcode: '15.4'
            scope: MKD
            tests: OFF

    steps:
      - name: Select XCode
        uses: maxim-lobanov/setup-xcode@v1
        with:
          xcode-version: ${{ matrix.xcode }}

      - name: Checkout
        uses: actions/checkout@v4
        with:
          submodules: true

      - name: Configure
        run: |
          cmake -B build -GNinja \
            -DCMAKE_BUILD_TYPE=Release \
            -DTEBAKO_BUILD=ON \
            -DTEBAKO_BUILD_SCOPE=${{ matrix.scope }} \
            -DWITH_TESTS=${{ matrix.tests }}

      - name: Build
        run: cmake --build build --parallel

      - name: Test
        if: matrix.tests == 'ON'
        run: ctest --test-dir build --output-on-failure

  tebako-alpine:
    name: Tebako Alpine - ${{ matrix.arch }} - ${{ matrix.compiler }} - ${{ matrix.scope }}
    if: |
      github.repository == 'tamatebako/dwarfs' ||
      github.repository_owner == 'riboseinc' ||
      contains(github.event.head_commit.message, '[tebako]')
    runs-on: ubuntu-latest
    container:
      image: 'ghcr.io/${{ github.repository_owner }}/tebako-${{ matrix.os }}-dev'
      options: --privileged

    strategy:
      matrix:
        include:
          - arch: amd64
            os: alpine-3.17
            compiler: gcc
            scope: ALL
            tests: ON

          - arch: amd64
            os: alpine-3.17
            compiler: clang
            scope: MKD
            tests: OFF

    env:
      CC: ${{ matrix.compiler }}
      CXX: ${{ matrix.compiler == 'gcc' && 'g++' || 'clang++' }}

    steps:
      - name: Checkout
        uses: actions/checkout@v4
        with:
          submodules: true

      - name: Install FUSE3
        if: matrix.scope == 'ALL'
        run: apk add --no-cache fuse3 fuse3-dev

      - name: Configure
        run: |
          cmake -B build -GNinja \
            -DCMAKE_BUILD_TYPE=Release \
            -DTEBAKO_BUILD=ON \
            -DTEBAKO_BUILD_SCOPE=${{ matrix.scope }} \
            -DWITH_TESTS=${{ matrix.tests }}

      - name: Build
        run: cmake --build build --parallel

      - name: Test
        if: matrix.tests == 'ON'
        run: ctest --test-dir build --output-on-failure
```

### Fork Maintenance Strategy

1. **Upstream Sync**:
   - Regularly merge upstream/main into fork
   - Tebako jobs won't run in upstream (condition prevents it)
   - Our extensions run in both upstream and fork

2. **Contribution Flow**:
   ```
   Fork (tamatebako/dwarfs)
   ├── Upstream contributions (windows-vcpkg, msys2, metadata, benchmark)
   │   └── PR to upstream → mhx/dwarfs
   └── Tebako-specific jobs (conditional, only in fork)
       └── Stay in fork, don't PR upstream
   ```

3. **Testing Before PR**:
   - Test extensions in fork first
   - Verify they work in both contexts (with/without Tebako jobs)
   - Submit clean PR with only upstream-relevant jobs

4. **Documentation**:
   - Add comments clearly marking Tebako sections
   - Document condition logic
   - Maintain TEBAKO_INTEGRATION.md explaining the separation

---

## Migration Plan

### Phase 1: Preparation (Week 1)
1. ✅ Create this architecture document
2. Create `.github/actions/setup-deps` composite action
3. Test extensions locally with Act or similar
4. Validate conditional job logic

### Phase 2: Implementation (Week 2)
1. Add `windows-vcpkg` job to build.yml
2. Add `windows-msys2` job to build.yml
3. Add `metadata-formats` job to build.yml
4. Add `benchmark-smoke` job to build.yml
5. Test all extensions in fork

### Phase 3: Tebako Integration (Week 3)
1. Add conditional Tebako jobs to build.yml
2. Deprecate old separate workflows:
   - ubuntu.yml → tebako-ubuntu job
   - macos.yml → tebako-macos job
   - alpine.yml → tebako-alpine job
   - windows.yml → already deprecated
   - metadata-format-ci.yml → metadata-formats job
3. Update documentation

### Phase 4: Upstream Contribution (Week 4)
1. Create clean branch with only upstream extensions
2. Remove Tebako-specific jobs from PR branch
3. Submit PR to mhx/dwarfs with:
   - windows-vcpkg
   - windows-msys2
   - metadata-formats
   - benchmark-smoke
4. Provide documentation and rationale

### Phase 5: Merge and Maintain
1. If upstream accepts:
   - Merge upstream changes back to fork
   - Re-add Tebako conditional jobs
   - Verify everything works
2. If upstream rejects:
   - Keep all extensions in fork
   - Maintain separate from upstream

---

## File Structure After Migration

```
.github/workflows/
├── build.yml                    # Extended with our contributions + Tebako jobs
├── docker-run-build.yml         # Keep (infrastructure)
├── tebako-build-test.yml        # Keep (Tebako integration testing)
├── tebako-lint.yml              # Keep (Tebako linting)
├── DEPRECATED/
│   ├── ubuntu.yml               # → Merged into build.yml (tebako-ubuntu)
│   ├── macos.yml                # → Merged into build.yml (tebako-macos)
│   ├── alpine.yml               # → Merged into build.yml (tebako-alpine)
│   ├── windows.yml              # → Merged into build.yml (windows-vcpkg)
│   ├── windows-msys.yml         # → Merged into build.yml (windows-msys2)
│   ├── metadata-format-ci.yml   # → Merged into build.yml (metadata-formats)
│   └── ci-unified.yml           # → Replaced by extended build.yml
└── .github/actions/
    └── setup-deps/              # New: Composite action for dependency setup
        └── action.yml
```

---

## Benefits of This Approach

### For Upstream (mhx/dwarfs)
1. **Better Windows support** - Standard vcpkg builds anyone can run
2. **MSys2 validation** - More comprehensive Windows testing
3. **Metadata format testing** - Ensures serialization compatibility
4. **Benchmark validation** - Catches performance regressions
5. **Minimal changes** - Non-invasive additions to existing workflow
6. **Clear separation** - Easy to understand and review

### For Fork (tamatebako/dwarfs)
1. **Easy maintenance** - Single workflow file, conditional jobs
2. **Upstream sync** - Minimal merge conflicts
3. **Clear separation** - Tebako jobs clearly marked
4. **Comprehensive testing** - All scenarios in one place
5. **No duplication** - Shared infrastructure with upstream

### For Both
1. **Code reuse** - Shared dependency setup
2. **Consistent patterns** - Similar job structures
3. **Better documentation** - Clear purpose for each job
4. **Easier debugging** - All related tests together

---

## Risk Mitigation

### Risk: Upstream Rejects Extensions
**Mitigation**:
- Extensions are self-contained jobs
- Can be maintained in fork independently
- No changes to existing upstream jobs

### Risk: Conditional Logic Too Complex
**Mitigation**:
- Simple repository-based conditions
- Clear documentation
- Optional commit message override (`[tebako]`)

### Risk: Merge Conflicts on Sync
**Mitigation**:
- Tebako jobs at end of file (minimal conflict zone)
- Clear comment markers
- Regular syncs (don't let divergence grow)

### Risk: Performance Impact
**Mitigation**:
- Tebako jobs only run in fork
- Upstream extensions are opt-in via conditions
- Use job dependencies to parallelize

---

## Next Steps

1. **Review this plan** with team
2. **Create setup-deps action** as reusable component
3. **Implement Phase 1** (one extension at a time)
4. **Test thoroughly** in fork before PR
5. **Prepare upstream PR** with clear documentation
6. **Maintain fork** with Tebako extensions

---

## Conclusion

This architecture provides a clean path to **contribute valuable Windows and metadata testing to upstream** while **maintaining Tebako-specific customizations in the fork**. The conditional job approach minimizes divergence and makes ongoing maintenance straightforward.

The key insight is that build.yml is already designed as a comprehensive testing matrix - we're just extending it with more platforms and test types that align with its existing structure.