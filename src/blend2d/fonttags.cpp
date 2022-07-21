// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_p.h"
#include "fonttags_p.h"

namespace BLFontTagsPrivate {

// BLFontFeatureTables
// ===================

const BLTag featureIdToTagTable[] = {
  BL_MAKE_TAG('a', 'a', 'l', 't'), // BL_FONT_FEATURE_ID_AALT - Access All Alternates.
  BL_MAKE_TAG('a', 'b', 'v', 'f'), // BL_FONT_FEATURE_ID_ABVF - Above-base Forms.
  BL_MAKE_TAG('a', 'b', 'v', 'm'), // BL_FONT_FEATURE_ID_ABVM - Above-base Mark Positioning.
  BL_MAKE_TAG('a', 'b', 'v', 's'), // BL_FONT_FEATURE_ID_ABVS - Above-base Substitutions.
  BL_MAKE_TAG('a', 'f', 'r', 'c'), // BL_FONT_FEATURE_ID_AFRC - Alternative Fractions.
  BL_MAKE_TAG('a', 'k', 'h', 'n'), // BL_FONT_FEATURE_ID_AKHN - Akhand.
  BL_MAKE_TAG('b', 'l', 'w', 'f'), // BL_FONT_FEATURE_ID_BLWF - Below-base Forms.
  BL_MAKE_TAG('b', 'l', 'w', 'm'), // BL_FONT_FEATURE_ID_BLWM - Below-base Mark Positioning.
  BL_MAKE_TAG('b', 'l', 'w', 's'), // BL_FONT_FEATURE_ID_BLWS - Below-base Substitutions.
  BL_MAKE_TAG('c', '2', 'p', 'c'), // BL_FONT_FEATURE_ID_C2PC - Petite Capitals From Capitals.
  BL_MAKE_TAG('c', '2', 's', 'c'), // BL_FONT_FEATURE_ID_C2SC - Small Capitals From Capitals.
  BL_MAKE_TAG('c', 'a', 'l', 't'), // BL_FONT_FEATURE_ID_CALT - Contextual Alternates.
  BL_MAKE_TAG('c', 'a', 's', 'e'), // BL_FONT_FEATURE_ID_CASE - Case-Sensitive Forms.
  BL_MAKE_TAG('c', 'c', 'm', 'p'), // BL_FONT_FEATURE_ID_CCMP - Glyph Composition / Decomposition.
  BL_MAKE_TAG('c', 'f', 'a', 'r'), // BL_FONT_FEATURE_ID_CFAR - Conjunct Form After Ro.
  BL_MAKE_TAG('c', 'h', 'w', 's'), // BL_FONT_FEATURE_ID_CHWS - Contextual Half-width Spacing.
  BL_MAKE_TAG('c', 'j', 'c', 't'), // BL_FONT_FEATURE_ID_CJCT - Conjunct Forms.
  BL_MAKE_TAG('c', 'l', 'i', 'g'), // BL_FONT_FEATURE_ID_CLIG - Contextual Ligatures.
  BL_MAKE_TAG('c', 'p', 'c', 't'), // BL_FONT_FEATURE_ID_CPCT - Centered CJK Punctuation.
  BL_MAKE_TAG('c', 'p', 's', 'p'), // BL_FONT_FEATURE_ID_CPSP - Capital Spacing.
  BL_MAKE_TAG('c', 's', 'w', 'h'), // BL_FONT_FEATURE_ID_CSWH - Contextual Swash.
  BL_MAKE_TAG('c', 'u', 'r', 's'), // BL_FONT_FEATURE_ID_CURS - Cursive Positioning.
  BL_MAKE_TAG('c', 'v', '0', '1'), // BL_FONT_FEATURE_ID_CV01 - Character Variant 1.
  BL_MAKE_TAG('c', 'v', '0', '2'), // BL_FONT_FEATURE_ID_CV02 - Character Variant 2.
  BL_MAKE_TAG('c', 'v', '0', '3'), // BL_FONT_FEATURE_ID_CV03 - Character Variant 3.
  BL_MAKE_TAG('c', 'v', '0', '4'), // BL_FONT_FEATURE_ID_CV04 - Character Variant 4.
  BL_MAKE_TAG('c', 'v', '0', '5'), // BL_FONT_FEATURE_ID_CV05 - Character Variant 5.
  BL_MAKE_TAG('c', 'v', '0', '6'), // BL_FONT_FEATURE_ID_CV06 - Character Variant 6.
  BL_MAKE_TAG('c', 'v', '0', '7'), // BL_FONT_FEATURE_ID_CV07 - Character Variant 7.
  BL_MAKE_TAG('c', 'v', '0', '8'), // BL_FONT_FEATURE_ID_CV08 - Character Variant 8.
  BL_MAKE_TAG('c', 'v', '0', '9'), // BL_FONT_FEATURE_ID_CV09 - Character Variant 9.
  BL_MAKE_TAG('c', 'v', '1', '0'), // BL_FONT_FEATURE_ID_CV10 - Character Variant 10.
  BL_MAKE_TAG('c', 'v', '1', '1'), // BL_FONT_FEATURE_ID_CV11 - Character Variant 11.
  BL_MAKE_TAG('c', 'v', '1', '2'), // BL_FONT_FEATURE_ID_CV12 - Character Variant 12.
  BL_MAKE_TAG('c', 'v', '1', '3'), // BL_FONT_FEATURE_ID_CV13 - Character Variant 13.
  BL_MAKE_TAG('c', 'v', '1', '4'), // BL_FONT_FEATURE_ID_CV14 - Character Variant 14.
  BL_MAKE_TAG('c', 'v', '1', '5'), // BL_FONT_FEATURE_ID_CV15 - Character Variant 15.
  BL_MAKE_TAG('c', 'v', '1', '6'), // BL_FONT_FEATURE_ID_CV16 - Character Variant 16.
  BL_MAKE_TAG('c', 'v', '1', '7'), // BL_FONT_FEATURE_ID_CV17 - Character Variant 17.
  BL_MAKE_TAG('c', 'v', '1', '8'), // BL_FONT_FEATURE_ID_CV18 - Character Variant 18.
  BL_MAKE_TAG('c', 'v', '1', '9'), // BL_FONT_FEATURE_ID_CV19 - Character Variant 19.
  BL_MAKE_TAG('c', 'v', '2', '0'), // BL_FONT_FEATURE_ID_CV20 - Character Variant 20.
  BL_MAKE_TAG('c', 'v', '2', '1'), // BL_FONT_FEATURE_ID_CV21 - Character Variant 21.
  BL_MAKE_TAG('c', 'v', '2', '2'), // BL_FONT_FEATURE_ID_CV22 - Character Variant 22.
  BL_MAKE_TAG('c', 'v', '2', '3'), // BL_FONT_FEATURE_ID_CV23 - Character Variant 23.
  BL_MAKE_TAG('c', 'v', '2', '4'), // BL_FONT_FEATURE_ID_CV24 - Character Variant 24.
  BL_MAKE_TAG('c', 'v', '2', '5'), // BL_FONT_FEATURE_ID_CV25 - Character Variant 25.
  BL_MAKE_TAG('c', 'v', '2', '6'), // BL_FONT_FEATURE_ID_CV26 - Character Variant 26.
  BL_MAKE_TAG('c', 'v', '2', '7'), // BL_FONT_FEATURE_ID_CV27 - Character Variant 27.
  BL_MAKE_TAG('c', 'v', '2', '8'), // BL_FONT_FEATURE_ID_CV28 - Character Variant 28.
  BL_MAKE_TAG('c', 'v', '2', '9'), // BL_FONT_FEATURE_ID_CV29 - Character Variant 29.
  BL_MAKE_TAG('c', 'v', '3', '0'), // BL_FONT_FEATURE_ID_CV30 - Character Variant 30.
  BL_MAKE_TAG('c', 'v', '3', '1'), // BL_FONT_FEATURE_ID_CV31 - Character Variant 31.
  BL_MAKE_TAG('c', 'v', '3', '2'), // BL_FONT_FEATURE_ID_CV32 - Character Variant 32.
  BL_MAKE_TAG('c', 'v', '3', '3'), // BL_FONT_FEATURE_ID_CV33 - Character Variant 33.
  BL_MAKE_TAG('c', 'v', '3', '4'), // BL_FONT_FEATURE_ID_CV34 - Character Variant 34.
  BL_MAKE_TAG('c', 'v', '3', '5'), // BL_FONT_FEATURE_ID_CV35 - Character Variant 35.
  BL_MAKE_TAG('c', 'v', '3', '6'), // BL_FONT_FEATURE_ID_CV36 - Character Variant 36.
  BL_MAKE_TAG('c', 'v', '3', '7'), // BL_FONT_FEATURE_ID_CV37 - Character Variant 37.
  BL_MAKE_TAG('c', 'v', '3', '8'), // BL_FONT_FEATURE_ID_CV38 - Character Variant 38.
  BL_MAKE_TAG('c', 'v', '3', '9'), // BL_FONT_FEATURE_ID_CV39 - Character Variant 39.
  BL_MAKE_TAG('c', 'v', '4', '0'), // BL_FONT_FEATURE_ID_CV40 - Character Variant 40.
  BL_MAKE_TAG('c', 'v', '4', '1'), // BL_FONT_FEATURE_ID_CV41 - Character Variant 41.
  BL_MAKE_TAG('c', 'v', '4', '2'), // BL_FONT_FEATURE_ID_CV42 - Character Variant 42.
  BL_MAKE_TAG('c', 'v', '4', '3'), // BL_FONT_FEATURE_ID_CV43 - Character Variant 43.
  BL_MAKE_TAG('c', 'v', '4', '4'), // BL_FONT_FEATURE_ID_CV44 - Character Variant 44.
  BL_MAKE_TAG('c', 'v', '4', '5'), // BL_FONT_FEATURE_ID_CV45 - Character Variant 45.
  BL_MAKE_TAG('c', 'v', '4', '6'), // BL_FONT_FEATURE_ID_CV46 - Character Variant 46.
  BL_MAKE_TAG('c', 'v', '4', '7'), // BL_FONT_FEATURE_ID_CV47 - Character Variant 47.
  BL_MAKE_TAG('c', 'v', '4', '8'), // BL_FONT_FEATURE_ID_CV48 - Character Variant 48.
  BL_MAKE_TAG('c', 'v', '4', '9'), // BL_FONT_FEATURE_ID_CV49 - Character Variant 49.
  BL_MAKE_TAG('c', 'v', '5', '0'), // BL_FONT_FEATURE_ID_CV50 - Character Variant 50.
  BL_MAKE_TAG('c', 'v', '5', '1'), // BL_FONT_FEATURE_ID_CV51 - Character Variant 51.
  BL_MAKE_TAG('c', 'v', '5', '2'), // BL_FONT_FEATURE_ID_CV52 - Character Variant 52.
  BL_MAKE_TAG('c', 'v', '5', '3'), // BL_FONT_FEATURE_ID_CV53 - Character Variant 53.
  BL_MAKE_TAG('c', 'v', '5', '4'), // BL_FONT_FEATURE_ID_CV54 - Character Variant 54.
  BL_MAKE_TAG('c', 'v', '5', '5'), // BL_FONT_FEATURE_ID_CV55 - Character Variant 55.
  BL_MAKE_TAG('c', 'v', '5', '6'), // BL_FONT_FEATURE_ID_CV56 - Character Variant 56.
  BL_MAKE_TAG('c', 'v', '5', '7'), // BL_FONT_FEATURE_ID_CV57 - Character Variant 57.
  BL_MAKE_TAG('c', 'v', '5', '8'), // BL_FONT_FEATURE_ID_CV58 - Character Variant 58.
  BL_MAKE_TAG('c', 'v', '5', '9'), // BL_FONT_FEATURE_ID_CV59 - Character Variant 59.
  BL_MAKE_TAG('c', 'v', '6', '0'), // BL_FONT_FEATURE_ID_CV60 - Character Variant 60.
  BL_MAKE_TAG('c', 'v', '6', '1'), // BL_FONT_FEATURE_ID_CV61 - Character Variant 61.
  BL_MAKE_TAG('c', 'v', '6', '2'), // BL_FONT_FEATURE_ID_CV62 - Character Variant 62.
  BL_MAKE_TAG('c', 'v', '6', '3'), // BL_FONT_FEATURE_ID_CV63 - Character Variant 63.
  BL_MAKE_TAG('c', 'v', '6', '4'), // BL_FONT_FEATURE_ID_CV64 - Character Variant 64.
  BL_MAKE_TAG('c', 'v', '6', '5'), // BL_FONT_FEATURE_ID_CV65 - Character Variant 65.
  BL_MAKE_TAG('c', 'v', '6', '6'), // BL_FONT_FEATURE_ID_CV66 - Character Variant 66.
  BL_MAKE_TAG('c', 'v', '6', '7'), // BL_FONT_FEATURE_ID_CV67 - Character Variant 67.
  BL_MAKE_TAG('c', 'v', '6', '8'), // BL_FONT_FEATURE_ID_CV68 - Character Variant 68.
  BL_MAKE_TAG('c', 'v', '6', '9'), // BL_FONT_FEATURE_ID_CV69 - Character Variant 69.
  BL_MAKE_TAG('c', 'v', '7', '0'), // BL_FONT_FEATURE_ID_CV70 - Character Variant 70.
  BL_MAKE_TAG('c', 'v', '7', '1'), // BL_FONT_FEATURE_ID_CV71 - Character Variant 71.
  BL_MAKE_TAG('c', 'v', '7', '2'), // BL_FONT_FEATURE_ID_CV72 - Character Variant 72.
  BL_MAKE_TAG('c', 'v', '7', '3'), // BL_FONT_FEATURE_ID_CV73 - Character Variant 73.
  BL_MAKE_TAG('c', 'v', '7', '4'), // BL_FONT_FEATURE_ID_CV74 - Character Variant 74.
  BL_MAKE_TAG('c', 'v', '7', '5'), // BL_FONT_FEATURE_ID_CV75 - Character Variant 75.
  BL_MAKE_TAG('c', 'v', '7', '6'), // BL_FONT_FEATURE_ID_CV76 - Character Variant 76.
  BL_MAKE_TAG('c', 'v', '7', '7'), // BL_FONT_FEATURE_ID_CV77 - Character Variant 77.
  BL_MAKE_TAG('c', 'v', '7', '8'), // BL_FONT_FEATURE_ID_CV78 - Character Variant 78.
  BL_MAKE_TAG('c', 'v', '7', '9'), // BL_FONT_FEATURE_ID_CV79 - Character Variant 79.
  BL_MAKE_TAG('c', 'v', '8', '0'), // BL_FONT_FEATURE_ID_CV80 - Character Variant 80.
  BL_MAKE_TAG('c', 'v', '8', '1'), // BL_FONT_FEATURE_ID_CV81 - Character Variant 81.
  BL_MAKE_TAG('c', 'v', '8', '2'), // BL_FONT_FEATURE_ID_CV82 - Character Variant 82.
  BL_MAKE_TAG('c', 'v', '8', '3'), // BL_FONT_FEATURE_ID_CV83 - Character Variant 83.
  BL_MAKE_TAG('c', 'v', '8', '4'), // BL_FONT_FEATURE_ID_CV84 - Character Variant 84.
  BL_MAKE_TAG('c', 'v', '8', '5'), // BL_FONT_FEATURE_ID_CV85 - Character Variant 85.
  BL_MAKE_TAG('c', 'v', '8', '6'), // BL_FONT_FEATURE_ID_CV86 - Character Variant 86.
  BL_MAKE_TAG('c', 'v', '8', '7'), // BL_FONT_FEATURE_ID_CV87 - Character Variant 87.
  BL_MAKE_TAG('c', 'v', '8', '8'), // BL_FONT_FEATURE_ID_CV88 - Character Variant 88.
  BL_MAKE_TAG('c', 'v', '8', '9'), // BL_FONT_FEATURE_ID_CV89 - Character Variant 89.
  BL_MAKE_TAG('c', 'v', '9', '0'), // BL_FONT_FEATURE_ID_CV90 - Character Variant 90.
  BL_MAKE_TAG('c', 'v', '9', '1'), // BL_FONT_FEATURE_ID_CV91 - Character Variant 91.
  BL_MAKE_TAG('c', 'v', '9', '2'), // BL_FONT_FEATURE_ID_CV92 - Character Variant 92.
  BL_MAKE_TAG('c', 'v', '9', '3'), // BL_FONT_FEATURE_ID_CV93 - Character Variant 93.
  BL_MAKE_TAG('c', 'v', '9', '4'), // BL_FONT_FEATURE_ID_CV94 - Character Variant 94.
  BL_MAKE_TAG('c', 'v', '9', '5'), // BL_FONT_FEATURE_ID_CV95 - Character Variant 95.
  BL_MAKE_TAG('c', 'v', '9', '6'), // BL_FONT_FEATURE_ID_CV96 - Character Variant 96.
  BL_MAKE_TAG('c', 'v', '9', '7'), // BL_FONT_FEATURE_ID_CV97 - Character Variant 97.
  BL_MAKE_TAG('c', 'v', '9', '8'), // BL_FONT_FEATURE_ID_CV98 - Character Variant 98.
  BL_MAKE_TAG('c', 'v', '9', '9'), // BL_FONT_FEATURE_ID_CV99 - Character Variant 99.
  BL_MAKE_TAG('d', 'i', 's', 't'), // BL_FONT_FEATURE_ID_DIST - Distances.
  BL_MAKE_TAG('d', 'l', 'i', 'g'), // BL_FONT_FEATURE_ID_DLIG - Discretionary Ligatures.
  BL_MAKE_TAG('d', 'n', 'o', 'm'), // BL_FONT_FEATURE_ID_DNOM - Denominators.
  BL_MAKE_TAG('d', 't', 'l', 's'), // BL_FONT_FEATURE_ID_DTLS - Dotless Forms.
  BL_MAKE_TAG('e', 'x', 'p', 't'), // BL_FONT_FEATURE_ID_EXPT - Expert Forms.
  BL_MAKE_TAG('f', 'a', 'l', 't'), // BL_FONT_FEATURE_ID_FALT - Final Glyph on Line Alternates.
  BL_MAKE_TAG('f', 'i', 'n', '2'), // BL_FONT_FEATURE_ID_FIN2 - Terminal Forms #2.
  BL_MAKE_TAG('f', 'i', 'n', '3'), // BL_FONT_FEATURE_ID_FIN3 - Terminal Forms #3.
  BL_MAKE_TAG('f', 'i', 'n', 'a'), // BL_FONT_FEATURE_ID_FINA - Terminal Forms.
  BL_MAKE_TAG('f', 'l', 'a', 'c'), // BL_FONT_FEATURE_ID_FLAC - Flattened accent forms.
  BL_MAKE_TAG('f', 'r', 'a', 'c'), // BL_FONT_FEATURE_ID_FRAC - Fractions.
  BL_MAKE_TAG('f', 'w', 'i', 'd'), // BL_FONT_FEATURE_ID_FWID - Full Widths.
  BL_MAKE_TAG('h', 'a', 'l', 'f'), // BL_FONT_FEATURE_ID_HALF - Half Forms.
  BL_MAKE_TAG('h', 'a', 'l', 'n'), // BL_FONT_FEATURE_ID_HALN - Halant Forms.
  BL_MAKE_TAG('h', 'a', 'l', 't'), // BL_FONT_FEATURE_ID_HALT - Alternate Half Widths.
  BL_MAKE_TAG('h', 'i', 's', 't'), // BL_FONT_FEATURE_ID_HIST - Historical Forms.
  BL_MAKE_TAG('h', 'k', 'n', 'a'), // BL_FONT_FEATURE_ID_HKNA - Horizontal Kana Alternates.
  BL_MAKE_TAG('h', 'l', 'i', 'g'), // BL_FONT_FEATURE_ID_HLIG - Historical Ligatures.
  BL_MAKE_TAG('h', 'n', 'g', 'l'), // BL_FONT_FEATURE_ID_HNGL - Hangul.
  BL_MAKE_TAG('h', 'o', 'j', 'o'), // BL_FONT_FEATURE_ID_HOJO - Hojo Kanji Forms (JIS X 0212-1990 Kanji Forms).
  BL_MAKE_TAG('h', 'w', 'i', 'd'), // BL_FONT_FEATURE_ID_HWID - Half Widths.
  BL_MAKE_TAG('i', 'n', 'i', 't'), // BL_FONT_FEATURE_ID_INIT - Initial Forms.
  BL_MAKE_TAG('i', 's', 'o', 'l'), // BL_FONT_FEATURE_ID_ISOL - Isolated Forms.
  BL_MAKE_TAG('i', 't', 'a', 'l'), // BL_FONT_FEATURE_ID_ITAL - Italics.
  BL_MAKE_TAG('j', 'a', 'l', 't'), // BL_FONT_FEATURE_ID_JALT - Justification Alternates.
  BL_MAKE_TAG('j', 'p', '0', '4'), // BL_FONT_FEATURE_ID_JP04 - JIS2004 Forms.
  BL_MAKE_TAG('j', 'p', '7', '8'), // BL_FONT_FEATURE_ID_JP78 - JIS78 Forms.
  BL_MAKE_TAG('j', 'p', '8', '3'), // BL_FONT_FEATURE_ID_JP83 - JIS83 Forms.
  BL_MAKE_TAG('j', 'p', '9', '0'), // BL_FONT_FEATURE_ID_JP90 - JIS90 Forms.
  BL_MAKE_TAG('k', 'e', 'r', 'n'), // BL_FONT_FEATURE_ID_KERN - Kerning.
  BL_MAKE_TAG('l', 'f', 'b', 'd'), // BL_FONT_FEATURE_ID_LFBD - Left Bounds.
  BL_MAKE_TAG('l', 'i', 'g', 'a'), // BL_FONT_FEATURE_ID_LIGA - Standard Ligatures.
  BL_MAKE_TAG('l', 'j', 'm', 'o'), // BL_FONT_FEATURE_ID_LJMO - Leading Jamo Forms.
  BL_MAKE_TAG('l', 'n', 'u', 'm'), // BL_FONT_FEATURE_ID_LNUM - Lining Figures.
  BL_MAKE_TAG('l', 'o', 'c', 'l'), // BL_FONT_FEATURE_ID_LOCL - Localized Forms.
  BL_MAKE_TAG('l', 't', 'r', 'a'), // BL_FONT_FEATURE_ID_LTRA - Left-to-right alternates.
  BL_MAKE_TAG('l', 't', 'r', 'm'), // BL_FONT_FEATURE_ID_LTRM - Left-to-right mirrored forms.
  BL_MAKE_TAG('m', 'a', 'r', 'k'), // BL_FONT_FEATURE_ID_MARK - Mark Positioning.
  BL_MAKE_TAG('m', 'e', 'd', '2'), // BL_FONT_FEATURE_ID_MED2 - Medial Forms #2.
  BL_MAKE_TAG('m', 'e', 'd', 'i'), // BL_FONT_FEATURE_ID_MEDI - Medial Forms.
  BL_MAKE_TAG('m', 'g', 'r', 'k'), // BL_FONT_FEATURE_ID_MGRK - Mathematical Greek.
  BL_MAKE_TAG('m', 'k', 'm', 'k'), // BL_FONT_FEATURE_ID_MKMK - Mark to Mark Positioning.
  BL_MAKE_TAG('m', 's', 'e', 't'), // BL_FONT_FEATURE_ID_MSET - Mark Positioning via Substitution.
  BL_MAKE_TAG('n', 'a', 'l', 't'), // BL_FONT_FEATURE_ID_NALT - Alternate Annotation Forms.
  BL_MAKE_TAG('n', 'l', 'c', 'k'), // BL_FONT_FEATURE_ID_NLCK - NLC Kanji Forms.
  BL_MAKE_TAG('n', 'u', 'k', 't'), // BL_FONT_FEATURE_ID_NUKT - Nukta Forms.
  BL_MAKE_TAG('n', 'u', 'm', 'r'), // BL_FONT_FEATURE_ID_NUMR - Numerators.
  BL_MAKE_TAG('o', 'n', 'u', 'm'), // BL_FONT_FEATURE_ID_ONUM - Oldstyle Figures.
  BL_MAKE_TAG('o', 'p', 'b', 'd'), // BL_FONT_FEATURE_ID_OPBD - Optical Bounds.
  BL_MAKE_TAG('o', 'r', 'd', 'n'), // BL_FONT_FEATURE_ID_ORDN - Ordinals.
  BL_MAKE_TAG('o', 'r', 'n', 'm'), // BL_FONT_FEATURE_ID_ORNM - Ornaments.
  BL_MAKE_TAG('p', 'a', 'l', 't'), // BL_FONT_FEATURE_ID_PALT - Proportional Alternate Widths.
  BL_MAKE_TAG('p', 'c', 'a', 'p'), // BL_FONT_FEATURE_ID_PCAP - Petite Capitals.
  BL_MAKE_TAG('p', 'k', 'n', 'a'), // BL_FONT_FEATURE_ID_PKNA - Proportional Kana.
  BL_MAKE_TAG('p', 'n', 'u', 'm'), // BL_FONT_FEATURE_ID_PNUM - Proportional Figures.
  BL_MAKE_TAG('p', 'r', 'e', 'f'), // BL_FONT_FEATURE_ID_PREF - Pre-Base Forms.
  BL_MAKE_TAG('p', 'r', 'e', 's'), // BL_FONT_FEATURE_ID_PRES - Pre-base Substitutions.
  BL_MAKE_TAG('p', 's', 't', 'f'), // BL_FONT_FEATURE_ID_PSTF - Post-base Forms.
  BL_MAKE_TAG('p', 's', 't', 's'), // BL_FONT_FEATURE_ID_PSTS - Post-base Substitutions.
  BL_MAKE_TAG('p', 'w', 'i', 'd'), // BL_FONT_FEATURE_ID_PWID - Proportional Widths.
  BL_MAKE_TAG('q', 'w', 'i', 'd'), // BL_FONT_FEATURE_ID_QWID - Quarter Widths.
  BL_MAKE_TAG('r', 'a', 'n', 'd'), // BL_FONT_FEATURE_ID_RAND - Randomize.
  BL_MAKE_TAG('r', 'c', 'l', 't'), // BL_FONT_FEATURE_ID_RCLT - Required Contextual Alternates.
  BL_MAKE_TAG('r', 'k', 'r', 'f'), // BL_FONT_FEATURE_ID_RKRF - Rakar Forms.
  BL_MAKE_TAG('r', 'l', 'i', 'g'), // BL_FONT_FEATURE_ID_RLIG - Required Ligatures.
  BL_MAKE_TAG('r', 'p', 'h', 'f'), // BL_FONT_FEATURE_ID_RPHF - Reph Forms.
  BL_MAKE_TAG('r', 't', 'b', 'd'), // BL_FONT_FEATURE_ID_RTBD - Right Bounds.
  BL_MAKE_TAG('r', 't', 'l', 'a'), // BL_FONT_FEATURE_ID_RTLA - Right-to-left alternates.
  BL_MAKE_TAG('r', 't', 'l', 'm'), // BL_FONT_FEATURE_ID_RTLM - Right-to-left mirrored forms.
  BL_MAKE_TAG('r', 'u', 'b', 'y'), // BL_FONT_FEATURE_ID_RUBY - Ruby Notation Forms.
  BL_MAKE_TAG('r', 'v', 'r', 'n'), // BL_FONT_FEATURE_ID_RVRN - Required Variation Alternates.
  BL_MAKE_TAG('s', 'a', 'l', 't'), // BL_FONT_FEATURE_ID_SALT - Stylistic Alternates.
  BL_MAKE_TAG('s', 'i', 'n', 'f'), // BL_FONT_FEATURE_ID_SINF - Scientific Inferiors.
  BL_MAKE_TAG('s', 'i', 'z', 'e'), // BL_FONT_FEATURE_ID_SIZE - Optical size.
  BL_MAKE_TAG('s', 'm', 'c', 'p'), // BL_FONT_FEATURE_ID_SMCP - Small Capitals.
  BL_MAKE_TAG('s', 'm', 'p', 'l'), // BL_FONT_FEATURE_ID_SMPL - Simplified Forms.
  BL_MAKE_TAG('s', 's', '0', '1'), // BL_FONT_FEATURE_ID_SS01 - Stylistic Set 1.
  BL_MAKE_TAG('s', 's', '0', '2'), // BL_FONT_FEATURE_ID_SS02 - Stylistic Set 2.
  BL_MAKE_TAG('s', 's', '0', '3'), // BL_FONT_FEATURE_ID_SS03 - Stylistic Set 3.
  BL_MAKE_TAG('s', 's', '0', '4'), // BL_FONT_FEATURE_ID_SS04 - Stylistic Set 4.
  BL_MAKE_TAG('s', 's', '0', '5'), // BL_FONT_FEATURE_ID_SS05 - Stylistic Set 5.
  BL_MAKE_TAG('s', 's', '0', '6'), // BL_FONT_FEATURE_ID_SS06 - Stylistic Set 6.
  BL_MAKE_TAG('s', 's', '0', '7'), // BL_FONT_FEATURE_ID_SS07 - Stylistic Set 7.
  BL_MAKE_TAG('s', 's', '0', '8'), // BL_FONT_FEATURE_ID_SS08 - Stylistic Set 8.
  BL_MAKE_TAG('s', 's', '0', '9'), // BL_FONT_FEATURE_ID_SS09 - Stylistic Set 9.
  BL_MAKE_TAG('s', 's', '1', '0'), // BL_FONT_FEATURE_ID_SS10 - Stylistic Set 10.
  BL_MAKE_TAG('s', 's', '1', '1'), // BL_FONT_FEATURE_ID_SS11 - Stylistic Set 11.
  BL_MAKE_TAG('s', 's', '1', '2'), // BL_FONT_FEATURE_ID_SS12 - Stylistic Set 12.
  BL_MAKE_TAG('s', 's', '1', '3'), // BL_FONT_FEATURE_ID_SS13 - Stylistic Set 13.
  BL_MAKE_TAG('s', 's', '1', '4'), // BL_FONT_FEATURE_ID_SS14 - Stylistic Set 14.
  BL_MAKE_TAG('s', 's', '1', '5'), // BL_FONT_FEATURE_ID_SS15 - Stylistic Set 15.
  BL_MAKE_TAG('s', 's', '1', '6'), // BL_FONT_FEATURE_ID_SS16 - Stylistic Set 16.
  BL_MAKE_TAG('s', 's', '1', '7'), // BL_FONT_FEATURE_ID_SS17 - Stylistic Set 17.
  BL_MAKE_TAG('s', 's', '1', '8'), // BL_FONT_FEATURE_ID_SS18 - Stylistic Set 18.
  BL_MAKE_TAG('s', 's', '1', '9'), // BL_FONT_FEATURE_ID_SS19 - Stylistic Set 19.
  BL_MAKE_TAG('s', 's', '2', '0'), // BL_FONT_FEATURE_ID_SS20 - Stylistic Set 20.
  BL_MAKE_TAG('s', 's', 't', 'y'), // BL_FONT_FEATURE_ID_SSTY - Math script style alternates.
  BL_MAKE_TAG('s', 't', 'c', 'h'), // BL_FONT_FEATURE_ID_STCH - Stretching Glyph Decomposition.
  BL_MAKE_TAG('s', 'u', 'b', 's'), // BL_FONT_FEATURE_ID_SUBS - Subscript.
  BL_MAKE_TAG('s', 'u', 'p', 's'), // BL_FONT_FEATURE_ID_SUPS - Superscript.
  BL_MAKE_TAG('s', 'w', 's', 'h'), // BL_FONT_FEATURE_ID_SWSH - Swash.
  BL_MAKE_TAG('t', 'i', 't', 'l'), // BL_FONT_FEATURE_ID_TITL - Titling.
  BL_MAKE_TAG('t', 'j', 'm', 'o'), // BL_FONT_FEATURE_ID_TJMO - Trailing Jamo Forms.
  BL_MAKE_TAG('t', 'n', 'a', 'm'), // BL_FONT_FEATURE_ID_TNAM - Traditional Name Forms.
  BL_MAKE_TAG('t', 'n', 'u', 'm'), // BL_FONT_FEATURE_ID_TNUM - Tabular Figures.
  BL_MAKE_TAG('t', 'r', 'a', 'd'), // BL_FONT_FEATURE_ID_TRAD - Traditional Forms.
  BL_MAKE_TAG('t', 'w', 'i', 'd'), // BL_FONT_FEATURE_ID_TWID - Third Widths.
  BL_MAKE_TAG('u', 'n', 'i', 'c'), // BL_FONT_FEATURE_ID_UNIC - Unicase.
  BL_MAKE_TAG('v', 'a', 'l', 't'), // BL_FONT_FEATURE_ID_VALT - Alternate Vertical Metrics.
  BL_MAKE_TAG('v', 'a', 't', 'u'), // BL_FONT_FEATURE_ID_VATU - Vattu Variants.
  BL_MAKE_TAG('v', 'c', 'h', 'w'), // BL_FONT_FEATURE_ID_VCHW - Vertical Contextual Half-width Spacing.
  BL_MAKE_TAG('v', 'e', 'r', 't'), // BL_FONT_FEATURE_ID_VERT - Vertical Writing.
  BL_MAKE_TAG('v', 'h', 'a', 'l'), // BL_FONT_FEATURE_ID_VHAL - Alternate Vertical Half Metrics.
  BL_MAKE_TAG('v', 'j', 'm', 'o'), // BL_FONT_FEATURE_ID_VJMO - Vowel Jamo Forms.
  BL_MAKE_TAG('v', 'k', 'n', 'a'), // BL_FONT_FEATURE_ID_VKNA - Vertical Kana Alternates.
  BL_MAKE_TAG('v', 'k', 'r', 'n'), // BL_FONT_FEATURE_ID_VKRN - Vertical Kerning.
  BL_MAKE_TAG('v', 'p', 'a', 'l'), // BL_FONT_FEATURE_ID_VPAL - Proportional Alternate Vertical Metrics.
  BL_MAKE_TAG('v', 'r', 't', '2'), // BL_FONT_FEATURE_ID_VRT2 - Vertical Alternates and Rotation.
  BL_MAKE_TAG('v', 'r', 't', 'r'), // BL_FONT_FEATURE_ID_VRTR - Vertical Alternates for Rotation.
  BL_MAKE_TAG('z', 'e', 'r', 'o')  // BL_FONT_FEATURE_ID_ZERO - Slashed Zero.
};

const BLTag variationIdToTagTable[] = {
  BL_MAKE_TAG('i', 't', 'a', 'l'), // BL_FONT_VARIATION_ID_ITAL - Italic.
  BL_MAKE_TAG('o', 'p', 's', 'z'), // BL_FONT_VARIATION_ID_OPSZ - Optical size.
  BL_MAKE_TAG('s', 'l', 'n', 't'), // BL_FONT_VARIATION_ID_SLNT - Slant.
  BL_MAKE_TAG('w', 'd', 't', 'h'), // BL_FONT_VARIATION_ID_WDTH - Width.
  BL_MAKE_TAG('w', 'g', 'h', 't')  // BL_FONT_VARIATION_ID_WGHT - Weight.
};

} // {BLFontTagsPrivate}