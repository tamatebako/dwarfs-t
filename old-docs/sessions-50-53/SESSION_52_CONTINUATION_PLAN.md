# Session 52: Environment Variable Documentation

**Date**: TBD (after Session 51)
**Previous Session**: 51 (--man flag integration - COMPLETE)
**Status**: Planned
**Priority**: Medium (user-facing feature)
**Duration**: ~2 hours

---

## Executive Summary

Document the environment variable support infrastructure that was implemented in Session 50. Users need comprehensive documentation to understand how to use environment variables to configure DwarFS tools.

---

## Current State

✅ **Infrastructure Complete** (Session 50):
- All 4 tools support environment variables via `argtable3_base_parser::load_environment_variables()`
- Pattern: `DWARFS_<TOOL>_<OPTION>`
- Priority: CLI > ENV > defaults (MECE)
- Infrastructure tested and working

❌ **Documentation Missing**:
- No user-facing documentation
- Format and examples not documented
- Priority rules not explained

---

## Objectives

1. Create comprehensive environment variable reference document
2. Update tool manpages with environment variable sections
3. Add examples to README.adoc
4. Document priority rules (MECE)

---

## Tasks

### Task 1: Create Comprehensive Reference (1 hour)

**File**: `doc/ENVIRONMENT_VARIABLES.md` (NEW)

**Content Structure**:
```markdown
# DwarFS Environment Variables

## Overview
DwarFS tools support environment variables for all command-line options...

## Naming Convention
DWARFS_<TOOL>_<OPTION>

## Priority Order (MECE)
1. Command-line arguments (highest priority)
2. Environment variables
3. Default values (lowest priority)

## Tool-Specific Variables

### mkdwarfs
- DWARFS_MKDWARFS_COMPRESSION_LEVEL
- DWARFS_MKDWARFS_NUM_WORKERS
- DWARFS_MKDWARFS_BLOCK_SIZE_BITS
- ...

### dwarfs
- DWARFS_DWARFS_CACHE_SIZE
- DWARFS_DWARFS_NUM_WORKERS
- ...

### dwarfsck
- DWARFS_DWARFSCK_CACHE_SIZE
- ...

### dwarfsextract
- DWARFS_DWARFSEXTRACT_NUM_WORKERS
- ...

## Common Variables (All Tools)
- DWARFS_LOG_LEVEL
- DWARFS_VERBOSE

## Examples
[Practical examples for each tool]

## Notes
- Environment variables are case-sensitive
- Invalid values are ignored (defaults used)
- CLI always overrides environment variables
```

### Task 2: Update Tool Manpages (30 min)

**Files to Update**:
- `doc/mkdwarfs.md`
- `doc/dwarfs.md`
- `doc/dwarfsck.md`
- `doc/dwarfsextract.md`

**Add Section** (template):
```markdown
## ENVIRONMENT VARIABLES

All options can be configured via environment variables using the pattern:

    DWARFS_<TOOL>_<OPTION>=value

For example:

    export DWARFS_MKDWARFS_COMPRESSION_LEVEL=5
    mkdwarfs -i /src -o fs.dff  # Uses level 5

Command-line arguments always take precedence over environment variables.

See ENVIRONMENT_VARIABLES.md for complete reference.
```

### Task 3: Update README.adoc (15 min)

**File**: `README.adoc`

**Add Section**:
```adoc
== Environment Variables

DwarFS tools support configuration via environment variables. This is useful for:

* Setting default options in shell profiles
* Configuring tools in containerized environments
* Scripting without repetitive command-line arguments

Example:
[source,bash]
----
export DWARFS_MKDWARFS_COMPRESSION_LEVEL=7
export DWARFS_MKDWARFS_NUM_WORKERS=8
mkdwarfs -i /data -o archive.dff
----

See link:doc/ENVIRONMENT_VARIABLES.md[ENVIRONMENT_VARIABLES.md] for complete documentation.
```

### Task 4: Format-Specific Examples (15 min)

**Create examples for each tool** showing real-world use cases:

**mkdwarfs**:
```bash
# System-wide defaults
export DWARFS_MKDWARFS_COMPRESSION_LEVEL=5
export DWARFS_MKDWARFS_NUM_WORKERS=8

# CLI can override
mkdwarfs -i /src -o fs.dff -l 7  # Uses level 7, workers 8
```

**dwarfs**:
```bash
# Mount with standard cache settings
export DWARFS_DWARFS_CACHE_SIZE=1g
dwarfs image.dff /mnt
```

**dwarfsck**:
```bash
export DWARFS_DWARFSCK_NUM_WORKERS=4
dwarfsck image.dff --check-integrity
```

**dwarfsextract**:
```bash
export DWARFS_DWARFSEXTRACT_NUM_WORKERS=8
dwarfsextract -i image.dff -o /dest
```

---

## Files to Create/Modify

### New Files (1)
- `doc/ENVIRONMENT_VARIABLES.md` - Comprehensive reference

### Updated Files (5)
- `README.adoc` - Add environment variable section
- `doc/mkdwarfs.md` - Add environment variable section
- `doc/dwarfs.md` - Add environment variable section
- `doc/dwarfsck.md` - Add environment variable section
- `doc/dwarfsextract.md` - Add environment variable section

**Total**: 6 files

---

## Success Criteria

✅ Comprehensive reference document created
✅ All tool manpages updated with environment variable sections
✅ README.adoc updated with examples
✅ Priority rules clearly documented (MECE)
✅ Practical examples for each tool
✅ Format is clear and easy to understand

---

## Testing

No code changes, documentation only. Verification:
1. Read through all documentation for clarity
2. Test examples to ensure they work
3. Verify MECE property of priority rules

---

## Timeline

| Task | Duration | Dependencies |
|------|----------|--------------|
| Create reference doc | 1 hour | None |
| Update tool manpages | 30 min | Reference doc |
| Update README.adoc | 15 min | Reference doc |
| Add examples | 15 min | All above |
| **Total** | **2 hours** | - |

---

## Next Session

**Session 53**: Enhancement 3 - Environment variable testing (2 hours)

---

**Status**: Planned
**Ready to Start**: Yes (no blockers)
**Priority**: Medium