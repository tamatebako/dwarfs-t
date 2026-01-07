# DwarFS Environment Variables

## Overview

All DwarFS tools support configuration via environment variables. This allows you to:

- Set default options in shell profiles
- Configure tools in containerized environments
- Reduce repetitive command-line arguments in scripts
- Maintain consistent settings across sessions

Environment variables follow a consistent naming pattern and priority order across all tools.

## Naming Convention

All DwarFS environment variables use the pattern:

```
DWARFS_<TOOL>_<OPTION>
```

Where:
- `<TOOL>` is the tool name in uppercase: `MKDWARFS`, `DWARFS`, `DWARFSCK`, or `DWARFSEXTRACT`
- `<OPTION>` is the option name in uppercase with underscores

**Examples**:
- `DWARFS_MKDWARFS_COMPRESSION_LEVEL` - mkdwarfs compression level
- `DWARFS_DWARFS_CACHE_SIZE` - dwarfs cache size
- `DWARFS_DWARFSCK_NUM_WORKERS` - dwarfsck worker threads
- `DWARFS_DWARFSEXTRACT_NUM_WORKERS` - dwarfsextract worker threads

## Priority Order (MECE)

Environment variables follow a strict **Mutually Exclusive, Collectively Exhaustive** priority order:

1. **Command-line arguments** (highest priority) - Always override everything
2. **Environment variables** - Used if no CLI argument provided
3. **Default values** (lowest priority) - Used if neither CLI nor ENV set

**Example**:
```bash
export DWARFS_MKDWARFS_COMPRESSION_LEVEL=5
mkdwarfs -i /src -o fs.dff              # Uses level 5 (ENV)
mkdwarfs -i /src -o fs.dff -l 7         # Uses level 7 (CLI overrides ENV)
```

## Common Variables (All Tools)

These variables are supported by all DwarFS tools:

### DWARFS_<TOOL>_LOG_LEVEL

Set the logging level for the tool.

**Values**: `error`, `warn`, `info`, `verbose`, `debug`, `trace`

**Equivalent CLI**: `--log-level`, `-L`

**Example**:
```bash
export DWARFS_MKDWARFS_LOG_LEVEL=verbose
mkdwarfs -i /src -o fs.dff  # Verbose logging
```

## Tool-Specific Variables

### mkdwarfs Variables

#### DWARFS_MKDWARFS_COMPRESSION_LEVEL

Default compression level (0-9).

**Values**: Integer from 0 (fast) to 9 (best)

**Equivalent CLI**: `--compress-level`, `-l`

**Example**:
```bash
export DWARFS_MKDWARFS_COMPRESSION_LEVEL=7
mkdwarfs -i /data -o archive.dff
```

#### DWARFS_MKDWARFS_NUM_WORKERS

Number of compression worker threads.

**Values**: Positive integer (default: number of CPU cores)

**Equivalent CLI**: `--num-workers`, `-N`

**Example**:
```bash
export DWARFS_MKDWARFS_NUM_WORKERS=8
mkdwarfs -i /data -o archive.dff
```

#### DWARFS_MKDWARFS_MEMORY_LIMIT

Block manager memory limit.

**Values**: Size with unit (e.g., `2g`, `512m`)

**Equivalent CLI**: `--memory-limit`, `-L`

**Example**:
```bash
export DWARFS_MKDWARFS_MEMORY_LIMIT=4g
mkdwarfs -i /data -o archive.dff
```

#### DWARFS_MKDWARFS_BLOCK_SIZE_BITS

Block size as a power of 2.

**Values**: Integer from 10 to 30 (actual size = 2^N bytes)

**Equivalent CLI**: `--block-size-bits`, `-S`

**Example**:
```bash
export DWARFS_MKDWARFS_BLOCK_SIZE_BITS=22  # 4 MiB blocks
mkdwarfs -i /data -o archive.dff
```

### dwarfs Variables

#### DWARFS_DWARFS_CACHE_SIZE

Size of the block cache.

**Values**: Size with unit (e.g., `1g`, `512m`)

**Equivalent CLI**: `-o cachesize=SIZE`

**Example**:
```bash
export DWARFS_DWARFS_CACHE_SIZE=1g
dwarfs image.dff /mnt
```

#### DWARFS_DWARFS_NUM_WORKERS

Number of decompression worker threads.

**Values**: Positive integer (default: 2)

**Equivalent CLI**: `-o workers=N`

**Example**:
```bash
export DWARFS_DWARFS_NUM_WORKERS=4
dwarfs image.dff /mnt
```

### dwarfsck Variables

#### DWARFS_DWARFSCK_NUM_WORKERS

Number of worker threads for parallel operations.

**Values**: Positive integer

**Equivalent CLI**: `--num-workers`

**Example**:
```bash
export DWARFS_DWARFSCK_NUM_WORKERS=4
dwarfsck image.dff --check-integrity
```

#### DWARFS_DWARFSCK_CACHE_SIZE

Cache size for metadata operations.

**Values**: Size with unit (e.g., `512m`, `1g`)

**Example**:
```bash
export DWARFS_DWARFSCK_CACHE_SIZE=512m
dwarfsck image.dff
```

### dwarfsextract Variables

#### DWARFS_DWARFSEXTRACT_NUM_WORKERS

Number of extraction worker threads.

**Values**: Positive integer (default: number of CPU cores)

**Equivalent CLI**: `--num-workers`

**Example**:
```bash
export DWARFS_DWARFSEXTRACT_NUM_WORKERS=8
dwarfsextract -i image.dff -o /dest
```

#### DWARFS_DWARFSEXTRACT_CACHE_SIZE

Cache size for extraction operations.

**Values**: Size with unit (e.g., `512m`, `1g`)

**Example**:
```bash
export DWARFS_DWARFSEXTRACT_CACHE_SIZE=1g
dwarfsextract -i image.dff -o /dest
```

## Practical Examples

### System-Wide Defaults

Set default options for all users via `/etc/profile.d/dwarfs.sh`:

```bash
#!/bin/bash
# System-wide DwarFS defaults

# mkdwarfs: Moderate compression, utilize all cores
export DWARFS_MKDWARFS_COMPRESSION_LEVEL=5
export DWARFS_MKDWARFS_NUM_WORKERS=8

# dwarfs: 1 GiB cache, 4 workers
export DWARFS_DWARFS_CACHE_SIZE=1g
export DWARFS_DWARFS_NUM_WORKERS=4

# dwarfsextract: Parallel extraction
export DWARFS_DWARFSEXTRACT_NUM_WORKERS=8
```

### User-Specific Defaults

Add to `~/.bashrc` or `~/.zshrc`:

```bash
# DwarFS personal defaults
export DWARFS_MKDWARFS_COMPRESSION_LEVEL=7
export DWARFS_MKDWARFS_MEMORY_LIMIT=4g
export DWARFS_DWARFS_CACHE_SIZE=2g
```

### Container Environment

Perfect for Docker/Kubernetes:

```dockerfile
FROM alpine:latest
RUN apk add --no-cache dwarfs

# Configure DwarFS via environment
ENV DWARFS_MKDWARFS_COMPRESSION_LEVEL=3
ENV DWARFS_MKDWARFS_NUM_WORKERS=4
ENV DWARFS_DWARFS_CACHE_SIZE=512m

COPY entrypoint.sh /
ENTRYPOINT ["/entrypoint.sh"]
```

### Script Example

Reduce repetitive arguments:

```bash
#!/bin/bash
# Backup script with env var defaults

# Set defaults
export DWARFS_MKDWARFS_COMPRESSION_LEVEL=5
export DWARFS_MKDWARFS_NUM_WORKERS=8
export DWARFS_MKDWARFS_MEMORY_LIMIT=4g

# Simple invocation - uses env vars
mkdwarfs -i /data/project1 -o backups/project1.dff

# Override for specific case
mkdwarfs -i /data/important -o backups/important.dff -l 9
```

### CI/CD Pipeline

GitHub Actions example:

```yaml
jobs:
  create-filesystem:
    runs-on: ubuntu-latest
    env:
      DWARFS_MKDWARFS_COMPRESSION_LEVEL: 7
      DWARFS_MKDWARFS_NUM_WORKERS: 4
    steps:
      - name: Create DwarFS image
        run: mkdwarfs -i src -o release.dff
```

## Implementation Notes

### Case Sensitivity

Environment variable names are **case-sensitive**. Always use uppercase:

```bash
export DWARFS_MKDWARFS_COMPRESSION_LEVEL=5  # ✓ Correct
export dwarfs_mkdwarfs_compression_level=5  # ✗ Won't work
```

### Invalid Values

Invalid environment variable values are **silently ignored**, falling back to defaults:

```bash
export DWARFS_MKDWARFS_COMPRESSION_LEVEL=invalid
mkdwarfs -i /src -o fs.dff  # Uses default level (7)
```

### Empty Values

Empty strings are treated as "not set":

```bash
export DWARFS_MKDWARFS_COMPRESSION_LEVEL=  # No effect
mkdwarfs -i /src -o fs.dff                  # Uses default (7)
```

### Boolean Options

For boolean flags, use `1` for true, `0` or empty for false:

```bash
export DWARFS_MKDWARFS_FORCE_OVERWRITE=1  # Enable flag
export DWARFS_MKDWARFS_FORCE_OVERWRITE=0  # Disable flag
```

## Debugging

### Check Current Settings

Use `env | grep DWARFS_` to see all DwarFS environment variables:

```bash
$ env | grep DWARFS_
DWARFS_MKDWARFS_COMPRESSION_LEVEL=7
DWARFS_MKDWARFS_NUM_WORKERS=8
DWARFS_DWARFS_CACHE_SIZE=1g
```

### Verify Priority

Add verbose logging to see which settings are used:

```bash
export DWARFS_MKDWARFS_COMPRESSION_LEVEL=5
export DWARFS_MKDWARFS_LOG_LEVEL=verbose
mkdwarfs -i /src -o fs.dff -l 7  # Will show level 7 in use
```

### Unset Variables

To clear an environment variable:

```bash
unset DWARFS_MKDWARFS_COMPRESSION_LEVEL
```

## Migration from Command-Line Only

If you're migrating from command-line only usage:

**Before** (repetitive):
```bash
mkdwarfs -i /data1 -o fs1.dff -l 7 -N 8
mkdwarfs -i /data2 -o fs2.dff -l 7 -N 8
mkdwarfs -i /data3 -o fs3.dff -l 7 -N 8
```

**After** (cleaner):
```bash
export DWARFS_MKDWARFS_COMPRESSION_LEVEL=7
export DWARFS_MKDWARFS_NUM_WORKERS=8

mkdwarfs -i /data1 -o fs1.dff
mkdwarfs -i /data2 -o fs2.dff
mkdwarfs -i /data3 -o fs3.dff
```

## See Also

- [mkdwarfs manual](mkdwarfs.md) - Complete mkdwarfs options
- [dwarfs manual](dwarfs.md) - Complete dwarfs options
- [dwarfsck manual](dwarfsck.md) - Complete dwarfsck options
- [dwarfsextract manual](dwarfsextract.md) - Complete dwarfsextract options
- [Environment variables manual](dwarfs-env.md) - Additional runtime variables

## Version

This documentation applies to DwarFS v0.16.0 and later.

Earlier versions do not support environment variable configuration.