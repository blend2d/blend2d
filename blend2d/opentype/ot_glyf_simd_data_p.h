// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_OPENTYPE_OT_GLYF_SIMD_DATA_P_H_INCLUDED
#define BLEND2D_OPENTYPE_OT_GLYF_SIMD_DATA_P_H_INCLUDED

#include <blend2d/opentype/ot_glyf_p.h>

//! \cond INTERNAL
//! \addtogroup blend2d_opentype_impl
//! \{

namespace bl::OpenType::GlyfImpl {

//! Flags that are used by the vectorized outline decoder implementation.
//!
//! Most of the flags are the same as flags used in TT outlines, however, the following modifications were made in
//! order to make the implementation faster:
//!
//!   1. XByte|YByte|XSame|YSame flags were moved to [3:0] bits so they can be used as a predicate with VPSHUFB
//!      instruction. These 4 bits are the only important bits to decode X/Y vertices.
//!   2. XSameOrPositive and YSameOrPositive flags were negated. After negation when all [3:0] bits are zero, the
//!      vertex is zero as well. This is required when processing multiple flags at once at the end. Extra flags
//!      in a loop that processes 8 or 16 flags at a time are zero, thus they don't contribute to X/Y data lengths.
//!   3. OffCurve flag is a negation of OnCurve flag to make some SIMD code easier to combine - for example we need
//!      basically to know PrevOffCurve and OffCurve to know we have to emit a mid-point of a quad-spline. We also
//!      need these two to know whether to emit a command into control buffer.
//!   4. Emit flags instrument the decoder to emit mid-point (needed by off-curve splines) and path commands to a
//!      control buffer.
//!        a) EmitCommand is 1 unless the vertex is OnCurve and the previous vertex is not. All other combinations
//!           mean that a command must be emitted.
//!        b) EmitMidpoint is 1 when OFF is true and PrevOFF is true as well.
//!   5. Repeat flag is last so we can use VPMOVMSKB instruction to quickly check for repeated flags. However, it is
//!      only used during the flag transformation and expansion loop - then the bit at this position is reused for
//!      other flags.

static constexpr uint32_t kVFXByte = 0x01u;              // X flag is byte (not word).
static constexpr uint32_t kVFYByte = 0x02u;              // Y flag is byte (not word).
static constexpr uint32_t kVFXNotSameOrPositive = 0x04u; // X flag is not the same.
static constexpr uint32_t kVFYNotSameOrPositive = 0x08u; // Y flag is not the same.

static constexpr uint32_t kVFOnCurve = 0x10u;            // The point is on-curve (a negation of off-curve for BLPathCmd calculation).
static constexpr uint32_t kVFOffCurve = 0x20u;           // The point is off-curve (part of TT data, just a reordered bit position).

static constexpr uint32_t kVFEmitCommand = 0x40u;        // Whether this vertex emits command into the control buffer.
static constexpr uint32_t kVFEmitMidpoint = 0x80u;       // Whether this vertex emits mid-point before it (off-curve spline support).

static constexpr uint32_t kVFToPathCmdRShift = 2;
static constexpr uint32_t kVFToPathCmdMask = uint8_t(BL_PATH_CMD_LINE | BL_PATH_CMD_QUAD);

// The first 16 bytes are used as a predicate for VPSHUFB/TBL.
alignas(16) extern const uint8_t convert_flags_predicate[16];

// Vertex size for each y_same|x_same|y_byte|x_byte combination.
alignas(16) extern const uint8_t sizes_per_xy_predicate[16];

// These tables provide PSHUFB predicate (and additional payload) for decoding X/Y coordinates based on flags.
// The trick is to add 0x41 to each even byte to create a predicate for both LO and HI byte based on a single
// table. We add values to the LO byte as TT words are stored in big endian, so this trick makes byteswapping
// of the input words for free.
//
// NOTEs:
//   PSHUFB only uses [7] and [3:0] bits, other bits are ignored
//   0x20 means negation of both LO and HI bytes (single byte decode having X/YSameOrPositive == 0).
//   hi = (val     ) & 0x8F
//   lo = (val+0x41) & 0x8F
alignas(16) extern const uint8_t decode_op_x_table[16];
alignas(16) extern const uint8_t decode_op_y_table[16];

} // {bl::OpenType::GlyfImpl}

//! \}
//! \endcond

#endif // BLEND2D_OPENTYPE_OT_GLYF_SIMD_DATA_P_H_INCLUDED
