// Blend2D - 2D Vector Graphics Powered by a JIT Compiler
//
//  * Official Blend2D Home Page: https://blend2d.com
//  * Official Github Repository: https://github.com/blend2d/blend2d
//
// Copyright (c) 2017-2020 The Blend2D Authors
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.

#ifndef BLEND2D_FILESYSTEM_P_H_INCLUDED
#define BLEND2D_FILESYSTEM_P_H_INCLUDED

#include "./filesystem.h"

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

  void* _data;
  size_t _size;

#if defined(_WIN32)
  HANDLE _fileMappingHandle;
#endif

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  BL_INLINE BLFileMapping() noexcept
    : _data(nullptr),
      _size(0)
#if defined(_WIN32)
      , _fileMappingHandle(INVALID_HANDLE_VALUE)
#endif
  {}

  BL_INLINE BLFileMapping(BLFileMapping&& other) noexcept {
    void* data = other._data;
    size_t size = other._size;

    other._data = nullptr;
    other._size = 0;

    this->_data = data;
    this->_size = size;

#if defined(_WIN32)
    HANDLE fileMappingHandle = other._fileMappingHandle;
    other._fileMappingHandle = INVALID_HANDLE_VALUE;
    this->_fileMappingHandle = fileMappingHandle;
#endif
  }

  BL_INLINE ~BLFileMapping() noexcept { unmap(); }

  // --------------------------------------------------------------------------
  // [Accessors]
  // --------------------------------------------------------------------------

  //! Returns whether the mapping is empty (i.e. not file has been mapped).
  BL_NODISCARD
  BL_INLINE bool empty() const noexcept { return _size == 0; }

  //! Returns mapped data casted to `T`.
  template<typename T = void>
  BL_NODISCARD
  BL_INLINE T* data() noexcept { return static_cast<T*>(_data); }

  //! Returns mapped data casted to `T` (const).
  template<typename T = void>
  BL_NODISCARD
  BL_INLINE const T* data() const noexcept { return static_cast<const T*>(_data); }

  //! Returns the size of the mapped data.
  BL_NODISCARD
  BL_INLINE size_t size() const noexcept { return _size; }

#if defined(_WIN32)
  //! Returns a Windows-specific HANDLE of the mapped object.
  BL_NODISCARD
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
    void* data = other._data;
    size_t size = other._size;

    other._data = nullptr;
    other._size = 0;

#if defined(_WIN32)
    HANDLE fileMappingHandle = other._fileMappingHandle;
    other._fileMappingHandle = INVALID_HANDLE_VALUE;
#endif

    unmap();

    this->_data = data;
    this->_size = size;
#if defined(_WIN32)
    this->_fileMappingHandle = fileMappingHandle;
#endif

    return *this;
  }
};

//! \}
//! \endcond

#endif // BLEND2D_FILESYSTEM_P_H_INCLUDED
