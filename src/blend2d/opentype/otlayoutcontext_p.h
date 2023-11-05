// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_OPENTYPE_OTLAYOUTCONTEXT_P_H_INCLUDED
#define BLEND2D_OPENTYPE_OTLAYOUTCONTEXT_P_H_INCLUDED

#include "../opentype/otcore_p.h"
#include "../opentype/otlayout_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_opentype_impl
//! \{

namespace bl {
namespace OpenType {

//! A context used for OpenType glyph substitution (GSUB) processing.
struct GSubContext {
  enum class AllocMode {
    //! Allocated buffer replaces the current primary buffer (is flipped), the existing buffer would still be valid
    //! after the allocation. This mode is used by multiple substitution, which calculates the final buffer first;
    //! and then allocates the buffer and fills it with glyphs / infos.
    kCurrent = 0,

    //! Allocated buffer doesn't replace the current buffer. When multiple requests to a separate buffer are made
    //! within a single GSUB lookup, the content of the previous buffer is copied to the new one (this is to handle
    //! outputs where multiple substitution grows the buffer beyond the initial estimate).
    kSeparate = 1
  };

  struct WorkBuffer {
    BLGlyphId* glyphData;
    BLGlyphInfo* infoData;
    size_t size;
    size_t capacity;
  };

  typedef BLResult (BL_CDECL* PrepareOutputBufferFunc)(GSubContext* self, size_t size) BL_NOEXCEPT;

  //! \name Members
  //! \{

  WorkBuffer _workBuffer;
  DebugSink _debugSink;
  PrepareOutputBufferFunc _prepareOutputBuffer;

  //! \}

  //! \name Accessors
  //! \{

  BL_INLINE WorkBuffer& workBuffer() noexcept { return _workBuffer; }
  BL_INLINE const WorkBuffer& workBuffer() const noexcept { return _workBuffer; }

  BL_INLINE BLGlyphId* glyphData() const noexcept { return _workBuffer.glyphData; }
  BL_INLINE BLGlyphInfo* infoData() const noexcept { return _workBuffer.infoData; }

  BL_INLINE bool empty() const noexcept { return _workBuffer.size == 0u; }
  BL_INLINE size_t size() const noexcept { return _workBuffer.size; }
  BL_INLINE size_t capacity() const noexcept { return _workBuffer.capacity; }

  BL_INLINE void truncate(size_t newSize) noexcept {
    BL_ASSERT(newSize <= _workBuffer.size);
    _workBuffer.size = newSize;
  }

  BL_INLINE BLGlyphId* glyphEndData() const noexcept { return _workBuffer.glyphData + _workBuffer.size; }
  BL_INLINE BLGlyphInfo* infoEndData() const noexcept { return _workBuffer.infoData + _workBuffer.size; }

  //! \}

  //! \name Processing
  //! \{

  //! Used by a substitution that substitutes a single glyph or a sequence of glyphs with a longer sequence.
  BL_INLINE BLResult prepareOutputBuffer(size_t size) noexcept {
    return _prepareOutputBuffer(this, size);
  }

  //! Makes sure that there is at least `size` bytes in the work buffer.
  //!
  //! If the capacity of work buffer is sufficient, it's size is set to `size` and nothing else happens, however,
  //! if the capacity of work buffer is not sufficient, a new buffer is created and set as `_workBuffer`. The
  //! caller must remember the pointers to the previous buffer before the call as the buffers will be flipped.
  BL_INLINE BLResult ensureWorkBuffer(size_t size) noexcept {
    if (size > _workBuffer.capacity)
      BL_PROPAGATE(prepareOutputBuffer(size));

    _workBuffer.size = size;
    return BL_SUCCESS;
  }

  //! \}
};

//! A nested GSUB context that is used to process nested lookups.
//!
//! There can only be a single nested context to avoid recursion.
struct GSubContextNested : public GSubContext {
  enum : uint32_t {
    //! Maximum buffer size of a nested GSUB context.
    kNestedStorageSize = 64
  };

  //! Nested buffer having a fixed size.
  struct NestedBuffer {
    BLGlyphId glyphData[kNestedStorageSize];
    BLGlyphInfo infoData[kNestedStorageSize];
  };

  //! \name Members
  //! \{

  //! Two nested buffers, restricted to `kNestedStorageSize`.
  NestedBuffer _nestedBuffers[2];
  //! The id of the next nested buffers, for flipping - every time flip happens `_nextNestedBufferId` is XORed by 1.
  uint32_t _nextNestedBufferId;

  //! \}

  //! \name Initialization
  //! \{

  BL_INLINE void init(BLGlyphBufferPrivateImpl* gbd) noexcept {
    // Initialize GSubContext.
    _debugSink.init(gbd->debugSink, gbd->debugSinkUserData);
    _prepareOutputBuffer = prepareOutputBufferImpl;

    // Initialize GSubContextNested.
    _nextNestedBufferId = 0;
  }

  BL_INLINE void initNested(BLGlyphId* glyphData, BLGlyphInfo* infoData, size_t size) noexcept {
    _workBuffer.glyphData = glyphData;
    _workBuffer.infoData = infoData;
    _workBuffer.size = size;
    _workBuffer.capacity = size;
  }

  //! \}

  //! \name Implementation
  //! \{

  static BLResult BL_CDECL prepareOutputBufferImpl(GSubContext* self_, size_t size) noexcept {
    GSubContextNested* self = static_cast<GSubContextNested*>(self_);
    if (size > kNestedStorageSize)
      return blTraceError(BL_ERROR_GLYPH_SUBSTITUTION_TOO_LARGE);

    NestedBuffer& nested = self->_nestedBuffers[self->_nextNestedBufferId];
    self->_workBuffer.glyphData = nested.glyphData;
    self->_workBuffer.infoData = nested.infoData;
    self->_workBuffer.size = size;
    self->_workBuffer.capacity = kNestedStorageSize;
    self->_nextNestedBufferId ^= 1u;

    return BL_SUCCESS;
  }

  //! \}
};

//! A primary GSUB context that is used to process top-level lookups.
struct GSubContextPrimary : public GSubContext {
  //! \name Members
  //! \{

  BLGlyphBufferPrivateImpl* _gbd;
  GSubContextNested _nested;

  //! \}

  //! \name Initialization
  //! \{

  BL_INLINE void init(BLGlyphBufferPrivateImpl* gbd) noexcept {
    // Initialize GSubContext.
    _debugSink.init(gbd->debugSink, gbd->debugSinkUserData);
    _prepareOutputBuffer = prepareOutputBufferImpl;

    _workBuffer.glyphData = gbd->content;
    _workBuffer.infoData = gbd->infoData;
    _workBuffer.size = gbd->size;
    _workBuffer.capacity = gbd->capacity[0];

    // Initialize GSubContextPrimary.
    _gbd = gbd;
    _nested.init(gbd);
  }

  BL_INLINE void done() noexcept {
    _gbd->size = _workBuffer.size;
  }

  //! \}

  //! \name Implementation
  //! \{

  static BLResult BL_CDECL prepareOutputBufferImpl(GSubContext* self_, size_t size) noexcept {
    GSubContextPrimary* self = static_cast<GSubContextPrimary*>(self_);
    BLGlyphBufferPrivateImpl* gbd = self->_gbd;

    BL_PROPAGATE(gbd->ensureBuffer(1, 0, size));
    gbd->getGlyphDataPtrs(1, &self->_workBuffer.glyphData, &self->_workBuffer.infoData);
    self->_workBuffer.size = size;
    self->_workBuffer.capacity = gbd->capacity[1];
    gbd->flip();

    return BL_SUCCESS;
  }

  //! \}
};

//! A context used for OpenType glyph positioning.
struct GPosContext {
  struct WorkBuffer {
    BLGlyphId* glyphData;
    BLGlyphInfo* infoData;
    BLGlyphPlacement* placementData;
    size_t size;
  };

  WorkBuffer _workBuffer;
  DebugSink _debugSink;
  BLGlyphBufferPrivateImpl* _gbd;

  BL_INLINE void init(BLGlyphBufferPrivateImpl* gbd) noexcept {
    _gbd = gbd;
    _debugSink.init(gbd->debugSink, gbd->debugSinkUserData);

    _workBuffer.glyphData = gbd->content;
    _workBuffer.infoData = gbd->infoData;
    _workBuffer.placementData = gbd->placementData;
    _workBuffer.size = gbd->size;
  }

  BL_INLINE void done() noexcept {}

  BL_INLINE WorkBuffer& workBuffer() noexcept { return _workBuffer; }
  BL_INLINE const WorkBuffer& workBuffer() const noexcept { return _workBuffer; }

  BL_INLINE BLGlyphId* glyphData() const noexcept { return _workBuffer.glyphData; }
  BL_INLINE BLGlyphInfo* infoData() const noexcept { return _workBuffer.infoData; }
  BL_INLINE BLGlyphPlacement* placementData() const noexcept { return _workBuffer.placementData; }

  BL_INLINE bool empty() const noexcept { return _workBuffer.size == 0u; }
  BL_INLINE size_t size() const noexcept { return _workBuffer.size; }
};

} // {OpenType}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_OPENTYPE_OTLAYOUTCONTEXT_P_H_INCLUDED
