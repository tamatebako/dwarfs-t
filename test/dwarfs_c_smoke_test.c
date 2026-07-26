/* vim:set ts=2 sw=2 sts=2 et: */
/**
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
 * @file dwarfs_c_smoke_test.c
 * @brief C (not C++) compile+link+run smoke test for the libdwarfs_c binding.
 *
 * Exercises the whole reader surface against the test/data.dwarfs fixture:
 * open from file / memory / file region, stat, pread, directory listing,
 * image metadata, and the errno-style error paths.
 */

#include <dwarfs_c.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef TEST_DATA_DIR
#define TEST_DATA_DIR "."
#endif

#if defined(_WIN32)
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#include <sys/types.h>
#define MKDIR(p) mkdir((p), 0755)
#endif

static int failures = 0;

#define CHECK(cond)                                                          \
  do {                                                                       \
    if (!(cond)) {                                                           \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);        \
      ++failures;                                                            \
    }                                                                        \
  } while (0)

static int write_text(const char* path, const char* text) {
  FILE* f = fopen(path, "wb");
  int rc = -1;
  if (f) {
    rc = fputs(text, f) < 0 ? -1 : 0;
    if (fclose(f) != 0) {
      rc = -1;
    }
  }
  return rc;
}

/* Open `inner` in the mounted image `fs` and verify its full contents. */
static int readback_matches(dwarfs_c_filesystem* fs, const char* inner,
                            const char* expect) {
  struct dwarfs_c_stat st;
  char buf[4096];
  size_t const want = strlen(expect);
  int64_t n;

  if (dwarfs_c_stat(fs, inner, &st) != 0) {
    return 0;
  }
  if (st.type != DWARFS_C_FILE_REGULAR || (size_t)st.size != want) {
    return 0;
  }
  n = dwarfs_c_pread(fs, inner, buf, want, 0);
  return n == (int64_t)want && memcmp(buf, expect, want) == 0;
}

static long file_size_of(const char* path) {
  FILE* f = fopen(path, "rb");
  long size = -1;
  if (f) {
    if (fseek(f, 0, SEEK_END) == 0) {
      size = ftell(f);
    }
    fclose(f);
  }
  return size;
}

int main(void) {
  char image_path[4096];
  struct dwarfs_c_stat st;
  int64_t format_sh_size = 0;

  snprintf(image_path, sizeof(image_path), "%s/data.dwarfs", TEST_DATA_DIR);

  printf("libdwarfs_c version: %d (%s)\n", dwarfs_c_version(),
         dwarfs_c_version_string());
  /* NB: version can legitimately be 0 in tag-less (dev) clones */
  CHECK(dwarfs_c_version() >= 0);
  CHECK(dwarfs_c_version_string() != NULL);

  /* ---------------------------------------------------------------- */
  /* Error paths: opening a nonexistent image                          */
  /* ---------------------------------------------------------------- */
  {
    dwarfs_c_filesystem* bad = dwarfs_c_open("/nonexistent/image.dwarfs");
    CHECK(bad == NULL);
    CHECK(dwarfs_c_errno() == ENOENT);
    CHECK(strlen(dwarfs_c_error_message()) > 0);
    CHECK(strlen(dwarfs_c_strerror(dwarfs_c_errno())) > 0);
  }

  /* ---------------------------------------------------------------- */
  /* Open from file                                                    */
  /* ---------------------------------------------------------------- */
  dwarfs_c_filesystem* fs = dwarfs_c_open(image_path);
  if (!fs) {
    fprintf(stderr, "FAIL: cannot open %s: %s\n", image_path,
            dwarfs_c_error_message());
    return 1;
  }
  CHECK(dwarfs_c_errno() == 0); /* error state reset after success */

  /* ---------------------------------------------------------------- */
  /* stat                                                              */
  /* ---------------------------------------------------------------- */
  CHECK(dwarfs_c_stat(fs, "/", &st) == 0);
  CHECK(st.type == DWARFS_C_FILE_DIRECTORY);

  CHECK(dwarfs_c_stat(fs, "format.sh", &st) == 0);
  CHECK(st.type == DWARFS_C_FILE_REGULAR);
  CHECK(st.size > 0);
  format_sh_size = st.size;

  /* leading slash must work as well */
  CHECK(dwarfs_c_stat(fs, "/format.sh", &st) == 0);
  CHECK(st.size == format_sh_size);

  CHECK(dwarfs_c_stat(fs, "no-such-file", &st) == -1);
  CHECK(dwarfs_c_errno() == ENOENT);

  /* ---------------------------------------------------------------- */
  /* pread                                                             */
  /* ---------------------------------------------------------------- */
  {
    char* whole = (char*)malloc((size_t)format_sh_size);
    CHECK(whole != NULL);
    if (whole) {
      int64_t total =
          dwarfs_c_pread(fs, "format.sh", whole, (size_t)format_sh_size, 0);
      CHECK(total == format_sh_size);

      /* partial read at an offset must match the full image */
      if (format_sh_size >= 16) {
        char tail[16];
        int64_t n = dwarfs_c_pread(fs, "format.sh", tail, sizeof(tail),
                                   format_sh_size - (int64_t)sizeof(tail));
        CHECK(n == (int64_t)sizeof(tail));
        CHECK(memcmp(tail, whole + format_sh_size - sizeof(tail),
                     sizeof(tail)) == 0);
      }

      /* reading at EOF yields 0 */
      {
        char byte;
        CHECK(dwarfs_c_pread(fs, "format.sh", &byte, 1, format_sh_size) == 0);
      }

      free(whole);
    }

    /* errno paths */
    {
      char buf[4];
      CHECK(dwarfs_c_pread(fs, "no-such-file", buf, sizeof(buf), 0) == -1);
      CHECK(dwarfs_c_errno() == ENOENT);

      CHECK(dwarfs_c_pread(fs, "/", buf, sizeof(buf), 0) == -1);
      CHECK(dwarfs_c_errno() == EISDIR);

      CHECK(dwarfs_c_pread(fs, "format.sh", buf, sizeof(buf), -1) == -1);
      CHECK(dwarfs_c_errno() == EINVAL);
    }
  }

  /* ---------------------------------------------------------------- */
  /* Directory listing                                                 */
  /* ---------------------------------------------------------------- */
  {
    dwarfs_c_dir* dir = dwarfs_c_opendir(fs, "/");
    CHECK(dir != NULL);
    if (dir) {
      dwarfs_c_dirent de;
      int rc;
      int count = 0;
      int found_format_sh = 0;
      while ((rc = dwarfs_c_readdir(dir, &de)) == 1) {
        ++count;
        CHECK(de.name != NULL && de.name[0] != '\0');
        CHECK(strcmp(de.name, ".") != 0);
        CHECK(strcmp(de.name, "..") != 0);
        if (strcmp(de.name, "format.sh") == 0) {
          found_format_sh = 1;
          CHECK(de.type == DWARFS_C_FILE_REGULAR);
        }
      }
      CHECK(rc == 0);
      CHECK(count > 0);
      CHECK(found_format_sh);
      dwarfs_c_closedir(dir);
    }

    /* errno paths */
    CHECK(dwarfs_c_opendir(fs, "no-such-dir") == NULL);
    CHECK(dwarfs_c_errno() == ENOENT);

    CHECK(dwarfs_c_opendir(fs, "format.sh") == NULL);
    CHECK(dwarfs_c_errno() == ENOTDIR);
  }

  /* ---------------------------------------------------------------- */
  /* Image metadata                                                    */
  /* ---------------------------------------------------------------- */
  {
    char* info = dwarfs_c_image_info_json(fs);
    CHECK(info != NULL);
    if (info) {
      CHECK(strstr(info, "\"version\"") != NULL);
      CHECK(strstr(info, "\"block_size\"") != NULL);
      dwarfs_c_free(info);
    }
  }

  dwarfs_c_close(fs);

  /* ---------------------------------------------------------------- */
  /* Open from memory                                                  */
  /* ---------------------------------------------------------------- */
  {
    long const size = file_size_of(image_path);
    FILE* f;
    void* data;
    CHECK(size > 0);
    f = fopen(image_path, "rb");
    CHECK(f != NULL);
    data = malloc((size_t)size);
    CHECK(data != NULL);
    if (f && data) {
      dwarfs_c_filesystem* mfs;
      CHECK(fread(data, 1, (size_t)size, f) == (size_t)size);
      mfs = dwarfs_c_open_memory(data, (size_t)size);
      CHECK(mfs != NULL);
      if (mfs) {
        char buf[8];
        CHECK(dwarfs_c_stat(mfs, "format.sh", &st) == 0);
        CHECK(st.size == format_sh_size);
        CHECK(dwarfs_c_pread(mfs, "format.sh", buf, sizeof(buf), 0) ==
              (int64_t)sizeof(buf));
        dwarfs_c_close(mfs);
      }
    }
    free(data);
    if (f) {
      fclose(f);
    }

    CHECK(dwarfs_c_open_memory(NULL, 0) == NULL);
    CHECK(dwarfs_c_errno() == EINVAL);
  }

  /* ---------------------------------------------------------------- */
  /* Open from file region (offset + length)                           */
  /* ---------------------------------------------------------------- */
  {
    long const size = file_size_of(image_path);
    dwarfs_c_filesystem* rfs;
    CHECK(size > 0);

    /* the whole file as a region */
    rfs = dwarfs_c_open_region(image_path, 0, size);
    CHECK(rfs != NULL);
    if (rfs) {
      CHECK(dwarfs_c_stat(rfs, "format.sh", &st) == 0);
      CHECK(st.size == format_sh_size);
      dwarfs_c_close(rfs);
    }

    /* the image embedded behind a junk prefix */
    {
      const char* embedded = "dwarfs_c_smoke_embedded.dwarfs";
      const char junk[128] = {0};
      FILE* in = fopen(image_path, "rb");
      FILE* out = fopen(embedded, "wb");
      CHECK(in != NULL && out != NULL);
      if (in && out) {
        char copy_buf[4096];
        size_t n;
        dwarfs_c_filesystem* efs;
        CHECK(fwrite(junk, 1, sizeof(junk), out) == sizeof(junk));
        while ((n = fread(copy_buf, 1, sizeof(copy_buf), in)) > 0) {
          CHECK(fwrite(copy_buf, 1, n, out) == n);
        }
        fclose(out);
        fclose(in);
        out = NULL;
        in = NULL;

        efs = dwarfs_c_open_region(embedded, (int64_t)sizeof(junk), size);
        CHECK(efs != NULL);
        if (efs) {
          CHECK(dwarfs_c_stat(efs, "format.sh", &st) == 0);
          CHECK(st.size == format_sh_size);
          dwarfs_c_close(efs);
        }
        remove(embedded);
      }
      if (in) {
        fclose(in);
      }
      if (out) {
        fclose(out);
        remove(embedded);
      }
    }

    /* invalid arguments */
    CHECK(dwarfs_c_open_region(image_path, -5, size) == NULL);
    CHECK(dwarfs_c_errno() == EINVAL);
    CHECK(dwarfs_c_open_region(image_path, 0, 0) == NULL);
    CHECK(dwarfs_c_errno() == EINVAL);
  }

  /* ================================================================ */
  /* Writer API                                                       */
  /* ================================================================ */

  /* ---- option validation ---------------------------------------- */
  {
    dwarfs_c_writer_options opts;
    dwarfs_c_writer* w;

    dwarfs_c_writer_options_init(&opts);
    CHECK(opts.struct_version == DWARFS_C_WRITER_OPTIONS_VERSION);
    CHECK(opts.compression == DWARFS_C_COMPRESSION_ZSTD);

    opts.struct_version = 9999;
    CHECK(dwarfs_c_writer_create(&opts) == NULL);
    CHECK(dwarfs_c_errno() == EINVAL);

    dwarfs_c_writer_options_init(&opts);
    opts.compression = 99;
    CHECK(dwarfs_c_writer_create(&opts) == NULL);
    CHECK(dwarfs_c_errno() == EINVAL);

    dwarfs_c_writer_options_init(&opts);
    opts.compression_level = 99; /* out of range for zstd */
    CHECK(dwarfs_c_writer_create(&opts) == NULL);
    CHECK(dwarfs_c_errno() == EINVAL);

    dwarfs_c_writer_options_init(&opts);
    opts.block_size_bits = 7; /* below the 10..30 range */
    CHECK(dwarfs_c_writer_create(&opts) == NULL);
    CHECK(dwarfs_c_errno() == EINVAL);

    /* NULL options mean "all defaults" */
    w = dwarfs_c_writer_create(NULL);
    CHECK(w != NULL);
    CHECK(dwarfs_c_writer_write(w, "wout_nosource.dwarfs") == -1);
    CHECK(dwarfs_c_errno() == EINVAL);
    dwarfs_c_writer_free(w);
    dwarfs_c_writer_free(NULL); /* safe */
  }

  /* ---- add_tree error paths ------------------------------------- */
  {
    dwarfs_c_writer* w = dwarfs_c_writer_create(NULL);
    CHECK(w != NULL);
    CHECK(dwarfs_c_writer_add_tree(w, "/nonexistent/tree", "/") == -1);
    CHECK(dwarfs_c_errno() == ENOENT);
    CHECK(dwarfs_c_writer_add_tree(w, image_path, "/") == -1);
    CHECK(dwarfs_c_errno() == ENOTDIR);
    CHECK(dwarfs_c_writer_add_tree(w, "wtree", "/appsub") == -1);
    CHECK(dwarfs_c_errno() == EINVAL);
    dwarfs_c_writer_free(w);
  }

  /* ---- build a fixture tree ------------------------------------- */
  CHECK(MKDIR("wtree") == 0);
  CHECK(MKDIR("wtree/sub") == 0);
  CHECK(write_text("wtree/hello.txt", "hello-dwarfs-c\n") == 0);
  CHECK(write_text("wtree/sub/nested.txt", "nested-payload-42\n") == 0);

  /* ---- add_tree: EALREADY / EEXIST / round-trip ------------------ */
  {
    dwarfs_c_writer* w = dwarfs_c_writer_create(NULL);
    FILE* f;
    CHECK(w != NULL);
    CHECK(dwarfs_c_writer_add_tree(w, "wtree", "/") == 0);
    CHECK(dwarfs_c_writer_add_tree(w, "wtree", "/") == -1);
    CHECK(dwarfs_c_errno() == EALREADY);
    CHECK(dwarfs_c_writer_add_file(w, "wtree/hello.txt", "hello.txt") == -1);
    CHECK(dwarfs_c_errno() == EALREADY);

    /* the writer never overwrites */
    f = fopen("wout_tree.dwarfs", "wb");
    CHECK(f != NULL);
    if (f) {
      fclose(f);
    }
    CHECK(dwarfs_c_writer_write(w, "wout_tree.dwarfs") == -1);
    CHECK(dwarfs_c_errno() == EEXIST);
    remove("wout_tree.dwarfs");

    CHECK(dwarfs_c_writer_write(w, "wout_tree.dwarfs") == 0);
    CHECK(dwarfs_c_errno() == 0);
    /* single-shot: no second write, no adds after write */
    CHECK(dwarfs_c_writer_write(w, "wout_tree2.dwarfs") == -1);
    CHECK(dwarfs_c_errno() == EALREADY);
    CHECK(dwarfs_c_writer_add_tree(w, "wtree", "/") == -1);
    CHECK(dwarfs_c_errno() == EALREADY);
    dwarfs_c_writer_free(w);
  }

  /* read the tree image back through the reader API */
  {
    dwarfs_c_filesystem* rfs = dwarfs_c_open("wout_tree.dwarfs");
    char* json;
    CHECK(rfs != NULL);
    if (rfs) {
      CHECK(dwarfs_c_stat(rfs, "/", &st) == 0);
      CHECK(st.type == DWARFS_C_FILE_DIRECTORY);
      CHECK(dwarfs_c_stat(rfs, "sub", &st) == 0);
      CHECK(st.type == DWARFS_C_FILE_DIRECTORY);
      CHECK(readback_matches(rfs, "hello.txt", "hello-dwarfs-c\n"));
      CHECK(readback_matches(rfs, "sub/nested.txt", "nested-payload-42\n"));

      /* metadata sections must be present and parse */
      json = dwarfs_c_image_info_json(rfs);
      CHECK(json != NULL);
      if (json) {
        CHECK(strstr(json, "\"sections\"") != NULL);
        CHECK(strstr(json, "\"history\"") != NULL);
        dwarfs_c_free(json);
      }
      dwarfs_c_close(rfs);
    }
  }

  /* ---- add_file mode --------------------------------------------- */
  {
    dwarfs_c_writer* w = dwarfs_c_writer_create(NULL);
    CHECK(w != NULL);
    /* rename not supported in v1 */
    CHECK(dwarfs_c_writer_add_file(w, "wtree/hello.txt", "renamed.txt") == -1);
    CHECK(dwarfs_c_errno() == EINVAL);
    CHECK(dwarfs_c_writer_add_file(w, "/nonexistent/f.txt", "f.txt") == -1);
    CHECK(dwarfs_c_errno() == ENOENT);
    CHECK(dwarfs_c_writer_add_file(w, "wtree/hello.txt", "hello.txt") == 0);
    /* all files must share one directory in v1 */
    CHECK(dwarfs_c_writer_add_file(w, "wtree/sub/nested.txt", "nested.txt") == -1);
    CHECK(dwarfs_c_errno() == EINVAL);
    CHECK(dwarfs_c_writer_write(w, "wout_files.dwarfs") == 0);
    dwarfs_c_writer_free(w);

    /* read it back */
    {
      dwarfs_c_filesystem* rfs = dwarfs_c_open("wout_files.dwarfs");
      CHECK(rfs != NULL);
      if (rfs) {
        CHECK(readback_matches(rfs, "hello.txt", "hello-dwarfs-c\n"));
        dwarfs_c_close(rfs);
      }
    }
  }

  /* cleanup (wout_tree.dwarfs stays: the dwarfs_c_writer_tool_check
     ctest inspects it with the repo's own dwarfsck) */
  remove("wout_files.dwarfs");
  remove("wtree/hello.txt");
  remove("wtree/sub/nested.txt");
  remove("wtree/sub");
  remove("wtree");

  if (failures > 0) {
    fprintf(stderr, "dwarfs_c_smoke_test: %d check(s) failed\n", failures);
    return 1;
  }

  printf("dwarfs_c_smoke_test: all checks passed\n");
  return 0;
}
