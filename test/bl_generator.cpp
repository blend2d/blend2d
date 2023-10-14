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

#include "../src/blend2d/fonttagdataids_p.h"
#include "../src/blend2d/fonttagdataids.cpp"

int main(int argc, char* argv[]) {
  printf("-- Finding table tags to ids hash function --\n");
  StupidHash::Finder tableTagsFinder(bl::FontTagData::tableIdToTagTable, bl::FontTagData::kTableIdCount);
  if (!tableTagsFinder.findSolution()) {
    printf("Solution not found!\n");
    return 1;
  }

  printf("-- Finding script tags to ids hash function --\n");
  StupidHash::Finder scriptTagsFinder(bl::FontTagData::scriptIdToTagTable, bl::FontTagData::kScriptIdCount);
  if (!scriptTagsFinder.findSolution()) {
    printf("Solution not found!\n");
    return 1;
  }

  printf("-- Finding language tags to ids hash function --\n");
  StupidHash::Finder languageTagsFinder(bl::FontTagData::languageIdToTagTable, bl::FontTagData::kLanguageIdCount);
  if (!languageTagsFinder.findSolution()) {
    printf("Solution not found!\n");
    return 1;
  }

  printf("-- Finding feature tags to ids hash function --\n");
  StupidHash::Finder featureTagsFinder(bl::FontTagData::featureIdToTagTable, bl::FontTagData::kFeatureIdCount);
  if (!featureTagsFinder.findSolution()) {
    printf("Solution not found!\n");
    return 1;
  }

  printf("-- Finding baseline tags to ids hash function --\n");
  StupidHash::Finder baselineTagsFinder(bl::FontTagData::baselineIdToTagTable, bl::FontTagData::kBaselineIdCount);
  if (!baselineTagsFinder.findSolution()) {
    printf("Solution not found!\n");
    return 1;
  }

  printf("-- Finding variation tags to ids hash function --\n");
  StupidHash::Finder variationTagsFinder(bl::FontTagData::variationIdToTagTable, bl::FontTagData::kVariationIdCount);
  if (!variationTagsFinder.findSolution()) {
    printf("Solution not found!\n");
    return 1;
  }

  printf("%s\n", tableTagsFinder._hf.body("static BL_INLINE uint32_t tableTagToId(uint32_t tag) noexcept", "tag", "tableIdToTagTable[", "]").data());
  printf("%s\n", scriptTagsFinder._hf.body("static BL_INLINE uint32_t scriptTagToId(uint32_t tag) noexcept", "tag", "scriptIdToTagTable[", "]").data());
  printf("%s\n", languageTagsFinder._hf.body("static BL_INLINE uint32_t languageTagToId(uint32_t tag) noexcept", "tag", "languageIdToTagTable[", "]").data());
  printf("%s\n", featureTagsFinder._hf.body("static BL_INLINE uint32_t featureTagToId(uint32_t tag) noexcept", "tag", "featureIdToTagTable[", "]").data());
  printf("%s\n", baselineTagsFinder._hf.body("static BL_INLINE uint32_t baselineTagToId(uint32_t tag) noexcept", "tag", "baselineIdToTagTable[", "]").data());
  printf("%s\n", variationTagsFinder._hf.body("static BL_INLINE uint32_t variationTagToId(uint32_t tag) noexcept", "tag", "variationIdToTagTable[", "]").data());

  return 0;
}