// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#ifndef BLEND2D_BLFILESYSTEM_P_H
#define BLEND2D_BLFILESYSTEM_P_H

#include "./blfilesystem.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

// ============================================================================
// [BLFileMapping]
// ============================================================================

enum : uint32_t {
  BL_FILE_SYSTEM_SMALL_FILE_SIZE_THRESHOLD = 16 * 1024
};

//! A thin abstraction over `mmap() / munmap()` (Posix) and `FileMapping` (Windows)
//! to create a read-only file mapping for loading fonts and other resources.
class BLFileMapping {
public:
  BL_NONCOPYABLE(BLFileMapping)

  BLFileCore _file;
#if defined(_WIN32)
  HANDLE _fileMappingHandle;
#endif

  void* _data;
  size_t _size;

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  BL_INLINE BLFileMapping() noexcept
    : _file{-1},
#if defined(_WIN32)
      _fileMappingHandle(INVALID_HANDLE_VALUE),
#endif
      _data(nullptr),
      _size(0) {}

  BL_INLINE BLFileMapping(BLFileMapping&& other) noexcept {
    BLFileCore file = other._file;
    other._file.handle = -1;
    this->_file.handle = file.handle;

#if defined(_WIN32)
    HANDLE fileMappingHandle = other._fileMappingHandle;
    other._fileMappingHandle = INVALID_HANDLE_VALUE;
    this->_fileMappingHandle = fileMappingHandle;
#endif

    void* data = other._data;
    other._data = nullptr;
    this->_data = data;

    size_t size = other._size;
    other._size = 0;
    this->_size = size;
  }

  BL_INLINE ~BLFileMapping() noexcept { unmap(); }

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  //! Returns whether the mapping is empty (i.e. not fille has been mapped).
  BL_INLINE bool empty() const noexcept { return _size == 0; }

  //! Returns mapped data casted to `T`.
  template<typename T = void>
  BL_INLINE T* data() noexcept { return static_cast<T*>(_data); }
  //! Returns mapped data casted to `T` (const).
  template<typename T = void>
  BL_INLINE const T* data() const noexcept { return static_cast<const T*>(_data); }

  //! Returns the size of the mapped data.
  BL_INLINE size_t size() const noexcept { return _size; }

  //! Returns the associated file with the mapping.
  BL_INLINE BLFile& file() noexcept { return blDownCast(_file); }

#if defined(_WIN32)
  //! Returns a Windows-specific HANDLE of file mapping.
  BL_INLINE HANDLE fileMappingHandle() const noexcept { return _fileMappingHandle; }
#endif

  // --------------------------------------------------------------------------
  // [Map / Unmap]
  // --------------------------------------------------------------------------

  //! Maps file `file` to memory. Takes ownership of `file` (moves) on success.
  BL_HIDDEN BLResult map(BLFile& file, size_t size, uint32_t flags = 0) noexcept;

  //! Unmaps previously mapped file or does nothing, if no file was mapped.
  BL_HIDDEN BLResult unmap() noexcept;

  // --------------------------------------------------------------------------
  // [Operator Overload]
  // --------------------------------------------------------------------------

  BL_INLINE BLFileMapping& operator=(BLFileMapping&& other) noexcept {
    BLFileCore file = other._file;
    other._file.handle = -1;

#if defined(_WIN32)
    HANDLE fileMappingHandle = other._fileMappingHandle;
    other._fileMappingHandle = INVALID_HANDLE_VALUE;
#endif

    void* data = other._data;
    other._data = nullptr;

    size_t size = other._size;
    other._size = 0;

    unmap();

    this->_file.handle = file.handle;
#if defined(_WIN32)
    this->_fileMappingHandle = fileMappingHandle;
#endif
    this->_data = data;
    this->_size = size;

    return *this;
  }
};

//! \}
//! \endcond

#endif // BLEND2D_BLFILESYSTEM_P_H
