# Alpine Linux (musl) triplet for x64, dynamic library linkage
# Uses musl libc instead of glibc
#
# VCPKG_CRT_LINKAGE must stay "dynamic" on this triplet: with "static",
# vcpkg's Linux toolchain (scripts/toolchains/linux.cmake) appends -static
# to CMAKE_*_LINKER_FLAGS, which vcpkg_configure_make then exports via
# LDFLAGS, breaking every shared-library link of configure/make ports
# (botan 3.10: "crtbeginT.o: relocation R_X86_64_32 against hidden symbol
# `__TMC_END__' can not be used when making a shared object";
# tebako-rs release run 30217620407).
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)

set(VCPKG_CMAKE_SYSTEM_NAME Linux)

# Compiler flags for musl
set(VCPKG_C_FLAGS "-fPIC")
set(VCPKG_CXX_FLAGS "-fPIC")
