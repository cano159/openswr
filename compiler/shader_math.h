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

#include "knobs.h"
#include "api.h"
#include "widevector.hpp"
#include "rdtsc.h"

#include <smmintrin.h>

#ifndef __WIN64
#define SHORT_ABI_DECORATE &
#else
#define SHORT_ABI_DECORATE
#endif

#define max(a, b) (((a) > (b)) ? (a) : (b))

// Compute the gradients of U and V.
INLINE __m128 CalculateGradients(__m128 U, __m128 V)
{
    // U 3 2 1 0
    // du/dx = U1 - U0
    // du/dy = U2 - U0
    // dv/dx = V1 - V0
    // dv/dy = V2 - V0
    return _mm_hsub_ps(_mm_shuffle_ps(V, V, _MM_SHUFFLE(2, 0, 1, 0)), _mm_shuffle_ps(U, U, _MM_SHUFFLE(2, 0, 1, 0)));
}

INLINE __m128 Vec3Normalize(__m128 vec)
{
    __m128 vecLen = _mm_dp_ps(vec, vec, 0xFF);
    vecLen = _mm_rsqrt_ps(vecLen);
    return _mm_mul_ps(vec, vecLen);
}

INLINE __m128 Vec3Normalize(FLOAT *pNormal)
{
    __m128 vec = _mm_load_ps(pNormal);
    return Vec3Normalize(vec);
}

INLINE __m128 Vec3Diffuse(const __m128 &normal, const __m128 &lightDir)
{
    __m128 ndotl = _mm_dp_ps(normal, lightDir, 0xFF);
    return _mm_max_ps(ndotl, _mm_setzero_ps());
}

INLINE __m128 Mat4Vec3Multiply(__m128 &vec, const __m128 &row0, const __m128 &row1, const __m128 &row2, const __m128 &row3)
{
    __m128 result0;
    __m128 result1;
    result0 = _mm_dp_ps(vec, row0, 0xF1);
    result1 = _mm_dp_ps(vec, row1, 0xF2);
    result0 = _mm_or_ps(result0, result1);
    result1 = _mm_dp_ps(vec, row2, 0xF4);
    result0 = _mm_or_ps(result0, result1);
    result1 = _mm_dp_ps(vec, row3, 0xF8);
    return _mm_or_ps(result0, result1);
}

INLINE simdscalar vplaneps(simdscalar vA, simdscalar vB, simdscalar vC, simdscalar &vX, simdscalar &vY)
{
    simdscalar vOut = _simd_fmadd_ps(vA, vX, vC);
    vOut = _simd_fmadd_ps(vB, vY, vOut);
    return vOut;
}

INLINE simdscalari vFloatToUnorm(simdscalar vIn)
{
    // clamp to 0, 1
    vIn = _simd_max_ps(vIn, _simd_setzero_ps());
    vIn = _simd_min_ps(vIn, _simd_set1_ps(1.0));
    vIn = _simd_mul_ps(vIn, _simd_set1_ps(255.0));
    return _simd_cvtps_epi32(vIn);
}

INLINE simdscalar vUnormToFloat(simdscalari vIn)
{
    return _simd_mul_ps(_simd_cvtepi32_ps(vIn), _simd_set1_ps((float)(1.0 / 255.0)));
}

INLINE void Mat4Vec3Multiply(
    FLOAT *pVecResult,
    const FLOAT *pMatrix,
    const FLOAT *pVector)
{
    // Matrix * Vector (Essentially 4 dot products for each vertex)
    //   outVec.x = (m00 * inVec.x) + (m01 * inVec.y) + (m02 * inVec.z) + (m03 * inVec.w)
    //   outVec.y = (m10 * inVec.x) + (m11 * inVec.y) + (m12 * inVec.z) + (m13 * inVec.w)
    //   outVec.z = (m20 * inVec.x) + (m21 * inVec.y) + (m22 * inVec.z) + (m23 * inVec.w)
    //   outVec.w = (m30 * inVec.x) + (m31 * inVec.y) + (m32 * inVec.z) + (m33 * inVec.w)
    __m128 vec = _mm_load_ps(pVector);
    __m128 row0 = _mm_load_ps(&pMatrix[0]);
    __m128 row1 = _mm_load_ps(&pMatrix[4]);
    __m128 row2 = _mm_load_ps(&pMatrix[8]);
    __m128 row3 = _mm_load_ps(&pMatrix[12]);

    __m128 result = Mat4Vec3Multiply(vec, row0, row1, row2, row3);
    _mm_store_ps(pVecResult, result);
}

extern const __m128 gMaskToVec[];

#if KNOB_VS_SIMD_WIDTH == 4
const __m128 vQuadOffsetsX = { 0.5, 1.5, 0.5, 1.5 };
const __m128 vQuadOffsetsY = { 0.5, 0.5, 1.5, 1.5 };
#define MASK 0xf
#elif KNOB_VS_SIMD_WIDTH == 8
const __m256 vQuadOffsetsX = { 0.5, 1.5, 0.5, 1.5, 2.5, 3.5, 2.5, 3.5 };
const __m256 vQuadOffsetsY = { 0.5, 0.5, 1.5, 1.5, 0.5, 0.5, 1.5, 1.5 };
#define MASK 0xff
#endif

template <typename AttrSelector, SWR_ZFUNCTION ZFunc = ZFUNC_LE, bool ZWrite = true, int XIterations = KNOB_TILE_X_DIM / SIMD_TILE_X_DIM, int YIterations = KNOB_TILE_Y_DIM / SIMD_TILE_Y_DIM>
struct GenericPixelShader
{
    typedef WideVector<AttrSelector::NUM_ATTRIBUTES, simdscalar> WV;

    void run(const SWR_TRIANGLE_DESC &work, SWR_PIXELOUTPUT &pOut, AttrSelector &attrSel)
    {
        UINT64 coverageMask = work.coverageMask;

        UINT x = work.tileX << KNOB_TILE_X_DIM_SHIFT;
        UINT y = work.tileY << KNOB_TILE_Y_DIM_SHIFT;

        BYTE *pBuffer = (BYTE *)pOut.pRenderTargets[0] + y * work.widthInBytes + work.tileX * KNOB_TILE_X_DIM * KNOB_TILE_Y_DIM * 4;
        BYTE *pZBuffer = (BYTE *)pOut.pRenderTargets[1] + y * work.widthInBytes + work.tileX * KNOB_TILE_X_DIM * KNOB_TILE_Y_DIM * 4;

        UINT32 curBit = 0;

        WV vInit;
        WV vStepX;
        WV vStepY;

        simdscalar vOneOverW;
        simdscalar vOneOverWStepX;
        simdscalar vOneOverWStepY;

        ///////////////////////////////////////////
        // Compute I, J, and 1/W stuff.
        simdscalar vIa = _simd_broadcast_ss(&work.I[0]);
        simdscalar vIb = _simd_broadcast_ss(&work.I[1]);
        simdscalar vIc = _simd_broadcast_ss(&work.I[2]);

        simdscalar vJa = _simd_broadcast_ss(&work.J[0]);
        simdscalar vJb = _simd_broadcast_ss(&work.J[1]);
        simdscalar vJc = _simd_broadcast_ss(&work.J[2]);

        simdscalar vZa = _simd_broadcast_ss(&work.Z[0]);
        simdscalar vZb = _simd_broadcast_ss(&work.Z[1]);
        simdscalar vZc = _simd_broadcast_ss(&work.Z[2]);

        simdscalar vX = _simd_add_ps(vQuadOffsetsX, _simd_set1_ps((float)x));
        simdscalar vY = _simd_add_ps(vQuadOffsetsY, _simd_set1_ps((float)y));

        // evaluate I,J at top left corner of tile
        simdscalar vI = vplaneps(vIa, vIb, vIc, vX, vY);
        simdscalar vJ = vplaneps(vJa, vJb, vJc, vX, vY);

        // multiply by 1/det
        simdscalar vRecipDet = _simd_broadcast_ss(&work.recipDet);
        vI = _simd_mul_ps(vI, vRecipDet);
        vJ = _simd_mul_ps(vJ, vRecipDet);

        // evaluate Z at top left corner of tile
        simdscalar vZ = vplaneps(vZa, vZb, vZc, vI, vJ);

        // Z step functions
        simdscalar vZStepX = _simd_broadcast_ss(&work.zStepX);
        simdscalar vZStepY = _simd_broadcast_ss(&work.zStepY);

        // i and j step functions
        simdscalar vXOverDet = _simd_mul_ps(_simd_set1_ps(SIMD_TILE_X_DIM), vRecipDet);
        simdscalar vYOverDet = _simd_mul_ps(_simd_set1_ps(SIMD_TILE_Y_DIM), vRecipDet);

        simdscalar vIStepX = _simd_mul_ps(vIa, vXOverDet);
        simdscalar vIStepY = _simd_mul_ps(vIb, vYOverDet);
        simdscalar vJStepX = _simd_mul_ps(vJa, vXOverDet);
        simdscalar vJStepY = _simd_mul_ps(vJb, vYOverDet);

        if (AttrSelector::DO_PERSPECTIVE)
        {
            // interpolate 1/w
            simdscalar vAOneOverW = _simd_broadcast_ss(&work.OneOverW[0]);
            simdscalar vBOneOverW = _simd_broadcast_ss(&work.OneOverW[1]);
            simdscalar vCOneOverW = _simd_broadcast_ss(&work.OneOverW[2]);

            vOneOverW = vplaneps(vAOneOverW, vBOneOverW, vCOneOverW, vI, vJ);

            vOneOverWStepX = _simd_add_ps(_simd_mul_ps(vAOneOverW, vIStepX), _simd_mul_ps(vBOneOverW, vJStepX));
            vOneOverWStepY = _simd_add_ps(_simd_mul_ps(vAOneOverW, vIStepY), _simd_mul_ps(vBOneOverW, vJStepY));
        }

        ///////////////////////////////////////////
        // Per-attribute information.
        const float *pTemp = (const float *)work.pInterpBuffer;

        UINT attr = 0;
        if (AttrSelector::NUM_INTERPOLANTS >= 1)
        {
            const float *pVA = &pTemp[0];
            const float *pVB = &pTemp[4];
            const float *pVC = &pTemp[8];

            simdscalar vA;
            simdscalar vB;
            simdscalar vC;
            switch (AttrSelector::SIGNATURE[0])
            {
            case 4:
                vA = _simd_broadcast_ss(&pVA[3]);
                vB = _simd_broadcast_ss(&pVB[3]);
                vC = _simd_broadcast_ss(&pVC[3]);
                get(vInit, attr + 3) = vplaneps(vA, vB, vC, vI, vJ);
                get(vStepX, attr + 3) = _simd_add_ps(_simd_mul_ps(vA, vIStepX), _simd_mul_ps(vB, vJStepX));
                get(vStepY, attr + 3) = _simd_add_ps(_simd_mul_ps(vA, vIStepY), _simd_mul_ps(vB, vJStepY));
            case 3:
                vA = _simd_broadcast_ss(&pVA[2]);
                vB = _simd_broadcast_ss(&pVB[2]);
                vC = _simd_broadcast_ss(&pVC[2]);
                get(vInit, attr + 2) = vplaneps(vA, vB, vC, vI, vJ);
                get(vStepX, attr + 2) = _simd_add_ps(_simd_mul_ps(vA, vIStepX), _simd_mul_ps(vB, vJStepX));
                get(vStepY, attr + 2) = _simd_add_ps(_simd_mul_ps(vA, vIStepY), _simd_mul_ps(vB, vJStepY));
            case 2:
                vA = _simd_broadcast_ss(&pVA[1]);
                vB = _simd_broadcast_ss(&pVB[1]);
                vC = _simd_broadcast_ss(&pVC[1]);
                get(vInit, attr + 1) = vplaneps(vA, vB, vC, vI, vJ);
                get(vStepX, attr + 1) = _simd_add_ps(_simd_mul_ps(vA, vIStepX), _simd_mul_ps(vB, vJStepX));
                get(vStepY, attr + 1) = _simd_add_ps(_simd_mul_ps(vA, vIStepY), _simd_mul_ps(vB, vJStepY));
            case 1:
                vA = _simd_broadcast_ss(&pVA[0]);
                vB = _simd_broadcast_ss(&pVB[0]);
                vC = _simd_broadcast_ss(&pVC[0]);
                get(vInit, attr + 0) = vplaneps(vA, vB, vC, vI, vJ);
                get(vStepX, attr + 0) = _simd_add_ps(_simd_mul_ps(vA, vIStepX), _simd_mul_ps(vB, vJStepX));
                get(vStepY, attr + 0) = _simd_add_ps(_simd_mul_ps(vA, vIStepY), _simd_mul_ps(vB, vJStepY));
            }

            attr += AttrSelector::SIGNATURE[0];
            pTemp += 12;
        }

        if (AttrSelector::NUM_INTERPOLANTS >= 2)
        {
            const float *pVA = &pTemp[0];
            const float *pVB = &pTemp[4];
            const float *pVC = &pTemp[8];

            simdscalar vA;
            simdscalar vB;
            simdscalar vC;
            switch (AttrSelector::SIGNATURE[1])
            {
            case 4:
                vA = _simd_broadcast_ss(&pVA[3]);
                vB = _simd_broadcast_ss(&pVB[3]);
                vC = _simd_broadcast_ss(&pVC[3]);
                get(vInit, attr + 3) = vplaneps(vA, vB, vC, vI, vJ);
                get(vStepX, attr + 3) = _simd_add_ps(_simd_mul_ps(vA, vIStepX), _simd_mul_ps(vB, vJStepX));
                get(vStepY, attr + 3) = _simd_add_ps(_simd_mul_ps(vA, vIStepY), _simd_mul_ps(vB, vJStepY));
            case 3:
                vA = _simd_broadcast_ss(&pVA[2]);
                vB = _simd_broadcast_ss(&pVB[2]);
                vC = _simd_broadcast_ss(&pVC[2]);
                get(vInit, attr + 2) = vplaneps(vA, vB, vC, vI, vJ);
                get(vStepX, attr + 2) = _simd_add_ps(_simd_mul_ps(vA, vIStepX), _simd_mul_ps(vB, vJStepX));
                get(vStepY, attr + 2) = _simd_add_ps(_simd_mul_ps(vA, vIStepY), _simd_mul_ps(vB, vJStepY));
            case 2:
                vA = _simd_broadcast_ss(&pVA[1]);
                vB = _simd_broadcast_ss(&pVB[1]);
                vC = _simd_broadcast_ss(&pVC[1]);
                get(vInit, attr + 1) = vplaneps(vA, vB, vC, vI, vJ);
                get(vStepX, attr + 1) = _simd_add_ps(_simd_mul_ps(vA, vIStepX), _simd_mul_ps(vB, vJStepX));
                get(vStepY, attr + 1) = _simd_add_ps(_simd_mul_ps(vA, vIStepY), _simd_mul_ps(vB, vJStepY));
            case 1:
                vA = _simd_broadcast_ss(&pVA[0]);
                vB = _simd_broadcast_ss(&pVB[0]);
                vC = _simd_broadcast_ss(&pVC[0]);
                get(vInit, attr + 0) = vplaneps(vA, vB, vC, vI, vJ);
                get(vStepX, attr + 0) = _simd_add_ps(_simd_mul_ps(vA, vIStepX), _simd_mul_ps(vB, vJStepX));
                get(vStepY, attr + 0) = _simd_add_ps(_simd_mul_ps(vA, vIStepY), _simd_mul_ps(vB, vJStepY));
            }

            attr += AttrSelector::SIGNATURE[1];
            pTemp += 12;
        }

        if (AttrSelector::NUM_INTERPOLANTS > 2)
        {
            for (UINT i = 2; i < AttrSelector::NUM_INTERPOLANTS; ++i)
            {
                const float *pVA = &pTemp[0];
                const float *pVB = &pTemp[4];
                const float *pVC = &pTemp[8];

                simdscalar vA;
                simdscalar vB;
                simdscalar vC;
                switch (AttrSelector::SIGNATURE[i])
                {
                case 4:
                    vA = _simd_broadcast_ss(&pVA[3]);
                    vB = _simd_broadcast_ss(&pVB[3]);
                    vC = _simd_broadcast_ss(&pVC[3]);
                    get(vInit, attr + 3) = vplaneps(vA, vB, vC, vI, vJ);
                    get(vStepX, attr + 3) = _simd_add_ps(_simd_mul_ps(vA, vIStepX), _simd_mul_ps(vB, vJStepX));
                    get(vStepY, attr + 3) = _simd_add_ps(_simd_mul_ps(vA, vIStepY), _simd_mul_ps(vB, vJStepY));
                case 3:
                    vA = _simd_broadcast_ss(&pVA[2]);
                    vB = _simd_broadcast_ss(&pVB[2]);
                    vC = _simd_broadcast_ss(&pVC[2]);
                    get(vInit, attr + 2) = vplaneps(vA, vB, vC, vI, vJ);
                    get(vStepX, attr + 2) = _simd_add_ps(_simd_mul_ps(vA, vIStepX), _simd_mul_ps(vB, vJStepX));
                    get(vStepY, attr + 2) = _simd_add_ps(_simd_mul_ps(vA, vIStepY), _simd_mul_ps(vB, vJStepY));
                case 2:
                    vA = _simd_broadcast_ss(&pVA[1]);
                    vB = _simd_broadcast_ss(&pVB[1]);
                    vC = _simd_broadcast_ss(&pVC[1]);
                    get(vInit, attr + 1) = vplaneps(vA, vB, vC, vI, vJ);
                    get(vStepX, attr + 1) = _simd_add_ps(_simd_mul_ps(vA, vIStepX), _simd_mul_ps(vB, vJStepX));
                    get(vStepY, attr + 1) = _simd_add_ps(_simd_mul_ps(vA, vIStepY), _simd_mul_ps(vB, vJStepY));
                case 1:
                    vA = _simd_broadcast_ss(&pVA[0]);
                    vB = _simd_broadcast_ss(&pVB[0]);
                    vC = _simd_broadcast_ss(&pVC[0]);
                    get(vInit, attr + 0) = vplaneps(vA, vB, vC, vI, vJ);
                    get(vStepX, attr + 0) = _simd_add_ps(_simd_mul_ps(vA, vIStepX), _simd_mul_ps(vB, vJStepX));
                    get(vStepY, attr + 0) = _simd_add_ps(_simd_mul_ps(vA, vIStepY), _simd_mul_ps(vB, vJStepY));
                }

                attr += AttrSelector::SIGNATURE[i];
                pTemp += 12;
            }
        }

        for (UINT i = 0; i < YIterations; ++i)
        {
            this->YLoop(work, pZBuffer, pBuffer, coverageMask, curBit, vInit, vZ, vOneOverW, vStepX, vStepY, vZStepX,
                        vZStepY, vOneOverWStepX, vOneOverWStepY, attrSel);
        }
    }

    INLINE void YLoop(const SWR_TRIANGLE_DESC &work, BYTE *&pZBuffer, BYTE *&pBuffer, UINT64 &coverageMask, UINT32 &curBit,
                      WV &vInit, simdscalar &vZ, simdscalar &vOneOverW, WV &vStepX, WV &vStepY, const simdscalar &vZStepX,
                      const simdscalar &vZStepY, const simdscalar &vOneOverWStepX, const simdscalar &vOneOverWStepY, AttrSelector &attrSel)
    {
        simdscalar vStartOneOverW;
        if (AttrSelector::DO_PERSPECTIVE)
        {
            vStartOneOverW = vOneOverW;
        }

        WV vStart = vInit;
        simdscalar vStartRowZ = vZ;

        switch (XIterations)
        {
        case 4:
            this->XLoop(work, pZBuffer, pBuffer, vZ, coverageMask, curBit, vOneOverW, vInit, vStepX, vStepY, vZStepX, vZStepY, vOneOverWStepX, attrSel);
        case 3:
            this->XLoop(work, pZBuffer, pBuffer, vZ, coverageMask, curBit, vOneOverW, vInit, vStepX, vStepY, vZStepX, vZStepY, vOneOverWStepX, attrSel);
        case 2:
            this->XLoop(work, pZBuffer, pBuffer, vZ, coverageMask, curBit, vOneOverW, vInit, vStepX, vStepY, vZStepX, vZStepY, vOneOverWStepX, attrSel);
        case 1:
            this->XLoop(work, pZBuffer, pBuffer, vZ, coverageMask, curBit, vOneOverW, vInit, vStepX, vStepY, vZStepX, vZStepY, vOneOverWStepX, attrSel);
        }

        vInit = vStart + vStepY;

        vZ = vStartRowZ;
        vZ = _simd_add_ps(vZ, vZStepY);

        if (AttrSelector::DO_PERSPECTIVE)
        {
            vOneOverW = _simd_add_ps(vStartOneOverW, vOneOverWStepY);
        }
    }

    template <SWR_ZFUNCTION ZTFunc>
    INLINE UINT ZTest(const float *pZBuffer, simdscalar vZ, UINT coverageMask)
    {
        simdscalar vCmp;
        if (ZTFunc == ZFUNC_NEVER)
        {
            return 0;
        }
        else if (ZTFunc == ZFUNC_ALWAYS)
        {
            vCmp = _simd_set1_ps(-1);
        }
        else
        {
            simdscalar vZBuf = _simd_load_ps(pZBuffer);

            switch (ZTFunc)
            {
            case ZFUNC_LE:
                vCmp = _simd_cmple_ps(vZ, vZBuf);
                break;
            case ZFUNC_LT:
                vCmp = _simd_cmplt_ps(vZ, vZBuf);
                break;
            case ZFUNC_GT:
                vCmp = _simd_cmpgt_ps(vZ, vZBuf);
                break;
            case ZFUNC_GE:
                vCmp = _simd_cmpge_ps(vZ, vZBuf);
                break;
            case ZFUNC_EQ:
                vCmp = _simd_cmpeq_ps(vZ, vZBuf);
                break;
            }
        }

        // cull any pixels outside depth range
        UINT range = MASK;
        if (ZTFunc != ZFUNC_NEVER && ZTFunc != ZFUNC_ALWAYS)
        {
            simdscalar vClampZ = _simd_cmpge_ps(vZ, _simd_setzero_ps());
            simdscalar vSatZ = _simd_cmple_ps(vZ, _simd_set1_ps(1.0));

            simdscalar vRange = _simd_and_ps(vClampZ, vSatZ);
            range = _simd_movemask_ps(vRange);
        }

        return coverageMask & range & _simd_movemask_ps(vCmp);
    }

#if KNOB_VS_SIMD_WIDTH == 4
    INLINE __m128i maskToVec(INT mask)
    {
        return _mm_castps_si128(gMaskToVec[mask]);
    }
#elif KNOB_VS_SIMD_WIDTH == 8
    INLINE __m256i maskToVec(INT mask)
    {
        __m256i vec = _mm256_set1_epi32(mask);
        const __m256i bit = _mm256_set_epi32(0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01);
        vec = _simd_and_si(vec, bit);
        vec = _simd_cmplt_epi32(_mm256_setzero_si256(), vec);
        return vec;
    }
#endif

    INLINE void XLoop(const SWR_TRIANGLE_DESC &work, BYTE *&pZBuffer, BYTE *&pBuffer, simdscalar &vZ, UINT64 &coverageMask,
                      UINT32 &curBit, simdscalar &vOneOverW, WV &vInit, WV &vStepX, WV &vStepY, simdscalar vZStepX,
                      simdscalar vZStepY, simdscalar vOneOverWStepX, AttrSelector &attrSel)
    {
        // z compare
        UINT mask = ZTest<ZFunc>((const float *)pZBuffer, vZ, (coverageMask >> curBit) & MASK);

        if (mask)
        {

            WV vComputedW;

            if (AttrSelector::DO_PERSPECTIVE)
            {
                simdscalar rcp = _simd_div_ps(_simd_set1_ps(1.0), vOneOverW);
                vComputedW = vInit * rcp;
            }
            else
            {
                vComputedW = vInit;
            }

            UINT shadeMask = mask;

            RDTSC_START(BEPixelShaderFunc);
            simdscalar vShaded = shade(attrSel, work, vComputedW, pBuffer, pZBuffer, &shadeMask);
            RDTSC_STOP(BEPixelShaderFunc, 0, 0);

            UINT outMask = mask & shadeMask;
            _simd_maskstore_ps((float *)pBuffer, maskToVec(outMask), vShaded);

            if (ZWrite)
            {
                _simd_maskstore_ps((float *)pZBuffer, maskToVec(outMask), vZ);
            }
        }

        curBit += KNOB_VS_SIMD_WIDTH;
        pBuffer += KNOB_VS_SIMD_WIDTH * sizeof(UINT);

        vInit += vStepX;

        pZBuffer += KNOB_VS_SIMD_WIDTH * sizeof(float);
        vZ = _simd_add_ps(vZ, vZStepX);

        if (AttrSelector::DO_PERSPECTIVE)
        {
            vOneOverW = _simd_add_ps(vOneOverW, vOneOverWStepX);
        }
    }
};
