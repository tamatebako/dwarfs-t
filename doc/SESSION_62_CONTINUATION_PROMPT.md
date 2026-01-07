# Session 62 Continuation Prompt: Legacy Thrift Implementation

**Date**: 2026-01-01
**Status**: 📋 **READY TO START**
**Mode**: Switch to **Code Mode**
**Duration**: ~4 hours (Phase 1)

---

## Quick Context

**What**: Implement Legacy Thrift metadata format by porting dwarfs-rs's hand-coded Thrift Compact protocol

**Why**: Unblock Homebrew v0.14.1 compatibility without fbthrift dependency hell (Session 60: 15 failed attempts)

**How**: Port ~600 lines of Rust code from dwarfs-rs to C++

---

## Session 62 Goals

Implement **Phase 1: Thrift Compact Primitives** (4 hours):

1. ✅ Create directory structure
2. ✅ Implement Tag enum
3. ✅ Implement `ThriftCompactWriter` (varint, zigzag, primitives)
4. ✅ Implement `ThriftCompactReader` (mirror of writer)
5. ✅ Write comprehensive unit tests
6. ✅ Validate byte-for-byte against dwarfs-rs

---

## Pre-Flight Checklist

Before starting implementation:

- [x] Read [`doc/SESSION_62_LEGACY_THRIFT_ARCHITECTURE.md`](SESSION_62_LEGACY_THRIFT_ARCHITECTURE.md) - Full architecture
- [x] Read [`doc/SESSION_62_CONTINUATION_PLAN.md`](SESSION_62_CONTINUATION_PLAN.md) - Implementation plan
- [x] Read [`doc/SESSION_62_IMPLEMENTATION_STATUS.md`](SESSION_62_IMPLEMENTATION_STATUS.md) - Status tracker
- [ ] Read dwarfs-rs source:
  - `/Users/mulgogi/src/external/dwarfs-rs/dwarfs/src/metadata/de_thrift.rs` (273 lines)
  - `/Users/mulgogi/src/external/dwarfs-rs/dwarfs/src/metadata/ser_thrift.rs` (322 lines)
  - `/Users/mulgogi/src/external/dwarfs-rs/dwarfs/src/metadata.rs` (602 lines)
- [ ] Have test vectors ready from dwarfs-rs

---

## Implementation Order

### Step 1: Foundation (30 min)

Create directory structure and basic scaffolding:

```bash
# Create directories
mkdir -p include/dwarfs/metadata/legacy
mkdir -p src/metadata/legacy
mkdir -p test/metadata/legacy

# Create files
touch include/dwarfs/metadata/legacy/{thrift_types.h,thrift_compact_writer.h,thrift_compact_reader.h}
touch src/metadata/legacy/{thrift_types.cpp,thrift_compact_writer.cpp,thrift_compact_reader.cpp}
touch test/metadata/legacy/thrift_compact_test.cpp
```

**File 1**: `include/dwarfs/metadata/legacy/thrift_types.h`
```cpp
#pragma once
#include <cstdint>

namespace dwarfs::metadata::legacy {

// Thrift Compact protocol type tags
enum class Tag : uint8_t {
  BOOL_TRUE = 1,
  BOOL_FALSE = 2,
  I16 = 4,
  I32 = 5,
  BINARY = 8,
  MAP = 11,
  STRUCT = 12,

  // Internal use only
  UNKNOWN_BOOL = 0,  // For non-inline bool
  INVALID = 15
};

// For debugging/logging
const char* tag_name(Tag t);

} // namespace dwarfs::metadata::legacy
```

**File 2**: `src/metadata/legacy/thrift_types.cpp`
```cpp
#include "dwarfs/metadata/legacy/thrift_types.h"

namespace dwarfs::metadata::legacy {

const char* tag_name(Tag t) {
  switch (t) {
    case Tag::BOOL_TRUE: return "BOOL_TRUE";
    case Tag::BOOL_FALSE: return "BOOL_FALSE";
    case Tag::I16: return "I16";
    case Tag::I32: return "I32";
    case Tag::BINARY: return "BINARY";
    case Tag::MAP: return "MAP";
    case Tag::STRUCT: return "STRUCT";
    case Tag::UNKNOWN_BOOL: return "UNKNOWN_BOOL";
    case Tag::INVALID: return "INVALID";
  }
  return "UNKNOWN";
}

} // namespace dwarfs::metadata::legacy
```

### Step 2: Writer Class (1 hour)

**File 3**: `include/dwarfs/metadata/legacy/thrift_compact_writer.h`

Reference: [`/Users/mulgogi/src/external/dwarfs-rs/dwarfs/src/metadata/ser_thrift.rs:22-43`](../../../dwarfs-rs/dwarfs/src/metadata/ser_thrift.rs:22-43)

```cpp
#pragma once
#include "dwarfs/metadata/legacy/thrift_types.h"
#include <vector>
#include <cstdint>
#include <string_view>

namespace dwarfs::metadata::legacy {

class ThriftCompactWriter {
public:
  explicit ThriftCompactWriter(std::vector<uint8_t>& buffer);

  // Primitives (return Tag for serializer use)
  Tag write_bool(bool v, bool inline_bool = false);
  Tag write_i16(int16_t v);
  Tag write_i32(int32_t v);
  Tag write_string(std::string_view s);

  // Low-level encodings
  void write_varint(uint32_t v);
  void write_zigzag(int32_t v);

  // Struct support
  void begin_struct();
  void write_field_header(uint16_t field_id, Tag type);
  void end_struct();

  // Map support
  void begin_map(uint32_t size);
  void write_map_type_byte(Tag ktype, Tag vtype);
  void end_map();  // No-op

  size_t position() const { return buf_.size(); }

private:
  std::vector<uint8_t>& buf_;
  uint8_t field_id_diff_tag_{0x10};
};

} // namespace dwarfs::metadata::legacy
```

**File 4**: `src/metadata/legacy/thrift_compact_writer.cpp`

Key implementations to port:

1. **Varint** (from [`ser_thrift.rs:28-38`](../../../dwarfs-rs/dwarfs/src/metadata/ser_thrift.rs:28-38)):
```cpp
void ThriftCompactWriter::write_varint(uint32_t v) {
  do {
    uint32_t more = v >> 7;
    bool has_more = more > 0;
    buf_.push_back(static_cast<uint8_t>(v & 0x7F) | (has_more << 7));
    v = more;
  } while (v > 0);
}
```

2. **Zigzag** (from [`ser_thrift.rs:40-42`](../../../dwarfs-rs/dwarfs/src/metadata/ser_thrift.rs:40-42)):
```cpp
void ThriftCompactWriter::write_zigzag(int32_t v) {
  write_varint(static_cast<uint32_t>((v << 1) ^ (v >> 31)));
}
```

3. **Field header** (from [`ser_thrift.rs:254-264`](../../../dwarfs-rs/dwarfs/src/metadata/ser_thrift.rs:254-264)):
```cpp
void ThriftCompactWriter::write_field_header(uint16_t field_id, Tag type) {
  size_t pos = buf_.size();
  buf_.push_back(0);  // Placeholder

  // field_id_diff_tag starts at 0x10, increments by 0x10
  buf_[pos] = field_id_diff_tag_ | static_cast<uint8_t>(type);
  field_id_diff_tag_ = 0x10;
}
```

### Step 3: Reader Class (1 hour)

**File 5**: `include/dwarfs/metadata/legacy/thrift_compact_reader.h`

Reference: [`/Users/mulgogi/src/external/dwarfs-rs/dwarfs/src/metadata/de_thrift.rs:88-119`](../../../dwarfs-rs/dwarfs/src/metadata/de_thrift.rs:88-119)

```cpp
#pragma once
#include "dwarfs/metadata/legacy/thrift_types.h"
#include <span>
#include <optional>
#include <string_view>

namespace dwarfs::metadata::legacy {

class ThriftCompactReader {
public:
  explicit ThriftCompactReader(std::span<uint8_t const> data);

  // Primitives
  bool read_bool(Tag hint = Tag::UNKNOWN_BOOL);
  int16_t read_i16();
  int32_t read_i32();
  std::string_view read_string();

  // Low-level decodings
  uint32_t read_varint();
  int32_t read_zigzag();
  uint8_t read_byte();

  // Struct support
  struct FieldHeader {
    uint16_t field_id;
    Tag type;
  };
  void begin_struct();
  std::optional<FieldHeader> read_field_header();
  void end_struct();

  // Map support
  struct MapHeader {
    uint32_t size;
    Tag ktype;
    Tag vtype;
  };
  MapHeader begin_map();
  void end_map();

  bool at_end() const { return pos_ >= data_.size(); }
  size_t position() const { return pos_; }

private:
  std::span<uint8_t const> data_;
  size_t pos_{0};
  int16_t current_field_id_{0};
};

} // namespace dwarfs::metadata::legacy
```

**File 6**: `src/metadata/legacy/thrift_compact_reader.cpp`

Key implementations:

1. **Varint** (from [`de_thrift.rs:103-113`](../../../dwarfs-rs/dwarfs/src/metadata/de_thrift.rs:103-113)):
```cpp
uint32_t ThriftCompactReader::read_varint() {
  uint32_t x = 0;
  for (int i = 0; i < 5; ++i) {
    uint8_t b = read_byte();
    x += static_cast<uint32_t>(b & 0x7F) << (i * 7);
    if ((b & 0x80) == 0) {
      return x;
    }
  }
  throw std::runtime_error("varint too long");
}
```

2. **Zigzag** (from [`de_thrift.rs:115-118`](../../../dwarfs-rs/dwarfs/src/metadata/de_thrift.rs:115-118)):
```cpp
int32_t ThriftCompactReader::read_zigzag() {
  uint32_t x = read_varint();
  return static_cast<int32_t>(x >> 1) ^ -(static_cast<int32_t>(x) & 1);
}
```

### Step 4: Unit Tests (1.5 hours)

**File 7**: `test/metadata/legacy/thrift_compact_test.cpp`

```cpp
#include "dwarfs/metadata/legacy/thrift_compact_writer.h"
#include "dwarfs/metadata/legacy/thrift_compact_reader.h"
#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace dwarfs::metadata::legacy;
using ::testing::ElementsAre;

// Varint tests
TEST(ThriftCompactWriter, Varint_Zero) {
  std::vector<uint8_t> buf;
  ThriftCompactWriter w(buf);
  w.write_varint(0);
  EXPECT_THAT(buf, ElementsAre(0x00));
}

TEST(ThriftCompactWriter, Varint_127) {
  std::vector<uint8_t> buf;
  ThriftCompactWriter w(buf);
  w.write_varint(127);
  EXPECT_THAT(buf, ElementsAre(0x7F));
}

TEST(ThriftCompactWriter, Varint_128) {
  std::vector<uint8_t> buf;
  ThriftCompactWriter w(buf);
  w.write_varint(128);
  EXPECT_THAT(buf, ElementsAre(0x80, 0x01));
}

TEST(ThriftCompactWriter, Varint_16383) {
  std::vector<uint8_t> buf;
  ThriftCompactWriter w(buf);
  w.write_varint(16383);
  EXPECT_THAT(buf, ElementsAre(0xFF, 0x7F));
}

// Zigzag tests
TEST(ThriftCompactWriter, Zigzag_Zero) {
  std::vector<uint8_t> buf;
  ThriftCompactWriter w(buf);
  w.write_zigzag(0);
  EXPECT_THAT(buf, ElementsAre(0x00));
}

TEST(ThriftCompactWriter, Zigzag_NegativeOne) {
  std::vector<uint8_t> buf;
  ThriftCompactWriter w(buf);
  w.write_zigzag(-1);
  EXPECT_THAT(buf, ElementsAre(0x01));
}

TEST(ThriftCompactWriter, Zigzag_MinusTwo) {
  std::vector<uint8_t> buf;
  ThriftCompactWriter w(buf);
  w.write_zigzag(-2);
  EXPECT_THAT(buf, ElementsAre(0x03));
}

// Round-trip tests
TEST(ThriftCompact, RoundTrip_Varint) {
  std::vector<uint8_t> buf;
  ThriftCompactWriter w(buf);

  w.write_varint(0);
  w.write_varint(127);
  w.write_varint(128);
  w.write_varint(16383);
  w.write_varint(UINT32_MAX);

  ThriftCompactReader r(buf);
  EXPECT_EQ(r.read_varint(), 0);
  EXPECT_EQ(r.read_varint(), 127);
  EXPECT_EQ(r.read_varint(), 128);
  EXPECT_EQ(r.read_varint(), 16383);
  EXPECT_EQ(r.read_varint(), UINT32_MAX);
  EXPECT_TRUE(r.at_end());
}

TEST(ThriftCompact, RoundTrip_Zigzag) {
  std::vector<uint8_t> buf;
  ThriftCompactWriter w(buf);

  w.write_zigzag(0);
  w.write_zigzag(-1);
  w.write_zigzag(1);
  w.write_zigzag(-2);
  w.write_zigzag(2);
  w.write_zigzag(INT32_MIN);
  w.write_zigzag(INT32_MAX);

  ThriftCompactReader r(buf);
  EXPECT_EQ(r.read_zigzag(), 0);
  EXPECT_EQ(r.read_zigzag(), -1);
  EXPECT_EQ(r.read_zigzag(), 1);
  EXPECT_EQ(r.read_zigzag(), -2);
  EXPECT_EQ(r.read_zigzag(), 2);
  EXPECT_EQ(r.read_zigzag(), INT32_MIN);
  EXPECT_EQ(r.read_zigzag(), INT32_MAX);
  EXPECT_TRUE(r.at_end());
}

TEST(ThriftCompact, RoundTrip_AllTypes) {
  std::vector<uint8_t> buf;
  ThriftCompactWriter w(buf);

  w.write_i32(42);
  w.write_i16(-123);
  w.write_string("hello");
  w.write_bool(true);
  w.write_bool(false);

  ThriftCompactReader r(buf);
  EXPECT_EQ(r.read_i32(), 42);
  EXPECT_EQ(r.read_i16(), -123);
  EXPECT_EQ(r.read_string(), "hello");
  EXPECT_EQ(r.read_bool(), true);
  EXPECT_EQ(r.read_bool(), false);
  EXPECT_TRUE(r.at_end());
}
```

### Step 5: CMake Integration (15 min)

Update `cmake/metadata_serialization.cmake`:

```cmake
# Legacy Thrift (always available, no external deps)
option(DWARFS_WITH_LEGACY_THRIFT "Enable Legacy Thrift format support" ON)

if(DWARFS_WITH_LEGACY_THRIFT)
  add_library(dwarfs_metadata_legacy
    src/metadata/legacy/thrift_types.cpp
    src/metadata/legacy/thrift_compact_writer.cpp
    src/metadata/legacy/thrift_compact_reader.cpp
  )

  target_include_directories(dwarfs_metadata_legacy
    PUBLIC include
  )

  target_compile_features(dwarfs_metadata_legacy
    PUBLIC cxx_std_20
  )

  target_compile_definitions(dwarfs_metadata_legacy
    PUBLIC DWARFS_HAVE_LEGACY_THRIFT=1
  )

  # Tests
  if(WITH_TESTS)
    add_executable(legacy_thrift_tests
      test/metadata/legacy/thrift_compact_test.cpp
    )

    target_link_libraries(legacy_thrift_tests
      dwarfs_metadata_legacy
      GTest::gtest_main
      GTest::gmock
    )

    gtest_discover_tests(legacy_thrift_tests)
  endif()
endif()
```

---

## Build & Test

```bash
# Configure
cmake -B build -GNinja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DDWARFS_WITH_LEGACY_THRIFT=ON \
  -DWITH_TESTS=ON

# Build
ninja -C build dwarfs_metadata_legacy legacy_thrift_tests

# Test
./build/legacy_thrift_tests
```

---

## Success Criteria

Phase 1 complete when:

- [ ] All files created
- [ ] All primitives implemented (varint, zigzag, bool, i16, i32, string)
- [ ] All tests pass (>30 tests)
- [ ] Byte-for-byte match with dwarfs-rs output
- [ ] No compiler warnings
- [ ] Code follows C++20 best practices

---

## Quick Reference

**dwarfs-rs Source**: `/Users/mulgogi/src/external/dwarfs-rs/dwarfs/src/metadata/`
- `de_thrift.rs` - Deserializer (lines 28-118 are key)
- `ser_thrift.rs` - Serializer (lines 28-103 are key)
- `metadata.rs` - Domain structures

**Key Encodings**:
- Varint: 7-bit chunks, continuation bit in bit 7
- Zigzag: `(v << 1) ^ (v >> 31)` for encoding, `(x >> 1) ^ -(x & 1)` for decoding
- Field delta: Upper 4 bits = field ID delta, lower 4 bits = type tag
- Map type byte: `(ktype << 4) | vtype`

---

## Next Steps After Phase 1

1. Update [`doc/SESSION_62_IMPLEMENTATION_STATUS.md`](SESSION_62_IMPLEMENTATION_STATUS.md)
2. Start **Session 63 Phase 2**: Wire primitives to `domain::metadata`
3. Implement metadata serialization/deserialization

---

**Start Command**: Switch to **Code mode** and begin with Step 1 (Foundation)