set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE static)
set(VCPKG_LIBRARY_LINKAGE static)
# No VCPKG_CMAKE_SYSTEM_NAME: desktop-MSVC triplets are plain (the
# builtin/community windows triplets never set it — it is the
# mingw/uwp-class cross switch). With it set, vcpkg's detect_compiler
# probe configures with -DCMAKE_SYSTEM_NAME=Windows, cmake's MSVC
# auto-detection is disabled, and EnableLanguage dies with
# "CMAKE_C_COMPILER not set" before the compiler hash exists (proven on
# the tebako windows-arm64 leg, run 35427240315; the same probe passes
# for the plain arm64-windows-static-md triplet).
set(VCPKG_CXX_FLAGS "/MT")
set(VCPKG_C_FLAGS "/MT")
set(VCPKG_CXX_FLAGS_RELEASE "/MT")
set(VCPKG_C_FLAGS_RELEASE "/MT")
set(VCPKG_CXX_FLAGS_DEBUG "/MTd")
set(VCPKG_C_FLAGS_DEBUG "/MTd")