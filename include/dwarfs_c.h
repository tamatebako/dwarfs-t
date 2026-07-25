/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Marcus Holland-Moritz (github@mhxnet.de)
 * \copyright  Copyright (c) Marcus Holland-Moritz
 *
 * This file is part of dwarfs.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the “Software”), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file dwarfs_c.h
 * @brief Stable C ABI for the DwarFS reader.
 *
 * This is the ONLY consumer-facing header of the C binding (libdwarfs_c).
 * C consumers (and Rust/Go via FFI) can read DwarFS images without touching
 * C++ headers, templates, or ABI-fragile internals. The C++ runtime stays
 * inside the binding.
 *
 * Scope: READER only. Image creation (mkdwarfs) is not exposed here.
 *
 * Error channel
 * -------------
 * No C++ exceptions cross this boundary. Functions report errors through a
 * thread-local, errno-style channel:
 *
 *  - Functions returning a pointer return NULL on error.
 *  - Functions returning int return -1 on error.
 *  - dwarfs_c_pread() returns -1 on error, otherwise the byte count.
 *  - dwarfs_c_readdir() returns 1 for a valid entry, 0 at end of directory,
 *    and -1 on error.
 *
 * After an error, dwarfs_c_errno() yields an errno.h-style code (ENOENT,
 * EISDIR, ENOTDIR, EINVAL, EIO, ...) and dwarfs_c_error_message() yields a
 * human-readable message. Both are thread-local and are overwritten by the
 * next call on the same thread. A successful call resets the error state
 * to zero.
 *
 * Ownership
 * -------------
 *  - dwarfs_c_filesystem and dwarfs_c_dir handles are owned by the caller
 *    and must be released with dwarfs_c_close() / dwarfs_c_closedir().
 *  - Strings returned by dwarfs_c_error_message(), dwarfs_c_strerror() and
 *    dwarfs_c_version_string() are borrowed; do not free them.
 *    dwarfs_c_error_message() is valid until the next API call on the same
 *    thread.
 *  - The name pointer in dwarfs_c_dirent is owned by the directory iterator
 *    and is valid until the next dwarfs_c_readdir() or dwarfs_c_closedir()
 *    on that iterator.
 *  - The string returned by dwarfs_c_image_info_json() is heap-allocated
 *    and must be released with dwarfs_c_free().
 *  - The buffer passed to dwarfs_c_open_memory() is borrowed, NOT copied;
 *    it must remain valid until dwarfs_c_close().
 *
 * Threading
 * -------------
 * Distinct handles may be used concurrently from multiple threads. A single
 * dwarfs_c_dir iterator must not be shared between threads without external
 * synchronization. Concurrent dwarfs_c_stat()/dwarfs_c_pread() calls on the
 * same filesystem handle are safe.
 */

#ifndef DWARFS_C_H
#define DWARFS_C_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
/* Static library: no dllimport/dllexport needed. */
#define DWARFS_C_API
#else
#define DWARFS_C_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** Opaque filesystem handle. */
typedef struct dwarfs_c_filesystem dwarfs_c_filesystem;

/** Opaque directory iterator handle. */
typedef struct dwarfs_c_dir dwarfs_c_dir;

/** File type classification (mirrors the POSIX mode type bits). */
typedef enum dwarfs_c_file_type {
  DWARFS_C_FILE_UNKNOWN = 0,  /**< type could not be determined */
  DWARFS_C_FILE_REGULAR = 1,  /**< regular file */
  DWARFS_C_FILE_DIRECTORY = 2,  /**< directory */
  DWARFS_C_FILE_SYMLINK = 3,  /**< symbolic link */
  DWARFS_C_FILE_OTHER = 4  /**< device, fifo, socket, ... */
} dwarfs_c_file_type;

/** Stat-equivalent information for a filesystem entry. */
struct dwarfs_c_stat {
  int64_t size;       /**< file size in bytes (regular files) */
  int64_t mtime;      /**< modification time, seconds since the epoch */
  int32_t mtime_nsec; /**< modification time, sub-second nanoseconds */
  uint32_t mode;      /**< POSIX st_mode value (type and permission bits) */
  uint32_t uid;       /**< owner user id */
  uint32_t gid;       /**< owner group id */
  uint32_t nlink;     /**< number of hard links */
  int32_t type;       /**< dwarfs_c_file_type */
};

/** Directory entry produced by dwarfs_c_readdir(). */
typedef struct dwarfs_c_dirent {
  const char* name; /**< entry name; owned by the iterator, see header docs */
  int32_t type;     /**< dwarfs_c_file_type */
} dwarfs_c_dirent;

/** Pass as offset to dwarfs_c_open_region() to auto-detect the image start. */
#define DWARFS_C_OFFSET_AUTO ((int64_t) - 1)

/* --------------------------------------------------------------------- */
/* Error channel                                                          */
/* --------------------------------------------------------------------- */

/**
 * Return the thread-local error code set by the last failed API call on
 * this thread (0 if the last call succeeded). Values are errno.h codes.
 */
DWARFS_C_API int dwarfs_c_errno(void);

/**
 * Return the thread-local, human-readable message for the last failed API
 * call on this thread. Never NULL; empty string if there is no message.
 * The pointer is borrowed and valid until the next API call on this thread.
 */
DWARFS_C_API const char* dwarfs_c_error_message(void);

/**
 * Return a static, borrowed string describing an errno-style error code
 * (as returned by dwarfs_c_errno()).
 */
DWARFS_C_API const char* dwarfs_c_strerror(int err);

/* --------------------------------------------------------------------- */
/* Library information                                                    */
/* --------------------------------------------------------------------- */

/**
 * Return the library version as a single integer:
 * major * 10000 + minor * 100 + patch.
 */
DWARFS_C_API int dwarfs_c_version(void);

/** Return a borrowed, static version string (e.g. the git description). */
DWARFS_C_API const char* dwarfs_c_version_string(void);

/* --------------------------------------------------------------------- */
/* Filesystem lifecycle                                                   */
/* --------------------------------------------------------------------- */

/**
 * Open a DwarFS image from a file.
 *
 * @param path  path of the image file (UTF-8)
 * @return filesystem handle, or NULL on error (ENOENT if the file does not
 *         exist, EIO if the image cannot be parsed)
 */
DWARFS_C_API dwarfs_c_filesystem* dwarfs_c_open(const char* path);

/**
 * Open a DwarFS image from a region of a file. This is intended for images
 * embedded at an offset inside a larger file (e.g. self-extracting stubs).
 *
 * @param path    path of the file containing the image (UTF-8)
 * @param offset  byte offset of the image inside the file, or
 *                DWARFS_C_OFFSET_AUTO to auto-detect
 * @param length  length of the image in bytes; must be > 0
 * @return filesystem handle, or NULL on error (EINVAL for bad arguments,
 *         ENOENT if the file does not exist, EIO on parse failure)
 */
DWARFS_C_API dwarfs_c_filesystem*
dwarfs_c_open_region(const char* path, int64_t offset, int64_t length);

/**
 * Open a DwarFS image from a memory buffer.
 *
 * The buffer is borrowed, NOT copied; it must remain valid until
 * dwarfs_c_close().
 *
 * @param data  pointer to the image data
 * @param size  size of the image data in bytes; must be > 0
 * @return filesystem handle, or NULL on error (EINVAL for bad arguments,
 *         EIO on parse failure)
 */
DWARFS_C_API dwarfs_c_filesystem*
dwarfs_c_open_memory(const void* data, size_t size);

/**
 * Close a filesystem handle and release all associated resources.
 * Safe to call with NULL. All directory iterators obtained from this
 * handle must be closed before calling dwarfs_c_close().
 */
DWARFS_C_API void dwarfs_c_close(dwarfs_c_filesystem* fs);

/* --------------------------------------------------------------------- */
/* Lookup / stat                                                          */
/* --------------------------------------------------------------------- */

/**
 * Look up an entry by path and fill in its stat-equivalent information.
 *
 * Paths are relative to the filesystem root; a leading '/' is accepted and
 * ignored. "" or "/" denote the root directory. Lookup never resolves the
 * final path component if it is a symbolic link (lstat semantics).
 *
 * @param fs   filesystem handle
 * @param path entry path
 * @param st   output stat information
 * @return 0 on success, -1 on error (ENOENT if the path does not exist)
 */
DWARFS_C_API int dwarfs_c_stat(dwarfs_c_filesystem* fs, const char* path,
                               struct dwarfs_c_stat* st);

/* --------------------------------------------------------------------- */
/* Reading                                                                */
/* --------------------------------------------------------------------- */

/**
 * Read from a regular file at a given offset (pread primitive).
 *
 * Reads are clamped to the end of the file; reading at or past the end
 * yields 0.
 *
 * @param fs     filesystem handle
 * @param path   path of a regular file
 * @param buf    destination buffer
 * @param count  number of bytes to read
 * @param offset byte offset inside the file; must be >= 0
 * @return number of bytes read (>= 0), or -1 on error (ENOENT if the path
 *         does not exist, EISDIR for a directory, EINVAL for other
 *         non-regular entries or bad arguments, EIO on read failure)
 */
DWARFS_C_API int64_t dwarfs_c_pread(dwarfs_c_filesystem* fs, const char* path,
                                    void* buf, size_t count, int64_t offset);

/* --------------------------------------------------------------------- */
/* Directory listing                                                      */
/* --------------------------------------------------------------------- */

/**
 * Open a directory for iteration.
 *
 * @param fs   filesystem handle
 * @param path path of a directory
 * @return directory iterator handle, or NULL on error (ENOENT if the path
 *         does not exist, ENOTDIR if it is not a directory)
 */
DWARFS_C_API dwarfs_c_dir* dwarfs_c_opendir(dwarfs_c_filesystem* fs,
                                            const char* path);

/**
 * Fetch the next directory entry.
 *
 * The entries "." and ".." are never returned.
 *
 * @param dir  directory iterator handle
 * @param out  output entry; out->name is borrowed from the iterator and
 *             remains valid until the next dwarfs_c_readdir() or
 *             dwarfs_c_closedir() on this iterator
 * @return 1 if an entry was returned, 0 at end of directory, -1 on error
 */
DWARFS_C_API int dwarfs_c_readdir(dwarfs_c_dir* dir, dwarfs_c_dirent* out);

/** Close a directory iterator. Safe to call with NULL. */
DWARFS_C_API void dwarfs_c_closedir(dwarfs_c_dir* dir);

/* --------------------------------------------------------------------- */
/* Image metadata                                                         */
/* --------------------------------------------------------------------- */

/**
 * Return image-level metadata as a JSON string: image format version,
 * image offset, creation history (timestamps, mkdwarfs version, system,
 * command line), metadata summary (inode/directory/chunk counts, block
 * size, total size) and the section list.
 *
 * @param fs  filesystem handle
 * @return heap-allocated, NUL-terminated JSON string to be released with
 *         dwarfs_c_free(), or NULL on error
 */
DWARFS_C_API char* dwarfs_c_image_info_json(dwarfs_c_filesystem* fs);

/** Free a pointer returned by this library (e.g. dwarfs_c_image_info_json). */
DWARFS_C_API void dwarfs_c_free(void* ptr);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* DWARFS_C_H */
