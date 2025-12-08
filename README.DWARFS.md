[![Latest Release](https://img.shields.io/github/release/mhx/dwarfs?label=Latest%20Release)](https://github.com/mhx/dwarfs/releases/latest)
[![Total Downloads](https://img.shields.io/github/downloads/mhx/dwarfs/total.svg?&color=E95420&label=Total%20Downloads)](https://github.com/mhx/dwarfs/releases)
[![Homebrew Downloads](https://img.shields.io/homebrew/installs/dm/dwarfs?label=Homebrew)](https://formulae.brew.sh/formula/dwarfs)
[![DwarFS CI Build](https://github.com/mhx/dwarfs/actions/workflows/build.yml/badge.svg)](https://github.com/mhx/dwarfs/actions/workflows/build.yml)
[![Codacy Badge](https://app.codacy.com/project/badge/Grade/53489f77755248c999e380500267e889)](https://app.codacy.com/gh/mhx/dwarfs/dashboard)
[![codecov](https://codecov.io/github/mhx/dwarfs/graph/badge.svg?token=BKR4A3XKA9)](https://codecov.io/github/mhx/dwarfs)
[![OpenSSF Best Practices](https://www.bestpractices.dev/projects/8663/badge)](https://www.bestpractices.dev/projects/8663)

# DwarFS

The **D**eduplicating **W**arp-speed **A**dvanced **R**ead-only **F**ile **S**ystem.

A fast high-compression read-only file system for Linux, FreeBSD, macOS and Windows.

## Table of contents

<a href="https://repology.org/project/dwarfs/versions">
    <img src="https://repology.org/badge/vertical-allrepos/dwarfs.svg" alt="Packaging status" align="right">
</a>

- [Find the section about building and add dual-format information](#find-the-section-about-building-and-add-dual-format-information)
- [Look for the section that mentions dependencies and build configuration](#look-for-the-section-that-mentions-dependencies-and-build-configuration)
- [Around line 355-380, add dual-format information after the folly/fbthrift note](#around-line-355-380-add-dual-format-information-after-the-follyfbthrift-note)
- [Line 367 is where the folly/fbthrift note ends](#line-367-is-where-the-follyfbthrift-note-ends)
- [Add after line 367:](#add-after-line-367)
- [](#)
- [- DwarFS now supports dual-format metadata serialization with both FlatBuffers](#--dwarfs-now-supports-dual-format-metadata-serialization-with-both-flatbuffers)
- [(modern default, required) and Thrift Compact (legacy, optional). FlatBuffers](#modern-default-required-and-thrift-compact-legacy-optional-flatbuffers)
- [provides excellent portability and works on all platforms, while Thrift offers](#provides-excellent-portability-and-works-on-all-platforms-while-thrift-offers)
- [slightly better compression but has complex dependencies. Both formats are](#slightly-better-compression-but-has-complex-dependencies-both-formats-are)
- [automatically detected when reading filesystems, and you can choose the format](#automatically-detected-when-reading-filesystems-and-you-can-choose-the-format)
- [when creating new images with `--metadata-format=flatbuffers|thrift`.](#when-creating-new-images-with---metadata-formatflatbuffersthrift)
- [Look for the section about metadata serialization formats](#look-for-the-section-about-metadata-serialization-formats)
- [Update the metadata packing section around line 660-680 to mention both formats](#update-the-metadata-packing-section-around-line-660-680-to-mention-both-formats)
- [DwarFS](#dwarfs)
  - [Table of contents](#table-of-contents)
  - [What is DwarFS (in plain words)?](#what-is-dwarfs-in-plain-words)
  - [Why not just use .zip or .tar.gz?](#why-not-just-use-zip-or-targz)
  - [Performance comparison overview](#performance-comparison-overview)
    - [1139 complete Perl installations](#1139-complete-perl-installations)
    - [All artifacts from 205 DwarFS CI builds](#all-artifacts-from-205-dwarfs-ci-builds)
    - [Game audio assets from sonniss.com](#game-audio-assets-from-sonnisscom)

## What is DwarFS (in plain words)?

DwarFS is a **mountable archive**: you pack a directory into one file
(an "image"), then **open it instantly like a normal folder** — no full
extraction, no temporary files. It’s read-only when mounted, so you can
browse, open, and run files safely in place. This works particularly well
for big collections with lots of **similar or repeated content** — think
many versions of a project or dataset, folders of documents and text
files, backups/snapshots that mostly overlap, or libraries with many
near-duplicates. The image can be mounted in fractions of a second, you
can use the contents immediately with no delay, and you extract only if
you actually need to.

## Why not just use .zip or .tar.gz?

Traditional archives are fine for storage, but they’re usually slow to
open and awkward for random access (jumping around inside large files or
across many files). DwarFS is built for **fast random reads** *and*
**space savings**. It groups similar files and removes duplication, so
images are **often smaller** than a simple tar/zip, while reads stay
snappy — even when accessing lots of files simultaneously. In practice,
you keep huge directories compressed, mount them in milliseconds, and
work as if they were already unpacked.

## Performance comparison overview

This comparison uses DwarFS 0.14.1, 7-Zip 25.00 (x64), and `tar` +
[pigz](https://github.com/madler/pigz) for all `.tar.gz` tests since plain
`gzip` would have been incredibly slow. Both `.tar.gz` and `7zip` archives
were mounted using [fuse-archive](https://github.com/google/fuse-archive)
v1.16 with the `-olazycache` option since the default of caching the entire
archive in memory is infeasible for large archives.

### 1139 complete Perl installations

![Perl • Compression ratio](doc/perf/perl_compression_ratio.svg)
![Perl • Compression speed](doc/perf/perl_compression_speed.svg)
![Perl • Compression CPU time](doc/perf/perl_compression_cpu_time.svg)
![Perl • Decompression speed](doc/perf/perl_decompression_speed.svg)
![Perl • Decompression CPU time](doc/perf/perl_decompression_cpu_time.svg)
![Perl • Mount time](doc/perf/perl_mount_time.svg)
![Perl • Lookup speed](doc/perf/perl_lookup_speed.svg)
![Perl • Random access speed](doc/perf/perl_random_access_speed.svg)

| **Perl** (47.49 GiB, 1.9M files)      | .tar.gz [^pl1] | .tar.zst [^pl2] | 7zip (`-mx=7`) | SquashFS [^pl7] | DwarFS (lzma) | DwarFS (zstd) |
|---------------------------------------|---------------:|----------------:|---------------:|----------------:|--------------:|--------------:|
| Compression time                      |         4m 59s |          8m 06s |        23m 27s |          5m 37s |    **2m 13s** |         5m 3s |
| Compression CPU time                  |         1h 47m |          2h 18m |          5h 5m |          2h 29m |   **31m 17s** |       49m 51s |
| Compressed size                       |      12.17 GiB |       0.387 GiB |      1.219 GiB |       3.245 GiB | **0.310 GiB** |     0.352 GiB |
| Compression ratio                     |          3.902 |           122.7 |          38.96 |           14.63 |     **153.2** |         134.9 |
| Decompression time                    |         2m 19s |           57.2s |         1m 14s |       **39.3s** |        1m 14s |        1m 14s |
| Decompression CPU time                |         3m 44s |      **1m 21s** |         2m 28s |          1m 25s |        1m 47s |        1m 30s |
| Mount time                            |         2m 07s |      ❌  [^pl3] |         3.638s |          0.011s |        0.420s |    **0.009s** |
| Find all 1.9M files [^pl8]            |         5.670s |      ❌  [^pl3] |         5.695s |          5.311s |    **2.800s** |        2.821s |
| Checksum 1139 files (2.58 GiB) [^pl4] |     ❌  [^pl5] |      ❌  [^pl3] |     ~5h [^pl6] |          1.541s |        4.330s |    **1.134s** |

[^pl1]: using `pigz -9`
[^pl2]: using `zstd --long=31 --ultra -22 -T0`
[^pl3]: not supported by fuse-archive
[^pl4]: `$ ls -1 mnt/*/perl*/bin/perl5* | xargs -d $'\n' -n1 -P16 sha256sum`
[^pl5]: killed after making no progress for 15 minutes
[^pl6]: killed when only 78 files were finished after about 20 minutes
[^pl7]: using `-comp zstd -Xcompression-level 22 -b 1M -tailends`; using `squashfuse_ll` 0.6.0
[^pl8]: `$ fd -t f . mnt | wc -l`

### All artifacts from 205 DwarFS CI builds

![DwarFS CI • Compression ratio](doc/perf/dwarfsci_compression_ratio.svg)
![DwarFS CI • Compression speed](doc/perf/dwarfsci_compression_speed.svg)
![DwarFS CI • Compression CPU time](doc/perf/dwarfsci_compression_cpu_time.svg)
![DwarFS CI • Decompression speed](doc/perf/dwarfsci_decompression_speed.svg)
![DwarFS CI • Decompression CPU time](doc/perf/dwarfsci_decompression_cpu_time.svg)
![DwarFS CI • Mount time](doc/perf/dwarfsci_mount_time.svg)
![DwarFS CI • Random access speed](doc/perf/dwarfsci_random_access_speed.svg)

| **DwarFS CI** (465.2 GiB, 3.6M files) | .tar.gz (`pigz -9`) | 7zip (`-mx=7`) | DwarFS (zstd) |
|---------------------------------------|--------------------:|---------------:|--------------:|
| Compression time                      |         **40m 28s** |       141m 46s |        68m 6s |
| Compression CPU time                  |         **13h 55m** |        42h 47m |       31h 12m |
| Compressed size                       |           142.9 GiB |      60.04 GiB | **28.63 GiB** |
| Compression ratio                     |               3.255 |          7.748 |     **16.25** |
| Decompression time                    |             26m 17s |         8m 15s |    **7m 21s** |
| Decompression CPU time                |             35m 16s |        41m 29s |    **8m 59s** |
| Mount time                            |             22m 18s |          10.5s |    **0.024s** |
| Run 441 binaries (2.72 GiB) [^ci1]    |          ❌  [^ci2] |     ❌  [^ci3] |    **0.774s** |

[^ci1]: `$ ls -1 mnt/*/dwarfs-*-Linux-x86_64-*-minsize*/bin/mkdwarfs | xargs -d $'\n' -n1 -P16 sh -c '$0 -h'`
[^ci2]: killed after making no progress for 15 minutes
[^ci3]: killed after making no progress and consuming more than 32 GiB of RAM

### Game audio assets from sonniss.com

![Sonniss • Compression ratio](doc/perf/sonniss_compression_ratio.svg)
![Sonniss • Compression speed](doc/perf/sonniss_compression_speed.svg)
![Sonniss • Compression CPU time](doc/perf/sonniss_compression_cpu_time.svg)
![Sonniss • Decompression speed](doc/perf/sonniss_decompression_speed.svg)
![Sonniss • Decompression CPU time](doc/perf/sonniss_decompression_cpu_time.svg)
![Sonniss • Mount time](doc/perf/sonniss_mount_time.svg)
![Sonniss • Random access speed](doc/perf/sonniss_random_access_speed.svg)

| **Sonniss** (3.072 GiB, 171 files) [^wav1] | .tar.gz (`pigz -9`) | 7zip (`-mx=7`) | SquashFS [^wav3] | DwarFS (categorize) |
|--------------------------------------------|--------------------:|---------------:|-----------------:|--------------------:|
| Compression time                           |               5.34s |         6m 21s |           19.52s |           **3.98s** |
| Compression CPU time                       |               2m 1s |        19m 14s |           9m 47s |           **29.0s** |
| Compressed size                            |           2.725 GiB |      2.255 GiB |        2.711 GiB |       **1.664 GiB** |
| Compression ratio                          |               1.127 |          1.362 |            1.133 |           **1.846** |
| Decompression time                         |               13.7s |         1m 50s |       **0.774s** |               1.32s |
| Decompression CPU time                     |               17.8s |         1m 52s |        **4.60s** |               9.15s |
| Mount time                                 |               13.6s |         0.014s |           0.018s |          **0.008s** |
| Checksum all files [^wav2]                 |               3m 2s |        30m 42s |            2.81s |           **2.64s** |

[^wav1]: https://hippolytus.feralhosting.com/sonniss/Sonniss.com-GDC2024-GameAudioBundle1of9.zip
[^wav2]: `$ find mnt -type f | xargs -d $'\n' -n1 -P16 sha256sum`

## Tool Architecture (v0.16.0+)

Starting with v0.16.0, all DwarFS command-line tools follow a consistent **handler pattern** architecture that separates CLI parsing, business logic, and reusable library components.

### Benefits

- **Modular design**: 50-80% reduction in main() complexity
- **Reusable libraries**: Embed DwarFS in C++ applications
- **Testable components**: Each module tested independently
- **Consistent patterns**: All tools follow same structure

### Architecture

```
tool_main.cpp (thin CLI, ~350 lines)
  ├── options_parser (CLI arguments)
  └── handler (business logic)
      └── Library Classes (reusable)
```

### Library APIs for Embedding

DwarFS v0.16.0+ provides reusable library APIs:

**Filesystem Loading** (`dwarfs::reader::filesystem_loader`):
```cpp
#include <dwarfs/reader/filesystem_loader.h>

dwarfs::reader::filesystem_load_config config;
config.image_path = "myfs.dwarfs";
config.cache_size = 1_GiB;
auto fs = dwarfs::reader::filesystem_loader::load(logger, os, config);
```

**FUSE Operations** (`dwarfs::reader::fuse_driver`):
```cpp
#include <dwarfs/reader/fuse_driver.h>

dwarfs::reader::fuse_driver driver(std::move(fs), config);
driver.mount("/mnt/point");
```

### Platform Support

- **Linux**: FUSE2, FUSE3
- **macOS**: macFUSE, FUSE-T (userspace, no kernel extension)
- **Windows**: WinFsp
- **FreeBSD**: Linux compatibility layer

For detailed documentation, see:
- [`doc/mkdwarfs.md`](doc/mkdwarfs.md) - Create filesystems
- [`doc/dwarfs.md`](doc/dwarfs.md) - Mount filesystems
- [`doc/dwarfsck.md`](doc/dwarfsck.md) - Check/inspect
- [`doc/dwarfsextract.md`](doc/dwarfsextract.md) - Extract files

## Metadata Serialization Formats (v0.16.0+)

DwarFS v0.16.0 introduces support for two metadata serialization formats, allowing you to choose between portability and compression efficiency.

### FlatBuffers (Modern Default) ✅

**Status**: Fully supported, recommended for all new images

**Characteristics**:
- 🚀 Memory-mappable, zero-copy access
- 📦 Self-describing format (schema embedded)
- 🌍 Excellent cross-platform portability
- 📚 Header-only library (no complex dependencies)
- ♻️ Forward/backward compatible
- 📊 ~5-10% larger than Thrift

**Use cases**:
- New filesystem images (default since v0.16.0)
- Platforms where Thrift unavailable (Windows, older macOS, various architectures)
- Static linking scenarios
- Tebako integration
- Simplified builds

**Build requirement**: Always enabled (FlatBuffers is required)

### Thrift Compact (Legacy) ⚠️

**Status**: Optional, for backward compatibility only

**Characteristics**:
- 🚀 Memory-mappable, zero-copy access (Frozen2 bit-packed)
- 📦 Smallest format (bit-level packing)
- 🔧 Requires Apache Thrift + Facebook Folly
- 🚫 Platform limitations (no MSys2/MinGW, difficult static linking)
- 📑 2 sections (schema + data)

**Use cases**:
- Reading existing Thrift-format images
- Legacy compatibility

**Build requirement**: Optional (`-DDWARFS_WITH_THRIFT=OFF` to disable)

### Format Selection

**Creation** (`mkdwarfs`):
- Default: FlatBuffers format (since v0.16.0)
- All new images use FlatBuffers automatically

**Reading** (`dwarfs`, `dwarfsck`, `dwarfsextract`):
- 🔍 Automatic format detection via magic bytes
- Supports reading both formats (if Thrift enabled at build time)
- Format reported in `dwarfsck --json` output

**Build configurations**:
```bash
# FlatBuffers-only (recommended for new projects)
cmake -B build -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=OFF

# Dual-format (backward compatibility for reading old images)
cmake -B build -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=ON

# Thrift-only (supported since v0.16.0, for backward compatibility)
cmake -B build -DDWARFS_WITH_FLATBUFFERS=OFF -DDWARFS_WITH_THRIFT=ON
```

**Note**: Thrift-only builds are fully supported as of v0.16.0 but recommended only for reading legacy images. History tracking requires Thrift.

**Recommendation**: Use **FlatBuffers-only** for new projects. Enable Thrift only if you need to read legacy Thrift-format images or require history tracking.

**Performance Comparison**: See [`benchmark-results/THREE_WAY_MANUAL_SUMMARY.md`](benchmark-results/THREE_WAY_MANUAL_SUMMARY.md) for detailed comparison of all three build configurations.

[^wav3]: using `-comp zstd -Xcompression-level 22 -b 1M -tailends`
