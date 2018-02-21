/*!
 ******************************************************************************
 *
 * \file
 *
 * \brief   RAJA header file defining automatic and builtin atomic operations.
 *
 ******************************************************************************
 */

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
// Copyright (c) 2016-17, Lawrence Livermore National Security, LLC.
//
// Produced at the Lawrence Livermore National Laboratory
//
// LLNL-CODE-689114
//
// All rights reserved.
//
// This file is part of RAJA.
//
// For details about use and distribution, please read RAJA/LICENSE.
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//

#ifndef RAJA_policy_atomic_auto_HPP
#define RAJA_policy_atomic_auto_HPP

#include "RAJA/config.hpp"
#include "RAJA/util/defines.hpp"

#include "RAJA/policy/sequential/atomic.hpp"

/*!
 * Provides priority between atomic policies that should do the "right thing"
 *
 * If we are in a CUDA __device__ function, then it always uses the cuda_atomic
 * policy.
 *
 * If we are in a ROCm [[hc]] function, then it always uses the rocm_atomic
 * policy.
 *
 * Next, if OpenMP is enabled we always use the omp_atomic, which should
 * generally work everywhere.
 *
 * Finally, we fallback on the seq_atomic, which performs non-atomic operations
 * because we assume there is no thread safety issues (no parallel model)
 */
#ifdef __CUDA_ARCH__
 #define RAJA_AUTO_ATOMIC \
  RAJA::atomic::cuda_atomic {}
#else
 #if defined(RAJA_ENABLE_ROCM) && (__KALMAR_ACCELERATOR__ == 1)
  #define RAJA_AUTO_ATOMIC \
   RAJA::atomic::rocm_atomic {}
 #else
  #ifdef RAJA_ENABLE_OPENMP
   #define RAJA_AUTO_ATOMIC \
     RAJA::atomic::omp_atomic {}
  #else
   #define RAJA_AUTO_ATOMIC \
     RAJA::atomic::seq_atomic {}
  #endif
 #endif
#endif


namespace RAJA
{
namespace atomic
{

//! Atomic policy that automatically does "the right thing"
struct auto_atomic {
};


template <typename T>
RAJA_INLINE RAJA_HOST_DEVICE T atomicAdd(auto_atomic, T volatile *acc, T value)
{
  return atomicAdd(RAJA_AUTO_ATOMIC, acc, value);
}


template <typename T>
RAJA_INLINE RAJA_HOST_DEVICE T atomicSub(auto_atomic, T volatile *acc, T value)
{
  return atomicSub(RAJA_AUTO_ATOMIC, acc, value);
}

template <typename T>
RAJA_INLINE RAJA_HOST_DEVICE T atomicMin(auto_atomic, T volatile *acc, T value)
{
  return atomicMin(RAJA_AUTO_ATOMIC, acc, value);
}

template <typename T>
RAJA_INLINE RAJA_HOST_DEVICE T atomicMax(auto_atomic, T volatile *acc, T value)
{
  return atomicMax(RAJA_AUTO_ATOMIC, acc, value);
}

template <typename T>
RAJA_INLINE RAJA_HOST_DEVICE T atomicInc(auto_atomic, T volatile *acc)
{
  return atomicInc(RAJA_AUTO_ATOMIC, acc);
}

template <typename T>
RAJA_INLINE RAJA_HOST_DEVICE T atomicInc(auto_atomic,
                                         T volatile *acc,
                                         T compare)
{
  return atomicInc(RAJA_AUTO_ATOMIC, acc, compare);
}

template <typename T>
RAJA_INLINE RAJA_HOST_DEVICE T atomicDec(auto_atomic, T volatile *acc)
{
  return atomicDec(RAJA_AUTO_ATOMIC, acc);
}

template <typename T>
RAJA_INLINE RAJA_HOST_DEVICE T atomicDec(auto_atomic,
                                         T volatile *acc,
                                         T compare)
{
  return atomicDec(RAJA_AUTO_ATOMIC, acc, compare);
}

template <typename T>
RAJA_INLINE RAJA_HOST_DEVICE T atomicAnd(auto_atomic, T volatile *acc, T value)
{
  return atomicAnd(RAJA_AUTO_ATOMIC, acc, value);
}

template <typename T>
RAJA_INLINE RAJA_HOST_DEVICE T atomicOr(auto_atomic, T volatile *acc, T value)
{
  return atomicOr(RAJA_AUTO_ATOMIC, acc, value);
}

template <typename T>
RAJA_INLINE RAJA_HOST_DEVICE T atomicXor(auto_atomic, T volatile *acc, T value)
{
  return atomicXor(RAJA_AUTO_ATOMIC, acc, value);
}

template <typename T>
RAJA_INLINE RAJA_HOST_DEVICE T atomicExchange(auto_atomic,
                                              T volatile *acc,
                                              T value)
{
  return atomicExchange(RAJA_AUTO_ATOMIC, acc, value);
}

template <typename T>
RAJA_INLINE RAJA_HOST_DEVICE T
atomicCAS(auto_atomic, T volatile *acc, T compare, T value)
{
  return atomicCAS(RAJA_AUTO_ATOMIC, acc, compare, value);
}


}  // namespace atomic
}  // namespace RAJA

// make sure this define doesn't bleed out of this header
#undef RAJA_AUTO_ATOMIC

#endif
