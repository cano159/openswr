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

#ifndef __SWR_SIMDINTRIN_H__
#define __SWR_SIMDINTRIN_H__

#include "os.h"

#include <cassert>

#include <emmintrin.h>
#include <immintrin.h>
#include <xmmintrin.h>

#if (KNOB_VS_SIMD_WIDTH == 4)
#define simdscalar __m128
#define simdscalari __m128i
#else
#define simdscalar __m256
#define simdscalari __m256i
#endif

// simd vector
OSALIGNSIMD(union) simdvector
{
    simdscalar v[4];
    struct
    {
        simdscalar x, y, z, w;
    };

    simdscalar &operator[](const int i)
    {
        return v[i];
    }
    const simdscalar &operator[](const int i) const
    {
        return v[i];
    }
};

#if (KNOB_VS_SIMD_WIDTH == 4)

#define _simd_crcint unsigned long
#define _simd_crc32 _mm_crc32_u32

#define _simd_setzero_ps _mm_setzero_ps
#define _simd_set1_ps _mm_set1_ps
#define _simd_load1_ps _mm_load1_ps
#define _simd_loadu_ps _mm_loadu_ps
#define _simd_mul_ps _mm_mul_ps
#define _simd_add_ps _mm_add_ps
#define _simd_rsqrt_ps _mm_rsqrt_ps
#define _simd_max_ps _mm_max_ps
#define _simd_min_ps _mm_min_ps
#define _simd_movemask_ps _mm_movemask_ps
#define _simd_cmplt_ps _mm_cmplt_ps
#define _simd_cmpgt_ps _mm_cmpgt_ps
#define _simd_cmpneq_ps _mm_cmpneq_ps
#define _simd_cmpeq_ps _mm_cmpeq_ps
#define _simd_cmpge_ps _mm_cmpge_ps
#define _simd_cmple_ps _mm_cmple_ps
#define _simd_rcp_ps _mm_rcp_ps
#define _simd_div_ps _mm_div_ps
#define _simd_sub_ps _mm_sub_ps
#define _simd_cvtepi32_ps _mm_cvtepi32_ps
#define _simd_blend_ps _mm_blend_ps
#define _simd_store_ps _mm_store_ps
#define _simd_load_ps _mm_load_ps
#define _simd_shuffle_ps _mm_shuffle_ps
#define _simd_srlisi_ps(vec, imm) _mm_castsi128_ps(_mm_srli_si128(_mm_castps_si128(vec), imm))
#define _simd_blendv_ps _mm_blendv_ps
#define _simd_and_ps _mm_and_ps
#define _simd_or_ps _mm_or_ps
#define _simd_andnot_ps _mm_andnot_ps
#define _simd_round_ps _mm_round_ps

#define _simd_cvtps_epi32 _mm_cvtps_epi32
#define _simd_sub_epi32 _mm_sub_epi32
#define _simd_shuffle_epi32 _mm_shuffle_epi32
#define _simd_mul_epi32 _mm_mul_epi32
#define _simd_mullo_epi32 _mm_mullo_epi32
#define _simd_sub_epi64 _mm_sub_epi64
#define _simd_shuffleps_epi32(vA, vB, imm) _mm_castps_si128(_mm_shuffle_ps(_mm_castsi128_ps(vA), _mm_castsi128_ps(vB), imm))
#define _simd_srli_si _mm_srli_si128
#define _simd_slli_epi32 _mm_slli_epi32
#define _simd_min_epi32 _mm_min_epi32
#define _simd_max_epi32 _mm_max_epi32
#define _simd_setzero_si _mm_setzero_si128
#define _simd_set1_epi32 _mm_set1_epi32
#define _simd_store_si _mm_store_si128
#define _simd_cvttps_epi32 _mm_cvttps_epi32
#define _simd_srai_epi32 _mm_srai_epi32
#define _simd_add_epi32 _mm_add_epi32
#define _simd_and_si _mm_and_si128
#define _simd_or_si _mm_or_si128
#define _simd_cmpeq_epi32 _mm_cmpeq_epi32
#define _simd_cmplt_epi32 _mm_cmplt_epi32
#define _simd_unpacklo_epi32 _mm_unpacklo_epi32
#define _simd_unpackhi_epi32 _mm_unpackhi_epi32
#define _simd_castsi_ps _mm_castsi128_ps
#define _simd_castps_si _mm_castps_si128

#define _simd128_fmadd_ps _mm_fmaddemu_ps
#define _simd_fmadd_ps _mm_fmaddemu_ps
#define _simd_broadcast_ss _mm_load1_ps
#define _simd_maskstore_ps _simd128_maskstore_ps
#define _simd_shuffle_epi8 _mm_shuffle_epi8
#define _simd_load_si _mm_load_si128

INLINE
__m128 _mm_fmaddemu_ps(__m128 a, __m128 b, __m128 c)
{
    __m128 res = _mm_mul_ps(a, b);
    res = _mm_add_ps(res, c);
    return res;
}

INLINE
void _simd_mov(simdscalar &r, unsigned int rlane, simdscalar &s, unsigned int slane)
{
    OSALIGNSIMD(float)rArray[KNOB_VS_SIMD_WIDTH], sArray[KNOB_VS_SIMD_WIDTH];
    _mm_store_ps(rArray, r);
    _mm_store_ps(sArray, s);
    rArray[rlane] = sArray[slane];
    r = _mm_load_ps(rArray);
}

INLINE
void _simdvec_transpose(simdvector &v)
{
    __m128i row0i = _mm_castps_si128(v[0]);
    __m128i row1i = _mm_castps_si128(v[1]);
    __m128i row2i = _mm_castps_si128(v[2]);
    __m128i row3i = _mm_castps_si128(v[3]);

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

    v[0] = _mm_castsi128_ps(row0i);
    v[1] = _mm_castsi128_ps(row1i);
    v[2] = _mm_castsi128_ps(row2i);
    v[3] = _mm_castsi128_ps(row3i);
}

INLINE
void _simd128_maskstore_ps(float *mem_addr, __m128i mask, __m128 a)
{
    __m128 val = _mm_load_ps(mem_addr);
    __m128 result = _mm_blendv_ps(val, a, _mm_castsi128_ps(mask));
    _mm_store_ps(mem_addr, result);
}

#else
// (KNOB_VS_SIMD_WIDTH == 8)

#define _simd_crcint unsigned long long
#define _simd_crc32 _mm_crc32_u64

#define _simd128_maskstore_ps _mm_maskstore_ps
#define _simd_load_ps _mm256_load_ps
#define _simd_load1_ps _mm256_broadcast_ss
#define _simd_loadu_ps _mm256_loadu_ps
#define _simd_setzero_ps _mm256_setzero_ps
#define _simd_set1_ps _mm256_set1_ps
#define _simd_blend_ps _mm256_blend_ps
#define _simd_blendv_ps _mm256_blendv_ps
#define _simd_store_ps _mm256_store_ps
#define _simd_mul_ps _mm256_mul_ps
#define _simd_add_ps _mm256_add_ps
#define _simd_sub_ps _mm256_sub_ps
#define _simd_rsqrt_ps _mm256_rsqrt_ps
#define _simd_min_ps _mm256_min_ps
#define _simd_max_ps _mm256_max_ps
#define _simd_movemask_ps _mm256_movemask_ps
#define _simd_cvtps_epi32 _mm256_cvtps_epi32
#define _simd_cvtepi32_ps _mm256_cvtepi32_ps
#define _simd_cmplt_ps(a, b) _mm256_cmp_ps(a, b, _CMP_LT_OQ)
#define _simd_cmpgt_ps(a, b) _mm256_cmp_ps(a, b, _CMP_GT_OQ)
#define _simd_cmpneq_ps(a, b) _mm256_cmp_ps(a, b, _CMP_NEQ_OQ)
#define _simd_cmpeq_ps(a, b) _mm256_cmp_ps(a, b, _CMP_EQ_OQ)
#define _simd_cmpge_ps(a, b) _mm256_cmp_ps(a, b, _CMP_GE_OQ)
#define _simd_cmple_ps(a, b) _mm256_cmp_ps(a, b, _CMP_LE_OQ)
#define _simd_and_ps _mm256_and_ps
#define _simd_or_ps _mm256_or_ps

#define _simd_rcp_ps _mm256_rcp_ps
#define _simd_div_ps _mm256_div_ps
#define _simd_castsi_ps _mm256_castsi256_ps
#define _simd_andnot_ps _mm256_andnot_ps
#define _simd_round_ps _mm256_round_ps

// emulated integer simd
#define SIMD_EMU_EPI(func, intrin)                          \
    INLINE __m256i func(__m256i a, __m256i b)               \
    {                                                       \
        __m128i aHi = _mm256_extractf128_si256(a, 1);       \
        __m128i bHi = _mm256_extractf128_si256(b, 1);       \
        __m128i aLo = _mm256_castsi256_si128(a);            \
        __m128i bLo = _mm256_castsi256_si128(b);            \
                                                            \
        __m128i subLo = intrin(aLo, bLo);                   \
        __m128i subHi = intrin(aHi, bHi);                   \
                                                            \
        __m256i result = _mm256_castsi128_si256(subLo);     \
        result = _mm256_insertf128_si256(result, subHi, 1); \
                                                            \
        return result;                                      \
    }

#if (KNOB_ARCH == KNOB_ARCH_AVX)
#define _simd_mul_epi32 _simdemu_mul_epi32
#define _simd_mullo_epi32 _simdemu_mullo_epi32
#define _simd_sub_epi32 _simdemu_sub_epi32
#define _simd_sub_epi64 _simdemu_sub_epi64
#define _simd_min_epi32 _simdemu_min_epi32
#define _simd_max_epi32 _simdemu_max_epi32
#define _simd_add_epi32 _simdemu_add_epi32
#define _simd_and_si _simdemu_and_si
#define _simd_cmpeq_epi32 _simdemu_cmpeq_epi32
#define _simd_cmplt_epi32 _simdemu_cmplt_epi32
#define _simd_or_si _simdemu_or_si
#define _simd_castps_si _mm256_castps_si256

SIMD_EMU_EPI(_simdemu_mul_epi32, _mm_mul_epi32)
SIMD_EMU_EPI(_simdemu_mullo_epi32, _mm_mullo_epi32)
SIMD_EMU_EPI(_simdemu_sub_epi32, _mm_sub_epi32)
SIMD_EMU_EPI(_simdemu_sub_epi64, _mm_sub_epi64)
SIMD_EMU_EPI(_simdemu_min_epi32, _mm_min_epi32)
SIMD_EMU_EPI(_simdemu_max_epi32, _mm_max_epi32)
SIMD_EMU_EPI(_simdemu_add_epi32, _mm_add_epi32)
SIMD_EMU_EPI(_simdemu_and_si, _mm_and_si128)
SIMD_EMU_EPI(_simdemu_cmpeq_epi32, _mm_cmpeq_epi32)
SIMD_EMU_EPI(_simdemu_cmplt_epi32, _mm_cmplt_epi32)
SIMD_EMU_EPI(_simdemu_or_si, _mm_or_si128)

#define _simd_unpacklo_epi32(a, b) _mm256_castps_si256(_mm256_unpacklo_ps(_mm256_castsi256_ps(a), _mm256_castsi256_ps(b)))
#define _simd_unpackhi_epi32(a, b) _mm256_castps_si256(_mm256_unpackhi_ps(_mm256_castsi256_ps(a), _mm256_castsi256_ps(b)))

#define _simd_srli_si(a, i) _simdemu_srli_si128<i>(a)
#define _simd_slli_epi32(a, i) _simdemu_slli_epi32<i>(a)
#define _simd_srai_epi32(a, i) _simdemu_srai_epi32<i>(a)
#define _simd_srlisi_ps(a, i) _mm256_castsi256_ps(_simdemu_srli_si128<i>(_mm256_castps_si256(a)))

#define _simd128_fmadd_ps _mm_fmaddemu_ps
#define _simd_fmadd_ps _mm_fmaddemu256_ps
#define _simd_shuffle_epi8 _simdemu_shuffle_epi8
SIMD_EMU_EPI(_simdemu_shuffle_epi8, _mm_shuffle_epi8)

INLINE
__m128 _mm_fmaddemu_ps(__m128 a, __m128 b, __m128 c)
{
    __m128 res = _mm_mul_ps(a, b);
    res = _mm_add_ps(res, c);
    return res;
}

INLINE
__m256 _mm_fmaddemu256_ps(__m256 a, __m256 b, __m256 c)
{
    __m256 res = _mm256_mul_ps(a, b);
    res = _mm256_add_ps(res, c);
    return res;
}
#else

#define _simd_mul_epi32 _mm256_mul_epi32
#define _simd_mullo_epi32 _mm256_mullo_epi32
#define _simd_sub_epi32 _mm256_sub_epi32
#define _simd_sub_epi64 _mm256_sub_epi64
#define _simd_min_epi32 _mm256_min_epi32
#define _simd_max_epi32 _mm256_max_epi32
#define _simd_add_epi32 _mm256_add_epi32
#define _simd_and_si _mm256_and_si256
#define _simd_cmpeq_epi32 _mm256_cmpeq_epi32
#define _simd_cmplt_epi32(a, b) _mm256_cmpgt_epi32(b, a)
#define _simd_or_si _mm256_or_si256
#define _simd_castps_si _mm256_castps_si256

#define _simd_unpacklo_epi32 _mm256_unpacklo_epi32
#define _simd_unpackhi_epi32 _mm256_unpackhi_epi32

#define _simd_srli_si(a, i) _simdemu_srli_si128<i>(a)
#define _simd_slli_epi32 _mm256_slli_epi32
#define _simd_srai_epi32 _mm256_srai_epi32
#define _simd_srlisi_ps(a, i) _mm256_castsi256_ps(_simdemu_srli_si128<i>(_mm256_castps_si256(a)))
#define _simd128_fmadd_ps _mm_fmadd_ps
#define _simd_fmadd_ps _mm256_fmadd_ps
#define _simd_shuffle_epi8 _mm256_shuffle_epi8

#endif

#define _simd_shuffleps_epi32(vA, vB, imm) _mm256_castps_si256(_mm256_shuffle_ps(_mm256_castsi256_ps(vA), _mm256_castsi256_ps(vB), imm))
#define _simd_shuffle_ps _mm256_shuffle_ps
#define _simd_set1_epi32 _mm256_set1_epi32
#define _simd_setzero_si _mm256_setzero_si256
#define _simd_cvttps_epi32 _mm256_cvttps_epi32
#define _simd_store_si _mm256_store_si256
#define _simd_broadcast_ss _mm256_broadcast_ss
#define _simd_maskstore_ps _mm256_maskstore_ps
#define _simd_load_si _mm256_load_si256

INLINE
void _simd_mov(simdscalar &r, unsigned int rlane, simdscalar &s, unsigned int slane)
{
    OSALIGNSIMD(float)rArray[KNOB_VS_SIMD_WIDTH], sArray[KNOB_VS_SIMD_WIDTH];
    _mm256_store_ps(rArray, r);
    _mm256_store_ps(sArray, s);
    rArray[rlane] = sArray[slane];
    r = _mm256_load_ps(rArray);
}

template <int i>
__m256i _simdemu_srli_si128(__m256i a)
{
    __m128i aHi = _mm256_extractf128_si256(a, 1);
    __m128i aLo = _mm256_castsi256_si128(a);

    __m128i resHi = _mm_srli_si128(aHi, i);
    __m128i resLo = _mm_alignr_epi8(aHi, aLo, i);

    __m256i result = _mm256_castsi128_si256(resLo);
    result = _mm256_insertf128_si256(result, resHi, 1);

    return result;
}

template <int i>
__m256i _simdemu_slli_epi32(__m256i a)
{
    __m128i aHi = _mm256_extractf128_si256(a, 1);
    __m128i aLo = _mm256_castsi256_si128(a);

    __m128i resHi = _mm_slli_epi32(aHi, i);
    __m128i resLo = _mm_slli_epi32(aLo, i);

    __m256i result = _mm256_castsi128_si256(resLo);
    result = _mm256_insertf128_si256(result, resHi, 1);

    return result;
}

template <int i>
__m256i _simdemu_srai_epi32(__m256i a)
{
    __m128i aHi = _mm256_extractf128_si256(a, 1);
    __m128i aLo = _mm256_castsi256_si128(a);

    __m128i resHi = _mm_srai_epi32(aHi, i);
    __m128i resLo = _mm_srai_epi32(aLo, i);

    __m256i result = _mm256_castsi128_si256(resLo);
    result = _mm256_insertf128_si256(result, resHi, 1);

    return result;
}
INLINE
void _simdvec_transpose(simdvector &v)
{
    assert(0); // need to implement 8 wide version
}

#endif

// Populates a simdvector from a vector. So p = xyzw becomes xxxx yyyy zzzz wwww.
INLINE
void _simdvec_load_ps(simdvector &r, const float *p)
{
    r[0] = _simd_set1_ps(p[0]);
    r[1] = _simd_set1_ps(p[1]);
    r[2] = _simd_set1_ps(p[2]);
    r[3] = _simd_set1_ps(p[3]);
}

INLINE
void _simdvec_mov(simdvector &r, const simdscalar &s)
{
    r[0] = s;
    r[1] = s;
    r[2] = s;
    r[3] = s;
}

INLINE
void _simdvec_mov(simdvector &r, const simdvector &v)
{
    r[0] = v[0];
    r[1] = v[1];
    r[2] = v[2];
    r[3] = v[3];
}

// just move a lane from the source simdvector to dest simdvector
INLINE
void _simdvec_mov(simdvector &r, unsigned int rlane, simdvector &s, unsigned int slane)
{
    _simd_mov(r[0], rlane, s[0], slane);
    _simd_mov(r[1], rlane, s[1], slane);
    _simd_mov(r[2], rlane, s[2], slane);
    _simd_mov(r[3], rlane, s[3], slane);
}

INLINE
void _simdvec_dp3_ps(simdscalar &r, const simdvector &v0, const simdvector &v1)
{
    simdscalar tmp;
    r = _simd_mul_ps(v0[0], v1[0]); // (v0.x*v1.x)

    tmp = _simd_mul_ps(v0[1], v1[1]); // (v0.y*v1.y)
    r = _simd_add_ps(r, tmp);         // (v0.x*v1.x) + (v0.y*v1.y)

    tmp = _simd_mul_ps(v0[2], v1[2]); // (v0.z*v1.z)
    r = _simd_add_ps(r, tmp);         // (v0.x*v1.x) + (v0.y*v1.y) + (v0.z*v1.z)
}

INLINE
void _simdvec_dp4_ps(simdscalar &r, const simdvector &v0, const simdvector &v1)
{
    simdscalar tmp;
    r = _simd_mul_ps(v0[0], v1[0]); // (v0.x*v1.x)

    tmp = _simd_mul_ps(v0[1], v1[1]); // (v0.y*v1.y)
    r = _simd_add_ps(r, tmp);         // (v0.x*v1.x) + (v0.y*v1.y)

    tmp = _simd_mul_ps(v0[2], v1[2]); // (v0.z*v1.z)
    r = _simd_add_ps(r, tmp);         // (v0.x*v1.x) + (v0.y*v1.y) + (v0.z*v1.z)

    tmp = _simd_mul_ps(v0[3], v1[3]); // (v0.w*v1.w)
    r = _simd_add_ps(r, tmp);         // (v0.x*v1.x) + (v0.y*v1.y) + (v0.z*v1.z)
}

INLINE
simdscalar _simdvec_rcp_length_ps(const simdvector &v)
{
    simdscalar length;
    _simdvec_dp4_ps(length, v, v);
    return _simd_rsqrt_ps(length);
}

INLINE
void _simdvec_normalize_ps(simdvector &r, const simdvector &v)
{
    simdscalar vecLength;
    vecLength = _simdvec_rcp_length_ps(v);

    r[0] = _simd_mul_ps(v[0], vecLength);
    r[1] = _simd_mul_ps(v[1], vecLength);
    r[2] = _simd_mul_ps(v[2], vecLength);
    r[3] = _simd_mul_ps(v[3], vecLength);
}

INLINE
void _simdvec_mul_ps(simdvector &r, const simdvector &v, const simdscalar &s)
{
    r[0] = _simd_mul_ps(v[0], s);
    r[1] = _simd_mul_ps(v[1], s);
    r[2] = _simd_mul_ps(v[2], s);
    r[3] = _simd_mul_ps(v[3], s);
}

INLINE
void _simdvec_mul_ps(simdvector &r, const simdvector &v0, const simdvector &v1)
{
    r[0] = _simd_mul_ps(v0[0], v1[0]);
    r[1] = _simd_mul_ps(v0[1], v1[1]);
    r[2] = _simd_mul_ps(v0[2], v1[2]);
    r[3] = _simd_mul_ps(v0[3], v1[3]);
}

INLINE
void _simdvec_add_ps(simdvector &r, const simdvector &v0, const simdvector &v1)
{
    r[0] = _simd_add_ps(v0[0], v1[0]);
    r[1] = _simd_add_ps(v0[1], v1[1]);
    r[2] = _simd_add_ps(v0[2], v1[2]);
    r[3] = _simd_add_ps(v0[3], v1[3]);
}

INLINE
void _simdvec_min_ps(simdvector &r, const simdvector &v0, const simdscalar &s)
{
    r[0] = _simd_min_ps(v0[0], s);
    r[1] = _simd_min_ps(v0[1], s);
    r[2] = _simd_min_ps(v0[2], s);
    r[3] = _simd_min_ps(v0[3], s);
}

INLINE
void _simdvec_max_ps(simdvector &r, const simdvector &v0, const simdscalar &s)
{
    r[0] = _simd_max_ps(v0[0], s);
    r[1] = _simd_max_ps(v0[1], s);
    r[2] = _simd_max_ps(v0[2], s);
    r[3] = _simd_max_ps(v0[3], s);
}

// Matrix4x4 * Vector4
//   outVec.x = (m00 * v.x) + (m01 * v.y) + (m02 * v.z) + (m03 * v.w)
//   outVec.y = (m10 * v.x) + (m11 * v.y) + (m12 * v.z) + (m13 * v.w)
//   outVec.z = (m20 * v.x) + (m21 * v.y) + (m22 * v.z) + (m23 * v.w)
//   outVec.w = (m30 * v.x) + (m31 * v.y) + (m32 * v.z) + (m33 * v.w)
INLINE
void _simd_mat4x4_vec4_multiply(
    simdvector &result,
    const float *pMatrix,
    const simdvector &v)
{
    simdscalar m;
    simdscalar r0;
    simdscalar r1;

    m = _simd_load1_ps(pMatrix + 0 * 4 + 0); // m[row][0]
    r0 = _simd_mul_ps(m, v[0]);              // (m00 * v.x)
    m = _simd_load1_ps(pMatrix + 0 * 4 + 1); // m[row][1]
    r1 = _simd_mul_ps(m, v[1]);              // (m1 * v.y)
    r0 = _simd_add_ps(r0, r1);               // (m0 * v.x) + (m1 * v.y)
    m = _simd_load1_ps(pMatrix + 0 * 4 + 2); // m[row][2]
    r1 = _simd_mul_ps(m, v[2]);              // (m2 * v.z)
    r0 = _simd_add_ps(r0, r1);               // (m0 * v.x) + (m1 * v.y) + (m2 * v.z)
    m = _simd_load1_ps(pMatrix + 0 * 4 + 3); // m[row][3]
    r1 = _simd_mul_ps(m, v[3]);              // (m3 * v.z)
    r0 = _simd_add_ps(r0, r1);               // (m0 * v.x) + (m1 * v.y) + (m2 * v.z) + (m2 * v.w)
    result[0] = r0;

    m = _simd_load1_ps(pMatrix + 1 * 4 + 0); // m[row][0]
    r0 = _simd_mul_ps(m, v[0]);              // (m00 * v.x)
    m = _simd_load1_ps(pMatrix + 1 * 4 + 1); // m[row][1]
    r1 = _simd_mul_ps(m, v[1]);              // (m1 * v.y)
    r0 = _simd_add_ps(r0, r1);               // (m0 * v.x) + (m1 * v.y)
    m = _simd_load1_ps(pMatrix + 1 * 4 + 2); // m[row][2]
    r1 = _simd_mul_ps(m, v[2]);              // (m2 * v.z)
    r0 = _simd_add_ps(r0, r1);               // (m0 * v.x) + (m1 * v.y) + (m2 * v.z)
    m = _simd_load1_ps(pMatrix + 1 * 4 + 3); // m[row][3]
    r1 = _simd_mul_ps(m, v[3]);              // (m3 * v.z)
    r0 = _simd_add_ps(r0, r1);               // (m0 * v.x) + (m1 * v.y) + (m2 * v.z) + (m2 * v.w)
    result[1] = r0;

    m = _simd_load1_ps(pMatrix + 2 * 4 + 0); // m[row][0]
    r0 = _simd_mul_ps(m, v[0]);              // (m00 * v.x)
    m = _simd_load1_ps(pMatrix + 2 * 4 + 1); // m[row][1]
    r1 = _simd_mul_ps(m, v[1]);              // (m1 * v.y)
    r0 = _simd_add_ps(r0, r1);               // (m0 * v.x) + (m1 * v.y)
    m = _simd_load1_ps(pMatrix + 2 * 4 + 2); // m[row][2]
    r1 = _simd_mul_ps(m, v[2]);              // (m2 * v.z)
    r0 = _simd_add_ps(r0, r1);               // (m0 * v.x) + (m1 * v.y) + (m2 * v.z)
    m = _simd_load1_ps(pMatrix + 2 * 4 + 3); // m[row][3]
    r1 = _simd_mul_ps(m, v[3]);              // (m3 * v.z)
    r0 = _simd_add_ps(r0, r1);               // (m0 * v.x) + (m1 * v.y) + (m2 * v.z) + (m2 * v.w)
    result[2] = r0;

    m = _simd_load1_ps(pMatrix + 3 * 4 + 0); // m[row][0]
    r0 = _simd_mul_ps(m, v[0]);              // (m00 * v.x)
    m = _simd_load1_ps(pMatrix + 3 * 4 + 1); // m[row][1]
    r1 = _simd_mul_ps(m, v[1]);              // (m1 * v.y)
    r0 = _simd_add_ps(r0, r1);               // (m0 * v.x) + (m1 * v.y)
    m = _simd_load1_ps(pMatrix + 3 * 4 + 2); // m[row][2]
    r1 = _simd_mul_ps(m, v[2]);              // (m2 * v.z)
    r0 = _simd_add_ps(r0, r1);               // (m0 * v.x) + (m1 * v.y) + (m2 * v.z)
    m = _simd_load1_ps(pMatrix + 3 * 4 + 3); // m[row][3]
    r1 = _simd_mul_ps(m, v[3]);              // (m3 * v.z)
    r0 = _simd_add_ps(r0, r1);               // (m0 * v.x) + (m1 * v.y) + (m2 * v.z) + (m2 * v.w)
    result[3] = r0;
}

// Matrix4x4 * Vector3 - Direction Vector where w = 0.
//   outVec.x = (m00 * v.x) + (m01 * v.y) + (m02 * v.z) + (m03 * 0)
//   outVec.y = (m10 * v.x) + (m11 * v.y) + (m12 * v.z) + (m13 * 0)
//   outVec.z = (m20 * v.x) + (m21 * v.y) + (m22 * v.z) + (m23 * 0)
//   outVec.w = (m30 * v.x) + (m31 * v.y) + (m32 * v.z) + (m33 * 0)
INLINE
void _simd_mat3x3_vec3_w0_multiply(
    simdvector &result,
    const float *pMatrix,
    const simdvector &v)
{
    simdscalar m;
    simdscalar r0;
    simdscalar r1;

    m = _simd_load1_ps(pMatrix + 0 * 4 + 0); // m[row][0]
    r0 = _simd_mul_ps(m, v[0]);              // (m00 * v.x)
    m = _simd_load1_ps(pMatrix + 0 * 4 + 1); // m[row][1]
    r1 = _simd_mul_ps(m, v[1]);              // (m1 * v.y)
    r0 = _simd_add_ps(r0, r1);               // (m0 * v.x) + (m1 * v.y)
    m = _simd_load1_ps(pMatrix + 0 * 4 + 2); // m[row][2]
    r1 = _simd_mul_ps(m, v[2]);              // (m2 * v.z)
    r0 = _simd_add_ps(r0, r1);               // (m0 * v.x) + (m1 * v.y) + (m2 * v.z)
    result[0] = r0;

    m = _simd_load1_ps(pMatrix + 1 * 4 + 0); // m[row][0]
    r0 = _simd_mul_ps(m, v[0]);              // (m00 * v.x)
    m = _simd_load1_ps(pMatrix + 1 * 4 + 1); // m[row][1]
    r1 = _simd_mul_ps(m, v[1]);              // (m1 * v.y)
    r0 = _simd_add_ps(r0, r1);               // (m0 * v.x) + (m1 * v.y)
    m = _simd_load1_ps(pMatrix + 1 * 4 + 2); // m[row][2]
    r1 = _simd_mul_ps(m, v[2]);              // (m2 * v.z)
    r0 = _simd_add_ps(r0, r1);               // (m0 * v.x) + (m1 * v.y) + (m2 * v.z)
    result[1] = r0;

    m = _simd_load1_ps(pMatrix + 2 * 4 + 0); // m[row][0]
    r0 = _simd_mul_ps(m, v[0]);              // (m00 * v.x)
    m = _simd_load1_ps(pMatrix + 2 * 4 + 1); // m[row][1]
    r1 = _simd_mul_ps(m, v[1]);              // (m1 * v.y)
    r0 = _simd_add_ps(r0, r1);               // (m0 * v.x) + (m1 * v.y)
    m = _simd_load1_ps(pMatrix + 2 * 4 + 2); // m[row][2]
    r1 = _simd_mul_ps(m, v[2]);              // (m2 * v.z)
    r0 = _simd_add_ps(r0, r1);               // (m0 * v.x) + (m1 * v.y) + (m2 * v.z)
    result[2] = r0;

    result[3] = _simd_setzero_ps();
}

// Matrix4x4 * Vector3 - Position vector where w = 1.
//   outVec.x = (m00 * v.x) + (m01 * v.y) + (m02 * v.z) + (m03 * 1)
//   outVec.y = (m10 * v.x) + (m11 * v.y) + (m12 * v.z) + (m13 * 1)
//   outVec.z = (m20 * v.x) + (m21 * v.y) + (m22 * v.z) + (m23 * 1)
//   outVec.w = (m30 * v.x) + (m31 * v.y) + (m32 * v.z) + (m33 * 1)
INLINE
void _simd_mat4x4_vec3_w1_multiply(
    simdvector &result,
    const float *pMatrix,
    const simdvector &v)
{
    simdscalar m;
    simdscalar r0;
    simdscalar r1;

    m = _simd_load1_ps(pMatrix + 0 * 4 + 0); // m[row][0]
    r0 = _simd_mul_ps(m, v[0]);              // (m00 * v.x)
    m = _simd_load1_ps(pMatrix + 0 * 4 + 1); // m[row][1]
    r1 = _simd_mul_ps(m, v[1]);              // (m1 * v.y)
    r0 = _simd_add_ps(r0, r1);               // (m0 * v.x) + (m1 * v.y)
    m = _simd_load1_ps(pMatrix + 0 * 4 + 2); // m[row][2]
    r1 = _simd_mul_ps(m, v[2]);              // (m2 * v.z)
    r0 = _simd_add_ps(r0, r1);               // (m0 * v.x) + (m1 * v.y) + (m2 * v.z)
    m = _simd_load1_ps(pMatrix + 0 * 4 + 3); // m[row][3]
    r0 = _simd_add_ps(r0, m);                // (m0 * v.x) + (m1 * v.y) + (m2 * v.z) + (m2 * 1)
    result[0] = r0;

    m = _simd_load1_ps(pMatrix + 1 * 4 + 0); // m[row][0]
    r0 = _simd_mul_ps(m, v[0]);              // (m00 * v.x)
    m = _simd_load1_ps(pMatrix + 1 * 4 + 1); // m[row][1]
    r1 = _simd_mul_ps(m, v[1]);              // (m1 * v.y)
    r0 = _simd_add_ps(r0, r1);               // (m0 * v.x) + (m1 * v.y)
    m = _simd_load1_ps(pMatrix + 1 * 4 + 2); // m[row][2]
    r1 = _simd_mul_ps(m, v[2]);              // (m2 * v.z)
    r0 = _simd_add_ps(r0, r1);               // (m0 * v.x) + (m1 * v.y) + (m2 * v.z)
    m = _simd_load1_ps(pMatrix + 1 * 4 + 3); // m[row][3]
    r0 = _simd_add_ps(r0, m);                // (m0 * v.x) + (m1 * v.y) + (m2 * v.z) + (m2 * 1)
    result[1] = r0;

    m = _simd_load1_ps(pMatrix + 2 * 4 + 0); // m[row][0]
    r0 = _simd_mul_ps(m, v[0]);              // (m00 * v.x)
    m = _simd_load1_ps(pMatrix + 2 * 4 + 1); // m[row][1]
    r1 = _simd_mul_ps(m, v[1]);              // (m1 * v.y)
    r0 = _simd_add_ps(r0, r1);               // (m0 * v.x) + (m1 * v.y)
    m = _simd_load1_ps(pMatrix + 2 * 4 + 2); // m[row][2]
    r1 = _simd_mul_ps(m, v[2]);              // (m2 * v.z)
    r0 = _simd_add_ps(r0, r1);               // (m0 * v.x) + (m1 * v.y) + (m2 * v.z)
    m = _simd_load1_ps(pMatrix + 2 * 4 + 3); // m[row][3]
    r0 = _simd_add_ps(r0, m);                // (m0 * v.x) + (m1 * v.y) + (m2 * v.z) + (m2 * 1)
    result[2] = r0;

    m = _simd_load1_ps(pMatrix + 3 * 4 + 0); // m[row][0]
    r0 = _simd_mul_ps(m, v[0]);              // (m00 * v.x)
    m = _simd_load1_ps(pMatrix + 3 * 4 + 1); // m[row][1]
    r1 = _simd_mul_ps(m, v[1]);              // (m1 * v.y)
    r0 = _simd_add_ps(r0, r1);               // (m0 * v.x) + (m1 * v.y)
    m = _simd_load1_ps(pMatrix + 3 * 4 + 2); // m[row][2]
    r1 = _simd_mul_ps(m, v[2]);              // (m2 * v.z)
    r0 = _simd_add_ps(r0, r1);               // (m0 * v.x) + (m1 * v.y) + (m2 * v.z)
    m = _simd_load1_ps(pMatrix + 3 * 4 + 3); // m[row][3]
    result[3] = _simd_add_ps(r0, m);         // (m0 * v.x) + (m1 * v.y) + (m2 * v.z) + (m2 * 1)
}

INLINE
void _simd_mat4x3_vec3_w1_multiply(
    simdvector &result,
    const float *pMatrix,
    const simdvector &v)
{
    simdscalar m;
    simdscalar r0;
    simdscalar r1;

    m = _simd_load1_ps(pMatrix + 0 * 4 + 0); // m[row][0]
    r0 = _simd_mul_ps(m, v[0]);              // (m00 * v.x)
    m = _simd_load1_ps(pMatrix + 0 * 4 + 1); // m[row][1]
    r1 = _simd_mul_ps(m, v[1]);              // (m1 * v.y)
    r0 = _simd_add_ps(r0, r1);               // (m0 * v.x) + (m1 * v.y)
    m = _simd_load1_ps(pMatrix + 0 * 4 + 2); // m[row][2]
    r1 = _simd_mul_ps(m, v[2]);              // (m2 * v.z)
    r0 = _simd_add_ps(r0, r1);               // (m0 * v.x) + (m1 * v.y) + (m2 * v.z)
    m = _simd_load1_ps(pMatrix + 0 * 4 + 3); // m[row][3]
    r0 = _simd_add_ps(r0, m);                // (m0 * v.x) + (m1 * v.y) + (m2 * v.z) + (m2 * 1)
    result[0] = r0;

    m = _simd_load1_ps(pMatrix + 1 * 4 + 0); // m[row][0]
    r0 = _simd_mul_ps(m, v[0]);              // (m00 * v.x)
    m = _simd_load1_ps(pMatrix + 1 * 4 + 1); // m[row][1]
    r1 = _simd_mul_ps(m, v[1]);              // (m1 * v.y)
    r0 = _simd_add_ps(r0, r1);               // (m0 * v.x) + (m1 * v.y)
    m = _simd_load1_ps(pMatrix + 1 * 4 + 2); // m[row][2]
    r1 = _simd_mul_ps(m, v[2]);              // (m2 * v.z)
    r0 = _simd_add_ps(r0, r1);               // (m0 * v.x) + (m1 * v.y) + (m2 * v.z)
    m = _simd_load1_ps(pMatrix + 1 * 4 + 3); // m[row][3]
    r0 = _simd_add_ps(r0, m);                // (m0 * v.x) + (m1 * v.y) + (m2 * v.z) + (m2 * 1)
    result[1] = r0;

    m = _simd_load1_ps(pMatrix + 2 * 4 + 0); // m[row][0]
    r0 = _simd_mul_ps(m, v[0]);              // (m00 * v.x)
    m = _simd_load1_ps(pMatrix + 2 * 4 + 1); // m[row][1]
    r1 = _simd_mul_ps(m, v[1]);              // (m1 * v.y)
    r0 = _simd_add_ps(r0, r1);               // (m0 * v.x) + (m1 * v.y)
    m = _simd_load1_ps(pMatrix + 2 * 4 + 2); // m[row][2]
    r1 = _simd_mul_ps(m, v[2]);              // (m2 * v.z)
    r0 = _simd_add_ps(r0, r1);               // (m0 * v.x) + (m1 * v.y) + (m2 * v.z)
    m = _simd_load1_ps(pMatrix + 2 * 4 + 3); // m[row][3]
    r0 = _simd_add_ps(r0, m);                // (m0 * v.x) + (m1 * v.y) + (m2 * v.z) + (m2 * 1)
    result[2] = r0;
    result[3] = _simd_set1_ps(1.0f);
}

#endif //__SWR_SIMDINTRIN_H__
