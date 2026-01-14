# Tebako DwarFS v0.17.0 Pre-Release Checklist

**Date:** 2026-01-14
**Author:** Generated via brainstorming session
**Status:** Approved for Implementation
**Target Release:** v0.17.0 (First Tebako DwarFS Release)

---

## Executive Summary

This document provides a comprehensive pre-release checklist for Tebako DwarFS v0.17.0, the first official release of the Tebako fork. The checklist ensures production readiness across technical completeness, user experience, distribution, and post-release support.

**Key Objectives:**
1. Validate all 3 metadata formats work correctly
2. Ensure cross-platform build stability
3. Complete all traditional release activities
4. Establish distribution channels
5. Prepare for post-release maintenance

---

## Architecture Overview

The pre-release process is organized into 8 sections:

```
┌─────────────────────────────────────────────────────────────┐
│                  PRE-RELEASE PROCESS                        │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│  ┌──────────────────┐      ┌──────────────────┐            │
│  │  Section 1       │      │  Section 2       │            │
│  │  Technical       │─────▶│  Release         │            │
│  │  Validation      │      │  Artifacts       │            │
│  └──────────────────┘      └──────────────────┘            │
│           │                         │                        │
│           ▼                         ▼                        │
│  ┌──────────────────┐      ┌──────────────────┐            │
│  │  Section 3       │      │  Section 4       │            │
│  │  Documentation   │      │  Distribution    │            │
│  └──────────────────┘      └──────────────────┘            │
│           │                         │                        │
│           ▼                         ▼                        │
│  ┌──────────────────┐      ┌──────────────────┐            │
│  │  Section 5       │      │  Section 6       │            │
│  │  Announcement    │      │  Security        │            │
│  └──────────────────┘      └──────────────────┘            │
│           │                         │                        │
│           ▼                         ▼                        │
│  ┌──────────────────┐      ┌──────────────────┐            │
│  │  Section 7       │      │  Section 8       │            │
│  │  Post-Release    │      │  Sign-Off        │            │
│  └──────────────────┘      └──────────────────┘            │
│                                                               │
└─────────────────────────────────────────────────────────────┘
```

---

## Detailed Components

### Section 1: Technical Validation (40% Complete)

**Purpose:** Ensure all functionality works correctly across supported configurations.

**Subsections:**
1.1. Metadata Format Compatibility
   - FlatBuffers: ✅ Complete
   - Legacy Thrift: ❌ Needs test suite + Homebrew interoperability
   - Modern Thrift: ✅ Complete

1.2. Build Configurations
   - Default build (FlatBuffers + Legacy Thrift)
   - Thrift-enabled build (all 3 formats)

1.3. Benchmarking
   - Execute `scripts/benchmark-all.sh`
   - Test all formats across all operations
   - Update README with results

1.4. Example Application
   - Test `example/static-site-server`

1.5. Build Scripts
   - Validate `scripts/build-all-and-test.sh`
   - Clean up MECE violations

1.6. Platform Triplets
   - 16 configurations across Windows, macOS, Linux

1.7. Code Organization
   - Directory cleanup (reference: xz project)
   - GitHub workflows DRY

**Acceptance Criteria:** All tests pass, benchmarks complete, no critical bugs

---

### Section 2: Release Artifacts (0% Complete)

**Purpose:** Prepare all files needed for the release.

**Components:**
- Version management (CMakeLists.txt, CHANGES.md, man pages)
- Git tagging and GPG signing
- Source tarballs (.tar.gz, .tar.xz)
- Binary builds (macOS universal, Windows, Linux)
- SHA256 checksums

**Acceptance Criteria:** All artifacts generated, signed, and verified

---

### Section 3: User-Facing Documentation (20% Complete)

**Purpose:** Ensure users can successfully use and understand the release.

**Deliverables:**
- Updated README.md with benchmarks
- MIGRATION.md (upgrading from upstream)
- Breaking changes documentation
- KNOWN_ISSUES.md
- API/ABI documentation

**Acceptance Criteria:** Documentation reviewed by external user

---

### Section 4: Distribution Channels (0% Complete)

**Purpose:** Make the release available through all relevant channels.

**Channels:**
- GitHub Release
- Homebrew formula (new tap or homebrew-core)
- vcpkg port
- Optional: AUR, Scoop, Chocolatey, Conda

**Acceptance Criteria:** All channels tested and documented

---

### Section 5: Release Announcement (0% Complete)

**Purpose:** Communicate the release to the community.

**Components:**
- Comprehensive release notes
- Multi-channel announcement (GitHub, Reddit, HN, Twitter, LinkedIn)
- Demo content and examples

**Acceptance Criteria:** Announcement scheduled, content prepared

---

### Section 6: Security & Compliance (0% Complete)

**Purpose:** Ensure the release is secure and compliant.

**Checks:**
- Static/dynamic analysis
- Dependency vulnerability scan
- License compliance verification
- Data privacy review
- SECURITY.md creation

**Acceptance Criteria:** No critical vulnerabilities, all licenses documented

---

### Section 7: Post-Release Planning (0% Complete)

**Purpose:** Prepare for maintenance after release.

**Setup:**
- Issue tracking and triage process
- Continuous benchmarking for regression detection
- Documentation maintenance schedule
- v0.18.0 roadmap

**Acceptance Criteria:** Monitoring infrastructure in place

---

### Section 8: Sign-Off Checklist (0% Complete)

**Purpose:** Final approvals before release.

**Approvals Required:**
- Project lead
- Tech lead
- Documentation
- Security

**Acceptance Criteria:** All sign-offs obtained

---

## Dependencies

### Internal Dependencies

```
Section 2 (Release Artifacts)
    ├── depends on → Section 1 (Technical Validation)
    └── blocks → Sections 3, 4, 5

Section 3 (Documentation)
    ├── depends on → Section 1 (benchmark results)
    └── blocks → Section 5 (Announcement)

Section 4 (Distribution)
    ├── depends on → Section 2 (artifacts)
    └── depends on → Section 3 (docs)

Section 5 (Announcement)
    └── depends on → Sections 2, 3, 4
```

### External Dependencies

- Homebrew installation at `/opt/homebrew/Cellar/dwarfs/0.14.1_3`
- vcpkg installation with overlay ports
- CI/CD infrastructure (GitHub Actions)

---

## Success Criteria

The release is considered ready when:

1. **Technical Criteria:**
   - ✅ All 3 metadata formats serialize/deserialize correctly
   - ✅ All 16 platform triplets build successfully
   - ✅ 100% test pass rate maintained
   - ✅ Benchmarks complete and documented
   - ✅ No critical bugs known

2. **Documentation Criteria:**
   - ✅ README tested by fresh user
   - ✅ Migration guide prevents user confusion
   - ✅ API docs generate without errors
   - ✅ Breaking changes clearly documented

3. **Distribution Criteria:**
   - ✅ Release artifacts signed and verified
   - ✅ Homebrew formula installs correctly
   - ✅ vcpkg port builds all triplets
   - ✅ GitHub release page complete

4. **Security Criteria:**
   - ✅ No critical vulnerabilities
   - ✅ All licenses compatible
   - ✅ SECURITY.md published

---

## Risk Mitigation

### Known Risks

| Risk | Impact | Probability | Mitigation |
|------|--------|-------------|------------|
| Legacy Thrift incompatibility | HIGH | MEDIUM | Create test suite early (Section 1.1) |
| CI failures on exotic platforms | MEDIUM | LOW | Test incrementally, document known issues |
| Documentation incomplete | MEDIUM | LOW | External review, user testing |
| vcpkg/Homebrew delays | LOW | LOW | Prepare manual installation fallback |

### Contingency Plans

**If Legacy Thrift tests fail:**
- Document as known limitation
- Release as "experimental" support
- Plan v0.17.1 fix

**If CI fails on platforms:**
- Document supported platforms only
- Mark others as "best effort"
- Add to roadmap

**If documentation incomplete:**
- Release with TODO docs
- Add "Documentation Updates" to v0.17.1
- Solicit community contributions

---

## Timeline Estimate

| Section | Estimated Time | Dependencies |
|---------|---------------|--------------|
| 1. Technical Validation | 16-24 hours | None |
| 2. Release Artifacts | 4-6 hours | Section 1 |
| 3. Documentation | 8-12 hours | Section 1 |
| 4. Distribution | 6-8 hours | Sections 2, 3 |
| 5. Announcement | 4-6 hours | Sections 2, 3, 4 |
| 6. Security | 4-6 hours | Section 1 |
| 7. Post-Release | 4-6 hours | None |
| 8. Sign-Off | 2 hours | All above |
| **Total** | **48-78 hours** | |

**Recommended Schedule:** 2 weeks of focused work

---

## Quick Reference

### Critical Path (Must Complete in Order)

1. ✅ Legacy Thrift test suite (Section 1.1)
2. ✅ Benchmark execution (Section 1.3)
3. ✅ Release artifacts (Section 2)
4. ✅ Documentation updates (Section 3)
5. ✅ Distribution setup (Section 4)
6. ✅ Final release (Section 5)

### Can Parallelize

- Platform triplet testing (Section 1.6)
- Documentation drafting (Section 3)
- Security scanning (Section 6)
- Post-release setup (Section 7)

### Key Commands

```bash
# Full validation
./scripts/validate-release.sh

# Generate artifacts
./scripts/generate-release.sh 0.17.0

# Test suite
./scripts/build-all-and-test.sh --vcpkg

# Benchmarks
./scripts/benchmark-all.sh

# Homebrew interoperability
./scripts/test-homebrew-compat.sh
```

---

## Next Steps

1. **Immediate:** Start Section 1.1 (Legacy Thrift test suite)
2. **This Week:** Complete Section 1 (Technical Validation)
3. **Next Week:** Sections 2, 3, 4 (Artifacts, Docs, Distribution)
4. **Final Week:** Sections 5, 6, 7, 8 (Announce, Security, Post-Release, Sign-Off)

**Review Date:** 2026-01-21 (after Section 1 completion)

---

## Appendix: File Structure

```
dwarfs/
├── README.pre-release-checks.md    # Detailed checklist (created)
├── TODO.pre-release-checks.md      # Original TODO (reference)
├── docs/plans/
│   └── 2026-01-14-pre-release-checklist.md  # This file
├── scripts/
│   ├── validate-release.sh         # TODO: Create
│   ├── generate-release.sh         # TODO: Create
│   └── test-homebrew-compat.sh     # TODO: Create
└── docs/
    ├── MIGRATION.md                # TODO: Create
    ├── KNOWN_ISSUES.md             # TODO: Create
    └── SECURITY.md                 # TODO: Create
```

---

**Document Status:** ✅ Approved
**Implementation:** Ready to begin with Section 1
**Next Review:** After Technical Validation complete
