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

#pragma once
#include "context.h"

INLINE
__m128i fpToFixedPoint(const __m128 vIn)
{
    __m128 vFixed = _mm_mul_ps(vIn, _mm_set1_ps(FIXED_POINT_SIZE));
    return _mm_cvtps_epi32(vFixed);
}

INLINE
simdscalari fpToFixedPointVertical(const simdscalar vIn)
{
    simdscalar vFixed = _simd_mul_ps(vIn, _simd_set1_ps(FIXED_POINT_SIZE));
    return _simd_cvtps_epi32(vFixed);
}

INLINE
void triangleSetupAB(const __m128 vX, const __m128 vY, __m128 &vA, __m128 &vB)
{
    // generate edge equations
    // A = y0 - y1
    // B = x1 - x0
    // C = x0y1 - x1y0
    __m128 vYsub = _mm_shuffle_ps(vY, vY, _MM_SHUFFLE(3, 0, 2, 1));
    vA = _mm_sub_ps(vY, vYsub);

    __m128 vXsub = _mm_shuffle_ps(vX, vX, _MM_SHUFFLE(3, 0, 2, 1));
    vB = _mm_sub_ps(vXsub, vX);
}

INLINE
void triangleSetupABVertical(const simdscalar vX[3], const simdscalar vY[3], simdscalar (&vA)[3], simdscalar (&vB)[3])
{
    // generate edge equations
    // A = y0 - y1
    // B = x1 - x0
    vA[0] = _simd_sub_ps(vY[0], vY[1]);
    vA[1] = _simd_sub_ps(vY[1], vY[2]);
    vA[2] = _simd_sub_ps(vY[2], vY[0]);

    vB[0] = _simd_sub_ps(vX[1], vX[0]);
    vB[1] = _simd_sub_ps(vX[2], vX[1]);
    vB[2] = _simd_sub_ps(vX[0], vX[2]);
}

INLINE
void triangleSetupABInt(const __m128i vX, const __m128i vY, __m128i &vA, __m128i &vB)
{
    // generate edge equations
    // A = y0 - y1
    // B = x1 - x0
    // C = x0y1 - x1y0
    __m128i vYsub = _mm_shuffle_epi32(vY, _MM_SHUFFLE(3, 0, 2, 1));
    vA = _mm_sub_epi32(vY, vYsub);

    __m128i vXsub = _mm_shuffle_epi32(vX, _MM_SHUFFLE(3, 0, 2, 1));
    vB = _mm_sub_epi32(vXsub, vX);
}

INLINE
void triangleSetupABIntVertical(const simdscalari vX[3], const simdscalari vY[3], simdscalari (&vA)[3], simdscalari (&vB)[3])
{
    // A = y0 - y1
    // B = x1 - x0
    vA[0] = _simd_sub_epi32(vY[0], vY[1]);
    vA[1] = _simd_sub_epi32(vY[1], vY[2]);
    vA[2] = _simd_sub_epi32(vY[2], vY[0]);

    vB[0] = _simd_sub_epi32(vX[1], vX[0]);
    vB[1] = _simd_sub_epi32(vX[2], vX[1]);
    vB[2] = _simd_sub_epi32(vX[0], vX[2]);
}

INLINE
float calcDeterminantInt(const __m128i vA, const __m128i vB)
{
    // (y1-y2)(x0-x2) + (x2-x1)*(y0-y2)
    //A1*B2 - B1*A2

    __m128i vAShuf = _mm_shuffle_epi32(vA, _MM_SHUFFLE(0, 2, 0, 1));
    __m128i vBShuf = _mm_shuffle_epi32(vB, _MM_SHUFFLE(0, 1, 0, 2));
    __m128i vMul = _mm_mul_epi32(vAShuf, vBShuf);

    // shuffle upper to lower
    __m128i vMul2 = _mm_shuffle_epi32(vMul, _MM_SHUFFLE(3, 2, 3, 2));
    vMul = _mm_sub_epi64(vMul, vMul2);

    // convert back to fixed point -
    assert(FIXED_POINT_WIDTH == 8);
    vMul = _mm_srli_si128(vMul, 1);

    // convert back to float
    __m128 vMulF = _mm_cvtepi32_ps(vMul);
    vMulF = _mm_mul_ps(vMulF, _mm_set1_ps(1.0 / FIXED_POINT_SIZE));

    float result;
    _mm_store_ss(&result, vMulF);
    return result;
}

INLINE
void calcDeterminantIntVertical(const simdscalari vA[3], const simdscalari vB[3], simdscalar &vDet)
{
    // (y1-y2)(x0-x2) + (x2-x1)*(y0-y2)
    //A1*B2 - B1*A2

    // @todo make this work for 8 wide
    // A1*B2
    simdscalari vA1Lo = _simd_unpacklo_epi32(vA[1], vA[1]);
    simdscalari vA1Hi = _simd_unpackhi_epi32(vA[1], vA[1]);

    simdscalari vB2Lo = _simd_unpacklo_epi32(vB[2], vB[2]);
    simdscalari vB2Hi = _simd_unpackhi_epi32(vB[2], vB[2]);

    simdscalari vA1B2Lo = _simd_mul_epi32(vA1Lo, vB2Lo);
    simdscalari vA1B2Hi = _simd_mul_epi32(vA1Hi, vB2Hi);

    // B1*A2
    simdscalari vA2Lo = _simd_unpacklo_epi32(vA[2], vA[2]);
    simdscalari vA2Hi = _simd_unpackhi_epi32(vA[2], vA[2]);

    simdscalari vB1Lo = _simd_unpacklo_epi32(vB[1], vB[1]);
    simdscalari vB1Hi = _simd_unpackhi_epi32(vB[1], vB[1]);

    simdscalari vA2B1Lo = _simd_mul_epi32(vA2Lo, vB1Lo);
    simdscalari vA2B1Hi = _simd_mul_epi32(vA2Hi, vB1Hi);

    // A2*B2 - A2*B1
    simdscalari detLo = _simd_sub_epi64(vA1B2Lo, vA2B1Lo);
    simdscalari detHi = _simd_sub_epi64(vA1B2Hi, vA2B1Hi);

    // convert back to fixed point
    assert(FIXED_POINT_WIDTH == 8);
    detLo = _simd_srli_si(detLo, 1);
    detHi = _simd_srli_si(detHi, 1);

    // pack
    simdscalari detFull = _simd_shuffleps_epi32(detLo, detHi, _MM_SHUFFLE(2, 0, 2, 0));

    // convert back to float
    simdscalar detFloat = _simd_cvtepi32_ps(detFull);
    detFloat = _simd_mul_ps(detFloat, _simd_set1_ps(1.0f / FIXED_POINT_SIZE));

    vDet = detFloat;
}

INLINE
void triangleSetupC(const __m128 vX, const __m128 vY, const __m128 vA, const __m128 &vB, __m128 &vC)
{
    // C = -Ax - By
    vC = _mm_mul_ps(vA, vX);
    __m128 vCy = _mm_mul_ps(vB, vY);
    vC = _mm_mul_ps(vC, _mm_set1_ps(-1.0f));
    vC = _mm_sub_ps(vC, vCy);
}

INLINE
void transform(__m128 &vX, __m128 &vY, __m128 &vZ, const RASTSTATE &state, DRIVER_TYPE driverType)
{
    __m128 vHalfWidth = _mm_set1_ps(state.vp.halfWidth);
    __m128 vHalfHeight = _mm_set1_ps(state.vp.halfHeight);

    // DX inverts Y screen coordinates
    if (driverType == DX)
    {
        _mm_mul_ps(vY, _mm_set1_ps(-1));
    }

    // x = width/2 * x + (width/2) + vpX
    vX = _mm_mul_ps(vX, vHalfWidth);
    vX = _mm_add_ps(vX, vHalfWidth);
    vX = _mm_add_ps(vX, _mm_set1_ps(state.vp.x));

    // y = height/2 - (height/2 * y) + vpY
    vY = _mm_mul_ps(vY, vHalfHeight);
    vY = _mm_add_ps(vHalfHeight, vY);
    vY = _mm_add_ps(vY, _mm_set1_ps(state.vp.y));

    // ogl depth range rules: linear map -1..1 to zNear..zFar
    // (z + 1 ) / 2 * (zFar - zNear) + zNear
    __m128 zRange = _mm_set1_ps(state.vp.maxZ - state.vp.minZ);
    vZ = _mm_add_ps(vZ, _mm_set1_ps(1.0));
    vZ = _mm_mul_ps(vZ, _mm_set1_ps(0.5));
    vZ = _mm_mul_ps(vZ, zRange);
    vZ = _mm_add_ps(vZ, _mm_set1_ps(state.vp.minZ));
}

INLINE
void transformVertical(simdvector &v0, simdvector &v1, simdvector &v2, const RASTSTATE &state, DRIVER_TYPE driverType)
{
    simdscalar vHalfWidth = _simd_set1_ps(state.vp.halfWidth);
    simdscalar vHalfHeight = _simd_set1_ps(state.vp.halfHeight);

    // DX inverts Y screen coordinates
    if (driverType == DX)
    {
        _simd_mul_ps(v0.y, _simd_set1_ps(-1));
        _simd_mul_ps(v1.y, _simd_set1_ps(-1));
        _simd_mul_ps(v2.y, _simd_set1_ps(-1));
    }

    // x = width/2 * x + (width/2) + vpX
    v0.x = _simd_mul_ps(v0.x, vHalfWidth);
    v0.x = _simd_add_ps(v0.x, vHalfWidth);
    v0.x = _simd_add_ps(v0.x, _simd_set1_ps(state.vp.x));

    v1.x = _simd_mul_ps(v1.x, vHalfWidth);
    v1.x = _simd_add_ps(v1.x, vHalfWidth);
    v1.x = _simd_add_ps(v1.x, _simd_set1_ps(state.vp.x));

    v2.x = _simd_mul_ps(v2.x, vHalfWidth);
    v2.x = _simd_add_ps(v2.x, vHalfWidth);
    v2.x = _simd_add_ps(v2.x, _simd_set1_ps(state.vp.x));

    // y = height/2 - (height/2 * y) + vpY
    v0.y = _simd_mul_ps(v0.y, vHalfHeight);
    v0.y = _simd_add_ps(vHalfHeight, v0.y);
    v0.y = _simd_add_ps(v0.y, _simd_set1_ps(state.vp.y));

    v1.y = _simd_mul_ps(v1.y, vHalfHeight);
    v1.y = _simd_add_ps(vHalfHeight, v1.y);
    v1.y = _simd_add_ps(v1.y, _simd_set1_ps(state.vp.y));

    v2.y = _simd_mul_ps(v2.y, vHalfHeight);
    v2.y = _simd_add_ps(vHalfHeight, v2.y);
    v2.y = _simd_add_ps(v2.y, _simd_set1_ps(state.vp.y));

    // ogl depth range rules: linear map -1..1 to zNear..zFar
    // (z + 1 ) / 2 * (zFar - zNear) + zNear
    simdscalar zRange = _simd_set1_ps(state.vp.maxZ - state.vp.minZ);
    v0.z = _simd_add_ps(v0.z, _simd_set1_ps(1.0));
    v0.z = _simd_mul_ps(v0.z, _simd_set1_ps(0.5));
    v0.z = _simd_mul_ps(v0.z, zRange);
    v0.z = _simd_add_ps(v0.z, _simd_set1_ps(state.vp.minZ));

    v1.z = _simd_add_ps(v1.z, _simd_set1_ps(1.0));
    v1.z = _simd_mul_ps(v1.z, _simd_set1_ps(0.5));
    v1.z = _simd_mul_ps(v1.z, zRange);
    v1.z = _simd_add_ps(v1.z, _simd_set1_ps(state.vp.minZ));

    v2.z = _simd_add_ps(v2.z, _simd_set1_ps(1.0));
    v2.z = _simd_mul_ps(v2.z, _simd_set1_ps(0.5));
    v2.z = _simd_mul_ps(v2.z, zRange);
    v2.z = _simd_add_ps(v2.z, _simd_set1_ps(state.vp.minZ));
}

INLINE
void calcBoundingBoxInt(const __m128i &vX, const __m128i &vY, BBOX &bbox)
{
    // Need horizontal fp min here
    __m128i vX1 = _mm_shuffle_epi32(vX, _MM_SHUFFLE(3, 2, 0, 1));
    __m128i vX2 = _mm_shuffle_epi32(vX, _MM_SHUFFLE(3, 0, 1, 2));

    __m128i vY1 = _mm_shuffle_epi32(vY, _MM_SHUFFLE(3, 2, 0, 1));
    __m128i vY2 = _mm_shuffle_epi32(vY, _MM_SHUFFLE(3, 0, 1, 2));

    __m128i vMinX = _mm_min_epi32(vX, vX1);
    vMinX = _mm_min_epi32(vMinX, vX2);

    __m128i vMaxX = _mm_max_epi32(vX, vX1);
    vMaxX = _mm_max_epi32(vMaxX, vX2);

    __m128i vMinY = _mm_min_epi32(vY, vY1);
    vMinY = _mm_min_epi32(vMinY, vY2);

    __m128i vMaxY = _mm_max_epi32(vY, vY1);
    vMaxY = _mm_max_epi32(vMaxY, vY2);

    bbox.left = _mm_extract_epi32(vMinX, 0);
    bbox.right = _mm_extract_epi32(vMaxX, 0);
    bbox.top = _mm_extract_epi32(vMinY, 0);
    bbox.bottom = _mm_extract_epi32(vMaxY, 0);

#if 0
	Jacob:  A = _mm_shuffle_ps(X, Y, 0 0 0 0)
B = _mm_shuffle_ps(Z, W, 0 0 0 0)
A = _mm_shuffle_epi32(A, 3 0 3 0)
A = _mm_shuffle_ps(A, B, 1 0 1 0)
#endif
}

INLINE
void calcBoundingBoxIntVertical(const simdscalari (&vX)[3], const simdscalari (&vY)[3], simdBBox &bbox)
{
    simdscalari vMinX = vX[0];
    vMinX = _simd_min_epi32(vMinX, vX[1]);
    vMinX = _simd_min_epi32(vMinX, vX[2]);

    simdscalari vMaxX = vX[0];
    vMaxX = _simd_max_epi32(vMaxX, vX[1]);
    vMaxX = _simd_max_epi32(vMaxX, vX[2]);

    simdscalari vMinY = vY[0];
    vMinY = _simd_min_epi32(vMinY, vY[1]);
    vMinY = _simd_min_epi32(vMinY, vY[2]);

    simdscalari vMaxY = vY[0];
    vMaxY = _simd_max_epi32(vMaxY, vY[1]);
    vMaxY = _simd_max_epi32(vMaxY, vY[2]);

    bbox.left = vMinX;
    bbox.right = vMaxX;
    bbox.top = vMinY;
    bbox.bottom = vMaxY;
}

void ProcessDraw(SWR_CONTEXT *pContext, DRAW_CONTEXT *pDC, void *pUserData);
void ProcessDrawIndexed(SWR_CONTEXT *pContext, DRAW_CONTEXT *pDC, void *pUserData);
void ProcessClear(SWR_CONTEXT *pContext, DRAW_CONTEXT *pDC, void *pUserData);
void ProcessPresent(SWR_CONTEXT *pContext, DRAW_CONTEXT *pDC, void *pUserData);
void ProcessCopy(SWR_CONTEXT *pContext, DRAW_CONTEXT *pDC, void *pUserData);
