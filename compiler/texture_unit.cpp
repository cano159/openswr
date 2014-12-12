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

#include "texture_unit.h"
#include "utils.h"
#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdio>
#include <emmintrin.h>
#include <smmintrin.h>
#include <tmmintrin.h>

#ifdef WIN32
#pragma warning(disable : 4345)
#endif

#undef min
#undef max
#undef near
#undef far

HANDLE SwrCreateTextureView(
    HANDLE hContext,
    SWR_TEXTUREVIEW const &args)
{
    return new TextureView(args);
}

void SwrDestroyTextureView(
    HANDLE hContext,
    HANDLE hTextureView)
{
    delete reinterpret_cast<TextureView *>(hTextureView);
}

HANDLE SwrCreateSampler(
    HANDLE hContext,
    SWR_CREATESAMPLER const &args)
{
    return new Sampler(args);
}

void SwrDestroySampler(
    HANDLE hContext,
    HANDLE hSampler)
{
    delete reinterpret_cast<Sampler *>(hSampler);
}

void SampleDefaultColor(TextureView const &txView, Sampler const &smp, TexCoord const &tc, WideColor &color)
{
    color.R = _simd_set1_ps(smp.mDefaultColor[0]);
    color.G = _simd_set1_ps(smp.mDefaultColor[1]);
    color.B = _simd_set1_ps(smp.mDefaultColor[2]);
    color.A = _simd_set1_ps(smp.mDefaultColor[3]);
}

#if KNOB_VS_SIMD_WIDTH == 8
INLINE
void vTranspose8x4(WideColor &dst, __m128 &row0, __m128 &row1, __m128 &row2, __m128 &row3, __m128 &row4, __m128 &row5, __m128 &row6, __m128 &row7)
{
    __m256 r0r4 = _mm256_insertf128_ps(_mm256_castps128_ps256(row0), row4, 0x1); // x0y0z0w0x4y4z4w4
    __m256 r2r6 = _mm256_insertf128_ps(_mm256_castps128_ps256(row2), row6, 0x1); // x2y2z2w2x6y6z6w6
    __m256 r1r5 = _mm256_insertf128_ps(_mm256_castps128_ps256(row1), row5, 0x1); // x1y1z1w1x5y5z5w5
    __m256 r3r7 = _mm256_insertf128_ps(_mm256_castps128_ps256(row3), row7, 0x1); // x3y3z3w3x7y7z7w7

    __m256 xyeven = _mm256_unpacklo_ps(r0r4, r2r6); // x0x2y0y2x4x6y4y6
    __m256 zweven = _mm256_unpackhi_ps(r0r4, r2r6); // z0z2w0w2z4z6w4w6
    __m256 xyodd = _mm256_unpacklo_ps(r1r5, r3r7);  // x1x3y1y3x5x7y5y7
    __m256 zwodd = _mm256_unpackhi_ps(r1r5, r3r7);  // z1z3w1w3z5z7w5w7

    dst.R = _mm256_unpacklo_ps(xyeven, xyodd); // x0x1x2x3x4x5x6x7
    dst.G = _mm256_unpackhi_ps(xyeven, xyodd); // y0y1y2y3y4y5y6y7
    dst.B = _mm256_unpacklo_ps(zweven, zwodd); // z0z1z2z3z4z5z6z7
    dst.A = _mm256_unpackhi_ps(zweven, zwodd); // w0w1w2w3w4w5w6w7
}
#endif

template <bool UseFloat, bool Brolinear, SWR_ADDRESSING_MODE AddrModeU, SWR_ADDRESSING_MODE AddrModeV>
void SampleSimplePointRGBA(TextureView const &txView, Sampler const &smp, TexCoord const &tc, UINT (&mips)[4], WideColor &color)
{
    // Widths and heights.
    simdscalar txW = _simd_set1_ps((float)txView.mpTexture->mTexelWidth[0]);
    simdscalar txH = _simd_set1_ps((float)txView.mpTexture->mTexelHeight[0]);
    simdscalari pyW = _simd_set1_epi32(txView.mpTexture->mPhysicalWidth[0]);

    // wrap
    simdscalar vWrapU;
    simdscalar vWrapV;
    simdscalar over, under;
    simdscalar outOfBounds = _simd_set1_ps(0.0f);

    switch (AddrModeU)
    {
    case SWR_AM_MIRROR:
    {
        vWrapU = _simd_andnot_ps(_simd_set1_ps(-0.0f), tc.U);
        vWrapU = _simd_mul_ps(_simd_set1_ps(0.5f), vWrapU);
        vWrapU = _simd_sub_ps(vWrapU, _simd_round_ps(vWrapU, _MM_FROUND_TO_NEG_INF));
        vWrapU = _simd_mul_ps(_simd_set1_ps(2.0f), vWrapU);
        simdscalar odd = _simd_sub_ps(_simd_set1_ps(2.0f), vWrapU);
        vWrapU = _simd_blendv_ps(vWrapU, odd, _simd_cmpgt_ps(vWrapU, _simd_set1_ps(1.0f)));
    }
    break;
    case SWR_AM_WRAP:
        vWrapU = _simd_sub_ps(tc.U, _simd_round_ps(tc.U, _MM_FROUND_TO_NEG_INF));
        break;
    case SWR_AM_DEFAULT:
        over = _simd_cmpge_ps(tc.U, _simd_set1_ps(1.0f));
        under = _simd_cmplt_ps(tc.U, _simd_set1_ps(0.0f));
        outOfBounds = _simd_or_ps(over, under);
    // fall through; DCE will eliminate outOfBounds for non-CLAMP cases.
    case SWR_AM_CLAMP:
        vWrapU = _simd_blendv_ps(tc.U, _simd_set1_ps(0.99999995f), over);
        vWrapU = _simd_blendv_ps(tc.U, _simd_set1_ps(0.0f), under);
        break;
    default:
        break;
    }

    switch (AddrModeV)
    {
    case SWR_AM_MIRROR:
    {
        vWrapV = _simd_andnot_ps(_simd_set1_ps(-0.0f), tc.V);
        vWrapV = _simd_mul_ps(_simd_set1_ps(0.5f), vWrapV);
        vWrapV = _simd_sub_ps(vWrapV, _simd_round_ps(vWrapV, _MM_FROUND_TO_NEG_INF));
        vWrapV = _simd_mul_ps(_simd_set1_ps(2.0f), vWrapV);
        simdscalar odd = _simd_sub_ps(_simd_set1_ps(2.0f), vWrapV);
        vWrapV = _simd_blendv_ps(vWrapV, odd, _simd_cmpgt_ps(vWrapV, _simd_set1_ps(1.0f)));
    }
    break;
    case SWR_AM_WRAP:
        vWrapV = _simd_sub_ps(tc.V, _simd_round_ps(tc.V, _MM_FROUND_TO_NEG_INF));
        break;
    case SWR_AM_DEFAULT:
        over = _simd_cmpge_ps(tc.V, _simd_set1_ps(1.0f));
        under = _simd_cmplt_ps(tc.V, _simd_set1_ps(0.0f));
        outOfBounds = _simd_or_ps(outOfBounds, _simd_or_ps(over, under)); // Join of U & V.
                                                                          // fall through; DCE will eliminate outOfBounds for non-CLAMP cases.
    case SWR_AM_CLAMP:
        vWrapV = _simd_blendv_ps(tc.V, _simd_set1_ps(0.99999995f), over);
        vWrapV = _simd_blendv_ps(tc.V, _simd_set1_ps(0.0f), under);
        break;
    default:
        break;
    }

    // Scale, clamp to edge, and fetch.
    simdscalar X = _simd_mul_ps(vWrapU, txW);
    simdscalar Y = _simd_mul_ps(vWrapV, txH);

#if 0
	// Clamp.
	simdscalar txW1	= _simd_sub_ps(txW, _simd_set1_ps(1.0f));
	simdscalar txH1	= _simd_sub_ps(txH, _simd_set1_ps(1.0f));

	X			= _simd_min_ps(_simd_max_ps(_simd_set1_ps(0.0f), X), txW1);
	Y			= _simd_min_ps(_simd_max_ps(_simd_set1_ps(0.0f), Y), txH1);
#endif

    X = _simd_round_ps(X, _MM_FROUND_TO_NEG_INF);
    Y = _simd_round_ps(Y, _MM_FROUND_TO_NEG_INF);

    // Convert to integer.
    simdscalari x = _simd_cvtps_epi32(X);
    simdscalari y = _simd_cvtps_epi32(Y);

#if 0
	simdscalar Z	= _simd_mul_ps(tc.W, _simd_set1_ps((float)txView.mpTexture->mNumPlanes));
	Z			= _simd_min_ps(_simd_max_ps(_simd_set1_ps(0.0f), Z), _simd_set1_ps((float)txView.mpTexture->mNumPlanes-1));
	Z			= _simd_round_ps(Z, _MM_FROUND_TO_NEG_INF);
	simdscalari z	= _simd_cvtps_epi32(Z);
#endif

    // Get offset.
    simdscalari offset = _simd_mullo_epi32(y, pyW);
    offset = _simd_add_epi32(offset, x);
    offset = _simd_mullo_epi32(offset, _simd_set1_epi32(txView.mpTexture->mElementSizeInBytes));

    // Fetch color data. Ignore Z; ignore MIP.
    OSALIGN(UINT, 32) offsets[KNOB_VS_SIMD_WIDTH];
    _simd_store_si((simdscalari *)&offsets[0], offset);

    if (UseFloat)
    {
        __m128 result[KNOB_VS_SIMD_WIDTH];
        for (UINT i = 0; i < KNOB_VS_SIMD_WIDTH; ++i)
        {
            result[i] = _mm_loadu_ps((float *)(txView.mpTexture->mSubtextures[0] + offsets[i]));
        }

//if ((AddrModeU == SWR_AM_DEFAULT) || (AddrModeV == SWR_AM_DEFAULT))
//{
//  __m128 defClr	= _mm_loadu_ps(smp.mDefaultColor);
//	clr7		= _mm_blendv_ps(clr7, defClr, outOfBounds);
//	clr6		= _mm_blendv_ps(clr6, defClr, outOfBounds);
//	clr5		= _mm_blendv_ps(clr5, defClr, outOfBounds);
//	clr4		= _mm_blendv_ps(clr4, defClr, outOfBounds);
//	clr3		= _mm_blendv_ps(clr3, defClr, outOfBounds);
//	clr2		= _mm_blendv_ps(clr2, defClr, outOfBounds);
//	clr1		= _mm_blendv_ps(clr1, defClr, outOfBounds);
//	clr0		= _mm_blendv_ps(clr0, defClr, outOfBounds);
//}

#if 0
		if (Brolinear)
		{
			__m128 alpha	= _mm_sub_ps(X, _mm_cvtepi32_ps(x));
			__m128 beta		= _mm_sub_ps(Y, _mm_cvtepi32_ps(y));
			__m128 _1_alpha	= _mm_sub_ps(_mm_set1_ps(1.0f), alpha);
			__m128 _1_beta	= _mm_sub_ps(_mm_set1_ps(1.0f), beta);

			__m128 a_b		= _mm_mul_ps(alpha, beta);
			__m128 a_1b		= _mm_mul_ps(alpha, _1_beta);
			__m128 _1a_b	= _mm_mul_ps(_1_alpha, beta);
			__m128 _1a_1b	= _mm_mul_ps(_1_alpha, _1_beta);

			clr3			= _mm_mul_ps(clr3, a_b);
			clr2			= _mm_mul_ps(clr2, a_b);
			clr1			= _mm_mul_ps(clr1, a_b);
			clr0			= _mm_mul_ps(clr0, a_b);

			__m128i Xnext	= _mm_add_epi32(offset, _mm_set1_epi32(txView.mpTexture->mElementSizeInBytes));
			__m128i Ynext	= _mm_add_epi32(offset, _mm_mullo_epi32(pyW, _mm_set1_epi32(txView.mpTexture->mElementSizeInBytes)));
			__m128i XYnext	= _mm_add_epi32(Ynext, _mm_set1_epi32(txView.mpTexture->mElementSizeInBytes));

            simdscalar txW1	= _simd_sub_ps(txW, _simd_set1_ps(1.0f));
            simdscalar txH1	= _simd_sub_ps(txH, _simd_set1_ps(1.0f));

			__m128 xlt		= _mm_cmplt_ps(X, txW1);
			__m128 ylt		= _mm_cmplt_ps(Y, txH1);

			_mm_store_si128((__m128i*)&offsets[0],
				_mm_castps_si128(
					_mm_blendv_ps(
						_mm_castsi128_ps(offset),
						_mm_castsi128_ps(Xnext),
						xlt)));
			clr3	= _mm_add_ps(clr3, _mm_mul_ps(_1a_b, _mm_loadu_ps((float*)(txView.mpTexture->mSubtextures[mips[gO[0]]]+offsets[gO[0]]))));
			clr2	= _mm_add_ps(clr2, _mm_mul_ps(_1a_b, _mm_loadu_ps((float*)(txView.mpTexture->mSubtextures[mips[gO[1]]]+offsets[gO[1]]))));
			clr1	= _mm_add_ps(clr1, _mm_mul_ps(_1a_b, _mm_loadu_ps((float*)(txView.mpTexture->mSubtextures[mips[gO[2]]]+offsets[gO[2]]))));
			clr0	= _mm_add_ps(clr0, _mm_mul_ps(_1a_b, _mm_loadu_ps((float*)(txView.mpTexture->mSubtextures[mips[gO[3]]]+offsets[gO[3]]))));

			if ((AddrModeU == SWR_AM_DEFAULT) || (AddrModeV == SWR_AM_DEFAULT))
			{
				assert(false && "Default color for mip is ... complicated. Bailing, for now.");
			}

			_mm_store_si128((__m128i*)&offsets[0],
				_mm_castps_si128(
					_mm_blendv_ps(
						_mm_castsi128_ps(offset),
						_mm_castsi128_ps(Ynext),
						ylt)));
			clr3	= _mm_add_ps(clr3, _mm_mul_ps(a_1b, _mm_loadu_ps((float*)(txView.mpTexture->mSubtextures[mips[gO[0]]]+offsets[gO[0]]))));
			clr2	= _mm_add_ps(clr2, _mm_mul_ps(a_1b, _mm_loadu_ps((float*)(txView.mpTexture->mSubtextures[mips[gO[1]]]+offsets[gO[1]]))));
			clr1	= _mm_add_ps(clr1, _mm_mul_ps(a_1b, _mm_loadu_ps((float*)(txView.mpTexture->mSubtextures[mips[gO[2]]]+offsets[gO[2]]))));
			clr0	= _mm_add_ps(clr0, _mm_mul_ps(a_1b, _mm_loadu_ps((float*)(txView.mpTexture->mSubtextures[mips[gO[3]]]+offsets[gO[3]]))));

			if ((AddrModeU == SWR_AM_DEFAULT) || (AddrModeV == SWR_AM_DEFAULT))
			{
				assert(false && "Default color for mip is ... complicated. Bailing, for now.");
			}

			_mm_store_si128((__m128i*)&offsets[0],
				_mm_castps_si128(
					_mm_blendv_ps(
						_mm_blendv_ps(
							_mm_castsi128_ps(Xnext),
							_mm_blendv_ps(
								_mm_castsi128_ps(Ynext),
								_mm_castsi128_ps(offset),
								xlt),
							ylt),
						_mm_castsi128_ps(XYnext),
						_mm_and_ps(xlt, ylt))));

			clr3	= _mm_add_ps(clr3, _mm_mul_ps(_1a_1b, _mm_loadu_ps((float*)(txView.mpTexture->mSubtextures[mips[gO[0]]]+offsets[gO[0]]))));
			clr2	= _mm_add_ps(clr2, _mm_mul_ps(_1a_1b, _mm_loadu_ps((float*)(txView.mpTexture->mSubtextures[mips[gO[1]]]+offsets[gO[1]]))));
			clr1	= _mm_add_ps(clr1, _mm_mul_ps(_1a_1b, _mm_loadu_ps((float*)(txView.mpTexture->mSubtextures[mips[gO[2]]]+offsets[gO[2]]))));
			clr0	= _mm_add_ps(clr0, _mm_mul_ps(_1a_1b, _mm_loadu_ps((float*)(txView.mpTexture->mSubtextures[mips[gO[3]]]+offsets[gO[3]]))));
		}
#endif
#if KNOB_VS_SIMD_WIDTH == 4
        color.R = result[0];
        color.G = result[1];
        color.B = result[2];
        color.A = result[3];
        vTranspose(color.R, color.G, color.B, color.A);
#elif KNOB_VS_SIMD_WIDTH == 8
        vTranspose8x4(color, result[0], result[1], result[2], result[3], result[4], result[5], result[6], result[7]);
#endif
    }
    else
    {
#if 0
		assert(false && "This is not going to work, so suck it.");
		__m128i clr	= _mm_setzero_si128();
		// Oh, gather! Where art thou!
		clr			= _mm_insert_epi32(clr, *(UINT32*)(txView.mpTexture->mSubtextures[mips[gO[0]]]+offsets[gO[0]]), 0);
		clr			= _mm_insert_epi32(clr, *(UINT32*)(txView.mpTexture->mSubtextures[mips[gO[1]]]+offsets[gO[1]]), 1);
		clr			= _mm_insert_epi32(clr, *(UINT32*)(txView.mpTexture->mSubtextures[mips[gO[2]]]+offsets[gO[2]]), 2);
		clr			= _mm_insert_epi32(clr, *(UINT32*)(txView.mpTexture->mSubtextures[mips[gO[3]]]+offsets[gO[3]]), 3);

		// Split & upconvert.
		color.R		= _mm_cvtepi32_ps(_mm_and_si128(clr, _mm_set1_epi32(0xFF)));
		clr			= _mm_srai_epi32(clr, 8);
		color.G		= _mm_cvtepi32_ps(_mm_and_si128(clr, _mm_set1_epi32(0xFF)));
		clr			= _mm_srai_epi32(clr, 8);
		color.B		= _mm_cvtepi32_ps(_mm_and_si128(clr, _mm_set1_epi32(0xFF)));
		clr			= _mm_srai_epi32(clr, 8);
		color.A		= _mm_cvtepi32_ps(_mm_and_si128(clr, _mm_set1_epi32(0xFF)));
#endif
    }
}

#if 0

template <bool UseFloat, bool Brolinear, SWR_ADDRESSING_MODE AddrModeU, SWR_ADDRESSING_MODE AddrModeV>
void SampleSimplePointQuadRGBA(TextureView const& txView, Sampler const& smp, TexCoord const& tc, WideColor& color)
{
	// We assume that U,V are related as a quad.
	__m128 diff	= _mm_hsub_ps(tc.U, tc.V); // U0 - U1, U2 - U3, V0 - V1, V2 - V3
	diff		= _mm_mul_ps(diff, diff); // dU.0, dU.1, dV.0, dV.1
	__m128 Us	= _mm_shuffle_ps(diff, diff, _MM_SHUFFLE(3, 2, 3, 2));
	__m128 Vs	= _mm_shuffle_ps(diff, diff, _MM_SHUFFLE(1, 0, 0, 1));
	diff		= _mm_add_ps(Us, Vs);	// distances
	__m128 M1	= _mm_shuffle_ps(diff, diff, _MM_SHUFFLE(1, 0, 3, 2));
	diff		= _mm_max_ps(diff, M1); // {3, 1}, {2, 0}, {1, 3}, {0, 2}
	M1			= _mm_shuffle_ps(diff, diff, _MM_SHUFFLE(2, 3, 0, 1));
	diff		= _mm_max_ps(diff, M1); // {3, 2, 1, 0} ...

	// grab the floor(log_2(diff))
	__m128i flg2	= _mm_sub_epi32(_mm_castps_si128(_mm_and_ps(_mm_castsi128_ps(_mm_srai_epi32(_mm_castps_si128(diff), 23)), _mm_castsi128_ps(_mm_set1_epi32(0xFF)))), _mm_set1_epi32(127));
	__m128 low	= _mm_round_ps(diff, _MM_FROUND_FLOOR);
	low			= _mm_sub_ps(diff, low);
	__m128 high	= _mm_sub_ps(_mm_set1_ps(1.0f), low);

	OSALIGN(UINT, 16) mips[4];
	_mm_store_si128((__m128i*)&mips[0], flg2);
	WideColor lowClr;
	SampleSimplePointRGBA<UseFloat, Brolinear, AddrModeU, AddrModeV>(txView, smp, tc, mips, lowClr);

	// Use txView.mMipMax to clamp upper bound.
	flg2		= _mm_add_epi32(flg2, _mm_set1_epi32(1));
	flg2		= _mm_min_epi32(flg2, _mm_set1_epi32(txView.mpTexture->mNumMipLevels));
	_mm_store_si128((__m128i*)&mips[0], flg2);
	WideColor highClr;
	SampleSimplePointRGBA<UseFloat, Brolinear, AddrModeU, AddrModeV>(txView, smp, tc, mips, highClr);

	color.A	= _mm_add_ps(_mm_mul_ps(low, lowClr.A), _mm_mul_ps(high, highClr.A));
	color.R	= _mm_add_ps(_mm_mul_ps(low, lowClr.R), _mm_mul_ps(high, highClr.R));
	color.G	= _mm_add_ps(_mm_mul_ps(low, lowClr.G), _mm_mul_ps(high, highClr.G));
	color.B	= _mm_add_ps(_mm_mul_ps(low, lowClr.B), _mm_mul_ps(high, highClr.B));
}

void SampleSimplePointQuadRGBAU8(TextureView const& txView, Sampler const& smp, TexCoord const& tc, WideColor& color)
{
	SampleSimplePointQuadRGBA<false, false, SWR_AM_WRAP, SWR_AM_WRAP>(txView, smp, tc, color);
}

void SampleSimplePointRGBAU8(TextureView const& txView, Sampler const& smp, TexCoord const& tc, UINT (&mips)[4], WideColor& color)
{
	SampleSimplePointRGBA<false, false, SWR_AM_WRAP, SWR_AM_WRAP>(txView, smp, tc, mips, color);
}

void SampleSimplePointQuadRGBAF32(TextureView const& txView, Sampler const& smp, TexCoord const& tc, WideColor& color)
{
	SampleSimplePointQuadRGBA<true, false, SWR_AM_WRAP, SWR_AM_WRAP>(txView, smp, tc, color);
}
#endif

void SampleSimplePointRGBAF32(TextureView const &txView, Sampler const &smp, TexCoord const &tc, UINT (&mips)[4], WideColor &color)
{
    SampleSimplePointRGBA<true, false, SWR_AM_WRAP, SWR_AM_WRAP>(txView, smp, tc, mips, color);
}

#if 0
void SampleSimpleLinearQuadRGBAF32(TextureView const& txView, Sampler const& smp, TexCoord const& tc, WideColor& color)
{
	SampleSimplePointQuadRGBA<true, true, SWR_AM_WRAP, SWR_AM_WRAP>(txView, smp, tc, color);
}

void SampleSimpleLinearRGBAF32(TextureView const& txView, Sampler const& smp, TexCoord const& tc, UINT (&mips)[4], WideColor& color)
{
	SampleSimplePointRGBA<true, true, SWR_AM_WRAP, SWR_AM_WRAP>(txView, smp, tc, mips, color);
}

#endif
