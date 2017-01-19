/*!
 ******************************************************************************
 *
 * \file
 *
 * \brief   
 *
 ******************************************************************************
 */

#ifndef RAJA_CUDA_Logger_HXX
#define RAJA_CUDA_Logger_HXX

#include "RAJA/config.hxx"

#ifdef RAJA_ENABLE_CUDA

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
// Copyright (c) 2016, Lawrence Livermore National Security, LLC.
//
// Produced at the Lawrence Livermore National Laboratory
//
// LLNL-CODE-689114
//
// All rights reserved.
//
// This file is part of RAJA.
//
// For additional details, please also read RAJA/LICENSE.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the disclaimer below.
//
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the disclaimer (as noted below) in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of the LLNS/LLNL nor the names of its contributors may
//   be used to endorse or promote products derived from this software without
//   specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL LAWRENCE LIVERMORE NATIONAL SECURITY,
// LLC, THE U.S. DEPARTMENT OF ENERGY OR CONTRIBUTORS BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
// OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
// IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//

#include "RAJA/Logger.hxx"

#include <type_traits>
#include <vector>

#include <string.h>
#include <stdio.h>
#include <stdint.h>

namespace RAJA
{

namespace Internal
{

  enum struct print_types: char {
    int8,
    int16,
    int32,
    int64,
    uint8,
    uint16,
    uint32,
    uint64,
    flt32,
    flt64,
    ptr,
    char_ptr,
    char_arr
  };

class CudaLogManager {
public:
  static const int buffer_size = 1024*1024;
  static const int error_buffer_size = 4*1024;

  static inline CudaLogManager* getInstance()
  {
    static CudaLogManager* me = new (allocateInstance()) CudaLogManager();
    return me;
  }

  static void s_check_logs()
  {
    if (s_instance_buffer != nullptr) {
      getInstance()->check_logs();
    }
  }

  void check_logs() volatile
  {
    // fprintf(stderr, "RAJA logger: s_instance_buffer = %p.\n", s_instance_buffer);
    if (m_flag) {
      // fprintf(stderr, "RAJA logger: found log in queue.\n");
      // handle logs
      handle_in_order();
    }
  }

  template < typename T_fmt, typename... Ts >
  RAJA_HOST_DEVICE
  void
  error_impl(RAJA::logging_function_type func, int udata, T_fmt const& fmt, Ts const&... args) volatile
  {
#ifdef __CUDA_ARCH__
    int warpIdx = ((threadIdx.z * (blockDim.x * blockDim.y))
                + (threadIdx.y * blockDim.x) + threadIdx.x) % WARP_SIZE;
    int warpIdxmask = 1 << warpIdx;
    int mask = __ballot(1);
    int first = __ffs(mask) - 1;
    loggingID_type num;
    if (warpIdx == first) {
      while ( atomicCAS(&md_data->mutex, 0, 1) != 0 ); // lock per warp
      num = atomicAdd(&md_data->count, __popc(mask));
    }
    num = RAJA::HIDDEN::shfl(num, first);
    num += __popc(mask & (warpIdxmask - 1));
    // serialize warp
    for (int i = 0; i < WARP_SIZE; i++) {
      if (warpIdxmask == (1 << i)) {
        char* buf_pos = m_error_pos;
        char* buf_end = m_error_end;

        bool err = write_log(buf_pos, buf_end, num, func, udata, fmt, args...);
        if (err) {
          printf("RAJA logger error: Writing error on device failed.\n");
          buf_pos = m_error_pos;
          write_log(buf_pos, buf_end, num, func, udata, "RAJA logger error: Writing error on device failed.");
        }

        m_error_pos = buf_pos;
        __threadfence_block();
      }
    }
    __threadfence_system();
    if (warpIdx == first) {
      m_flag = true;
      atomicExch(&md_data->mutex, 0); // unlock
    }
#else
    cudaDeviceSynchronize();
    check_logs();
    ResourceHandler::getInstance().error_cleanup(RAJA::error::user);
    int len = snprintf(nullptr, 0, fmt, args...);
    if (len >= 0) {
      char* msg = new char[len+1];
      snprintf(msg, len+1, fmt, args...);
      func(udata, msg);
      delete[] msg;
    } else {
      fprintf(stderr, "RAJA logger error: could not format message");
    }
    exit(1);
#endif
  }

  template < typename T_fmt, typename... Ts >
  RAJA_HOST_DEVICE
  void
  log_impl(RAJA::logging_function_type func, int udata, T_fmt const& fmt, Ts const&... args) volatile
  {
#ifdef __CUDA_ARCH__
    int warpIdx = ((threadIdx.z * (blockDim.x * blockDim.y))
                + (threadIdx.y * blockDim.x) + threadIdx.x) % WARP_SIZE;
    int warpIdxmask = 1 << warpIdx;
    int mask = __ballot(1);
    int first = __ffs(mask) - 1;
    loggingID_type num;
    if (warpIdx == first) {
      while ( atomicCAS(&md_data->mutex, 0, 1) != 0 ); // lock per warp
      num = atomicAdd(&md_data->count, __popc(mask));
    }
    num = RAJA::HIDDEN::shfl(num, first);
    num += __popc(mask & (warpIdxmask - 1));
    // serialize warp
    for (int i = 0; i < WARP_SIZE; i++) {
      if (warpIdxmask == (1 << i)) {
        char* buf_pos = m_log_pos;
        char* buf_end = m_log_end;

        bool err = write_log(buf_pos, buf_end, num, func, udata, fmt, args...);
        if (err) {
          printf("RAJA logger error: Writing log on device failed.\n");
          buf_pos = m_error_pos;
          write_log(buf_pos, buf_end, num, func, udata, "RAJA logger error: Writing log on device failed.");
        }

        m_log_pos = buf_pos;
        __threadfence_block();
      }
    }
    __threadfence_system();
    if (warpIdx == first) {
      m_flag = true;
      atomicExch(&md_data->mutex, 0); // unlock
    }
#else
    int len = snprintf(nullptr, 0, fmt, args...);
    if (len >= 0) {
      char* msg = new char[len+1];
      snprintf(msg, len+1, fmt, args...);
      func(udata, msg);
      delete[] msg;
    } else {
      fprintf(stderr, "RAJA logger error: could not format message");
    }
#endif
  }

private:
  struct CudaLogManagerDeviceData
  {
    int mutex = 0;
    loggingID_type count = 0;
  };

  bool  m_flag = false;
  char* m_error_begin = nullptr;
  char* m_error_pos = nullptr;
  char* m_error_end = nullptr;
  char* m_log_begin = nullptr;
  char* m_log_pos = nullptr;
  char* m_log_end = nullptr;
  CudaLogManagerDeviceData* md_data = nullptr;

  static char* s_instance_buffer;

  static void* allocateInstance()
  {
    if (s_instance_buffer == nullptr) {
      cudaHostAlloc(&s_instance_buffer, buffer_size, cudaHostAllocDefault);
      memset(s_instance_buffer, 0, buffer_size);
    }
    return s_instance_buffer;
  }

  static void deallocateInstance()
  {
    if (s_instance_buffer != nullptr) {
      getInstance()->~CudaLogManager();
      cudaFreeHost(s_instance_buffer);
    }
  }

  CudaLogManager()
    : m_flag(false),
      m_error_begin(s_instance_buffer + sizeof(CudaLogManager)),
      m_error_pos(s_instance_buffer + sizeof(CudaLogManager)),
      m_error_end(s_instance_buffer + sizeof(CudaLogManager) + error_buffer_size),
      m_log_begin(s_instance_buffer + sizeof(CudaLogManager) + error_buffer_size),
      m_log_pos(s_instance_buffer + sizeof(CudaLogManager) + error_buffer_size),
      m_log_end(s_instance_buffer + buffer_size),
      md_data(nullptr)
  {
    cudaMalloc((void**)&md_data, sizeof(CudaLogManagerDeviceData));
    cudaMemset(md_data, 0, sizeof(CudaLogManagerDeviceData));
  }

  ~CudaLogManager()
  {
    cudaFree(md_data);
  }

  template < typename T_fmt, typename... Ts >
  RAJA_HOST_DEVICE
  static bool 
  write_log(char*& buf_pos, char*const& buf_end,
            loggingID_type id, RAJA::logging_function_type func,
            int udata, T_fmt const& fmt, Ts const&... args)
  {
    // printf("RAJA logger: writing log on device.\n");
    char* init_buf_pos = buf_pos;
    bool err = write_value(buf_pos, buf_end, static_cast<char*>(nullptr));
    err = write_value(buf_pos, buf_end, id) || err;
    err = write_value(buf_pos, buf_end, func) || err;
    err = write_value(buf_pos, buf_end, udata) || err;
    err = write_types_values(buf_pos, buf_end, fmt, args...) || err;
    err = write_value(init_buf_pos, buf_end, buf_pos) || err;

    return err;
  }

  template < typename T >
  RAJA_HOST_DEVICE
  static typename std::enable_if< !std::is_arithmetic< T >::value
                                && !std::is_pointer< T >::value
                                && !(std::is_array< T >::value
                                  && std::is_same< char, 
                                                   typename std::remove_cv< 
                                                     typename std::remove_extent< T >::type
                                                   >::type
                                                 >::value),
                                  bool >::type
  write_type(char*&, char*const, T const&)
  {
    static_assert(!std::is_same<T, void>::value || std::is_same<T, void>::value, "Raja error: unknown type in logger");
    return true;
  }

  template < typename T >
  RAJA_HOST_DEVICE
  static typename std::enable_if< std::is_integral< T >::value && std::is_signed< T >::value && sizeof(T) == 1, bool>::type
  write_type(char*& pos, char*const& end, T const&)
  {
    if (pos >= end) return true;
    *pos++ = static_cast<char>(RAJA::Internal::print_types::int8);
    return false;
  }

  template < typename T >
  RAJA_HOST_DEVICE
  static typename std::enable_if< std::is_integral< T >::value && std::is_signed< T >::value && sizeof(T) == 2, bool>::type
  write_type(char*& pos, char*const& end, T const&)
  {
    if (pos >= end) return true;
    *pos++ = static_cast<char>(RAJA::Internal::print_types::int16);
    return false;
  }

  template < typename T >
  RAJA_HOST_DEVICE
  static typename std::enable_if< std::is_integral< T >::value && std::is_signed< T >::value && sizeof(T) == 4, bool>::type
  write_type(char*& pos, char*const& end, T const&)
  {
    if (pos >= end) return true;
    *pos++ = static_cast<char>(RAJA::Internal::print_types::int32);
    return false;
  }

  template < typename T >
  RAJA_HOST_DEVICE
  static typename std::enable_if< std::is_integral< T >::value && std::is_signed< T >::value && sizeof(T) == 8, bool>::type
  write_type(char*& pos, char*const& end, T const&)
  {
    if (pos >= end) return true;
    *pos++ = static_cast<char>(RAJA::Internal::print_types::int64);
    return false;
  }

  template < typename T >
  RAJA_HOST_DEVICE
  static typename std::enable_if< std::is_integral< T >::value && std::is_unsigned< T >::value && sizeof(T) == 1, bool>::type
  write_type(char*& pos, char*const& end, T const&)
  {
    if (pos >= end) return true;
    *pos++ = static_cast<char>(RAJA::Internal::print_types::uint8);
    return false;
  }

  template < typename T >
  RAJA_HOST_DEVICE
  static typename std::enable_if< std::is_integral< T >::value && std::is_unsigned< T >::value && sizeof(T) == 2, bool>::type
  write_type(char*& pos, char*const& end, T const&)
  {
    if (pos >= end) return true;
    *pos++ = static_cast<char>(RAJA::Internal::print_types::uint16);
    return false;
  }

  template < typename T >
  RAJA_HOST_DEVICE
  static typename std::enable_if< std::is_integral< T >::value && std::is_unsigned< T >::value && sizeof(T) == 4, bool>::type
  write_type(char*& pos, char*const& end, T const&)
  {
    if (pos >= end) return true;
    *pos++ = static_cast<char>(RAJA::Internal::print_types::uint32);
    return false;
  }

  template < typename T >
  RAJA_HOST_DEVICE
  static typename std::enable_if< std::is_integral< T >::value && std::is_unsigned< T >::value && sizeof(T) == 8, bool>::type
  write_type(char*& pos, char*const& end, T const&)
  {
    if (pos >= end) return true;
    *pos++ = static_cast<char>(RAJA::Internal::print_types::uint64);
    return false;
  }

  template < typename T >
  RAJA_HOST_DEVICE
  static typename std::enable_if< std::is_floating_point< T >::value && sizeof(T) == 4, bool>::type
  write_type(char*& pos, char*const& end, T const&)
  {
    if (pos >= end) return true;
    *pos++ = static_cast<char>(RAJA::Internal::print_types::flt32);
    return false;
  }

  template < typename T >
  RAJA_HOST_DEVICE
  static typename std::enable_if< std::is_floating_point< T >::value && sizeof(T) == 8, bool>::type
  write_type(char*& pos, char*const& end, T const&)
  {
    if (pos >= end) return true;
    *pos++ = static_cast<char>(RAJA::Internal::print_types::flt64);
    return false;
  }

  template < typename T >
  RAJA_HOST_DEVICE
  static typename std::enable_if< std::is_pointer< T >::value
                              && !std::is_same< char, 
                                                typename std::remove_cv< 
                                                  typename std::remove_pointer< T >::type
                                                >::type
                                              >::value,
                                  bool >::type
  write_type(char*& pos, char*const& end, T const&)
  {
    if (pos >= end) return true;
    *pos++ = static_cast<char>(RAJA::Internal::print_types::ptr);
    return false;
  }

  template < typename T >
  RAJA_HOST_DEVICE
  static typename std::enable_if< std::is_pointer< T >::value
                               && std::is_same< char, 
                                                typename std::remove_cv< 
                                                  typename std::remove_pointer< T >::type
                                                >::type
                                              >::value,
                                  bool >::type
  write_type(char*& pos, char*const& end, T const&)
  {
    if (pos >= end) return true;
    *pos++ = static_cast<char>(RAJA::Internal::print_types::char_ptr);
    return false;
  }

  template < typename T >
  RAJA_HOST_DEVICE
  static typename std::enable_if< std::is_array< T >::value
                               && std::is_same< char, 
                                                typename std::remove_cv< 
                                                  typename std::remove_extent< T >::type
                                                >::type
                                              >::value,
                                  bool >::type
  write_type(char*& pos, char*const& end, T const&)
  {
    if (pos >= end) return true;
    *pos++ = static_cast<char>(RAJA::Internal::print_types::char_arr);
    return false;
  }

  template < typename T >
  RAJA_HOST_DEVICE
  static typename std::enable_if< !std::is_array< T >::value,
                                  bool >::type
  write_value(char*& pos, char*const& end, T const& arg)
  {
    union Tchar_union {
      T t;
      char ca[sizeof(T)];
    };

    bool err = false;

    Tchar_union u;
    u.t = arg;

    for (int i = 0; i < sizeof(T); i++) {
      if (pos >= end) {
        err = true;
        break;
      }
      *pos++ = u.ca[i];
    }

    return err;
  }

  template < typename T >
  RAJA_HOST_DEVICE
  static typename std::enable_if< std::is_array< T >::value
                               && std::is_same< char, 
                                                typename std::remove_cv< 
                                                  typename std::remove_extent< T >::type
                                                >::type
                                              >::value,
                                  bool >::type
  write_value(char*& pos, char*const& end, T const& arg)
  {
    bool err = false;
    for ( int i = 0; i < std::extent<T>::value; ++i ) {
      if (pos >= end) {
        err = true;
        break;
      }
      if ('\0' == (*pos++ = arg[i])) break;
    }

    return err;
  }

  RAJA_HOST_DEVICE
  static bool
  write_types_values(char*&, char*const&)
  {
    return false;
  }

  template < typename T, typename... Ts >
  RAJA_HOST_DEVICE
  static bool
  write_types_values(char*& pos, char*const& end, T const& arg, Ts const&... args)
  {
    bool err = write_type(pos, end, arg);
    err = write_value(pos, end, arg) || err;
    err = write_types_values(pos, end, args...) || err;
    return err;
  }

  // functions that read from the buffer
  void handle_in_order() volatile;
  
};

} // closing brace for Internal namespace

template < >
class Logger< RAJA::cuda_logger > {
public:
  using func_type = RAJA::logging_function_type;

  explicit Logger(func_type f = RAJA::basic_logger)
    : m_func(f),
      m_logman(Internal::CudaLogManager::getInstance())
  {

  }

  template < typename T_fmt, typename... Ts >
  RAJA_HOST_DEVICE
  typename std::enable_if< std::is_pointer< typename std::decay< T_fmt >::type >::value
                        && std::is_same< char, typename std::remove_cv< 
                                                 typename std::remove_pointer< 
                                                   typename std::decay< T_fmt
                                                   >::type
                                                 >::type
                                               >::type
                           >::value
                         >::type
  error(int udata, T_fmt const& fmt, Ts const&... args) const
  {
    m_logman->error_impl(m_func, udata, fmt, args...);
  }

  template < typename T_fmt, typename... Ts >
  RAJA_HOST_DEVICE
  typename std::enable_if< std::is_pointer< typename std::decay< T_fmt >::type >::value
                        && std::is_same< char, typename std::remove_cv< 
                                                 typename std::remove_pointer< 
                                                   typename std::decay< T_fmt
                                                   >::type
                                                 >::type
                                               >::type
                           >::value
                         >::type
  log(int udata, T_fmt const& fmt, Ts const&... args) const
  {
    m_logman->log_impl(m_func, udata, fmt, args...);
  }

private:
  const func_type m_func;
  volatile Internal::CudaLogManager* const m_logman;
};

}  // closing brace for RAJA namespace

#endif

#endif  // closing endif for header file include guard
