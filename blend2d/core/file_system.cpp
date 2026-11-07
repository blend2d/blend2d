// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_p.h>
#if !defined(BL_BUILD_NO_FILESYSTEM)

#include <blend2d/axl/commons.h>
#include <blend2d/axl/mem_alloc.h>
#include <blend2d/axl/string_utils_p.h>
#include <blend2d/core/array_p.h>
#include <blend2d/core/file_system_p.h>
#include <blend2d/core/runtime_p.h>
#include <blend2d/threading/atomic_p.h>
#include <blend2d/unicode/unicode_p.h>

#if !defined(_WIN32)
  #include <dirent.h>
  #include <errno.h>
  #include <fcntl.h>
  #include <unistd.h>
  #include <sys/mman.h>
  #include <sys/stat.h>
  #include <sys/types.h>
  #if defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
    #define BL_FS_STAT_MTIMESPEC(s) s.st_mtim
  #elif defined(__NetBSD__) || defined(__APPLE__)
    #define BL_FS_STAT_MTIMESPEC(s) s.st_mtimespec
  #endif

  #if !defined(AT_NO_AUTOMOUNT)
  #endif
#endif

// FileSystem - File - API - Generic Implementation
// ================================================

namespace bl::FileSystemDetail {

// Just a helper to get this without having to case to `BLFile` to access `BLFile::is_open()`.
static BL_INLINE bool is_file_open(const BLFileCore* self) noexcept { return self->handle != -1; }

} // {bl::FileSystemDetail}

BL_API_IMPL BLResult bl_file_init(BLFileCore* self) noexcept {
  self->handle = -1;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_file_reset(BLFileCore* self) noexcept {
  return bl_file_close(self);
}

BL_API_IMPL bool bl_file_is_open(const BLFileCore* self) noexcept {
  using namespace bl;

  return FileSystemDetail::is_file_open(self);
}

#ifdef _WIN32

// FileSystem - File - Windows Utilities
// =====================================

namespace bl::FileSystemDetail {

static constexpr size_t kStaticUTF16StringSize = 1024 + 512;

template<size_t N = kStaticUTF16StringSize>
class Utf16PathTmp {
public:
  uint16_t* _data;
  size_t _size;
  size_t _capacity;
  uint16_t _embedded_data[N + 1];

  BL_NONCOPYABLE(Utf16PathTmp);

  BL_INLINE Utf16PathTmp() noexcept
    : _data(_embedded_data),
      _size(0),
      _capacity(N) {}

  BL_INLINE ~Utf16PathTmp() noexcept {
    if (_data != _embedded_data) {
      axl::mem_free(_data);
    }
  }

  BL_INLINE BLResult init_capacity(size_t capacity) noexcept {
    // Must be called to initialize the capacity only - never multiple times.
    BL_ASSERT(_data == _embedded_data);
    BL_ASSERT(_size == 0u);

    if (capacity > _capacity) {
      uint16_t* new_data = axl::mem_alloc<uint16_t>((capacity + 1) * 2u);
      if (new_data == nullptr) {
        return bl_make_error(BL_ERROR_OUT_OF_MEMORY);
      }
      _data = new_data;
      _capacity = capacity;
    }
    return BL_SUCCESS;
  }

  BL_INLINE_NODEBUG uint16_t* data() const noexcept { return _data; }
  BL_INLINE_NODEBUG wchar_t* data_as_wchar() const noexcept { return reinterpret_cast<wchar_t*>(_data); }

  BL_INLINE_NODEBUG size_t size() const noexcept { return _size; }
  BL_INLINE_NODEBUG size_t capacity() const noexcept { return _capacity; }

  BL_INLINE_NODEBUG void null_terminate() noexcept { _data[_size] = uint16_t(0); }

  BL_NOINLINE BLResult append_utf8(BLStringView str) noexcept {
    size_t remaining_capacity = _capacity - _size;

    Unicode::ConversionState conversion_state;
    BLResult result = Unicode::convert_unicode(
      _data + _size, remaining_capacity * 2u, BL_TEXT_ENCODING_UTF16, str.data(), str.size(), BL_TEXT_ENCODING_UTF8, conversion_state);

    _size += conversion_state.dst_index / 2u;
    null_terminate();
    return result;
  }

  BL_INLINE void append_char(uint16_t uc) noexcept {
    BL_ASSERT(_size < _capacity);
    _data[_size++] = uc;
    null_terminate();
  }
};

template<size_t N>
static BL_NOINLINE BLResult convert_path_to_utf16(Utf16PathTmp<N>& dst, BLStringView str) noexcept {
  if (str.is_empty()) {
    return bl_make_error(BL_ERROR_INVALID_VALUE);
  }

  // Init with extra capacity so the path can be prepared to API call - appending "*", slashes, etc...
  BL_PROPAGATE(dst.init_capacity(str.size() + 8u));

  // Append UTF-16 encoded file-name.
  BL_PROPAGATE(dst.append_utf8(str));

  // Convert slashes to Windows format.
  size_t dst_size = dst.size();
  uint16_t* dst_data = dst.data();

  for (size_t i = 0; i < dst_size; i++) {
    if (dst_data[i] == '/') {
      dst_data[i] = '\\';
    }
  }

  return BL_SUCCESS;
}

static BL_INLINE uint64_t combine_hi_lo(uint32_t hi, uint32_t lo) noexcept {
  return (uint64_t(hi) << 32) | lo;
}

static BL_INLINE uint64_t unix_micro_from_file_time(FILETIME ft) noexcept {
  constexpr uint64_t kFileTimeToUnixTimeS = 11644473600;
  constexpr uint32_t kMicrosecondsPerSecond = 1000000;

  // First convert to microseconds, starting from 1601-01-01 [UTC].
  uint64_t t = combine_hi_lo(ft.dwHighDateTime, ft.dwLowDateTime) / 10u;

  return int64_t(t) - int64_t(kFileTimeToUnixTimeS * kMicrosecondsPerSecond);
}

static BLFilePermissions file_permissions_from_win_file_attributes(DWORD file_attributes) noexcept {
  uint32_t perm = 0;

  // Windows specific file attributes.
  if (file_attributes & FILE_ATTRIBUTE_READONLY) {
    perm |= BL_FILE_PERMISSION_WINDOWS_READ_ONLY;
  }

  return static_cast<BLFilePermissions>(perm);
}

static BLFileInfoFlags file_flags_from_win_file_attributes(DWORD file_attributes) noexcept {
  uint32_t flags = BL_FILE_INFO_VALID;

  if (file_attributes & FILE_ATTRIBUTE_DIRECTORY) {
    flags |= BL_FILE_INFO_DIRECTORY;
  }
  else {
    flags |= BL_FILE_INFO_REGULAR;
  }

  if (file_attributes & FILE_ATTRIBUTE_REPARSE_POINT) {
    flags |= BL_FILE_INFO_SYMLINK;
  }

  if (file_attributes & FILE_ATTRIBUTE_DEVICE) {
    flags |= BL_FILE_INFO_CHAR_DEVICE;
  }

  if (file_attributes & FILE_ATTRIBUTE_HIDDEN) {
    flags |= BL_FILE_INFO_HIDDEN;
  }

  // Windows specific file attributes.
  if (file_attributes & FILE_ATTRIBUTE_ARCHIVE) {
    flags |= BL_FILE_INFO_ARCHIVE;
  }

  if (file_attributes & FILE_ATTRIBUTE_SYSTEM) {
    flags |= BL_FILE_INFO_SYSTEM;
  }

  return static_cast<BLFileInfoFlags>(flags);
}

static BLResult file_info_from_win_file_attribute_data(BLFileInfo& info, const WIN32_FILE_ATTRIBUTE_DATA& fa) noexcept {
  info.permissions = file_permissions_from_win_file_attributes(fa.dwFileAttributes);
  info.flags = file_flags_from_win_file_attributes(fa.dwFileAttributes);
  info.size = combine_hi_lo(fa.nFileSizeHigh, fa.nFileSizeLow);
  info.modified_time = unix_micro_from_file_time(fa.ftLastWriteTime);

  return BL_SUCCESS;
}

} // {bl::FileSystemDetail} namespace

// FileSystem - File - API - Windows Implementation
// ================================================

static const constexpr DWORD kFileBufferSize = 32 * 1024 * 1024; // 32 MB.

BL_API_IMPL BLResult bl_file_open(BLFileCore* self, const char* file_name, BLFileOpenFlags open_flags) noexcept {
  using namespace bl;

  // Desired Access
  // --------------
  //
  // The same flags as O_RDONLY|O_WRONLY|O_RDWR:

  DWORD desired_access = 0;
  switch (open_flags & BL_FILE_OPEN_RW) {
    case BL_FILE_OPEN_READ : desired_access = GENERIC_READ ; break;
    case BL_FILE_OPEN_WRITE: desired_access = GENERIC_WRITE; break;
    case BL_FILE_OPEN_RW   : desired_access = GENERIC_READ | GENERIC_WRITE; break;

    default:
      return bl_make_error(BL_ERROR_INVALID_VALUE);
  }

  if (open_flags & BL_FILE_OPEN_DELETE) {
    desired_access |= DELETE;
  }

  // Creation Disposition
  // --------------------
  //
  // Since WinAPI documentation is so brief here is a better explanation about various CreationDisposition modes,
  // reformatted from SO:
  //
  //   https://stackoverflow.com/questions/14469607/difference-between-open-always-and-create-always-in-createfile-of-windows-api
  //
  // +-------------------------+-------------+--------------------+
  // | Creation Disposition    | File Exists | File Doesn't Exist |
  // +-------------------------+-------------+--------------------+
  // | CREATE_ALWAYS           | Truncate    | Create New         |
  // | CREATE_NEW              | Fail        | Create New         |
  // | OPEN_ALWAYS             | Open        | Create New         |
  // | OPEN_EXISTING           | Open        | Fail               |
  // | TRUNCATE_EXISTING       | Truncate    | Fail               |
  // +-------------------------+-------------+--------------------+

  uint32_t kExtFlags = BL_FILE_OPEN_CREATE | BL_FILE_OPEN_CREATE_EXCLUSIVE | BL_FILE_OPEN_TRUNCATE;

  if ((open_flags & kExtFlags) && (!(open_flags & BL_FILE_OPEN_WRITE))) {
    return bl_make_error(BL_ERROR_INVALID_VALUE);
  }

  DWORD creation_disposition = OPEN_EXISTING;
  if (open_flags & BL_FILE_OPEN_CREATE_EXCLUSIVE) {
    creation_disposition = CREATE_NEW;
  }
  else if ((open_flags & (BL_FILE_OPEN_CREATE | BL_FILE_OPEN_TRUNCATE)) == BL_FILE_OPEN_CREATE) {
    creation_disposition = OPEN_ALWAYS;
  }
  else if ((open_flags & (BL_FILE_OPEN_CREATE | BL_FILE_OPEN_TRUNCATE)) == (BL_FILE_OPEN_CREATE | BL_FILE_OPEN_TRUNCATE)) {
    creation_disposition = CREATE_ALWAYS;
  }
  else if (open_flags & BL_FILE_OPEN_TRUNCATE) {
    creation_disposition = TRUNCATE_EXISTING;
  }

  // Share Mode
  // ----------

  DWORD share_mode = 0;

  auto is_shared = [&](uint32_t access, uint32_t exclusive) noexcept -> bool {
    return (open_flags & (access | exclusive)) == access;
  };

  if (is_shared(BL_FILE_OPEN_READ, BL_FILE_OPEN_READ_EXCLUSIVE)) share_mode |= FILE_SHARE_READ;
  if (is_shared(BL_FILE_OPEN_WRITE, BL_FILE_OPEN_WRITE_EXCLUSIVE)) share_mode |= FILE_SHARE_WRITE;
  if (is_shared(BL_FILE_OPEN_DELETE, BL_FILE_OPEN_DELETE_EXCLUSIVE)) share_mode |= FILE_SHARE_DELETE;

  // WinAPI Call
  // -----------

  // NOTE: Do not close the file before calling `CreateFileW()`. We should behave atomically, which means that
  // we only close the existing file if `CreateFileW()` succeeds, otherwise we do nothing and the previous file
  // would still be open.
  size_t file_name_len = strlen(file_name);

  FileSystemDetail::Utf16PathTmp<> file_name_w;
  BL_PROPAGATE(FileSystemDetail::convert_path_to_utf16(file_name_w, {file_name, file_name_len}));

#if defined(BL_PLATFORM_UWP)
  // UWP platform doesn't provide CreateFileW, but we can use CreateFile2 instead.
  HANDLE handle = CreateFile2(
    file_name_w.data_as_wchar(),
    desired_access,
    share_mode,
    creation_disposition,
    nullptr
  );
#else
  DWORD flags_and_attributes = 0;
  LPSECURITY_ATTRIBUTES security_attributes_ptr = nullptr;

  HANDLE handle = CreateFileW(
    file_name_w.data_as_wchar(),
    desired_access,
    share_mode,
    security_attributes_ptr,
    creation_disposition,
    flags_and_attributes,
    nullptr
  );
#endif

  if (handle == INVALID_HANDLE_VALUE) {
    return bl_make_error(bl_result_from_win_error(GetLastError()));
  }

  bl_file_close(self);
  self->handle = intptr_t(handle);

  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_file_close(BLFileCore* self) noexcept {
  using namespace bl;

  // Not sure what should happen if `CloseHandle()` fails, if the handle is invalid or the close can be called
  // again? To ensure compatibility with POSIX implementation we just make it invalid.
  if (FileSystemDetail::is_file_open(self)) {
    HANDLE handle = (HANDLE)self->handle;
    BOOL result = CloseHandle(handle);

    self->handle = -1;
    if (!result) {
      return bl_make_error(bl_result_from_win_error(GetLastError()));
    }
  }

  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_file_seek(BLFileCore* self, int64_t offset, BLFileSeekType seek_type, int64_t* position_out) noexcept {
  using namespace bl;

  *position_out = -1;
  DWORD move_method = 0;

  switch (seek_type) {
    case BL_FILE_SEEK_SET: move_method = FILE_BEGIN  ; break;
    case BL_FILE_SEEK_CUR: move_method = FILE_CURRENT; break;
    case BL_FILE_SEEK_END: move_method = FILE_END    ; break;

    default:
      return bl_make_error(BL_ERROR_INVALID_VALUE);
  }

  if (!FileSystemDetail::is_file_open(self)) {
    return bl_make_error(BL_ERROR_INVALID_HANDLE);
  }

  LARGE_INTEGER to;
  LARGE_INTEGER prev;

  to.QuadPart = offset;
  prev.QuadPart = 0;

  HANDLE handle = (HANDLE)self->handle;
  BOOL result = SetFilePointerEx(handle, to, &prev, move_method);

  if (!result) {
    return bl_make_error(bl_result_from_win_error(GetLastError()));
  }

  *position_out = prev.QuadPart;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_file_read(BLFileCore* self, void* buffer, size_t n, size_t* bytes_read_out) noexcept {
  using namespace bl;

  *bytes_read_out = 0;
  if (!FileSystemDetail::is_file_open(self)) {
    return bl_make_error(BL_ERROR_INVALID_HANDLE);
  }

  BOOL result = true;
  HANDLE handle = (HANDLE)self->handle;

  size_t remaining_size = n;
  size_t bytes_read_total = 0;

  while (remaining_size) {
    DWORD local_size = static_cast<DWORD>(axl::min<size_t>(remaining_size, kFileBufferSize));
    DWORD bytes_read = 0;

    result = ReadFile(handle, axl::offset_ptr(buffer, bytes_read_total), local_size, &bytes_read, nullptr);
    remaining_size -= local_size;
    bytes_read_total += bytes_read;

    if (bytes_read < local_size || !result) {
      break;
    }
  }

  *bytes_read_out = bytes_read_total;
  if (!result) {
    DWORD e = GetLastError();
    if (e == ERROR_HANDLE_EOF) {
      return BL_SUCCESS;
    }
    return bl_make_error(bl_result_from_win_error(e));
  }
  else {
    return BL_SUCCESS;
  }
}

BL_API_IMPL BLResult bl_file_write(BLFileCore* self, const void* buffer, size_t n, size_t* bytes_written_out) noexcept {
  using namespace bl;

  *bytes_written_out = 0;
  if (!FileSystemDetail::is_file_open(self)) {
    return bl_make_error(BL_ERROR_INVALID_HANDLE);
  }

  HANDLE handle = (HANDLE)self->handle;
  BOOL result = true;

  size_t remaining_size = n;
  size_t bytes_written_total = 0;

  while (remaining_size) {
    DWORD local_size = static_cast<DWORD>(axl::min<size_t>(remaining_size, kFileBufferSize));
    DWORD bytes_written = 0;

    result = WriteFile(handle, axl::offset_ptr(buffer, bytes_written_total), local_size, &bytes_written, nullptr);
    remaining_size -= local_size;
    bytes_written_total += bytes_written;

    if (bytes_written < local_size || !result) {
      break;
    }
  }

  *bytes_written_out = bytes_written_total;
  if (!result) {
    return bl_make_error(bl_result_from_win_error(GetLastError()));
  }

  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_file_truncate(BLFileCore* self, int64_t max_size) noexcept {
  using namespace bl;

  if (!FileSystemDetail::is_file_open(self)) {
    return bl_make_error(BL_ERROR_INVALID_HANDLE);
  }

  if (BL_UNLIKELY(max_size < 0)) {
    return bl_make_error(BL_ERROR_INVALID_VALUE);
  }

  uint64_t current_size;
  BL_PROPAGATE(bl_file_get_size(self, &current_size));
  if (current_size <= uint64_t(max_size)) {
    return BL_SUCCESS;
  }

  FILE_END_OF_FILE_INFO info {};
  info.EndOfFile.QuadPart = max_size;

  HANDLE handle = (HANDLE)self->handle;
  if (!SetFileInformationByHandle(handle, FileEndOfFileInfo, &info, sizeof(FILE_END_OF_FILE_INFO))) {
    return bl_make_error(bl_result_from_win_error(GetLastError()));
  }

  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_file_get_info(BLFileCore* self, BLFileInfo* info_out) noexcept {
  using namespace bl;

  *info_out = BLFileInfo{};
  if (!FileSystemDetail::is_file_open(self)) {
    return bl_make_error(BL_ERROR_INVALID_HANDLE);
  }

  HANDLE handle = (HANDLE)self->handle;
  BY_HANDLE_FILE_INFORMATION fi;

  if (!GetFileInformationByHandle(handle, &fi)) {
    return bl_make_error(bl_result_from_win_error(GetLastError()));
  }

  info_out->size = FileSystemDetail::combine_hi_lo(fi.nFileSizeHigh, fi.nFileSizeLow);
  info_out->modified_time = FileSystemDetail::unix_micro_from_file_time(fi.ftLastWriteTime);
  info_out->permissions = FileSystemDetail::file_permissions_from_win_file_attributes(fi.dwFileAttributes);
  info_out->flags = FileSystemDetail::file_flags_from_win_file_attributes(fi.dwFileAttributes);

  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_file_get_size(BLFileCore* self, uint64_t* file_size_out) noexcept {
  using namespace bl;

  *file_size_out = 0;
  if (!FileSystemDetail::is_file_open(self)) {
    return bl_make_error(BL_ERROR_INVALID_HANDLE);
  }

  HANDLE handle = (HANDLE)self->handle;
  LARGE_INTEGER size;

  if (!GetFileSizeEx(handle, &size)) {
    return bl_make_error(bl_result_from_win_error(GetLastError()));
  }

  *file_size_out = uint64_t(size.QuadPart);
  return BL_SUCCESS;
}

// FileSystem - DirectoryIterator - Windows Implementation
// =======================================================

namespace bl::FileSystemDetail {

struct DirIteratorData {
  HANDLE handle;
  WIN32_FIND_DATAW find_data;
  uint32_t entry_pending;
  uint32_t entry_name_w_len;
  char entry_name_utf8[MAX_PATH * 3 + 1];
};

} // {bl::FileSystemDetail}

BL_API_IMPL BLResult bl_directory_iterator_init(BLDirectoryIteratorCore* self) noexcept {
  using namespace bl;

  self->handle = nullptr;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_directory_iterator_reset(BLDirectoryIteratorCore* self) noexcept {
  using namespace bl;

  return bl_directory_iterator_close(self);
}

BL_API_IMPL BLResult bl_directory_iterator_open(BLDirectoryIteratorCore* self, const char* dir_path) noexcept {
  using namespace bl;

  size_t dir_path_size = strlen(dir_path);

  FileSystemDetail::Utf16PathTmp dir_path_w;
  BL_PROPAGATE(FileSystemDetail::convert_path_to_utf16(dir_path_w, {dir_path, dir_path_size}));

  if (dir_path_w.data()[dir_path_w.size() - 1u] != '\\') {
    dir_path_w.append_char(uint16_t('\\'));
  }

  dir_path_w.append_char(uint16_t('*'));

  WIN32_FIND_DATAW fd;
  HANDLE h = FindFirstFileW(dir_path_w.data_as_wchar(), &fd);

  if (h == INVALID_HANDLE_VALUE) {
    return bl_make_error(bl_result_from_win_error(GetLastError()));
  }

  FileSystemDetail::DirIteratorData* d = static_cast<FileSystemDetail::DirIteratorData*>(self->handle);
  if (d != nullptr) {
    FindClose(d->handle);
  }
  else {
    d = axl::mem_alloc<FileSystemDetail::DirIteratorData>(sizeof(FileSystemDetail::DirIteratorData));
    if (d == nullptr) {
      FindClose(h);
      return bl_make_error(BL_ERROR_OUT_OF_MEMORY);
    }
    self->handle = d;
  }

  d->handle = h;
  size_t entry_name_w_len = axl::str_nlen(fd.cFileName, MAX_PATH);

  d->entry_pending = 1;
  d->entry_name_w_len = static_cast<uint32_t>(entry_name_w_len);
  memcpy(&d->find_data, &fd, BL_OFFSET_OF(WIN32_FIND_DATAW, cFileName) + (entry_name_w_len + 1u) * 2u);

  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_directory_iterator_close(BLDirectoryIteratorCore* self) noexcept {
  using namespace bl;

  FileSystemDetail::DirIteratorData* d = static_cast<FileSystemDetail::DirIteratorData*>(self->handle);
  if (d == nullptr) {
    return BL_SUCCESS;
  }

  self->handle = nullptr;
  BOOL ok = FindClose(d->handle);
  BLResult result = BL_SUCCESS;

  if (!ok) {
    result = bl_make_error(bl_result_from_win_error(GetLastError()));
  }

  axl::mem_free(d);
  return result;
}

BL_API_IMPL bool bl_directory_iterator_is_open(const BLDirectoryIteratorCore* self) noexcept {
  using namespace bl;
  return (self->handle != nullptr);
}

BL_API_IMPL BLResult bl_directory_iterator_next(BLDirectoryIteratorCore* self, BLStringView* name_out, BLFileInfo* info_out) noexcept {
  using namespace bl;

  *name_out = BLStringView{};
  if (info_out) {
    *info_out = BLFileInfo{};
  }

  FileSystemDetail::DirIteratorData* d = static_cast<FileSystemDetail::DirIteratorData*>(self->handle);
  if (BL_UNLIKELY(d == nullptr)) {
    return bl_make_error(BL_ERROR_INVALID_HANDLE);
  }

  // Loop so we can skip "", ".", and ".." entries.
  for (;;) {
    size_t entry_name_w_len = d->entry_name_w_len;
    if (!d->entry_pending) {
      if (!FindNextFileW(d->handle, &d->find_data)) {
        DWORD err = GetLastError();
        if (err == ERROR_NO_MORE_FILES) {
          // Not using `bl_make_error()` for the expected end of iteration.
          return BL_ERROR_NO_MORE_FILES;
        }
        return bl_make_error(bl_result_from_win_error(err));
      }

      entry_name_w_len = axl::str_nlen(d->find_data.cFileName, MAX_PATH);
    }

    d->entry_pending = 0;
    d->entry_name_w_len = static_cast<uint32_t>(entry_name_w_len);

    const wchar_t* entry_name_w = d->find_data.cFileName;
    if (entry_name_w_len == 0 ||
        (entry_name_w[0] == L'.' && (entry_name_w_len == 1 || (entry_name_w_len == 2 && entry_name_w[1] == L'.')))) {
      continue;
    }

    // Encode the file-name from UTF-16 to UTF-8, which is what Blend2D uses.
    Unicode::ConversionState conversion_state;
    BL_PROPAGATE(Unicode::convert_unicode(
      d->entry_name_utf8, sizeof(d->entry_name_utf8) - 1u, BL_TEXT_ENCODING_UTF8,
      entry_name_w, entry_name_w_len * 2u, BL_TEXT_ENCODING_UTF16,
      conversion_state));

    d->entry_name_utf8[conversion_state.dst_index] = '\0';
    *name_out = BLStringView{d->entry_name_utf8, conversion_state.dst_index};

    if (info_out) {
      const WIN32_FIND_DATAW& fd = d->find_data;
      info_out->size = FileSystemDetail::combine_hi_lo(fd.nFileSizeHigh, fd.nFileSizeLow);
      info_out->modified_time = FileSystemDetail::unix_micro_from_file_time(fd.ftLastWriteTime);
      info_out->permissions = FileSystemDetail::file_permissions_from_win_file_attributes(fd.dwFileAttributes);
      info_out->flags = FileSystemDetail::file_flags_from_win_file_attributes(fd.dwFileAttributes);
    }

    return BL_SUCCESS;
  }
}

// FileSystem - Utilities - Windows Implementation
// ===============================================

BL_API_IMPL BLResult BL_CDECL bl_file_system_get_file_info(const char* file_name, BLFileInfo* info_out) noexcept {
  using namespace bl;

  *info_out = BLFileInfo{};

  size_t file_name_len = strlen(file_name);
  FileSystemDetail::Utf16PathTmp<> file_name_w;
  BL_PROPAGATE(file_name_w.init_capacity(file_name_len + 8u));
  BL_PROPAGATE(file_name_w.append_utf8({file_name, file_name_len}));

  WIN32_FILE_ATTRIBUTE_DATA fa;
  if (!GetFileAttributesExW(file_name_w.data_as_wchar(), GetFileExInfoStandard, &fa)) {
    return bl_make_error(bl_result_from_win_error(GetLastError()));
  }

  return FileSystemDetail::file_info_from_win_file_attribute_data(*info_out, fa);
}

// FileSystem - FileMapping - Windows Implementation
// =================================================

BLResult BLFileMapping::map(BLFile& file, size_t size, uint32_t flags) noexcept {
  using namespace bl;
  axl::maybe_unused(flags);

  if (!file.is_open()) {
    return bl_make_error(BL_ERROR_INVALID_VALUE);
  }

  DWORD map_protect = PAGE_READONLY;
  DWORD desired_access = FILE_MAP_READ;

  // Create a file mapping handle and map view of file into it.
#if defined(BL_PLATFORM_UWP)
  HANDLE file_mapping_handle = CreateFileMappingFromApp(
    (HANDLE)file.handle, // FileHandle
    nullptr,             // SecurityAttributes
    map_protect,         // PageProtection
    0,                   // MaximumSize
    nullptr              // Name
  );
#else
  HANDLE file_mapping_handle = CreateFileMappingW(
    (HANDLE)file.handle, // FileHandle
    nullptr,             // FileMappingAttributes
    map_protect,         // PageProtection
    0,                   // MaximumSizeHigh
    0,                   // MaximumSizeLow
    nullptr              // Name
  );
#endif

  if (file_mapping_handle == nullptr)
    return bl_make_error(bl_result_from_win_error(GetLastError()));

#if defined(BL_PLATFORM_UWP)
  void* data = MapViewOfFileFromApp(
    file_mapping_handle, // FileMappingHandle
    desired_access,      // DesiredAccess
    0,                   // FileOffset
    size                 // NumberOfBytesToMap
  );
#else
  void* data = MapViewOfFile(
    file_mapping_handle, // FileMappingHandle
    desired_access,      // DesiredAccess
    0,                   // FileOffsetHigh
    0,                   // FileOffsetLow
    size                 // NumberOfBytesToMap
  );
#endif

  if (!data) {
    BLResult result = bl_result_from_win_error(GetLastError());
    CloseHandle(file_mapping_handle);
    return bl_make_error(result);
  }

  // Succeeded, now is the time to change the content of `FileMapping`.
  unmap();

  _file_mapping_handle = file_mapping_handle;
  _data = data;
  _size = size;

  return BL_SUCCESS;
}

BLResult BLFileMapping::unmap() noexcept {
  if (is_empty())
    return BL_SUCCESS;

  BLResult result = BL_SUCCESS;
  DWORD err = 0;

  if (!UnmapViewOfFile(_data)) {
    err = GetLastError();
  }

  if (!CloseHandle(_file_mapping_handle) && !err) {
    err = GetLastError();
  }

  if (err) {
    result = bl_make_error(bl_result_from_win_error(err));
  }

  _file_mapping_handle = INVALID_HANDLE_VALUE;
  _data = nullptr;
  _size = 0;

  return result;
}

#else

// FileSystem - File - POSIX Implementation
// ========================================

namespace bl::FileSystemDetail {

template<uint32_t kDst, uint32_t kSrc, uint32_t kMsk = 0x1>
static BL_INLINE_NODEBUG uint32_t translate_permissions_flags(uint32_t src) noexcept {
  constexpr uint32_t kDstOffset = axl::ctz_const(kDst);
  constexpr uint32_t kSrcOffset = axl::ctz_const(kSrc);

  if constexpr (kDstOffset < kSrcOffset) {
    return (src >> (kSrcOffset - kDstOffset)) & (kMsk << kDstOffset);
  }
  else {
    return (src << (kDstOffset - kSrcOffset)) & (kMsk << kDstOffset);
  }
}

template<BLFilePermissions kDstX, uint32_t kSrcR, uint32_t kSrcW, uint32_t kSrcX>
static BL_INLINE_NODEBUG uint32_t translate_permissions_rwx(uint32_t src) noexcept {
  if constexpr (kSrcW == (kSrcX << 1) && kSrcR == (kSrcX << 2)) {
    return translate_permissions_flags<kDstX, kSrcX, 0x7>(src);
  }
  else {
    return translate_permissions_flags<kDstX << 0, kSrcX>(src) |
           translate_permissions_flags<kDstX << 1, kSrcW>(src) |
           translate_permissions_flags<kDstX << 2, kSrcR>(src) ;
  }
}

template<typename T>
static BL_INLINE int64_t unix_micro_from_file_time(const T& t) noexcept {
  return int64_t(uint64_t(t) * 1000000);
}

#if defined(BL_FS_STAT_MTIMESPEC)
template<typename T>
static BL_INLINE int64_t unix_micro_from_time_spec(const T& ts) noexcept {
  return int64_t(uint64_t(ts.tv_sec) * 1000000u + (uint32_t(ts.tv_nsec) / 1000u));
}
#endif

static BLResult file_info_from_stat(BLFileInfo& info, struct stat& s) noexcept {
  uint32_t perm = 0u;
  uint32_t flags = BL_FILE_INFO_VALID;

  // Translate file type to a portable representation:
  if (S_ISREG(s.st_mode)) flags |= BL_FILE_INFO_REGULAR;
  if (S_ISDIR(s.st_mode)) flags |= BL_FILE_INFO_DIRECTORY;

#if defined(S_ISLNK)
  if (S_ISLNK(s.st_mode)) flags |= BL_FILE_INFO_SYMLINK;
#endif // S_ISLNK

#if defined(S_ISCHR)
  if (S_ISCHR(s.st_mode)) flags |= BL_FILE_INFO_CHAR_DEVICE;
#endif // S_ISCHR

#if defined(S_ISBLK)
  if (S_ISBLK(s.st_mode)) flags |= BL_FILE_INFO_BLOCK_DEVICE;
#endif // S_ISBLK

#if defined(S_ISFIFO)
  if (S_ISFIFO(s.st_mode)) flags |= BL_FILE_INFO_FIFO;
#endif // S_ISFIFO

#if defined(S_ISSOCK)
  if (S_ISSOCK(s.st_mode)) flags |= BL_FILE_INFO_SOCKET;
#endif // S_ISSOCK

  // Translate file permissions to a portable representation:
  perm |= translate_permissions_rwx<BL_FILE_PERMISSION_OWNER_X, S_IRUSR, S_IWUSR, S_IXUSR>(s.st_mode);
  perm |= translate_permissions_rwx<BL_FILE_PERMISSION_GROUP_X, S_IRGRP, S_IWGRP, S_IXGRP>(s.st_mode);
  perm |= translate_permissions_rwx<BL_FILE_PERMISSION_OTHER_X, S_IROTH, S_IWOTH, S_IXOTH>(s.st_mode);
  perm |= translate_permissions_flags<BL_FILE_PERMISSION_SUID, S_ISUID>(s.st_mode);
  perm |= translate_permissions_flags<BL_FILE_PERMISSION_SGID, S_ISGID>(s.st_mode);

  info = BLFileInfo{};

  if (flags & BL_FILE_INFO_REGULAR) {
    info.size = uint64_t(s.st_size);
  }

  info.permissions = static_cast<BLFilePermissions>(perm);
  info.flags = static_cast<BLFileInfoFlags>(flags);
  info.uid = static_cast<uint32_t>(s.st_uid);
  info.gid = static_cast<uint32_t>(s.st_gid);

#if defined(BL_FS_STAT_MTIMESPEC)
  info.modified_time = unix_micro_from_time_spec(BL_FS_STAT_MTIMESPEC(s));
#else
  info.modified_time = unix_micro_from_file_time(s.st_mtime);
#endif

  return BL_SUCCESS;
}

} // {bl::FileSystemDetail} namespace

// BLFile - API - POSIX Implementation
// ===================================

BL_API_IMPL BLResult bl_file_open(BLFileCore* self, const char* file_name, BLFileOpenFlags open_flags) noexcept {
  using namespace bl;

  int of = O_CLOEXEC;
  switch (open_flags & BL_FILE_OPEN_RW) {
    case BL_FILE_OPEN_READ : of |= O_RDONLY; break;
    case BL_FILE_OPEN_WRITE: of |= O_WRONLY; break;
    case BL_FILE_OPEN_RW   : of |= O_RDWR  ; break;

    default:
      return bl_make_error(BL_ERROR_INVALID_VALUE);
  }

  uint32_t kExtFlags = BL_FILE_OPEN_CREATE | BL_FILE_OPEN_CREATE_EXCLUSIVE | BL_FILE_OPEN_TRUNCATE;

  if ((open_flags & kExtFlags) && !(open_flags & BL_FILE_OPEN_WRITE)) {
    return bl_make_error(BL_ERROR_INVALID_VALUE);
  }

  if (open_flags & BL_FILE_OPEN_CREATE          ) of |= O_CREAT;
  if (open_flags & BL_FILE_OPEN_CREATE_EXCLUSIVE) of |= O_CREAT | O_EXCL;
  if (open_flags & BL_FILE_OPEN_TRUNCATE        ) of |= O_TRUNC;

  mode_t om = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;

  // NOTE: Do not close the file before calling `open()`. We should behave atomically, which means that we won't
  // close the existing file if `open()` fails...
  int fd = open(file_name, of, om);
  if (fd < 0) {
    return bl_make_error(bl_result_from_posix_error(errno));
  }

  bl_file_close(self);
  self->handle = intptr_t(fd);

  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_file_close(BLFileCore* self) noexcept {
  using namespace bl;

  if (FileSystemDetail::is_file_open(self)) {
    int fd = int(self->handle);
    int result = close(fd);

    // NOTE: Even when `close()` fails the handle cannot be used again as it could have already been reused. The
    // failure is just to inform the user that something failed and that there may be data-loss or handle leakage.
    self->handle = -1;

    if (BL_UNLIKELY(result != 0)) {
      return bl_make_error(bl_result_from_posix_error(errno));
    }
  }

  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_file_seek(BLFileCore* self, int64_t offset, BLFileSeekType seek_type, int64_t* position_out) noexcept {
  using namespace bl;

  *position_out = -1;
  int whence = 0;

  switch (seek_type) {
    case BL_FILE_SEEK_SET: whence = SEEK_SET; break;
    case BL_FILE_SEEK_CUR: whence = SEEK_CUR; break;
    case BL_FILE_SEEK_END: whence = SEEK_END; break;

    default:
      return bl_make_error(BL_ERROR_INVALID_VALUE);
  }

  if (!FileSystemDetail::is_file_open(self)) {
    return bl_make_error(BL_ERROR_INVALID_HANDLE);
  }

  int fd = int(self->handle);
  int64_t result = lseek(fd, offset, whence);

  if (result < 0) {
    int e = errno;

    // Returned when the file was not open for reading or writing.
    if (e == EBADF) {
      return bl_make_error(BL_ERROR_NOT_PERMITTED);
    }

    return bl_make_error(bl_result_from_posix_error(errno));
  }

  *position_out = result;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_file_read(BLFileCore* self, void* buffer, size_t n, size_t* bytes_read_out) noexcept {
  using namespace bl;
  using SignedSizeT = std::make_signed_t<size_t>;

  if (!FileSystemDetail::is_file_open(self)) {
    *bytes_read_out = 0;
    return bl_make_error(BL_ERROR_INVALID_HANDLE);
  }

  int fd = int(self->handle);
  size_t bytes_read_total = 0;

  for (;;) {
    SignedSizeT result = read(fd, axl::offset_ptr(buffer, bytes_read_total), n - bytes_read_total);
    if (result < 0) {
      int e = errno;
      *bytes_read_out = bytes_read_total;

      // Returned when the file was not open for reading.
      if (e == EBADF) {
        return bl_make_error(BL_ERROR_NOT_PERMITTED);
      }

      return bl_make_error(bl_result_from_posix_error(e));
    }
    else {
      bytes_read_total += static_cast<size_t>(result);
      if (bytes_read_total == n || result == 0) {
        break;
      }
    }
  }

  *bytes_read_out = bytes_read_total;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_file_write(BLFileCore* self, const void* buffer, size_t n, size_t* bytes_written_out) noexcept {
  using namespace bl;
  using SignedSizeT = std::make_signed_t<size_t>;

  if (!FileSystemDetail::is_file_open(self)) {
    *bytes_written_out = 0;
    return bl_make_error(BL_ERROR_INVALID_HANDLE);
  }

  int fd = int(self->handle);
  size_t bytes_written_total = 0;

  for (;;) {
    SignedSizeT result = write(fd, axl::offset_ptr(buffer, bytes_written_total), n - bytes_written_total);
    if (result < 0) {
      int e = errno;
      *bytes_written_out = bytes_written_total;

      // These are the two errors that would be returned if the file was open for read-only.
      if (e == EBADF || e == EINVAL) {
        return bl_make_error(BL_ERROR_NOT_PERMITTED);
      }

      return bl_make_error(bl_result_from_posix_error(e));
    }
    else {
      bytes_written_total += static_cast<size_t>(result);
      if (bytes_written_total == n || result == 0) {
        break;
      }
    }
  }

  *bytes_written_out = bytes_written_total;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_file_truncate(BLFileCore* self, int64_t max_size) noexcept {
  using namespace bl;

  if (!FileSystemDetail::is_file_open(self)) {
    return bl_make_error(BL_ERROR_INVALID_HANDLE);
  }

  if (max_size < 0) {
    return bl_make_error(BL_ERROR_INVALID_VALUE);
  }

  uint64_t current_size;
  BL_PROPAGATE(bl_file_get_size(self, &current_size));
  if (current_size <= uint64_t(max_size)) {
    return BL_SUCCESS;
  }

  int fd = int(self->handle);
  int result = ftruncate(fd, max_size);

  if (result != 0) {
    int e = errno;

    // These are the two errors that would be returned if the file was open for read-only.
    if (e == EBADF || e == EINVAL) {
      return bl_make_error(BL_ERROR_NOT_PERMITTED);
    }

    return bl_make_error(bl_result_from_posix_error(e));
  }
  else {
    return BL_SUCCESS;
  }
}

BL_API_IMPL BLResult bl_file_get_info(BLFileCore* self, BLFileInfo* info_out) noexcept {
  using namespace bl;

  if (!FileSystemDetail::is_file_open(self)) {
    *info_out = BLFileInfo{};
    return bl_make_error(BL_ERROR_INVALID_HANDLE);
  }

  int fd = int(self->handle);
  struct stat s;

  if (fstat(fd, &s) != 0) {
    *info_out = BLFileInfo{};
    return bl_make_error(bl_result_from_posix_error(errno));
  }

  return FileSystemDetail::file_info_from_stat(*info_out, s);
}

BL_API_IMPL BLResult bl_file_get_size(BLFileCore* self, uint64_t* file_size_out) noexcept {
  using namespace bl;

  if (!FileSystemDetail::is_file_open(self)) {
    *file_size_out = 0;
    return bl_make_error(BL_ERROR_INVALID_HANDLE);
  }

  int fd = int(self->handle);
  struct stat s;

  if (fstat(fd, &s) != 0) {
    *file_size_out = 0;
    return bl_make_error(bl_result_from_posix_error(errno));
  }

  *file_size_out = uint64_t(s.st_size);
  return BL_SUCCESS;
}

// FileSystem - DirectoryIterator - POSIX Implementation
// =====================================================

BL_API_IMPL BLResult bl_directory_iterator_init(BLDirectoryIteratorCore* self) noexcept {
  using namespace bl;

  self->handle = nullptr;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_directory_iterator_reset(BLDirectoryIteratorCore* self) noexcept {
  using namespace bl;

  return bl_directory_iterator_close(self);
}

BL_API_IMPL BLResult bl_directory_iterator_open(BLDirectoryIteratorCore* self, const char* dir_path) noexcept {
  using namespace bl;

  DIR* dir = opendir(dir_path);
  if (dir == nullptr) {
    return bl_result_from_posix_error(errno);
  }

  bl_directory_iterator_close(self);
  self->handle = static_cast<void*>(dir);
  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_directory_iterator_close(BLDirectoryIteratorCore* self) noexcept {
  using namespace bl;

  DIR* handle = static_cast<DIR*>(self->handle);
  if (handle != nullptr) {
    self->handle = nullptr;
    if (::closedir(handle) != 0) {
      return bl_result_from_posix_error(errno);
    }
  }

  return BL_SUCCESS;
}

BL_API_IMPL bool bl_directory_iterator_is_open(const BLDirectoryIteratorCore* self) noexcept {
  using namespace bl;
  return (self->handle != nullptr);
}

BL_API_IMPL BLResult bl_directory_iterator_next(BLDirectoryIteratorCore* self, BLStringView* name_out, BLFileInfo* info_out) noexcept {
  using namespace bl;

  DIR* handle = static_cast<DIR*>(self->handle);
  *name_out = BLStringView{};

  if (info_out) {
    *info_out = BLFileInfo{};
  }

  if (BL_UNLIKELY(handle == nullptr)) {
    return bl_make_error(BL_ERROR_INVALID_HANDLE);
  }

  // Loop so we can skip "", ".", and ".." entries.
  for (;;) {
    errno = 0;
    struct dirent* de = readdir(handle);

    if (de == nullptr) {
      int err = errno;
      if (err == 0) {
        // Not using `bl_make_error()` for the expected end of iteration.
        return BL_ERROR_NO_MORE_FILES;
      }
      return bl_make_error(bl_result_from_posix_error(err));
    }

    size_t d_name_len = strlen(de->d_name);
    if (d_name_len <= 2) {
      // Skip entries without a name - this seems to be underspecified what these should be.
      if (d_name_len == 0) {
        continue;
      }

      // Skip '.' and '..' entries.
      if (de->d_name[0] == '.') {
        if (d_name_len == 1) {
          continue;
        }

        if (de->d_name[1] == '.') {
          continue;
        }
      }
    }

    *name_out = BLStringView{de->d_name, d_name_len};
    if (info_out == nullptr) {
      return BL_SUCCESS;
    }
    else {
      struct stat s;
      if (fstatat(dirfd(handle), de->d_name, &s, 0) != 0) {
        *info_out = BLFileInfo{};
        return bl_result_from_posix_error(errno);
      }
      return FileSystemDetail::file_info_from_stat(*info_out, s);
    }
  }
}

// FileSystem - Utilities - POSIX Implementation
// =============================================

BL_API_IMPL BLResult bl_file_system_get_file_info(const char* file_name, BLFileInfo* info_out) noexcept {
  using namespace bl;

  struct stat s;
  if (stat(file_name, &s) != 0) {
    *info_out = BLFileInfo{};
    return bl_make_error(bl_result_from_posix_error(errno));
  }

  return FileSystemDetail::file_info_from_stat(*info_out, s);
}

// FileMapping - File Mapping - POSIX Implementation
// =================================================

BLResult BLFileMapping::map(BLFile& file, size_t size, uint32_t flags) noexcept {
  bl::axl::maybe_unused(flags);

  if (!file.is_open())
    return bl_make_error(BL_ERROR_INVALID_VALUE);

  int mmap_prot = PROT_READ;
  int mmap_flags = MAP_SHARED;

  // Create the mapping.
  void* data = mmap(nullptr, size, mmap_prot, mmap_flags, int(file.handle), 0);
  if (data == (void *)-1) {
    return bl_make_error(bl_result_from_posix_error(errno));
  }

  // Succeeded, now is the time to change the content of `BLFileMapping`.
  unmap();

  _data = data;
  _size = size;

  return BL_SUCCESS;
}

BLResult BLFileMapping::unmap() noexcept {
  if (is_empty()) {
    return BL_SUCCESS;
  }

  BLResult result = BL_SUCCESS;
  int unmap_status = munmap(_data, _size);

  // If error happened we must read `errno` now as a call to `close()` may
  // trash it. We prefer the first error instead of the last one.
  if (unmap_status != 0) {
    result = bl_make_error(bl_result_from_posix_error(errno));
  }

  _data = nullptr;
  _size = 0;

  return result;
}

#endif

// FileSystem - Utilities - Memory Mapped File
// ===========================================

namespace bl {

struct MemoryMappedArrayImpl : public BLArrayImpl {
  BLFileMapping _file_mapping;
};

static void BL_CDECL destroy_memory_mapped_file(BLObjectCore* object, void* external_data, void* user_data) noexcept {
  axl::maybe_unused(external_data, user_data);

  MemoryMappedArrayImpl* impl = object->_d.impl<MemoryMappedArrayImpl>();
  bl_call_dtor(impl->_file_mapping);
}

static BLResult create_memory_mapped_file(BLArray<uint8_t>* dst, BLFile& file, size_t size) noexcept {
  // This condition must be handled before.
  BL_ASSERT(size != 0);

  BLFileMapping file_mapping;
  BL_PROPAGATE(file_mapping.map(file, size));

  BLArrayCore mapped_array;
  ObjectImplSize impl_size = ObjectImplSize{sizeof(MemoryMappedArrayImpl)};

  BL_PROPAGATE(ObjectLayer::alloc_external_impl(&mapped_array, impl_size, ObjectLayer::AllocExternalImplParams{
    .object_type    = BL_OBJECT_TYPE_ARRAY_UINT8,
    .access_mode    = BL_DATA_ACCESS_MODE_RO,
    .external_data  = file_mapping.data(),
    .size           = file_mapping.size(),
    .capacity       = file_mapping.size(),
    .destroy_func   = destroy_memory_mapped_file,
    .user_data      = nullptr
  }));

  bl_call_ctor(mapped_array._d.impl<MemoryMappedArrayImpl>()->_file_mapping, axl::move(file_mapping));
  return ObjectLayer::replace_object<BLArrayCore>(dst, &mapped_array);
}

} // {bl}

// FileSystem - Utilities - Read/Write File
// ========================================

static constexpr uint32_t kSmallFileSizeThreshold = 16 * 1024;

#if !defined(_WIN32)
namespace bl::FileSystemDetail {

static BLResult read_maybe_vfs_file(BLFile& file, BLArray<uint8_t>& dst, size_t max_size) noexcept {
  // Should be guaranteed here.
  BL_ASSERT(ObjectLayer::is_object_mutable(&dst));

  // We want a local buffer smaller than 4KiB so the compiler doesn't have to generate stack probes.
  constexpr size_t kLocalBufferCapacity =  1024u * 3u + 512u;
  constexpr size_t kInitialDynamicCapacity = 1024u * 32u;

  uint8_t local_buffer[kLocalBufferCapacity];

  if (!max_size) {
    max_size = SIZE_MAX;
  }

  size_t read_size = kLocalBufferCapacity;
  if (max_size < kLocalBufferCapacity) {
    read_size = max_size;
  }

  uint8_t* dst_buffer;
  size_t dst_capacity;
  bool local_buffer_in_use = true;

  if (dst.capacity() >= read_size) {
    dst_buffer = ObjectLayer::get_data<uint8_t>(&dst);
    dst_capacity = dst.capacity();
    local_buffer_in_use = false;
  }
  else {
    dst_buffer = local_buffer;
    dst_capacity = kLocalBufferCapacity;
  }

  size_t dst_byte_index = 0u;
  for (;;) {
    size_t bytes_read;
    BL_PROPAGATE(file.read(dst_buffer + dst_byte_index, read_size, &bytes_read));

    // file.read() is already an abstraction - if the returned `bytes_read` is smaller than `read_size`, we
    // have reached end-of-file (EOF) and can just terminate.
    dst_byte_index += bytes_read;
    if (bytes_read < read_size || dst_byte_index == max_size) {
      break;
    }

    size_t remaining_size = max_size - dst_byte_index;
    read_size = axl::min(remaining_size, dst_capacity - dst_byte_index);

    if (read_size == 0) {
      // Need to reallocate the buffer - if the buffer is local, we just need to reserve enough space
      // in dst so we can read bigger bulks of data.
      if (local_buffer_in_use) {
        // Estimate to 32KiB - we are reading most likely sysfs file or some other virtual file, and these
        // files are never huge.
        BL_PROPAGATE(dst.reserve(kInitialDynamicCapacity));

        // NOTE: This one should never fail after a successful `reserve()` - but we still want to propagate.
        BL_PROPAGATE(dst.append_data(dst_buffer, dst_byte_index));

        dst_buffer = ObjectLayer::dyn_data<uint8_t>(&dst);
        dst_capacity = dst.capacity();

        local_buffer_in_use = false;
      }
      else {
        // Large file?
        ObjectLayer::set_object_size(&dst, dst_byte_index);
        BL_PROPAGATE(dst.reserve_for_append(dst.capacity()));

        dst_buffer = ObjectLayer::dyn_data<uint8_t>(&dst);
        dst_capacity = ObjectLayer::dyn_capacity(&dst);
      }

      read_size = axl::min(remaining_size, dst_capacity - dst_byte_index);
    }
  }

  // If only local buffer was used, we need to assign it to `dst`.
  if (local_buffer_in_use) {
    return dst.assign_data(local_buffer, dst_byte_index);
  }
  else {
    ObjectLayer::set_object_size(&dst, dst_byte_index);
    return BL_SUCCESS;
  }
}

} // {bl::FileSystemDetail}
#endif

BL_API_IMPL BLResult bl_file_system_read_file(const char* file_name, BLArrayCore* dst_, size_t max_size, BLFileReadFlags read_flags) noexcept {
  using namespace bl;

  if (BL_UNLIKELY(!dst_->_d.is_array_u8())) {
    return bl_make_error(BL_ERROR_INVALID_STATE);
  }

  BLArray<uint8_t>& dst = dst_->dcast<BLArray<uint8_t>>();

  // Also makes sure that the destination is mutable.
  dst.clear();

  BLFile file;
  BL_PROPAGATE(file.open(file_name, BL_FILE_OPEN_READ));

  uint64_t size64;
  BL_PROPAGATE(file.get_size(&size64));

  if (size64 == 0) {
#if !defined(_WIN32)
    return FileSystemDetail::read_maybe_vfs_file(file, dst, max_size);
#else
    return BL_SUCCESS;
#endif
  }

  if (max_size) {
    size64 = axl::min<uint64_t>(size64, max_size);
  }

  if (bl_runtime_is_32bit() && BL_UNLIKELY(size64 >= uint64_t(SIZE_MAX))) {
    return bl_make_error(BL_ERROR_FILE_TOO_LARGE);
  }

  size_t size = size_t(size64);

  // Use memory mapped file IO if enabled.
  if (read_flags & BL_FILE_READ_MMAP_ENABLED) {
    bool is_small = size < kSmallFileSizeThreshold;
    if (!(read_flags & BL_FILE_READ_MMAP_AVOID_SMALL) || !is_small) {
      BLResult result = create_memory_mapped_file(&dst, file, size);
      if (result == BL_SUCCESS)
        return result;

      if (read_flags & BL_FILE_READ_MMAP_NO_FALLBACK)
        return result;
    }
  }

  uint8_t* data;
  BL_PROPAGATE(dst.assign_uninitialized(size, &data));

  size_t bytes_read;
  BLResult result = file.read(data, size, &bytes_read);
  dst.resize(bytes_read, 0);
  return result;
}

BL_API_IMPL BLResult bl_file_system_write_file(const char* file_name, const void* data, size_t size, size_t* bytes_written_out) noexcept {
  using namespace bl;

  *bytes_written_out = 0;

  BLFile file;
  BL_PROPAGATE(file.open(file_name, BLFileOpenFlags(BL_FILE_OPEN_WRITE | BL_FILE_OPEN_CREATE | BL_FILE_OPEN_TRUNCATE)));
  return size ? file.write(data, size , bytes_written_out) : BL_SUCCESS;
}

#endif // !BL_BUILD_NO_FILESYSTEM
