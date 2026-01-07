# Session 25 Continuation Prompt

**Date**: 2025-12-22
**Objective**: Begin clean OOP architecture with domain model abstraction
**Approach**: Option B - Full redesign leveraging existing domain model

## Context

Sessions 22-24 revealed fundamental architectural coupling between Thrift and FlatBuffers metadata readers. The immediate fix attempts failed due to:
- Namespace collisions (`metadata_v2_data` defined in same namespace by both backends)
- Cross-format dependencies throughout code
- No clean abstraction layer

**CRITICAL DISCOVERY**: The project already has a domain model at [`include/dwarfs/metadata/domain/metadata.h`](../include/dwarfs/metadata/domain/metadata.h) used by the **writer** side. We will extend this architecture to the **reader** side for consistency and enable format re-encoding.

## Architecture Overview

**See**: [`SESSION_25_DOMAIN_MODEL_ARCHITECTURE_PLAN.md`](SESSION_25_DOMAIN_MODEL_ARCHITECTURE_PLAN.md)

```
Domain Model (format-agnostic)
      ↕
  Readers/Writers (Strategy Pattern)
      ↕
Wire Formats (Thrift Frozen2, FlatBuffers)
```

**Benefits**:
- ✅ Zero coupling between formats
- ✅ Re-encoding support (Thrift ↔ FlatBuffers)
- ✅ Easy to add new formats
- ✅ Clean testable interfaces

## Your Mission: Phase 1 - Analyze Domain Model

### Step 1: Read Existing Domain Model

Read [`include/dwarfs/metadata/domain/metadata.h`](../include/dwarfs/metadata/domain/metadata.h) to understand what structures already exist.

### Step 2: Map to Wire Formats

Compare domain model against:
- **Thrift schema**: [`thrift/metadata.thrift`](../thrift/metadata.thrift)
- **FlatBuffers schema**: [`flatbuffers/metadata.fbs`](../flatbuffers/metadata.fbs)

Create mapping table:
| Domain Model | Thrift Type | FlatBuffers Type | Notes |
|--------------|-------------|------------------|-------|
| ? | `thrift::metadata::inode` | `gen_flatbuffers::inode` | ? |

### Step 3: Identify Gaps

Document:
- What's missing from domain model
- What's extra in wire formats
- Conversion challenges (packed data, string tables, etc.)

### Step 4: Create Analysis Document

Write `doc/DOMAIN_MODEL_ANALYSIS.md` with:
- Complete domain model structure
- Wire format mappings
- Gap analysis
- Recommendations for extensions

## Expected Output

After Phase 1, you should have:

1. **Understanding** of existing domain model completeness
2. **Mapping** between domain ↔ Thrift ↔ FlatBuffers
3. **Gap analysis** showing what needs adding
4. **Analysis document** for next phases

## Revert Session 24 Changes First

Before starting, revert partial changes:

```bash
cd /Users/mulgogi/src/external/dwarfs
git checkout include/dwarfs/reader/internal/metadata_v2.h
git checkout include/dwarfs/reader/metadata_types.h  
git checkout src/reader/internal/metadata_v2_thrift.cpp
git checkout src/reader/internal/metadata_v2_flatbuffers.cpp
```

## Key Files to Read

### Domain Model (Priority 1)
- [`include/dwarfs/metadata/domain/metadata.h`](../include/dwarfs/metadata/domain/metadata.h)

### Wire Schemas (Priority 2)
- [`thrift/metadata.thrift`](../thrift/metadata.thrift) - Thrift schema
- [`flatbuffers/metadata.fbs`](../flatbuffers/metadata.fbs) - FlatBuffers schema

### Current Implementations (Priority 3)
- [`src/reader/internal/metadata_v2_thrift.cpp`](../src/reader/internal/metadata_v2_thrift.cpp) - Thrift reader
- [`src/reader/internal/metadata_v2_flatbuffers.cpp`](../src/reader/internal/metadata_v2_flatbuffers.cpp) - FlatBuffers reader

### Existing Serializers (Priority 4)
- [`src/metadata/serialization/`](../src/metadata/serialization/) - Current writer implementations

## Success Criteria for Phase 1

- ✅ Complete understanding of domain model
- ✅ Mapping table created (domain ↔ Thrift ↔ FlatBuffers)
- ✅ Gap analysis completed
- ✅ Analysis document written
- ✅ Ready to design reader interface (Phase 2)

## Timeline

**Phase 1 Estimate**: 2 hours
**Total Project Estimate**: 38-51 hours (12 phases)
**Target Completion**: ~3 weeks (assuming 2h sessions)

## Next Session After Phase 1

After completing domain model analysis:
- **Phase 2**: Design and implement reader interface
- **Phase 3**: Implement Thrift reader with converters
- **Phase 4**: Implement FlatBuffers reader with converters

## Critical Principles

1. **Full OOP** - Interfaces, abstractions, zero concrete coupling
2. **MECE** - Each format isolated, domain model complete
3. **Separation of Concerns** - Domain | IO | Wire Formats
4. **Extensibility** - Adding 3rd format = new Reader + Writer
5. **Correctness** - Architecture matters more than immediate tests passing

## Documentation Location

- **Plan**: [`SESSION_25_DOMAIN_MODEL_ARCHITECTURE_PLAN.md`](SESSION_25_DOMAIN_MODEL_ARCHITECTURE_PLAN.md)
- **Status**: [`SESSION_25_IMPLEMENTATION_STATUS.md`](SESSION_25_IMPLEMENTATION_STATUS.md)
- **This Prompt**: `SESSION_25_CONTINUATION_PROMPT.md`

---

**Ready to Start**: Yes
**First Action**: Revert Session 24 changes
**Second Action**: Read domain model header
**Goal**: Complete Phase 1 analysis document