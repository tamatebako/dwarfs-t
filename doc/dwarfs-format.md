# dwarfs-format(5) -- DwarFS File System Format v2.5

## DESCRIPTION

This document describes the DwarFS file system format, version 2.5.

## TERMINOLOGY

### High-Level Terms

- **DwarFS image** or **DwarFS file system image**: A file that contains
  a DwarFS file system. **DwarFS archive** is another commonly used term.

- **Section**: A contiguous part of a DwarFS image that contains either
  file system data or metadata. Each section has a header with a magic,
  version number, type, length and hashes for integrity checking.

- **Header**: An optional, arbitrary prefix before the first section of a
  DwarFS image. Typically, this is a shell script or other executable that
  intends to use the bundled DwarFS image. The DwarFS binary tools use an
  efficient algorithm to automatically skip this header.

- **Block**: A section of type `BLOCK` that contains file data. There can be
  an arbitrary number of `BLOCK` sections in a DwarFS image.

- **Metadata**: A section of type `METADATA_V2` that contains all metadata
  to interpret the file system. Without the metadata, the structure of the
  file system is completely lost and all you will have is a collection of
  shredded bits and pieces of file data with no association to the original
  files.

- **Inode**: An entry in the `inodes` list in the metadata. Each inode
  represents a file system object, i.e. a directory, regular file, symlink,
  device, socket or pipe.

- **Directory Entry**: An entry in the `dir_entries` list in the metadata.
  Each directory entry associates a name with an inode number. Directory
  entries are grouped by directory using the `directories` list. Also, within
  a single directory, the entries are sorted asciibetically by name.

- **Chunk**: A part of a file that references a contiguous range of bytes
  in a single `BLOCK`. Each regular file inode references a list of chunks
  that, when concatenated, make up the contents of the file.

- **Shared File**: A regular file that shares its contents with one or more
  other regular files. This is similar to hardlinks, but on a "file level"
  instead of an "inode level". Each shared file has its own inode, but the
  inodes reference the same list of chunks through the `shared_files_table`.

### Internal Terms and Types

- **`file_view`**: An abstraction representing a single file in a file system
  and allowing read access to its contents. The exact mechanism to access the
  file contents is implementation-defined. Different implementation have
  different trade-offs in terms of memory usage, speed and error handling.
  For example, the default on 64-bit systems is to memory-map the entire file.
  While this is usually *extremely* fast, there are no means for gracefully
  handling I/O errors and they will typically result in the process crashing
  with a bus error (`SIGBUS`). On 32-bit systems, the default is to only
  memory-map "segments" of the file at a time. This limits the amount of
  address space used, bus uses more system calls and is thus slower. On
  either platform, it is also possible to use an implementation that reads
  data into allocated buffers. This is the slowest option and the one that
  will use the most process memory, but it is the only option that allows for
  graceful handling of I/O errors.

- **`file_extent`**: Using a `file_view`, you can iterate over the "extents"
  of a file. Extents are contiguous ranges of either data or holes. In sparse
  files, holes are ranges of zeros that do not actually occupy any space in the
  file system. This allows the code to efficiently skip over these holes if
  possible.

- **`file_segment`**: A contiguous range of data within a file. You can get
  a `file_segment` either directly from a `file_view` (using offset and size),
  or by using `file_extent::segments()` to iterate over a range of segments
  given the preferred segment size and an optional overlap between segments.
  A `file_segment` is *always* backed by contiguous memory representing the
  corresponding range of data in the file.

- **Fragments** are contiguous ranges of categorized file data. A categorizer
  can split each file into a sequence of fragments, each of which will be
  assigned a category. For example, the `pcmaudio_categorizer` will typically
  split an audio file into a `pcmaudio/metadata` fragment at the start, followed
  by a `pcmaudio/waveform` fragment, optionally followed by `pcmaudio/metadata`
  if there's any trailing metadata. The fragments from each category will be
  processed as separate streams: that is, all `pcmaudio/metadata` fragments will
  be ordered (e.g. by similarity) and the fed into the segmenter, which will
  further split each fragment into "chunks" (these are the chunks that will
  eventually be stored in the `chunk_table` in the metadata).

- **Chunks** are the smallest entity of contiguous file data that DwarFS
  manages. The segmenter splits data into chunks, adding references to data
  it has already seen if possible.

## FILE STRUCTURE

A DwarFS file system image is just a sequence of sections, optionally
prefixed by a "header", which is typically some sort of shell script
or other executable that intends to use the "bundled" DwarFS image.

Each section in the DwarFS image has the following format:

         ┌───┬───┬───┬───┬───┬───┬───┬───┐
    0x00 │'D'│'W'│'A'│'R'│'F'│'S'│MAJ│MIN│  MAJ=0x02, MIN=0x05 for v2.5
         ├───┴───┴───┴───┴───┴───┴───┴───┤
    0x08 │                               │  Used for full (slow) integrity
         ├─ SHA-512/256 integrity hash  ─┤  check with `dwarfsck`.
    0x10 │  over the remainder of the    │
         ├─ section data, starting at   ─┤
    0x18 │  offset 0x28.                 │
         ├─                             ─┤
    0x20 │                               │
         ├───────────────────────────────┤
    0x28 │  XXH3-64 hash over remainder  │  Used for fast integrity check.
         ├───────────────┬───────┬───────┤
    0x30 │Section Number │SecType│CompAlg│  All integer fields are in LE
         ├───────────────┴───────┴───────┤  byte order.
    0x38 │   Length of remaining data    │
         ├───────────────────────────────┤
    0x40 │                               │
         │ Section data compressed using │
         │ CompAlg algorithm.            │
         │                               │
         │                               │
         │                               │
         └───────────────────────────────┘

A couple of notes:

- No padding is added between sections.

- The list of sections can easily be traversed by using the length field
  to skip to the start of the next section.

- Corruption can easily be detected using the XXH3-64 hash. Computation
  of this hash is so fast that it is in fact checked every single time a
  file system section is loaded.

- Integrity can furthermore be checked using the SHA-512/256 hash. This
  is much slower, but should rarely be needed.

- All header fields, except for the magic and version number, are
  protected by the hashes.

- In case of corruption, sections can easily be retrieved by scanning
  for the magic. The version number can be recovered by looking at all
  sections and choosing the majority. The explicit section number helps
  to recover data if multiple sections are missing.

- A major version number change will render the format incompatible.

- A minor version number change will be backwards compatible, i.e. an
  old program will refuse to read a file system with a minor version
  larger than the one it supports. However, a new program can still
  read all file systems with a smaller minor version number, although
  very old versions may at some point no longer be supported.

### Header Detection

In order to access the file system data when it is prefixed by a header,
the size of the header must be known. It can either be given to the
tools or the FUSE driver explicitly (using e.g. the `--image-offset` or
`-o offset` options), or it can be determined automatically (by passing
`auto` as the argument to the aforementioned options).

Automatic detection works by scanning the file for the section header
magic (`DWARFS`) and validating the match by looking up the second
section header using the length of the first section and also checking
its magic. It is rather unlikely that a file is created accidentally
that would pass this check, although one could be crafted manually
without any problems.

### Section Types

Currently, the following different section types are defined:

- `BLOCK` (0):
  A block of data. This is where all file data is stored. There can be an
  arbitrary number of sections of this type. The file data in these `BLOCK`s
  can only be interpreted using the metadata section. The metadata contains
  a list of chunks for each file, each of which references a small part of
  the data in a single `BLOCK`.

- `METADATA_V2_SCHEMA` (7):
  **Used only by `thrift` format:** The
  [schema](https://github.com/facebook/fbthrift/blob/main/thrift/lib/thrift/frozen.thrift)
  used to layout the `METADATA_V2` section contents. This is stored in
  "compact" thrift encoding. The metadata cannot be read without the
  schema, as it defines the exact bit widths used to store each metadata
  field. The `flatbuffers` format does not use this section type
  as it is self-describing.

- `METADATA_V2` (8):
  This section contains the bulk of the metadata. The format used depends
  on the metadata serialization format:

  * **For `thrift` format** (legacy, optional): A collection of bit-packed arrays and structures.
    The exact layout of each list and structure depends on the actual data and
    is stored separately in `METADATA_V2_SCHEMA`. The metadata definition is in
    [metadata.thrift](../thrift/metadata.thrift) and the binary format uses
    [Frozen2](https://github.com/facebook/fbthrift/blob/main/thrift/lib/cpp2/frozen/Frozen.h),
    which is extremely space efficient and allows accessing huge data structures
    directly through memory-mapping.

  * **For `flatbuffers` format** (modern default, required): A self-describing portable binary
    format that provides memory-mappable, zero-copy access. The metadata structures are defined
    in [`flatbuffers/metadata.fbs`](../flatbuffers/metadata.fbs). No separate schema section
    is needed. This format offers excellent portability and works on all platforms.

- `SECTION_INDEX` (9):
  The section index is, well, an index of all sections in the file
  system. If present (creation of the index can be suppressed with
  `--no-section-index`), this is *required* to be the last section.
  Each entry in the section index is a 64-bit value with the upper
  16 bits being the section type and the lower 48 bits being the
  offset relative to the first section. That is, the section index
  is independent of whether or not a header is present before the
  first section. The whole point of the section index is to avoid
  having to build an index by visiting all section headers. Since
  the offsets in the index are sorted, the section index is *always*
  stored uncompressed, and the section index *must* be the last
  section, you can find the start of the section index by reading
  the last 64-bit value from the image file, checking if the upper
  16 bits match the `SECTION_INDEX` type, and then add the image
  offset (header size) to the lower 48 bits. At that position in
  the file, you should find a valid section header for the section
  index.

- `HISTORY` (10):
  File system history information as defined `thrift/history.thrift`.
  This is stored in "compact" thrift encoding. Zero or more history
  sections are supported. This section type is purely informational
  and not needed to read the DwarFS image.

### Compression Algorithms

DwarFS supports a wide range of section compression algorithms, some of
which require additional metadata. The full list of supported algorithms
is defined in [`dwarfs/compression.h`](../include/dwarfs/compression.h).

For compression algorithms with metadata, the compression algorithm metadata
structures are serialized using the same format as the main metadata:

* **`thrift` format**: Compression algorithm metadata is defined in
  [`thrift/compression.thrift`](../thrift/compression.thrift) and stored in
  compact thrift encoding at the beginning of the section, just after the
  header.

* **`flatbuffers` format**: Compression algorithm metadata is defined in
  the FlatBuffers schema and stored in FlatBuffers binary format at the
  beginning of the section, just after the header.

### Metadata Serialization Formats

DwarFS supports two different metadata serialization formats: `flatbuffers`
(modern default) and `thrift` (legacy). The serialization format affects both
the size of the metadata and the performance characteristics when reading it.

#### Core Domain Model

Both serialization formats serialize the same core domain model defined in
[`include/dwarfs/metadata/domain/metadata.h`](../include/dwarfs/metadata/domain/metadata.h).
This ensures that filesystem semantics remain identical regardless of which
format is chosen. The domain model includes all metadata structures such as
inodes, directory entries, chunks, symlinks, and devices.

#### Format Definitions and Schema Handling

The two formats differ in how they define their serialization schemas:

* **`flatbuffers` format** (modern default, required): Uses the FlatBuffers schema
  language to define the metadata schema in
  [`flatbuffers/metadata.fbs`](../flatbuffers/metadata.fbs). The format is
  self-describing and does not require a separate schema section. All metadata is
  stored in a single `METADATA_V2` section. This is a header-only dependency with
  excellent cross-platform support.

* **`thrift` format** (legacy, optional): Uses external IDL (Interface Definition
  Language) files to define the metadata schema. The schema is defined in
  [`thrift/metadata.thrift`](../thrift/metadata.thrift) and compiled at
  filesystem creation time. The compiled schema is stored separately in the
  `METADATA_V2_SCHEMA` section, and the serialized metadata is stored in the
  `METADATA_V2` section. This two-section approach allows the Frozen2 layout to
  use bit-packed, memory-mappable structures with field widths optimized for the
  actual data.

#### Format Selection Rationale

These formats were chosen to provide flexibility on platform compatibility. The
original `thrift` format depends on the Apache Thrift library, which in turn
depends on Folly. These dependencies are complex and not compatible with all build
environments (particularly static linking scenarios, Windows variants, and some
architectures). The `flatbuffers` format is now the required default and provides
excellent portability with minimal dependencies.

DwarFS can read both formats (as long as the build dependencies are available on
the build system), and they both have similar levels of performance and resource
needs. The choice of format is thus based on platform compatibility and build
requirements.

#### Technical Characteristics

* The **`flatbuffers`** format provides memory-mappable, zero-copy access with a
  self-describing binary format. It works on all platforms, requires only header
  files, and is excellent for forward/backward compatibility. Size is slightly
  larger than Thrift (~5-10%), but portability is substantially better.

* The **`thrift`** format uses Apache Thrift's Compact Protocol with Facebook's
  Frozen2 layout for bit-packed, memory-mappable structures. Field widths are
  optimized based on actual data values, resulting in extremely compact
  representation. However, it requires complex dependencies (Folly + fbthrift)
  that are difficult to build on some platforms.

When mounting or extracting a filesystem, the format is automatically detected,
so no special options are needed.

* To select a format when creating a filesystem, use the `--metadata-format`
  option with `mkdwarfs`. Note that FlatBuffers is now the default.

* To convert between formats, use
  `mkdwarfs --recompress=metadata --rebuild-metadata` with the desired
  `--metadata-format`. Note that this only changes the metadata format; file
  data blocks remain unchanged unless `--recompress=all` is used.

For detailed performance comparisons and benchmarking methodology, see
[benchmark-metadata(7)](benchmark-metadata.md).

## METADATA FORMAT

Here is a high-level overview of how all the bits and pieces relate
to each other:

    ═════════════           ┌─────────────────────────────────────────────────────────────────────────┐
     DwarFS v2.5            │                                                                         │
    ═════════════           │         ┌───────────────────────────────────────────┐                   │
                            │         │                                           │                   │
              dir_entries[] ▼         │              inodes[]                     │   directories[]   │
    ╔════╗   ┌────────────────┐       │  S_IFDIR ──►┌───────────────────┐         │  ┌────────────────┴─┐
    ║root╟──►│ name_index:  0 │       │             │ mode_index:     0 ├──────┐  └─►│ parent_entry:  0 │
    ╚════╝   │ inode_num:   0 ├───────┴────────────►│ owner_index:    0 │      │     │ first_entry:   1 │
             ├────────────────┤                     │ group_index:    0 │      │     | self_entry:    0 |
         ┌───┤ name_index:  2 │                     │ *time_offset: 417 │      │     ├──────────────────┤
    ┌────┼───┤ inode_num:   5 ├───────┐             │ *time_subsec:  13 │      │     │ parent_entry:  0 │
    │    │   ├────────────────┤       │             │ nlink_minus_1:  0 │      │     │ first_entry:  11 │
    │ ┌──┼───┤ name_index:  3 │       │             ├───────────────────┤      │     | self_entry:    1 |
    │ │  │   │ inode_num:   9 ├────┐  │             │        ...        │      │     ├──────────────────┤
    │ │  │   ├────────────────┤    │  │  S_IFLNK ──►├───────────────────┤      │     │ parent_entry:  5 │
    │ │  │   │                │    │  │             │ mode_index:     2 │      │     │ first_entry:  12 │
    │ │  │   │      ...       │    │  │             │ owner_index:    2 │      │     | self_entry:    7 |
    │ │  │   │                │    │  │             │ group_index:    0 │      │     ├──────────────────┤
    │ │  │   └────────────────┘    │             │ *time_offset: 298 │      │     │       ...        │
    │ │  │                         │             │ *time_subsec:  88 │      │     └──────────────────┘
    │ │  │                         │             │ nlink_minus_1:  0 │      │
    │ │  │    names[]              │             ├───────────────────┤      │      modes[]
    │ │  │   ┌────────────┐        │             │        ...        │      │     ┌─────────────┐
    │ │  │   │ "usr"      │        │     S_IFREG ──►├───────────────────┤      └────►│   0040775   │
    │ │  │   ├────────────┤        │     (unique)   │ mode_index:     1 │            ├─────────────┤
    │ │  │   │ "share"    │        ├───────────────►│ owner_index:    0 ├──────┐     │   0100644   │
    │ │  │   ├────────────┤        │                │ group_index:    0 │      │     ├─────────────┤
    │ │  │   │ "words"    │        │                │ *time_offset: 298 │      │     │     ...     │
    │ │  │   ├────────────┤        │                │ *time_subsec:  94 │      │     └─────────────┘
    │ │  │   │ "lib"      │        │                │ nlink_minus_1:  2 │      │
    │ │  │   ├────────────┤        │                ├───────────────────┤      │      uids[]
    │ │   ┌─►│ "ls"       │        │                │        ...        │      │     ┌─────────────┐
    │ │   │ │  │ "true"   │        │  │             │ owner_index:    2 │            ├─────────────┤
    ▼▼▼▼▼ │ │          │        │  │             │ group_index:    0 │      │     ├─────────────┤
( inode-off )▼▼▼▼▼▼▼▼▼▼▼▼▼      │  │             │ *time_offset: 298 │      │     │     ...     │
(               )▼▼▼▼▼▼▼▼▼▼▼▼▼▼   │  │             │ *time_subsec:  38 │      │     └─────────────┘
(          inode-off )▼▼▼▼▼▼▼▼▼▼□▼│  │             │ nlink_minus_1:  0 │      │
└─────────────┘►(inode-off)      │  │             ├───────────────────┤      │      gids[]
               │     │             │             │        ...        │      │     ┌─────────────┐
               │     ▼             │             │                   │      │     │       0     │
               ├────►               │             └───────────────────┘      │     ├─────────────┤
               │   │      shared_files_table[]  ┌───────────┐                  │     │     ...     │
               ├────┼─►(inode-off)             │ "files-id"│
                        ▼                        ├───────────┤
              devices[]  │ (inode-off)                 │ "files-ref" │
             ┌────────────┐  │                          ├───────────┤
             │   0x0107   │  │                          │ "files-id"  │
             ├────────────┤  │                          ├───────────┤
             │   0x0502   │◄─┘                          │ "files-ref" │
             ├────────────┤                             ├───────────┤
             │    ...     │                             │ "files-id"  │
             └────────────┘                             ├───────────┤
              ┌────────────────┐                         │ "files-ref" │
              ├────────────────┤                         ├───────────┤
              │ "foo/bar"      ├───┬────→ child id      │ "files-id"  │
              ├────────────────┤   │                    ├───────────┤
              │ "foo/lib"      ├───┘                 ┌►├───────────┤
              ├────────────────┤                      │ │ dir_id      │
              │ "foo/baz"      ├───┬────→ parent id   │ │             │
              ├────────────────┤   │                    ├─►├─────────┤
              │ "/boot/ls"     ├─►├─────→ inode num   │ │ file_id     │
              ├────────────────┤   │                    ├─►└─────────┤
              │ "true"         ├───┘                 ┌►│     ...       │
              ├───┬───┬───┬ враG")


class ProducentSwapType(Enum):
    """Rodzaje swap'ow na producentach."""

    CPU_CORE = 6
    MEMORY_CHANNEL = 4
    LOGICAL_ROOT_DEVICE = 3
    UNALLOCATED = 1
    UNKNOWN = 2
