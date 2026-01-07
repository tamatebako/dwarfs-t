# Session 21: Manual Documentation Updates

**Created**: 2025-12-21
**Reason**: Fast Apply limitations on large files

---

## Updates Needed

### 1. README.md (Main Project)

**Location**: After line 389 (after "Advanced Options" section, before "Build Configuration")

**Insert**:

```markdown
---

## Practical Example

### Static Site Webserver

A complete example demonstrating libdwarfs C++ API integration: [`example/static-site-server/`](example/static-site-server/)

**What it demonstrates**:
- Serving static HTML content from DwarFS archive
- Integration with libmicrohttpd HTTP server
- Clean C++ architecture with proper error handling
- MIME type detection and 404 pages

**Quick start**:
```bash
cd example/static-site-server
./build.sh
./build/static-site-server --image aesop.dff
# Browse to http://localhost:8080/
```

**Features**:
- ✅ Instant startup (<100 ms)
- ✅ Fast random access (~1 ms warm cache)
- ✅ Small footprint (1.2 MB compressed from 3.8 MB)
- ✅ Comprehensive documentation

See [`example/static-site-server/README.md`](example/static-site-server/README.md) for complete documentation.
```

---

### 2. doc/LIBDWARFS_INTEGRATION_GUIDE.md

**Location**: Line 557, replace "Additional Resources" section

**Replace**:

```markdown
## Additional Resources

- **API Reference**: [`include/dwarfs/reader/`](../include/dwarfs/reader/)
- **Examples**: [`benchmarks/libdwarfs/`](../benchmarks/libdwarfs/)
- **Performance Report**: [`doc/LIBDWARFS_API_PERFORMANCE.md`](LIBDWARFS_API_PERFORMANCE.md)
- **Architecture**: [`.kilocode/rules/memory-bank/architecture.md`](../.kilocode/rules/memory-bank/architecture.md)
```

**With**:

```markdown
## Additional Resources

- **Practical Example**: [`example/static-site-server/`](../example/static-site-server/) - Complete HTTP server example
- **API Reference**: [`include/dwarfs/reader/`](../include/dwarfs/reader/)
- **Benchmarks**: [`benchmarks/libdwarfs/`](../benchmarks/libdwarfs/)
- **Performance Report**: [`doc/LIBDWARFS_API_PERFORMANCE.md`](LIBDWARFS_API_PERFORMANCE.md)
- **Architecture**: [`.kilocode/rules/memory-bank/architecture.md`](../.kilocode/rules/memory-bank/architecture.md)
```

---

## Verification

After making these changes manually:

1. Verify links work:
   ```bash
   grep -r "example/static-site-server" README.md
   grep -r "example/static-site-server" doc/LIBDWARFS_INTEGRATION_GUIDE.md
   ```

2. Check formatting:
   ```bash
   # Links should be clickable in GitHub
   # Code blocks should render correctly
   ```

---

**Status**: Ready for manual application