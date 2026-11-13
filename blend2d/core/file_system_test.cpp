// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_test_p.h>
#if !defined(BL_BUILD_NO_FILESYSTEM) && defined(BL_TEST)

#include <blend2d/axl/commons.h>
#include <blend2d/core/file_system.h>
#include <blend2d/core/string.h>

#include <stdio.h>

#if !defined(_WIN32)
  #include <sys/time.h> // required by gettimeofday()
#endif

// FileSystem - Tests
// ==================

namespace bl::FileSystemTests {

// It's not fully random, just something (inspired in AsmJit).
static void generate_random_name(BLString& dst) noexcept {
  static std::atomic<uint32_t> internal_counter;

#if defined(__GNUC__) && BL_TARGET_ARCH_X86
  // Use RDTSC instruction to avoid gettimeofday() as we just need some "random" bits.
  uint64_t mix = __builtin_ia32_rdtsc();
#elif defined(_WIN32)
  uint64_t mix = GetTickCount();
#else
  struct timeval tm {};
  uint64_t mix = 1; // only used when gettimeofday() fails, which is unlikely.
  if (gettimeofday(&tm, nullptr) == 0) {
    mix = uint64_t(tm.tv_usec) ^ uint64_t(tm.tv_sec);
  }
#endif

  uint32_t count = internal_counter.fetch_add(1u);
  uint64_t bits = (uint64_t(reinterpret_cast<uintptr_t>(&dst)) & 0x1010505000055590u) - mix * 773703683;
  bits = (bits >> 33) ^ (bits << 7) ^ (count * 87178291199);
  bits += uint64_t(count) * 10619863;
  EXPECT_OK(dst.assign_format("__bl_test_fs_%u_%016llX.tmp", count, bits));
}

struct TemporaryFile {
  BLString name;
  bool _failed {};

  TemporaryFile() noexcept {
    BLFile file;

    for (;;) {
      BLFileInfo info;

      generate_random_name(name);
      BLResult result = BLFileSystem::get_file_info(name.data(), &info);

      if (result == BL_SUCCESS) {
        continue;
      }

      if (result == BL_ERROR_NO_ENTRY) {
        break;
      }

      _failed = true;
      break;
    }
  }

  ~TemporaryFile() noexcept {
    // The public filesystem API doesn't provide file removal.
    if (!_failed) {
      EXPECT_EQ(remove(name.data()), 0);
    }
  }
};

TEST_CASE(core_file_basics) {
  TemporaryFile tmp;

  BLFile file;
  EXPECT_OK(file.open(tmp.name.data(), BLFileOpenFlags(BL_FILE_OPEN_RW | BL_FILE_OPEN_CREATE)));
  EXPECT_TRUE(file.is_open());

  const uint8_t data[] = { 0x00, 0x12, 0x34, 0x56, 0x78, 0x80, 0xFE, 0xFF };
  uint8_t buffer[sizeof(data) + 1] = {};
  size_t bytes_written = 0;
  size_t bytes_read = 0;
  uint64_t size = 1;
  int64_t position = -1;

  EXPECT_OK(file.get_size(&size));
  EXPECT_EQ(size, 0u);
  EXPECT_OK(file.write(data, sizeof(data), &bytes_written));
  EXPECT_EQ(bytes_written, sizeof(data));
  EXPECT_OK(file.get_size(&size));
  EXPECT_EQ(size, sizeof(data));

  BLFileInfo info{};
  EXPECT_OK(file.get_info(&info));
  EXPECT_TRUE(info.is_valid());
  EXPECT_TRUE(info.is_regular());
  EXPECT_FALSE(info.is_directory());
  EXPECT_EQ(info.size, size);

  BLFileInfo path_info{};
  EXPECT_OK(BLFileSystem::get_file_info(tmp.name.data(), &path_info));
  EXPECT_TRUE(path_info.is_valid());
  EXPECT_TRUE(path_info.is_regular());
  EXPECT_EQ(path_info.size, info.size);
  EXPECT_EQ(path_info.modified_time, info.modified_time);
  EXPECT_EQ(path_info.permissions, info.permissions);

  EXPECT_OK(file.seek(0, BL_FILE_SEEK_CUR, &position));
  EXPECT_EQ(position, int64_t(sizeof(data)));

  EXPECT_OK(file.seek(-2, BL_FILE_SEEK_END, &position));
  EXPECT_EQ(position, int64_t(sizeof(data) - 2));

  EXPECT_OK(file.seek(-2, BL_FILE_SEEK_CUR, &position));
  EXPECT_EQ(position, int64_t(sizeof(data) - 4));

  EXPECT_OK(file.read(buffer, 2, &bytes_read));
  EXPECT_EQ(bytes_read, 2u);
  EXPECT_EQ(buffer[0], data[4]);
  EXPECT_EQ(buffer[1], data[5]);

  EXPECT_OK(file.seek(0, BL_FILE_SEEK_SET));
  EXPECT_OK(file.read(buffer, sizeof(buffer), &bytes_read));
  EXPECT_EQ(bytes_read, sizeof(data));
  for (size_t i = 0; i < sizeof(data); i++) {
    EXPECT_EQ(buffer[i], data[i]);
  }

  EXPECT_OK(file.read(buffer, sizeof(buffer), &bytes_read));
  EXPECT_EQ(bytes_read, 0u);

  EXPECT_OK(file.write(data, 0, &bytes_written));
  EXPECT_EQ(bytes_written, 0u);

  // Truncation must preserve the position and must never extend a file.
  EXPECT_OK(file.truncate(3));
  EXPECT_OK(file.get_size(&size));
  EXPECT_EQ(size, 3u);

  EXPECT_OK(file.seek(0, BL_FILE_SEEK_CUR, &position));
  EXPECT_EQ(position, int64_t(sizeof(data)));

  EXPECT_OK(file.truncate(sizeof(data)));
  EXPECT_OK(file.get_size(&size));
  EXPECT_EQ(size, 3u);
  EXPECT_EQ(file.truncate(-1), BL_ERROR_INVALID_VALUE);

  EXPECT_OK(file.close());
  EXPECT_FALSE(file.is_open());

  EXPECT_OK(file.close());
  EXPECT_OK(file.open(tmp.name.data(), BL_FILE_OPEN_READ));
  EXPECT_OK(file.read(buffer, sizeof(buffer), &bytes_read));
  EXPECT_EQ(bytes_read, 3u);

  for (size_t i = 0; i < bytes_read; i++) {
    EXPECT_EQ(buffer[i], data[i]);
  }

  EXPECT_OK(file.close());
  EXPECT_OK(file.open(tmp.name.data(), BLFileOpenFlags(BL_FILE_OPEN_WRITE | BL_FILE_OPEN_TRUNCATE)));
  EXPECT_OK(file.get_size(&size));
  EXPECT_EQ(size, 0u);
}

TEST_CASE(core_file_closed) {
  BLFile file;
  EXPECT_FALSE(file.is_open());
  EXPECT_OK(file.close());

  uint8_t buffer[1] = {};
  size_t bytes_read = 1;
  size_t bytes_written = 1;
  uint64_t size = 1;
  int64_t position = 0;

  EXPECT_EQ(file.read(buffer, sizeof(buffer), &bytes_read), BL_ERROR_INVALID_HANDLE);
  EXPECT_EQ(bytes_read, 0u);
  EXPECT_EQ(file.write(buffer, sizeof(buffer), &bytes_written), BL_ERROR_INVALID_HANDLE);
  EXPECT_EQ(bytes_written, 0u);
  EXPECT_EQ(file.seek(0, BL_FILE_SEEK_SET, &position), BL_ERROR_INVALID_HANDLE);
  EXPECT_EQ(position, -1);
  EXPECT_EQ(file.truncate(0), BL_ERROR_INVALID_HANDLE);
  EXPECT_EQ(file.get_size(&size), BL_ERROR_INVALID_HANDLE);
  EXPECT_EQ(size, 0u);

  BLFileInfo info{};
  EXPECT_EQ(file.get_info(&info), BL_ERROR_INVALID_HANDLE);
  EXPECT_FALSE(info.is_valid());
  EXPECT_FALSE(info.is_regular());
  EXPECT_FALSE(info.is_directory());
  EXPECT_FALSE(info.is_symlink());
}

TEST_CASE(core_file_move_swap) {
  TemporaryFile tmp;
  BLFile a;
  EXPECT_OK(a.open(tmp.name.data(), BLFileOpenFlags(BL_FILE_OPEN_RW | BL_FILE_OPEN_CREATE)));
  EXPECT_OK(a.seek(3, BL_FILE_SEEK_SET));

  BLFile b(axl::move(a));
  EXPECT_FALSE(a.is_open());
  EXPECT_TRUE(b.is_open());
  int64_t position = -1;
  EXPECT_OK(b.seek(0, BL_FILE_SEEK_CUR, &position));
  EXPECT_EQ(position, 3);

  EXPECT_OK(a.open(tmp.name.data(), BL_FILE_OPEN_RW));
  a.swap(b);
  EXPECT_OK(a.seek(0, BL_FILE_SEEK_CUR, &position));
  EXPECT_EQ(position, 3);
  EXPECT_OK(b.seek(0, BL_FILE_SEEK_CUR, &position));
  EXPECT_EQ(position, 0);

  b = axl::move(a);
  EXPECT_FALSE(a.is_open());
  EXPECT_TRUE(b.is_open());
  EXPECT_OK(b.seek(0, BL_FILE_SEEK_CUR, &position));
  EXPECT_EQ(position, 3);

  a.swap(b);
  EXPECT_TRUE(a.is_open());
  EXPECT_FALSE(b.is_open());
  EXPECT_OK(a.seek(0, BL_FILE_SEEK_CUR, &position));
  EXPECT_EQ(position, 3);
}

TEST_CASE(core_file_system_read_write_file) {
  TemporaryFile tmp;
  BLArray<uint8_t> data;
  BLArray<uint8_t> buffer;
  const uint8_t bytes[] = { 0x00, 0x12, 0x34, 0x56, 0x78, 0x80, 0xFE, 0xFF };
  EXPECT_OK(data.assign_data(bytes, sizeof(bytes)));

  auto verify_written_file = [&]() noexcept {
    EXPECT_OK(BLFileSystem::read_file(tmp.name.data(), buffer));
    EXPECT_EQ(buffer, data);
  };

  size_t bytes_written = 0;
  EXPECT_OK(BLFileSystem::write_file(tmp.name.data(), data.data(), data.size(), &bytes_written));
  EXPECT_EQ(bytes_written, data.size());
  verify_written_file();

  EXPECT_OK(BLFileSystem::write_file(tmp.name.data(), data.data(), data.size()));
  verify_written_file();

  bytes_written = 0;
  EXPECT_OK(BLFileSystem::write_file(tmp.name.data(), data.span(), &bytes_written));
  EXPECT_EQ(bytes_written, data.size());
  verify_written_file();

  EXPECT_OK(BLFileSystem::write_file(tmp.name.data(), data.span()));
  verify_written_file();

  bytes_written = 0;
  EXPECT_OK(BLFileSystem::write_file(tmp.name.data(), data, &bytes_written));
  EXPECT_EQ(bytes_written, data.size());
  verify_written_file();

  EXPECT_OK(BLFileSystem::write_file(tmp.name.data(), data));
  verify_written_file();

  const BLFileReadFlags read_flags[] = {
    BL_FILE_READ_NO_FLAGS,
    BLFileReadFlags(BL_FILE_READ_MMAP_ENABLED | BL_FILE_READ_MMAP_NO_FALLBACK),
    BLFileReadFlags(BL_FILE_READ_MMAP_ENABLED | BL_FILE_READ_MMAP_AVOID_SMALL | BL_FILE_READ_MMAP_NO_FALLBACK)
  };

  for (BLFileReadFlags flags : read_flags) {
    EXPECT_OK(BLFileSystem::read_file(tmp.name.data(), buffer, 0, flags));
    EXPECT_EQ(buffer, data);
    EXPECT_OK(BLFileSystem::read_file(tmp.name.data(), buffer, 3, flags));
    EXPECT_TRUE(buffer.equals(BLSpan<const uint8_t>{data.data(), 3}));
    EXPECT_OK(BLFileSystem::read_file(tmp.name.data(), buffer, data.size() + 1, flags));
    EXPECT_EQ(buffer, data);
  }

  // Release any mapping before overwriting the file (required on Windows).
  EXPECT_OK(buffer.reset());
  EXPECT_OK(data.resize(3, 0));

  EXPECT_OK(BLFileSystem::write_file(tmp.name.data(), data));
  verify_written_file();

  EXPECT_OK(BLFileSystem::write_file(tmp.name.data(), nullptr, 0, &bytes_written));
  EXPECT_EQ(bytes_written, 0u);
  for (BLFileReadFlags flags : read_flags) {
    EXPECT_OK(buffer.assign(data));
    EXPECT_OK(BLFileSystem::read_file(tmp.name.data(), buffer, 0, flags));
    EXPECT_TRUE(buffer.is_empty());
  }
}

TEST_CASE(core_file_system_errors) {
  TemporaryFile tmp;
  BLFile file;

  EXPECT_EQ(file.open(tmp.name.data(), BL_FILE_OPEN_NO_FLAGS), BL_ERROR_INVALID_VALUE);
  EXPECT_EQ(file.open(tmp.name.data(), BLFileOpenFlags(BL_FILE_OPEN_READ | BL_FILE_OPEN_CREATE)), BL_ERROR_INVALID_VALUE);
  EXPECT_FALSE(file.is_open());

  EXPECT_OK(file.open(tmp.name.data(), BLFileOpenFlags(BL_FILE_OPEN_RW | BL_FILE_OPEN_CREATE)));
  EXPECT_OK(file.seek(3, BL_FILE_SEEK_SET));
  EXPECT_EQ(file.open(tmp.name.data(), BLFileOpenFlags(BL_FILE_OPEN_RW | BL_FILE_OPEN_CREATE_EXCLUSIVE)), BL_ERROR_ALREADY_EXISTS);
  EXPECT_TRUE(file.is_open());

  int64_t position = -1;
  EXPECT_OK(file.seek(0, BL_FILE_SEEK_CUR, &position));
  EXPECT_EQ(position, 3);
  EXPECT_EQ(file.seek(0, BLFileSeekType(999), &position), BL_ERROR_INVALID_VALUE);
  EXPECT_EQ(position, -1);

  BLFileInfo info{};
  EXPECT_OK(BLFileSystem::get_file_info(".", &info));
  EXPECT_TRUE(info.is_valid());
  EXPECT_TRUE(info.is_directory());
  EXPECT_FALSE(info.is_regular());
}

TEST_CASE(core_directory_iterator) {
  TemporaryFile tmp;
  const char data[] = "DirectoryIterator test";
  EXPECT_OK(BLFileSystem::write_file(tmp.name.data(), data, sizeof(data)));

  BLFileInfo expected_info{};
  EXPECT_OK(BLFileSystem::get_file_info(tmp.name.data(), &expected_info));

  BLDirectoryIterator iterator;
  BLStringView name{};
  BLFileInfo info{};

  // Reopening the same iterator also tests replacement of an existing directory handle.
  const bool info_options[] = {false, true};
  for (bool with_info : info_options) {
    EXPECT_OK(iterator.open("."));
    EXPECT_TRUE(iterator.is_open());

    size_t matches = 0;
    BLResult result;

    for (;;) {
      result = with_info ? iterator.next(&name, &info) : iterator.next(&name);
      if (name.is_empty()) {
        break;
      }

      EXPECT_FALSE(name.equals(BLStringView{".", 1}));
      EXPECT_FALSE(name.equals(BLStringView{"..", 2}));

      if (tmp.name.equals(name)) {
        EXPECT_OK(result);
        EXPECT_EQ(++matches, 1u);
        if (with_info) {
          EXPECT_TRUE(info.is_valid());
          EXPECT_TRUE(info.is_regular());
          EXPECT_FALSE(info.is_directory());
          EXPECT_EQ(info.size, sizeof(data));
          EXPECT_EQ(info.modified_time, expected_info.modified_time);
          EXPECT_EQ(info.permissions, expected_info.permissions);
        }
      }
      else if (!with_info) {
        EXPECT_OK(result);
      }
      // Metadata of unrelated entries can fail, e.g. for dangling symlinks in the working directory.
    }

    EXPECT_EQ(result, BL_ERROR_NO_MORE_FILES);
    EXPECT_EQ(matches, 1u);

    name = BLStringView{"stale", 5};
    info = expected_info;
    EXPECT_EQ(iterator.next(&name, &info), BL_ERROR_NO_MORE_FILES);
    EXPECT_TRUE(name.is_empty());
    EXPECT_FALSE(info.is_valid());
  }

  EXPECT_OK(iterator.close());
  EXPECT_FALSE(iterator.is_open());
  EXPECT_OK(iterator.close());
  EXPECT_FALSE(iterator.is_open());

  name = BLStringView{"stale", 5};
  info = expected_info;
  EXPECT_EQ(iterator.next(&name, &info), BL_ERROR_INVALID_HANDLE);
  EXPECT_TRUE(name.is_empty());
  EXPECT_FALSE(info.is_valid());

  EXPECT_OK(iterator.open("."));
  EXPECT_NE(iterator.open(tmp.name.data()), BL_SUCCESS);

  BLDirectoryIterator moved(axl::move(iterator));
  EXPECT_EQ(iterator.next(&name), BL_ERROR_INVALID_HANDLE);
  EXPECT_OK(moved.next(&name));
  EXPECT_FALSE(name.is_empty());
  EXPECT_OK(moved.reset());
  EXPECT_EQ(moved.next(&name), BL_ERROR_INVALID_HANDLE);
  EXPECT_OK(moved.open("."));
}

TEST_CASE(core_file_system_c_api_vs_cxx_api) {
  BLFileCore file;

  EXPECT_OK(bl_file_init(&file));
  EXPECT_FALSE(bl_file_is_open(&file));
  EXPECT_OK(bl_file_reset(&file));

  BLDirectoryIteratorCore dir_it;
  EXPECT_OK(bl_directory_iterator_init(&dir_it));
  EXPECT_FALSE(bl_directory_iterator_is_open(&dir_it));
  EXPECT_OK(bl_directory_iterator_reset(&dir_it));
}

} // {bl::FileSystemTests}

#endif // !BL_BUILD_NO_FILESYSTEM && BL_TEST
