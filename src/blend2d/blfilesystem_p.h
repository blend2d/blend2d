// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_BLFILESYSTEM_P_H
#define BLEND2D_BLFILESYSTEM_P_H

#include "./blfilesystem.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

// ============================================================================
// [BLFileMapping]
// ============================================================================

//! A thin abstraction over `mmap() / munmap()` (Posix) and `FileMapping` (Windows)
//! to create a read-only file mapping for loading fonts and other resources.
class BLFileMapping {
public:
  BLFileCore _file;

  #if defined(_WIN32)
  HANDLE _fileMappingHandle;
  #endif

  void* _data;
  size_t _size;
  uint32_t _status;

  // Prevent copy-constructor and copy-assignment.
  BL_INLINE BLFileMapping(const BLFileMapping& other) noexcept = delete;
  BL_INLINE BLFileMapping& operator=(const BLFileMapping& other) noexcept = delete;

  enum Limits : uint32_t {
    //! Size threshold (32kB) used when `kCopySmallFiles` is enabled.
    kSmallFileSize = 1024 * 32
  };

  enum Status : uint32_t {
    //! File is empty (not mapped nor copied).
    kStatusEmpty = 0,
    //! File is memory-mapped.
    kStatusMapped = 1,
    //! File was copied.
    kStatusCopied = 2
  };

  enum Flags : uint32_t {
    //! Copy the file data if it's smaller/equal to `kSmallFileSize.
    kCopySmallFiles       = 0x01000000u,
    //! Copy the file data if mapping is not possible or it failed.
    kCopyOnFailure        = 0x02000000u,
    //! Don't close the `file` handle if the file was copied.
    kDontCloseOnCopy      = 0x04000000u
  };

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  #if defined(_WIN32)
  BL_INLINE BLFileMapping() noexcept
    : _file(),
      _fileMappingHandle(INVALID_HANDLE_VALUE),
      _data(nullptr),
      _size(0),
      _status(kStatusEmpty) {}

  BL_INLINE BLFileMapping(BLFileMapping&& other) noexcept
    : _file(std::move(other._file)),
      _fileMappingHandle(other._fileMappingHandle),
      _data(other._data),
      _size(other._size),
      _status(other._status) {
    other._fileMappingHandle = INVALID_HANDLE_VALUE;
    other._data = nullptr;
    other._size = 0;
    other._status = kStatusEmpty;
  }
  #else
  BL_INLINE BLFileMapping() noexcept
    : _data(nullptr),
      _size(0),
      _status(kStatusEmpty) {}

  BL_INLINE BLFileMapping(BLFileMapping&& other) noexcept
    : _data(other._data),
      _size(other._size),
      _status(other._status) {
    other._data = nullptr;
    other._size = 0;
    other._status = kStatusEmpty;
  }
  #endif

  BL_INLINE ~BLFileMapping() noexcept { unmap(); }

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  //! Get the status of the BLFileMapping, see `Status.
  BL_INLINE uint32_t status() const noexcept { return _status; }

  //! Get whether the BLFileMapping has mapped data.
  BL_INLINE bool isMapped() const noexcept { return _status == kStatusMapped; }
  //! Get whether the BLFileMapping has copied data.
  BL_INLINE bool isCopied() const noexcept { return _status == kStatusCopied; }

  //! Get whether the BLFileMapping is empty (no file mapped or loaded).
  BL_INLINE bool empty() const noexcept { return _status == kStatusEmpty; }
  //! Get whether the BLFileMapping has mapped or copied data.
  BL_INLINE bool hasData() const noexcept { return _status != kStatusEmpty; }

  //! Get raw data to the BLFileMapping content..
  template<typename T = void>
  BL_INLINE const T* data() const noexcept { return static_cast<T*>(_data); }

  //! Get the size of mapped or copied data.
  BL_INLINE size_t size() const noexcept { return _size; }

  //! Get the associated `BLFile` with `BLFileMapping` (Windows).
  BL_INLINE BLFile& file() noexcept { return blDownCast(_file); }

  #if defined(_WIN32)
  //! Get the BLFileMapping handle (Windows).
  BL_INLINE HANDLE fileMappingHandle() const noexcept { return _fileMappingHandle; }
  #endif

  // --------------------------------------------------------------------------
  // [Map / Unmap]
  // --------------------------------------------------------------------------

  //! Maps file `file` to memory. Takes ownership of `file` (moves) on success.
  BL_HIDDEN BLResult map(BLFile& file, uint32_t flags) noexcept;

  //! Unmaps previously mapped file or does nothing, if no file was mapped.
  BL_HIDDEN BLResult unmap() noexcept;

  // --------------------------------------------------------------------------
  // [Operator Overload]
  // --------------------------------------------------------------------------

  BL_INLINE BLFileMapping& operator=(BLFileMapping&& other) noexcept {
    unmap();

    #if defined(_WIN32)
    _file = std::move(other._file);
    _fileMappingHandle = other._fileMappingHandle;
    other._fileMappingHandle = INVALID_HANDLE_VALUE;
    #endif

    _data = other._data;
    _size = other._size;
    _status = other._status;

    other._data = nullptr;
    other._size = 0;
    other._status = kStatusEmpty;

    return *this;
  }
};

//! \}
//! \endcond

#endif // BLEND2D_BLFILESYSTEM_P_H
