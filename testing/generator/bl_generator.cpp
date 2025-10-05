// bl_generator is an application that generates code that is part of Blend2D. It started as a simple-hash generator
// to be able to convert OpenType tags to internal IDs for faster processing, however, it may grow in the future and
// generate more code.

#include <stdint.h>
#include "bl_generator.h"

// Supplement some Blend2D types and definitions (this generator is totally standalone).
#define BLEND2D_API_BUILD_P_H_INCLUDED
#define BLEND2D_API_INTERNAL_P_H_INCLUDED

typedef uint32_t BLTag;

#define BL_HIDDEN
#define BL_INLINE inline
#define BL_MAKE_TAG(A, B, C, D) ((BLTag)(((BLTag)(A) << 24) | ((BLTag)(B) << 16) | ((BLTag)(C) << 8) | ((BLTag)(D))))

#include "../../src/blend2d/fonttagdataids_p.h"
#include "../../src/blend2d/fonttagdataids.cpp"

int main() {
  printf("-- Finding table tags to ids hash function --\n");
  StupidHash::Finder table_tags_finder(bl::FontTagData::table_id_to_tag_table, bl::FontTagData::kTableIdCount);
  if (!table_tags_finder.find_solution()) {
    printf("Solution not found!\n");
    return 1;
  }

  printf("-- Finding script tags to ids hash function --\n");
  StupidHash::Finder script_tags_finder(bl::FontTagData::script_id_to_tag_table, bl::FontTagData::kScriptIdCount);
  if (!script_tags_finder.find_solution()) {
    printf("Solution not found!\n");
    return 1;
  }

  printf("-- Finding language tags to ids hash function --\n");
  StupidHash::Finder language_tags_finder(bl::FontTagData::language_id_to_tag_table, bl::FontTagData::kLanguageIdCount);
  if (!language_tags_finder.find_solution()) {
    printf("Solution not found!\n");
    return 1;
  }

  printf("-- Finding feature tags to ids hash function --\n");
  StupidHash::Finder feature_tags_finder(bl::FontTagData::feature_id_to_tag_table, bl::FontTagData::kFeatureIdCount);
  if (!feature_tags_finder.find_solution()) {
    printf("Solution not found!\n");
    return 1;
  }

  printf("-- Finding baseline tags to ids hash function --\n");
  StupidHash::Finder baseline_tags_finder(bl::FontTagData::baseline_id_to_tag_table, bl::FontTagData::kBaselineIdCount);
  if (!baseline_tags_finder.find_solution()) {
    printf("Solution not found!\n");
    return 1;
  }

  printf("-- Finding variation tags to ids hash function --\n");
  StupidHash::Finder variation_tags_finder(bl::FontTagData::variation_id_to_tag_table, bl::FontTagData::kVariationIdCount);
  if (!variation_tags_finder.find_solution()) {
    printf("Solution not found!\n");
    return 1;
  }

  printf("%s\n", table_tags_finder._hf.body("static BL_INLINE uint32_t table_tag_to_id(uint32_t tag) noexcept", "tag", "table_id_to_tag_table[", "]").data());
  printf("%s\n", script_tags_finder._hf.body("static BL_INLINE uint32_t script_tag_to_id(uint32_t tag) noexcept", "tag", "script_id_to_tag_table[", "]").data());
  printf("%s\n", language_tags_finder._hf.body("static BL_INLINE uint32_t language_tag_to_id(uint32_t tag) noexcept", "tag", "language_id_to_tag_table[", "]").data());
  printf("%s\n", feature_tags_finder._hf.body("static BL_INLINE uint32_t feature_tag_to_id(uint32_t tag) noexcept", "tag", "feature_id_to_tag_table[", "]").data());
  printf("%s\n", baseline_tags_finder._hf.body("static BL_INLINE uint32_t baseline_tag_to_id(uint32_t tag) noexcept", "tag", "baseline_id_to_tag_table[", "]").data());
  printf("%s\n", variation_tags_finder._hf.body("static BL_INLINE uint32_t variation_tag_to_id(uint32_t tag) noexcept", "tag", "variation_id_to_tag_table[", "]").data());

  return 0;
}