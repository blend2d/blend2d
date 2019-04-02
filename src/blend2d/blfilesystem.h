// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_BLFILESYSTEM_H
#define BLEND2D_BLFILESYSTEM_H

#include "./blarray.h"

//! \addtogroup blend2d_api_filesystem
//! \{

// ============================================================================
// [Constants]
// ============================================================================

//! File open flags, see `BLFile::open()`.
BL_DEFINE_ENUM(BLFileOpenFlags) {
  //! Open file for reading (O_RDONLY).
  BL_FILE_OPEN_READ = 0x00000001u,
  //! Open file for writing (O_WRONLY).
  BL_FILE_OPEN_WRITE = 0x00000002u,
  //! Open file for reading & writing (O_RDWR).
  BL_FILE_OPEN_RW = 0x00000003u,
  //! Create the file if it doesn't exist (O_CREAT).
  BL_FILE_OPEN_CREATE = 0x00000004u,
  //! Always create the file, fail if it already exists (O_EXCL).
  BL_FILE_OPEN_CREATE_ONLY = 0x00000008u,
  //! Truncate the file (O_TRUNC).
  BL_FILE_OPEN_TRUNCATE = 0x00000010u,
  //! Enables FILE_SHARE_READ option (Windows).
  BL_FILE_OPEN_SHARE_READ = 0x10000000u,
  //! Enables FILE_SHARE_WRITE option (Windows).
  BL_FILE_OPEN_SHARE_WRITE = 0x20000000u,
  //! Enables both FILE_SHARE_READ and FILE_SHARE_WRITE options (Windows).
  BL_FILE_OPEN_SHARE_RW = 0x30000000u,
  //! Enables FILE_SHARE_DELETE option (Windows).
  BL_FILE_OPEN_SHARE_DELETE = 0x40000000u
};

//! File seek mode, see `BLFile::seek()`.
//!
//! NOTE: Seek constants should be compatible with constants used by both POSIX
//! and Windows API.
BL_DEFINE_ENUM(BLFileSeek) {
  //! Seek from the beginning of the file (SEEK_SET).
  BL_FILE_SEEK_SET = 0,
  //! Seek from the current position (SEEK_CUR).
  BL_FILE_SEEK_CUR = 1,
  //! Seek from the end of the file (SEEK_END).
  BL_FILE_SEEK_END = 2,

  //! Count of seek modes.
  BL_FILE_SEEK_COUNT = 3
};

// ============================================================================
// [BLFile - Core]
// ============================================================================

//! A thin abstraction over a native OS file IO [C Interface - Core].
struct BLFileCore {
  //! A file handle - either a file descriptor used by POSIX or file handle used
  //! by Windows. On both platforms the handle is always `intptr_t` to make FFI
  //! easier (it's basically the size of a pointer / machine register).
  //!
  //! NOTE: In C++ mode you can use `BLFileCore::Handle` or `BLFile::Handle` to
  //! get the handle type. In C mode you must use `intptr_t`. A handle of value
  //! `-1` is considered invalid and/or uninitialized. This value also matches
  //! `INVALID_HANDLE_VALUE`, which is used by Windows API and defined to -1 as
  //! well.
  intptr_t handle;
};

// ============================================================================
// [BLFile - C++]
// ============================================================================

#ifdef __cplusplus
//! A thin abstraction over a native OS file IO [C++ API].
//!
//! A thin wrapper around a native OS file support. The file handle is always
//! `intptr_t` and it refers to either a file descriptor on POSIX targets and
//! file handle on Windows targets.
class BLFile : public BLFileCore {
public:
  // Prevent copy-constructor and copy-assignment.
  BL_INLINE BLFile(const BLFile& other) noexcept = delete;
  BL_INLINE BLFile& operator=(const BLFile& other) noexcept = delete;

  BL_INLINE BLFile() noexcept
    : BLFileCore { -1 } {}

  BL_INLINE BLFile(BLFile&& other) noexcept
    : BLFileCore { other.handle } { other.handle = -1; }

  BL_INLINE explicit BLFile(intptr_t handle) noexcept
    : BLFileCore { handle } {}

  BL_INLINE BLFile& operator=(BLFile&& other) noexcept {
    intptr_t h = other.handle;

    this->close();
    this->handle = h;
    other.handle = -1;

    return *this;
  }

  BL_INLINE ~BLFile() noexcept { blFileReset(this); }

  //! Get whether the file is open.
  BL_INLINE bool isOpen() const noexcept { return handle != -1; }

  //! Take the file handle and set the handle to invalid. After this operation
  //! you will be the sole owner of the handle and you will be responsible for
  //! closing it.
  BL_INLINE intptr_t takeHandle() noexcept {
    intptr_t h = this->handle;
    this->handle = -1;
    return h;
  }

  BL_INLINE BLResult open(const char* fileName, uint32_t openFlags) noexcept {
    return blFileOpen(this, fileName, openFlags);
  }

  BL_INLINE BLResult close() noexcept {
    return blFileClose(this);
  }

  BL_INLINE BLResult seek(int64_t offset, uint32_t seekType) noexcept {
    int64_t positionOut;
    return blFileSeek(this, offset, seekType, &positionOut);
  }

  BL_INLINE BLResult seek(int64_t offset, uint32_t seekType, int64_t* positionOut) noexcept {
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
};
#endif

// ============================================================================
// [BLFileSystem]
// ============================================================================

#ifdef __cplusplus
//! File-system utilities.
namespace BLFileSystem {
  static BL_INLINE BLResult readFile(const char* fileName, BLArray<uint8_t>& dst, size_t maxSize = 0) noexcept {
    return blFileSystemReadFile(fileName, &dst, maxSize);
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
}
#endif

//! \}

#endif // BLEND2D_BLFILESYSTEM_H
