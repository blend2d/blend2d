// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_p.h>

#include <blend2d/axl/string_utils_p.h>
#include <blend2d/core/file_system.h>
#include <blend2d/core/font_p.h>
#include <blend2d/core/font_face_p.h>
#include <blend2d/core/font_manager_p.h>
#include <blend2d/core/object_layer_p.h>
#include <blend2d/core/runtime_p.h>
#include <blend2d/core/string_p.h>

namespace bl::FontManagerDetail {

// FontManager - Detail - Globals
// ==============================

static const char no_name[1] = "";

static constinit ObjectLayer::EternalVirtImpl<FontManagerPrivateImpl, BLFontManagerVirt> default_font_manager{};

// FontManager - Detail - Constants
// ================================

enum QueryPrecedenceBits : uint32_t {
  kQueryDiffFamilyNameShift   = 24, // 0xFF000000 [ 8 bits].
  kQueryDiffStyleValueShift   = 22, // 0x00C00000 [ 2 bits].
  kQueryDiffStyleSignShift    = 21, // 0x00200000 [ 1 bit ].
  kQueryDiffWeightValueShift  = 10, // 0x001FFC00 [11 bits].
  kQueryDiffWeightSignShift   =  9, // 0x00000200 [ 1 bit ].
  kQueryDiffStretchValueShift =  5, // 0x000001E0 [ 4 bits].
  kQueryDiffStretchSignShift  =  4  // 0x00000010 [ 1 bit ].
};

static constexpr uint32_t kQueryInvalidDiff = 0xFFFFFFFFu;

// FontManager - Detail - Alloc & Free Impl
// ========================================

static BLResult font_manager_alloc_impl(BLFontManagerCore* self) noexcept {
  BL_PROPAGATE(ObjectLayer::alloc_virt_impl_t<FontManagerPrivateImpl>(self, &default_font_manager.virt));
  bl_call_ctor(*ObjectLayer::get_impl(self));
  return BL_SUCCESS;
}

static BLResult BL_CDECL font_manager_destroy_impl(BLObjectCore* self) noexcept {
  FontManagerPrivateImpl* impl = self->_d.impl<FontManagerPrivateImpl>();
  bl_call_dtor(*impl);
  return ObjectLayer::dealloc_impl(impl);
}

// FontManager - Detail - Faces
// ============================

static BL_INLINE size_t font_manager_index_of_face(const BLFontFace* array, size_t size, BLFontFaceImpl* face_impl) noexcept {
  for (size_t i = 0; i < size; i++) {
    if (array[i]._d.dyn.impl.ptr == face_impl) {
      return i;
    }
  }
  return SIZE_MAX;
}

static BL_INLINE uint32_t font_manager_calc_face_order(const BLFontFaceImpl* face_impl) noexcept {
  uint32_t style = face_impl->style;
  uint32_t weight = face_impl->weight;

  return (style << kQueryDiffStyleValueShift) |
         (weight << kQueryDiffWeightValueShift);
}

static BL_INLINE size_t font_manager_index_for_insertion(const BLFontFace* array, size_t size, BLFontFaceImpl* face_impl) noexcept {
  uint32_t face_order = font_manager_calc_face_order(face_impl);
  size_t i;

  for (i = 0; i < size; i++) {
    FontFacePrivateImpl* stored_face_impl = ObjectLayer::get_impl(&array[i]);
    uint32_t stored_face_order = font_manager_calc_face_order(stored_face_impl);
    if (stored_face_order >= face_order) {
      if (stored_face_order == face_order)
        return SIZE_MAX;
      break;
    }
  }

  return i;
}

// FontManager - Query - Utilities
// ===============================

static const BLFontQueryProperties font_default_query_properties = {
  BL_FONT_STYLE_NORMAL,
  BL_FONT_WEIGHT_NORMAL,
  BL_FONT_STRETCH_NORMAL
};

static bool font_sanitize_query_properties(BLFontQueryProperties& dst, const BLFontQueryProperties& src) noexcept {
  bool valid = src.weight <= 1000u &&
               src.style <= BL_FONT_STYLE_MAX_VALUE &&
               src.stretch <= BL_FONT_STRETCH_ULTRA_EXPANDED;

  if (valid) {
    dst.style = src.style;
    dst.weight = src.weight ? src.weight : BL_FONT_WEIGHT_NORMAL;
    dst.stretch = src.stretch ? src.stretch : BL_FONT_STRETCH_NORMAL;
  }

  return valid;
}

// FontManager - Query - Prepared Query
// ====================================

struct FontPreparedQuery {
  BLStringView _name;
  uint32_t _hash_code;

  typedef FontManagerPrivateImpl::FamiliesMapNode FamiliesMapNode;

  BL_INLINE BLStringView name() const noexcept { return _name; };
  BL_INLINE uint32_t hash_code() const noexcept { return _hash_code; }

  BL_INLINE bool matches(const FamiliesMapNode* node) const noexcept { return node->family_name.equals(_name); }
};

static bool font_prepare_query(const FontManagerPrivateImpl* impl, const char* name, size_t name_size, FontPreparedQuery* out) noexcept {
  axl::maybe_unused(impl);
  out->_name.reset(name, name_size);
  out->_hash_code = axl::hash_string_ci(name, name_size);
  return name_size != 0;
}

// FontManager - Query - Diff Calculation
// ======================================

static BL_INLINE uint32_t font_calc_family_name_diff(BLStringView a_str, BLStringView b_str) noexcept {
  if (a_str.size() != b_str.size())
    return kQueryInvalidDiff;

  uint32_t diff = 0;

  for (size_t i = 0; i < a_str.size(); i++) {
    uint32_t a = uint8_t(a_str[i]);
    uint32_t b = uint8_t(b_str[i]);

    if (a == b) {
      continue;
    }

    a = axl::ascii_to_lower(a);
    b = axl::ascii_to_lower(b);

    if (a != b) {
      return kQueryInvalidDiff;
    }

    diff++;
  }

  if (diff > 255) {
    diff = 255;
  }

  return diff << kQueryDiffFamilyNameShift;
}

static BL_INLINE uint32_t font_calc_property_diff(const BLFontFaceImpl* face_impl, const BLFontQueryProperties* properties) noexcept {
  uint32_t diff = 0;

  uint32_t fStyle = face_impl->style;
  uint32_t fWeight = face_impl->weight;
  uint32_t fStretch = face_impl->stretch;

  uint32_t pStyle = properties->style;
  uint32_t pWeight = properties->weight;
  uint32_t pStretch = properties->stretch;

  uint32_t style_diff = uint32_t(bl_abs(int(pStyle) - int(fStyle)));
  uint32_t weight_diff = uint32_t(bl_abs(int(pWeight) - int(fWeight)));
  uint32_t stretch_diff = uint32_t(bl_abs(int(pStretch) - int(fStretch)));

  diff |= style_diff << kQueryDiffStyleValueShift;
  diff |= uint32_t(pStyle < fStyle) << kQueryDiffStyleSignShift;

  diff |= weight_diff << kQueryDiffWeightValueShift;
  diff |= uint32_t(pWeight < fWeight) << kQueryDiffWeightSignShift;

  diff |= stretch_diff << kQueryDiffStretchValueShift;
  diff |= uint32_t(pStretch < fStretch) << kQueryDiffStretchSignShift;

  return diff;
}

// FontManager - Query - Match
// ===========================

class FontQueryBestMatch {
public:
  const BLFontQueryProperties* properties;
  const BLFontFace* face;
  uint32_t diff;

  BL_INLINE FontQueryBestMatch(const BLFontQueryProperties* properties) noexcept
    : properties(properties),
      face(nullptr),
      diff(0xFFFFFFFFu) {}

  BL_INLINE bool has_face() const noexcept { return face != nullptr; }

  void match(const BLFontFace& face_in, uint32_t base_diff = 0) noexcept {
    uint32_t local_diff = base_diff + font_calc_property_diff(face_in._impl(), properties);
    if (diff > local_diff) {
      face = &face_in;
      diff = local_diff;
    }
  }
};

} // {bl::FontManagerDetail}

// FontManager - API - Init & Destroy
// ==================================

BL_API_IMPL BLResult bl_font_manager_init(BLFontManagerCore* self) noexcept {
  self->_d = bl_object_defaults[BL_OBJECT_TYPE_FONT_MANAGER]._d;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_font_manager_init_move(BLFontManagerCore* self, BLFontManagerCore* other) noexcept {
  BL_USING_DETAIL(bl::FontManagerDetail);

  BL_ASSERT(self != other);
  BL_ASSERT(other->_d.is_font_manager());

  return ObjectLayer::move_object(self, other);
}

BL_API_IMPL BLResult bl_font_manager_init_weak(BLFontManagerCore* self, const BLFontManagerCore* other) noexcept {
  BL_USING_DETAIL(bl::FontManagerDetail);

  BL_ASSERT(self != other);
  BL_ASSERT(other->_d.is_font_manager());

  return ObjectLayer::copy_object(self, other);
}

BL_API_IMPL BLResult bl_font_manager_init_new(BLFontManagerCore* self) noexcept {
  BL_USING_DETAIL(bl::FontManagerDetail);

  self->_d = bl_object_defaults[BL_OBJECT_TYPE_FONT_MANAGER]._d;
  return font_manager_alloc_impl(self);
}

BL_API_IMPL BLResult bl_font_manager_destroy(BLFontManagerCore* self) noexcept {
  BL_USING_DETAIL(bl::FontManagerDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::release_object(self);
}

// FontManager - API - Reset
// =========================

BL_API_IMPL BLResult bl_font_manager_reset(BLFontManagerCore* self) noexcept {
  BL_USING_DETAIL(bl::FontManagerDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::reset_object(self);
}

// FontManager - API - Assign
// ==========================

BL_API_IMPL BLResult bl_font_manager_assign_move(BLFontManagerCore* self, BLFontManagerCore* other) noexcept {
  BL_USING_DETAIL(bl::FontManagerDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(other->_d.is_font_manager());

  return ObjectLayer::assign_object_move(self, other);
}

BL_API_IMPL BLResult bl_font_manager_assign_weak(BLFontManagerCore* self, const BLFontManagerCore* other) noexcept {
  BL_USING_DETAIL(bl::FontManagerDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(other->_d.is_font_manager());

  return ObjectLayer::assign_object_weak(self, other);
}

// FontManager - API - Equals
// ==========================

bool bl_font_manager_equals(const BLFontManagerCore* a, const BLFontManagerCore* b) noexcept {
  BL_USING_DETAIL(bl::FontManagerDetail);

  BL_ASSERT(a->_d.is_font_manager());
  BL_ASSERT(b->_d.is_font_manager());

  return a->_d.dyn.impl.ptr == b->_d.dyn.impl.ptr;
}

// FontManager - API - Create
// ==========================

BL_API_IMPL BLResult bl_font_manager_create(BLFontManagerCore* self) noexcept {
  BL_USING_DETAIL(bl::FontManagerDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLFontManagerCore new_o;
  BL_PROPAGATE(font_manager_alloc_impl(&new_o));

  return bl::ObjectLayer::replace_object(self, &new_o);
}

// FontManager - API - Accessors
// =============================

BL_API_IMPL size_t bl_font_manager_get_face_count(const BLFontManagerCore* self) noexcept {
  BL_USING_DETAIL(bl::FontManagerDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  FontManagerPrivateImpl* self_impl = ObjectLayer::get_impl(self);
  SharedLockGuard<SharedMutex> guard(self_impl->mutex);

  return self_impl->face_count;
}

BL_API_IMPL size_t bl_font_manager_get_family_count(const BLFontManagerCore* self) noexcept {
  BL_USING_DETAIL(bl::FontManagerDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  FontManagerPrivateImpl* self_impl = ObjectLayer::get_impl(self);
  SharedLockGuard<SharedMutex> guard(self_impl->mutex);

  return self_impl->families_map.size();
}

// FontManager - Internal Utilities
// ================================

static BL_INLINE BLResult bl_font_manager_make_mutable(BLFontManagerCore* self) noexcept {
  BL_USING_DETAIL(bl::FontManagerDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (!self->dcast().is_valid()) {
    return bl_font_manager_create(self);
  }

  return BL_SUCCESS;
}

// FontManager - API - Font Face Management
// ========================================

BL_API_IMPL bool bl_font_manager_has_face(const BLFontManagerCore* self, const BLFontFaceCore* face) noexcept {
  BL_USING_DETAIL(bl::FontManagerDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(self->_d.is_font_face());

  FontManagerPrivateImpl* self_impl = ObjectLayer::get_impl(self);
  FontFacePrivateImpl* face_impl = ObjectLayer::get_impl(face);

  uint32_t name_hash = bl::axl::hash_string_ci(face_impl->family_name.dcast().span());

  SharedLockGuard<SharedMutex> guard(self_impl->mutex);
  FontManagerPrivateImpl::FamiliesMapNode* families_node =
    self_impl->families_map.get(FontManagerPrivateImpl::FamilyMatcher{face_impl->family_name.dcast().span(), name_hash});

  if (!families_node)
    return false;

  size_t index = font_manager_index_of_face(families_node->faces.data(), families_node->faces.size(), face_impl);
  return index != SIZE_MAX;
}

BL_API_IMPL BLResult bl_font_manager_add_face(BLFontManagerCore* self, const BLFontFaceCore* face) noexcept {
  BL_USING_DETAIL(bl::FontManagerDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(self->_d.is_font_face());

  if (!face->dcast().is_valid())
    return bl_make_error(BL_ERROR_FONT_NOT_INITIALIZED);

  BL_PROPAGATE(bl_font_manager_make_mutable(self));

  FontManagerPrivateImpl* self_impl = ObjectLayer::get_impl(self);
  FontFacePrivateImpl* face_impl = ObjectLayer::get_impl(face);

  uint32_t name_hash = axl::hash_string_ci(face_impl->family_name.dcast().span());

  LockGuard<SharedMutex> guard(self_impl->mutex);
  ArenaAllocator::StatePtr allocator_state = self_impl->allocator.save_state();

  FontManagerPrivateImpl::FamiliesMapNode* families_node =
    self_impl->families_map.get(FontManagerPrivateImpl::FamilyMatcher{face_impl->family_name.dcast().span(), name_hash});

  if (!families_node) {
    families_node = self_impl->allocator.new_t<FontManagerPrivateImpl::FamiliesMapNode>(name_hash, face_impl->family_name.dcast());
    if (!families_node)
      return bl_make_error(BL_ERROR_OUT_OF_MEMORY);

    // Reserve for only one item at the beginning. This helps to decrease memory footprint when loading a lot of font
    // faces that don't share family names.
    BLResult result = families_node->faces.reserve(1u);
    if (BL_UNLIKELY(result != BL_SUCCESS)) {
      bl_call_dtor(*families_node);
      self_impl->allocator.restore_state(allocator_state);
      return result;
    }

    families_node->faces.append(face->dcast());
    self_impl->families_map.insert(families_node);
  }
  else {
    size_t index = font_manager_index_for_insertion(families_node->faces.data(), families_node->faces.size(), face_impl);
    if (index == SIZE_MAX)
      return BL_SUCCESS;
    BL_PROPAGATE(families_node->faces.insert(index, face->dcast()));
  }

  self_impl->face_count++;
  return BL_SUCCESS;
}

// FontManager - Query - API
// =========================

BL_API_IMPL BLResult bl_font_manager_query_faces_by_family_name_cstr(const BLFontManagerCore* self, const char* name, BLArrayCore* out) noexcept {
  BL_USING_DETAIL(bl::FontManagerDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (name == nullptr) {
    name = no_name;
  }

  size_t size = strlen(name);
  return bl_font_manager_query_faces_by_family_name_view(self, name, size, out);
}

BL_API_IMPL BLResult bl_font_manager_query_faces_by_family_name_view(const BLFontManagerCore* self, const char* name, size_t size, BLArrayCore* out) noexcept {
  BL_USING_DETAIL(bl::FontManagerDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (BL_UNLIKELY(!out->_d.is_array_obj())) {
    return bl_make_error(BL_ERROR_INVALID_VALUE);
  }
  else {
    FontPreparedQuery query;
    uint32_t candidate_diff = 0xFFFFFFFF;
    FontManagerPrivateImpl::FamiliesMapNode* candidate = nullptr;

    FontManagerPrivateImpl* self_impl = ObjectLayer::get_impl(self);
    SharedLockGuard<SharedMutex> guard(self_impl->mutex);

    if (font_prepare_query(self_impl, name, size, &query)) {
      FontManagerPrivateImpl::FamiliesMapNode* node = self_impl->families_map.get(query);
      while (node) {
        uint32_t family_diff = font_calc_family_name_diff(node->family_name.span(), query.name());
        if (candidate_diff > family_diff) {
          candidate_diff = family_diff;
          candidate = node;
        }
        node = node->next();
      }
    }

    if (candidate) {
      return out->dcast<BLArray<BLFontFace>>().assign(candidate->faces);
    }
  }

  // This is not considered to be an error, thus don't use bl_make_error().
  out->dcast<BLArray<BLFontFace>>().clear();
  return BL_ERROR_FONT_NO_MATCH;
}

BL_API_IMPL BLResult bl_font_manager_query_face_cstr(const BLFontManagerCore* self, const char* name, const BLFontQueryProperties* properties, BLFontFaceCore* out) noexcept {
  BL_USING_DETAIL(bl::FontManagerDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (name == nullptr) {
    name = no_name;
  }

  size_t size = strlen(name);
  return bl_font_manager_query_face_view(self, name, size, properties, out);
}

BL_API_IMPL BLResult bl_font_manager_query_face_view(const BLFontManagerCore* self, const char* name, size_t size, const BLFontQueryProperties* properties, BLFontFaceCore* out) noexcept {
  BL_USING_DETAIL(bl::FontManagerDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (!properties) {
    properties = &font_default_query_properties;
  }

  BLFontQueryProperties sanitized_properties;
  if (!font_sanitize_query_properties(sanitized_properties, *properties)) {
    return bl_make_error(BL_ERROR_INVALID_VALUE);
  }
  else {
    FontPreparedQuery query;
    FontQueryBestMatch best_match(&sanitized_properties);

    FontManagerPrivateImpl* self_impl = ObjectLayer::get_impl(self);
    SharedLockGuard<SharedMutex> guard(self_impl->mutex);

    if (font_prepare_query(self_impl, name, size, &query)) {
      FontManagerPrivateImpl::FamiliesMapNode* node = self_impl->families_map.nodes_by_hash_code(query.hash_code());
      while (node) {
        uint32_t family_diff = font_calc_family_name_diff(node->family_name.span(), query.name());
        if (family_diff != kQueryInvalidDiff) {
          for (const BLFontFace& face : node->faces.dcast<BLArray<BLFontFace>>()) {
            best_match.match(face, family_diff);
          }
        }
        node = node->next();
      }
    }

    if (best_match.has_face()) {
      return out->dcast().assign(*best_match.face);
    }
  }

  // This is not considered to be an error, thus don't use bl_make_error().
  out->dcast().reset();
  return BL_ERROR_FONT_NO_MATCH;
}

// FontManager - Runtime Registration
// ==================================

void bl_font_manager_rt_init([[maybe_unused]] BLRuntimeContext* rt) noexcept {
  BL_USING_DETAIL(bl::FontManagerDetail);
  static constexpr BLObjectType object_type = BLFontManager::Info::kType;

  auto& default_fm = default_font_manager;
  default_fm.virt.base.destroy = font_manager_destroy_impl;
  default_fm.virt.base.get_property = bl_object_impl_get_property;
  default_fm.virt.base.set_property = bl_object_impl_set_property;
  default_fm.impl.init();

  auto& default_obj = bl_object_defaults[object_type];
  ObjectLayer::init_virt(&default_obj, object_type, 0u, &default_fm.impl, &default_fm.virt);
  ObjectLayer::set_byte_field<BLFontManager::IsDefaultField>(&default_obj, 1u);
}
