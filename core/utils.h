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

#ifndef __SWR_UTILS_H__
#define __SWR_UTILS_H__

#include "os.h"
#include "simdintrin.h"

#define ALIGN_UP(addr, size) (((addr) + ((size)-1)) & (~((size)-1)))

#if defined(_WIN32)
void SaveBitmapToFile(
    const WCHAR *pFilename,
    void *pBuffer,
    UINT width,
    UINT height);

void OpenBitmapFromFile(
    const WCHAR *pFilename,
    void **pBuffer,
    UINT *width,
    UINT *height);
#endif

// @todo assume linux is always 64 bit
#if defined(_WIN64) || defined(__linux__) || defined(__gnu_linux__)
#define _MM_INSERT_EPI64 _mm_insert_epi64
#define _MM_EXTRACT_EPI64 _mm_extract_epi64
#else
INLINE INT64 _MM_EXTRACT_EPI64(__m128i a, const int ndx)
{
    OSALIGNLINE(UINT) elems[4];
    _mm_store_si128((__m128i *)elems, a);
    if (ndx == 0)
    {
        UINT64 foo = elems[0];
        foo |= (UINT64)elems[1] << 32;
        return foo;
    }
    else
    {
        UINT64 foo = elems[2];
        foo |= (UINT64)elems[3] << 32;
        return foo;
    }
}

INLINE __m128i _MM_INSERT_EPI64(__m128i a, INT64 b, const int ndx)
{
    OSALIGNLINE(INT64) elems[2];
    _mm_store_si128((__m128i *)elems, a);
    if (ndx == 0)
    {
        elems[0] = b;
    }
    else
    {
        elems[1] = b;
    }
    __m128i out;
    out = _mm_load_si128((const __m128i *)elems);
    return out;
}
#endif

typedef UINT DRAW_T;

OSALIGNLINE(struct) BBOX
{
    int top, bottom, left, right;

    BBOX()
    {
    }
    BBOX(int t, int b, int l, int r)
        : top(t), bottom(b), left(l), right(r)
    {
    }

    bool operator==(const BBOX &rhs)
    {
        return (this->top == rhs.top &&
                this->bottom == rhs.bottom &&
                this->left == rhs.left &&
                this->right == rhs.right);
    }

    bool operator!=(const BBOX &rhs)
    {
        return !(*this == rhs);
    }
};

struct simdBBox
{
    simdscalari top, bottom, left, right;
};

INLINE
void vTranspose(__m128 &row0, __m128 &row1, __m128 &row2, __m128 &row3)
{
    __m128i row0i = _mm_castps_si128(row0);
    __m128i row1i = _mm_castps_si128(row1);
    __m128i row2i = _mm_castps_si128(row2);
    __m128i row3i = _mm_castps_si128(row3);

    __m128i vTemp = row2i;
    row2i = _mm_unpacklo_epi32(row2i, row3i);
    vTemp = _mm_unpackhi_epi32(vTemp, row3i);

    row3i = row0i;
    row0i = _mm_unpacklo_epi32(row0i, row1i);
    row3i = _mm_unpackhi_epi32(row3i, row1i);

    row1i = row0i;
    row0i = _mm_unpacklo_epi64(row0i, row2i);
    row1i = _mm_unpackhi_epi64(row1i, row2i);

    row2i = row3i;
    row2i = _mm_unpacklo_epi64(row2i, vTemp);
    row3i = _mm_unpackhi_epi64(row3i, vTemp);

    row0 = _mm_castsi128_ps(row0i);
    row1 = _mm_castsi128_ps(row1i);
    row2 = _mm_castsi128_ps(row2i);
    row3 = _mm_castsi128_ps(row3i);
}

INLINE
void vTranspose(__m128i &row0, __m128i &row1, __m128i &row2, __m128i &row3)
{
    __m128i vTemp = row2;
    row2 = _mm_unpacklo_epi32(row2, row3);
    vTemp = _mm_unpackhi_epi32(vTemp, row3);

    row3 = row0;
    row0 = _mm_unpacklo_epi32(row0, row1);
    row3 = _mm_unpackhi_epi32(row3, row1);

    row1 = row0;
    row0 = _mm_unpacklo_epi64(row0, row2);
    row1 = _mm_unpackhi_epi64(row1, row2);

    row2 = row3;
    row2 = _mm_unpacklo_epi64(row2, vTemp);
    row3 = _mm_unpackhi_epi64(row3, vTemp);
}

#define GCC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)

#if defined(__GNUC__) && (GCC_VERSION < 40900)
#define _mm_undefined_ps _mm_setzero_ps
#if KNOB_VS_SIMD_WIDTH == 8
#define _mm256_undefined_ps _mm256_setzero_ps
#endif
#endif

#if KNOB_VS_SIMD_WIDTH == 8
INLINE
void vTranspose3x8(__m128 (&vDst)[8], __m256 &vSrc0, __m256 &vSrc1, __m256 &vSrc2)
{
    __m256 r0r2 = _mm256_unpacklo_ps(vSrc0, vSrc2);                 //x0z0x1z1 x4z4x5z5
    __m256 r1rx = _mm256_unpacklo_ps(vSrc1, _mm256_undefined_ps()); //y0w0y1w1 y4w4y5w5
    __m256 r02r1xlolo = _mm256_unpacklo_ps(r0r2, r1rx);             //x0y0z0w0 x4y4z4w4
    __m256 r02r1xlohi = _mm256_unpackhi_ps(r0r2, r1rx);             //x1y1z1w1 x5y5z5w5

    r0r2 = _mm256_unpackhi_ps(vSrc0, vSrc2);                 //x2z2x3z3 x6z6x7z7
    r1rx = _mm256_unpackhi_ps(vSrc1, _mm256_undefined_ps()); //y2w2y3w3 y6w6yw77
    __m256 r02r1xhilo = _mm256_unpacklo_ps(r0r2, r1rx);      //x2y2z2w2 x6y6z6w6
    __m256 r02r1xhihi = _mm256_unpackhi_ps(r0r2, r1rx);      //x3y3z3w3 x7y7z7w7

    vDst[0] = _mm256_castps256_ps128(r02r1xlolo);
    vDst[1] = _mm256_castps256_ps128(r02r1xlohi);
    vDst[2] = _mm256_castps256_ps128(r02r1xhilo);
    vDst[3] = _mm256_castps256_ps128(r02r1xhihi);

    vDst[4] = _mm256_extractf128_ps(r02r1xlolo, 1);
    vDst[5] = _mm256_extractf128_ps(r02r1xlohi, 1);
    vDst[6] = _mm256_extractf128_ps(r02r1xhilo, 1);
    vDst[7] = _mm256_extractf128_ps(r02r1xhihi, 1);
}

INLINE
void vTranspose4x8(__m128 (&vDst)[8], __m256 &vSrc0, __m256 &vSrc1, __m256 &vSrc2, __m256 &vSrc3)
{
    __m256 r0r2 = _mm256_unpacklo_ps(vSrc0, vSrc2);     //x0z0x1z1 x4z4x5z5
    __m256 r1rx = _mm256_unpacklo_ps(vSrc1, vSrc3);     //y0w0y1w1 y4w4y5w5
    __m256 r02r1xlolo = _mm256_unpacklo_ps(r0r2, r1rx); //x0y0z0w0 x4y4z4w4
    __m256 r02r1xlohi = _mm256_unpackhi_ps(r0r2, r1rx); //x1y1z1w1 x5y5z5w5

    r0r2 = _mm256_unpackhi_ps(vSrc0, vSrc2);            //x2z2x3z3 x6z6x7z7
    r1rx = _mm256_unpackhi_ps(vSrc1, vSrc3);            //y2w2y3w3 y6w6yw77
    __m256 r02r1xhilo = _mm256_unpacklo_ps(r0r2, r1rx); //x2y2z2w2 x6y6z6w6
    __m256 r02r1xhihi = _mm256_unpackhi_ps(r0r2, r1rx); //x3y3z3w3 x7y7z7w7

    vDst[0] = _mm256_castps256_ps128(r02r1xlolo);
    vDst[1] = _mm256_castps256_ps128(r02r1xlohi);
    vDst[2] = _mm256_castps256_ps128(r02r1xhilo);
    vDst[3] = _mm256_castps256_ps128(r02r1xhihi);

    vDst[4] = _mm256_extractf128_ps(r02r1xlolo, 1);
    vDst[5] = _mm256_extractf128_ps(r02r1xlohi, 1);
    vDst[6] = _mm256_extractf128_ps(r02r1xhilo, 1);
    vDst[7] = _mm256_extractf128_ps(r02r1xhihi, 1);
}

INLINE
void vTranspose8x8(__m256 (&vDst)[8], const __m256 &vMask0, const __m256 &vMask1, const __m256 &vMask2, const __m256 &vMask3, const __m256 &vMask4, const __m256 &vMask5, const __m256 &vMask6, const __m256 &vMask7)
{
    __m256 __t0 = _mm256_unpacklo_ps(vMask0, vMask1);
    __m256 __t1 = _mm256_unpackhi_ps(vMask0, vMask1);
    __m256 __t2 = _mm256_unpacklo_ps(vMask2, vMask3);
    __m256 __t3 = _mm256_unpackhi_ps(vMask2, vMask3);
    __m256 __t4 = _mm256_unpacklo_ps(vMask4, vMask5);
    __m256 __t5 = _mm256_unpackhi_ps(vMask4, vMask5);
    __m256 __t6 = _mm256_unpacklo_ps(vMask6, vMask7);
    __m256 __t7 = _mm256_unpackhi_ps(vMask6, vMask7);
    __m256 __tt0 = _mm256_shuffle_ps(__t0, __t2, _MM_SHUFFLE(1, 0, 1, 0));
    __m256 __tt1 = _mm256_shuffle_ps(__t0, __t2, _MM_SHUFFLE(3, 2, 3, 2));
    __m256 __tt2 = _mm256_shuffle_ps(__t1, __t3, _MM_SHUFFLE(1, 0, 1, 0));
    __m256 __tt3 = _mm256_shuffle_ps(__t1, __t3, _MM_SHUFFLE(3, 2, 3, 2));
    __m256 __tt4 = _mm256_shuffle_ps(__t4, __t6, _MM_SHUFFLE(1, 0, 1, 0));
    __m256 __tt5 = _mm256_shuffle_ps(__t4, __t6, _MM_SHUFFLE(3, 2, 3, 2));
    __m256 __tt6 = _mm256_shuffle_ps(__t5, __t7, _MM_SHUFFLE(1, 0, 1, 0));
    __m256 __tt7 = _mm256_shuffle_ps(__t5, __t7, _MM_SHUFFLE(3, 2, 3, 2));
    vDst[0] = _mm256_permute2f128_ps(__tt0, __tt4, 0x20);
    vDst[1] = _mm256_permute2f128_ps(__tt1, __tt5, 0x20);
    vDst[2] = _mm256_permute2f128_ps(__tt2, __tt6, 0x20);
    vDst[3] = _mm256_permute2f128_ps(__tt3, __tt7, 0x20);
    vDst[4] = _mm256_permute2f128_ps(__tt0, __tt4, 0x31);
    vDst[5] = _mm256_permute2f128_ps(__tt1, __tt5, 0x31);
    vDst[6] = _mm256_permute2f128_ps(__tt2, __tt6, 0x31);
    vDst[7] = _mm256_permute2f128_ps(__tt3, __tt7, 0x31);
}

INLINE
void vTranspose8x8(__m256 (&vDst)[8], const __m256i &vMask0, const __m256i &vMask1, const __m256i &vMask2, const __m256i &vMask3, const __m256i &vMask4, const __m256i &vMask5, const __m256i &vMask6, const __m256i &vMask7)
{
    vTranspose8x8(vDst, _mm256_castsi256_ps(vMask0), _mm256_castsi256_ps(vMask1), _mm256_castsi256_ps(vMask2), _mm256_castsi256_ps(vMask3),
                  _mm256_castsi256_ps(vMask4), _mm256_castsi256_ps(vMask5), _mm256_castsi256_ps(vMask6), _mm256_castsi256_ps(vMask7));
}
#endif

#endif //__SWR_UTILS_H__
