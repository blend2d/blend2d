// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_CORE_API_IMPL_H_INCLUDED
#define BLEND2D_CORE_API_IMPL_H_INCLUDED

#include <blend2d/core/api.h>
#include <blend2d/core/object.h>
#include <blend2d/core/object_layer.h>

#include <atomic>

//! \addtogroup bl_impl
//! \{

//! \name Atomic Operations
//!
//! Atomic operations are used extensively across Blend2D for reference counting and caching. You should always
//! use operations defined here as they implement all possible cases that Blend2D deals with in a correct way
//! (and if there is a bug it makes it fixable in a single place).
//!
//! \{


//! \}

//! \}

#endif // BLEND2D_CORE_API_IMPL_H_INCLUDED
