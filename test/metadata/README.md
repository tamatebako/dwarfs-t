# Metadata Serialization Test Suite

This directory contains comprehensive tests for the DwarFS metadata serialization framework.

## Overview

The test suite validates the new Cereal-based serialization implementation, ensuring:
- **Correctness**: All domain models serialize/deserialize accurately
- **Backward Compatibility**: Legacy Thrift format detection works
- **Performance**: Serialization meets acceptable speed targets
- **Integration**: High-level APIs function correctly

## Test Files

### [`domain_model_test.cpp`](domain_model_test.cpp)
Tests for individual domain model classes (chunk, directory, inode_data, metadata).

**Key Test Areas:**
- Round-trip serialization for all domain classes
- Edge cases (empty, max values, zero values)
- Optional field handling
- Version evolution
- Large data sets (10,000+ elements)

### [`cereal_serializer_test.cpp`](cereal_serializer_test.cpp)
Tests for [`CerealBinarySerializer`](../../include/dwarfs/metadata/serialization/cereal_binary_serializer.h) implementation.

**Key Test Areas:**
- Magic byte generation (0xCE 0xEA 0x01)
- Data integrity preservation
- Error handling (invalid sizes, wrong magic bytes, corrupted data)
- Serialization determinism
- Special character handling

### [`thrift_compat_test.cpp`](thrift_compat_test.cpp)
Tests for backward compatibility with legacy Thrift format.

**Key Test Areas:**
- Thrift format detection (0x82 0x21)
- Read-only adapter enforcement
- Format differentiation (Cereal vs Thrift)
- Error message clarity
- Migration path validation

### [`integration_test.cpp`](integration_test.cpp)
High-level integration tests for the serialization framework.

**Key Test Areas:**
- [`MetadataReader`](../../include/dwarfs/metadata/reader.h) with auto-detection
- [`MetadataWriter`](../../include/dwarfs/metadata/writer.h) functionality
- [`FormatDetector`](../../include/dwarfs/metadata/serialization/format_detector.h) accuracy
- [`SerializerFactory`](../../include/dwarfs/metadata/serialization/serializer_factory.h) creation
- Full round-trip workflows

### [`performance_test.cpp`](performance_test.cpp)
Performance benchmarks for serialization operations.

**Key Test Areas:**
- Serialization speed (small, medium, large datasets)
- Deserialization speed
- Memory efficiency
- Output size analysis

### [`test_fixtures.h`](test_fixtures.h)
Shared test data generators for consistent testing across all test files.

**Available Fixtures:**
- `create_minimal_metadata()` - Minimal valid metadata
- `create_small_metadata()` - 10 chunks, 5 directories
- `create_medium_metadata()` - 1000 chunks, 100 directories
- `create_large_metadata()` - 10000 chunks, 1000 directories

## Building and Running Tests

### Build Configuration

```bash
# Configure with tests enabled
cmake -B build-test -DWITH_TESTS=ON -DTEBAKO_BUILD=ON

# Build test suite
cmake --build build-test
```

### Running Tests

```bash
# Run all DwarFS tests
ctest --test-dir build-test --output-on-failure

# Run only metadata tests
./build-test/metadata_tests

# Run with verbose output
./build-test/metadata_tests --gtest_verbose

# Run specific test suite
./build-test/metadata_tests --gtest_filter="CerealSerializerTest.*"

# Run specific test
./build-test/metadata_tests --gtest_filter="CerealSerializerTest.BasicRoundTrip"
```

### Performance Benchmarks

Performance tests output timing information:

```bash
./build-test/metadata_tests --gtest_filter="PerformanceTest.*"
```

## Test Coverage

Total test count: **80+ test cases**

- Domain Model Tests: 14 tests
- Cereal Serializer Tests: 14 tests
- Thrift Compatibility Tests: 15 tests
- Integration Tests: 23 tests
- Performance Tests: 9 tests

**Expected Coverage:** >80% of serialization framework code

## Success Criteria

✅ All tests pass
✅ No memory leaks (run with ASAN or valgrind)
✅ Performance within acceptable ranges
✅ Code coverage >80%
✅ Backward compatibility validated

## Known Limitations

1. **No real legacy .dwarfs files** - Thrift tests use mock data
   - Future: Test with actual v2.0-v2.5 files

2. **ThriftConverter not fully tested** - Needs real Thrift objects
   - Future: Add converter tests with legacy data

3. **No Thrift vs Cereal comparison** - Performance comparison incomplete
   - Future: Add comparative benchmarks

## Troubleshooting

### Build Errors

If you encounter build errors:

1. Ensure GTest is available: `cmake` should find or fetch GoogleTest
2. Verify Cereal headers are found
3. Check that Thrift libraries are linked correctly

### Test Failures

If tests fail:

1. Check build configuration matches expected environment
2. Verify all dependencies are properly linked
3. Run with `--gtest_verbose` for detailed output
4. Check [`PHASE6_TEST_REPORT.md`](../../doc/PHASE6_TEST_REPORT.md) for known issues

### Performance Issues

If performance tests fail:

1. Performance targets are conservative
2. Run on a system with adequate resources
3. Check for background processes affecting timing
4. Timing thresholds can be adjusted if needed

## Contributing

When adding new serialization features:

1. Add corresponding domain model tests
2. Add serializer-specific tests
3. Update integration tests if APIs change
4. Add performance tests for new operations
5. Update test fixtures as needed
6. Maintain >80% code coverage

## References

- [Phase 6 Test Report](../../doc/PHASE6_TEST_REPORT.md)
- [Serialization README](../../include/dwarfs/metadata/serialization/README.md)
- [Domain Models README](../../include/dwarfs/metadata/domain/README.md)