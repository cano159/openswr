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

// rasterizer.cpp : Defines the entry point for the console application.
//

#include <vector>
#include <algorithm>

#include "rasterizer.h"
#include "rdtsc.h"
#include "backend.h"
#include "utils.h"
#include "frontend.h"

#define MASKTOVEC(i3, i2, i1, i0) \
    {                             \
        -i0, -i1, -i2, -i3        \
    }
const __m128 gMaskToVec[] = {
    MASKTOVEC(0, 0, 0, 0),
    MASKTOVEC(0, 0, 0, 1),
    MASKTOVEC(0, 0, 1, 0),
    MASKTOVEC(0, 0, 1, 1),
    MASKTOVEC(0, 1, 0, 0),
    MASKTOVEC(0, 1, 0, 1),
    MASKTOVEC(0, 1, 1, 0),
    MASKTOVEC(0, 1, 1, 1),
    MASKTOVEC(1, 0, 0, 0),
    MASKTOVEC(1, 0, 0, 1),
    MASKTOVEC(1, 0, 1, 0),
    MASKTOVEC(1, 0, 1, 1),
    MASKTOVEC(1, 1, 0, 0),
    MASKTOVEC(1, 1, 0, 1),
    MASKTOVEC(1, 1, 1, 0),
    MASKTOVEC(1, 1, 1, 1),
};

UINT64 rasterizePartialTile(DRAW_CONTEXT *pContext, UINT tileX, UINT tileY, const TRIANGLE_DESC &desc, __m128i vEdge0, __m128i vEdge1, __m128i vEdge2)
{
    UINT64 coverageMask = 0;

    __m128i vEdgeLeft, vEdgeRight, vEdgeTop, vEdgeBottom;
    __m128i vEdgeLeftStep, vEdgeRightStep, vEdgeTopStep, vEdgeBottomStep;

    if (desc.needScissor)
    {
        // Precompute quad step offsets
        const __m128i vQuadOffsetsXInt = _mm_set_epi32(256, 0, 256, 0);
        const __m128i vQuadOffsetsYInt = _mm_set_epi32(256, 256, 0, 0);

        const API_STATE &state = desc.pDC->state;

        int x = (tileX << (KNOB_TILE_X_DIM_SHIFT + FIXED_POINT_WIDTH)) + FIXED_POINT_SIZE / 2;
        int y = (tileY << (KNOB_TILE_Y_DIM_SHIFT + FIXED_POINT_WIDTH)) + FIXED_POINT_SIZE / 2;

        int leftA = -1;
        int rightA = 1;
        int topB = -1;
        int bottomB = 1;

        // evaluate scissor at tile topleft corner
        int edgeLeft = leftA * (x - state.scissorInFixedPoint.left);
        int edgeRight = rightA * (x - state.scissorInFixedPoint.right);
        int edgeTop = topB * (y - state.scissorInFixedPoint.top);
        int edgeBottom = bottomB * (y - state.scissorInFixedPoint.bottom);

        // step to 4 pixels of quad
        __m128i vLeftPixelStep = _mm_mullo_epi32(_mm_set1_epi32(leftA), vQuadOffsetsXInt);
        __m128i vRightPixelStep = _mm_mullo_epi32(_mm_set1_epi32(rightA), vQuadOffsetsXInt);
        __m128i vTopPixelStep = _mm_mullo_epi32(_mm_set1_epi32(topB), vQuadOffsetsYInt);
        __m128i vBottomPixelStep = _mm_mullo_epi32(_mm_set1_epi32(bottomB), vQuadOffsetsYInt);

        vEdgeLeft = _mm_add_epi32(vLeftPixelStep, _mm_set1_epi32(edgeLeft));
        vEdgeRight = _mm_add_epi32(vRightPixelStep, _mm_set1_epi32(edgeRight));
        vEdgeTop = _mm_add_epi32(vTopPixelStep, _mm_set1_epi32(edgeTop));
        vEdgeBottom = _mm_add_epi32(vBottomPixelStep, _mm_set1_epi32(edgeBottom));

        // step to next quad
        vEdgeLeftStep = _mm_set1_epi32(leftA * 2 * 256);
        vEdgeRightStep = _mm_set1_epi32(rightA * 2 * 256);
        vEdgeTopStep = _mm_set1_epi32(topB * 2 * 256);
        vEdgeBottomStep = _mm_set1_epi32(bottomB * 2 * 256);
    }

    // Step to the pixel centers of the 1st quad
    vEdge0 = _mm_shuffle_epi32(vEdge0, _MM_SHUFFLE(0, 0, 0, 0));
    vEdge1 = _mm_shuffle_epi32(vEdge1, _MM_SHUFFLE(0, 0, 0, 0));
    vEdge2 = _mm_shuffle_epi32(vEdge2, _MM_SHUFFLE(0, 0, 0, 0));

    vEdge0 = _mm_add_epi32(vEdge0, desc.vStepQuad0);
    vEdge1 = _mm_add_epi32(vEdge1, desc.vStepQuad1);
    vEdge2 = _mm_add_epi32(vEdge2, desc.vStepQuad2);

    // compute step to next quad
    __m128i vStep0X = _mm_slli_epi32(_mm_shuffle_epi32(desc.vA, _MM_SHUFFLE(0, 0, 0, 0)), 1);
    __m128i vStep0Y = _mm_slli_epi32(_mm_shuffle_epi32(desc.vB, _MM_SHUFFLE(0, 0, 0, 0)), 1);

    __m128i vStep1X = _mm_slli_epi32(_mm_shuffle_epi32(desc.vA, _MM_SHUFFLE(1, 1, 1, 1)), 1);
    __m128i vStep1Y = _mm_slli_epi32(_mm_shuffle_epi32(desc.vB, _MM_SHUFFLE(1, 1, 1, 1)), 1);

    __m128i vStep2X = _mm_slli_epi32(_mm_shuffle_epi32(desc.vA, _MM_SHUFFLE(2, 2, 2, 2)), 1);
    __m128i vStep2Y = _mm_slli_epi32(_mm_shuffle_epi32(desc.vB, _MM_SHUFFLE(2, 2, 2, 2)), 1);

// fast unrolled version for 8x8 tile
#if KNOB_TILE_X_DIM == 8 && KNOB_TILE_Y_DIM == 8
    int mask0, mask1, mask2, maskL, maskR, maskT, maskB;
    UINT64 mask;

#define EVAL                                           \
    mask0 = _mm_movemask_ps(_mm_castsi128_ps(vEdge0)); \
    mask1 = _mm_movemask_ps(_mm_castsi128_ps(vEdge1)); \
    mask2 = _mm_movemask_ps(_mm_castsi128_ps(vEdge2));

#define UPDATE_MASK(bit)          \
    mask = mask0 & mask1 & mask2; \
    coverageMask |= (mask << bit);

#define INCX                                 \
    vEdge0 = _mm_add_epi32(vEdge0, vStep0X); \
    vEdge1 = _mm_add_epi32(vEdge1, vStep1X); \
    vEdge2 = _mm_add_epi32(vEdge2, vStep2X);

#define INCY                                 \
    vEdge0 = _mm_add_epi32(vEdge0, vStep0Y); \
    vEdge1 = _mm_add_epi32(vEdge1, vStep1Y); \
    vEdge2 = _mm_add_epi32(vEdge2, vStep2Y);

#define DECX                                 \
    vEdge0 = _mm_sub_epi32(vEdge0, vStep0X); \
    vEdge1 = _mm_sub_epi32(vEdge1, vStep1X); \
    vEdge2 = _mm_sub_epi32(vEdge2, vStep2X);

#define EVAL_SCISSOR                                       \
    mask0 = _mm_movemask_ps(_mm_castsi128_ps(vEdge0));     \
    mask1 = _mm_movemask_ps(_mm_castsi128_ps(vEdge1));     \
    mask2 = _mm_movemask_ps(_mm_castsi128_ps(vEdge2));     \
    maskL = _mm_movemask_ps(_mm_castsi128_ps(vEdgeLeft));  \
    maskR = _mm_movemask_ps(_mm_castsi128_ps(vEdgeRight)); \
    maskT = _mm_movemask_ps(_mm_castsi128_ps(vEdgeTop));   \
    maskB = _mm_movemask_ps(_mm_castsi128_ps(vEdgeBottom));

#define UPDATE_MASK_SCISSOR(bit)                                  \
    mask = mask0 & mask1 & mask2 & maskL & maskR & maskT & maskB; \
    coverageMask |= (mask << bit);

#define INCX_SCISSOR                                     \
    vEdge0 = _mm_add_epi32(vEdge0, vStep0X);             \
    vEdge1 = _mm_add_epi32(vEdge1, vStep1X);             \
    vEdge2 = _mm_add_epi32(vEdge2, vStep2X);             \
    vEdgeLeft = _mm_add_epi32(vEdgeLeft, vEdgeLeftStep); \
    vEdgeRight = _mm_add_epi32(vEdgeRight, vEdgeRightStep);

#define INCY_SCISSOR                                  \
    vEdge0 = _mm_add_epi32(vEdge0, vStep0Y);          \
    vEdge1 = _mm_add_epi32(vEdge1, vStep1Y);          \
    vEdge2 = _mm_add_epi32(vEdge2, vStep2Y);          \
    vEdgeTop = _mm_add_epi32(vEdgeTop, vEdgeTopStep); \
    vEdgeBottom = _mm_add_epi32(vEdgeBottom, vEdgeBottomStep);

#define DECX_SCISSOR                                     \
    vEdge0 = _mm_sub_epi32(vEdge0, vStep0X);             \
    vEdge1 = _mm_sub_epi32(vEdge1, vStep1X);             \
    vEdge2 = _mm_sub_epi32(vEdge2, vStep2X);             \
    vEdgeLeft = _mm_sub_epi32(vEdgeLeft, vEdgeLeftStep); \
    vEdgeRight = _mm_sub_epi32(vEdgeRight, vEdgeRightStep);

    if (desc.needScissor)
    {
        // row 0
        EVAL_SCISSOR;
        UPDATE_MASK_SCISSOR(0);
        INCX_SCISSOR;
        EVAL_SCISSOR;
        UPDATE_MASK_SCISSOR(4);
        INCX_SCISSOR;
        EVAL_SCISSOR;
        UPDATE_MASK_SCISSOR(8);
        INCX_SCISSOR;
        EVAL_SCISSOR;
        UPDATE_MASK_SCISSOR(12);
        INCY_SCISSOR;

        //row 1
        EVAL_SCISSOR;
        UPDATE_MASK_SCISSOR(28);
        DECX_SCISSOR;
        EVAL_SCISSOR;
        UPDATE_MASK_SCISSOR(24);
        DECX_SCISSOR;
        EVAL_SCISSOR;
        UPDATE_MASK_SCISSOR(20);
        DECX_SCISSOR;
        EVAL_SCISSOR;
        UPDATE_MASK_SCISSOR(16);
        INCY_SCISSOR;

        // row 2
        EVAL_SCISSOR;
        UPDATE_MASK_SCISSOR(32);
        INCX_SCISSOR;
        EVAL_SCISSOR;
        UPDATE_MASK_SCISSOR(36);
        INCX_SCISSOR;
        EVAL_SCISSOR;
        UPDATE_MASK_SCISSOR(40);
        INCX_SCISSOR;
        EVAL_SCISSOR;
        UPDATE_MASK_SCISSOR(44);
        INCY_SCISSOR;

        // row 3
        EVAL_SCISSOR;
        UPDATE_MASK_SCISSOR(60);
        DECX_SCISSOR;
        EVAL_SCISSOR;
        UPDATE_MASK_SCISSOR(56);
        DECX_SCISSOR;
        EVAL_SCISSOR;
        UPDATE_MASK_SCISSOR(52);
        DECX_SCISSOR;
        EVAL_SCISSOR;
        UPDATE_MASK_SCISSOR(48);
    }
    else
    {
        // row 0
        EVAL;
        UPDATE_MASK(0);
        INCX;
        EVAL;
        UPDATE_MASK(4);
        INCX;
        EVAL;
        UPDATE_MASK(8);
        INCX;
        EVAL;
        UPDATE_MASK(12);
        INCY;

        //row 1
        EVAL;
        UPDATE_MASK(28);
        DECX;
        EVAL;
        UPDATE_MASK(24);
        DECX;
        EVAL;
        UPDATE_MASK(20);
        DECX;
        EVAL;
        UPDATE_MASK(16);
        INCY;

        // row 2
        EVAL;
        UPDATE_MASK(32);
        INCX;
        EVAL;
        UPDATE_MASK(36);
        INCX;
        EVAL;
        UPDATE_MASK(40);
        INCX;
        EVAL;
        UPDATE_MASK(44);
        INCY;

        // row 3
        EVAL;
        UPDATE_MASK(60);
        DECX;
        EVAL;
        UPDATE_MASK(56);
        DECX;
        EVAL;
        UPDATE_MASK(52);
        DECX;
        EVAL;
        UPDATE_MASK(48);
    }
#else
    UINT bit = 0;
    for (UINT y = 0; y < KNOB_TILE_Y_DIM / 2; ++y)
    {
        __m128i vStartOfRowEdge0 = vEdge0;
        __m128i vStartOfRowEdge1 = vEdge1;
        __m128i vStartOfRowEdge2 = vEdge2;

        for (UINT x = 0; x < KNOB_TILE_X_DIM / 2; ++x)
        {
            int mask0 = _mm_movemask_ps(_mm_castsi128_ps(vEdge0));
            int mask1 = _mm_movemask_ps(_mm_castsi128_ps(vEdge1));
            int mask2 = _mm_movemask_ps(_mm_castsi128_ps(vEdge2));

            UINT64 mask = mask0 & mask1 & mask2;
            coverageMask |= (mask << bit);

            // step to the next pixel in the x
            vEdge0 = _mm_add_epi32(vEdge0, vStep0X);
            vEdge1 = _mm_add_epi32(vEdge1, vStep1X);
            vEdge2 = _mm_add_epi32(vEdge2, vStep2X);
            bit += 4;
        }

        // step to the next row
        vEdge0 = _mm_add_epi32(vStartOfRowEdge0, vStep0Y);
        vEdge1 = _mm_add_epi32(vStartOfRowEdge1, vStep1Y);
        vEdge2 = _mm_add_epi32(vStartOfRowEdge2, vStep2Y);
    }
#endif
    return coverageMask;
}

INLINE __m128i adjustTopLeftRuleInt(const __m128i vA, const __m128i vB, const __m128i vEdge)
{
    // if vA < 0, vC--
    // if vA == 0 && vB < 0, vC--

    __m128i vEdgeOut = vEdge;
    __m128i vEdgeAdjust = _mm_sub_epi32(vEdge, _mm_set1_epi32(1));

    // if vA < 0
    int msk = _mm_movemask_ps(_mm_castsi128_ps(vA));

    // if vA == 0 && vB < 0
    __m128i vCmp = _mm_cmpeq_epi32(vA, _mm_setzero_si128());
    int msk2 = _mm_movemask_ps(_mm_castsi128_ps(vCmp));
    msk2 &= _mm_movemask_ps(_mm_castsi128_ps(vB));

    vEdgeOut = _mm_castps_si128(_mm_blendv_ps(_mm_castsi128_ps(vEdgeOut), _mm_castsi128_ps(vEdgeAdjust), gMaskToVec[msk | msk2]));
    return vEdgeOut;
}

// Prevent DCE by writing coverage mask from rasterizer to volatile
#ifdef KNOB_TOSS_RS
__declspec(thread) volatile UINT64 gToss;
#endif
template <bool Use32BitMath, bool DoPerspective>
void RasterizeTriangle(DRAW_CONTEXT *pDC, const TRIANGLE_WORK_DESC &knobDesc, UINT macroTile)
{
#ifdef KNOB_TOSS_BIN_TRIS
    return;
#endif

    RDTSC_START(BETriangleSetup);
    const API_STATE &state = pDC->state;

    OSALIGN(TRIANGLE_DESC, 16) fakeDesc;
    fakeDesc.widthInBytes = state.pRenderTargets[SWR_ATTACHMENT_COLOR0]->widthInBytes;
    fakeDesc.pDC = pDC;
    fakeDesc.pTextureViews = &state.aTextureViews[SHADER_PIXEL][0];
    fakeDesc.pSamplers = &state.aSamplers[SHADER_PIXEL][0];
    fakeDesc.pConstants = state.pVSConstantBufferAlloc->pData;

    __m128 vX, vY, vZ, vRecipW;
    vX = _mm_load_ps(knobDesc.pTriBuffer);
    vY = _mm_load_ps(knobDesc.pTriBuffer + 4);
    vZ = _mm_load_ps(knobDesc.pTriBuffer + 8);

    if (DoPerspective)
    {
        vRecipW = _mm_load_ps(knobDesc.pTriBuffer + 12);
    }

    // convert to fixed point
    __m128i vXi = fpToFixedPoint(vX);
    __m128i vYi = fpToFixedPoint(vY);

    // triangle setup
    __m128 vA, vB;
    triangleSetupAB(vX, vY, vA, vB);

    __m128i vAi, vBi;
    triangleSetupABInt(vXi, vYi, vAi, vBi);

    // determinant
    float det = calcDeterminantInt(vAi, vBi);

    // Convert CW triangles to CCW
    if (det > 0.0)
    {
        vA = _mm_mul_ps(vA, _mm_set1_ps(-1));
        vB = _mm_mul_ps(vB, _mm_set1_ps(-1));
        vAi = _mm_mullo_epi32(vAi, _mm_set1_epi32(-1));
        vBi = _mm_mullo_epi32(vBi, _mm_set1_epi32(-1));
        det = -det;
    }

    __m128 vC;
    // Finish triangle setup
    triangleSetupC(vX, vY, vA, vB, vC);

    // compute barycentric i and j
    // i = (A1x + B1y + C1)/det
    // j = (A2x + B2y + C2)/det
    __m128 vDet = _mm_set1_ps(det);
    __m128 vRecipDet = _mm_div_ps(_mm_set1_ps(1.0f), vDet); //_mm_rcp_ps(vDet);
    _mm_store_ss(&fakeDesc.recipDet, vRecipDet);

    _MM_EXTRACT_FLOAT(fakeDesc.I[0], vA, 1);
    _MM_EXTRACT_FLOAT(fakeDesc.I[1], vB, 1);
    _MM_EXTRACT_FLOAT(fakeDesc.I[2], vC, 1);
    _MM_EXTRACT_FLOAT(fakeDesc.J[0], vA, 2);
    _MM_EXTRACT_FLOAT(fakeDesc.J[1], vB, 2);
    _MM_EXTRACT_FLOAT(fakeDesc.J[2], vC, 2);

    OSALIGN(float, 16) oneOverW[4];

    if (DoPerspective)
    {
        _mm_store_ps(oneOverW, vRecipW);
        fakeDesc.OneOverW[0] = oneOverW[0] - oneOverW[2];
        fakeDesc.OneOverW[1] = oneOverW[1] - oneOverW[2];
        fakeDesc.OneOverW[2] = oneOverW[2];
    }

    // compute bary Z
    OSALIGN(float, 16) a[4];
    _mm_store_ps(a, vZ);
    fakeDesc.Z[0] = a[0] - a[2];
    fakeDesc.Z[1] = a[1] - a[2];
    fakeDesc.Z[2] = a[2];

#if KNOB_TILE_X_DIM != SIMD_TILE_X_DIM && KNOB_TILE_Y_DIM != SIMD_TILE_Y_DIM
    // Only need step for tile sizes > simd tile
    fakeDesc.zStepX = (SIMD_TILE_X_DIM * fakeDesc.Z[0] * fakeDesc.I[0] + SIMD_TILE_X_DIM * fakeDesc.Z[1] * fakeDesc.J[0]) * fakeDesc.recipDet;
    fakeDesc.zStepY = (SIMD_TILE_Y_DIM * fakeDesc.Z[0] * fakeDesc.I[1] + SIMD_TILE_Y_DIM * fakeDesc.Z[1] * fakeDesc.J[1]) * fakeDesc.recipDet;
#endif
    __m128i vAEdge0 = _mm_shuffle_epi32(vAi, _MM_SHUFFLE(0, 0, 0, 0));
    __m128i vAEdge1 = _mm_shuffle_epi32(vAi, _MM_SHUFFLE(1, 1, 1, 1));
    __m128i vAEdge2 = _mm_shuffle_epi32(vAi, _MM_SHUFFLE(2, 2, 2, 2));
    __m128i vBEdge0 = _mm_shuffle_epi32(vBi, _MM_SHUFFLE(0, 0, 0, 0));
    __m128i vBEdge1 = _mm_shuffle_epi32(vBi, _MM_SHUFFLE(1, 1, 1, 1));
    __m128i vBEdge2 = _mm_shuffle_epi32(vBi, _MM_SHUFFLE(2, 2, 2, 2));
    fakeDesc.vA = vAi;
    fakeDesc.vB = vBi;

    // Precompute quad step offsets
    const __m128i vQuadOffsetsXInt = _mm_set_epi32(1, 0, 1, 0);
    const __m128i vQuadOffsetsYInt = _mm_set_epi32(1, 1, 0, 0);

    __m128i vStepX0 = _mm_mullo_epi32(vAEdge0, vQuadOffsetsXInt);
    __m128i vStepX1 = _mm_mullo_epi32(vAEdge1, vQuadOffsetsXInt);
    __m128i vStepX2 = _mm_mullo_epi32(vAEdge2, vQuadOffsetsXInt);

    __m128i vStepY0 = _mm_mullo_epi32(vBEdge0, vQuadOffsetsYInt);
    __m128i vStepY1 = _mm_mullo_epi32(vBEdge1, vQuadOffsetsYInt);
    __m128i vStepY2 = _mm_mullo_epi32(vBEdge2, vQuadOffsetsYInt);

    fakeDesc.vStepQuad0 = _mm_add_epi32(vStepX0, vStepY0);
    fakeDesc.vStepQuad1 = _mm_add_epi32(vStepX1, vStepY1);
    fakeDesc.vStepQuad2 = _mm_add_epi32(vStepX2, vStepY2);

    // Precompute tile step offsets
    const __m128i vTileOffsetsXInt = _mm_set_epi32(KNOB_TILE_X_DIM - 1, 0, KNOB_TILE_X_DIM - 1, 0);
    const __m128i vTileOffsetsYInt = _mm_set_epi32(KNOB_TILE_Y_DIM - 1, KNOB_TILE_Y_DIM - 1, 0, 0);

    // Calc bounding box of triangle
    OSALIGN(BBOX, 16) bbox;
    calcBoundingBoxInt(vXi, vYi, bbox);

    // convert to tiles
    bbox.left >>= KNOB_TILE_X_DIM_SHIFT + FIXED_POINT_WIDTH;
    bbox.right >>= KNOB_TILE_X_DIM_SHIFT + FIXED_POINT_WIDTH;
    bbox.top >>= KNOB_TILE_Y_DIM_SHIFT + FIXED_POINT_WIDTH;
    bbox.bottom >>= KNOB_TILE_Y_DIM_SHIFT + FIXED_POINT_WIDTH;

    // intersect with scissor/viewport
    bbox.left = std::max(bbox.left, state.scissorInTiles.left);
    bbox.right = std::min(bbox.right, state.scissorInTiles.right);
    bbox.top = std::max(bbox.top, state.scissorInTiles.top);
    bbox.bottom = std::min(bbox.bottom, state.scissorInTiles.bottom);

    fakeDesc.needScissor = false;
    fakeDesc.triFlags = knobDesc.triFlags;
    fakeDesc.pInterpBuffer = knobDesc.pInterpBuffer;

    TRIANGLE_DESC &desc = fakeDesc;

    SWR_PIXELOUTPUT pOut;
    pOut.pRenderTargets[0] = state.pRenderTargets[SWR_ATTACHMENT_COLOR0]->pTileData;
    pOut.pRenderTargets[1] = state.pRenderTargets[SWR_ATTACHMENT_DEPTH]->pTileData;

    // further constrain backend to intersecting bounding box of macro tile and scissored triangle bbox
    INT macroBoxLeft = knobDesc.triFlags.macroX * state.scissorMacroWidthInTiles;
    INT macroBoxRight = macroBoxLeft + state.scissorMacroWidthInTiles - 1;
    INT macroBoxTop = knobDesc.triFlags.macroY * state.scissorMacroHeightInTiles;
    INT macroBoxBottom = macroBoxTop + state.scissorMacroHeightInTiles - 1;

    OSALIGN(BBOX, 16) intersect = bbox;
    intersect.left = std::max(bbox.left, macroBoxLeft);
    intersect.top = std::max(bbox.top, macroBoxTop);
    intersect.right = std::min(bbox.right, macroBoxRight);
    intersect.bottom = std::min(bbox.bottom, macroBoxBottom);

    assert(intersect.left <= intersect.right && intersect.top <= intersect.bottom && intersect.left >= 0 && intersect.right >= 0 && intersect.top >= 0 && intersect.bottom >= 0);

    RDTSC_STOP(BETriangleSetup, 0, pDC->drawId);

    // update triangle desc
    UINT tileX = intersect.left;
    UINT tileY = intersect.top;
    UINT maxTileX = intersect.right;
    UINT maxTileY = intersect.bottom;
    UINT numTilesX = maxTileX - tileX + 1;
    UINT numTilesY = maxTileY - tileY + 1;

    if (numTilesX == 0 || numTilesY == 0)
    {
        RDTSC_EVENT(BEEmptyTriangle, 1, 0);
        return;
    }

    RDTSC_START(BEStepSetup);
    // step to pixel center of top-left pixel of the triangle bbox
    int x = (intersect.left << (KNOB_TILE_X_DIM_SHIFT + FIXED_POINT_WIDTH)) + FIXED_POINT_SIZE / 2;
    int y = (intersect.top << (KNOB_TILE_Y_DIM_SHIFT + FIXED_POINT_WIDTH)) + FIXED_POINT_SIZE / 2;

    __m128i vTopLeftX = _mm_set1_epi32(x);
    __m128i vTopLeftY = _mm_set1_epi32(y);

    // evaluate edge equations at top-left pixel using 64bit math
    // all other evaluations will be 32bit steps from it
    // small triangles could skip this and do all 32bit math
    // edge 0
    //
    // line = Ax + By + C
    // line = A(x - x0) + B(y - y0)
    // line = A(x0+dX) + B(y0+dY) + C = Ax0 + AdX + By0 + BdY + c = AdX + BdY

    // edge 0 and 1
    // edge0 = A0(x - x0) + B0(y - y0)
    // edge1 = A1(x - x1) + B1(y - y1)
    __m128i vDeltaX = _mm_sub_epi32(vTopLeftX, vXi);
    __m128i vDeltaY = _mm_sub_epi32(vTopLeftY, vYi);

    __m128i vEdge0, vEdge1, vEdge2;
    if (Use32BitMath)
    {
        __m128i vAX = _mm_mullo_epi32(desc.vA, vDeltaX);
        __m128i vBY = _mm_mullo_epi32(desc.vB, vDeltaY);
        __m128i vEdge = _mm_add_epi32(vAX, vBY);
        vEdge = _mm_srai_epi32(vEdge, FIXED_POINT_WIDTH);
        vEdge = adjustTopLeftRuleInt(desc.vA, desc.vB, vEdge);
        vEdge0 = _mm_shuffle_epi32(vEdge, _MM_SHUFFLE(0, 0, 0, 0));
        vEdge1 = _mm_shuffle_epi32(vEdge, _MM_SHUFFLE(1, 1, 1, 1));
        vEdge2 = _mm_shuffle_epi32(vEdge, _MM_SHUFFLE(2, 2, 2, 2));
    }
    else
    {
        // shuffle 0 and 1 edges for multiply
        __m128i vDX01 = _mm_shuffle_epi32(vDeltaX, _MM_SHUFFLE(1, 1, 0, 0));
        __m128i vDY01 = _mm_shuffle_epi32(vDeltaY, _MM_SHUFFLE(1, 1, 0, 0));
        __m128i vA01 = _mm_shuffle_epi32(desc.vA, _MM_SHUFFLE(1, 1, 0, 0));
        __m128i vB01 = _mm_shuffle_epi32(desc.vB, _MM_SHUFFLE(1, 1, 0, 0));

        // multiply (result will be 64bits)
        __m128i vAX01 = _mm_mul_epi32(vDX01, vA01);
        __m128i vBY01 = _mm_mul_epi32(vDY01, vB01);

        // add
        __m128i vEdge01 = _mm_add_epi64(vAX01, vBY01);

        // truncate
        INT64 edge0 = _MM_EXTRACT_EPI64(vEdge01, 0);
        INT64 edge1 = _MM_EXTRACT_EPI64(vEdge01, 1);
        edge0 >>= FIXED_POINT_WIDTH;
        edge1 >>= FIXED_POINT_WIDTH;
        vEdge01 = _MM_INSERT_EPI64(vEdge01, edge0, 0);
        vEdge01 = _MM_INSERT_EPI64(vEdge01, edge1, 1);

        // top left rule
        vEdge01 = adjustTopLeftRuleInt(vA01, vB01, vEdge01);

        vEdge0 = _mm_shuffle_epi32(vEdge01, _MM_SHUFFLE(0, 0, 0, 0));
        vEdge1 = _mm_shuffle_epi32(vEdge01, _MM_SHUFFLE(2, 2, 2, 2));

        // edge 2, already in position, no shufs needed
        __m128i vAX2 = _mm_mul_epi32(vDeltaX, desc.vA);
        __m128i vBY2 = _mm_mul_epi32(vDeltaY, desc.vB);

        vEdge2 = _mm_add_epi64(vAX2, vBY2);
        edge0 = _MM_EXTRACT_EPI64(vEdge2, 1);
        edge0 >>= FIXED_POINT_WIDTH;
        vEdge2 = _MM_INSERT_EPI64(vEdge2, edge0, 1);

        // top left rule
        vEdge2 = adjustTopLeftRuleInt(desc.vA, desc.vB, vEdge2);
        vEdge2 = _mm_shuffle_epi32(vEdge2, _MM_SHUFFLE(2, 2, 2, 2));
    }

    // compute step to the next tile
    __m128i vNextXTile = _mm_set1_epi32(KNOB_TILE_X_DIM);
    __m128i vNextYTile = _mm_set1_epi32(KNOB_TILE_Y_DIM);
    __m128i vStep0X = _mm_mullo_epi32(vAEdge0, vNextXTile);
    __m128i vStep0Y = _mm_mullo_epi32(vBEdge0, vNextYTile);
    __m128i vStep1X = _mm_mullo_epi32(vAEdge1, vNextXTile);
    __m128i vStep1Y = _mm_mullo_epi32(vBEdge1, vNextYTile);
    __m128i vStep2X = _mm_mullo_epi32(vAEdge2, vNextXTile);
    __m128i vStep2Y = _mm_mullo_epi32(vBEdge2, vNextYTile);

    vAEdge0 = _mm_mullo_epi32(vAEdge0, vTileOffsetsXInt);
    vBEdge0 = _mm_mullo_epi32(vBEdge0, vTileOffsetsYInt);
    vEdge0 = _mm_add_epi32(vEdge0, _mm_add_epi32(vAEdge0, vBEdge0));

    vAEdge1 = _mm_mullo_epi32(vAEdge1, vTileOffsetsXInt);
    vBEdge1 = _mm_mullo_epi32(vBEdge1, vTileOffsetsYInt);
    vEdge1 = _mm_add_epi32(vEdge1, _mm_add_epi32(vAEdge1, vBEdge1));

    vAEdge2 = _mm_mullo_epi32(vAEdge2, vTileOffsetsXInt);
    vBEdge2 = _mm_mullo_epi32(vBEdge2, vTileOffsetsYInt);
    vEdge2 = _mm_add_epi32(vEdge2, _mm_add_epi32(vAEdge2, vBEdge2));

    RDTSC_STOP(BEStepSetup, 0, pDC->drawId);

    UINT tY = tileY;
    UINT tX = tileX;
    UINT maxY = maxTileY;
    UINT maxX = maxTileX;

    for (UINT tileY = tY; tileY <= maxY; ++tileY)
    {
        desc.tileY = tileY;
        __m128i vStartOfRowEdge0 = vEdge0;
        __m128i vStartOfRowEdge1 = vEdge1;
        __m128i vStartOfRowEdge2 = vEdge2;

        for (UINT tileX = tX; tileX <= maxX; ++tileX)
        {
            desc.tileX = tileX;
            int mask0 = _mm_movemask_ps(_mm_castsi128_ps(vEdge0));
            int mask1 = _mm_movemask_ps(_mm_castsi128_ps(vEdge1));
            int mask2 = _mm_movemask_ps(_mm_castsi128_ps(vEdge2));

#if KNOB_TILE_X_DIM == 2 && KNOB_TILE_Y_DIM == 2
            desc.coverageMask = mask0 & mask1 & mask2;

#ifdef KNOB_TOSS_RS
            gToss = desc.coverageMask;
#else
            if (!desc.coverageMask)
            {
                RDTSC_EVENT(BETrivialReject, 1, 0);
            }
            else
            {
                RDTSC_START(BEPixelShader);
                pDC->state.pfnPixelFunc(desc, pOut);
                RDTSC_STOP(BEPixelShader, 0, 0);
            }
#endif

#else // KNOB_TILE_DIM != 2

            // trivial reject, at least one edge has all 4 corners outside
            bool trivialReject = (!(mask0 && mask1 && mask2)) ? true : false;

            if (!trivialReject)
            {
                // trivial accept mask
                desc.coverageMask = 0xffffffffffffffffULL;
                if (!desc.needScissor && (mask0 & mask1 & mask2) == 0xf)
                {
                    // trivial accept, all 4 corners of all 3 edges are negative
                    RDTSC_EVENT(BETrivialAccept, 1, 0);
                }
                else
                {
                    // not trivial accept or reject, must rasterize full tile
                    RDTSC_START(BERasterizePartial);
                    desc.coverageMask = rasterizePartialTile(pDC, tileX, tileY, desc, vEdge0, vEdge1, vEdge2);
                    RDTSC_STOP(BERasterizePartial, 0, 0);
                }

#ifdef KNOB_TOSS_RS
                gToss = coverageMask;
#else
                if (desc.coverageMask)
                {
                    RDTSC_START(BEPixelShader);
                    pDC->state.pfnPixelFunc(desc, pOut);
                    RDTSC_STOP(BEPixelShader, 0, 0);
                }
#endif
            }
            else
            {
                RDTSC_EVENT(BETrivialReject, 1, 0);
            }
#endif

            // step to the next tile in X
            vEdge0 = _mm_add_epi32(vEdge0, vStep0X);
            vEdge1 = _mm_add_epi32(vEdge1, vStep1X);
            vEdge2 = _mm_add_epi32(vEdge2, vStep2X);
        }

        // step to the next tile in Y
        vEdge0 = _mm_add_epi32(vStartOfRowEdge0, vStep0Y);
        vEdge1 = _mm_add_epi32(vStartOfRowEdge1, vStep1Y);
        vEdge2 = _mm_add_epi32(vStartOfRowEdge2, vStep2Y);
    }
}

template <bool DoPerspective>
void RasterizeOneTileTriangle(DRAW_CONTEXT *pDC, const TRIANGLE_WORK_DESC &knobDesc, UINT macroTile)
{
#ifdef KNOB_TOSS_BIN_TRIS
    return;
#endif

    RDTSC_START(BETriangleSetup);
    const API_STATE &state = pDC->state;

    OSALIGN(TRIANGLE_DESC, 16) fakeDesc;
    fakeDesc.widthInBytes = state.pRenderTargets[SWR_ATTACHMENT_COLOR0]->widthInBytes;
    fakeDesc.pDC = pDC;
    fakeDesc.pTextureViews = &state.aTextureViews[SHADER_PIXEL][0];
    fakeDesc.pSamplers = &state.aSamplers[SHADER_PIXEL][0];
    fakeDesc.pConstants = state.pVSConstantBufferAlloc->pData;

    SWR_PIXELOUTPUT pOut;
    pOut.pRenderTargets[0] = state.pRenderTargets[SWR_ATTACHMENT_COLOR0]->pTileData;
    pOut.pRenderTargets[1] = state.pRenderTargets[SWR_ATTACHMENT_DEPTH]->pTileData;

    __m128 vX, vY, vZ, vRecipW;
    vX = _mm_load_ps(knobDesc.pTriBuffer);
    vY = _mm_load_ps(knobDesc.pTriBuffer + 4);
    vZ = _mm_load_ps(knobDesc.pTriBuffer + 8);

    if (DoPerspective)
    {
        vRecipW = _mm_load_ps(knobDesc.pTriBuffer + 12);
    }

    // convert to fixed point
    __m128i vXi = fpToFixedPoint(vX);
    __m128i vYi = fpToFixedPoint(vY);

    // triangle setup
    __m128 vA, vB;
    triangleSetupAB(vX, vY, vA, vB);

    __m128i vAi, vBi;
    triangleSetupABInt(vXi, vYi, vAi, vBi);

    // determinant
    float det = calcDeterminantInt(vAi, vBi);

    // Convert CW triangles to CCW
    if (det > 0.0)
    {
        vA = _mm_mul_ps(vA, _mm_set1_ps(-1));
        vB = _mm_mul_ps(vB, _mm_set1_ps(-1));
        vAi = _mm_mullo_epi32(vAi, _mm_set1_epi32(-1));
        vBi = _mm_mullo_epi32(vBi, _mm_set1_epi32(-1));
        det = -det;
    }

    __m128 vC;
    // Finish triangle setup
    triangleSetupC(vX, vY, vA, vB, vC);

    // compute barycentric i and j
    // i = (A1x + B1y + C1)/det
    // j = (A2x + B2y + C2)/det
    __m128 vDet = _mm_set1_ps(det);
    __m128 vRecipDet = _mm_div_ps(_mm_set1_ps(1.0f), vDet); //_mm_rcp_ps(vDet);
    _mm_store_ss(&fakeDesc.recipDet, vRecipDet);

    _MM_EXTRACT_FLOAT(fakeDesc.I[0], vA, 1);
    _MM_EXTRACT_FLOAT(fakeDesc.I[1], vB, 1);
    _MM_EXTRACT_FLOAT(fakeDesc.I[2], vC, 1);
    _MM_EXTRACT_FLOAT(fakeDesc.J[0], vA, 2);
    _MM_EXTRACT_FLOAT(fakeDesc.J[1], vB, 2);
    _MM_EXTRACT_FLOAT(fakeDesc.J[2], vC, 2);

    OSALIGN(float, 16) oneOverW[4];

    if (DoPerspective)
    {
        _mm_store_ps(oneOverW, vRecipW);
        fakeDesc.OneOverW[0] = oneOverW[0] - oneOverW[2];
        fakeDesc.OneOverW[1] = oneOverW[1] - oneOverW[2];
        fakeDesc.OneOverW[2] = oneOverW[2];
    }

    // compute bary Z
    OSALIGN(float, 16) a[4];
    _mm_store_ps(a, vZ);
    fakeDesc.Z[0] = a[0] - a[2];
    fakeDesc.Z[1] = a[1] - a[2];
    fakeDesc.Z[2] = a[2];

    fakeDesc.vA = vAi;
    fakeDesc.vB = vBi;

    // Calc bounding box of triangle
    OSALIGN(BBOX, 16) bbox;
    calcBoundingBoxInt(vXi, vYi, bbox);

    // convert to tiles
    bbox.left >>= KNOB_TILE_X_DIM_SHIFT + FIXED_POINT_WIDTH;
    bbox.right >>= KNOB_TILE_X_DIM_SHIFT + FIXED_POINT_WIDTH;
    bbox.top >>= KNOB_TILE_Y_DIM_SHIFT + FIXED_POINT_WIDTH;
    bbox.bottom >>= KNOB_TILE_Y_DIM_SHIFT + FIXED_POINT_WIDTH;

    // intersect with scissor/viewport
    bbox.left = std::max(bbox.left, state.scissorInTiles.left);
    bbox.top = std::max(bbox.top, state.scissorInTiles.top);

    // constrain backend to intersecting bounding box of macro tile and triangle bbox
    INT macroBoxLeft = knobDesc.triFlags.macroX * state.scissorMacroWidthInTiles;
    INT macroBoxTop = knobDesc.triFlags.macroY * state.scissorMacroHeightInTiles;

    OSALIGN(BBOX, 16) intersect = bbox;
    intersect.left = std::max(bbox.left, macroBoxLeft);
    intersect.top = std::max(bbox.top, macroBoxTop);

    fakeDesc.needScissor = false;
    fakeDesc.triFlags = knobDesc.triFlags;
    fakeDesc.pInterpBuffer = knobDesc.pInterpBuffer;
    fakeDesc.pDC = pDC;
    fakeDesc.tileX = intersect.left;
    fakeDesc.tileY = intersect.top;

    TRIANGLE_DESC &desc = fakeDesc;

    RDTSC_STOP(BETriangleSetup, 0, pDC->drawId);

    desc.coverageMask = knobDesc.triFlags.coverageMask;

    RDTSC_START(BEPixelShader);
    pDC->state.pfnPixelFunc(desc, pOut);
    RDTSC_STOP(BEPixelShader, 1, 0);
}

void rastLargeTri(DRAW_CONTEXT *pDC, UINT macroTile, void *pData)
{
    RDTSC_START(BERasterizeLargeTri);
    TRIANGLE_WORK_DESC *pDesc = (TRIANGLE_WORK_DESC *)pData;
    RasterizeTriangle<false, true>(pDC, *pDesc, macroTile);
    RDTSC_STOP(BERasterizeLargeTri, 0, pDC->drawId);
};

void extractTri(BYTE *pBuffer, UINT index, float *triBuffer)
{
    UINT offset = sizeof(float) * index;
    // x
    triBuffer[0] = *(float *)(pBuffer + offset);
    triBuffer[1] = *(float *)(pBuffer + offset + sizeof(simdvector));
    triBuffer[2] = *(float *)(pBuffer + offset + sizeof(simdvector) * 2);
    triBuffer[3] = 0;

    // y
    offset += sizeof(simdscalar);
    triBuffer[4] = *(float *)(pBuffer + offset);
    triBuffer[5] = *(float *)(pBuffer + offset + sizeof(simdvector));
    triBuffer[6] = *(float *)(pBuffer + offset + sizeof(simdvector) * 2);
    triBuffer[7] = 0;

    // z
    offset += sizeof(simdscalar);
    triBuffer[8] = *(float *)(pBuffer + offset);
    triBuffer[9] = *(float *)(pBuffer + offset + sizeof(simdvector));
    triBuffer[10] = *(float *)(pBuffer + offset + sizeof(simdvector) * 2);
    triBuffer[11] = 0;

    // w
    offset += sizeof(simdscalar);
    triBuffer[12] = *(float *)(pBuffer + offset);
    triBuffer[13] = *(float *)(pBuffer + offset + sizeof(simdvector));
    triBuffer[14] = *(float *)(pBuffer + offset + sizeof(simdvector) * 2);
    triBuffer[15] = 0;
}

void rastSmallTri(DRAW_CONTEXT *pDC, UINT macroTile, void *pData)
{
    RDTSC_START(BERasterizeSmallTri);

#if KNOB_VERTICALIZED_BINNER
    OSALIGNSIMD(float)triBuffer[4 * 4];
    VERTICAL_TRIANGLE_DESC *pDesc = (VERTICAL_TRIANGLE_DESC *)pData;
    TRIANGLE_WORK_DESC triDesc;
    triDesc.pDC = pDesc->pDC;
    triDesc.pTriBuffer = &triBuffer[0];
    BYTE *pBuffer = (BYTE *)pDesc->pTriBuffer;
    for (UINT i = 0; i < pDesc->numTris; ++i)
    {
        extractTri(pBuffer, i, &triBuffer[0]);
        triDesc.pInterpBuffer = pDesc->pInterpBuffer[i];
        RasterizeTriangle2x2Tile<true, true>(pDC, triDesc, macroTile);
    }
#else
    TRIANGLE_WORK_DESC *pDesc = (TRIANGLE_WORK_DESC *)pData;
    RasterizeTriangle<true, true>(pDC, *pDesc, macroTile);
#endif

    RDTSC_STOP(BERasterizeSmallTri, 0, pDC->drawId);
};

void rastOneTileTri(DRAW_CONTEXT *pDC, UINT macroTile, void *pData)
{
    RDTSC_START(BERasterizeOneTileTri);
    TRIANGLE_WORK_DESC *pDesc = (TRIANGLE_WORK_DESC *)pData;
    RasterizeOneTileTriangle<true>(pDC, *pDesc, macroTile);
    RDTSC_STOP(BERasterizeOneTileTri, 0, pDC->drawId);
};
