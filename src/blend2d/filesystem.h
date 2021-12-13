// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_FILESYSTEM_H_INCLUDED
#define BLEND2D_FILESYSTEM_H_INCLUDED

#include "array.h"

//! \addtogroup blend2d_api_filesystem
//! \{

//! \name BLFile API Constants
//!
//! \{

//! File open flags, see `BLFile::open()`.
BL_DEFINE_ENUM(BLFileOpenFlags) {
  //! No flags.
  BL_FILE_OPEN_NO_FLAGS = 0u,

  //! Opens the file for reading.
  //!
  //! The following system flags are used when opening the file:
  //!   * `O_RDONLY` (Posix)
  //!   * `GENERIC_READ` (Windows)
  BL_FILE_OPEN_READ = 0x00000001u,

  //! Opens the file for writing:
  //!
  //! The following system flags are used when opening the file:
  //!   * `O_WRONLY` (Posix)
  //!   * `GENERIC_WRITE` (Windows)
  BL_FILE_OPEN_WRITE = 0x00000002u,

  //! Opens the file for reading & writing.
  //!
  //! The following system flags are used when opening the file:
  //!   * `O_RDWR` (Posix)
  //!   * `GENERIC_READ | GENERIC_WRITE` (Windows)
  BL_FILE_OPEN_RW = 0x00000003u,

  //! Creates the file if it doesn't exist or opens it if it does.
  //!
  //! The following system flags are used when opening the file:
  //!   * `O_CREAT` (Posix)
  //!   * `CREATE_ALWAYS` or `OPEN_ALWAYS` depending on other flags (Windows)
  BL_FILE_OPEN_CREATE = 0x00000004u,

  //! Opens the file for deleting or renaming (Windows).
  //!
  //! Adds `DELETE` flag when opening the file to `ACCESS_MASK`.
  BL_FILE_OPEN_DELETE = 0x00000008u,

  //! Truncates the file.
  //!
  //! The following system flags are used when opening the file:
  //!   * `O_TRUNC` (Posix)
  //!   * `TRUNCATE_EXISTING` (Windows)
  BL_FILE_OPEN_TRUNCATE = 0x00000010u,

  //! Opens the file for reading in exclusive mode (Windows).
  //!
  //! Exclusive mode means to not specify the `FILE_SHARE_READ` option.
  BL_FILE_OPEN_READ_EXCLUSIVE = 0x10000000u,

  //! Opens the file for writing in exclusive mode (Windows).
  //!
  //! Exclusive mode means to not specify the `FILE_SHARE_WRITE` option.
  BL_FILE_OPEN_WRITE_EXCLUSIVE = 0x20000000u,

  //! Opens the file for both reading and writing (Windows).
  //!
  //! This is a combination of both `BL_FILE_OPEN_READ_EXCLUSIVE` and
  //! `BL_FILE_OPEN_WRITE_EXCLUSIVE`.
  BL_FILE_OPEN_RW_EXCLUSIVE = 0x30000000u,

  //! Creates the file in exclusive mode - fails if the file already exists.
  //!
  //! The following system flags are used when opening the file:
  //!   * `O_EXCL` (Posix)
  //!   * `CREATE_NEW` (Windows)
  BL_FILE_OPEN_CREATE_EXCLUSIVE = 0x40000000u,

  //! Opens the file for deleting or renaming in exclusive mode (Windows).
  //!
  //! Exclusive mode means to not specify the `FILE_SHARE_DELETE` option.
  BL_FILE_OPEN_DELETE_EXCLUSIVE = 0x80000000u

  BL_FORCE_ENUM_UINT32(BL_FILE_OPEN)
};

//! File seek mode, see `BLFile::seek()`.
//!
//! \note Seek constants should be compatible with constants used by both POSIX
//! and Windows API.
BL_DEFINE_ENUM(BLFileSeekType) {
  //! Seek from the beginning of the file (SEEK_SET).
  BL_FILE_SEEK_SET = 0,
  //! Seek from the current position (SEEK_CUR).
  BL_FILE_SEEK_CUR = 1,
  //! Seek from the end of the file (SEEK_END).
  BL_FILE_SEEK_END = 2,

  //! Maximum value of `BLFileSeekType`.
  BL_FILE_SEEK_MAX_VALUE = 3

  BL_FORCE_ENUM_UINT32(BL_FILE_SEEK)
};

//! File read flags used by `BLFileSystem::readFile()`.
BL_DEFINE_ENUM(BLFileReadFlags) {
  //! No flags.
  BL_FILE_READ_NO_FLAGS = 0u,

  //! Use memory mapping to read the content of the file.
  //!
  //! The destination buffer `BLArray<>` would be configured to use the memory mapped buffer instead of allocating its
  //! own.
  BL_FILE_READ_MMAP_ENABLED = 0x00000001u,

  //! Avoid memory mapping of small files.
  //!
  //! The size of small file is determined by Blend2D, however, you should expect it to be 16kB or 64kB depending on
  //! host operating system.
  BL_FILE_READ_MMAP_AVOID_SMALL = 0x00000002u,

  //! Do not fallback to regular read if memory mapping fails. It's worth noting that memory mapping would fail for
  //! files stored on filesystem that is not local (like a mounted network filesystem, etc...).
  BL_FILE_READ_MMAP_NO_FALLBACK = 0x00000008u

  BL_FORCE_ENUM_UINT32(BL_FILE_READ)
};

//! \}

//! \name BLFile C API Structs
//!
//! \{

//! A thin abstraction over a native OS file IO [C API].
struct BLFileCore {
  //! A file handle - either a file descriptor used by POSIX or file handle used by Windows. On both platforms the
  //! handle is always `intptr_t` to make FFI easier (it's basically the size of a pointer / machine register).
  //!
  //! \note In C++ mode you can use `BLFileCore::Handle` or `BLFile::Handle` to get the handle type. In C mode you
  //! must use `intptr_t`. A handle of value `-1` is considered invalid and/or uninitialized. This value also matches
  //! `INVALID_HANDLE_VALUE`, which is used by Windows API and defined to -1 as well.
  intptr_t handle;
};

//! \}

BL_BEGIN_C_DECLS
//! \name BLFile C API Functions
//!
//! File read/write functionality is provided by \ref BLFileCore in C API and wrapped by \ref BLFile in C++ API.
//!
//! \{
BL_API BLResult BL_CDECL blFileInit(BLFileCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFileReset(BLFileCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFileOpen(BLFileCore* self, const char* fileName, BLFileOpenFlags openFlags) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFileClose(BLFileCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFileSeek(BLFileCore* self, int64_t offset, BLFileSeekType seekType, int64_t* positionOut) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFileRead(BLFileCore* self, void* buffer, size_t n, size_t* bytesReadOut) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFileWrite(BLFileCore* self, const void* buffer, size_t n, size_t* bytesWrittenOut) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFileTruncate(BLFileCore* self, int64_t maxSize) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFileGetSize(BLFileCore* self, uint64_t* fileSizeOut) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blFileSystemReadFile(const char* fileName, BLArrayCore* dst, size_t maxSize, BLFileReadFlags readFlags) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFileSystemWriteFile(const char* fileName, const void* data, size_t size, size_t* bytesWrittenOut) BL_NOEXCEPT_C;
//! \}
BL_END_C_DECLS

#ifdef __cplusplus
//! \name BLFile C++ API
//!
//! \{

//! A thin abstraction over a native OS file IO [C++ API].
//!
//! A thin wrapper around a native OS file support. The file handle is always `intptr_t` and it refers to either
//! a file descriptor on POSIX targets and file handle on Windows targets.
class BLFile : public BLFileCore {
public:
  // Prevent copy-constructor and copy-assignment.
  BL_INLINE BLFile(const BLFile& other) noexcept = delete;
  BL_INLINE BLFile& operator=(const BLFile& other) noexcept = delete;

  //! \name Construction & Destruction
  //! \{

  BL_INLINE BLFile() noexcept
    : BLFileCore { -1 } {}

  BL_INLINE BLFile(BLFile&& other) noexcept {
    intptr_t h = other.handle;
    other.handle = -1;
    handle = h;
  }

  BL_INLINE explicit BLFile(intptr_t handle) noexcept
    : BLFileCore { handle } {}

  BL_INLINE BLFile& operator=(BLFile&& other) noexcept {
    intptr_t h = other.handle;
    other.handle = -1;

    this->close();
    this->handle = h;

    return *this;
  }

  BL_INLINE ~BLFile() noexcept { blFileReset(this); }

  //! \}

  //! \name Common Functionality
  //! \{

  BL_INLINE void swap(BLFile& other) noexcept { std::swap(this->handle, other.handle); }

  //! \}

  //! \name Interface
  //! \{

  //! Tests whether the file is open.
  BL_INLINE bool isOpen() const noexcept { return handle != -1; }

  BL_INLINE BLResult open(const char* fileName, BLFileOpenFlags openFlags) noexcept {
    return blFileOpen(this, fileName, openFlags);
  }

  //! Closes the file (if open) and sets the file handle to -1.
  BL_INLINE BLResult close() noexcept {
    return blFileClose(this);
  }

  BL_INLINE BLResult seek(int64_t offset, BLFileSeekType seekType) noexcept {
    int64_t positionOut;
    return blFileSeek(this, offset, seekType, &positionOut);
  }

  BL_INLINE BLResult seek(int64_t offset, BLFileSeekType seekType, int64_t* positionOut) noexcept {
    return blFileSeek(this, offset, seekType, positionOut);
  }

  BL_INLINE BLResult read(void* buffer, size_t n, size_t* bytesReadOut) noexcept {
    return blFileRead(this, buffer, n, bytesReadOut);
  }

  BL_INLINE BLResult write(const void* buffer, size_t n, size_t* bytesWrittenOut) noexcept {
    return blFileWrite(this, buffer, n, bytesWrittenOut);
  }

  BL_INLINE BLResult truncate(int64_t maxSize) noexcept {
    return blFileTruncate(this, maxSize);
  }

  BL_INLINE BLResult getSize(uint64_t* sizeOut) noexcept {
    return blFileGetSize(this, sizeOut);
  }

  //! \}
};

//! File-system utilities.
namespace BLFileSystem {

//! Reads a file into the `dst` buffer.
//!
//! Optionally you can set `maxSize` to non-zero value that would restrict the maximum bytes to read to such
//! value. In addition, `readFlags` can be used to enable file mapping. See `BLFileReadFlags` for more details.
static BL_INLINE BLResult readFile(const char* fileName, BLArray<uint8_t>& dst, size_t maxSize = 0, BLFileReadFlags readFlags = BL_FILE_READ_NO_FLAGS) noexcept {
  return blFileSystemReadFile(fileName, &dst, maxSize, readFlags);
}

static BL_INLINE BLResult writeFile(const char* fileName, const void* data, size_t size) noexcept {
  size_t bytesWrittenOut;
  return blFileSystemWriteFile(fileName, data, size, &bytesWrittenOut);
}

static BL_INLINE BLResult writeFile(const char* fileName, const void* data, size_t size, size_t* bytesWrittenOut) noexcept {
  return blFileSystemWriteFile(fileName, data, size, bytesWrittenOut);
}

static BL_INLINE BLResult writeFile(const char* fileName, const BLArrayView<uint8_t>& view) noexcept {
  return writeFile(fileName, view.data, view.size);
}

static BL_INLINE BLResult writeFile(const char* fileName, const BLArrayView<uint8_t>& view, size_t* bytesWrittenOut) noexcept {
  return writeFile(fileName, view.data, view.size, bytesWrittenOut);
}

static BL_INLINE BLResult writeFile(const char* fileName, const BLArray<uint8_t>& array) noexcept {
  return writeFile(fileName, array.view());
}

static BL_INLINE BLResult writeFile(const char* fileName, const BLArray<uint8_t>& array, size_t* bytesWrittenOut) noexcept {
  return writeFile(fileName, array.view(), bytesWrittenOut);
}

} // {BLFileSystem}

//! \}
#endif

//! \}

#endif // BLEND2D_FILESYSTEM_H_INCLUDED
