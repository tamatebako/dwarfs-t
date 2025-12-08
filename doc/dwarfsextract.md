# dwarfsextract(1) -- extract DwarFS image

## SYNOPSIS

`dwarfsextract` `-i` *image* [`-o` *dir*] [*options*...]  
`dwarfsextract` `-i` *image* -f *format* [`-o` *file*] [*options*...]

## DESCRIPTION

**dwarfsextract** allows you to extract a DwarFS image, either directly
into another archive file, or to a directory on disk.

To extract the filesystem image to a directory, you can use:

    dwarfsextract -i image.dwarfs -o output-directory

The output directory must exist.

You can also rewrite the contents of the filesystem image as another
archive type, for example, to write a tar archive, you can use:

    dwarfsextract -i image.dwarfs -o output.tar -f ustar

For a list of supported formats, see libarchive-formats(5).

If you want to compress the output archive, you can use a pipeline:

    dwarfsextract -i image.dwarfs -f ustar | gzip > output.tar.gz

You could also use this as an alternative way to extract the files
to disk:

    dwarfsextract -i image.dwarfs -f cpio | cpio -id

## BENCHMARKING

**dwarfsextract** includes a benchmark mode for measuring extraction performance
and comparing different metadata formats (FlatBuffers vs Thrift).

### Benchmark Mode

Enable detailed metrics collection:

```bash
dwarfsextract --benchmark-mode -i image.dwarfs -o output/
```

### Output JSON

Export metrics to JSON file for automated analysis:

```bash
dwarfsextract --benchmark-mode --output-json=metrics.json \
    -i image.dwarfs -o output/
```

### Repeated Runs

Average results over multiple runs:

```bash
dwarfsextract --benchmark-mode --repeat=10 \
    --output-json=avg-metrics.json \
    -i image.dwarfs -o output/
```

### Metrics Collected

When benchmark mode is enabled, the following metrics are collected:

- **metadata_load_time**: Time to load and parse filesystem metadata (microseconds)
- **extraction_time**: Total extraction time (microseconds)
- **bytes_extracted**: Total bytes written to disk
- **files_extracted**: Number of regular files extracted
- **directories_extracted**: Number of directories created
- **symlinks_extracted**: Number of symbolic links created
- **hard_errors**: Number of fatal extraction errors
- **soft_errors**: Number of recoverable errors

### JSON Output Format

```json
{
  "image": "path/to/image.dwarfs",
  "repeat_count": 3,
  "runs": [
    {
      "metadata_load_us": 8234,
      "extraction_time_us": 180543,
      "bytes_extracted": 1024000,
      "files_extracted": 42,
      "directories_extracted": 5,
      "symlinks_extracted": 2,
      "hard_errors": 0,
      "soft_errors": 0
    }
  ]
}
```

### Format Comparison

To compare FlatBuffers and Thrift metadata formats:

```bash
# Extract from FlatBuffers image
dwarfsextract --benchmark-mode --repeat=5 \
    --output-json=fb-metrics.json \
    -i image-flatbuffers.dwarfs -o /tmp/extract-fb/

# Extract from Thrift image
dwarfsextract --benchmark-mode --repeat=5 \
    --output-json=th-metrics.json \
    -i image-thrift.dwarfs -o /tmp/extract-th/

# Compare results
python benchmarks/run_format_comparison.py \
    --flatbuffers-json=fb-metrics.json \
    --thrift-json=th-metrics.json \
    --output=comparison.md
```

See `benchmarks/run_format_comparison.py` for automated comparison tools.

## OPTIONS

- `-i`, `--input=`*file*:
  Path to the source filesystem.

- `-o`, `--output=`*directory*|*file*:
  If no format is specified, this is the directory to which the contents
  of the filesystem should be extracted. If a format is specified, this
  is the name of the output archive. This option can be omitted, in which
  case the default is to extract the files to the current directory, or
  to write the archive data to stdout.

- `--pattern=`*glob-pattern*:
  If specified, only extract entries matching the pattern. Can be specified
  multiple times, in which case all files matching one or more patterns will
  be extracted. Can also be specified without `--pattern` for simplicity.
  If prefixed with `i:`, the glob pattern match will be case-insensitive,
  e.g. `i:**/*.txt` would match all `.txt`, `.TXT` and `.tXt` files. If you
  want the *actual* pattern to start with `i:`, you have to prefix the
  pattern with `:`, i.e. `:i:...`. The glob patterns support the wildcards
  `*` and `?`, character classes (`[avt]`), ranges (`[a-h]`), complementation
  (`[!a-h]`) and globstar (`**`).

- `-O`, `--image-offset=`*value*|`auto`:
  Specify the byte offset at which the filesystem is located in the image.
  Use `auto` to detect the offset automatically. This is also the default.
  This is only useful for images that have some header located before the
  actual filesystem data.

- `-f`, `--format=`*format*:
  The archive format to produce. If this is left empty or unspecified,
  files will be extracted to the output directory (or the current directory
  if no output directory is specified). For a full list of supported formats,
  see libarchive-formats(5). If an output file is specified, `--format=auto`
  can be used to automatically determine the format and filters from the
  file name.

- `--format-filters=`*filters*:
  Comma-separated list of libarchive filters to apply to the selected format.
  Filters will be selected based on the file name with `--format=auto`. It is
  not supported to use both `--format=auto` and `--format-filters`.

- `--format-options=`*options*:
  Comma-separated libarchive options for the specific output format/filters.
  The options are passed to libarchive. For a full list of options for each
  output format, see archive_write_set_options(3).

- `--continue-on-error`:
  Try to continue with extraction even when errors are encountered. This
  only applies to errors when reading from the file system image. Errors
  when writing the extracted files will still be fatal.

- `--disable-integrity-check`:
  This option disables all block integrity checks on the file system data.
  There is a non-zero chance that this allows further data to be read from
  corrupted file systems. However, there's also a non-zero chance that it
  will completely crash the program. So please don't use this unless you
  know what you're doing.

- `--stdout-progress`:
  Write progress percentage to stdout. Useful for piping to tools like
  `zenity`.

- `-n`, `--num-workers=`*value*:
  Number of worker threads used for extracting the filesystem.

- `-s`, `--cache-size=`*value*:
  Size of the block cache, in bytes. You can append suffixes (`k`, `m`, `g`)
  to specify the size in KiB, MiB and GiB, respectively. Note that this is
  not the upper memory limit of the process, as there may be blocks in
  flight that are not stored in the cache. Also, each block that hasn't been
  fully decompressed yet will carry decompressor state along with it, which
  can use a significant amount of additional memory.

- `--log-level=`*name*:
  Specify a logging level.

- `--log-with-context`:
  Enable logging context regardless of level. By default, context is enabled
  if the level is `verbose`, `debug` or `trace`.

- `--perfmon=`*name*[`,`*name*...]:
  Enable performance monitoring for the list of comma-separated components.
  This option is only available if the project was built with performance
  monitoring enabled. Available components include `fuse`, `filesystem_v2`,
  `inode_reader_v2` and `block_cache`.

- `--perfmon-trace=`*file*:
  Write JSON trace data for all components enabled by `--perfmon` to this
  file when the process exits.

- `-h`, `--help`:
  Show program help, including option defaults.

- `--man`:
  If the project was built with support for built-in manual pages, this
  option will show the manual page. If supported by the terminal and a
  suitable pager (e.g. `less`) is found, the manual page is displayed
  in the pager.

## ENVIRONMENT VARIABLES

See [dwarfs-env(7)](dwarfs-env.md) for environment variables that
influence the behavior of `dwarfsextract`.

## AUTHOR

Written by Marcus Holland-Moritz.

## COPYRIGHT

Copyright (C) Marcus Holland-Moritz.

## SEE ALSO

[mkdwarfs(1)](mkdwarfs.md), [dwarfs(1)](dwarfs.md), [dwarfsck(1)](dwarfsck.md), [dwarfs-format(5)](dwarfs-format.md), [dwarfs-env(7)](dwarfs-env.md), libarchive-formats(5)
