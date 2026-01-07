/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * Stub implementations for manpage functions
 * Used when building tools separately without generated manpages
 */

#include <dwarfs/tool/manpage.h>

namespace dwarfs::tool::manpage {

document get_mkdwarfs_manpage() {
  return document{};
}

document get_dwarfs_manpage() {
  return document{};
}

document get_dwarfsck_manpage() {
  return document{};
}

document get_dwarfsextract_manpage() {
  return document{};
}

} // namespace dwarfs::tool::manpage