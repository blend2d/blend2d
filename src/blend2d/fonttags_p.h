// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_FONTTAGTABLES_P_H_INCLUDED
#define BLEND2D_FONTTAGTABLES_P_H_INCLUDED

#include "api-internal_p.h"
#include "fontfeaturesettings.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

namespace BLFontTagsPrivate {

static constexpr uint32_t kCharRangeInTag = 95;

//! Number of unique tags.
//!
//! This constant is used as a maximum capacity of containers that store tag to value mapping. There is 95 characters
//! between ' ' (32) and 126 (~), which are allowed in tags, we just need to power it by 4 to get the number of all
//! combinations.
static constexpr uint32_t kUniqueTagCount = kCharRangeInTag * kCharRangeInTag * kCharRangeInTag * kCharRangeInTag;

//! Invalid feature or variation id.
//!
//! Returned by tag to id mapping functions.
static constexpr uint32_t kInvalidId = 0xFFFFFFFFu;

//! Internal font feature identifiers that can be used as small indexes for SSO containers and BitArrays.
enum BLFontFeatureId : uint32_t {
  BL_FONT_FEATURE_ID_AALT, //!< Access All Alternates.
  BL_FONT_FEATURE_ID_ABVF, //!< Above-base Forms.
  BL_FONT_FEATURE_ID_ABVM, //!< Above-base Mark Positioning.
  BL_FONT_FEATURE_ID_ABVS, //!< Above-base Substitutions.
  BL_FONT_FEATURE_ID_AFRC, //!< Alternative Fractions.
  BL_FONT_FEATURE_ID_AKHN, //!< Akhand.
  BL_FONT_FEATURE_ID_BLWF, //!< Below-base Forms.
  BL_FONT_FEATURE_ID_BLWM, //!< Below-base Mark Positioning.
  BL_FONT_FEATURE_ID_BLWS, //!< Below-base Substitutions.
  BL_FONT_FEATURE_ID_C2PC, //!< Petite Capitals From Capitals.
  BL_FONT_FEATURE_ID_C2SC, //!< Small Capitals From Capitals.
  BL_FONT_FEATURE_ID_CALT, //!< Contextual Alternates.
  BL_FONT_FEATURE_ID_CASE, //!< Case-Sensitive Forms.
  BL_FONT_FEATURE_ID_CCMP, //!< Glyph Composition / Decomposition.
  BL_FONT_FEATURE_ID_CFAR, //!< Conjunct Form After Ro.
  BL_FONT_FEATURE_ID_CHWS, //!< Contextual Half-width Spacing.
  BL_FONT_FEATURE_ID_CJCT, //!< Conjunct Forms.
  BL_FONT_FEATURE_ID_CLIG, //!< Contextual Ligatures.
  BL_FONT_FEATURE_ID_CPCT, //!< Centered CJK Punctuation.
  BL_FONT_FEATURE_ID_CPSP, //!< Capital Spacing.
  BL_FONT_FEATURE_ID_CSWH, //!< Contextual Swash.
  BL_FONT_FEATURE_ID_CURS, //!< Cursive Positioning.
  BL_FONT_FEATURE_ID_CV01, //!< Character Variant 1.
  BL_FONT_FEATURE_ID_CV02, //!< Character Variant 2.
  BL_FONT_FEATURE_ID_CV03, //!< Character Variant 3.
  BL_FONT_FEATURE_ID_CV04, //!< Character Variant 4.
  BL_FONT_FEATURE_ID_CV05, //!< Character Variant 5.
  BL_FONT_FEATURE_ID_CV06, //!< Character Variant 6.
  BL_FONT_FEATURE_ID_CV07, //!< Character Variant 7.
  BL_FONT_FEATURE_ID_CV08, //!< Character Variant 8.
  BL_FONT_FEATURE_ID_CV09, //!< Character Variant 9.
  BL_FONT_FEATURE_ID_CV10, //!< Character Variant 10.
  BL_FONT_FEATURE_ID_CV11, //!< Character Variant 11.
  BL_FONT_FEATURE_ID_CV12, //!< Character Variant 12.
  BL_FONT_FEATURE_ID_CV13, //!< Character Variant 13.
  BL_FONT_FEATURE_ID_CV14, //!< Character Variant 14.
  BL_FONT_FEATURE_ID_CV15, //!< Character Variant 15.
  BL_FONT_FEATURE_ID_CV16, //!< Character Variant 16.
  BL_FONT_FEATURE_ID_CV17, //!< Character Variant 17.
  BL_FONT_FEATURE_ID_CV18, //!< Character Variant 18.
  BL_FONT_FEATURE_ID_CV19, //!< Character Variant 19.
  BL_FONT_FEATURE_ID_CV20, //!< Character Variant 20.
  BL_FONT_FEATURE_ID_CV21, //!< Character Variant 21.
  BL_FONT_FEATURE_ID_CV22, //!< Character Variant 22.
  BL_FONT_FEATURE_ID_CV23, //!< Character Variant 23.
  BL_FONT_FEATURE_ID_CV24, //!< Character Variant 24.
  BL_FONT_FEATURE_ID_CV25, //!< Character Variant 25.
  BL_FONT_FEATURE_ID_CV26, //!< Character Variant 26.
  BL_FONT_FEATURE_ID_CV27, //!< Character Variant 27.
  BL_FONT_FEATURE_ID_CV28, //!< Character Variant 28.
  BL_FONT_FEATURE_ID_CV29, //!< Character Variant 29.
  BL_FONT_FEATURE_ID_CV30, //!< Character Variant 30.
  BL_FONT_FEATURE_ID_CV31, //!< Character Variant 31.
  BL_FONT_FEATURE_ID_CV32, //!< Character Variant 32.
  BL_FONT_FEATURE_ID_CV33, //!< Character Variant 33.
  BL_FONT_FEATURE_ID_CV34, //!< Character Variant 34.
  BL_FONT_FEATURE_ID_CV35, //!< Character Variant 35.
  BL_FONT_FEATURE_ID_CV36, //!< Character Variant 36.
  BL_FONT_FEATURE_ID_CV37, //!< Character Variant 37.
  BL_FONT_FEATURE_ID_CV38, //!< Character Variant 38.
  BL_FONT_FEATURE_ID_CV39, //!< Character Variant 39.
  BL_FONT_FEATURE_ID_CV40, //!< Character Variant 40.
  BL_FONT_FEATURE_ID_CV41, //!< Character Variant 41.
  BL_FONT_FEATURE_ID_CV42, //!< Character Variant 42.
  BL_FONT_FEATURE_ID_CV43, //!< Character Variant 43.
  BL_FONT_FEATURE_ID_CV44, //!< Character Variant 44.
  BL_FONT_FEATURE_ID_CV45, //!< Character Variant 45.
  BL_FONT_FEATURE_ID_CV46, //!< Character Variant 46.
  BL_FONT_FEATURE_ID_CV47, //!< Character Variant 47.
  BL_FONT_FEATURE_ID_CV48, //!< Character Variant 48.
  BL_FONT_FEATURE_ID_CV49, //!< Character Variant 49.
  BL_FONT_FEATURE_ID_CV50, //!< Character Variant 50.
  BL_FONT_FEATURE_ID_CV51, //!< Character Variant 51.
  BL_FONT_FEATURE_ID_CV52, //!< Character Variant 52.
  BL_FONT_FEATURE_ID_CV53, //!< Character Variant 53.
  BL_FONT_FEATURE_ID_CV54, //!< Character Variant 54.
  BL_FONT_FEATURE_ID_CV55, //!< Character Variant 55.
  BL_FONT_FEATURE_ID_CV56, //!< Character Variant 56.
  BL_FONT_FEATURE_ID_CV57, //!< Character Variant 57.
  BL_FONT_FEATURE_ID_CV58, //!< Character Variant 58.
  BL_FONT_FEATURE_ID_CV59, //!< Character Variant 59.
  BL_FONT_FEATURE_ID_CV60, //!< Character Variant 60.
  BL_FONT_FEATURE_ID_CV61, //!< Character Variant 61.
  BL_FONT_FEATURE_ID_CV62, //!< Character Variant 62.
  BL_FONT_FEATURE_ID_CV63, //!< Character Variant 63.
  BL_FONT_FEATURE_ID_CV64, //!< Character Variant 64.
  BL_FONT_FEATURE_ID_CV65, //!< Character Variant 65.
  BL_FONT_FEATURE_ID_CV66, //!< Character Variant 66.
  BL_FONT_FEATURE_ID_CV67, //!< Character Variant 67.
  BL_FONT_FEATURE_ID_CV68, //!< Character Variant 68.
  BL_FONT_FEATURE_ID_CV69, //!< Character Variant 69.
  BL_FONT_FEATURE_ID_CV70, //!< Character Variant 70.
  BL_FONT_FEATURE_ID_CV71, //!< Character Variant 71.
  BL_FONT_FEATURE_ID_CV72, //!< Character Variant 72.
  BL_FONT_FEATURE_ID_CV73, //!< Character Variant 73.
  BL_FONT_FEATURE_ID_CV74, //!< Character Variant 74.
  BL_FONT_FEATURE_ID_CV75, //!< Character Variant 75.
  BL_FONT_FEATURE_ID_CV76, //!< Character Variant 76.
  BL_FONT_FEATURE_ID_CV77, //!< Character Variant 77.
  BL_FONT_FEATURE_ID_CV78, //!< Character Variant 78.
  BL_FONT_FEATURE_ID_CV79, //!< Character Variant 79.
  BL_FONT_FEATURE_ID_CV80, //!< Character Variant 80.
  BL_FONT_FEATURE_ID_CV81, //!< Character Variant 81.
  BL_FONT_FEATURE_ID_CV82, //!< Character Variant 82.
  BL_FONT_FEATURE_ID_CV83, //!< Character Variant 83.
  BL_FONT_FEATURE_ID_CV84, //!< Character Variant 84.
  BL_FONT_FEATURE_ID_CV85, //!< Character Variant 85.
  BL_FONT_FEATURE_ID_CV86, //!< Character Variant 86.
  BL_FONT_FEATURE_ID_CV87, //!< Character Variant 87.
  BL_FONT_FEATURE_ID_CV88, //!< Character Variant 88.
  BL_FONT_FEATURE_ID_CV89, //!< Character Variant 89.
  BL_FONT_FEATURE_ID_CV90, //!< Character Variant 90.
  BL_FONT_FEATURE_ID_CV91, //!< Character Variant 91.
  BL_FONT_FEATURE_ID_CV92, //!< Character Variant 92.
  BL_FONT_FEATURE_ID_CV93, //!< Character Variant 93.
  BL_FONT_FEATURE_ID_CV94, //!< Character Variant 94.
  BL_FONT_FEATURE_ID_CV95, //!< Character Variant 95.
  BL_FONT_FEATURE_ID_CV96, //!< Character Variant 96.
  BL_FONT_FEATURE_ID_CV97, //!< Character Variant 97.
  BL_FONT_FEATURE_ID_CV98, //!< Character Variant 98.
  BL_FONT_FEATURE_ID_CV99, //!< Character Variant 99.
  BL_FONT_FEATURE_ID_DIST, //!< Distances.
  BL_FONT_FEATURE_ID_DLIG, //!< Discretionary Ligatures.
  BL_FONT_FEATURE_ID_DNOM, //!< Denominators.
  BL_FONT_FEATURE_ID_DTLS, //!< Dotless Forms.
  BL_FONT_FEATURE_ID_EXPT, //!< Expert Forms.
  BL_FONT_FEATURE_ID_FALT, //!< Final Glyph on Line Alternates.
  BL_FONT_FEATURE_ID_FIN2, //!< Terminal Forms #2.
  BL_FONT_FEATURE_ID_FIN3, //!< Terminal Forms #3.
  BL_FONT_FEATURE_ID_FINA, //!< Terminal Forms.
  BL_FONT_FEATURE_ID_FLAC, //!< Flattened accent forms.
  BL_FONT_FEATURE_ID_FRAC, //!< Fractions.
  BL_FONT_FEATURE_ID_FWID, //!< Full Widths.
  BL_FONT_FEATURE_ID_HALF, //!< Half Forms.
  BL_FONT_FEATURE_ID_HALN, //!< Halant Forms.
  BL_FONT_FEATURE_ID_HALT, //!< Alternate Half Widths.
  BL_FONT_FEATURE_ID_HIST, //!< Historical Forms.
  BL_FONT_FEATURE_ID_HKNA, //!< Horizontal Kana Alternates.
  BL_FONT_FEATURE_ID_HLIG, //!< Historical Ligatures.
  BL_FONT_FEATURE_ID_HNGL, //!< Hangul.
  BL_FONT_FEATURE_ID_HOJO, //!< Hojo Kanji Forms (JIS X 0212-1990 Kanji Forms).
  BL_FONT_FEATURE_ID_HWID, //!< Half Widths.
  BL_FONT_FEATURE_ID_INIT, //!< Initial Forms.
  BL_FONT_FEATURE_ID_ISOL, //!< Isolated Forms.
  BL_FONT_FEATURE_ID_ITAL, //!< Italics.
  BL_FONT_FEATURE_ID_JALT, //!< Justification Alternates.
  BL_FONT_FEATURE_ID_JP04, //!< JIS2004 Forms.
  BL_FONT_FEATURE_ID_JP78, //!< JIS78 Forms.
  BL_FONT_FEATURE_ID_JP83, //!< JIS83 Forms.
  BL_FONT_FEATURE_ID_JP90, //!< JIS90 Forms.
  BL_FONT_FEATURE_ID_KERN, //!< Kerning.
  BL_FONT_FEATURE_ID_LFBD, //!< Left Bounds.
  BL_FONT_FEATURE_ID_LIGA, //!< Standard Ligatures.
  BL_FONT_FEATURE_ID_LJMO, //!< Leading Jamo Forms.
  BL_FONT_FEATURE_ID_LNUM, //!< Lining Figures.
  BL_FONT_FEATURE_ID_LOCL, //!< Localized Forms.
  BL_FONT_FEATURE_ID_LTRA, //!< Left-to-right alternates.
  BL_FONT_FEATURE_ID_LTRM, //!< Left-to-right mirrored forms.
  BL_FONT_FEATURE_ID_MARK, //!< Mark Positioning.
  BL_FONT_FEATURE_ID_MED2, //!< Medial Forms #2.
  BL_FONT_FEATURE_ID_MEDI, //!< Medial Forms.
  BL_FONT_FEATURE_ID_MGRK, //!< Mathematical Greek.
  BL_FONT_FEATURE_ID_MKMK, //!< Mark to Mark Positioning.
  BL_FONT_FEATURE_ID_MSET, //!< Mark Positioning via Substitution.
  BL_FONT_FEATURE_ID_NALT, //!< Alternate Annotation Forms.
  BL_FONT_FEATURE_ID_NLCK, //!< NLC Kanji Forms.
  BL_FONT_FEATURE_ID_NUKT, //!< Nukta Forms.
  BL_FONT_FEATURE_ID_NUMR, //!< Numerators.
  BL_FONT_FEATURE_ID_ONUM, //!< Oldstyle Figures.
  BL_FONT_FEATURE_ID_OPBD, //!< Optical Bounds.
  BL_FONT_FEATURE_ID_ORDN, //!< Ordinals.
  BL_FONT_FEATURE_ID_ORNM, //!< Ornaments.
  BL_FONT_FEATURE_ID_PALT, //!< Proportional Alternate Widths.
  BL_FONT_FEATURE_ID_PCAP, //!< Petite Capitals.
  BL_FONT_FEATURE_ID_PKNA, //!< Proportional Kana.
  BL_FONT_FEATURE_ID_PNUM, //!< Proportional Figures.
  BL_FONT_FEATURE_ID_PREF, //!< Pre-Base Forms.
  BL_FONT_FEATURE_ID_PRES, //!< Pre-base Substitutions.
  BL_FONT_FEATURE_ID_PSTF, //!< Post-base Forms.
  BL_FONT_FEATURE_ID_PSTS, //!< Post-base Substitutions.
  BL_FONT_FEATURE_ID_PWID, //!< Proportional Widths.
  BL_FONT_FEATURE_ID_QWID, //!< Quarter Widths.
  BL_FONT_FEATURE_ID_RAND, //!< Randomize.
  BL_FONT_FEATURE_ID_RCLT, //!< Required Contextual Alternates.
  BL_FONT_FEATURE_ID_RKRF, //!< Rakar Forms.
  BL_FONT_FEATURE_ID_RLIG, //!< Required Ligatures.
  BL_FONT_FEATURE_ID_RPHF, //!< Reph Forms.
  BL_FONT_FEATURE_ID_RTBD, //!< Right Bounds.
  BL_FONT_FEATURE_ID_RTLA, //!< Right-to-left alternates.
  BL_FONT_FEATURE_ID_RTLM, //!< Right-to-left mirrored forms.
  BL_FONT_FEATURE_ID_RUBY, //!< Ruby Notation Forms.
  BL_FONT_FEATURE_ID_RVRN, //!< Required Variation Alternates.
  BL_FONT_FEATURE_ID_SALT, //!< Stylistic Alternates.
  BL_FONT_FEATURE_ID_SINF, //!< Scientific Inferiors.
  BL_FONT_FEATURE_ID_SIZE, //!< Optical size.
  BL_FONT_FEATURE_ID_SMCP, //!< Small Capitals.
  BL_FONT_FEATURE_ID_SMPL, //!< Simplified Forms.
  BL_FONT_FEATURE_ID_SS01, //!< Stylistic Set 1.
  BL_FONT_FEATURE_ID_SS02, //!< Stylistic Set 2.
  BL_FONT_FEATURE_ID_SS03, //!< Stylistic Set 3.
  BL_FONT_FEATURE_ID_SS04, //!< Stylistic Set 4.
  BL_FONT_FEATURE_ID_SS05, //!< Stylistic Set 5.
  BL_FONT_FEATURE_ID_SS06, //!< Stylistic Set 6.
  BL_FONT_FEATURE_ID_SS07, //!< Stylistic Set 7.
  BL_FONT_FEATURE_ID_SS08, //!< Stylistic Set 8.
  BL_FONT_FEATURE_ID_SS09, //!< Stylistic Set 9.
  BL_FONT_FEATURE_ID_SS10, //!< Stylistic Set 10.
  BL_FONT_FEATURE_ID_SS11, //!< Stylistic Set 11.
  BL_FONT_FEATURE_ID_SS12, //!< Stylistic Set 12.
  BL_FONT_FEATURE_ID_SS13, //!< Stylistic Set 13.
  BL_FONT_FEATURE_ID_SS14, //!< Stylistic Set 14.
  BL_FONT_FEATURE_ID_SS15, //!< Stylistic Set 15.
  BL_FONT_FEATURE_ID_SS16, //!< Stylistic Set 16.
  BL_FONT_FEATURE_ID_SS17, //!< Stylistic Set 17.
  BL_FONT_FEATURE_ID_SS18, //!< Stylistic Set 18.
  BL_FONT_FEATURE_ID_SS19, //!< Stylistic Set 19.
  BL_FONT_FEATURE_ID_SS20, //!< Stylistic Set 20.
  BL_FONT_FEATURE_ID_SSTY, //!< Math script style alternates.
  BL_FONT_FEATURE_ID_STCH, //!< Stretching Glyph Decomposition.
  BL_FONT_FEATURE_ID_SUBS, //!< Subscript.
  BL_FONT_FEATURE_ID_SUPS, //!< Superscript.
  BL_FONT_FEATURE_ID_SWSH, //!< Swash.
  BL_FONT_FEATURE_ID_TITL, //!< Titling.
  BL_FONT_FEATURE_ID_TJMO, //!< Trailing Jamo Forms.
  BL_FONT_FEATURE_ID_TNAM, //!< Traditional Name Forms.
  BL_FONT_FEATURE_ID_TNUM, //!< Tabular Figures.
  BL_FONT_FEATURE_ID_TRAD, //!< Traditional Forms.
  BL_FONT_FEATURE_ID_TWID, //!< Third Widths.
  BL_FONT_FEATURE_ID_UNIC, //!< Unicase.
  BL_FONT_FEATURE_ID_VALT, //!< Alternate Vertical Metrics.
  BL_FONT_FEATURE_ID_VATU, //!< Vattu Variants.
  BL_FONT_FEATURE_ID_VCHW, //!< Vertical Contextual Half-width Spacing.
  BL_FONT_FEATURE_ID_VERT, //!< Vertical Writing.
  BL_FONT_FEATURE_ID_VHAL, //!< Alternate Vertical Half Metrics.
  BL_FONT_FEATURE_ID_VJMO, //!< Vowel Jamo Forms.
  BL_FONT_FEATURE_ID_VKNA, //!< Vertical Kana Alternates.
  BL_FONT_FEATURE_ID_VKRN, //!< Vertical Kerning.
  BL_FONT_FEATURE_ID_VPAL, //!< Proportional Alternate Vertical Metrics.
  BL_FONT_FEATURE_ID_VRT2, //!< Vertical Alternates and Rotation.
  BL_FONT_FEATURE_ID_VRTR, //!< Vertical Alternates for Rotation.
  BL_FONT_FEATURE_ID_ZERO, //!< Slashed Zero.

  BL_FONT_FEATURE_ID_MAX = BL_FONT_FEATURE_ID_ZERO
};

BL_HIDDEN extern const BLTag featureIdToTagTable[];

enum BLFontVariationId : uint32_t {
  BL_FONT_VARIATION_ID_ITAL, //!< Italic.
  BL_FONT_VARIATION_ID_OPSZ, //!< Optical size.
  BL_FONT_VARIATION_ID_SLNT, //!< Slant.
  BL_FONT_VARIATION_ID_WDTH, //!< Width.
  BL_FONT_VARIATION_ID_WGHT, //!< Weight.

  BL_FONT_VARIATION_ID_MAX = BL_FONT_VARIATION_ID_WGHT
};

BL_HIDDEN extern const BLTag variationIdToTagTable[];

//! Test whether all 4 characters encoded in `tag` are within [32, 126] range.
static BL_INLINE bool isTagValid(BLTag tag) noexcept {
  constexpr uint32_t kSubPattern = 32;        // Tests characters in range [0, 31].
  constexpr uint32_t kAddPattern = 127 - 126; // Tests characters in range [127, 255].

  uint32_t x = tag - BL_MAKE_TAG(kSubPattern, kSubPattern, kSubPattern, kSubPattern);
  uint32_t y = tag + BL_MAKE_TAG(kAddPattern, kAddPattern, kAddPattern, kAddPattern);

  // If `x` or `y` underflown/overflown it would have one or more bits in `0x80808080` mask set. In
  // that case the given `tag` is not valid and has one or more character outside of the allowed range.
  return ((x | y) & 0x80808080u) == 0u;
}

static BL_INLINE bool isOpenTypeCollectionTag(BLTag tag) noexcept {
  return tag == BL_MAKE_TAG('t', 't', 'c', 'f');
}

static BL_INLINE bool isOpenTypeVersionTag(BLTag tag) noexcept {
  return tag == BL_MAKE_TAG('O', 'T', 'T', 'O') ||
         tag == BL_MAKE_TAG( 0 ,  1 ,  0 ,  0 ) ||
         tag == BL_MAKE_TAG('t', 'r', 'u', 'e') ;
}

//! Converts `tag` to a null-terminated ASCII string `str`. Characters that are not printable are replaced by '?'.
static BL_INLINE void tagToAscii(char str[5], uint32_t tag) noexcept {
  for (size_t i = 0; i < 4; i++, tag <<= 8) {
    uint32_t c = tag >> 24;
    str[i] = (c < 32 || c > 126) ? char('?') : char(c);
  }
  str[4] = '\0';
}

static BL_INLINE uint32_t featureTagToId(uint32_t tag) noexcept {
  static const uint8_t hashTable1[256] = {
    30, 0, 40, 160, 50, 0, 60, 0, 70, 225, 80, 0, 90, 0, 100, 0, 110, 193, 120, 0, 204, 144, 23, 156,
    33, 0, 43, 0, 53, 186, 63, 139, 73, 0, 83, 163, 93, 137, 103, 184, 113, 166, 197, 145, 207, 0, 27, 143,
    37, 175, 47, 0, 57, 129, 67, 0, 77, 0, 87, 168, 97, 0, 107, 135, 117, 240, 201, 0, 211, 0, 0, 124,
    0, 142, 149, 0, 0, 190, 0, 15, 146, 0, 0, 126, 0, 3, 0, 0, 0, 154, 205, 127, 24, 136, 34, 13,
    44, 8, 54, 20, 64, 12, 74, 170, 84, 234, 94, 133, 104, 191, 114, 0, 198, 11, 208, 226, 28, 0, 38, 9,
    48, 0, 58, 233, 68, 10, 78, 182, 88, 231, 98, 0, 108, 0, 118, 0, 202, 0, 212, 237, 31, 187, 41, 0,
    51, 150, 61, 125, 71, 0, 81, 0, 91, 235, 101, 134, 111, 123, 0, 219, 0, 128, 25, 158, 35, 0, 45, 0,
    55, 0, 65, 0, 75, 130, 85, 161, 95, 2, 105, 229, 115, 0, 199, 222, 209, 131, 29, 220, 39, 7, 49, 195,
    59, 138, 69, 221, 79, 173, 89, 1, 99, 0, 109, 0, 119, 153, 203, 188, 22, 192, 32, 6, 42, 174, 52, 0,
    62, 14, 72, 0, 82, 179, 92, 183, 102, 224, 112, 194, 0, 0, 206, 0, 26, 122, 36, 152, 46, 5, 56, 172,
    66, 155, 76, 17, 86, 4, 96, 0, 106, 140, 116, 0, 200, 0, 210, 0
  };

  static const uint8_t hashTable2[64] = {
    230, 0, 213, 214, 215, 16, 0, 147, 0, 0, 0, 162, 178, 0, 167, 238, 169, 0, 0, 0, 217, 0, 0, 227,
    159, 132, 232, 157, 181, 0, 223, 18, 165, 19, 218, 185, 121, 0, 0, 0, 0, 176, 0, 141, 0, 0, 216, 189,
    236, 196, 177, 180, 148, 164, 0, 171, 0, 228, 0, 0, 239, 21, 0, 151
  };

  uint32_t h1 = (tag * 1174536694u) >> 24u;
  uint32_t h2 = (tag * 46218148u) >> 26u;

  uint32_t i1 = hashTable1[h1];
  uint32_t i2 = hashTable2[h2];

  uint32_t index = 0xFFFFFFFFu;

  if (featureIdToTagTable[i1] == tag)
    index = i1;

  if (featureIdToTagTable[i2] == tag)
    index = i2;

  return index;
}

static BL_INLINE uint32_t variationTagToId(uint32_t tag) noexcept {
  static const uint8_t hashTable[8] = {
    0, 1, 3, 0, 4, 0, 0, 2
  };

  uint32_t h1 = (tag * 1048576u) >> 29u;
  uint32_t i1 = hashTable[h1];
  uint32_t index = 0xFFFFFFFFu;

  if (variationIdToTagTable[i1] == tag)
    index = i1;

  return index;
}

} // {BLFontTagsPrivate}

//! \}
//! \endcond

#endif // BLEND2D_FONTTAGTABLES_P_H_INCLUDED
