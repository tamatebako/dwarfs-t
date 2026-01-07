vcpkg_check_features(
  OUT_FEATURE_OPTIONS
    FEATURE_OPTIONS
  FEATURES
    "lzma"     BOOST_IOSTREAMS_ENABLE_LZMA
    "zlib"     BOOST_IOSTREAMS_ENABLE_ZLIB
    "zstd"     BOOST_IOSTREAMS_ENABLE_ZSTD
)
# bzip2 is always enabled (mandatory dependency, not a feature)
set(BOOST_IOSTREAMS_ENABLE_BZIP2 ON)