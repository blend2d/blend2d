// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_p.h"
#include "array_p.h"
#include "filesystem_p.h"
#include "runtime_p.h"
#include "support/ptrops_p.h"
#include "threading/atomic_p.h"
#include "unicode/unicode_p.h"

#if !defined(_WIN32)
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
#endif

#ifdef _WIN32

// BLUtf16StringTmp
// ================

static constexpr size_t kStaticUTF16StringSize = 1024;

template<size_t N>
class BLUtf16StringTmp {
public:
  uint16_t* _data;
  size_t _size;
  size_t _capacity;
  uint16_t _embeddedData[N + 1];

  BL_NONCOPYABLE(BLUtf16StringTmp);

  BL_INLINE BLUtf16StringTmp() noexcept
    : _data(_embeddedData),
      _size(0),
      _capacity(N) {}

  BL_INLINE ~BLUtf16StringTmp() noexcept {
    if (_data != _embeddedData)
      free(_data);
  }

  BL_INLINE_NODEBUG uint16_t* data() const noexcept { return _data; }
  BL_INLINE_NODEBUG wchar_t* dataAsWCharT() const noexcept { return reinterpret_cast<wchar_t*>(_data); }

  BL_INLINE_NODEBUG size_t size() const noexcept { return _size; }
  BL_INLINE_NODEBUG size_t capacity() const noexcept { return _capacity; }

  BL_INLINE_NODEBUG void nullTerminate() noexcept { _data[_size] = uint16_t(0); }

  BL_NOINLINE BLResult fromUtf8(const char* src) noexcept {
    size_t srcSize = strlen(src);
    bl::Unicode::ConversionState conversionState;

    BLResult result = bl::Unicode::convertUnicode(
      _data, _capacity * 2u, BL_TEXT_ENCODING_UTF16, src, srcSize, BL_TEXT_ENCODING_UTF8, conversionState);

    if (result == BL_SUCCESS) {
      _size = conversionState.dstIndex / 2u;
      nullTerminate();
      return result;
    }

    if (result != BL_ERROR_NO_SPACE_LEFT) {
      _size = 0;
      nullTerminate();
      return result;
    }

    size_t procUtf8Size = conversionState.srcIndex;
    size_t procUtf16Size = conversionState.dstIndex / 2u;

    bl::Unicode::ValidationState validationState;
    BL_PROPAGATE(blValidateUtf8(src + procUtf8Size, srcSize - procUtf8Size, validationState));

    size_t newSize = procUtf16Size + validationState.utf16Index;
    uint16_t* newData = static_cast<uint16_t*>(malloc((newSize + 1) * sizeof(uint16_t)));

    if (BL_UNLIKELY(!newData)) {
      _size = 0;
      nullTerminate();
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);
    }

    memcpy(newData, _data, procUtf16Size * sizeof(uint16_t));
    bl::Unicode::convertUnicode(
      newData + procUtf16Size, (newSize - procUtf16Size) * 2u, BL_TEXT_ENCODING_UTF16,
      src + procUtf8Size, srcSize - procUtf8Size, BL_TEXT_ENCODING_UTF8, conversionState);
    BL_ASSERT(newSize == procUtf16Size + conversionState.dstIndex * 2u);

    if (_data != _embeddedData)
      free(_data);

    _data = newData;
    _size = newSize;
    _capacity = newSize;

    nullTerminate();
    return BL_SUCCESS;
  }
};

#endif

// BLFile - Utilities
// ==================

// Just a helper to cast to `BLFile` and call its `isOpen()` as this is the only thing we need from C++ API here.
static BL_INLINE bool isFileOpen(const BLFileCore* self) noexcept {
  return static_cast<const BLFile*>(self)->isOpen();
}

// BLFile - API - Construction & Destruction
// =========================================

BL_API_IMPL BLResult blFileInit(BLFileCore* self) noexcept {
  self->handle = -1;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blFileReset(BLFileCore* self) noexcept {
  return blFileClose(self);
}

#ifdef _WIN32

// BLFileSystem - Windows Implementation (Internal)
// ================================================

namespace bl {
namespace FileSystem {

static BL_INLINE uint64_t combineHiLo(uint32_t hi, uint32_t lo) noexcept {
  return (uint64_t(hi) << 32) | lo;
}

static BL_INLINE uint64_t unixMicroFromFileTime(FILETIME ft) noexcept {
  constexpr uint64_t kFileTimeToUnixTimeS = 11644473600;
  constexpr uint32_t kMicrosecondsPerSecond = 1000000;

  // First convert to microseconds, starting from 1601-01-01 [UTC].
  uint64_t t = combineHiLo(ft.dwHighDateTime, ft.dwLowDateTime) / 10u;

  return int64_t(t) - int64_t(kFileTimeToUnixTimeS * kMicrosecondsPerSecond);
}

static BLFileInfoFlags fileFlagsFromDWFileAttributes(DWORD dwAttr) noexcept {
  uint32_t flags = BL_FILE_INFO_VALID;

  if (dwAttr & FILE_ATTRIBUTE_DIRECTORY)
    flags |= BL_FILE_INFO_DIRECTORY;
  else
    flags |= BL_FILE_INFO_REGULAR;

  if (dwAttr & FILE_ATTRIBUTE_REPARSE_POINT)
    flags |= BL_FILE_INFO_SYMLINK;

  if (dwAttr & FILE_ATTRIBUTE_DEVICE)
    flags |= BL_FILE_INFO_CHAR_DEVICE;

  if (dwAttr & FILE_ATTRIBUTE_HIDDEN)
    flags |= BL_FILE_INFO_HIDDEN;

  // Windows specific file attributes.
  if (dwAttr & FILE_ATTRIBUTE_ARCHIVE)
    flags |= BL_FILE_INFO_ARCHIVE;

  if (dwAttr & FILE_ATTRIBUTE_SYSTEM)
    flags |= BL_FILE_INFO_SYSTEM;

  // Windows specific file attributes.
  if (dwAttr & FILE_ATTRIBUTE_READONLY) {
    flags |= BL_FILE_INFO_OWNER_R |
             BL_FILE_INFO_GROUP_R |
             BL_FILE_INFO_OTHER_R;
  }
  else {
    flags |= BL_FILE_INFO_OWNER_R | BL_FILE_INFO_OWNER_W |
             BL_FILE_INFO_GROUP_R | BL_FILE_INFO_GROUP_W |
             BL_FILE_INFO_OTHER_R | BL_FILE_INFO_OTHER_W ;
  }

  return BLFileInfoFlags(flags);
}

static BLResult fileInfoFromFileAttributeData(BLFileInfo& info, const WIN32_FILE_ATTRIBUTE_DATA& fa) noexcept {
  info.flags = fileFlagsFromDWFileAttributes(fa.dwFileAttributes);
  info.size = combineHiLo(fa.nFileSizeHigh, fa.nFileSizeLow);
  info.modifiedTime = unixMicroFromFileTime(fa.ftLastWriteTime);

  return BL_SUCCESS;
}

} // {FileSystem} namespace
} // {bl} namespace

// BLFile - API - Windows Implementation
// =====================================

static const constexpr DWORD kFileBufferSize = 32 * 1024 * 1024; // 32 MB.

BL_API_IMPL BLResult blFileOpen(BLFileCore* self, const char* fileName, BLFileOpenFlags openFlags) noexcept {
  // Desired Access
  // --------------
  //
  // The same flags as O_RDONLY|O_WRONLY|O_RDWR:

  DWORD dwDesiredAccess = 0;
  switch (openFlags & BL_FILE_OPEN_RW) {
    case BL_FILE_OPEN_READ : dwDesiredAccess = GENERIC_READ ; break;
    case BL_FILE_OPEN_WRITE: dwDesiredAccess = GENERIC_WRITE; break;
    case BL_FILE_OPEN_RW   : dwDesiredAccess = GENERIC_READ | GENERIC_WRITE; break;
    default:
      return blTraceError(BL_ERROR_INVALID_VALUE);
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

  uint32_t kExtFlags = BL_FILE_OPEN_CREATE           |
                       BL_FILE_OPEN_CREATE_EXCLUSIVE |
                       BL_FILE_OPEN_TRUNCATE         ;

  if ((openFlags & kExtFlags) && (!(openFlags & BL_FILE_OPEN_WRITE)))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  DWORD dwCreationDisposition = OPEN_EXISTING;
  if (openFlags & BL_FILE_OPEN_CREATE_EXCLUSIVE)
    dwCreationDisposition = CREATE_NEW;
  else if ((openFlags & (BL_FILE_OPEN_CREATE | BL_FILE_OPEN_TRUNCATE)) == BL_FILE_OPEN_CREATE)
    dwCreationDisposition = OPEN_ALWAYS;
  else if ((openFlags & (BL_FILE_OPEN_CREATE | BL_FILE_OPEN_TRUNCATE)) == (BL_FILE_OPEN_CREATE | BL_FILE_OPEN_TRUNCATE))
    dwCreationDisposition = CREATE_ALWAYS;
  else if (openFlags & BL_FILE_OPEN_TRUNCATE)
    dwCreationDisposition = TRUNCATE_EXISTING;

  // Share Mode
  // ----------

  DWORD dwShareMode = 0;

  auto isShared = [&](uint32_t access, uint32_t exclusive) noexcept -> bool {
    return (openFlags & (access | exclusive)) == access;
  };

  if (isShared(BL_FILE_OPEN_READ, BL_FILE_OPEN_READ_EXCLUSIVE)) dwShareMode |= FILE_SHARE_READ;
  if (isShared(BL_FILE_OPEN_WRITE, BL_FILE_OPEN_WRITE_EXCLUSIVE)) dwShareMode |= FILE_SHARE_WRITE;
  if (isShared(BL_FILE_OPEN_DELETE, BL_FILE_OPEN_DELETE_EXCLUSIVE)) dwShareMode |= FILE_SHARE_DELETE;

  // WinAPI Call
  // -----------

  // NOTE: Do not close the file before calling `CreateFileW()`. We should behave atomically, which means that
  // we won't close the existing file if `CreateFileW()` fails...
  BLUtf16StringTmp<kStaticUTF16StringSize> fileNameW;
  BL_PROPAGATE(fileNameW.fromUtf8(fileName));

#if defined(BL_PLATFORM_UWP)
  // UWP platform doesn't provide CreateFileW, but we can use CreateFile2 instead.
  HANDLE handle = CreateFile2(
    fileNameW.dataAsWCharT(),
    dwDesiredAccess,
    dwShareMode,
    dwCreationDisposition,
    nullptr
  );
#else
  DWORD dwFlagsAndAttributes = 0;
  LPSECURITY_ATTRIBUTES lpSecurityAttributes = nullptr;

  HANDLE handle = CreateFileW(
    fileNameW.dataAsWCharT(),
    dwDesiredAccess,
    dwShareMode,
    lpSecurityAttributes,
    dwCreationDisposition,
    dwFlagsAndAttributes,
    nullptr
  );
#endif

  if (handle == INVALID_HANDLE_VALUE)
    return blTraceError(blResultFromWinError(GetLastError()));

  blFileClose(self);
  self->handle = intptr_t(handle);

  return BL_SUCCESS;
}

BL_API_IMPL BLResult blFileClose(BLFileCore* self) noexcept {
  // Not sure what should happen if `CloseHandle()` fails, if the handle is invalid or the close can be called
  // again? To ensure compatibility with POSIX implementation we just make it invalid.
  if (isFileOpen(self)) {
    HANDLE handle = (HANDLE)self->handle;
    BOOL result = CloseHandle(handle);

    self->handle = -1;
    if (!result)
      return blTraceError(blResultFromWinError(GetLastError()));
  }

  return BL_SUCCESS;
}

BL_API_IMPL BLResult blFileSeek(BLFileCore* self, int64_t offset, BLFileSeekType seekType, int64_t* positionOut) noexcept {
  *positionOut = -1;

  DWORD dwMoveMethod = 0;
  switch (seekType) {
    case BL_FILE_SEEK_SET: dwMoveMethod = FILE_BEGIN  ; break;
    case BL_FILE_SEEK_CUR: dwMoveMethod = FILE_CURRENT; break;
    case BL_FILE_SEEK_END: dwMoveMethod = FILE_END    ; break;

    default:
      return blTraceError(BL_ERROR_INVALID_VALUE);
  }

  if (!isFileOpen(self))
    return blTraceError(BL_ERROR_INVALID_HANDLE);

  LARGE_INTEGER to;
  LARGE_INTEGER prev;

  to.QuadPart = offset;
  prev.QuadPart = 0;

  HANDLE handle = (HANDLE)self->handle;
  BOOL result = SetFilePointerEx(handle, to, &prev, dwMoveMethod);

  if (!result)
    return blTraceError(blResultFromWinError(GetLastError()));

  *positionOut = prev.QuadPart;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blFileRead(BLFileCore* self, void* buffer, size_t n, size_t* bytesReadOut) noexcept {
  *bytesReadOut = 0;
  if (!isFileOpen(self))
    return blTraceError(BL_ERROR_INVALID_HANDLE);

  BOOL result = true;
  HANDLE handle = (HANDLE)self->handle;

  size_t remainingSize = n;
  size_t bytesReadTotal = 0;

  while (remainingSize) {
    DWORD localSize = static_cast<DWORD>(blMin<size_t>(remainingSize, kFileBufferSize));
    DWORD bytesRead = 0;

    result = ReadFile(handle, buffer, localSize, &bytesRead, nullptr);
    remainingSize -= localSize;
    bytesReadTotal += bytesRead;

    if (bytesRead < localSize || !result)
      break;

    buffer = bl::PtrOps::offset(buffer, bytesRead);
  }

  *bytesReadOut = bytesReadTotal;
  if (!result) {
    DWORD e = GetLastError();
    if (e == ERROR_HANDLE_EOF)
      return BL_SUCCESS;
    return blTraceError(blResultFromWinError(e));
  }
  else {
    return BL_SUCCESS;
  }
}

BL_API_IMPL BLResult blFileWrite(BLFileCore* self, const void* buffer, size_t n, size_t* bytesWrittenOut) noexcept {
  *bytesWrittenOut = 0;
  if (!isFileOpen(self))
    return blTraceError(BL_ERROR_INVALID_HANDLE);

  HANDLE handle = (HANDLE)self->handle;
  BOOL result = true;

  size_t remainingSize = n;
  size_t bytesWrittenTotal = 0;

  while (remainingSize) {
    DWORD localSize = static_cast<DWORD>(blMin<size_t>(remainingSize, kFileBufferSize));
    DWORD bytesWritten = 0;

    result = WriteFile(handle, buffer, localSize, &bytesWritten, nullptr);
    remainingSize -= localSize;
    bytesWrittenTotal += bytesWritten;

    if (bytesWritten < localSize || !result)
      break;

    buffer = bl::PtrOps::offset(buffer, bytesWritten);
  }

  *bytesWrittenOut = bytesWrittenTotal;
  if (!result)
    return blTraceError(blResultFromWinError(GetLastError()));

  return BL_SUCCESS;
}

BL_API_IMPL BLResult blFileTruncate(BLFileCore* self, int64_t maxSize) noexcept {
  if (!isFileOpen(self))
    return blTraceError(BL_ERROR_INVALID_HANDLE);

  if (BL_UNLIKELY(maxSize < 0))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  int64_t prev;
  BL_PROPAGATE(blFileSeek(self, maxSize, BL_FILE_SEEK_SET, &prev));

  HANDLE handle = (HANDLE)self->handle;
  BOOL result = SetEndOfFile(handle);

  if (prev < maxSize)
    blFileSeek(self, prev, BL_FILE_SEEK_SET, &prev);

  if (!result)
    return blTraceError(blResultFromWinError(GetLastError()));
  else
    return BL_SUCCESS;
}

BL_API_IMPL BLResult blFileGetInfo(BLFileCore* self, BLFileInfo* infoOut) noexcept {
  *infoOut = BLFileInfo{};

  if (!isFileOpen(self)) {
    return blTraceError(BL_ERROR_INVALID_HANDLE);
  }

  HANDLE handle = (HANDLE)self->handle;
  BY_HANDLE_FILE_INFORMATION fi;

  if (!GetFileInformationByHandle(handle, &fi)) {
    return blTraceError(blResultFromWinError(GetLastError()));
  }

  infoOut->size = bl::FileSystem::combineHiLo(fi.nFileSizeHigh, fi.nFileSizeLow);
  infoOut->modifiedTime = bl::FileSystem::unixMicroFromFileTime(fi.ftLastWriteTime);
  infoOut->flags = bl::FileSystem::fileFlagsFromDWFileAttributes(fi.dwFileAttributes);

  return BL_SUCCESS;
}

BL_API_IMPL BLResult blFileGetSize(BLFileCore* self, uint64_t* fileSizeOut) noexcept {
  *fileSizeOut = 0;

  if (!isFileOpen(self)) {
    return blTraceError(BL_ERROR_INVALID_HANDLE);
  }

  HANDLE handle = (HANDLE)self->handle;
  LARGE_INTEGER size;

  if (!GetFileSizeEx(handle, &size)) {
    return blTraceError(blResultFromWinError(GetLastError()));
  }

  *fileSizeOut = uint64_t(size.QuadPart);
  return BL_SUCCESS;
}

// BLFileSystem - API - Windows Implementation
// ===========================================

BL_API_IMPL BLResult BL_CDECL blFileSystemGetInfo(const char* fileName, BLFileInfo* infoOut) noexcept {
  *infoOut = BLFileInfo{};

  BLUtf16StringTmp<kStaticUTF16StringSize> fileNameW;
  BL_PROPAGATE(fileNameW.fromUtf8(fileName));

  WIN32_FILE_ATTRIBUTE_DATA fa;
  if (!GetFileAttributesExW(fileNameW.dataAsWCharT(), GetFileExInfoStandard, &fa)) {
    return blTraceError(blResultFromWinError(GetLastError()));
  }

  return bl::FileSystem::fileInfoFromFileAttributeData(*infoOut, fa);
}

#else

// BLFileSystem - API - POSIX Implementation (Internal)
// ====================================================

namespace bl {
namespace FileSystem {

template<uint32_t kDst, uint32_t kSrc, uint32_t kMsk = 0x1>
static BL_INLINE uint32_t translateFlags(uint32_t src) noexcept {
  constexpr uint32_t kDstOffset = bl::IntOps::ctzStatic(kDst);
  constexpr uint32_t kSrcOffset = bl::IntOps::ctzStatic(kSrc);

  if BL_CONSTEXPR (kDstOffset < kSrcOffset) {
    return (src >> (kSrcOffset - kDstOffset)) & (kMsk << kDstOffset);
  }
  else {
    return (src << (kDstOffset - kSrcOffset)) & (kMsk << kDstOffset);
  }
}

template<BLFileInfoFlags kDstX, uint32_t kSrcR, uint32_t kSrcW, uint32_t kSrcX>
static BL_INLINE uint32_t translateRWX(uint32_t src) noexcept {
  if BL_CONSTEXPR (kSrcW == (kSrcX << 1) && kSrcR == (kSrcX << 2)) {
    return translateFlags<kDstX, kSrcX, 0x7>(src);
  }
  else {
    return translateFlags<kDstX << 0, kSrcX>(src) |
           translateFlags<kDstX << 1, kSrcW>(src) |
           translateFlags<kDstX << 2, kSrcR>(src) ;
  }
}

template<typename T>
static BL_INLINE int64_t unixMicroFromFileTime(const T& t) noexcept {
  return int64_t(uint64_t(t) * 1000000);
}

#if defined(BL_FS_STAT_MTIMESPEC)
template<typename T>
static BL_INLINE int64_t unixMicroFromTimeSpec(const T& ts) noexcept {
  return int64_t(uint64_t(ts.tv_sec) + (uint32_t(ts.tv_nsec) / 1000u));
}
#endif

static BLResult fileInfoFromStat(BLFileInfo& info, struct BL_FILE64_API(stat)& s) noexcept {
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
  flags |= translateRWX<BL_FILE_INFO_OWNER_X, S_IRUSR, S_IWUSR, S_IXUSR>(s.st_mode);
  flags |= translateRWX<BL_FILE_INFO_GROUP_X, S_IRGRP, S_IWGRP, S_IXGRP>(s.st_mode);
  flags |= translateRWX<BL_FILE_INFO_OTHER_X, S_IROTH, S_IWOTH, S_IXOTH>(s.st_mode);
  flags |= translateFlags<BL_FILE_INFO_SUID, S_ISUID>(s.st_mode);
  flags |= translateFlags<BL_FILE_INFO_SGID, S_ISGID>(s.st_mode);

  info = BLFileInfo{};

  if (flags & BL_FILE_INFO_REGULAR)
    info.size = uint64_t(s.st_size);

  info.flags = BLFileInfoFlags(flags);
  info.uid = uint32_t(s.st_uid);
  info.gid = uint32_t(s.st_gid);

#if defined(BL_FS_STAT_MTIMESPEC)
  info.modifiedTime = unixMicroFromTimeSpec(BL_FS_STAT_MTIMESPEC(s));
#else
  info.modifiedTime = unixMicroFromFileTime(s.st_mtime);
#endif

  return BL_SUCCESS;
}

} // {FileSystem} namespace
} // {bl} namespace

// BLFile - API - POSIX Implementation
// ===================================

BL_API_IMPL BLResult blFileOpen(BLFileCore* self, const char* fileName, BLFileOpenFlags openFlags) noexcept {
  int of = 0;

  switch (openFlags & BL_FILE_OPEN_RW) {
    case BL_FILE_OPEN_READ : of |= O_RDONLY; break;
    case BL_FILE_OPEN_WRITE: of |= O_WRONLY; break;
    case BL_FILE_OPEN_RW   : of |= O_RDWR  ; break;
    default:
      return blTraceError(BL_ERROR_INVALID_VALUE);
  }

  uint32_t kExtFlags = BL_FILE_OPEN_CREATE           |
                       BL_FILE_OPEN_CREATE_EXCLUSIVE |
                       BL_FILE_OPEN_TRUNCATE         ;

  if ((openFlags & kExtFlags) && !(openFlags & BL_FILE_OPEN_WRITE))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  if (openFlags & BL_FILE_OPEN_CREATE          ) of |= O_CREAT;
  if (openFlags & BL_FILE_OPEN_CREATE_EXCLUSIVE) of |= O_CREAT | O_EXCL;
  if (openFlags & BL_FILE_OPEN_TRUNCATE        ) of |= O_TRUNC;

  mode_t om = S_IRUSR | S_IWUSR |
              S_IRGRP | S_IWGRP |
              S_IROTH | S_IWOTH ;

  // NOTE: Do not close the file before calling `open()`. We should behave atomically, which means that we won't
  // close the existing file if `open()` fails...
  int fd = BL_FILE64_API(open)(fileName, of, om);
  if (fd < 0) {
    return blTraceError(blResultFromPosixError(errno));
  }

  blFileClose(self);
  self->handle = intptr_t(fd);

  return BL_SUCCESS;
}

BL_API_IMPL BLResult blFileClose(BLFileCore* self) noexcept {
  if (isFileOpen(self)) {
    int fd = int(self->handle);
    int result = close(fd);

    // NOTE: Even when `close()` fails the handle cannot be used again as it could have already been reused. The
    // failure is just to inform the user that something failed and that there may be data-loss or handle leakage.
    self->handle = -1;

    if (BL_UNLIKELY(result != 0))
      return blTraceError(blResultFromPosixError(errno));
  }

  return BL_SUCCESS;
}

BL_API_IMPL BLResult blFileSeek(BLFileCore* self, int64_t offset, BLFileSeekType seekType, int64_t* positionOut) noexcept {
  *positionOut = -1;

  int whence = 0;
  switch (seekType) {
    case BL_FILE_SEEK_SET: whence = SEEK_SET; break;
    case BL_FILE_SEEK_CUR: whence = SEEK_CUR; break;
    case BL_FILE_SEEK_END: whence = SEEK_END; break;

    default:
      return blTraceError(BL_ERROR_INVALID_VALUE);
  }

  if (!isFileOpen(self))
    return blTraceError(BL_ERROR_INVALID_HANDLE);

  int fd = int(self->handle);
  int64_t result = BL_FILE64_API(lseek)(fd, offset, whence);

  if (result < 0) {
    int e = errno;

    // Returned when the file was not open for reading or writing.
    if (e == EBADF)
      return blTraceError(BL_ERROR_NOT_PERMITTED);

    return blTraceError(blResultFromPosixError(errno));
  }

  *positionOut = result;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blFileRead(BLFileCore* self, void* buffer, size_t n, size_t* bytesReadOut) noexcept {
  using SignedSizeT = std::make_signed<size_t>::type;

  if (!isFileOpen(self)) {
    *bytesReadOut = 0;
    return blTraceError(BL_ERROR_INVALID_HANDLE);
  }

  int fd = int(self->handle);
  size_t bytesRead = 0;

  for (;;) {
    SignedSizeT result = read(fd, buffer, n - bytesRead);
    if (result < 0) {
      int e = errno;
      *bytesReadOut = bytesRead;

      // Returned when the file was not open for reading.
      if (e == EBADF)
        return blTraceError(BL_ERROR_NOT_PERMITTED);

      return blTraceError(blResultFromPosixError(e));
    }
    else {
      bytesRead += size_t(result);
      if (bytesRead == n || result == 0)
        break;
    }
  }

  *bytesReadOut = bytesRead;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blFileWrite(BLFileCore* self, const void* buffer, size_t n, size_t* bytesWrittenOut) noexcept {
  typedef std::make_signed<size_t>::type SignedSizeT;

  if (!isFileOpen(self)) {
    *bytesWrittenOut = 0;
    return blTraceError(BL_ERROR_INVALID_HANDLE);
  }

  int fd = int(self->handle);
  size_t bytesWritten = 0;

  for (;;) {
    SignedSizeT result = write(fd, buffer, n - bytesWritten);
    if (result < 0) {
      int e = errno;
      *bytesWrittenOut = bytesWritten;

      // These are the two errors that would be returned if the file was open for read-only.
      if (e == EBADF || e == EINVAL)
        return blTraceError(BL_ERROR_NOT_PERMITTED);

      return blTraceError(blResultFromPosixError(e));
    }
    else {
      bytesWritten += size_t(result);
      if (bytesWritten == n || result == 0)
        break;
    }
  }

  *bytesWrittenOut = bytesWritten;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blFileTruncate(BLFileCore* self, int64_t maxSize) noexcept {
  if (!isFileOpen(self))
    return blTraceError(BL_ERROR_INVALID_HANDLE);

  if (maxSize < 0)
    return blTraceError(BL_ERROR_INVALID_VALUE);

  int fd = int(self->handle);
  int result = BL_FILE64_API(ftruncate)(fd, maxSize);

  if (result != 0) {
    int e = errno;

    // These are the two errors that would be returned if the file was open for read-only.
    if (e == EBADF || e == EINVAL)
      return blTraceError(BL_ERROR_NOT_PERMITTED);

    // File was smaller than `maxSize` - we don't consider this to be an error.
    if (e == EFBIG)
      return BL_SUCCESS;

    return blTraceError(blResultFromPosixError(e));
  }
  else {
    return BL_SUCCESS;
  }
}

BL_API_IMPL BLResult blFileGetInfo(BLFileCore* self, BLFileInfo* infoOut) noexcept {
  if (!isFileOpen(self)) {
    *infoOut = BLFileInfo{};
    return blTraceError(BL_ERROR_INVALID_HANDLE);
  }

  int fd = int(self->handle);
  struct BL_FILE64_API(stat) s;

  if (BL_FILE64_API(fstat)(fd, &s) != 0) {
    *infoOut = BLFileInfo{};
    return blTraceError(blResultFromPosixError(errno));
  }

  return bl::FileSystem::fileInfoFromStat(*infoOut, s);
}

BL_API_IMPL BLResult blFileGetSize(BLFileCore* self, uint64_t* fileSizeOut) noexcept {
  if (!isFileOpen(self)) {
    *fileSizeOut = 0;
    return blTraceError(BL_ERROR_INVALID_HANDLE);
  }

  int fd = int(self->handle);
  struct BL_FILE64_API(stat) s;

  if (BL_FILE64_API(fstat)(fd, &s) != 0) {
    *fileSizeOut = 0;
    return blTraceError(blResultFromPosixError(errno));
  }

  *fileSizeOut = uint64_t(s.st_size);
  return BL_SUCCESS;
}

// BLFileSystem - API - POSIX Implementation
// =========================================

BL_API_IMPL BLResult blFileSystemGetInfo(const char* fileName, BLFileInfo* infoOut) noexcept {
  struct BL_FILE64_API(stat) s;

  if (BL_FILE64_API(stat)(fileName, &s) != 0) {
    *infoOut = BLFileInfo{};
    return blTraceError(blResultFromPosixError(errno));
  }

  return bl::FileSystem::fileInfoFromStat(*infoOut, s);
}
#endif

#if defined(_WIN32)

// BLFileMapping - Windows Implementation
// ======================================

BLResult BLFileMapping::map(BLFile& file, size_t size, uint32_t flags) noexcept {
  blUnused(flags);

  if (!file.isOpen())
    return blTraceError(BL_ERROR_INVALID_VALUE);

  DWORD dwProtect = PAGE_READONLY;
  DWORD dwDesiredAccess = FILE_MAP_READ;

  // Create a file mapping handle and map view of file into it.
#if defined(BL_PLATFORM_UWP)
  HANDLE hFileMapping = CreateFileMappingFromApp(
    (HANDLE)file.handle, // hFile
    nullptr,             // SecurityAttributes
    dwProtect,           // PageProtection
    0,                   // MaximumSize
    nullptr              // Name
  );
#else
  HANDLE hFileMapping = CreateFileMappingW(
    (HANDLE)file.handle, // hFile
    nullptr,             // FileMappingAttributes
    dwProtect,           // PageProtection
    0,                   // MaximumSizeHigh
    0,                   // MaximumSizeLow
    nullptr              // Name
  );
#endif

  if (hFileMapping == nullptr)
    return blTraceError(blResultFromWinError(GetLastError()));

#if defined(BL_PLATFORM_UWP)
  void* data = MapViewOfFileFromApp(
    hFileMapping,        // hFileMappingObject
    dwDesiredAccess,     // DesiredAccess
    0,                   // FileOffset
    0                    // NumberOfBytesToMap
  );
#else
  void* data = MapViewOfFile(
    hFileMapping,        // hFileMappingObject
    dwDesiredAccess,     // DesiredAccess
    0,                   // FileOffsetHigh
    0,                   // FileOffsetLow
    0                    // NumberOfBytesToMap
  );
#endif

  if (!data) {
    BLResult result = blResultFromWinError(GetLastError());
    CloseHandle(hFileMapping);
    return blTraceError(result);
  }

  // Succeeded, now is the time to change the content of `FileMapping`.
  unmap();

  _fileMappingHandle = hFileMapping;
  _data = data;
  _size = size;

  return BL_SUCCESS;
}

BLResult BLFileMapping::unmap() noexcept {
  if (empty())
    return BL_SUCCESS;

  BLResult result = BL_SUCCESS;
  DWORD err = 0;

  if (!UnmapViewOfFile(_data))
    err = GetLastError();

  if (!CloseHandle(_fileMappingHandle) && !err)
    err = GetLastError();

  if (err)
    result = blTraceError(blResultFromWinError(err));

  _fileMappingHandle = INVALID_HANDLE_VALUE;
  _data = nullptr;
  _size = 0;

  return result;
}

#else

// BLFileMapping - POSIX Implementation
// ====================================

BLResult BLFileMapping::map(BLFile& file, size_t size, uint32_t flags) noexcept {
  blUnused(flags);

  if (!file.isOpen())
    return blTraceError(BL_ERROR_INVALID_VALUE);

  int mmapProt = PROT_READ;
  int mmapFlags = MAP_SHARED;

  // Create the mapping.
  void* data = mmap(nullptr, size, mmapProt, mmapFlags, int(file.handle), 0);
  if (data == (void *)-1)
    return blTraceError(blResultFromPosixError(errno));

  // Succeeded, now is the time to change the content of `BLFileMapping`.
  unmap();

  _data = data;
  _size = size;

  return BL_SUCCESS;
}

BLResult BLFileMapping::unmap() noexcept {
  if (empty())
    return BL_SUCCESS;

  BLResult result = BL_SUCCESS;
  int unmapStatus = munmap(_data, _size);

  // If error happened we must read `errno` now as a call to `close()` may
  // trash it. We prefer the first error instead of the last one.
  if (unmapStatus != 0)
    result = blTraceError(blResultFromPosixError(errno));

  _data = nullptr;
  _size = 0;

  return result;
}

#endif

// BLFileSystem - Memory Mapped File
// =================================

namespace bl {

static void BL_CDECL destroyMemoryMappedFile(void* impl, void* externalData, void* userData) noexcept {
  blUnused(externalData, userData);

  BLFileMapping* implFileMapping = PtrOps::offset<BLFileMapping>(impl, sizeof(BLArrayImpl));
  blCallDtor(*implFileMapping);
}

static BLResult createMemoryMappedFile(BLArray<uint8_t>* dst, BLFile& file, size_t size) noexcept {
  // This condition must be handled before.
  BL_ASSERT(size != 0);

  BLFileMapping fileMapping;
  BL_PROPAGATE(fileMapping.map(file, size));

  BLObjectImplSize implSize(sizeof(BLArrayImpl) + sizeof(BLFileMapping));
  uint32_t info = BLObjectInfo::packTypeWithMarker(BL_OBJECT_TYPE_ARRAY_UINT8);

  BLArrayCore newO;
  BL_PROPAGATE(ObjectInternal::allocImplExternalT<BLArrayImpl>(&newO, BLObjectInfo{info}, implSize, true, destroyMemoryMappedFile, nullptr));

  BLArrayImpl* impl = bl::ArrayInternal::getImpl(&newO);
  impl->data = fileMapping.data<void>();
  impl->size = size;
  impl->capacity = size;

  BLFileMapping* implFileMapping = PtrOps::offset<BLFileMapping>(impl, sizeof(BLArrayImpl));
  blCallCtor(*implFileMapping, BLInternal::move(fileMapping));

  return bl::ArrayInternal::replaceInstance(dst, &newO);
}

} // {bl}

// BLFileSystem - Read & Write File
// ================================

static constexpr uint32_t kSmallFileSizeThreshold = 16 * 1024;

BL_API_IMPL BLResult blFileSystemReadFile(const char* fileName, BLArrayCore* dst_, size_t maxSize, BLFileReadFlags readFlags) noexcept {
  if (BL_UNLIKELY(dst_->_d.rawType() != BL_OBJECT_TYPE_ARRAY_UINT8))
    return blTraceError(BL_ERROR_INVALID_STATE);

  BLArray<uint8_t>& dst = dst_->dcast<BLArray<uint8_t>>();
  dst.clear();

  BLFile file;
  BL_PROPAGATE(file.open(fileName, BL_FILE_OPEN_READ));

  // TODO: This won't read `stat` files.
  uint64_t size64;
  BL_PROPAGATE(file.getSize(&size64));

  if (size64 == 0)
    return BL_SUCCESS;

  if (maxSize)
    size64 = blMin<uint64_t>(size64, maxSize);

  if (blRuntimeIs32Bit() && BL_UNLIKELY(size64 >= uint64_t(SIZE_MAX)))
    return blTraceError(BL_ERROR_FILE_TOO_LARGE);

  size_t size = size_t(size64);

  // Use memory mapped file IO if enabled.
  if (readFlags & BL_FILE_READ_MMAP_ENABLED) {
    bool isSmall = size < kSmallFileSizeThreshold;
    if (!(readFlags & BL_FILE_READ_MMAP_AVOID_SMALL) || !isSmall) {
      BLResult result = bl::createMemoryMappedFile(&dst, file, size);
      if (result == BL_SUCCESS)
        return result;

      if (readFlags & BL_FILE_READ_MMAP_NO_FALLBACK)
        return result;
    }
  }

  uint8_t* data;
  BL_PROPAGATE(dst.modifyOp(BL_MODIFY_OP_ASSIGN_FIT, size, &data));

  size_t bytesRead;
  BLResult result = file.read(data, size, &bytesRead);
  dst.resize(bytesRead, 0);
  return result;
}

BL_API_IMPL BLResult blFileSystemWriteFile(const char* fileName, const void* data, size_t size, size_t* bytesWrittenOut) noexcept {
  *bytesWrittenOut = 0;

  BLFile file;
  BL_PROPAGATE(file.open(fileName, BLFileOpenFlags(BL_FILE_OPEN_WRITE | BL_FILE_OPEN_CREATE | BL_FILE_OPEN_TRUNCATE)));
  return size ? file.write(data, size , bytesWrittenOut) : BL_SUCCESS;
}
