Do the following:

1. Tebako Dwarfs supports 3 metadata formats in 2 builds:
   1. FlatBuffers (modern default)
   2. Legacy Thrift (hand-coded, always available)
   3. Modern Thrift (fbthrift, optional)
2. The two builds are:
   1. Default build: FlatBuffers + Legacy Thrift
   2. Thrift-enabled build: FlatBuffers + Legacy Thrift + Modern Thrift
3. For Legacy Thrift, we have hand-written serialization code in
   `src/metadata/legacy/` that does not depend on any external Thrift libraries.
   This code is always included in both builds. We need to test this code by
   encoding and decoding images against the Homebrew `dwarfs` tool which uses
   Legacy Thrift serialization (only). You need to write a test suite for this
   within our existing test framework. Also, need to write tests that verify
   that we can read and write Legacy Thrift files correctly (can use Homebrew
   `dwarfs` to generate fixtures). We need to verify that our
   legacy-thrift-enabled build can detect and read dwarfs files (they are
   Thrift/DFT files) written by `dwarfs` installed by Homebrew
   (`/opt/homebrew/Cellar/dwarfs/0.14.1_3`), and that we can create
   Dwarfs-LegacyThrift files that the Homebrew `dwarfs` tool can read.


4. Run this:
```bash
cd scripts
./benchmark-all.sh
```

It should work and the benchmarks should pass. (we also have benchmarks/build_and_test_all.py but we want the scripts/ folder to work). Update the README.md with benchmarks (replace the numbers where they exist).
We need to benchmark all 3 metadata formats (FlatBuffers, Legacy Thrift, Modern Thrift), and across all typical FS operations (create, read, write, delete, list, copy, move, etc, one file, multiple files, whole archive).

2. Run this:
```bash
cd example/static-site-server
./build.sh
./test.sh
```
This needs to work.

3. This should work. Run:
```bash
# clean build folder first
cmake -B build -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DDWARFS_WITH_FLATBUFFERS=ON \ # this should be default ON so we should not need to specify it
  -DDWARFS_WITH_THRIFT=ON
ninja -C build
```

4. This should work. Run:
```bash
./scripts/build-all-and-test.sh --vcpkg
```

5. Clean up scripts/ and benchmark/ to ensure things are useful and MECE (mutually exclusive,
   collectively exhaustive).

6. We need to verify that our Thrift-enabled build can detect and read dwarfs files
(they are Thrift/DFT files) written by `dwarfs` installed by Homebrew
(`/opt/homebrew/Cellar/dwarfs/0.14.1_3`), and that we can create Dwarfs-Thrift
files that the Homebrew `dwarfs` tool can read.

7. Triplets:

While we work with triplets, notice that we need to support these platforms (test on CI):

- arm64-windows-{static, dynamic}
- arm64-mingw-{static, dynamic}
- x64-windows-{static, dynamic}
- x64-mingw-{static, dynamic}
- arm64-osx-{static, dynamic}
- x64-osx-{static, dynamic}
- arm64-linux-{static, dynamic}
- x64-linux-{static, dynamic}

8. We want to clean our directory structure and code to make it easier to maintain,
an example is given in /Users/mulgogi/src/external/xz.

9. Fully DRY the .github/workflows by using reusable workflows and composite
   actions. Update all the workflows so they use the proper way of compiling
   through CMake with vcpkg overlay ports.

