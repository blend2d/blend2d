// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_FILESYSTEM_H_INCLUDED
#define BLEND2D_FILESYSTEM_H_INCLUDED

#include "array.h"

//! \addtogroup bl_filesystem
//! \{

//! \name BLFile API Constants
//! \{

//! File information flags, used by \ref BLFileInfo.
BL_DEFINE_ENUM(BLFileInfoFlags) {
  //! File owner has read permission (compatible with 0400 octal notation).
  BL_FILE_INFO_OWNER_R = 0x00000100u,
  //! File owner has write permission (compatible with 0200 octal notation).
  BL_FILE_INFO_OWNER_W = 0x00000080u,
  //! File owner has execute permission (compatible with 0100 octal notation).
  BL_FILE_INFO_OWNER_X = 0x00000040u,
  //! A combination of \ref BL_FILE_INFO_OWNER_R, \ref BL_FILE_INFO_OWNER_W, and \ref BL_FILE_INFO_OWNER_X.
  BL_FILE_INFO_OWNER_MASK = 0x000001C0u,

  //! File group owner has read permission (compatible with 040 octal notation).
  BL_FILE_INFO_GROUP_R = 0x00000020u,
  //! File group owner has write permission (compatible with 020 octal notation).
  BL_FILE_INFO_GROUP_W = 0x00000010u,
  //! File group owner has execute permission (compatible with 010 octal notation).
  BL_FILE_INFO_GROUP_X = 0x00000008u,
  //! A combination of \ref BL_FILE_INFO_GROUP_R, \ref BL_FILE_INFO_GROUP_W, and \ref BL_FILE_INFO_GROUP_X.
  BL_FILE_INFO_GROUP_MASK = 0x00000038u,

  //! Other users have read permission (compatible with 04 octal notation).
  BL_FILE_INFO_OTHER_R = 0x00000004u,
  //! Other users have write permission (compatible with 02 octal notation).
  BL_FILE_INFO_OTHER_W = 0x00000002u,
  //! Other users have execute permission (compatible with 01 octal notation).
  BL_FILE_INFO_OTHER_X = 0x00000001u,
  //! A combination of \ref BL_FILE_INFO_OTHER_R, \ref BL_FILE_INFO_OTHER_W, and \ref BL_FILE_INFO_OTHER_X.
  BL_FILE_INFO_OTHER_MASK = 0x00000007u,

  //! Set user ID to file owner user ID on execution (compatible with 04000 octal notation).
  BL_FILE_INFO_SUID = 0x00000800u,
  //! Set group ID to file's user group ID on execution (compatible with 02000 octal notation).
  BL_FILE_INFO_SGID = 0x00000400u,

  //! A combination of all file permission bits.
  BL_FILE_INFO_PERMISSIONS_MASK = 0x00000FFFu,

  //! A flag specifying that this is a regular file.
  BL_FILE_INFO_REGULAR = 0x00010000u,
  //! A flag specifying that this is a directory.
  BL_FILE_INFO_DIRECTORY = 0x00020000u,
  //! A flag specifying that this is a symbolic link.
  BL_FILE_INFO_SYMLINK = 0x00040000u,

  //! A flag describing a character device.
  BL_FILE_INFO_CHAR_DEVICE = 0x00100000u,
  //! A flag describing a block device.
  BL_FILE_INFO_BLOCK_DEVICE = 0x00200000u,
  //! A flag describing a FIFO (named pipe).
  BL_FILE_INFO_FIFO = 0x00400000u,
  //! A flag describing a socket.
  BL_FILE_INFO_SOCKET = 0x00800000u,

  //! A flag describing a hidden file (Windows only).
  BL_FILE_INFO_HIDDEN = 0x01000000u,
  //! A flag describing a hidden file (Windows only).
  BL_FILE_INFO_EXECUTABLE = 0x02000000u,
  //! A flag describing an archive (Windows only).
  BL_FILE_INFO_ARCHIVE = 0x04000000u,
  //! A flag describing a system file (Windows only).
  BL_FILE_INFO_SYSTEM = 0x08000000u,

  //! File information is valid (the request succeeded).
  BL_FILE_INFO_VALID = 0x80000000u

  BL_FORCE_ENUM_UINT32(BL_FILE_INFO)
};

//! File open flags, see \ref BLFile::open().
BL_DEFINE_ENUM(BLFileOpenFlags) {
  //! No flags.
  BL_FILE_OPEN_NO_FLAGS = 0u,

  //! Opens the file for reading.
  //!
  //! The following system flags are used when opening the file:
  //!   - `O_RDONLY` (Posix)
  //!   - `GENERIC_READ` (Windows)
  BL_FILE_OPEN_READ = 0x00000001u,

  //! Opens the file for writing:
  //!
  //! The following system flags are used when opening the file:
  //!   - `O_WRONLY` (Posix)
  //!   - `GENERIC_WRITE` (Windows)
  BL_FILE_OPEN_WRITE = 0x00000002u,

  //! Opens the file for reading & writing.
  //!
  //! The following system flags are used when opening the file:
  //!   - `O_RDWR` (Posix)
  //!   - `GENERIC_READ | GENERIC_WRITE` (Windows)
  BL_FILE_OPEN_RW = 0x00000003u,

  //! Creates the file if it doesn't exist or opens it if it does.
  //!
  //! The following system flags are used when opening the file:
  //!   - `O_CREAT` (Posix)
  //!   - `CREATE_ALWAYS` or `OPEN_ALWAYS` depending on other flags (Windows)
  BL_FILE_OPEN_CREATE = 0x00000004u,

  //! Opens the file for deleting or renaming (Windows).
  //!
  //! Adds `DELETE` flag when opening the file to `ACCESS_MASK`.
  BL_FILE_OPEN_DELETE = 0x00000008u,

  //! Truncates the file.
  //!
  //! The following system flags are used when opening the file:
  //!   - `O_TRUNC` (Posix)
  //!   - `TRUNCATE_EXISTING` (Windows)
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
  //! This is a combination of both `BL_FILE_OPEN_READ_EXCLUSIVE` and `BL_FILE_OPEN_WRITE_EXCLUSIVE`.
  BL_FILE_OPEN_RW_EXCLUSIVE = 0x30000000u,

  //! Creates the file in exclusive mode - fails if the file already exists.
  //!
  //! The following system flags are used when opening the file:
  //!   - `O_EXCL` (Posix)
  //!   - `CREATE_NEW` (Windows)
  BL_FILE_OPEN_CREATE_EXCLUSIVE = 0x40000000u,

  //! Opens the file for deleting or renaming in exclusive mode (Windows).
  //!
  //! Exclusive mode means to not specify the `FILE_SHARE_DELETE` option.
  BL_FILE_OPEN_DELETE_EXCLUSIVE = 0x80000000u

  BL_FORCE_ENUM_UINT32(BL_FILE_OPEN)
};

//! File seek mode, see \ref BLFile::seek().
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

//! File read flags used by \ref BLFileSystem::readFile().
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
  //! \note A handle of value `-1` is considered invalid and/or uninitialized. This value also matches Windows API
  //! `INVALID_HANDLE_VALUE`, which is also defined to be -1.
  intptr_t handle;
};

//! \}

//! \name BLFileInfo Structs
//!
//! \{

//! File information.
struct BLFileInfo {
  //! \name Members
  //! \{

  uint64_t size;
  int64_t modifiedTime;
  BLFileInfoFlags flags;
  uint32_t uid;
  uint32_t gid;
  uint32_t reserved[5];

  //! \}

#if defined(__cplusplus)

  //! \name Accessors
  //! \{

  //! Tests whether the file information has the given `flag` set in `flags`.
  BL_INLINE_NODEBUG bool hasFlag(BLFileInfoFlags flag) const noexcept { return (flags & flag) != 0; }

  BL_INLINE_NODEBUG bool hasOwnerR() const noexcept { return hasFlag(BL_FILE_INFO_OWNER_R); }
  BL_INLINE_NODEBUG bool hasOwnerW() const noexcept { return hasFlag(BL_FILE_INFO_OWNER_W); }
  BL_INLINE_NODEBUG bool hasOwnerX() const noexcept { return hasFlag(BL_FILE_INFO_OWNER_X); }

  BL_INLINE_NODEBUG bool hasGroupR() const noexcept { return hasFlag(BL_FILE_INFO_GROUP_R); }
  BL_INLINE_NODEBUG bool hasGroupW() const noexcept { return hasFlag(BL_FILE_INFO_GROUP_W); }
  BL_INLINE_NODEBUG bool hasGroupX() const noexcept { return hasFlag(BL_FILE_INFO_GROUP_X); }

  BL_INLINE_NODEBUG bool hasOtherR() const noexcept { return hasFlag(BL_FILE_INFO_OTHER_R); }
  BL_INLINE_NODEBUG bool hasOtherW() const noexcept { return hasFlag(BL_FILE_INFO_OTHER_W); }
  BL_INLINE_NODEBUG bool hasOtherX() const noexcept { return hasFlag(BL_FILE_INFO_OTHER_X); }

  BL_INLINE_NODEBUG bool hasSUID() const noexcept { return hasFlag(BL_FILE_INFO_SUID); }
  BL_INLINE_NODEBUG bool hasSGID() const noexcept { return hasFlag(BL_FILE_INFO_SGID); }

  BL_INLINE_NODEBUG bool isRegular() const noexcept { return hasFlag(BL_FILE_INFO_REGULAR); }
  BL_INLINE_NODEBUG bool isDirectory() const noexcept { return hasFlag(BL_FILE_INFO_DIRECTORY); }
  BL_INLINE_NODEBUG bool isSymlink() const noexcept { return hasFlag(BL_FILE_INFO_SYMLINK); }

  BL_INLINE_NODEBUG bool isCharDevice() const noexcept { return hasFlag(BL_FILE_INFO_CHAR_DEVICE); }
  BL_INLINE_NODEBUG bool isBlockDevice() const noexcept { return hasFlag(BL_FILE_INFO_BLOCK_DEVICE); }
  BL_INLINE_NODEBUG bool isFIFO() const noexcept { return hasFlag(BL_FILE_INFO_FIFO); }
  BL_INLINE_NODEBUG bool isSocket() const noexcept { return hasFlag(BL_FILE_INFO_SOCKET); }

  BL_INLINE_NODEBUG bool isHidden() const noexcept { return hasFlag(BL_FILE_INFO_HIDDEN); }
  BL_INLINE_NODEBUG bool isExecutable() const noexcept { return hasFlag(BL_FILE_INFO_EXECUTABLE); }
  BL_INLINE_NODEBUG bool isArchive() const noexcept { return hasFlag(BL_FILE_INFO_ARCHIVE); }
  BL_INLINE_NODEBUG bool isSystem() const noexcept { return hasFlag(BL_FILE_INFO_SYSTEM); }

  BL_INLINE_NODEBUG bool isValid() const noexcept { return hasFlag(BL_FILE_INFO_VALID); }

  //! \}

#endif
};

//! \}

//! \}

//! \addtogroup bl_c_api
//! \{

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
BL_API BLResult BL_CDECL blFileGetInfo(BLFileCore* self, BLFileInfo* infoOut) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFileGetSize(BLFileCore* self, uint64_t* fileSizeOut) BL_NOEXCEPT_C;

//! \}

//! \name BLFileSystem C API Functions
//!
//! \{

BL_API BLResult BL_CDECL blFileSystemGetInfo(const char* fileName, BLFileInfo* infoOut) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFileSystemReadFile(const char* fileName, BLArrayCore* dst, size_t maxSize, BLFileReadFlags readFlags) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blFileSystemWriteFile(const char* fileName, const void* data, size_t size, size_t* bytesWrittenOut) BL_NOEXCEPT_C;

//! \}

BL_END_C_DECLS

//! \}

//! \addtogroup bl_filesystem
//! \{

#ifdef __cplusplus
//! \name BLFile C++ API
//! \{

//! A thin abstraction over a native OS file IO [C++ API].
//!
//! A thin wrapper around a native OS file support. The file handle is always `intptr_t` and it refers to either
//! a file descriptor on POSIX targets and file handle on Windows targets.
class BLFile final : public BLFileCore {
public:
  // Prevent copy-constructor and copy-assignment.
  BL_INLINE_NODEBUG BLFile(const BLFile& other) noexcept = delete;
  BL_INLINE_NODEBUG BLFile& operator=(const BLFile& other) noexcept = delete;

  //! \name Construction & Destruction
  //! \{

  //! Creates an empty file instance, which doesn't represent any open file.
  //!
  //! \note The internal file handle of non-opened files is set to -1.
  BL_INLINE_NODEBUG BLFile() noexcept
    : BLFileCore { -1 } {}

  //! Move constructor - copies file descriptor from `other` to this instance and resets `other` to a default
  //! constructed state.
  BL_INLINE_NODEBUG BLFile(BLFile&& other) noexcept {
    intptr_t h = other.handle;
    other.handle = -1;
    handle = h;
  }

  //! Creates a file instance from an existing file `handle`, which either represents a file descriptor or Windows
  //! `HANDLE` (if compiled for Windows platform).
  BL_INLINE_NODEBUG explicit BLFile(intptr_t handle) noexcept
    : BLFileCore { handle } {}

  BL_INLINE_NODEBUG BLFile& operator=(BLFile&& other) noexcept {
    intptr_t h = other.handle;
    other.handle = -1;

    this->close();
    this->handle = h;

    return *this;
  }

  //! Destroys this file instance - closes the file descriptor or handle when it's referencing an open file.
  BL_INLINE_NODEBUG ~BLFile() noexcept { blFileReset(this); }

  //! \}

  //! \name Common Functionality
  //! \{

  BL_INLINE_NODEBUG void swap(BLFile& other) noexcept { BLInternal::swap(this->handle, other.handle); }

  //! \}

  //! \name Interface
  //! \{

  //! Tests whether the file is open.
  BL_INLINE_NODEBUG bool isOpen() const noexcept { return handle != -1; }

  //! Attempts to open a file specified by `fileName` with the given `openFlags`.
  BL_INLINE_NODEBUG BLResult open(const char* fileName, BLFileOpenFlags openFlags) noexcept {
    return blFileOpen(this, fileName, openFlags);
  }

  //! Closes the file (if open) and sets the file handle to -1.
  BL_INLINE_NODEBUG BLResult close() noexcept {
    return blFileClose(this);
  }

  //! Sets the file position of the file to the given `offset` by using the specified `seekType`.
  BL_INLINE_NODEBUG BLResult seek(int64_t offset, BLFileSeekType seekType) noexcept {
    int64_t positionOut;
    return blFileSeek(this, offset, seekType, &positionOut);
  }

  //! Sets the file position of the file to the given `offset` by using the specified `seekType` and writes the new
  //! position into `positionOut` output parameter.
  BL_INLINE_NODEBUG BLResult seek(int64_t offset, BLFileSeekType seekType, int64_t* positionOut) noexcept {
    return blFileSeek(this, offset, seekType, positionOut);
  }

  //! Reads `n` bytes from the file into the given `buffer` and stores the number of bytes actually read into
  //! the `bytesReadOut` output parameter.
  BL_INLINE_NODEBUG BLResult read(void* buffer, size_t n, size_t* bytesReadOut) noexcept {
    return blFileRead(this, buffer, n, bytesReadOut);
  }

  //! Writes `n` bytes to the file from the given `buffer` and stores the number of bytes actually written into
  //! the `bytesReadOut` output parameter.
  BL_INLINE_NODEBUG BLResult write(const void* buffer, size_t n, size_t* bytesWrittenOut) noexcept {
    return blFileWrite(this, buffer, n, bytesWrittenOut);
  }

  //! Truncates the file to the given maximum size `maxSize`.
  BL_INLINE_NODEBUG BLResult truncate(int64_t maxSize) noexcept {
    return blFileTruncate(this, maxSize);
  }

  //! Queries an information of the file and stores it to `infoOut`.
  BL_INLINE_NODEBUG BLResult getInfo(BLFileInfo* infoOut) noexcept {
    return blFileGetInfo(this, infoOut);
  }

  //! Queries a size of the file and stores it to `sizeOut`.
  BL_INLINE_NODEBUG BLResult getSize(uint64_t* sizeOut) noexcept {
    return blFileGetSize(this, sizeOut);
  }

  //! \}
};

//! File-system utilities.
namespace BLFileSystem {

static BL_INLINE_NODEBUG BLResult fileInfo(const char* fileName, BLFileInfo* infoOut) noexcept {
  return blFileSystemGetInfo(fileName, infoOut);
}

//! Reads a file into the `dst` buffer.
//!
//! Optionally you can set `maxSize` to non-zero value that would restrict the maximum bytes to read to such value.
//! In addition, `readFlags` can be used to enable file mapping. See \ref BLFileReadFlags for more details.
static BL_INLINE_NODEBUG BLResult readFile(const char* fileName, BLArray<uint8_t>& dst, size_t maxSize = 0, BLFileReadFlags readFlags = BL_FILE_READ_NO_FLAGS) noexcept {
  return blFileSystemReadFile(fileName, &dst, maxSize, readFlags);
}

static BL_INLINE_NODEBUG BLResult writeFile(const char* fileName, const void* data, size_t size) noexcept {
  size_t bytesWrittenOut;
  return blFileSystemWriteFile(fileName, data, size, &bytesWrittenOut);
}

static BL_INLINE_NODEBUG BLResult writeFile(const char* fileName, const void* data, size_t size, size_t* bytesWrittenOut) noexcept {
  return blFileSystemWriteFile(fileName, data, size, bytesWrittenOut);
}

static BL_INLINE_NODEBUG BLResult writeFile(const char* fileName, const BLArrayView<uint8_t>& view) noexcept {
  return writeFile(fileName, view.data, view.size);
}

static BL_INLINE_NODEBUG BLResult writeFile(const char* fileName, const BLArrayView<uint8_t>& view, size_t* bytesWrittenOut) noexcept {
  return writeFile(fileName, view.data, view.size, bytesWrittenOut);
}

static BL_INLINE_NODEBUG BLResult writeFile(const char* fileName, const BLArray<uint8_t>& array) noexcept {
  return writeFile(fileName, array.view());
}

static BL_INLINE_NODEBUG BLResult writeFile(const char* fileName, const BLArray<uint8_t>& array, size_t* bytesWrittenOut) noexcept {
  return writeFile(fileName, array.view(), bytesWrittenOut);
}

} // {BLFileSystem}

//! \}
#endif

//! \}

#endif // BLEND2D_FILESYSTEM_H_INCLUDED
