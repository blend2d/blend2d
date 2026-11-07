// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_p.h>

#include <blend2d/axl/commons.h>
#include <blend2d/core/array_p.h>
#include <blend2d/core/file_system.h>
#include <blend2d/core/font_data_p.h>
#include <blend2d/core/font_tag_data_p.h>
#include <blend2d/core/object_layer_p.h>
#include <blend2d/core/runtime_p.h>
#include <blend2d/opentype/ot_core_p.h>

namespace bl {

// FontData - Null
// ===============

static constinit ObjectLayer::EternalVirtImpl<FontDataPrivateImpl, BLFontDataVirt> null_font_data{};

static BLResult BL_CDECL null_font_data_destroy(BLObjectCore* self) noexcept {
  axl::maybe_unused(self);
  return BL_SUCCESS;
}

static BLResult BL_CDECL null_font_data_get_table_tags(const BLFontDataCore* self, uint32_t face_index, BLArrayCore* out) noexcept {
  axl::maybe_unused(self, face_index);
  return bl_array_clear(out);
}

static size_t BL_CDECL null_font_data_get_tables(const BLFontDataCore* self, uint32_t face_index, BLFontTable* dst, const BLTag* tags, size_t n) noexcept {
  axl::maybe_unused(self, face_index, tags);
  for (size_t i = 0; i < n; i++) {
    dst[i].reset();
  }
  return 0;
}

// FontData - Memory
// =================

struct MemFontDataImpl : public FontDataPrivateImpl {
  //! Array that holds font data. The data may be externally allocated.
  BLArray<uint8_t> data_array;
  //! Offset to an array that contains offsets for each font face.
  uint32_t offset_array_index;
};

static BLFontDataVirt mem_font_data_virt;

// Destroys `MemFontDataImpl` - this is a real destructor.
static BLResult mem_font_data_real_destroy(BLFontDataCore* self) noexcept {
  ObjectLayer::MetaByte meta_byte{self->_d.meta_byte()};
  MemFontDataImpl* impl = self->_impl<MemFontDataImpl>();

  if (meta_byte.is_external()) {
    ObjectLayer::call_external_destroy_func(self, impl, self->_d.dyn.data_or_virt.ptr);
  }

  bl_call_dtor(impl->data_array);
  bl_call_dtor(impl->face_cache);

  return ObjectLayer::dealloc_impl(impl, meta_byte);
}

static BLResult BL_CDECL mem_font_data_destroy(BLObjectCore* self) noexcept {
  return mem_font_data_real_destroy(static_cast<BLFontDataCore*>(self));
}

static BLResult BL_CDECL mem_font_data_get_table_tags(const BLFontDataCore* self, uint32_t face_index, BLArrayCore* out) noexcept {
  const MemFontDataImpl* impl = self->_impl<MemFontDataImpl>();

  const uint8_t* font_data = impl->data_array.data();
  size_t data_size = impl->data_array.size();

  if (BL_UNLIKELY(face_index >= impl->face_count)) {
    bl_array_clear(out);
    return bl_make_error(BL_ERROR_INVALID_VALUE);
  }

  uint32_t header_offset = 0;
  if (impl->offset_array_index) {
    header_offset = axl::offset_ptr<OpenType::UInt32>(font_data, impl->offset_array_index)[face_index].value();
  }

  if (BL_LIKELY(header_offset <= data_size - sizeof(OpenType::SFNTHeader))) {
    const OpenType::SFNTHeader* sfnt = axl::offset_ptr<OpenType::SFNTHeader>(font_data, header_offset);
    if (FontTagData::is_open_type_version_tag(sfnt->version_tag())) {
      // We can safely multiply `table_count` as SFNTHeader::num_tables is `UInt16`.
      uint32_t table_count = sfnt->num_tables();
      uint32_t min_data_size = uint32_t(sizeof(OpenType::SFNTHeader)) + table_count * uint32_t(sizeof(OpenType::SFNTHeader::TableRecord));

      if (BL_LIKELY(data_size - header_offset >= min_data_size)) {
        uint32_t* dst;
        BL_PROPAGATE(bl_array_assign_uninitialized(out, table_count, (void**)&dst));

        const OpenType::SFNTHeader::TableRecord* tables = sfnt->table_records();
        for (uint32_t table_index = 0; table_index < table_count; table_index++) {
          dst[table_index] = tables[table_index].tag();
        }
        return BL_SUCCESS;
      }
    }
  }

  bl_array_clear(out);
  return bl_make_error(BL_ERROR_INVALID_DATA);
}

static size_t BL_CDECL mem_font_data_get_tables(const BLFontDataCore* self, uint32_t face_index, BLFontTable* dst, const BLTag* tags, size_t n) noexcept {
  const MemFontDataImpl* impl = self->_impl<MemFontDataImpl>();
  const uint8_t* font_data = impl->data_array.data();
  size_t data_size = impl->data_array.size();

  memset(dst, 0, n * sizeof(BLFontTable));

  if (BL_LIKELY(face_index < impl->face_count)) {
    size_t header_offset = 0;
    if (impl->offset_array_index) {
      header_offset = axl::offset_ptr<OpenType::UInt32>(font_data, impl->offset_array_index)[face_index].value();
    }

    if (BL_LIKELY(header_offset <= data_size - sizeof(OpenType::SFNTHeader))) {
      const OpenType::SFNTHeader* sfnt = axl::offset_ptr<OpenType::SFNTHeader>(font_data, header_offset);
      if (FontTagData::is_open_type_version_tag(sfnt->version_tag())) {
        size_t table_count = sfnt->num_tables();

        // We can safely multiply `table_count` as SFNTHeader::num_tables is `UInt16`.
        size_t min_data_size = sizeof(OpenType::SFNTHeader) + table_count * sizeof(OpenType::SFNTHeader::TableRecord);
        if (BL_LIKELY(data_size - header_offset >= min_data_size)) {
          // Iterate over all tables and try to find all tables as specified by `tags`.
          const OpenType::SFNTHeader::TableRecord* tables = sfnt->table_records();
          size_t match_count = 0;

          // If all tags are known (convertible to table_id) we can just build a small index and then try
          // to match all tags by doing a linear scan. If there is one or more unknown tags, we would go
          // with linear scan.
          //
          // In general, we do this, because Blend2D's OpenType engine requests all tables in one go, and
          // then inspects them on load - this means that this table lookup is in general optimized and
          // only does a single iteration of 'sfnt' tables.
          if (n >= 3u && n < 255u) {
            constexpr uint32_t kTableIdCountAligned = axl::align_up(FontTagData::kTableIdCount, 16);

            uint8_t table_id_to_index[kTableIdCountAligned];
            memset(table_id_to_index, 0xFF, kTableIdCountAligned);

            size_t i = 0;
            while (i < n) {
              uint32_t table_id = FontTagData::table_tag_to_id(tags[i]);
              if (table_id == FontTagData::kInvalidId)
                break;
              table_id_to_index[table_id] = uint8_t(i);
              i++;
            }

            if (i == n) {
              // All requested tags known, table id to dst index lookup built successfully.
              for (uint32_t table_index = 0; table_index < table_count; table_index++) {
                const OpenType::SFNTHeader::TableRecord& table = tables[table_index];

                BLTag table_tag = table.tag();
                uint32_t table_id = FontTagData::table_tag_to_id(table_tag);

                // Since we know that all requested tags have their unique IDs, we can refuse any tag that doesn't.
                if (table_id == FontTagData::kInvalidId)
                  continue;

                i = table_id_to_index[table_id];
                if (i != 0xFF) {
                  uint32_t table_offset = table.offset();
                  uint32_t table_size = table.length();

                  if (table_offset < data_size && table_size && table_size <= data_size - table_offset) {
                    dst[i].reset(axl::offset_ptr<uint8_t>(font_data, table_offset), table_size);
                    match_count++;
                  }
                }
              }
            }

            return match_count;
          }

          // Linear search.
          for (size_t tag_index = 0; tag_index < n; tag_index++) {
            uint32_t tag = axl::byteswap_be(tags[tag_index]);
            for (uint32_t table_index = 0; table_index < table_count; table_index++) {
              const OpenType::SFNTHeader::TableRecord& table = tables[table_index];

              if (table.tag.raw_value() == tag) {
                uint32_t table_offset = table.offset();
                uint32_t table_size = table.length();

                if (table_offset < data_size && table_size && table_size <= data_size - table_offset) {
                  dst[tag_index].reset(axl::offset_ptr<uint8_t>(font_data, table_offset), table_size);
                  match_count++;
                }

                break;
              }
            }
          }

          return match_count;
        }
      }
    }
  }

  return 0;
}

} // {bl}

// FontData - API - Init & Destroy
// ===============================

BLResult bl_font_data_init(BLFontDataCore* self) noexcept {
  self->_d = bl_object_defaults[BL_OBJECT_TYPE_FONT_DATA]._d;
  return BL_SUCCESS;
}

BLResult bl_font_data_init_move(BLFontDataCore* self, BLFontDataCore* other) noexcept {
  using namespace bl;

  BL_ASSERT(self != other);
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::move_object(self, other);
}

BLResult bl_font_data_init_weak(BLFontDataCore* self, const BLFontDataCore* other) noexcept {
  using namespace bl;

  BL_ASSERT(self != other);
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::copy_object(self, other);
}

BLResult bl_font_data_destroy(BLFontDataCore* self) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::release_object(self);
}

// FontData - API - Reset
// ======================

BLResult bl_font_data_reset(BLFontDataCore* self) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::reset_object(self);
}

// FontData - API - Assign
// =======================

BLResult bl_font_data_assign_move(BLFontDataCore* self, BLFontDataCore* other) noexcept {
  using namespace bl;

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::assign_object_move(self, other);
}

BLResult bl_font_data_assign_weak(BLFontDataCore* self, const BLFontDataCore* other) noexcept {
  using namespace bl;

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::assign_object_weak(self, other);
}

// FontData - API - Equality & Comparison
// ======================================

bool bl_font_data_equals(const BLFontDataCore* a, const BLFontDataCore* b) noexcept {
  using namespace bl;

  BL_ASSERT(ObjectLayer::is_object_valid(a));
  BL_ASSERT(ObjectLayer::is_object_valid(b));

  return a->_d.dyn.impl.ptr == b->_d.dyn.impl.ptr;
}

// FontData - API - Create
// =======================

#if !defined(BL_BUILD_NO_FILESYSTEM)
BLResult bl_font_data_create_from_file(BLFontDataCore* self, const char* file_name, BLFileReadFlags read_flags) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLArray<uint8_t> buffer;
  BL_PROPAGATE(BLFileSystem::read_file(file_name, buffer, 0, read_flags));

  if (buffer.is_empty())
    return bl_make_error(BL_ERROR_FILE_EMPTY);

  return bl_font_data_create_from_data_array(self, &buffer);
}
#endif // !BL_BUILD_NO_FILESYSTEM

static BLResult bl_font_data_create_from_data_internal(BLFontDataCore* self, const uint8_t* data, size_t data_size, const BLArray<uint8_t>* underlying_array, BLDestroyExternalDataFunc destroy_func, void* user_data) noexcept {
  using namespace bl;

  constexpr uint32_t header_base_size = bl_min<uint32_t>(OpenType::SFNTHeader::kBaseSize, OpenType::TTCFHeader::kBaseSize);
  if (BL_UNLIKELY(data_size < header_base_size)) {
    return bl_make_error(BL_ERROR_INVALID_DATA);
  }

  if (BL_UNLIKELY(sizeof(size_t) > 4 && data_size > 0xFFFFFFFFu)) {
    return bl_make_error(BL_ERROR_DATA_TOO_LARGE);
  }

  uint32_t header_tag = axl::offset_ptr<const OpenType::UInt32>(data, 0)->value();
  uint32_t face_count = 1;
  uint32_t data_flags = 0;

  uint32_t offset_array_index = 0;
  const OpenType::UInt32* offset_array = nullptr;

  if (FontTagData::is_open_type_collection_tag(header_tag)) {
    if (BL_UNLIKELY(data_size < OpenType::TTCFHeader::kBaseSize)) {
      return bl_make_error(BL_ERROR_INVALID_DATA);
    }

    const OpenType::TTCFHeader* header = axl::offset_ptr<const OpenType::TTCFHeader>(data, 0);

    face_count = header->fonts.count();
    if (BL_UNLIKELY(!face_count || face_count > BL_FONT_DATA_MAX_FACE_COUNT)) {
      return bl_make_error(BL_ERROR_INVALID_DATA);
    }

    size_t ttc_header_size = header->calc_size(face_count);
    if (BL_UNLIKELY(ttc_header_size > data_size)) {
      return bl_make_error(BL_ERROR_INVALID_DATA);
    }

    offset_array = header->fonts.array();
    offset_array_index = (uint32_t)((uintptr_t)offset_array - (uintptr_t)header);

    data_flags |= BL_FONT_DATA_FLAG_COLLECTION;
  }
  else {
    if (!FontTagData::is_open_type_version_tag(header_tag)) {
      return bl_make_error(BL_ERROR_INVALID_SIGNATURE);
    }
  }

  BLArray<BLFontFaceImpl*> face_cache;
  BL_PROPAGATE(face_cache.resize(face_count, nullptr));

  BLFontDataCore new_o;
  BL_PROPAGATE(ObjectLayer::alloc_virt_impl_t<MemFontDataImpl>(&new_o, &mem_font_data_virt));
  MemFontDataImpl* new_impl = new_o._impl<MemFontDataImpl>();
  font_data_init_impl(new_impl);

  new_impl->face_type = uint8_t(BL_FONT_FACE_TYPE_OPENTYPE);
  new_impl->face_count = face_count;
  new_impl->flags = data_flags;
  bl_call_ctor(new_impl->face_cache, bl::Internal::move(face_cache));
  new_impl->offset_array_index = offset_array_index;

  if (underlying_array) {
    bl_call_ctor(new_impl->data_array, *underlying_array);
  }
  else {
    bl_call_ctor(new_impl->data_array);
    BL_PROPAGATE(new_impl->data_array.assign_external_data_const(data, data_size, data_size, destroy_func, user_data));
  }

  return ObjectLayer::replace_object(self, &new_o);
}

BLResult bl_font_data_create_from_data_array(BLFontDataCore* self, const BLArrayCore* data_array) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (!data_array->_d.is_array_u8()) {
    return bl_make_error(BL_ERROR_INVALID_VALUE);
  }

  const BLArray<uint8_t>& array_u8 = data_array->dcast<BLArray<uint8_t>>();
  return bl_font_data_create_from_data_internal(self, array_u8.data(), array_u8.size(), &array_u8, nullptr, nullptr);
}

BLResult bl_font_data_create_from_data(BLFontDataCore* self, const void* data, size_t data_size, BLDestroyExternalDataFunc destroy_func, void* user_data) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return bl_font_data_create_from_data_internal(self, static_cast<const uint8_t*>(data), data_size, nullptr, destroy_func, user_data);
};

// FontData - API - Accessors
// ==========================

uint32_t bl_font_data_get_face_count(const BLFontDataCore* self) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  const FontDataPrivateImpl* self_impl = ObjectLayer::get_impl(self);
  return self_impl->face_count;
}

BLFontFaceType bl_font_data_get_face_type(const BLFontDataCore* self) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  const FontDataPrivateImpl* self_impl = ObjectLayer::get_impl(self);
  return BLFontFaceType(self_impl->face_type);
}

BLFontDataFlags bl_font_data_get_flags(const BLFontDataCore* self) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  const FontDataPrivateImpl* self_impl = ObjectLayer::get_impl(self);
  return BLFontDataFlags(self_impl->flags);
}

BLResult bl_font_data_get_table_tags(const BLFontDataCore* self, uint32_t face_index, BLArrayCore* dst) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));
  return self->_virt()->get_table_tags(self, face_index, dst);
}

size_t bl_font_data_get_tables(const BLFontDataCore* self, uint32_t face_index, BLFontTable* dst, const BLTag* tags, size_t count) noexcept {
  using namespace bl;
  BL_ASSERT(ObjectLayer::is_object_valid(self));
  return self->_virt()->get_tables(self, face_index, dst, tags, count);
}

// FontData - Runtime Registration
// ===============================

void bl_font_data_rt_init([[maybe_unused]] BLRuntimeContext* rt) noexcept {
  using namespace bl;
  static constexpr BLObjectType object_type = BLFontData::Info::kType;

  null_font_data.virt.base.destroy = null_font_data_destroy;
  null_font_data.virt.base.get_property = bl_object_impl_get_property;
  null_font_data.virt.base.set_property = bl_object_impl_set_property;
  null_font_data.virt.get_table_tags = null_font_data_get_table_tags;
  null_font_data.virt.get_tables = null_font_data_get_tables;
  font_data_init_impl(&null_font_data.impl);

  mem_font_data_virt.base.destroy = mem_font_data_destroy;
  mem_font_data_virt.base.get_property = bl_object_impl_get_property;
  mem_font_data_virt.base.set_property = bl_object_impl_set_property;
  mem_font_data_virt.get_table_tags = mem_font_data_get_table_tags;
  mem_font_data_virt.get_tables = mem_font_data_get_tables;

  ObjectLayer::init_virt(&bl_object_defaults[object_type], object_type, 0u, &null_font_data.impl, &null_font_data.virt);
}
