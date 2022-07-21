// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

// ----------------------------------------------------------------------------
// IMPORTANT: DO NOT USE THIS HEADER IN PRODUCTION, ONLY FOR BUG REPORTING!
//
// This file provides debug helpers that are not a part of Blend2D library.
// Functions this header provides are not exported, they are only provided
// for making it easier to report bugs and to give more information about the
// environment where such bugs happened to Blend2D developers. The functions
// defined below are not stable and are subject to change in future Blend2D
// releases.
//
// This file can be included by both C and C++ API users.
// ----------------------------------------------------------------------------

#ifndef BLEND2D_DEBUG_H_INCLUDED
#define BLEND2D_DEBUG_H_INCLUDED

#include <stdio.h>
#include "blend2d.h"

//! \cond INTERNAL

// Forward Declarations
// ====================

static void blDebugObject_(const void* obj, const char* name, int indent);

// BLDebug - Begin
// ===============

#define BL_DEBUG_OUT(MSG) blRuntimeMessageFmt("%*s%s", indent * 2, "", MSG);
#define BL_DEBUG_FMT(FMT, ...) blRuntimeMessageFmt("%*s" FMT, indent * 2, "", __VA_ARGS__);

// BLDebug - Utilities
// ===================

static const char* blDebugGetEnumAsString(uint32_t value, const char* enumData) {
  uint32_t i = 0;
  const char* p = enumData;

  while (i != value) {
    // 0 indicates end of data.
    if (!*p)
      return "Unknown";

    // Skip the entire sub-string.
    while (*++p)
      continue;

    i++; // Advance the current index.
    p++; // Skip the zero value.
  }

  return p;
}

// BLDebug - Runtime
// =================

static void blDebugRuntimeCpuFeatures(char* buf, size_t bufSize, uint32_t arch, uint32_t features) {
  if (!arch) {
    #if defined(_M_X64) || defined(__x86_64__) || defined(_M_IX86) || defined(__X86__) || defined(__i386__)
    arch = BL_RUNTIME_CPU_ARCH_X86;
    #endif

    #if defined(__arm64__) || defined(__aarch64__) || defined(_M_ARM) || defined(_M_ARMT) || defined(__arm__) || defined(__thumb__) || defined(__thumb2__)
    arch = BL_RUNTIME_CPU_ARCH_ARM;
    #endif

    #if defined(_MIPS_ARCH_MIPS64) || defined(__mips64) || defined(_MIPS_ARCH_MIPS32) || defined(_M_MRX000) || defined(__mips__)
    arch = BL_RUNTIME_CPU_ARCH_MIPS;
    #endif
  }

  switch (arch) {
    case BL_RUNTIME_CPU_ARCH_X86:
      snprintf(buf, bufSize, "%s%s%s%s%s%s%s",
        (features & BL_RUNTIME_CPU_FEATURE_X86_SSE2  ) ? "SSE2 "   : "",
        (features & BL_RUNTIME_CPU_FEATURE_X86_SSE3  ) ? "SSE3 "   : "",
        (features & BL_RUNTIME_CPU_FEATURE_X86_SSSE3 ) ? "SSSE3 "  : "",
        (features & BL_RUNTIME_CPU_FEATURE_X86_SSE4_1) ? "SSE4.1 " : "",
        (features & BL_RUNTIME_CPU_FEATURE_X86_SSE4_2) ? "SSE4.2 " : "",
        (features & BL_RUNTIME_CPU_FEATURE_X86_AVX   ) ? "AVX "    : "",
        (features & BL_RUNTIME_CPU_FEATURE_X86_AVX2  ) ? "AVX2 "   : "");
      break;

    default:
      buf[0] = '\0';
      break;
  }
}

//! Dumps `BLRuntimeBuildInfo` queried through `blRuntimeQueryInfo()`.
static void blDebugRuntimeBuildInfo(void) {
  const char* buildMode = "";
  char baselineCpuFeatures[128];
  char supportedCpuFeatures[128];

  BLRuntimeBuildInfo info;
  blRuntimeQueryInfo(BL_RUNTIME_INFO_TYPE_BUILD, &info);

  #if defined(BL_STATIC)
  buildMode = "Static";
  #else
  buildMode = "Shared";
  #endif

  blDebugRuntimeCpuFeatures(baselineCpuFeatures, 128, 0, info.baselineCpuFeatures);
  blDebugRuntimeCpuFeatures(supportedCpuFeatures, 128, 0, info.supportedCpuFeatures);

  blRuntimeMessageFmt(
    "BuildInformation: {\n"
    "  Version: %u.%u.%u\n"
    "  BuildType: %s\n"
    "  BuildMode: %s\n"
    "  BaselineCpuFeatures: %s\n"
    "  SupportedCpuFeatures: %s\n"
    "  Compiler: %s\n"
    "  MaxImageSize: %u\n"
    "  MaxThreadCount: %u\n"
    "}\n",
    info.majorVersion,
    info.minorVersion,
    info.patchVersion,
    info.buildType == BL_RUNTIME_BUILD_TYPE_DEBUG ? "Debug" : "Release",
    buildMode,
    baselineCpuFeatures,
    supportedCpuFeatures,
    info.compilerInfo,
    info.maxImageSize,
    info.maxThreadCount);
}

//! Dumps `BLRuntimeSystemInfo` queried through `blRuntimeQueryInfo()`.
static void blDebugRuntimeSystemInfo(void) {
  const char* cpuArchEnum = "NONE\0X86\0ARM\0MIPS\0";
  const char* os = "Unknown";
  char cpuFeatures[128];

  BLRuntimeSystemInfo info;
  blRuntimeQueryInfo(BL_RUNTIME_INFO_TYPE_SYSTEM, &info);
  cpuFeatures[0] = '\0';

#if defined(__linux__)
  os = "Linux";
#elif defined(__APPLE__)
  os = "Apple";
#elif defined(__DragonFly__)
  os = "DragonFlyBSD";
#elif defined(__FreeBSD__)
  os = "FreeBSD";
#elif defined(__NetBSD__)
  os = "NetBSD";
#elif defined(__OpenBSD__)
  os = "OpenBSD";
#elif defined(__HAIKU__)
  os = "Haiku";
#elif defined(_WIN32)
  os = "Windows";
#endif

  blDebugRuntimeCpuFeatures(cpuFeatures, 128, info.cpuArch, info.cpuFeatures);

  blRuntimeMessageFmt(
    "SystemInformation: {\n"
    "  OperatingSystem: %s\n"
    "  CpuArch: %s [%u bit]\n"
    "  CpuFeatures: %s\n"
    "  ThreadCount: %u\n"
    "  ThreadStackSize: %u\n"
    "  AllocationGranularity: %u\n"
    "}\n",
    os,
    blDebugGetEnumAsString(info.cpuArch, cpuArchEnum), sizeof(void*) >= 8 ? 64 : 32,
    cpuFeatures,
    info.threadCount,
    info.threadStackSize,
    info.allocationGranularity);
}

// BLDebug - Matrix
// ================

static void blDebugMatrix2D_(const BLMatrix2D* obj, const char* name, int indent) {
  const char* matrixTypeEnum = "IDENTITY\0TRANSLATE\0SCALE\0SWAP\0AFFINE\0INVALID\0";

  BL_DEBUG_FMT("%s: [%s] {\n", name, blDebugGetEnumAsString(blMatrix2DGetType(obj), matrixTypeEnum));
  BL_DEBUG_FMT("  [% 3.14f |% 3.14f]\n", obj->m00, obj->m01);
  BL_DEBUG_FMT("  [% 3.14f |% 3.14f]\n", obj->m10, obj->m11);
  BL_DEBUG_FMT("  [% 3.14f |% 3.14f]\n", obj->m20, obj->m21);
  BL_DEBUG_OUT("}\n");
}

// BLDebug - StrokeOptions
// =======================

static void blDebugStrokeOptions_(const BLStrokeOptionsCore* obj, const char* name, int indent) {
  const char* strokeCapPositionEnum = "StartCap\0EndCap\0";
  const char* strokeCapEnum = "BUTT\0SQUARE\0ROUND\0ROUND_REV\0TRIANGLE\0TRIANGLE_REV\0";
  const char* strokeJoinEnum = "MITER_CLIP\0MITER_BEVEL\0MITER_ROUND\0BEVEL\0ROUND\0";
  const char* strokeTransformOrderEnum = "AFTER\0BEFORE\0";

  uint32_t i;

  BL_DEBUG_FMT("%s: {\n", name);
  indent++;

  for (i = 0; i <= BL_STROKE_CAP_POSITION_MAX_VALUE; i++)
    BL_DEBUG_FMT("%s: %s\n", blDebugGetEnumAsString(i, strokeCapPositionEnum), blDebugGetEnumAsString(obj->caps[i], strokeCapEnum));
  BL_DEBUG_FMT("Join: %s\n", blDebugGetEnumAsString(obj->join, strokeJoinEnum));
  BL_DEBUG_FMT("TransformOrder: %s\n", blDebugGetEnumAsString(obj->transformOrder, strokeTransformOrderEnum));
  BL_DEBUG_FMT("Width: %g\n", obj->width);
  BL_DEBUG_FMT("MiterLimit: %g\n", obj->miterLimit);
  BL_DEBUG_FMT("DashOffset: %g\n", obj->dashOffset);
  blDebugObject_(&obj->dashArray, "DashArray", indent);

  indent--;
  BL_DEBUG_OUT("}\n");
}
static void blDebugStrokeOptions(const BLStrokeOptionsCore* obj) { return blDebugStrokeOptions_(obj, "BLStrokeOptions", 0); }

// BLDebug - Array
// ===============

static void blDebugArray_(const BLArrayCore* obj, const char* name, int indent) {
  BLObjectType objectType = blVarGetType(obj);
  const void* voidData = blArrayGetData(obj);

  size_t i;
  size_t size = blArrayGetSize(obj);

  if (size == 0) {
    BL_DEBUG_FMT("%s: {}\n", name);
    return;
  }

  BL_DEBUG_FMT("%s: {\n", name);
  indent++;

  switch (objectType) {
    case BL_OBJECT_TYPE_ARRAY_OBJECT: {
      char prefix[64];
      const BLObjectCore** data = (const BLObjectCore**)voidData;
      for (i = 0; i < size; i++) {
        snprintf(prefix, 64, "[%zu]", i);
        blDebugObject_(data[i], prefix, indent);
      }
      break;
    }
    case BL_OBJECT_TYPE_ARRAY_INT8: {
      const int8_t* data = (const int8_t*)voidData;
      for (i = 0; i < size; i++)
        BL_DEBUG_FMT("[%zu] %d", i, data[i]);
      break;
    }
    case BL_OBJECT_TYPE_ARRAY_UINT8: {
      const uint8_t* data = (const uint8_t*)voidData;
      for (i = 0; i < size; i++)
        BL_DEBUG_FMT("[%zu] %u", i, data[i]);
      break;
    }
    case BL_OBJECT_TYPE_ARRAY_INT16: {
      const int16_t* data = (const int16_t*)voidData;
      for (i = 0; i < size; i++)
        BL_DEBUG_FMT("[%zu] %d", i, data[i]);
      break;
    }
    case BL_OBJECT_TYPE_ARRAY_UINT16: {
      const uint16_t* data = (const uint16_t*)voidData;
      for (i = 0; i < size; i++)
        BL_DEBUG_FMT("[%zu] %u", i, data[i]);
      break;
    }
    case BL_OBJECT_TYPE_ARRAY_INT32: {
      const int32_t* data = (const int32_t*)voidData;
      for (i = 0; i < size; i++)
        BL_DEBUG_FMT("[%zu] %d", i, data[i]);
      break;
    }
    case BL_OBJECT_TYPE_ARRAY_UINT32: {
      const uint32_t* data = (const uint32_t*)voidData;
      for (i = 0; i < size; i++)
        BL_DEBUG_FMT("[%zu] %u", i, data[i]);
      break;
    }
    case BL_OBJECT_TYPE_ARRAY_INT64: {
      const int64_t* data = (const int64_t*)voidData;
      for (i = 0; i < size; i++)
        BL_DEBUG_FMT("[%zu] %lld", i, (long long)data[i]);
      break;
    }
    case BL_OBJECT_TYPE_ARRAY_UINT64: {
      const uint64_t* data = (const uint64_t*)voidData;
      for (i = 0; i < size; i++)
        BL_DEBUG_FMT("[%zu] %llu", i, (unsigned long long)data[i]);
      break;
    }
    case BL_OBJECT_TYPE_ARRAY_FLOAT32: {
      const float* data = (const float*)voidData;
      for (i = 0; i < size; i++)
        BL_DEBUG_FMT("[%zu] %g", i, data[i]);
      break;
    }
    case BL_OBJECT_TYPE_ARRAY_FLOAT64: {
      const double* data = (const double*)voidData;
      for (i = 0; i < size; i++)
        BL_DEBUG_FMT("[%zu] %g", i, data[i]);
      break;
    }
    default:
      break;
  }

  indent--;
  BL_DEBUG_OUT("}\n");
}

// BLDebug - Image
// ===============

static void blDebugImage_(const BLImageCore* obj, const char* name, int indent) {
  const char* formatEnum = "NONE\0PRGB32\0XRGB32\0A8\0";

  BLImageData data;
  blImageGetData(obj, &data);

  BL_DEBUG_FMT("%s: {\n", name);
  BL_DEBUG_FMT("  Size: %ux%u\n", data.size.w, data.size.h);
  BL_DEBUG_FMT("  Format: %s\n", blDebugGetEnumAsString(data.format, formatEnum));
  BL_DEBUG_OUT("}\n");
}

// BLDebug - Path
// ==============

static void blDebugPath_(const BLPathCore* obj, const char* name, int indent) {
  size_t i = 0;
  size_t size = blPathGetSize(obj);

  const uint8_t* cmd = blPathGetCommandData(obj);
  const BLPoint* vtx = blPathGetVertexData(obj);

  BL_DEBUG_FMT("%s: {\n", name);
  indent++;

  while (i < size) {
    switch (cmd[i]) {
      case BL_PATH_CMD_MOVE:
        BL_DEBUG_FMT("p.moveTo(%g, %g);\n", vtx[i].x, vtx[i].y);
        i++;
        continue;
      case BL_PATH_CMD_ON:
        BL_DEBUG_FMT("p.lineTo(%g, %g);\n", vtx[i].x, vtx[i].y);
        i++;
        continue;
      case BL_PATH_CMD_QUAD:
        if ((size - i) < 2)
          break;
        BL_DEBUG_FMT("p.quadTo(%g, %g, %g, %g);\n", vtx[i].x, vtx[i].y, vtx[i+1].x, vtx[i+1].y);
        i += 2;
        continue;
      case BL_PATH_CMD_CUBIC:
        if ((size - i) < 3)
          break;
        BL_DEBUG_FMT("p.cubicTo(%g, %g, %g, %g, %g, %g);\n", vtx[i].x, vtx[i].y, vtx[i+1].x, vtx[i+1].y, vtx[i+2].x, vtx[i+2].y);
        i += 3;
        continue;
      case BL_PATH_CMD_CLOSE:
        BL_DEBUG_OUT("p.close();\n");
        i++;
        continue;
    }

    BL_DEBUG_FMT("p.unknownCommand(%u, %g, %g);\n", cmd[i], vtx[i].x, vtx[i].y);
    i++;
  }

  indent--;
  BL_DEBUG_OUT("}\n");
}

// BLDebug - Context
// =================

static void blDebugContext_(const BLContextCore* obj, const char* name, int indent) {
  const char* contextTypeEnum = "NONE\0DUMMY\0PROXY\0RASTER\0";
  const char* fillRuleEnum = "NON_ZERO\0EVEN_ODD\0";

  const BLContextState* state = ((BLContextImpl*)obj->_d.impl)->state;

  BL_DEBUG_FMT("%s: {\n", name);
  indent++;

  BL_DEBUG_FMT("Type: %s\n", blDebugGetEnumAsString(blContextGetType(obj), contextTypeEnum));
  BL_DEBUG_FMT("GlobalAlpha: %f\n", state->globalAlpha);
  BL_DEBUG_FMT("SavedStateCount: %zu\n", state->savedStateCount);

  blDebugMatrix2D_(&state->metaMatrix, "MetaMatrix", indent);
  blDebugMatrix2D_(&state->userMatrix, "UserMatrix", indent);

  BL_DEBUG_FMT("FillAlpha: %f\n", state->styleAlpha[BL_CONTEXT_OP_TYPE_FILL]);
  BL_DEBUG_FMT("FillRule: %s\n", blDebugGetEnumAsString(state->fillRule, fillRuleEnum));

  BL_DEBUG_FMT("StrokeAlpha: %f\n", state->styleAlpha[BL_CONTEXT_OP_TYPE_STROKE]);
  blDebugStrokeOptions_(&state->strokeOptions, "StrokeOptions", indent);

  indent--;
  BL_DEBUG_OUT("}\n");
}

// BLDebug - Object
// ================

static void blDebugObject_(const void* obj, const char* name, int indent) {
  BLObjectType type = blVarGetType(obj);
  switch (type) {
    case BL_OBJECT_TYPE_ARRAY_OBJECT:
    case BL_OBJECT_TYPE_ARRAY_INT8:
    case BL_OBJECT_TYPE_ARRAY_UINT8:
    case BL_OBJECT_TYPE_ARRAY_INT16:
    case BL_OBJECT_TYPE_ARRAY_UINT16:
    case BL_OBJECT_TYPE_ARRAY_INT32:
    case BL_OBJECT_TYPE_ARRAY_UINT32:
    case BL_OBJECT_TYPE_ARRAY_INT64:
    case BL_OBJECT_TYPE_ARRAY_UINT64:
    case BL_OBJECT_TYPE_ARRAY_FLOAT32:
    case BL_OBJECT_TYPE_ARRAY_FLOAT64:
    case BL_OBJECT_TYPE_ARRAY_STRUCT_1:
    case BL_OBJECT_TYPE_ARRAY_STRUCT_2:
    case BL_OBJECT_TYPE_ARRAY_STRUCT_3:
    case BL_OBJECT_TYPE_ARRAY_STRUCT_4:
    case BL_OBJECT_TYPE_ARRAY_STRUCT_6:
    case BL_OBJECT_TYPE_ARRAY_STRUCT_8:
    case BL_OBJECT_TYPE_ARRAY_STRUCT_10:
    case BL_OBJECT_TYPE_ARRAY_STRUCT_12:
    case BL_OBJECT_TYPE_ARRAY_STRUCT_16:
    case BL_OBJECT_TYPE_ARRAY_STRUCT_20:
    case BL_OBJECT_TYPE_ARRAY_STRUCT_24:
    case BL_OBJECT_TYPE_ARRAY_STRUCT_32:
      blDebugArray_((const BLArrayCore*)obj, name, indent);
      break;

    case BL_OBJECT_TYPE_IMAGE:
      blDebugImage_((const BLImageCore*)obj, name, indent);
      break;

    case BL_OBJECT_TYPE_PATH:
      blDebugPath_((const BLPathCore*)obj, name, indent);
      break;

    case BL_OBJECT_TYPE_CONTEXT:
      blDebugContext_((const BLContextCore*)obj, name, indent);
      break;

    default:
      BL_DEBUG_FMT("BLObject { Type: %u }\n", (uint32_t)type);
      break;
  }
}

// BLDebug - Public API
// ====================

//! Dumps both `BLRuntimeBuildInfo` and `BLRuntimeSystemInfo`.
//!
//! This function should be used to retrieve information for Blend2D bug reports.
static void blDebugRuntime(void) {
  blDebugRuntimeBuildInfo();
  blDebugRuntimeSystemInfo();
}

static void blDebugMatrix2D(const BLMatrix2D* obj) {
  blDebugMatrix2D_(obj, "BLMatrix", 0);
}

//! Dumps BLArrayCore or BLArray<T>.
static void blDebugArray(const BLArrayCore* obj) {
  blDebugArray_(obj, "BLArray", 0);
}

//! Dumps BLImageCore or BLImage.
static void blDebugImage(const BLImageCore* obj) {
  blDebugImage_(obj, "BLImage", 0);
}

//! Dumps BLPathCore or BLPath.
static void blDebugPath(const BLPathCore* obj) {
  return blDebugPath_(obj, "BLPath", 0);
}

//! Dumps BLContextCore or BLContext.
static void blDebugContext(const BLContextCore* obj) {
  return blDebugContext_(obj, "BLContext", 0);
}

//! Dumps BLObjectCore or BLObject.
//!
//! You can use this function with any object that implements `BLObject`
//! interface.
static void blDebugObject(const void* obj) {
  return blDebugObject_(obj, "BLObject", 0);
}

// BLDebug - End
// =============

#undef BL_DEBUG_FMT
#undef BL_DEBUG_OUT

//! \endcond

#endif // BLEND2D_DEBUG_H_INCLUDED
