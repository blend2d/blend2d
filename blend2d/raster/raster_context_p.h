// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_RASTER_RASTER_CONTEXT_P_H_INCLUDED
#define BLEND2D_RASTER_RASTER_CONTEXT_P_H_INCLUDED

#include <blend2d/axl/arena_allocator_p.h>
#include <blend2d/axl/arena_pool_p.h>
#include <blend2d/core/api_internal_p.h>
#include <blend2d/core/compopinfo_p.h>
#include <blend2d/core/context_p.h>
#include <blend2d/core/font_p.h>
#include <blend2d/core/gradient_p.h>
#include <blend2d/core/matrix_p.h>
#include <blend2d/core/path_p.h>
#include <blend2d/core/rgba.h>
#include <blend2d/pipeline/pipe_runtime_p.h>
#include <blend2d/raster/analytic_rasterizer_p.h>
#include <blend2d/raster/edge_builder_p.h>
#include <blend2d/raster/raster_defs_p.h>
#include <blend2d/raster/render_command_p.h>
#include <blend2d/raster/render_fetch_data_p.h>
#include <blend2d/raster/render_job_p.h>
#include <blend2d/raster/render_queue_p.h>
#include <blend2d/raster/render_target_info_p.h>
#include <blend2d/raster/state_data_p.h>
#include <blend2d/raster/style_data_p.h>
#include <blend2d/raster/work_data_p.h>
#include <blend2d/raster/worker_manager_p.h>
#include <blend2d/threading/unique_id_generator_p.h>

#if !defined(BL_BUILD_NO_JIT)
  #include <blend2d/pipeline/jit/jit_pipe_runtime_p.h>
#endif

namespace bl::RasterEngine {

//! \cond INTERNAL
//! \addtogroup blend2d_raster_engine_impl
//! \{

//! Preferred fill-rule (fastest) to use when the fill-rule doesn't matter.
//!
//! Since the filler doesn't care of fill-rule (it always uses the same code-path for non-zero and even-odd fills) it
//! doesn't really matter. However, if there is more rasterizers added in the future this can be adjusted to always
//! select the fastest one.
static constexpr const BLFillRule BL_RASTER_CONTEXT_PREFERRED_FILL_RULE = BL_FILL_RULE_EVEN_ODD;

//! Preferred extend mode (fastest) to use when blitting images. The extend mode can be either PAD or REFLECT as these
//! have the same effect on blits that are bound to the size of the image. We prefer REFLECT, because it's useful also
//! outside regular blits.
static constexpr const BLExtendMode BL_RASTER_CONTEXT_PREFERRED_BLIT_EXTEND = BL_EXTEND_MODE_REFLECT;

//! Minimum size of a path (in vertices) to make it an asynchronous job. The reason for this threshold is that very
//! small paths actually do not benefit from being dispatched into a worker thread (the cost of serializing the job
//! is higher than the cost of processing that path in a user thread).
static constexpr const uint32_t BL_RASTER_CONTEXT_MINIMUM_ASYNC_PATH_BYTE_SIZE = uint32_t(sizeof(BLPoint) * 16 + 16);

//! Maximum size of a text to be copied as is when dispatching asynchronous jobs. When the limit is reached the job
//! serialized would create a BLGlyphBuffer instead of making raw copy of the text, as the glyph-buffer has to copy
//! it anyway.
static constexpr const uint32_t BL_RASTER_CONTEXT_MAXIMUM_EMBEDDED_TEXT_SIZE = 256;

static constexpr const uint32_t BL_RASTER_CONTEXT_DEFAULT_SAVED_STATE_LIMIT = 4096;

static constexpr const uint32_t BL_RASTER_CONTEXT_DEFAULT_COMMAND_QUEUE_LIMIT = 10240;

//! Raster rendering context implementation (software accelerated).
class RasterContextImpl : public BLContextImpl {
public:
  BL_NONCOPYABLE(RasterContextImpl)

  //! \name Members
  //! \{

  //! Context flags.
  ContextFlags context_flags;
  //! Rendering mode.
  uint8_t rendering_mode;
  //! Whether worker_mgr has been initialized.
  uint8_t worker_mgr_initialized;
  //! Precision information.
  RenderTargetInfo render_target_info;

  //! Work data used by synchronous rendering that also holds part of the current state. In async mode the work data
  //! can still be used by user thread in case it's allowed, otherwise it would only hold some states that are used
  //! by the rendering context directly.
  WorkData sync_work_data;

  //! Pipeline lookup cache (always used before attempting to use `pipe_provider`).
  Pipeline::PipeLookupCache pipe_lookup_cache;

  //! Composition operator simplification that matches the destination format and current `comp_op`.
  const CompOpSimplifyInfo* comp_op_simplify_info;
  //! Solid format table used to select the best pixel format for solid fills.
  uint8_t solid_format_table[RASTER_CONTEXT_SOLID_FORMAT_COUNT];
  //! Table that can be used to override a fill/stroke color by one from SolidId (after a simplification).
  RenderFetchDataSolid* solid_override_fill_table;
  //! Solid fill override table indexed by \ref CompOpSolidId.
  RenderFetchDataHeader* solid_fetch_data_override_table[uint32_t(CompOpSolidId::kAlwaysNop) + 1u];

  //! The current state of the rendering context, the `BLContextState` part is public.
  RasterContextState internal_state;
  //! Link to the previous saved state that will be restored by `BLContext::restore()`.
  SavedState* saved_state;
  //! An actual shared fill-state (asynchronous rendering).
  SharedFillState* shared_fill_state;
  //! An actual shared stroke-state (asynchronous rendering).
  SharedBaseStrokeState* shared_stroke_state;

  //! Arena allocator used to allocate base data structures required by `RasterContextImpl`.
  ArenaAllocator base_zone;
  //! Object pool used to allocate `RenderFetchData`.
  ArenaPool<RenderFetchData> fetch_data_pool;
  //! Object pool used to allocate `SavedState`.
  ArenaPool<SavedState> saved_state_pool;

  //! Pipeline runtime (either global or isolated, depending on create-options).
  Pipeline::PipeProvider pipe_provider;
  //! Worker manager (only used by asynchronous rendering context).
  axl::Wrap<WorkerManager> worker_mgr;

  //! Context origin ID used in `data0` member of `BLContextCookie`.
  uint64_t context_origin_id;
  //! Used to generate unique IDs of this context.
  uint64_t state_id_counter;

  //! The number of states that can be saved by `BLContext::save()` call.
  uint32_t saved_state_limit;

  //! Destination image.
  BLImageCore dst_image;
  //! Destination image data.
  BLImageData dst_data;

  //! Minimum safe coordinate for integral transformation (scaled by 256.0 or 65536.0).
  double fp_min_safe_coord_d;
  //! Maximum safe coordinate for integral transformation (scaled by 256.0 or 65536.0).
  double fp_max_safe_coord_d;

  //! Pointers to essential transformations that can be applied to styles.
  const BLMatrix2D* transform_ptrs[BL_CONTEXT_STYLE_TRANSFORM_MODE_MAX_VALUE + 1u];

  //! \}

  //! \name Construction & Destruction
  //! \{

  BL_INLINE RasterContextImpl(void* static_data, size_t static_size) noexcept
    : context_flags(ContextFlags::kNoFlagsSet),
      rendering_mode(uint8_t(RenderingMode::kSync)),
      worker_mgr_initialized(false),
      render_target_info {},
      sync_work_data(this, nullptr),
      pipe_lookup_cache{},
      comp_op_simplify_info{},
      solid_format_table{},
      solid_override_fill_table{},
      solid_fetch_data_override_table{},
      saved_state{},
      shared_fill_state{},
      shared_stroke_state{},
      base_zone(8192, 16, static_data, static_size),
      fetch_data_pool(),
      saved_state_pool(),
      pipe_provider(),
      context_origin_id(UniqueIdGenerator::generate_id(UniqueIdGenerator::Domain::kContext)),
      state_id_counter(0),
      saved_state_limit(0),
      dst_image{},
      dst_data{},
      fp_min_safe_coord_d(0.0),
      fp_max_safe_coord_d(0.0) {
    context_type = BL_CONTEXT_TYPE_RASTER;
    transform_ptrs[BL_CONTEXT_STYLE_TRANSFORM_MODE_USER] = &internal_state.final_transform;
    transform_ptrs[BL_CONTEXT_STYLE_TRANSFORM_MODE_META] = &internal_state.meta_transform;
    transform_ptrs[BL_CONTEXT_STYLE_TRANSFORM_MODE_NONE] = &TransformDetail::identity_transform;
  }

  BL_INLINE ~RasterContextImpl() noexcept {
    destroy_worker_mgr();
  }

  //! \}

  //! \name Memory Management
  //! \{

  BL_INLINE_NODEBUG ArenaAllocator& fetch_data_zone() noexcept { return base_zone; }
  BL_INLINE_NODEBUG ArenaAllocator& saved_state_zone() noexcept { return base_zone; }

  BL_INLINE RenderFetchData* alloc_fetch_data() noexcept { return fetch_data_pool.alloc(fetch_data_zone()); }
  BL_INLINE void free_fetch_data(RenderFetchData* fetch_data) noexcept { fetch_data_pool.free(fetch_data); }

  BL_INLINE SavedState* alloc_saved_state() noexcept { return saved_state_pool.alloc(saved_state_zone()); }
  BL_INLINE void free_saved_state(SavedState* state) noexcept { saved_state_pool.free(state); }

  BL_INLINE void ensure_worker_mgr() noexcept {
    if (!worker_mgr_initialized) {
      worker_mgr.init();
      worker_mgr_initialized = true;
    }
  }

  BL_INLINE void destroy_worker_mgr() noexcept {
    if (worker_mgr_initialized) {
      worker_mgr.destroy();
      worker_mgr_initialized = false;
    }
  }

  //! \}

  //! \name Context Accessors
  //! \{

  BL_INLINE_NODEBUG bool is_sync() const noexcept { return rendering_mode == uint8_t(RenderingMode::kSync); }

  BL_INLINE_NODEBUG FormatExt format() const noexcept { return FormatExt(dst_data.format); }
  BL_INLINE_NODEBUG double fp_scale_d() const noexcept { return render_target_info.fp_scale_d; }
  BL_INLINE_NODEBUG double full_alpha_d() const noexcept { return render_target_info.full_alpha_d; }

  BL_INLINE_NODEBUG uint32_t band_count() const noexcept { return sync_work_data.band_count(); }
  BL_INLINE_NODEBUG uint32_t band_height() const noexcept { return sync_work_data.band_height(); }
  BL_INLINE_NODEBUG uint32_t command_quantization_shift_aa() const noexcept { return sync_work_data.command_quantization_shift_aa(); }
  BL_INLINE_NODEBUG uint32_t command_quantization_shift_fp() const noexcept { return sync_work_data.command_quantization_shift_fp(); }

  //! \}

  //! \name State Accessors
  //! \{

  BL_INLINE_NODEBUG uint8_t clip_mode() const noexcept { return sync_work_data.clip_mode; }

  BL_INLINE_NODEBUG uint8_t comp_op() const noexcept { return internal_state.comp_op; }
  BL_INLINE_NODEBUG BLFillRule fill_rule() const noexcept { return BLFillRule(internal_state.fill_rule); }
  BL_INLINE_NODEBUG const BLContextHints& hints() const noexcept { return internal_state.hints; }

  BL_INLINE_NODEBUG const BLStrokeOptions& stroke_options() const noexcept { return internal_state.stroke_options.dcast(); }
  BL_INLINE_NODEBUG const BLApproximationOptions& approximation_options() const noexcept { return internal_state.approximation_options; }

  BL_INLINE_NODEBUG uint32_t global_alpha_i() const noexcept { return internal_state.global_alpha_i; }
  BL_INLINE_NODEBUG double global_alpha_d() const noexcept { return internal_state.global_alpha; }

  BL_INLINE_NODEBUG const StyleData* get_style(size_t index) const noexcept { return &internal_state.style[index]; }

  BL_INLINE_NODEBUG const BLMatrix2D& meta_transform() const noexcept { return internal_state.meta_transform; }
  BL_INLINE_NODEBUG BLTransformType meta_transform_type() const noexcept { return BLTransformType(internal_state.meta_transform_type); }

  BL_INLINE_NODEBUG const BLMatrix2D& meta_transform_fixed() const noexcept { return internal_state.meta_transform_fixed; }
  BL_INLINE_NODEBUG BLTransformType meta_transform_fixed_type() const noexcept { return BLTransformType(internal_state.meta_transform_fixed_type); }

  BL_INLINE_NODEBUG const BLMatrix2D& user_transform() const noexcept { return internal_state.user_transform; }

  BL_INLINE_NODEBUG const BLMatrix2D& final_transform() const noexcept { return internal_state.final_transform; }
  BL_INLINE_NODEBUG BLTransformType final_transform_type() const noexcept { return BLTransformType(internal_state.final_transform_type); }

  BL_INLINE_NODEBUG const BLMatrix2D& final_transform_fixed() const noexcept { return internal_state.final_transform_fixed; }
  BL_INLINE_NODEBUG BLTransformType final_transform_fixed_type() const noexcept { return BLTransformType(internal_state.final_transform_fixed_type); }

  BL_INLINE_NODEBUG const BLPointI& translation_i() const noexcept { return internal_state.translation_i; }
  BL_INLINE_NODEBUG void set_translation_i(const BLPointI& pt) noexcept { internal_state.translation_i = pt; }

  BL_INLINE_NODEBUG const BLBoxI& meta_clip_box_i() const noexcept { return internal_state.meta_clip_box_i; }
  BL_INLINE_NODEBUG const BLBoxI& final_clip_box_i() const noexcept { return internal_state.final_clip_box_i; }
  BL_INLINE_NODEBUG const BLBox& final_clip_box_d() const noexcept { return internal_state.final_clip_box_d; }

  BL_INLINE_NODEBUG const BLBoxI& final_clip_box_fixed_i() const noexcept { return sync_work_data.edge_builder._clip_box_i; }
  BL_INLINE_NODEBUG const BLBox& final_clip_box_fixed_d() const noexcept { return sync_work_data.edge_builder._clip_box_d; }
  BL_INLINE_NODEBUG void set_final_clip_box_fixed_d(const BLBox& clip_box) { sync_work_data.edge_builder.set_clip_box(clip_box); }

  //! \}

  //! \name Error Accumulation
  //! \{

  BL_INLINE_NODEBUG BLResult accumulate_error(BLResult error) noexcept { return sync_work_data.accumulate_error(error); }

  //! \}
};

//! \}
//! \endcond

} // {bl::RasterEngine}

BL_HIDDEN BLResult bl_raster_context_init_impl(BLContextCore* self, BLImageCore* image, const BLContextCreateInfo* options) noexcept;
BL_HIDDEN void bl_raster_context_on_init(BLRuntimeContext* rt) noexcept;

#endif // BLEND2D_RASTER_RASTER_CONTEXT_P_H_INCLUDED
