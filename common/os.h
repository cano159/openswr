// Copyright 2014 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef __SWR_OS_H__
#define __SWR_OS_H__

#include "knobs.h"

#if (defined(FORCE_WINDOWS) || defined(_WIN32)) && !defined(FORCE_LINUX)

#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX
#include "Windows.h"
#include <intrin.h>

#define OSALIGN(RWORD, WIDTH) __declspec(align(WIDTH)) RWORD
#define THREAD __declspec(thread)
#define INLINE __forceinline

#elif defined(FORCE_LINUX) || defined(__linux__) || defined(__gnu_linux__)

#include <cstdint>
#include <stdlib.h>
#include <x86intrin.h>

typedef void VOID;
typedef void *LPVOID;
typedef uint8_t BOOL;
typedef wchar_t WCHAR;
typedef int INT;
typedef int INT32;
typedef unsigned int UINT;
typedef uint32_t UINT32;
typedef uint64_t UINT64;
typedef int64_t INT64;
typedef void *HANDLE;
typedef float FLOAT;
typedef int LONG;
typedef uint8_t BYTE;
typedef unsigned char UCHAR;
typedef unsigned int DWORD;

#define OSALIGN(RWORD, WIDTH) RWORD __attribute__((aligned(WIDTH)))
#define THREAD __thread
#define INLINE __inline

#define GCC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)

#if (__GNUC__) && (GCC_VERSION < 40500) && (!defined(__clang__))
inline uint64_t __rdtsc()
{
    long low, high;
    asm volatile("rdtsc"
                 : "=a"(low), "=d"(high));
    return (low | ((uint64_t)high << 32));
}
#endif

inline unsigned char _BitScanForward(unsigned int *Index, unsigned int Mask)
{
    *Index = __builtin_ctz(Mask);
    return (Mask != 0);
}

inline void *_aligned_malloc(unsigned int size, unsigned int alignment)
{
    void *ret;
    if (posix_memalign(&ret, alignment, size))
    {
        return NULL;
    }
    return ret;
}

#define _aligned_free free
#define InterlockedCompareExchange(Dest, Exchange, Comparand) __sync_val_compare_and_swap(Dest, Comparand, Exchange)
#define InterlockedExchangeAdd(Addend, Value) __sync_fetch_and_add(Addend, Value)
#define _ReadWriteBarrier() asm volatile("" :: \
                                             : "memory")
#define __stdcall

#else

#error Unsupported OS/system.

#endif

#define OSALIGNLINE(RWORD) OSALIGN(RWORD, 64)
#if KNOB_VS_SIMD_WIDTH == 4
#define OSALIGNSIMD(RWORD) OSALIGN(RWORD, 16)
#elif KNOB_VS_SIMD_WIDTH == 8
#define OSALIGNSIMD(RWORD) OSALIGN(RWORD, 32)
#else
#error Unknown SIMD width!
#endif

#endif //__SWR_OS_H__
