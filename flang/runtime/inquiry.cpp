//===-- runtime/inquiry.cpp --------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// Implements the inquiry intrinsic functions of Fortran 2018 that
// inquire about shape information of arrays -- LBOUND and SIZE.

#include "flang/Runtime/inquiry.h"
#include "copy.h"
#include "terminator.h"
#include "tools.h"
#include "flang/Runtime/descriptor.h"
#include <algorithm>

namespace Fortran::runtime {

template <int KIND> struct RawStoreIntegerAt {
  RT_API_ATTRS void operator()(
      void *contiguousIntegerArray, std::size_t at, std::int64_t value) const {
    reinterpret_cast<Fortran::runtime::CppTypeFor<
        Fortran::common::TypeCategory::Integer, KIND> *>(
        contiguousIntegerArray)[at] = value;
  }
};

extern "C" {
std::int64_t RTDEF(LboundDim)(
    const Descriptor &array, int dim, const char *sourceFile, int line) {
  if (dim < 1 || dim > array.rank()) {
    Terminator terminator{sourceFile, line};
    terminator.Crash(
        "SIZE: bad DIM=%d for ARRAY with rank=%d", dim, array.rank());
  }
  const Dimension &dimension{array.GetDimension(dim - 1)};
  return static_cast<std::int64_t>(dimension.LowerBound());
}

void RTDEF(Ubound)(Descriptor &result, const Descriptor &array, int kind,
    const char *sourceFile, int line) {
  SubscriptValue extent[1]{array.rank()};
  result.Establish(TypeCategory::Integer, kind, nullptr, 1, extent,
      CFI_attribute_allocatable);
  // The array returned by UBOUND has a lower bound of 1 and an extent equal to
  // the rank of its input array.
  result.GetDimension(0).SetBounds(1, array.rank());
  Terminator terminator{sourceFile, line};
  if (int stat{result.Allocate()}) {
    terminator.Crash(
        "UBOUND: could not allocate memory for result; STAT=%d", stat);
  }
  auto storeIntegerAt = [&](std::size_t atIndex, std::int64_t value) {
    Fortran::runtime::ApplyIntegerKind<StoreIntegerAt, void>(
        kind, terminator, result, atIndex, value);
  };

  INTERNAL_CHECK(result.rank() == 1);
  for (SubscriptValue i{0}; i < array.rank(); ++i) {
    const Dimension &dimension{array.GetDimension(i)};
    storeIntegerAt(i, dimension.UpperBound());
  }
}

std::int64_t RTDEF(Size)(
    const Descriptor &array, const char *sourceFile, int line) {
  std::int64_t result{1};
  for (int i = 0; i < array.rank(); ++i) {
    const Dimension &dimension{array.GetDimension(i)};
    result *= dimension.Extent();
  }
  return result;
}

std::int64_t RTDEF(SizeDim)(
    const Descriptor &array, int dim, const char *sourceFile, int line) {
  if (dim < 1 || dim > array.rank()) {
    Terminator terminator{sourceFile, line};
    terminator.Crash(
        "SIZE: bad DIM=%d for ARRAY with rank=%d", dim, array.rank());
  }
  const Dimension &dimension{array.GetDimension(dim - 1)};
  return static_cast<std::int64_t>(dimension.Extent());
}

void RTDEF(Shape)(void *result, const Descriptor &array, int kind) {
  Terminator terminator{__FILE__, __LINE__};
  INTERNAL_CHECK(array.rank() <= common::maxRank);
  for (SubscriptValue i{0}; i < array.rank(); ++i) {
    const Dimension &dimension{array.GetDimension(i)};
    Fortran::runtime::ApplyIntegerKind<RawStoreIntegerAt, void>(
        kind, terminator, result, i, dimension.Extent());
  }
}

} // extern "C"
} // namespace Fortran::runtime
