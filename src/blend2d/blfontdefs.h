// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_BLFONTDEFS_H
#define BLEND2D_BLFONTDEFS_H

#include "./blgeometry.h"

//! \addtogroup blend2d_api_text
//! \{

// ============================================================================
// [Constants]
// ============================================================================

//! Flags used by `BLGlyphItem::value` member.
//!
//! Glyph flags are only available after code-points were mapped to glyphs as
//! `BLGlyphItem::value` member contains either the code-point or glyph-id and
//! glyph flags.
BL_DEFINE_ENUM(BLGlyphItemFlags) {
  //! Glyph is marked for operation (GSUB/GPOS). This flag should never be
  //! set after glyph processing. It's an internal flag that is used during
  //! processing to mark glyphs that need a second pass, but the engine
  //! should always clear the flag after it processes marked glyphs.
  //!
  //! Used by:
  //!   - GSUB::LookupType #2 Format #1 - Multiple Substitution.
  BL_GLYPH_ITEM_FLAG_MARK = 0x80000000u
};

//! Placement of glyphs stored in a `BLGlyphRun`.
BL_DEFINE_ENUM(BLGlyphPlacementType) {
  //! No placement (custom handling by `BLPathSinkFunc`).
  BL_GLYPH_PLACEMENT_TYPE_NONE = 0,
  //! Each glyph has a BLGlyphPlacement (advance + offset).
  BL_GLYPH_PLACEMENT_TYPE_ADVANCE_OFFSET = 1,
  //! Each glyph has a BLPoint offset in design-space units.
  BL_GLYPH_PLACEMENT_TYPE_DESIGN_UNITS = 2,
  //! Each glyph has a BLPoint offset in user-space units.
  BL_GLYPH_PLACEMENT_TYPE_USER_UNITS = 3,
  //! Each glyph has a BLPoint offset in absolute units.
  BL_GLYPH_PLACEMENT_TYPE_ABSOLUTE_UNITS = 4
};

BL_DEFINE_ENUM(BLGlyphRunFlags) {
  //! Glyph-run contains USC-4 string and not glyphs (glyph-buffer only).
  BL_GLYPH_RUN_FLAG_UCS4_CONTENT              = 0x10000000u,
  //! Glyph-run was created from text that was not a valid unicode.
  BL_GLYPH_RUN_FLAG_INVALID_TEXT              = 0x20000000u,
  //! Not the whole text was mapped to glyphs (contains undefined glyphs).
  BL_GLYPH_RUN_FLAG_UNDEFINED_GLYPHS          = 0x40000000u,
  //! Encountered invalid font-data during text / glyph processing.
  BL_GLYPH_RUN_FLAG_INVALID_FONT_DATA         = 0x80000000u
};

//! Type of a font or font-face.
BL_DEFINE_ENUM(BLFontFaceType) {
  //! None or unknown font type.
  BL_FONT_FACE_TYPE_NONE = 0,
  //! TrueType/OpenType font type.
  BL_FONT_FACE_TYPE_OPENTYPE = 1,

  //! Count of font-face types.
  BL_FONT_FACE_TYPE_COUNT = 2
};

BL_DEFINE_ENUM(BLFontFaceFlags) {
  //! Font uses typographic family and subfamily names.
  BL_FONT_FACE_FLAG_TYPOGRAPHIC_NAMES         = 0x00000001u,
  //! Font uses typographic metrics.
  BL_FONT_FACE_FLAG_TYPOGRAPHIC_METRICS       = 0x00000002u,
  //! Character to glyph mapping is available.
  BL_FONT_FACE_FLAG_CHAR_TO_GLYPH_MAPPING     = 0x00000004u,
  //! Horizontal glyph metrics (advances, side bearings) is available.
  BL_FONT_FACE_FLAG_HORIZONTAL_METIRCS        = 0x00000010u,
  //! Vertical glyph metrics (advances, side bearings) is available.
  BL_FONT_FACE_FLAG_VERTICAL_METRICS          = 0x00000020u,
  //! Legacy horizontal kerning feature ('kern' table with horizontal kerning data).
  BL_FONT_FACE_FLAG_HORIZONTAL_KERNING        = 0x00000040u,
  //! Legacy vertical kerning feature ('kern' table with vertical kerning data).
  BL_FONT_FACE_FLAG_VERTICAL_KERNING          = 0x00000080u,
  //! OpenType features (GDEF, GPOS, GSUB) are available.
  BL_FONT_FACE_FLAG_OPENTYPE_FEATURES         = 0x00000100u,
  //! OpenType BLFont Variations feature is available.
  BL_FONT_FACE_FLAG_OPENTYPE_VARIATIONS       = 0x20000000u,
  //! Panose classification is available.
  BL_FONT_FACE_FLAG_PANOSE_DATA               = 0x00000200u,
  //! Unicode coverage information is available.
  BL_FONT_FACE_FLAG_UNICODE_COVERAGE          = 0x00000400u,
  //! Unicode variation sequences feature is available.
  BL_FONT_FACE_FLAG_VARIATION_SEQUENCES       = 0x10000000u,
  //! This is a symbol font.
  BL_FONT_FACE_FLAG_SYMBOL_FONT               = 0x40000000u,
  //! This is a last resort font.
  BL_FONT_FACE_FLAG_LAST_RESORT_FONT          = 0x80000000u
};

BL_DEFINE_ENUM(BLFontFaceDiagFlags) {
  //! Wront data in 'name' table.
  BL_FONT_FACE_DIAG_WRONG_NAME_DATA           = 0x00000001u,
  //! Fixed data read from 'name' table and possibly fixed font family/subfamily name.
  BL_FONT_FACE_DIAG_FIXED_NAME_DATA           = 0x00000002u,

  //! Wrong data in 'kern' table [kerning disabled].
  BL_FONT_FACE_DIAG_WRONG_KERN_DATA           = 0x00000004u,
  //! Fixed data read from 'kern' table so it can be used.
  BL_FONT_FACE_DIAG_FIXED_KERN_DATA           = 0x00000008u,

  //! Wrong data in 'cmap' table.
  BL_FONT_FACE_DIAG_WRONG_CMAP_DATA           = 0x00000010u,
  //! Wrong format in 'cmap' (sub)table.
  BL_FONT_FACE_DIAG_WRONG_CMAP_FORMAT         = 0x00000020u,

  //! Wrong data in 'GDEF' table.
  BL_FONT_FACE_DIAG_WRONG_GDEF_DATA           = 0x00000100u,
  //! Wrong data in 'GPOS' table.
  BL_FONT_FACE_DIAG_WRONG_GPOS_DATA           = 0x00000400u,
  //! Wrong data in 'GSUB' table.
  BL_FONT_FACE_DIAG_WRONG_GSUB_DATA           = 0x00001000u
};

BL_DEFINE_ENUM(BLFontLoaderFlags) {
  BL_FONT_LOADER_FLAG_COLLECTION              = 0x00000001u  //!< Font loader contains a font-collection (multiple font-faces).
};

//! Format of an outline stored in a font.
BL_DEFINE_ENUM(BLFontOutlineType) {
  //! None.
  BL_FONT_OUTLINE_TYPE_NONE                   = 0,
  //! Truetype outlines.
  BL_FONT_OUTLINE_TYPE_TRUETYPE               = 1,
  //! OpenType (CFF) outlines.
  BL_FONT_OUTLINE_TYPE_CFF                    = 2,
  //! OpenType (CFF2) outlines (font variations support).
  BL_FONT_OUTLINE_TYPE_CFF2                   = 3
};

//! Font stretch.
BL_DEFINE_ENUM(BLFontStretch) {
  //! Ultra condensed stretch.
  BL_FONT_STRETCH_ULTRA_CONDENSED             = 1,
  //! Extra condensed stretch.
  BL_FONT_STRETCH_EXTRA_CONDENSED             = 2,
  //! Condensed stretch.
  BL_FONT_STRETCH_CONDENSED                   = 3,
  //! Semi condensed stretch.
  BL_FONT_STRETCH_SEMI_CONDENSED              = 4,
  //! Normal stretch.
  BL_FONT_STRETCH_NORMAL                      = 5,
  //! Semi expanded stretch.
  BL_FONT_STRETCH_SEMI_EXPANDED               = 6,
  //! Expanded stretch.
  BL_FONT_STRETCH_EXPANDED                    = 7,
  //! Extra expanded stretch.
  BL_FONT_STRETCH_EXTRA_EXPANDED              = 8,
  //! Ultra expanded stretch.
  BL_FONT_STRETCH_ULTRA_EXPANDED              = 9
};

//! Font style.
BL_DEFINE_ENUM(BLFontStyle) {
  //! Normal style.
  BL_FONT_STYLE_NORMAL                        = 0,
  //! Oblique.
  BL_FONT_STYLE_OBLIQUE                       = 1,
  //! Italic.
  BL_FONT_STYLE_ITALIC                        = 2,

  //! Count of font styles.
  BL_FONT_STYLE_COUNT                         = 3
};

//! Font weight.
BL_DEFINE_ENUM(BLFontWeight) {
  //! Thin weight (100).
  BL_FONT_WEIGHT_THIN                         = 100,
  //! Extra light weight (200).
  BL_FONT_WEIGHT_EXTRA_LIGHT                  = 200,
  //! Light weight (300).
  BL_FONT_WEIGHT_LIGHT                        = 300,
  //! Semi light weight (350).
  BL_FONT_WEIGHT_SEMI_LIGHT                   = 350,
  //! Normal weight (400).
  BL_FONT_WEIGHT_NORMAL                       = 400,
  //! Medium weight (500).
  BL_FONT_WEIGHT_MEDIUM                       = 500,
  //! Semi bold weight (600).
  BL_FONT_WEIGHT_SEMI_BOLD                    = 600,
  //! Bold weight (700).
  BL_FONT_WEIGHT_BOLD                         = 700,
  //! Extra bold weight (800).
  BL_FONT_WEIGHT_EXTRA_BOLD                   = 800,
  //! Black weight (900).
  BL_FONT_WEIGHT_BLACK                        = 900,
  //! Extra black weight (950).
  BL_FONT_WEIGHT_EXTRA_BLACK                  = 950
};

//! Font string identifiers used by OpenType 'name' table.
BL_DEFINE_ENUM(BLFontStringId) {
  BL_FONT_STRING_COPYRIGHT_NOTICE             = 0,
  BL_FONT_STRING_FAMILY_NAME                  = 1,
  BL_FONT_STRING_SUBFAMILY_NAME               = 2,
  BL_FONT_STRING_UNIQUE_IDENTIFIER            = 3,
  BL_FONT_STRING_FULL_NAME                    = 4,
  BL_FONT_STRING_VERSION_STRING               = 5,
  BL_FONT_STRING_POST_SCRIPT_NAME             = 6,
  BL_FONT_STRING_TRADEMARK                    = 7,
  BL_FONT_STRING_MANUFACTURER_NAME            = 8,
  BL_FONT_STRING_DESIGNER_NAME                = 9,
  BL_FONT_STRING_DESCRIPTION                  = 10,
  BL_FONT_STRING_VENDOR_URL                   = 11,
  BL_FONT_STRING_DESIGNER_URL                 = 12,
  BL_FONT_STRING_LICENSE_DESCRIPTION          = 13,
  BL_FONT_STRING_LICENSE_INFO_URL             = 14,
  BL_FONT_STRING_RESERVED                     = 15,
  BL_FONT_STRING_TYPOGRAPHIC_FAMILY_NAME      = 16,
  BL_FONT_STRING_TYPOGRAPHIC_SUBFAMILY_NAME   = 17,
  BL_FONT_STRING_COMPATIBLE_FULL_NAME         = 18,
  BL_FONT_STRING_SAMPLE_TEXT                  = 19,
  BL_FONT_STRING_POST_SCRIPT_CID_NAME         = 20,
  BL_FONT_STRING_WWS_FAMILY_NAME              = 21,
  BL_FONT_STRING_WWS_SUBFAMILY_NAME           = 22,
  BL_FONT_STRING_LIGHT_BACKGROUND_PALETTE     = 23,
  BL_FONT_STRING_DARK_BACKGROUND_PALETTE      = 24,
  BL_FONT_STRING_VARIATIONS_POST_SCRIPT_PREFIX= 25,

  BL_FONT_STRING_COMMON_COUNT                 = 26,
  BL_FONT_STRING_CUSTOM_START_INDEX           = 255
};

//! Bit positions in `BLFontCoverage` structure.
//!
//! Each bit represents a range (or multiple ranges) of unicode characters.
BL_DEFINE_ENUM(BLFontCoverageIndex) {
  BL_FONT_COVERAGE_BASIC_LATIN,                              //!< [000000-00007F] Basic Latin.
  BL_FONT_COVERAGE_LATIN1_SUPPLEMENT,                        //!< [000080-0000FF] Latin-1 Supplement.
  BL_FONT_COVERAGE_LATIN_EXTENDED_A,                         //!< [000100-00017F] Latin Extended-A.
  BL_FONT_COVERAGE_LATIN_EXTENDED_B,                         //!< [000180-00024F] Latin Extended-B.
  BL_FONT_COVERAGE_IPA_EXTENSIONS,                           //!< [000250-0002AF] IPA Extensions.
                                                             //!< [001D00-001D7F] Phonetic Extensions.
                                                             //!< [001D80-001DBF] Phonetic Extensions Supplement.
  BL_FONT_COVERAGE_SPACING_MODIFIER_LETTERS,                 //!< [0002B0-0002FF] Spacing Modifier Letters.
                                                             //!< [00A700-00A71F] Modifier Tone Letters.
                                                             //!< [001DC0-001DFF] Combining Diacritical Marks Supplement.
  BL_FONT_COVERAGE_COMBINING_DIACRITICAL_MARKS,              //!< [000300-00036F] Combining Diacritical Marks.
  BL_FONT_COVERAGE_GREEK_AND_COPTIC,                         //!< [000370-0003FF] Greek and Coptic.
  BL_FONT_COVERAGE_COPTIC,                                   //!< [002C80-002CFF] Coptic.
  BL_FONT_COVERAGE_CYRILLIC,                                 //!< [000400-0004FF] Cyrillic.
                                                             //!< [000500-00052F] Cyrillic Supplement.
                                                             //!< [002DE0-002DFF] Cyrillic Extended-A.
                                                             //!< [00A640-00A69F] Cyrillic Extended-B.
  BL_FONT_COVERAGE_ARMENIAN,                                 //!< [000530-00058F] Armenian.
  BL_FONT_COVERAGE_HEBREW,                                   //!< [000590-0005FF] Hebrew.
  BL_FONT_COVERAGE_VAI,                                      //!< [00A500-00A63F] Vai.
  BL_FONT_COVERAGE_ARABIC,                                   //!< [000600-0006FF] Arabic.
                                                             //!< [000750-00077F] Arabic Supplement.
  BL_FONT_COVERAGE_NKO,                                      //!< [0007C0-0007FF] NKo.
  BL_FONT_COVERAGE_DEVANAGARI,                               //!< [000900-00097F] Devanagari.
  BL_FONT_COVERAGE_BENGALI,                                  //!< [000980-0009FF] Bengali.
  BL_FONT_COVERAGE_GURMUKHI,                                 //!< [000A00-000A7F] Gurmukhi.
  BL_FONT_COVERAGE_GUJARATI,                                 //!< [000A80-000AFF] Gujarati.
  BL_FONT_COVERAGE_ORIYA,                                    //!< [000B00-000B7F] Oriya.
  BL_FONT_COVERAGE_TAMIL,                                    //!< [000B80-000BFF] Tamil.
  BL_FONT_COVERAGE_TELUGU,                                   //!< [000C00-000C7F] Telugu.
  BL_FONT_COVERAGE_KANNADA,                                  //!< [000C80-000CFF] Kannada.
  BL_FONT_COVERAGE_MALAYALAM,                                //!< [000D00-000D7F] Malayalam.
  BL_FONT_COVERAGE_THAI,                                     //!< [000E00-000E7F] Thai.
  BL_FONT_COVERAGE_LAO,                                      //!< [000E80-000EFF] Lao.
  BL_FONT_COVERAGE_GEORGIAN,                                 //!< [0010A0-0010FF] Georgian.
                                                             //!< [002D00-002D2F] Georgian Supplement.
  BL_FONT_COVERAGE_BALINESE,                                 //!< [001B00-001B7F] Balinese.
  BL_FONT_COVERAGE_HANGUL_JAMO,                              //!< [001100-0011FF] Hangul Jamo.
  BL_FONT_COVERAGE_LATIN_EXTENDED_ADDITIONAL,                //!< [001E00-001EFF] Latin Extended Additional.
                                                             //!< [002C60-002C7F] Latin Extended-C.
                                                             //!< [00A720-00A7FF] Latin Extended-D.
  BL_FONT_COVERAGE_GREEK_EXTENDED,                           //!< [001F00-001FFF] Greek Extended.
  BL_FONT_COVERAGE_GENERAL_PUNCTUATION,                      //!< [002000-00206F] General Punctuation.
                                                             //!< [002E00-002E7F] Supplemental Punctuation.
  BL_FONT_COVERAGE_SUPERSCRIPTS_AND_SUBSCRIPTS,              //!< [002070-00209F] Superscripts And Subscripts.
  BL_FONT_COVERAGE_CURRENCY_SYMBOLS,                         //!< [0020A0-0020CF] Currency Symbols.
  BL_FONT_COVERAGE_COMBINING_DIACRITICAL_MARKS_FOR_SYMBOLS,  //!< [0020D0-0020FF] Combining Diacritical Marks For Symbols.
  BL_FONT_COVERAGE_LETTERLIKE_SYMBOLS,                       //!< [002100-00214F] Letterlike Symbols.
  BL_FONT_COVERAGE_NUMBER_FORMS,                             //!< [002150-00218F] Number Forms.
  BL_FONT_COVERAGE_ARROWS,                                   //!< [002190-0021FF] Arrows.
                                                             //!< [0027F0-0027FF] Supplemental Arrows-A.
                                                             //!< [002900-00297F] Supplemental Arrows-B.
                                                             //!< [002B00-002BFF] Miscellaneous Symbols and Arrows.
  BL_FONT_COVERAGE_MATHEMATICAL_OPERATORS,                   //!< [002200-0022FF] Mathematical Operators.
                                                             //!< [002A00-002AFF] Supplemental Mathematical Operators.
                                                             //!< [0027C0-0027EF] Miscellaneous Mathematical Symbols-A.
                                                             //!< [002980-0029FF] Miscellaneous Mathematical Symbols-B.
  BL_FONT_COVERAGE_MISCELLANEOUS_TECHNICAL,                  //!< [002300-0023FF] Miscellaneous Technical.
  BL_FONT_COVERAGE_CONTROL_PICTURES,                         //!< [002400-00243F] Control Pictures.
  BL_FONT_COVERAGE_OPTICAL_CHARACTER_RECOGNITION,            //!< [002440-00245F] Optical Character Recognition.
  BL_FONT_COVERAGE_ENCLOSED_ALPHANUMERICS,                   //!< [002460-0024FF] Enclosed Alphanumerics.
  BL_FONT_COVERAGE_BOX_DRAWING,                              //!< [002500-00257F] Box Drawing.
  BL_FONT_COVERAGE_BLOCK_ELEMENTS,                           //!< [002580-00259F] Block Elements.
  BL_FONT_COVERAGE_GEOMETRIC_SHAPES,                         //!< [0025A0-0025FF] Geometric Shapes.
  BL_FONT_COVERAGE_MISCELLANEOUS_SYMBOLS,                    //!< [002600-0026FF] Miscellaneous Symbols.
  BL_FONT_COVERAGE_DINGBATS,                                 //!< [002700-0027BF] Dingbats.
  BL_FONT_COVERAGE_CJK_SYMBOLS_AND_PUNCTUATION,              //!< [003000-00303F] CJK Symbols And Punctuation.
  BL_FONT_COVERAGE_HIRAGANA,                                 //!< [003040-00309F] Hiragana.
  BL_FONT_COVERAGE_KATAKANA,                                 //!< [0030A0-0030FF] Katakana.
                                                             //!< [0031F0-0031FF] Katakana Phonetic Extensions.
  BL_FONT_COVERAGE_BOPOMOFO,                                 //!< [003100-00312F] Bopomofo.
                                                             //!< [0031A0-0031BF] Bopomofo Extended.
  BL_FONT_COVERAGE_HANGUL_COMPATIBILITY_JAMO,                //!< [003130-00318F] Hangul Compatibility Jamo.
  BL_FONT_COVERAGE_PHAGS_PA,                                 //!< [00A840-00A87F] Phags-pa.
  BL_FONT_COVERAGE_ENCLOSED_CJK_LETTERS_AND_MONTHS,          //!< [003200-0032FF] Enclosed CJK Letters And Months.
  BL_FONT_COVERAGE_CJK_COMPATIBILITY,                        //!< [003300-0033FF] CJK Compatibility.
  BL_FONT_COVERAGE_HANGUL_SYLLABLES,                         //!< [00AC00-00D7AF] Hangul Syllables.
  BL_FONT_COVERAGE_NON_PLANE,                                //!< [00D800-00DFFF] Non-Plane 0 *.
  BL_FONT_COVERAGE_PHOENICIAN,                               //!< [010900-01091F] Phoenician.
  BL_FONT_COVERAGE_CJK_UNIFIED_IDEOGRAPHS,                   //!< [004E00-009FFF] CJK Unified Ideographs.
                                                             //!< [002E80-002EFF] CJK Radicals Supplement.
                                                             //!< [002F00-002FDF] Kangxi Radicals.
                                                             //!< [002FF0-002FFF] Ideographic Description Characters.
                                                             //!< [003400-004DBF] CJK Unified Ideographs Extension A.
                                                             //!< [020000-02A6DF] CJK Unified Ideographs Extension B.
                                                             //!< [003190-00319F] Kanbun.
  BL_FONT_COVERAGE_PRIVATE_USE_PLANE0,                       //!< [00E000-00F8FF] Private Use (Plane 0).
  BL_FONT_COVERAGE_CJK_STROKES,                              //!< [0031C0-0031EF] CJK Strokes.
                                                             //!< [00F900-00FAFF] CJK Compatibility Ideographs.
                                                             //!< [02F800-02FA1F] CJK Compatibility Ideographs Supplement.
  BL_FONT_COVERAGE_ALPHABETIC_PRESENTATION_FORMS,            //!< [00FB00-00FB4F] Alphabetic Presentation Forms.
  BL_FONT_COVERAGE_ARABIC_PRESENTATION_FORMS_A,              //!< [00FB50-00FDFF] Arabic Presentation Forms-A.
  BL_FONT_COVERAGE_COMBINING_HALF_MARKS,                     //!< [00FE20-00FE2F] Combining Half Marks.
  BL_FONT_COVERAGE_VERTICAL_FORMS,                           //!< [00FE10-00FE1F] Vertical Forms.
                                                             //!< [00FE30-00FE4F] CJK Compatibility Forms.
  BL_FONT_COVERAGE_SMALL_FORM_VARIANTS,                      //!< [00FE50-00FE6F] Small Form Variants.
  BL_FONT_COVERAGE_ARABIC_PRESENTATION_FORMS_B,              //!< [00FE70-00FEFF] Arabic Presentation Forms-B.
  BL_FONT_COVERAGE_HALFWIDTH_AND_FULLWIDTH_FORMS,            //!< [00FF00-00FFEF] Halfwidth And Fullwidth Forms.
  BL_FONT_COVERAGE_SPECIALS,                                 //!< [00FFF0-00FFFF] Specials.
  BL_FONT_COVERAGE_TIBETAN,                                  //!< [000F00-000FFF] Tibetan.
  BL_FONT_COVERAGE_SYRIAC,                                   //!< [000700-00074F] Syriac.
  BL_FONT_COVERAGE_THAANA,                                   //!< [000780-0007BF] Thaana.
  BL_FONT_COVERAGE_SINHALA,                                  //!< [000D80-000DFF] Sinhala.
  BL_FONT_COVERAGE_MYANMAR,                                  //!< [001000-00109F] Myanmar.
  BL_FONT_COVERAGE_ETHIOPIC,                                 //!< [001200-00137F] Ethiopic.
                                                             //!< [001380-00139F] Ethiopic Supplement.
                                                             //!< [002D80-002DDF] Ethiopic Extended.
  BL_FONT_COVERAGE_CHEROKEE,                                 //!< [0013A0-0013FF] Cherokee.
  BL_FONT_COVERAGE_UNIFIED_CANADIAN_ABORIGINAL_SYLLABICS,    //!< [001400-00167F] Unified Canadian Aboriginal Syllabics.
  BL_FONT_COVERAGE_OGHAM,                                    //!< [001680-00169F] Ogham.
  BL_FONT_COVERAGE_RUNIC,                                    //!< [0016A0-0016FF] Runic.
  BL_FONT_COVERAGE_KHMER,                                    //!< [001780-0017FF] Khmer.
                                                             //!< [0019E0-0019FF] Khmer Symbols.
  BL_FONT_COVERAGE_MONGOLIAN,                                //!< [001800-0018AF] Mongolian.
  BL_FONT_COVERAGE_BRAILLE_PATTERNS,                         //!< [002800-0028FF] Braille Patterns.
  BL_FONT_COVERAGE_YI_SYLLABLES_AND_RADICALS,                //!< [00A000-00A48F] Yi Syllables.
                                                             //!< [00A490-00A4CF] Yi Radicals.
  BL_FONT_COVERAGE_TAGALOG_HANUNOO_BUHID_TAGBANWA,           //!< [001700-00171F] Tagalog.
                                                             //!< [001720-00173F] Hanunoo.
                                                             //!< [001740-00175F] Buhid.
                                                             //!< [001760-00177F] Tagbanwa.
  BL_FONT_COVERAGE_OLD_ITALIC,                               //!< [010300-01032F] Old Italic.
  BL_FONT_COVERAGE_GOTHIC,                                   //!< [010330-01034F] Gothic.
  BL_FONT_COVERAGE_DESERET,                                  //!< [010400-01044F] Deseret.
  BL_FONT_COVERAGE_MUSICAL_SYMBOLS,                          //!< [01D000-01D0FF] Byzantine Musical Symbols.
                                                             //!< [01D100-01D1FF] Musical Symbols.
                                                             //!< [01D200-01D24F] Ancient Greek Musical Notation.
  BL_FONT_COVERAGE_MATHEMATICAL_ALPHANUMERIC_SYMBOLS,        //!< [01D400-01D7FF] Mathematical Alphanumeric Symbols.
  BL_FONT_COVERAGE_PRIVATE_USE_PLANE_15_16,                  //!< [0F0000-0FFFFD] Private Use (Plane 15).
                                                             //!< [100000-10FFFD] Private Use (Plane 16).
  BL_FONT_COVERAGE_VARIATION_SELECTORS,                      //!< [00FE00-00FE0F] Variation Selectors.
                                                             //!< [0E0100-0E01EF] Variation Selectors Supplement.
  BL_FONT_COVERAGE_TAGS,                                     //!< [0E0000-0E007F] Tags.
  BL_FONT_COVERAGE_LIMBU,                                    //!< [001900-00194F] Limbu.
  BL_FONT_COVERAGE_TAI_LE,                                   //!< [001950-00197F] Tai Le.
  BL_FONT_COVERAGE_NEW_TAI_LUE,                              //!< [001980-0019DF] New Tai Lue.
  BL_FONT_COVERAGE_BUGINESE,                                 //!< [001A00-001A1F] Buginese.
  BL_FONT_COVERAGE_GLAGOLITIC,                               //!< [002C00-002C5F] Glagolitic.
  BL_FONT_COVERAGE_TIFINAGH,                                 //!< [002D30-002D7F] Tifinagh.
  BL_FONT_COVERAGE_YIJING_HEXAGRAM_SYMBOLS,                  //!< [004DC0-004DFF] Yijing Hexagram Symbols.
  BL_FONT_COVERAGE_SYLOTI_NAGRI,                             //!< [00A800-00A82F] Syloti Nagri.
  BL_FONT_COVERAGE_LINEAR_B_SYLLABARY_AND_IDEOGRAMS,         //!< [010000-01007F] Linear B Syllabary.
                                                             //!< [010080-0100FF] Linear B Ideograms.
                                                             //!< [010100-01013F] Aegean Numbers.
  BL_FONT_COVERAGE_ANCIENT_GREEK_NUMBERS,                    //!< [010140-01018F] Ancient Greek Numbers.
  BL_FONT_COVERAGE_UGARITIC,                                 //!< [010380-01039F] Ugaritic.
  BL_FONT_COVERAGE_OLD_PERSIAN,                              //!< [0103A0-0103DF] Old Persian.
  BL_FONT_COVERAGE_SHAVIAN,                                  //!< [010450-01047F] Shavian.
  BL_FONT_COVERAGE_OSMANYA,                                  //!< [010480-0104AF] Osmanya.
  BL_FONT_COVERAGE_CYPRIOT_SYLLABARY,                        //!< [010800-01083F] Cypriot Syllabary.
  BL_FONT_COVERAGE_KHAROSHTHI,                               //!< [010A00-010A5F] Kharoshthi.
  BL_FONT_COVERAGE_TAI_XUAN_JING_SYMBOLS,                    //!< [01D300-01D35F] Tai Xuan Jing Symbols.
  BL_FONT_COVERAGE_CUNEIFORM,                                //!< [012000-0123FF] Cuneiform.
                                                             //!< [012400-01247F] Cuneiform Numbers and Punctuation.
  BL_FONT_COVERAGE_COUNTING_ROD_NUMERALS,                    //!< [01D360-01D37F] Counting Rod Numerals.
  BL_FONT_COVERAGE_SUNDANESE,                                //!< [001B80-001BBF] Sundanese.
  BL_FONT_COVERAGE_LEPCHA,                                   //!< [001C00-001C4F] Lepcha.
  BL_FONT_COVERAGE_OL_CHIKI,                                 //!< [001C50-001C7F] Ol Chiki.
  BL_FONT_COVERAGE_SAURASHTRA,                               //!< [00A880-00A8DF] Saurashtra.
  BL_FONT_COVERAGE_KAYAH_LI,                                 //!< [00A900-00A92F] Kayah Li.
  BL_FONT_COVERAGE_REJANG,                                   //!< [00A930-00A95F] Rejang.
  BL_FONT_COVERAGE_CHAM,                                     //!< [00AA00-00AA5F] Cham.
  BL_FONT_COVERAGE_ANCIENT_SYMBOLS,                          //!< [010190-0101CF] Ancient Symbols.
  BL_FONT_COVERAGE_PHAISTOS_DISC,                            //!< [0101D0-0101FF] Phaistos Disc.
  BL_FONT_COVERAGE_CARIAN_LYCIAN_LYDIAN,                     //!< [0102A0-0102DF] Carian.
                                                             //!< [010280-01029F] Lycian.
                                                             //!< [010920-01093F] Lydian.
  BL_FONT_COVERAGE_DOMINO_AND_MAHJONG_TILES,                 //!< [01F030-01F09F] Domino Tiles.
                                                             //!< [01F000-01F02F] Mahjong Tiles.
  BL_FONT_COVERAGE_INTERNAL_USAGE_123,                       //!< Reserved for internal usage (123).
  BL_FONT_COVERAGE_INTERNAL_USAGE_124,                       //!< Reserved for internal usage (124).
  BL_FONT_COVERAGE_INTERNAL_USAGE_125,                       //!< Reserved for internal usage (125).
  BL_FONT_COVERAGE_INTERNAL_USAGE_126,                       //!< Reserved for internal usage (126).
  BL_FONT_COVERAGE_INTERNAL_USAGE_127                        //!< Reserved for internal usage (127).
};

//! Text direction.
BL_DEFINE_ENUM(BLTextDirection) {
  //! Left-to-right direction.
  BL_TEXT_DIRECTION_LTR = 0,
  //! Right-to-left direction.
  BL_TEXT_DIRECTION_RTL = 1,

  //! Count of text direction types.
  BL_TEXT_DIRECTION_COUNT = 2
};

//! Text orientation.
BL_DEFINE_ENUM(BLTextOrientation) {
  //! Horizontal orientation.
  BL_TEXT_ORIENTATION_HORIZONTAL = 0,
  //! Vertical orientation.
  BL_TEXT_ORIENTATION_VERTICAL = 1,

  //! Count of text orientation types.
  BL_TEXT_ORIENTATION_COUNT = 2
};

// ============================================================================
// [BLGlyphItem]
// ============================================================================

//! Glyph item as a data structure that represents either a unicode character
//! or glyph. It contains data used by `BLGlyphBuffer` and is visible to end
//! users so they can inspect either the text stored in `BLGlyphBuffer` or its
//! glyph-run representation.
struct BLGlyphItem {
  union {
    uint32_t value;
    struct {
    #if BL_BUILD_BYTE_ORDER == 1234
      BLGlyphId glyphId;
      uint16_t reserved;
    #else
      uint16_t reserved;
      BLGlyphId glyphId;
    #endif
    };
  };

  // --------------------------------------------------------------------------
  #ifdef __cplusplus

  BL_INLINE void reset() noexcept { this->value = 0; }

  #endif
  // --------------------------------------------------------------------------
};

// ============================================================================
// [BLGlyphInfo]
// ============================================================================

//! Glyph information contains additional information to `BLGlyphItem` used by
//! `BLGlyphBuffer`.
struct BLGlyphInfo {
  uint32_t cluster;
  uint32_t reserved[2];

  // --------------------------------------------------------------------------
  #ifdef __cplusplus

  BL_INLINE void reset() noexcept { this->cluster = 0; }

  #endif
  // --------------------------------------------------------------------------
};

// ============================================================================
// [BLGlyphPlacement]
// ============================================================================

//! Glyph placement.
//!
//! Provides information about glyph offset (x/y) and advance (x/y).
struct BLGlyphPlacement {
  BLPointI placement;
  BLPointI advance;

  // --------------------------------------------------------------------------
  #ifdef __cplusplus

  BL_INLINE void reset() noexcept {
    this->placement.reset();
    this->advance.reset();
  }

  #endif
  // --------------------------------------------------------------------------
};

// ============================================================================
// [BLGlyphMappingState]
// ============================================================================

//! Character to glyph mapping state.
struct BLGlyphMappingState {
  //! Number of glyphs or glyph-items on output.
  size_t glyphCount;
  //! Index of the first undefined glyph (SIZE_MAX if none).
  size_t undefinedFirst;
  //! Undefined glyph count (chars that have no mapping).
  size_t undefinedCount;

  // --------------------------------------------------------------------------
  #ifdef __cplusplus
  BL_INLINE void reset() noexcept {
    glyphCount = 0;
    undefinedCount = 0;
    undefinedFirst = 0;
  }
  #endif
  // --------------------------------------------------------------------------
};

// ============================================================================
// [BLGlyphOutlineSinkInfo]
// ============================================================================

//! Information passed to a `BLPathSinkFunc` sink by `BLFont::getGlyphOutlines()`.
struct BLGlyphOutlineSinkInfo {
  size_t glyphIndex;
  size_t contourCount;
};

// ============================================================================
// [BLGlyphRun]
// ============================================================================

//! BLGlyphRun describes a set of consecutive glyphs and their placements.
//!
//! BLGlyphRun should only be used to pass glyph IDs and their placements to the
//! rendering context. The purpose of BLGlyphRun is to allow rendering glyphs,
//! which could be shaped by various shaping engines (Blend2D, Harfbuzz, etc).
//!
//! BLGlyphRun allows to render glyphs that are part of a bigger structure (for
//! example `BLGlyphItem`), raw array like `BLGlyphId[]`, etc. Glyph placements
//! at the moment use Blend2D's `BLGlyphPlacement` or `BLPoint`, but it's
//! possible to extend the data type in the future.
//!
//! See `BLGlyphRunPlacement` for placement modes provided by Blend2D.
struct BLGlyphRun {
  //! Glyph id array (abstract, incremented by `glyphIdAdvance`).
  void* glyphIdData;
  //! Glyph placement array (abstract, incremented by `placementAdvance`).
  void* placementData;
  //! Size of the glyph-run in glyph units.
  size_t size;
  //! Size of a `glyphId` - must be either 2 (uint16_t) or 4 (uint32_t) bytes.
  uint8_t glyphIdSize;
  //! Type of the placement, see `BLGlyphPlacementType`.
  uint8_t placementType;
  //! Advance of `glyphIdData` array.
  int8_t glyphIdAdvance;
  //! Advance of `placementData` array.
  int8_t placementAdvance;
  //! Glyph-run flags.
  uint32_t flags;

  // --------------------------------------------------------------------------
  #ifdef __cplusplus

  BL_INLINE bool empty() const noexcept { return size == 0; }
  BL_INLINE void reset() noexcept { memset(this, 0, sizeof(*this)); }

  template<typename T>
  BL_INLINE T* glyphIdDataAs() const noexcept { return static_cast<T*>(glyphIdData); }

  template<typename T>
  BL_INLINE T* placementDataAs() const noexcept { return static_cast<T*>(placementData); }

  BL_INLINE void setGlyphIdData(const BLGlyphId* glyphIds) noexcept { setGlyphIdData(glyphIds, intptr_t(sizeof(BLGlyphId))); }
  BL_INLINE void setGlyphItemData(const BLGlyphItem* itemData) noexcept { setGlyphIdData(itemData, intptr_t(sizeof(BLGlyphItem))); }

  BL_INLINE void setGlyphIdData(const void* data, intptr_t advance) noexcept {
    this->glyphIdData = const_cast<void*>(data);
    this->glyphIdAdvance = int8_t(advance);
  }

  BL_INLINE void resetGlyphIdData() noexcept {
    this->glyphIdData = nullptr;
    this->glyphIdAdvance = 0;
  }

  template<typename T>
  BL_INLINE void setPlacementData(const T* data) noexcept {
    setPlacementData(data, sizeof(T));
  }

  BL_INLINE void setPlacementData(const void* data, intptr_t advance) noexcept {
    this->placementData = const_cast<void*>(data);
    this->placementAdvance = int8_t(advance);
  }

  BL_INLINE void resetPlacementData() noexcept {
    this->placementData = nullptr;
    this->placementAdvance = 0;
  }

  #endif
  // --------------------------------------------------------------------------
};

// ============================================================================
// [BLGlyphRunIterator]
// ============================================================================

#ifdef __cplusplus
//! A helper to iterate over a `BLGlyphRun`.
//!
//! Takes into consideration glyph-id advance and glyph-offset advance.
//!
//! Example:
//!
//! ```
//! void inspectGlyphRun(const BLGlyphRun& glyphRun) noexcept {
//!   BLGlyphRunIterator it(glyphRun);
//!   if (it.hasOffsets()) {
//!     while (!it.atEnd()) {
//!       BLGlyphId glyphId = it.glyphId();
//!       BLPoint offset = it.placement();
//!
//!       // Do something with `glyphId` and `offset`.
//!
//!       it.advance();
//!     }
//!   }
//!   else {
//!     while (!it.atEnd()) {
//!       BLGlyphId glyphId = it.glyphId();
//!
//!       // Do something with `glyphId`.
//!
//!       it.advance();
//!     }
//!   }
//! }
//! ```
class BLGlyphRunIterator {
public:
  size_t index;
  size_t size;
  void* glyphIdData;
  void* placementData;
  intptr_t glyphIdAdvance;
  intptr_t placementAdvance;

  BL_INLINE BLGlyphRunIterator() noexcept { reset(); }
  BL_INLINE explicit BLGlyphRunIterator(const BLGlyphRun& glyphRun) noexcept { reset(glyphRun); }

  BL_INLINE void reset() noexcept {
    index = 0;
    size = 0;
    glyphIdData = nullptr;
    placementData = nullptr;
    glyphIdAdvance = 0;
    placementAdvance = 0;
  }

  BL_INLINE void reset(const BLGlyphRun& glyphRun) noexcept {
    index = 0;
    size = glyphRun.size;
    glyphIdData = glyphRun.glyphIdData;
    placementData = glyphRun.placementData;
    glyphIdAdvance = glyphRun.glyphIdAdvance;
    placementAdvance = glyphRun.placementAdvance;

    if (BL_BYTE_ORDER_NATIVE == BL_BYTE_ORDER_BE)
      glyphIdData = static_cast<uint8_t*>(glyphIdData) + (blMax<size_t>(glyphRun.glyphIdSize, 2) - 2);
  }

  BL_INLINE bool empty() const noexcept { return size == 0; }
  BL_INLINE bool atEnd() const noexcept { return index == size; }
  BL_INLINE bool hasPlacement() const noexcept { return placementData != nullptr; }

  BL_INLINE BLGlyphId glyphId() const noexcept { return *static_cast<const BLGlyphId*>(glyphIdData); }

  template<typename T>
  BL_INLINE const T& placement() const noexcept { return *static_cast<const T*>(placementData); }

  BL_INLINE void advance() noexcept {
    BL_ASSERT(!atEnd());

    index++;
    glyphIdData = static_cast<uint8_t*>(glyphIdData) + glyphIdAdvance;
    placementData = static_cast<uint8_t*>(placementData) + placementAdvance;
  }
};
#endif

// ============================================================================
// [BLFontTable]
// ============================================================================

//! A read only data that represents a font table or its sub-table.
struct BLFontTable {
  //! Pointer to the beginning of the data interpreted as `uint8_t*`.
  const uint8_t* data;
  //! Size of `data` in bytes.
  size_t size;

  // --------------------------------------------------------------------------
  #ifdef __cplusplus

  BL_INLINE void reset() noexcept {
    this->data = nullptr;
    this->size = 0;
  }

  template<typename T>
  BL_INLINE const T* dataAs() const noexcept { return reinterpret_cast<const T*>(data); }

  #endif
  // --------------------------------------------------------------------------
};

// ============================================================================
// [BLFontFeature]
// ============================================================================

//! Associates a value with a generic font feature where `tag` describes the
//! feature (as provided by the font) and `value` describes its value. Some
//! features only allow boolean values 0 and 1 and some also allow higher
//! values up to 65535.
//!
//! Registered OpenType features:
//!   - https://docs.microsoft.com/en-us/typography/opentype/spec/featuretags
//!   - https://helpx.adobe.com/typekit/using/open-type-syntax.html
struct BLFontFeature {
  //! Feature tag (32-bit).
  BLTag tag;
  //! Feature value (should not be greater than 65535).
  uint32_t value;

  // --------------------------------------------------------------------------
  #ifdef __cplusplus

  BL_INLINE void reset() noexcept {
    this->tag = 0;
    this->value = 0;
  }

  #endif
  // --------------------------------------------------------------------------
};

// ============================================================================
// [BLFontVariation]
// ============================================================================

//! Associates a value with a font variation feature where `tag` describes
//! variation axis and `value` defines its value.
struct BLFontVariation {
  //! Variation tag (32-bit).
  BLTag tag;
  //! Variation value.
  float value;

  // --------------------------------------------------------------------------
  #ifdef __cplusplus

  BL_INLINE void reset() noexcept {
    this->tag = 0;
    this->value = 0.0f;
  }

  #endif
  // --------------------------------------------------------------------------
};

// ============================================================================
// [BLFontCoverage]
// ============================================================================

//! Font unicode coverage.
//!
//! Unicode coverage describes which unicode characters are provided by a font.
//! Blend2D accesses this information by reading "OS/2" table, if available.
struct BLFontCoverage {
  uint32_t data[4];

  // --------------------------------------------------------------------------
  #ifdef __cplusplus

  BL_INLINE void reset() noexcept {
    this->data[0] = 0;
    this->data[1] = 0;
    this->data[2] = 0;
    this->data[3] = 0;
  }

  BL_INLINE bool empty() const noexcept {
    return (this->data[0] |
            this->data[1] |
            this->data[2] |
            this->data[3]) == 0;
  }

  BL_INLINE bool hasBit(uint32_t index) const noexcept {
    return ((this->data[index / 32] >> (index % 32)) & 0x1) != 0;
  }

  BL_INLINE void setBit(uint32_t index) noexcept {
    this->data[index / 32] |= uint32_t(1) << (index % 32);
  }

  BL_INLINE void clearBit(uint32_t index) noexcept {
    this->data[index / 32] &= ~(uint32_t(1) << (index % 32));
  }

  BL_INLINE bool equals(const BLFontCoverage& other) const noexcept {
    return blEquals(this->data[0], other.data[0]) &
           blEquals(this->data[1], other.data[1]) &
           blEquals(this->data[2], other.data[2]) &
           blEquals(this->data[3], other.data[3]) ;
  }

  BL_INLINE bool operator==(const BLFontCoverage& other) const noexcept { return  equals(other); }
  BL_INLINE bool operator!=(const BLFontCoverage& other) const noexcept { return !equals(other); }

  #endif
  // --------------------------------------------------------------------------
};

// ============================================================================
// [BLFontPanose]
// ============================================================================

//! Font PANOSE classification.
struct BLFontPanose {
  union {
    uint8_t data[10];
    uint8_t familyKind;

    struct {
      uint8_t familyKind;
      uint8_t serifStyle;
      uint8_t weight;
      uint8_t proportion;
      uint8_t contrast;
      uint8_t strokeVariation;
      uint8_t armStyle;
      uint8_t letterform;
      uint8_t midline;
      uint8_t xHeight;
    } text;

    struct {
      uint8_t familyKind;
      uint8_t toolKind;
      uint8_t weight;
      uint8_t spacing;
      uint8_t aspectRatio;
      uint8_t contrast;
      uint8_t topology;
      uint8_t form;
      uint8_t finials;
      uint8_t xAscent;
    } script;

    struct {
      uint8_t familyKind;
      uint8_t decorativeClass;
      uint8_t weight;
      uint8_t aspect;
      uint8_t contrast;
      uint8_t serifVariant;
      uint8_t treatment;
      uint8_t lining;
      uint8_t topology;
      uint8_t characterRange;
    } decorative;

    struct {
      uint8_t familyKind;
      uint8_t symbolKind;
      uint8_t weight;
      uint8_t spacing;
      uint8_t aspectRatioAndContrast;
      uint8_t aspectRatio94;
      uint8_t aspectRatio119;
      uint8_t aspectRatio157;
      uint8_t aspectRatio163;
      uint8_t aspectRatio211;
    } symbol;
  };

  // --------------------------------------------------------------------------
  #ifdef __cplusplus

  BL_INLINE void reset() noexcept {
    memset(this, 0, sizeof(*this));
  }

  BL_INLINE bool empty() const noexcept {
    return (this->data[0] == 0) &
           (this->data[1] == 0) &
           (this->data[2] == 0) &
           (this->data[3] == 0) &
           (this->data[4] == 0) &
           (this->data[5] == 0) &
           (this->data[6] == 0) &
           (this->data[7] == 0) &
           (this->data[8] == 0) &
           (this->data[9] == 0) ;
  }

  #endif
  // --------------------------------------------------------------------------
};

// ============================================================================
// [BLFontMatrix]
// ============================================================================

//! 2x2 transformation matrix used by `BLFont`. It's similar to `BLMatrix2D`,
//! however, it doesn't provide a translation part as it's assumed to be zero.
struct BLFontMatrix {
  union {
    double m[4];
    struct {
      double m00;
      double m01;
      double m10;
      double m11;
    };
  };

  // --------------------------------------------------------------------------
  #ifdef __cplusplus

  BL_INLINE BLFontMatrix() noexcept = default;
  BL_INLINE BLFontMatrix(const BLFontMatrix& other) noexcept = default;

  BL_INLINE BLFontMatrix(double m00, double m01, double m10, double m11) noexcept
    : m00(m00), m01(m01), m10(m10), m11(m11) {}

  BL_INLINE void reset() noexcept {
    this->m00 = 1.0;
    this->m01 = 0.0;
    this->m10 = 0.0;
    this->m11 = 1.0;
  }

  BL_INLINE void reset(double m00, double m01, double m10, double m11) noexcept {
    this->m00 = m00;
    this->m01 = m01;
    this->m10 = m10;
    this->m11 = m11;
  }

  #endif
  // --------------------------------------------------------------------------
};

// ============================================================================
// [BLFontMetrics]
// ============================================================================

//! Scaled `BLFontDesignMetrics` based on font size and other properties.
struct BLFontMetrics {
  //! Font size.
  float size;

  union {
    struct {
      //! Font ascent (horizontal orientation).
      float ascent;
      //! Font ascent (vertical orientation).
      float vAscent;
    };
    float ascentByOrientation[2];
  };

  union {
    struct {
      //! Font descent (horizontal orientation).
      float descent;
      //! Font descent (vertical orientation).
      float vDescent;
    };
    float descentByOrientation[2];
  };

  //! Line gap.
  float lineGap;
  //! Distance between the baseline and the mean line of lower-case letters.
  float xHeight;
  //! Maximum height of a capital letter above the baseline.
  float capHeight;

  //! Text underline position.
  float underlinePosition;
  //! Text underline thickness.
  float underlineThickness;
  //! Text strikethrough position.
  float strikethroughPosition;
  //! Text strikethrough thickness.
  float strikethroughThickness;

  // --------------------------------------------------------------------------
  #ifdef __cplusplus

  BL_INLINE void reset() noexcept { memset(this, 0, sizeof(*this)); }

  #endif
  // --------------------------------------------------------------------------
};

// ============================================================================
// [BLFontDesignMetrics]
// ============================================================================

//! Design metrics of a font.
//!
//! Design metrics is information that `BLFontFace` collected directly from the
//! font data. It means that all fields are measured in font design units.
//!
//! When a new `BLFont` instance is created a scaled metrics `BLFontMetrics` is
//! automatically calculated from `BLFontDesignMetrics` including other members
//! like transformation, etc...
struct BLFontDesignMetrics {
  //! Units per EM square.
  int unitsPerEm;
  //! Line gap.
  int lineGap;
  //! Distance between the baseline and the mean line of lower-case letters.
  int xHeight;
  //! Maximum height of a capital letter above the baseline.
  int capHeight;

  union {
    struct {
      //! Ascent (horizontal).
      int ascent;
      //! Ascent (vertical).
      int vAscent;
    };
    //! Horizontal & vertical ascents.
    int ascentByOrientation[2];
  };

  union {
    struct {
      //! Descent (horizontal).
      int descent;
      //! Descent (vertical).
      int vDescent;
    };
    //! Horizontal & vertical descents.
    int descentByOrientation[2];
  };

  union {
    struct {
      //! Minimum leading-side bearing (horizontal).
      int hMinLSB;
      //! Minimum leading-side bearing (vertical).
      int vMinLSB;
    };
    //! Minimum leading-side bearing (horizontal and vertical).
    int minLSBByOrientation[2];
  };

  union {
    struct {
      //! Minimum trailing-side bearing (horizontal).
      int hMinTSB;
      //! Minimum trailing-side bearing (vertical).
      int vMinTSB;
    };
    //! Minimum trailing-side bearing (horizontal and vertical)..
    int minTSBByOrientation[2];
  };

  union {
    struct {
      //! Maximum horizontal advance.
      int hMaxAdvance;
      //! Maximum vertical advance.
      int vMaxAdvance;
    };
    //! Maximum advance width (horizontal) and height (vertical).
    int maxAdvanceByOrientation[2];
  };

  //! Text underline position.
  int underlinePosition;
  //! Text underline thickness.
  int underlineThickness;
  //! Text strikethrough position.
  int strikethroughPosition;
  //! Text strikethrough thickness.
  int strikethroughThickness;

  // --------------------------------------------------------------------------
  #ifdef __cplusplus

  BL_INLINE void reset() noexcept { memset(this, 0, sizeof(*this)); }

  #endif
  // --------------------------------------------------------------------------
};

// ============================================================================
// [BLTextMetrics]
// ============================================================================

//! Text metrics.
struct BLTextMetrics {
  BLPoint advance;
  BLBox boundingBox;

  // --------------------------------------------------------------------------
  #ifdef __cplusplus

  BL_INLINE void reset() noexcept {
    this->advance.reset();
    this->boundingBox.reset();
  }

  #endif
  // --------------------------------------------------------------------------
};

//! \}

#endif // BLEND2D_BLFONTDEFS_H
