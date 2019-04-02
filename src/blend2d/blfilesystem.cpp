// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#include "./blapi-build_p.h"
#include "./blarray.h"
#include "./blfilesystem_p.h"
#include "./blruntime_p.h"
#include "./blsupport_p.h"
#include "./blunicode_p.h"

#ifndef _WIN32
  #include <errno.h>
  #include <fcntl.h>
  #include <unistd.h>
  #include <sys/mman.h>
  #include <sys/stat.h>
  #include <sys/types.h>
  #include <unistd.h>
#endif

// ============================================================================
// [BLWinU16String]
// ============================================================================

#ifdef _WIN32

template<size_t N>
class BLWinU16String {
public:
  BL_NONCOPYABLE(BLWinU16String);

  BL_INLINE BLWinU16String() noexcept
    : _data(_embeddedData),
      _size(0),
      _capacity(N) {}

  BL_INLINE ~BLWinU16String() noexcept {
    if (_data != _embeddedData)
      free(_data);
  }

  BL_INLINE uint16_t* data() const noexcept { return _data; }
  BL_INLINE wchar_t* dataAsWideChar() const noexcept { return reinterpret_cast<wchar_t*>(_data); }

  BL_INLINE size_t size() const noexcept { return _size; }
  BL_INLINE size_t capacity() const noexcept { return _capacity; }

  BL_INLINE void nullTerminate() noexcept { _data[_size] = uint16_t(0); }

  BL_NOINLINE BLResult fromUtf8(const char* src) noexcept {
    size_t srcSize = strlen(src);
    BLUnicodeConversionState conversionState;

    BLResult result = blConvertUnicode(
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

    BLUnicodeValidationState validationState;
    BL_PROPAGATE(blValidateUtf8(src + procUtf8Size, srcSize - procUtf8Size, validationState));

    size_t newSize = procUtf16Size + validationState.utf16Index;
    uint16_t* newData = static_cast<uint16_t*>(malloc((newSize + 1) * sizeof(uint16_t)));

    if (BL_UNLIKELY(!newData)) {
      _size = 0;
      nullTerminate();
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);
    }

    memcpy(newData, _data, procUtf16Size * sizeof(uint16_t));
    blConvertUnicode(
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

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  uint16_t* _data;
  size_t _size;
  size_t _capacity;
  uint16_t _embeddedData[N + 1];
};

#endif

// ============================================================================
// [BLFile]
// ============================================================================

// Just a helper to case to `BLFile` and call its `isOpen()` as this is the
// only thing we need from C++ API here.
static BL_INLINE bool blFileIsOpen(const BLFileCore* self) noexcept {
  return static_cast<const BLFile*>(self)->isOpen();
}

BLResult blFileInit(BLFileCore* self) noexcept {
  self->handle = -1;
  return BL_SUCCESS;
}

BLResult blFileReset(BLFileCore* self) noexcept {
  return blFileClose(self);
}

#ifdef _WIN32

static const constexpr DWORD BL_FILE_BUFFER_RW_SIZE = 32 * 1024 * 1024; // 32 MB.

BLResult blFileOpen(BLFileCore* self, const char* fileName, uint32_t openFlags) noexcept {
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
  // Since WinAPI documentation is so brief here is a better explanation
  // about various CreationDisposition modes, reformatted from SO:
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

  uint32_t kExtFlags = BL_FILE_OPEN_CREATE      |
                       BL_FILE_OPEN_CREATE_ONLY |
                       BL_FILE_OPEN_TRUNCATE    ;

  if ((openFlags & kExtFlags) && (!(openFlags & BL_FILE_OPEN_WRITE)))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  DWORD dwCreationDisposition = OPEN_EXISTING;
  if (openFlags & BL_FILE_OPEN_CREATE_ONLY)
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

  if (openFlags & BL_FILE_OPEN_SHARE_READ  ) dwShareMode |= FILE_SHARE_READ;
  if (openFlags & BL_FILE_OPEN_SHARE_WRITE ) dwShareMode |= FILE_SHARE_WRITE;
  if (openFlags & BL_FILE_OPEN_SHARE_DELETE) dwShareMode |= FILE_SHARE_DELETE;

  // Other Flags
  // -----------

  DWORD dwFlagsAndAttributes = 0;
  LPSECURITY_ATTRIBUTES lpSecurityAttributes = nullptr;

  // NOTE: Do not close the file before calling `CreateFileW()`. We should
  // behave atomically, which means that we won't close the existing file
  // if `CreateFileW()` fails...
  BLWinU16String<1024> fileNameW;
  BL_PROPAGATE(fileNameW.fromUtf8(fileName));

  HANDLE handle = CreateFileW(
    fileNameW.dataAsWideChar(),
    dwDesiredAccess,
    dwShareMode,
    lpSecurityAttributes,
    dwCreationDisposition,
    dwFlagsAndAttributes,
    nullptr);

  if (handle == INVALID_HANDLE_VALUE)
    return blTraceError(blResultFromWinError(GetLastError()));

  blFileClose(self);
  self->handle = intptr_t(handle);

  return BL_SUCCESS;
}

BLResult blFileClose(BLFileCore* self) noexcept {
  // Not sure what should happen if `CloseHandle()` fails, if the handle is
  // invalid or the close can be called again? To ensure compatibility with
  // POSIX implementation we just make it invalid.
  if (blFileIsOpen(self)) {
    HANDLE handle = (HANDLE)self->handle;
    BOOL result = CloseHandle(handle);

    self->handle = -1;
    if (!result)
      return blTraceError(blResultFromWinError(GetLastError()));
  }

  return BL_SUCCESS;
}

BLResult blFileSeek(BLFileCore* self, int64_t offset, uint32_t seekType, int64_t* positionOut) noexcept {
  *positionOut = -1;

  DWORD dwMoveMethod = 0;
  switch (seekType) {
    case BL_FILE_SEEK_SET: dwMoveMethod = FILE_BEGIN  ; break;
    case BL_FILE_SEEK_CUR: dwMoveMethod = FILE_CURRENT; break;
    case BL_FILE_SEEK_END: dwMoveMethod = FILE_END    ; break;

    default:
      return blTraceError(BL_ERROR_INVALID_VALUE);
  }

  if (!blFileIsOpen(self))
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

BLResult blFileRead(BLFileCore* self, void* buffer, size_t n, size_t* bytesReadOut) noexcept {
  *bytesReadOut = 0;
  if (!blFileIsOpen(self))
    return blTraceError(BL_ERROR_INVALID_HANDLE);

  BOOL result = true;
  HANDLE handle = (HANDLE)self->handle;

  size_t remainingSize = n;
  size_t bytesReadTotal = 0;

  while (remainingSize) {
    DWORD localSize = static_cast<DWORD>(blMin<size_t>(remainingSize, BL_FILE_BUFFER_RW_SIZE));
    DWORD bytesRead = 0;

    result = ReadFile(handle, buffer, localSize, &bytesRead, nullptr);
    remainingSize -= localSize;
    bytesReadTotal += bytesRead;

    if (bytesRead < localSize || !result)
      break;

    buffer = blOffsetPtr(buffer, bytesRead);
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

BLResult blFileWrite(BLFileCore* self, const void* buffer, size_t n, size_t* bytesWrittenOut) noexcept {
  *bytesWrittenOut = 0;
  if (!blFileIsOpen(self))
    return blTraceError(BL_ERROR_INVALID_HANDLE);

  HANDLE handle = (HANDLE)self->handle;
  BOOL result = true;

  size_t remainingSize = n;
  size_t bytesWrittenTotal = 0;

  while (remainingSize) {
    DWORD localSize = static_cast<DWORD>(blMin<size_t>(remainingSize, BL_FILE_BUFFER_RW_SIZE));
    DWORD bytesWritten = 0;

    result = WriteFile(handle, buffer, localSize, &bytesWritten, nullptr);
    remainingSize -= localSize;
    bytesWrittenTotal += bytesWritten;

    if (bytesWritten < localSize || !result)
      break;

    buffer = blOffsetPtr(buffer, bytesWritten);
  }

  *bytesWrittenOut = bytesWrittenTotal;
  if (!result)
    return blTraceError(blResultFromWinError(GetLastError()));

  return BL_SUCCESS;
}

BLResult blFileTruncate(BLFileCore* self, int64_t maxSize) noexcept {
  if (!blFileIsOpen(self))
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

BLResult blFileGetSize(BLFileCore* self, uint64_t* fileSizeOut) noexcept {
  *fileSizeOut = 0;
  if (!blFileIsOpen(self))
    return blTraceError(BL_ERROR_INVALID_HANDLE);

  LARGE_INTEGER size;
  BOOL result = GetFileSizeEx((HANDLE)self->handle, &size);

  if (!result)
    return blTraceError(blResultFromWinError(GetLastError()));

  *fileSizeOut = uint64_t(size.QuadPart);
  return BL_SUCCESS;
}

#else

// These OSes use 64-bit offsets by default.
#if defined(__APPLE__    ) || \
    defined(__HAIKU__    ) || \
    defined(__bsdi__     ) || \
    defined(__DragonFly__) || \
    defined(__FreeBSD__  ) || \
    defined(__NetBSD__   ) || \
    defined(__OpenBSD__  )
  #define BL_FILE64_API(NAME) NAME
#else
  #define BL_FILE64_API(NAME) NAME##64
#endif

BLResult blFileOpen(BLFileCore* self, const char* fileName, uint32_t openFlags) noexcept {
  int of = 0;

  switch (openFlags & BL_FILE_OPEN_RW) {
    case BL_FILE_OPEN_READ : of |= O_RDONLY; break;
    case BL_FILE_OPEN_WRITE: of |= O_WRONLY; break;
    case BL_FILE_OPEN_RW   : of |= O_RDWR  ; break;
    default:
      return blTraceError(BL_ERROR_INVALID_VALUE);
  }

  uint32_t kExtFlags = BL_FILE_OPEN_CREATE      |
                       BL_FILE_OPEN_CREATE_ONLY |
                       BL_FILE_OPEN_TRUNCATE    ;

  if ((openFlags & kExtFlags) && !(openFlags & BL_FILE_OPEN_WRITE))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  if (openFlags & BL_FILE_OPEN_CREATE     ) of |= O_CREAT;
  if (openFlags & BL_FILE_OPEN_CREATE_ONLY) of |= O_CREAT | O_EXCL;
  if (openFlags & BL_FILE_OPEN_TRUNCATE   ) of |= O_TRUNC;

  mode_t om = S_IRUSR | S_IWUSR |
              S_IRGRP | S_IWGRP |
              S_IROTH | S_IWOTH ;

  // NOTE: Do not close the file before calling `open()`. We should
  // behave atomically, which means that we won't close the existing
  // file if `open()` fails...
  int fd = BL_FILE64_API(open)(fileName, of, om);
  if (fd < 0) {
    return blTraceError(blResultFromPosixError(errno));
  }

  blFileClose(self);
  self->handle = intptr_t(fd);

  return BL_SUCCESS;
}

BLResult blFileClose(BLFileCore* self) noexcept {
  if (blFileIsOpen(self)) {
    int fd = int(self->handle);
    int result = close(fd);

    // NOTE: Even when `close()` fails the handle cannot be used again as it
    // could have already been reused. The failure is just to inform the user
    // that something failed and that there may be data-loss.
    self->handle = -1;

    if (BL_UNLIKELY(result != 0))
      return blTraceError(blResultFromPosixError(errno));
  }

  return BL_SUCCESS;
}

BLResult blFileSeek(BLFileCore* self, int64_t offset, uint32_t seekType, int64_t* positionOut) noexcept {
  *positionOut = -1;

  int whence = 0;
  switch (seekType) {
    case BL_FILE_SEEK_SET: whence = SEEK_SET; break;
    case BL_FILE_SEEK_CUR: whence = SEEK_CUR; break;
    case BL_FILE_SEEK_END: whence = SEEK_END; break;

    default:
      return blTraceError(BL_ERROR_INVALID_VALUE);
  }

  if (!blFileIsOpen(self))
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

BLResult blFileRead(BLFileCore* self, void* buffer, size_t n, size_t* bytesReadOut) noexcept {
  typedef std::make_signed<size_t>::type SignedSizeT;

  *bytesReadOut = 0;
  if (!blFileIsOpen(self))
    return blTraceError(BL_ERROR_INVALID_HANDLE);

  int fd = int(self->handle);
  SignedSizeT result = read(fd, buffer, n);

  if (result < 0) {
    int e = errno;

    // Returned when the file was not open for reading.
    if (e == EBADF)
      return blTraceError(BL_ERROR_NOT_PERMITTED);

    return blTraceError(blResultFromPosixError(e));
  }

  *bytesReadOut = size_t(result);
  return BL_SUCCESS;
}

BLResult blFileWrite(BLFileCore* self, const void* buffer, size_t n, size_t* bytesWrittenOut) noexcept {
  typedef std::make_signed<size_t>::type SignedSizeT;

  *bytesWrittenOut = 0;
  if (!blFileIsOpen(self))
    return blTraceError(BL_ERROR_INVALID_HANDLE);

  int fd = int(self->handle);
  SignedSizeT result = write(fd, buffer, n);

  if (result < 0) {
    int e = errno;

    // These are the two errors that would be returned if the file was open for read-only.
    if (e == EBADF || e == EINVAL)
      return blTraceError(BL_ERROR_NOT_PERMITTED);

    return blTraceError(blResultFromPosixError(e));
  }

  *bytesWrittenOut = size_t(result);
  return BL_SUCCESS;
}

BLResult blFileTruncate(BLFileCore* self, int64_t maxSize) noexcept {
  if (!blFileIsOpen(self))
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

BLResult blFileGetSize(BLFileCore* self, uint64_t* fileSizeOut) noexcept {
  *fileSizeOut = 0;
  if (!blFileIsOpen(self))
    return blTraceError(BL_ERROR_INVALID_HANDLE);

  int fd = int(self->handle);
  struct stat s;

  if (fstat(fd, &s) != 0)
    return blTraceError(blResultFromPosixError(errno));

  *fileSizeOut = s.st_size;
  return BL_SUCCESS;
}

#undef BL_FILE64_API
#endif

// ============================================================================
// [BLFileMapping - Utilities]
// ============================================================================

static BLResult blFileMappingCreateCopyOfFile(BLFileMapping* self, BLFile& file, uint32_t flags, size_t size) noexcept {
  BLResult result = file.seek(0, BL_FILE_SEEK_SET);
  if (result != BL_SUCCESS && result != BL_ERROR_INVALID_SEEK)
    return result;

  void* data = malloc(size);
  if (!data)
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  size_t bytesRead;
  result = file.read(data, size, &bytesRead);

  if (result != BL_SUCCESS || bytesRead != size) {
    free(data);
    return result ? result : blTraceError(BL_ERROR_NO_MORE_DATA);
  }

  self->unmap();
  self->_data = data;
  self->_size = size;
  self->_status = BLFileMapping::kStatusCopied;

  // Since we loaded the whole `file` and don't need to retain the handle we
  // close it. This makes the API to behave the same way as if we mapped it
  // and we kept the handle.
  if (!(flags & BLFileMapping::kDontCloseOnCopy))
    file.close();

  return BL_SUCCESS;
}

static BLResult blFileMappingReleaseCopyOfFile(BLFileMapping* self) noexcept {
  free(self->_data);
  self->_data = nullptr;
  self->_size = 0;
  self->_status = BLFileMapping::kStatusEmpty;
  return BL_SUCCESS;
}

static BLResult blFileMappingCheck64BitFileSize(size_t& dstSize, uint64_t srcSize) noexcept {
  if (!srcSize)
    return blTraceError(BL_ERROR_FILE_EMPTY);

  #if BL_TARGET_ARCH_BITS < 64
  if (srcSize > SIZE_MAX)
    return blTraceError(BL_ERROR_FILE_TOO_LARGE);
  #endif

  dstSize = size_t(srcSize);
  return BL_SUCCESS;
}

// ============================================================================
// [BLFileMapping - API]
// ============================================================================

#if defined(_WIN32)

BLResult BLFileMapping::map(BLFile& file, uint32_t flags) noexcept {
  if (!file.isOpen())
    return blTraceError(BL_ERROR_INVALID_VALUE);

  DWORD dwProtect = PAGE_READONLY;
  DWORD dwDesiredAccess = FILE_MAP_READ;

  // Get the size of the file.
  size_t size;
  uint64_t size64;

  BL_PROPAGATE(file.getSize(&size64));
  BL_PROPAGATE(blFileMappingCheck64BitFileSize(size, size64));

  if ((flags & kCopySmallFiles) && size <= kSmallFileSize)
    return blFileMappingCreateCopyOfFile(this, file, flags, size);

  // Create a file mapping handle and map view of file into it.
  HANDLE hBLFileMapping = CreateFileMappingW((HANDLE)file.handle, nullptr, dwProtect, 0, 0, nullptr);
  void* data = nullptr;

  if (hBLFileMapping != nullptr) {
    data = MapViewOfFile(hBLFileMapping, dwDesiredAccess, 0, 0, 0);
    if (!data)
      CloseHandle(hBLFileMapping);
  }

  if (data == nullptr) {
    if (!(flags & kCopyOnFailure))
      return blTraceError(blResultFromWinError(::GetLastError()));
    else
      return blFileMappingCreateCopyOfFile(this, file, flags, size);
  }
  else {
    // Succeeded, now is the time to change the content of `BLFileMapping`.
    unmap();

    _file.handle = file.takeHandle();
    _fileMappingHandle = hBLFileMapping;
    _data = data;
    _size = size;
    _status = kStatusMapped;

    return BL_SUCCESS;
  }
}

BLResult BLFileMapping::unmap() noexcept {
  switch (status()) {
    case kStatusMapped: {
      BLResult result = BL_SUCCESS;
      DWORD dwError = 0;

      if (!UnmapViewOfFile(_data))
        dwError = GetLastError();

      if (!CloseHandle(_fileMappingHandle) && !dwError)
        dwError = GetLastError();

      if (dwError)
        result = blTraceError(blResultFromWinError(dwError));

      blFileClose(&_file);
      _fileMappingHandle = INVALID_HANDLE_VALUE;
      _data = nullptr;
      _size = 0;
      _status = kStatusEmpty;

      return result;
    }

    case kStatusCopied:
      return blFileMappingReleaseCopyOfFile(this);

    default:
      return BL_SUCCESS;
  }
}

#else

BLResult BLFileMapping::map(BLFile& file, uint32_t flags) noexcept {
  if (!file.isOpen())
    return blTraceError(BL_ERROR_INVALID_VALUE);

  int mmapProt = PROT_READ;
  int mmapFlags = MAP_SHARED;

  size_t size;
  uint64_t size64;

  BL_PROPAGATE(file.getSize(&size64));
  BL_PROPAGATE(blFileMappingCheck64BitFileSize(size, size64));

  if ((flags & kCopySmallFiles) && size <= kSmallFileSize)
    return blFileMappingCreateCopyOfFile(this, file, flags, size);

  // Create the mapping.
  void* data = mmap(nullptr, size, mmapProt, mmapFlags, int(file.handle), 0);
  if (data == (void *)-1) {
    if (!(flags & kCopyOnFailure))
      return blTraceError(blResultFromPosixError(errno));
    else
      return blFileMappingCreateCopyOfFile(this, file, flags, size);
  }
  else {
    // Succeeded, now is the time to change the content of `BLFileMapping`.
    unmap();

    _file.handle = file.takeHandle();
    _data = data;
    _size = size;
    _status = kStatusMapped;

    return BL_SUCCESS;
  }
}

BLResult BLFileMapping::unmap() noexcept {
  switch (status()) {
    case kStatusMapped: {
      BLResult result = BL_SUCCESS;

      // If error happened we must read `errno` now as a call to `close()` may
      // trash it as well. We prefer the first error instead of the last one.
      int unmapStatus = munmap(_data, _size);
      if (unmapStatus != 0)
        result = blTraceError(blResultFromPosixError(errno));

      blFileClose(&_file);
      _data = nullptr;
      _size = 0;
      _status = kStatusEmpty;

      return result;
    }

    case kStatusCopied:
      return blFileMappingReleaseCopyOfFile(this);

    default:
      return BL_SUCCESS;
  }
}

#endif

// ============================================================================
// [BLFileSystem]
// ============================================================================

BLResult blFileSystemReadFile(const char* fileName, BLArrayCore* dst_, size_t maxSize) noexcept {
  BLArray<uint8_t>& dst = dst_->dcast<BLArray<uint8_t>>();
  dst.clear();

  if (BL_UNLIKELY(dst.impl->implType != BL_IMPL_TYPE_ARRAY_U8))
    return blTraceError(BL_ERROR_INVALID_STATE);

  BLFile file;
  BL_PROPAGATE(file.open(fileName, BL_FILE_OPEN_READ));

  // TODO: This won't read `stat` files.
  uint64_t size64;
  BL_PROPAGATE(file.getSize(&size64));

  if (size64 == 0)
    return blTraceError(BL_ERROR_FILE_EMPTY);

  if (maxSize)
    size64 = blMin<uint64_t>(size64, maxSize);

  if (size64 >= uint64_t(SIZE_MAX))
    return blTraceError(BL_ERROR_FILE_TOO_LARGE);

  uint8_t* data;
  BL_PROPAGATE(dst.modifyOp(BL_MODIFY_OP_ASSIGN_FIT, size_t(size64), &data));
  return file.read(data, size_t(size64), &dst.impl->size);
}

BLResult blFileSystemWriteFile(const char* fileName, const void* data, size_t size, size_t* bytesWrittenOut) noexcept {
  *bytesWrittenOut = 0;

  BLFile file;
  BL_PROPAGATE(file.open(fileName, BL_FILE_OPEN_WRITE | BL_FILE_OPEN_CREATE | BL_FILE_OPEN_TRUNCATE));
  return size ? file.write(data, size , bytesWrittenOut) : BL_SUCCESS;
}
