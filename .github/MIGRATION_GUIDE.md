# CI/CD Migration Guide

This guide helps you transition from the old CI/CD workflows to the new unified architecture.

**Migration Date**: 2025-01-19
**Status**: Ready for migration

---

## Quick Migration Summary

| Old Workflow | New Workflow | Action Required |
|-------------|--------------|-----------------|
| `build.yml` | `ci-main.yml` (for main branch) | Update references |
| `build.yml` | `pr-validation.yml` (for PRs) | Update references |
| `build-test.yml` | `_build-test-reusable.yml` | Update callers |
| `vcpkg-triplet-matrix.yml` | `scheduled.yml` | Update references |

---

## For Contributors

### Pull Request Workflow

**Before:**
```yaml
# You had no specific PR workflow
# PRs triggered build.yml (if enabled)
```

**After:**
```yaml
# PRs now trigger pr-validation.yml automatically
# Faster feedback (~15 min instead of ~45 min)
# Tests on: ubuntu-latest (x64), macos-14 (ARM64)
```

**No action required** - PRs automatically use the new workflow.

### Before Pushing Changes

**Before:**
```bash
# You might have run build.yml manually
gh workflow run build.yml
```

**After:**
```bash
# Use the dedicated workflows
gh workflow run pr-validation.yml  # For PR-style testing
gh workflow run ci-main.yml        # For comprehensive testing
```

---

## For Release Managers

### Release Creation Workflow

**Before:**
```bash
# No dedicated release workflow
# Manual artifact creation
```

**After:**
```bash
# 1. Ensure comprehensive tests pass
gh workflow run ci-main.yml

# 2. Create and push tag (triggers release.yml)
git tag v0.17.0
git push origin v0.17.0

# 3. Release.yml automatically:
#    - Builds for all 5 platforms
#    - Creates release artifacts
#    - Publishes GitHub release
```

### Scheduled Comprehensive Testing

**Before:**
```bash
# Manual triggering of vcpkg-triplet-matrix.yml
gh workflow run vcpkg-triplet-matrix.yml
```

**After:**
```bash
# Use scheduled.yml (includes all triplets + benchmarks)
gh workflow run scheduled.yml -f run-benchmarks=true
```

**Or rely on the automatic weekly schedule** (Sundays at 2 AM UTC).

---

## For Workflow Maintainers

### Updating Workflow References

**If you have external systems that trigger workflows:**

**Old references:**
```yaml
- .github/workflows/build.yml
- .github/workflows/build-test.yml
- .github/workflows/vcpkg-triplet-matrix.yml
```

**New references:**
```yaml
- .github/workflows/ci-main.yml
- .github/workflows/pr-validation.yml
- .github/workflows/release.yml
- .github/workflows/scheduled.yml
- .github/workflows/manual.yml
```

### Updating Reusable Workflow Calls

**If you call `build-test.yml` from other workflows:**

**Before:**
```yaml
uses: ./.github/workflows/build-test.yml
  with:
    os: ubuntu
    arch: x64
```

**After:**
```yaml
uses: ./.github/workflows/_build-test-reusable.yml
  with:
    runner: ubuntu-latest
    triplet: x64-linux
    config: flatbuffers-only
```

**Note**: The new reusable workflow has different parameters. See `.github/workflows/_build-test-reusable.yml` for reference.

---

## Workflow Mapping

### Detailed Mapping

| Old Use Case | Old Workflow | New Workflow | Notes |
|--------------|-------------|--------------|-------|
| **PR validation** | `build.yml` (if enabled) | `pr-validation.yml` | Faster, focused |
| **Main branch CI** | `build.yml` | `ci-main.yml` | Comprehensive |
| **Release artifacts** | Manual | `release.yml` | Automated |
| **All triplets testing** | `vcpkg-triplet-matrix.yml` | `scheduled.yml` | Plus benchmarks |
| **Manual testing** | Various | `manual.yml` | Unified interface |
| **Reusable build/test** | `build-test.yml` | `_build-test-reusable.yml` | vcpkg-based |
| **Compatibility tests** | `compat-test.yml` | `_compat-test-reusable.yml` | Reusable |
| **Benchmarks** | `benchmark-comprehensive.yml` | `_benchmark-reusable.yml` | Reusable |

---

## Migration Steps

### Step 1: Update Local Documentation

**If you have local scripts that reference old workflows:**

```bash
# Find references to old workflows
grep -r "build.yml" .github/
grep -r "build-test.yml" .github/
grep -r "vcpkg-triplet-matrix" .github/
```

**Update references to new workflows.**

### Step 2: Update External Integrations

**If you have external systems (webhooks, bots) that trigger workflows:**

1. Go to Repository Settings → Webhooks
2. Update webhook paths to use new workflows
3. Test with a manual trigger

**Old webhook paths:**
- `/workflows/build.yml`
- `/workflows/vcpkg-triplet-matrix.yml`

**New webhook paths:**
- `/workflows/ci-main.yml`
- `/workflows/pr-validation.yml`
- `/workflows/scheduled.yml`

### Step 3: Update CI Scripts

**If you have scripts that use `gh workflow run`:**

```bash
# Old
gh workflow run build.yml

# New
gh workflow run ci-main.yml
```

### Step 4: Update Documentation

**Update any README, CONTRIBUTING, or developer guides:**

- Remove references to `build.yml`
- Add references to `pr-validation.yml` and `ci-main.yml`
- Link to `.github/README.md` for CI/CD info

### Step 5: Verify New Workflows

**Before removing old workflows:**

1. Test new workflows manually:
   ```bash
   gh workflow run pr-validation.yml
   gh workflow run ci-main.yml
   ```

2. Verify they work correctly in the Actions tab

3. Check artifact generation

4. Confirm release workflow works (test on a non-main branch first)

---

## Breaking Changes

### Parameter Changes

If you were calling `build-test.yml` directly, the parameters have changed:

**Old (`build-test.yml`):**
```yaml
inputs:
  os: ubuntu
  arch: x64
  with_thrift: true
  with_flatbuffers: true
```

**New (`_build-test-reusable.yml`):**
```yaml
inputs:
  runner: ubuntu-latest
  triplet: x64-linux
  config: both-formats  # or flatbuffers-only
```

### Migration Example

**Old code:**
```yaml
uses: ./.github/workflows/build-test.yml
  with:
    os: ubuntu
    arch: x64
    with_thrift: true
    with_flatbuffers: true
```

**New code:**
```yaml
uses: ./.github/workflows/_build-test-reusable.yml
  with:
    runner: ubuntu-latest
    triplet: x64-linux
    config: both-formats
```

---

## Rollback Plan

If you encounter issues with the new workflows:

### Temporary Rollback

1. **Disable new workflows temporarily:**
   - Rename `pr-validation.yml` to `pr-validation.yml.disabled`
   - Rename `ci-main.yml` to `ci-main.yml.disabled`

2. **Re-enable old workflows:**
   - Remove deprecation notice from `build.yml`
   - Re-enable triggers in `build.yml`

### Getting Help

1. **Check the status:**
   ```bash
   gh workflow list
   ```

2. **View recent runs:**
   ```bash
   gh run list --workflow=pr-validation.yml
   gh run list --workflow=ci-main.yml
   ```

3. **Open an issue:**
   - Include workflow run ID
   - Describe the expected vs actual behavior
   - Link to failed run logs

---

## Validation Checklist

Before considering migration complete:

- [ ] `pr-validation.yml` runs successfully on PRs
- [ ] `ci-main.yml` runs successfully on main branch pushes
- [ ] `release.yml` creates artifacts correctly (test with non-main tag)
- [ ] `scheduled.yml` runs successfully (manual trigger)
- [ ] `manual.yml` works for all use cases
- [ ] All external integrations updated
- [ ] Documentation updated
- [ ] Team notified of changes

---

## Additional Resources

### Documentation

- **[CI_CD_ARCHITECTURE_PROPOSAL.md](CI_CD_ARCHITECTURE_PROPOSAL.md)** - Complete architecture proposal
- **[CI_CD_ARCHITECTURE_VISUAL.md](CI_CD_ARCHITECTURE_VISUAL.md)** - Visual diagrams
- **[.github/README.md](.github/README.md)** - CI/CD quick reference
- **[TESTING.md](../TESTING.md)** - Testing guide (updated)
- **[WORKFLOW_GUIDE.md](../WORKFLOW_GUIDE.md)** - Developer workflows

### Quick Commands

```bash
# List all workflows
gh workflow list

# Trigger workflow
gh workflow run pr-validation.yml
gh workflow run ci-main.yml
gh workflow run scheduled.yml -f run-benchmarks=true

# View runs
gh run list --workflow=pr-validation.yml
gh run view <run-id>

# View workflow definition
gh workflow view pr-validation.yml
gh workflow view ci-main.yml
```

---

## Timeline

| Phase | Date | Status |
|-------|------|--------|
| **Phase 1**: Create reusable workflows | 2025-01-19 | ✅ Complete |
| **Phase 2**: Create user-facing workflows | 2025-01-19 | ✅ Complete |
| **Phase 3**: Deprecate old workflows | 2025-01-19 | ✅ Complete |
| **Phase 4**: Update documentation | 2025-01-19 | ✅ Complete |
| **Phase 5**: Validation | 2025-01-19 | In Progress |

**Target completion**: 2025-01-19

---

## Support

For questions or issues during migration:

1. Check this guide first
2. Review the CI/CD architecture documentation
3. Open an issue with label `ci/cd`
4. Contact the maintainers

**Remember**: The old workflows still work (mostly), so migration can be gradual. Test the new workflows before fully committing to them.
