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

#include "api.h"
#include "frontend.h"
#include "backend.h"
#include "context.h"
#include "rdtsc.h"
#include "rasterizer.h"
#include "utils.h"
#include "threads.h"
#include "pa.h"
#include "clip.h"
#include "tilemgr.h"

static UINT gTileColors[] = { 0xff111111, 0xffaaaaaa };

#if 0
struct BinData
{
	UINT			numPacked;
	VERT_BE_WORK	work;
	FLOAT		   *pTriBuffer;
};

struct BinPacker
{
	DRAW_CONTEXT *pDC;

	BinData macroBinData[NUM_MACRO_TILES];

	BinPacker() {}

	BinPacker(DRAW_CONTEXT *pDC)
	{
		this->pDC = pDC;

		for (UINT i = 0; i < NUM_MACRO_TILES; ++i)
		{
			BinData *pBinData = &macroBinData[i];
			pBinData->numPacked			= 0;
			pBinData->work.type			= DRAW;
			pBinData->work.pfnWork		= rastSmallTri;
			pBinData->pTriBuffer = (float*)pDC->arena.AllocAligned(sizeof(simdvector)*3, 32);
		}
	}

	void addTriangle(UINT macroTile, simdvector tris[3], UINT lane, float *pInterpBuffer)
	{
		RDTSC_START(FEBinPacker);
		BinData *pBinData = &macroBinData[macroTile];

		UINT &curLane = pBinData->numPacked;

		simdvector* pTriBuffer = (simdvector*)pBinData->pTriBuffer;
		_simdvec_mov(pTriBuffer[0], curLane, tris[0], lane);
		_simdvec_mov(pTriBuffer[1], curLane, tris[1], lane);
		_simdvec_mov(pTriBuffer[2], curLane, tris[2], lane);

		pBinData->work.desc.tri.pInterpBuffer[curLane] = pInterpBuffer;

		curLane++;
		if (curLane == KNOB_VS_SIMD_WIDTH)
		{
			flush(macroTile);
			curLane = 0;
		}
		RDTSC_STOP(FEBinPacker, 0, pDC->drawId);
	}

	// flush a given macroTile out to the BE
	void flush(UINT macroTile)
	{
#if KNOB_VERTICALIZED_BINNER
		BinData *pBinData = &macroBinData[macroTile];

		pBinData->work.desc.tri.numTris		= pBinData->numPacked;
		pBinData->work.desc.tri.pTriBuffer	= pBinData->pTriBuffer;
		pDC->totalWorkItemsProducedFE++;
		pDC->totalWorkItemsFE[macroTile]++;
		pDC->drawFifo[macroTile].enqueue_try_nosync(&pBinData->work);

		pBinData->pTriBuffer = (float*)pDC->arena.AllocAligned(sizeof(simdvector)*3, 32);
#endif
	}

	// flush all macrotiles out
	void flush()
	{
		for (UINT i = 0; i < NUM_MACRO_TILES; ++i)
		{
			BinData *pBinData = &macroBinData[i];
			if (pBinData->numPacked)
			{
				flush(i);
				pBinData->numPacked = 0;
			}
		}
	}

};
#endif

void ProcessClear(SWR_CONTEXT *pContext, DRAW_CONTEXT *pDC, void *pUserData)
{
    CLEAR_DESC *pClear = (CLEAR_DESC *)pUserData;
    MacroTileMgr *pTileMgr = pDC->pTileMgr;

    // queue a clear to each macro tile
    // compute macro tile bounds for the current scissor/viewport
    UINT macroTileLeft = pDC->state.scissorInTiles.left / pDC->state.scissorMacroWidthInTiles;
    UINT macroTileRight = pDC->state.scissorInTiles.right / pDC->state.scissorMacroWidthInTiles;
    UINT macroTileTop = pDC->state.scissorInTiles.top / pDC->state.scissorMacroHeightInTiles;
    UINT macroTileBottom = pDC->state.scissorInTiles.bottom / pDC->state.scissorMacroHeightInTiles;

#if KNOB_VERTICALIZED_BINNER
    VERT_BE_WORK work;
#else
    BE_WORK work;
#endif
    work.pfnWork = ProcessClearBE;
    work.desc.clear = *pClear;

    UINT curColor = 0;
    for (UINT y = macroTileTop; y <= macroTileBottom; ++y)
    {
        for (UINT x = macroTileLeft; x <= macroTileRight; ++x)
        {

#ifdef KNOB_VISUALIZE_MACRO_TILES
            fakeWork.desc.clear.clearRTColor = gTileColors[curColor];
            curColor ^= 1;
#endif
            pTileMgr->enqueue(x, y, &work);
        }
    }

    _ReadWriteBarrier();
    pDC->doneFE = true;
}

void ProcessPresent(SWR_CONTEXT *pContext, DRAW_CONTEXT *pDC, void *pUserData)
{
    RDTSC_START(FEProcessPresent);
    STORE_DESC *pStore = (STORE_DESC *)pUserData;
    MacroTileMgr *pTileMgr = pDC->pTileMgr;

    // queue a store to each macro tile
    // compute macro tile bounds for the current render target
    RENDERTARGET *pRT = pDC->state.pRenderTargets[SWR_ATTACHMENT_COLOR0];

    UINT macroWidth = pTileMgr->getTileWidth();
    UINT macroHeight = pTileMgr->getTileHeight();

    UINT numMacroTilesX = (pRT->apiWidth + (macroWidth - 1)) / macroWidth;
    UINT numMacroTilesY = (pRT->apiHeight + (macroHeight - 1)) / macroHeight;

// store tiles
#if KNOB_VERTICALIZED_BINNER
    VERT_BE_WORK work;
#else
    BE_WORK work;
#endif
    work.pfnWork = ProcessStoreTileBE;
    work.desc.store.pData = pStore->pData;
    work.desc.store.pitch = pStore->pitch;

    for (UINT x = 0; x < numMacroTilesX; ++x)
    {
        for (UINT y = 0; y < numMacroTilesY; ++y)
        {
            pTileMgr->enqueue(x, y, &work);
        }
    }

    _ReadWriteBarrier();
    pDC->doneFE = true;

    RDTSC_STOP(FEProcessPresent, 0, pDC->drawId);
}

void ProcessCopy(SWR_CONTEXT *pContext, DRAW_CONTEXT *pDC, void *pUserData)
{
    COPY_DESC *pCopy = (COPY_DESC *)pUserData;
    RENDERTARGET *pRT = pDC->state.pRenderTargets[pCopy->rt];

    // determine range of macro tiles intersecting the copy rect
    UINT mtWidth = pDC->pTileMgr->getTileWidth();
    UINT mtHeight = pDC->pTileMgr->getTileHeight();
    UINT leftMT = pCopy->srcX / mtWidth;
    UINT rightMT = (pCopy->srcX + pCopy->width - 1) / mtWidth;
    UINT topMT = pCopy->srcY / mtHeight;
    UINT botMT = (pCopy->srcY + pCopy->height - 1) / mtHeight;

#if KNOB_VERTICALIZED_BINNER
    VERT_BE_WORK work;
#else
    BE_WORK work;
#endif
    work.pfnWork = ProcessCopyBE;
    work.desc.copy = *pCopy;

    for (UINT y = topMT; y <= botMT; ++y)
    {
        for (UINT x = leftMT; x <= rightMT; ++x)
        {
            pDC->pTileMgr->enqueue(x, y, &work);
        }
    }

    _ReadWriteBarrier();
    pDC->doneFE = true;
}

INLINE
__m128 fixedPointToFP(const __m128i vIn)
{
    __m128 vFP = _mm_cvtepi32_ps(vIn);
    vFP = _mm_mul_ps(vFP, _mm_set1_ps(1.0 / FIXED_POINT_SIZE));
    return vFP;
}

template <bool CullAndClip>
void BinTriangle(DRAW_CONTEXT *pDC, __m128 &vX, __m128 &vY, __m128 &vZ, __m128 &vW, UINT index[3], const float *pAttribs, UINT numAttribs);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////// VERTICAL ///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if KNOB_VERTICALIZED_FE
void BinTriangles(DRAW_CONTEXT *pDC, PA_STATE &pa, simdvector tri[3], UINT numTris);

void ProcessDraw(SWR_CONTEXT *pContext, DRAW_CONTEXT *pDC, void *pUserData)
{
    DRAW_WORK &work = *(DRAW_WORK *)pUserData;

#ifdef KNOB_TOSS_QUEUE_FE
    pDC->doneFE = 1;
    return;
#endif

    RDTSC_START(FEProcessDraw);
    SWR_FETCH_INFO fetchInfo = { { 0 }, 0, 0 };

    fetchInfo.pConstants = pDC->state.pFSConstantBufferAlloc->pData;

    for (UINT i = 0; i < KNOB_NUM_STREAMS; ++i)
    {
        if (pDC->state.ppVertexBufferAlloc[i])
        {
            fetchInfo.ppStreams[i] = (float *)pDC->state.ppVertexBufferAlloc[i]->pData;
        }
    }

    VERTEXINPUT vin;
    vin.pConstants = pDC->state.pVSConstantBufferAlloc->pData;

    UINT numPrims = NumElementsGivenIndices(pDC->state.topology, work.numVerts);
    INT i = work.startVertex;
    INT endVertex = work.startVertex + work.numVerts;

    PA_STATE pa(pDC, numPrims);
#if KNOB_VERTICALIZED_BINNER
    BinPacker bp(pDC);
#endif

    while (PaHasWork(pa))
    {
        // PaGetNextVsOutput currently has the side effect of updating some PA state machine state.
        // So we need to keep this outside of (i < endVertex) check.
        VERTEXOUTPUT &vout = PaGetNextVsOutput(pa);

        if (i < endVertex)
        {
            fetchInfo.pIndices = &i;

            // 1. Execute FS/VS for a single SIMD.
            RDTSC_START(FEFetchShader);
            pDC->state.pfnFetchFunc(fetchInfo, vin);
            RDTSC_STOP(FEFetchShader, 0, 0);

#ifndef KNOB_TOSS_FETCH
            RDTSC_START(FEVertexShader);
            pDC->state.pfnVertexFunc(vin, vout); // Call vertex shader
            RDTSC_STOP(FEVertexShader, 0, 0);
#endif
        }

        // 2. Assemble primitives given the last two SIMD.
        do
        {
            simdvector tri[3];
            // PaAssemble returns false if there is not enough verts to assemble.
            RDTSC_START(FEPAAssemble);
            bool assemble = PaAssemble(pa, VS_SLOT_POSITION, tri);
            RDTSC_STOP(FEPAAssemble, 1, 0);
#ifndef KNOB_TOSS_FETCH
#ifndef KNOB_TOSS_VS
            if (assemble)
            {
                RDTSC_START(FEBinTriangles);
                BinTriangles(pDC, pa, tri, PaNumTris(pa));
                RDTSC_STOP(FEBinTriangles, PaNumTris(pa), pDC->drawId);
            }
#endif
#endif
        } while (PaNextPrim(pa));

        i += KNOB_VS_SIMD_WIDTH;
    }

#if KNOB_VERTICALIZED_BINNER
    // flush remaining triangles to the backend
    bp.flush();
#endif

    pDC->doneFE = true;
    RDTSC_STOP(FEProcessDraw, numPrims, pDC->drawId);
}

void ProcessDrawIndexed(SWR_CONTEXT *pContext, DRAW_CONTEXT *pDC, void *pUserData)
{
    DRAW_WORK &work = *(DRAW_WORK *)pUserData;

#ifdef KNOB_TOSS_QUEUE_FE
    pDC->doneFE = 1;
    return;
#endif

    RDTSC_START(FEProcessDraw);
    SWR_FETCH_INFO fetchInfo = { { 0 }, 0, 0 };

    fetchInfo.pConstants = pDC->state.pFSConstantBufferAlloc->pData;

    for (UINT i = 0; i < KNOB_NUM_STREAMS; ++i)
    {
        if (pDC->state.ppVertexBufferAlloc[i])
        {
            fetchInfo.ppStreams[i] = (float *)pDC->state.ppVertexBufferAlloc[i]->pData;
        }
    }

    // @todo - jit this or have templated functions on index type
    int indexSize = 0;
    switch (work.type)
    {
    case SWR_TYPE_UINT32:
        indexSize = sizeof(UINT);
        break;
    case SWR_TYPE_UINT16:
        indexSize = sizeof(short);
        break;
    default:
        assert(0);
    }

    VERTEXINPUT vin;
    vin.pConstants = pDC->state.pVSConstantBufferAlloc->pData;

    UINT numPrims = NumElementsGivenIndices(pDC->state.topology, work.numVerts);
    INT i = work.startVertex;
    INT endVertex = work.startVertex + work.numVerts;

    PA_STATE pa(pDC, numPrims);
#if KNOB_VERTICALIZED_BINNER
    BinPacker bp(pDC);
#endif

    fetchInfo.pIndices = work.pIB;

    while (PaHasWork(pa))
    {
        // PaGetNextVsOutput currently has the side effect of updating some PA state machine state.
        // So we need to keep this outside of (i < endVertex) check.
        VERTEXOUTPUT &vout = PaGetNextVsOutput(pa);

        if (i < endVertex)
        {
            // 1. Execute FS/VS for a single SIMD.
            RDTSC_START(FEFetchShader);
            pDC->state.pfnFetchFunc(fetchInfo, vin);
            RDTSC_STOP(FEFetchShader, 0, 0);

#ifndef KNOB_TOSS_FETCH
            RDTSC_START(FEVertexShader);
            pDC->state.pfnVertexFunc(vin, vout); // Call vertex shader
            RDTSC_STOP(FEVertexShader, 0, 0);
#endif
        }

        // 2. Assemble primitives given the last two SIMD.
        do
        {
            simdvector tri[3];
            // PaAssemble returns false if there is not enough verts to assemble.
            RDTSC_START(FEPAAssemble);
            bool assemble = PaAssemble(pa, VS_SLOT_POSITION, tri);
            RDTSC_STOP(FEPAAssemble, 1, 0);
#ifndef KNOB_TOSS_FETCH
#ifndef KNOB_TOSS_VS
            if (assemble)
            {
                RDTSC_START(FEBinTriangles);
                BinTriangles(pDC, pa, tri, PaNumTris(pa));
                RDTSC_STOP(FEBinTriangles, PaNumTris(pa), pDC->drawId);
            }
#endif
#endif
        } while (PaNextPrim(pa));

        i += KNOB_VS_SIMD_WIDTH;
        fetchInfo.pIndices = (int *)((BYTE *)fetchInfo.pIndices + KNOB_VS_SIMD_WIDTH * indexSize);
    }

#if KNOB_VERTICALIZED_BINNER
    // flush remaining triangles to the backend
    bp.flush();
#endif

    pDC->doneFE = true;
    RDTSC_STOP(FEProcessDraw, numPrims, pDC->drawId);
}
static const UINT triangleMask[] = {
    0x0,
    0x1,
    0x3,
    0x7,
    0xf,
    0x1f,
    0x3f,
    0x7f,
    0xff
};

INLINE
void computeClipCodes(const API_STATE &state, const simdvector &vertex, simdscalar &clipCodes)
{
    clipCodes = _simd_setzero_ps();

    // -w
    simdscalar vNegW = _simd_mul_ps(vertex.w, _simd_set1_ps(-1.0f));

    // FRUSTUM_LEFT
    simdscalar vRes = _simd_cmplt_ps(vertex.x, vNegW);
    clipCodes = _simd_and_ps(vRes, _simd_castsi_ps(_simd_set1_epi32(FRUSTUM_LEFT)));

    // FRUSTUM_TOP
    vRes = _simd_cmplt_ps(vertex.y, vNegW);
    clipCodes = _simd_or_ps(clipCodes, _simd_and_ps(vRes, _simd_castsi_ps(_simd_set1_epi32(FRUSTUM_TOP))));

    // FRUSTUM_RIGHT
    vRes = _simd_cmpgt_ps(vertex.x, vertex.w);
    clipCodes = _simd_or_ps(clipCodes, _simd_and_ps(vRes, _simd_castsi_ps(_simd_set1_epi32(FRUSTUM_RIGHT))));

    // FRUSTUM_BOTTOM
    vRes = _simd_cmpgt_ps(vertex.y, vertex.w);
    clipCodes = _simd_or_ps(clipCodes, _simd_and_ps(vRes, _simd_castsi_ps(_simd_set1_epi32(FRUSTUM_BOTTOM))));

    // FRUSTUM_NEAR
    vRes = _simd_cmplt_ps(vertex.z, vNegW);
    clipCodes = _simd_or_ps(clipCodes, _simd_and_ps(vRes, _simd_castsi_ps(_simd_set1_epi32(FRUSTUM_NEAR))));

    // FRUSTUM_FAR
    vRes = _simd_cmpgt_ps(vertex.z, vertex.w);
    clipCodes = _simd_or_ps(clipCodes, _simd_and_ps(vRes, _simd_castsi_ps(_simd_set1_epi32(FRUSTUM_FAR))));

    // GUARDBAND_LEFT
    simdscalar gbMult = _simd_mul_ps(vNegW, _simd_set1_ps(state.gbState.left));
    vRes = _simd_cmplt_ps(vertex.x, gbMult);
    clipCodes = _simd_or_ps(clipCodes, _simd_and_ps(vRes, _simd_castsi_ps(_simd_set1_epi32(GUARDBAND_LEFT))));

    // GUARDBAND_TOP
    gbMult = _simd_mul_ps(vNegW, _simd_set1_ps(state.gbState.top));
    vRes = _simd_cmplt_ps(vertex.y, gbMult);
    clipCodes = _simd_or_ps(clipCodes, _simd_and_ps(vRes, _simd_castsi_ps(_simd_set1_epi32(GUARDBAND_TOP))));

    // GUARDBAND_RIGHT
    gbMult = _simd_mul_ps(vertex.w, _simd_set1_ps(state.gbState.right));
    vRes = _simd_cmpgt_ps(vertex.x, gbMult);
    clipCodes = _simd_or_ps(clipCodes, _simd_and_ps(vRes, _simd_castsi_ps(_simd_set1_epi32(GUARDBAND_RIGHT))));

    // GUARDBAND_BOTTOM
    gbMult = _simd_mul_ps(vertex.w, _simd_set1_ps(state.gbState.bottom));
    vRes = _simd_cmpgt_ps(vertex.y, gbMult);
    clipCodes = _simd_or_ps(clipCodes, _simd_and_ps(vRes, _simd_castsi_ps(_simd_set1_epi32(GUARDBAND_BOTTOM))));
}

void BinTriangles(DRAW_CONTEXT *pDC, PA_STATE &pa, simdvector tri[3], UINT numTris)
{
    SWR_CONTEXT *pContext = pDC->pContext;
    const RASTSTATE &state = pDC->state.rastState;
    const API_STATE &apiState = pDC->state;

    // Position vectors from the triangles.
    simdvector &v0 = tri[0];
    simdvector &v1 = tri[1];
    simdvector &v2 = tri[2];

    int triMask = triangleMask[numTris];

    // compute clip codes
    simdscalar clipCodes[3];
    computeClipCodes(apiState, tri[0], clipCodes[0]);
    computeClipCodes(apiState, tri[1], clipCodes[1]);
    computeClipCodes(apiState, tri[2], clipCodes[2]);

    // cull tris outside VP
    simdscalar clipIntersection = _simd_and_ps(clipCodes[0], clipCodes[1]);
    clipIntersection = _simd_and_ps(clipIntersection, clipCodes[2]);

    int valid = _simd_movemask_ps(_simd_cmpeq_ps(clipIntersection, _simd_setzero_ps()));

    triMask &= valid;

    // if no tris left, exit early
    if (!triMask)
    {
        RDTSC_EVENT(FEViewportCull, 1, 0);
        return;
    }

    // compute clip mask
    simdscalar clipUnion = _simd_or_ps(clipCodes[0], clipCodes[1]);
    clipUnion = _simd_or_ps(clipUnion, clipCodes[2]);
    clipUnion = _simd_and_ps(clipUnion, _simd_castsi_ps(_simd_set1_epi32(GUARDBAND_CLIP_MASK)));

    UINT clipMask = triMask & _simd_movemask_ps(_simd_cmpneq_ps(clipUnion, _simd_setzero_ps()));

    // perspective divide
    simdscalar vRecipW0 = _simd_div_ps(_simd_set1_ps(1.0f), v0.w);
    simdscalar vRecipW1 = _simd_div_ps(_simd_set1_ps(1.0f), v1.w);
    simdscalar vRecipW2 = _simd_div_ps(_simd_set1_ps(1.0f), v2.w);

    simdvector vert[4];
    vert[0].v[0] = _simd_mul_ps(v0.v[0], vRecipW0);
    vert[1].v[0] = _simd_mul_ps(v1.v[0], vRecipW1);
    vert[2].v[0] = _simd_mul_ps(v2.v[0], vRecipW2);

    vert[0].v[1] = _simd_mul_ps(v0.v[1], vRecipW0);
    vert[1].v[1] = _simd_mul_ps(v1.v[1], vRecipW1);
    vert[2].v[1] = _simd_mul_ps(v2.v[1], vRecipW2);

    vert[0].v[2] = _simd_mul_ps(v0.v[2], vRecipW0);
    vert[1].v[2] = _simd_mul_ps(v1.v[2], vRecipW1);
    vert[2].v[2] = _simd_mul_ps(v2.v[2], vRecipW2);

    vert[0].v[3] = v0.w;
    vert[1].v[3] = v1.w;
    vert[2].v[3] = v2.w;

    // viewport transform to screen coords
    transformVertical(vert[0], vert[1], vert[2], state, pContext->driverType);

    // bloat lines to tris
    if (apiState.topology == TOP_LINE_STRIP || apiState.topology == TOP_LINE_LIST)
    {
        const float bloatFactor = 0.50f;
#if (KNOB_VS_SIMD_WIDTH == 4)
        const __m128 vAdjust0 = _mm_set_ps(bloatFactor, -bloatFactor, bloatFactor, -bloatFactor);
        const __m128 vAdjust1 = _mm_set_ps(-bloatFactor, bloatFactor, -bloatFactor, bloatFactor);
#else
        const __m256 vAdjust0 = _mm256_set_ps(bloatFactor, -bloatFactor, bloatFactor, -bloatFactor, bloatFactor, -bloatFactor, bloatFactor, -bloatFactor);
        const __m256 vAdjust1 = _mm256_set_ps(-bloatFactor, bloatFactor, -bloatFactor, bloatFactor, -bloatFactor, bloatFactor, -bloatFactor, bloatFactor);
#endif

        // determine x-major or y-major line
        // ogl spec states x-major slope is [-1,1]
        // need _mm256_abs_ps instruction
        simdscalar vDeltaX = _simd_sub_ps(vert[0].x, vert[2].x);
        simdscalar vNegDeltaX = _simd_mul_ps(vDeltaX, _simd_set1_ps(-1.0f));
        vDeltaX = _simd_blendv_ps(vDeltaX, vNegDeltaX, vDeltaX);

        simdscalar vDeltaY = _simd_sub_ps(vert[0].y, vert[2].y);
        simdscalar vNegDeltaY = _simd_mul_ps(vDeltaY, _simd_set1_ps(-1.0f));
        vDeltaY = _simd_blendv_ps(vDeltaY, vNegDeltaY, vDeltaY);

        simdscalar vHoriz = _simd_cmpge_ps(vDeltaX, vDeltaY);

        simdscalar vAdjustX0 = _simd_add_ps(vert[0].x, vAdjust0);
        simdscalar vAdjustX1 = _simd_add_ps(vert[1].x, vAdjust1);
        simdscalar vAdjustX2 = _simd_add_ps(vert[2].x, vAdjust1);

        simdscalar vAdjustY0 = _simd_add_ps(vert[0].y, vAdjust0);
        simdscalar vAdjustY1 = _simd_add_ps(vert[1].y, vAdjust1);
        simdscalar vAdjustY2 = _simd_add_ps(vert[2].y, vAdjust1);

        vert[0].x = _simd_blendv_ps(vAdjustX0, vert[0].x, vHoriz);
        vert[1].x = _simd_blendv_ps(vAdjustX1, vert[1].x, vHoriz);
        vert[2].x = _simd_blendv_ps(vAdjustX2, vert[2].x, vHoriz);

        vert[0].y = _simd_blendv_ps(vert[0].y, vAdjustY0, vHoriz);
        vert[1].y = _simd_blendv_ps(vert[1].y, vAdjustY1, vHoriz);
        vert[2].y = _simd_blendv_ps(vert[2].y, vAdjustY2, vHoriz);
    }

    // bloat points to tri
    if (apiState.topology == TOP_POINTS)
    {
        const float bloat = 0.5f; // only support pointSize of 1
#if (KNOB_VS_SIMD_WIDTH == 4)
        const __m128 vAdjust0X = _mm_set_ps(-bloat, -bloat, -bloat, -bloat);
        const __m128 vAdjust0Y = _mm_set_ps(-bloat, -bloat, -bloat, -bloat);
        const __m128 vAdjust1X = _mm_set_ps(bloat, -bloat, bloat, -bloat);
        const __m128 vAdjust1Y = _mm_set_ps(bloat, bloat, bloat, bloat);
        const __m128 vAdjust2X = _mm_set_ps(bloat, bloat, bloat, bloat);
        const __m128 vAdjust2Y = _mm_set_ps(-bloat, bloat, -bloat, bloat);
#else
        const __m256 vAdjust0X = _mm256_set_ps(-bloat, -bloat, -bloat, -bloat, -bloat, -bloat, -bloat, -bloat);
        const __m256 vAdjust0Y = _mm256_set_ps(-bloat, -bloat, -bloat, -bloat, -bloat, -bloat, -bloat, -bloat);
        const __m256 vAdjust1X = _mm256_set_ps(bloat, -bloat, bloat, -bloat, bloat, -bloat, bloat, -bloat);
        const __m256 vAdjust1Y = _mm256_set_ps(bloat, bloat, bloat, bloat, bloat, bloat, bloat, bloat);
        const __m256 vAdjust2X = _mm256_set_ps(bloat, bloat, bloat, bloat, bloat, bloat, bloat, bloat);
        const __m256 vAdjust2Y = _mm256_set_ps(-bloat, bloat, -bloat, bloat, -bloat, bloat, -bloat, bloat);
#endif

        vert[0].x = _simd_add_ps(vert[0].x, vAdjust0X);
        vert[0].y = _simd_add_ps(vert[0].y, vAdjust0Y);
        vert[1].x = _simd_add_ps(vert[1].x, vAdjust1X);
        vert[1].y = _simd_add_ps(vert[1].y, vAdjust1Y);
        vert[2].x = _simd_add_ps(vert[2].x, vAdjust2X);
        vert[2].y = _simd_add_ps(vert[2].y, vAdjust2Y);
    }

    // convert to fixed point
    simdscalari vXi[3], vYi[3];
    vXi[0] = fpToFixedPointVertical(vert[0].x);
    vYi[0] = fpToFixedPointVertical(vert[0].y);
    vXi[1] = fpToFixedPointVertical(vert[1].x);
    vYi[1] = fpToFixedPointVertical(vert[1].y);
    vXi[2] = fpToFixedPointVertical(vert[2].x);
    vYi[2] = fpToFixedPointVertical(vert[2].y);

    // triangle setup
    simdscalari vAi[3], vBi[3];
    triangleSetupABIntVertical(vXi, vYi, vAi, vBi);

    // determinant
    simdscalar vDet;
    calcDeterminantIntVertical(vAi, vBi, vDet);

    // flip det for GL since Y is inverted
    if (pContext->driverType == GL)
    {
        vDet = _simd_mul_ps(vDet, _simd_set1_ps(-1.0f));
    }

    // cull zero area
    int mask = _simd_movemask_ps(_simd_cmpneq_ps(vDet, _simd_setzero_ps()));

    UINT origTriMask = triMask;
    triMask &= (mask | clipMask);

    // cull back facing
    if (state.cullMode == CCW)
    {
        int mask = _simd_movemask_ps(_simd_cmpgt_ps(vDet, _simd_setzero_ps()));
        triMask &= (mask | clipMask);
    }

    if (state.cullMode == CW)
    {
        int mask = _simd_movemask_ps(_simd_cmplt_ps(vDet, _simd_setzero_ps()));
        triMask &= (mask | clipMask);
    }

    if (origTriMask ^ triMask)
    {
        RDTSC_EVENT(FECullZeroAreaAndBackface, _mm_popcnt_u32(origTriMask ^ triMask), 0);
    }

    if (!triMask)
    {
        return;
    }

    // Calc bounding box of triangles
    simdBBox bbox;
    calcBoundingBoxIntVertical(vXi, vYi, bbox);

    // determine if triangle falls between pixel centers and discard
    // (left + 127) & ~255
    // (right + 128) & ~255

    origTriMask = triMask;

    simdscalari left = _simd_add_epi32(bbox.left, _simd_set1_epi32(127));
    left = _simd_and_si(left, _simd_set1_epi32(~255));
    simdscalari right = _simd_add_epi32(bbox.right, _simd_set1_epi32(128));
    right = _simd_and_si(right, _simd_set1_epi32(~255));

    simdscalari vMaskH = _simd_cmpeq_epi32(left, right);

    simdscalari top = _simd_add_epi32(bbox.top, _simd_set1_epi32(127));
    top = _simd_and_si(top, _simd_set1_epi32(~255));
    simdscalari bottom = _simd_add_epi32(bbox.bottom, _simd_set1_epi32(128));
    bottom = _simd_and_si(bottom, _simd_set1_epi32(~255));

    simdscalari vMaskV = _simd_cmpeq_epi32(top, bottom);
    vMaskV = _simd_or_si(vMaskH, vMaskV);
    mask = _simd_movemask_ps(_simd_castsi_ps(vMaskV));

    triMask &= (~mask | clipMask);

    if (origTriMask ^ triMask)
    {
        RDTSC_EVENT(FECullBetweenCenters, _mm_popcnt_u32(origTriMask ^ triMask), 0);
    }

    // convert to tiles
    bbox.left = _simd_srai_epi32(bbox.left, KNOB_TILE_X_DIM_SHIFT + FIXED_POINT_WIDTH);
    bbox.right = _simd_srai_epi32(bbox.right, KNOB_TILE_X_DIM_SHIFT + FIXED_POINT_WIDTH);
    bbox.top = _simd_srai_epi32(bbox.top, KNOB_TILE_Y_DIM_SHIFT + FIXED_POINT_WIDTH);
    bbox.bottom = _simd_srai_epi32(bbox.bottom, KNOB_TILE_Y_DIM_SHIFT + FIXED_POINT_WIDTH);

    // small triangle can use 32bit math in the BE
    simdscalari maskTilesX = _simd_cmplt_epi32(_simd_sub_epi32(bbox.right, bbox.left), _simd_set1_epi32(SMALL_TRI_X_TILES));
    simdscalari maskTilesY = _simd_cmplt_epi32(_simd_sub_epi32(bbox.bottom, bbox.top), _simd_set1_epi32(SMALL_TRI_Y_TILES));
    simdscalari maskTilesXY = _simd_and_si(maskTilesX, maskTilesY);
    UINT maskSmallTris = _simd_movemask_ps(_simd_castsi_ps(maskTilesXY));

    // intersect with scissor/viewport
    bbox.left = _simd_max_epi32(bbox.left, _simd_set1_epi32(apiState.scissorInTiles.left));
    bbox.top = _simd_max_epi32(bbox.top, _simd_set1_epi32(apiState.scissorInTiles.top));
    bbox.right = _simd_min_epi32(bbox.right, _simd_set1_epi32(apiState.scissorInTiles.right));
    bbox.bottom = _simd_min_epi32(bbox.bottom, _simd_set1_epi32(apiState.scissorInTiles.bottom));

    // cull tris completey outside scissor
    simdscalari maskOutsideScissorX = _simd_cmplt_epi32(bbox.right, bbox.left);
    simdscalari maskOutsideScissorY = _simd_cmplt_epi32(bbox.bottom, bbox.top);
    simdscalari maskOutsideScissorXY = _simd_or_si(maskOutsideScissorX, maskOutsideScissorY);
    UINT maskOutsideScissor = _simd_movemask_ps(_simd_castsi_ps(maskOutsideScissorXY));
    triMask = triMask & ~maskOutsideScissor;

    if (!triMask)
    {
        return;
    }

    UINT maskOneTile = 0;
    UINT aCoverageMask[KNOB_VS_SIMD_WIDTH];

// If macro tile size is equal to SIMD tile size, and the triangle bbox is fully contained within the tile,
// we can do a quick early-rast to try to cull tiny tris that don't intersect pixel centers
#if defined(KNOB_JIT_EARLY_RAST) && (KNOB_TILE_X_DIM == SIMD_TILE_X_DIM && KNOB_TILE_Y_DIM == SIMD_TILE_Y_DIM)
    // if triangle is within a single tile, do early rast
    simdscalari vTileX = _simd_cmpeq_epi32(bbox.left, bbox.right);
    simdscalari vTileY = _simd_cmpeq_epi32(bbox.top, bbox.bottom);
    UINT oneTileMask = triMask & _simd_movemask_ps(_simd_castsi_ps(_simd_and_si(vTileX, vTileY)));

    if (oneTileMask)
    {
        RDTSC_START(FEEarlyRast);
        // step to pixel center of top-left pixel of the triangle bbox
        simdscalari vTopLeftX = _simd_slli_epi32(bbox.left, KNOB_TILE_X_DIM_SHIFT + FIXED_POINT_WIDTH);
        vTopLeftX = _simd_add_epi32(vTopLeftX, _simd_set1_epi32(FIXED_POINT_SIZE / 2));

        simdscalari vTopLeftY = _simd_slli_epi32(bbox.top, KNOB_TILE_Y_DIM_SHIFT + FIXED_POINT_WIDTH);
        vTopLeftY = _simd_add_epi32(vTopLeftY, _simd_set1_epi32(FIXED_POINT_SIZE / 2));

        // negate A and B for CW tris
        simdscalari vNegA0 = _simd_mullo_epi32(vAi[0], _simd_set1_epi32(-1));
        simdscalari vNegA1 = _simd_mullo_epi32(vAi[1], _simd_set1_epi32(-1));
        simdscalari vNegA2 = _simd_mullo_epi32(vAi[2], _simd_set1_epi32(-1));
        simdscalari vNegB0 = _simd_mullo_epi32(vBi[0], _simd_set1_epi32(-1));
        simdscalari vNegB1 = _simd_mullo_epi32(vBi[1], _simd_set1_epi32(-1));
        simdscalari vNegB2 = _simd_mullo_epi32(vBi[2], _simd_set1_epi32(-1));

        vAi[0] = _simd_castps_si(_simd_blendv_ps(_simd_castsi_ps(vAi[0]), _simd_castsi_ps(vNegA0), vDet));
        vAi[1] = _simd_castps_si(_simd_blendv_ps(_simd_castsi_ps(vAi[1]), _simd_castsi_ps(vNegA1), vDet));
        vAi[2] = _simd_castps_si(_simd_blendv_ps(_simd_castsi_ps(vAi[2]), _simd_castsi_ps(vNegA2), vDet));
        vBi[0] = _simd_castps_si(_simd_blendv_ps(_simd_castsi_ps(vBi[0]), _simd_castsi_ps(vNegB0), vDet));
        vBi[1] = _simd_castps_si(_simd_blendv_ps(_simd_castsi_ps(vBi[1]), _simd_castsi_ps(vNegB1), vDet));
        vBi[2] = _simd_castps_si(_simd_blendv_ps(_simd_castsi_ps(vBi[2]), _simd_castsi_ps(vNegB2), vDet));

        // evaluate edge equations at top-left pixel
        simdscalari vDeltaX0 = _simd_sub_epi32(vTopLeftX, vXi[0]);
        simdscalari vDeltaX1 = _simd_sub_epi32(vTopLeftX, vXi[1]);
        simdscalari vDeltaX2 = _simd_sub_epi32(vTopLeftX, vXi[2]);

        simdscalari vDeltaY0 = _simd_sub_epi32(vTopLeftY, vYi[0]);
        simdscalari vDeltaY1 = _simd_sub_epi32(vTopLeftY, vYi[1]);
        simdscalari vDeltaY2 = _simd_sub_epi32(vTopLeftY, vYi[2]);

        simdscalari vAX0 = _simd_mullo_epi32(vAi[0], vDeltaX0);
        simdscalari vAX1 = _simd_mullo_epi32(vAi[1], vDeltaX1);
        simdscalari vAX2 = _simd_mullo_epi32(vAi[2], vDeltaX2);

        simdscalari vBY0 = _simd_mullo_epi32(vBi[0], vDeltaY0);
        simdscalari vBY1 = _simd_mullo_epi32(vBi[1], vDeltaY1);
        simdscalari vBY2 = _simd_mullo_epi32(vBi[2], vDeltaY2);

        simdscalari vEdge0 = _simd_add_epi32(vAX0, vBY0);
        simdscalari vEdge1 = _simd_add_epi32(vAX1, vBY1);
        simdscalari vEdge2 = _simd_add_epi32(vAX2, vBY2);

        vEdge0 = _simd_srai_epi32(vEdge0, FIXED_POINT_WIDTH);
        vEdge1 = _simd_srai_epi32(vEdge1, FIXED_POINT_WIDTH);
        vEdge2 = _simd_srai_epi32(vEdge2, FIXED_POINT_WIDTH);

        // top left rule
        simdscalari vEdgeAdjust0 = _simd_sub_epi32(vEdge0, _simd_set1_epi32(1));
        simdscalari vEdgeAdjust1 = _simd_sub_epi32(vEdge1, _simd_set1_epi32(1));
        simdscalari vEdgeAdjust2 = _simd_sub_epi32(vEdge2, _simd_set1_epi32(1));

        // vA < 0
        vEdge0 = _simd_castps_si(_simd_blendv_ps(_simd_castsi_ps(vEdge0), _simd_castsi_ps(vEdgeAdjust0), _simd_castsi_ps(vAi[0])));
        vEdge1 = _simd_castps_si(_simd_blendv_ps(_simd_castsi_ps(vEdge1), _simd_castsi_ps(vEdgeAdjust1), _simd_castsi_ps(vAi[1])));
        vEdge2 = _simd_castps_si(_simd_blendv_ps(_simd_castsi_ps(vEdge2), _simd_castsi_ps(vEdgeAdjust2), _simd_castsi_ps(vAi[2])));

        // vA == 0 && vB < 0
        simdscalari vCmp0 = _simd_cmpeq_epi32(vAi[0], _simd_setzero_si());
        simdscalari vCmp1 = _simd_cmpeq_epi32(vAi[1], _simd_setzero_si());
        simdscalari vCmp2 = _simd_cmpeq_epi32(vAi[2], _simd_setzero_si());

        vCmp0 = _simd_and_si(vCmp0, vBi[0]);
        vCmp1 = _simd_and_si(vCmp1, vBi[1]);
        vCmp2 = _simd_and_si(vCmp2, vBi[2]);

        vEdge0 = _simd_castps_si(_simd_blendv_ps(_simd_castsi_ps(vEdge0), _simd_castsi_ps(vEdgeAdjust0), _simd_castsi_ps(vCmp0)));
        vEdge1 = _simd_castps_si(_simd_blendv_ps(_simd_castsi_ps(vEdge1), _simd_castsi_ps(vEdgeAdjust1), _simd_castsi_ps(vCmp1)));
        vEdge2 = _simd_castps_si(_simd_blendv_ps(_simd_castsi_ps(vEdge2), _simd_castsi_ps(vEdgeAdjust2), _simd_castsi_ps(vCmp2)));

        // coverage pixel 0
        simdscalari vMask0 = _simd_and_si(vEdge0, vEdge1);
        vMask0 = _simd_and_si(vMask0, vEdge2);

        // coverage pixel 1
        simdscalari vEdge0N = _simd_add_epi32(vEdge0, vBi[0]);
        simdscalari vEdge1N = _simd_add_epi32(vEdge1, vBi[1]);
        simdscalari vEdge2N = _simd_add_epi32(vEdge2, vBi[2]);
        simdscalari vMask1 = _simd_and_si(vEdge0N, vEdge1N);
        vMask1 = _simd_and_si(vMask1, vEdge2N);

        // coverage pixel 2
        vEdge0N = _simd_add_epi32(vEdge0N, vAi[0]);
        vEdge1N = _simd_add_epi32(vEdge1N, vAi[1]);
        vEdge2N = _simd_add_epi32(vEdge2N, vAi[2]);
        simdscalari vMask2 = _simd_and_si(vEdge0N, vEdge1N);
        vMask2 = _simd_and_si(vMask2, vEdge2N);

        // coverage pixel 3
        vEdge0N = _simd_sub_epi32(vEdge0N, vBi[0]);
        vEdge1N = _simd_sub_epi32(vEdge1N, vBi[1]);
        vEdge2N = _simd_sub_epi32(vEdge2N, vBi[2]);
        simdscalari vMask3 = _simd_and_si(vEdge0N, vEdge1N);
        vMask3 = _simd_and_si(vMask3, vEdge2N);

#if KNOB_VS_SIMD_WIDTH == 8
        // coverage pixel 4
        vEdge0N = _simd_add_epi32(vEdge0N, vAi[0]);
        vEdge1N = _simd_add_epi32(vEdge1N, vAi[1]);
        vEdge2N = _simd_add_epi32(vEdge2N, vAi[2]);
        simdscalari vMask4 = _simd_and_si(vEdge0N, vEdge1N);
        vMask4 = _simd_and_si(vMask4, vEdge2N);

        // coverage pixel 5
        vEdge0N = _simd_add_epi32(vEdge0N, vBi[0]);
        vEdge1N = _simd_add_epi32(vEdge1N, vBi[1]);
        vEdge2N = _simd_add_epi32(vEdge2N, vBi[2]);
        simdscalari vMask5 = _simd_and_si(vEdge0N, vEdge1N);
        vMask5 = _simd_and_si(vMask5, vEdge2N);

        // coverage pixel 6
        vEdge0N = _simd_add_epi32(vEdge0N, vAi[0]);
        vEdge1N = _simd_add_epi32(vEdge1N, vAi[1]);
        vEdge2N = _simd_add_epi32(vEdge2N, vAi[2]);
        simdscalari vMask6 = _simd_and_si(vEdge0N, vEdge1N);
        vMask6 = _simd_and_si(vMask6, vEdge2N);

        // coverage pixel 7
        vEdge0N = _simd_sub_epi32(vEdge0N, vBi[0]);
        vEdge1N = _simd_sub_epi32(vEdge1N, vBi[1]);
        vEdge2N = _simd_sub_epi32(vEdge2N, vBi[2]);
        simdscalari vMask7 = _simd_and_si(vEdge0N, vEdge1N);
        vMask7 = _simd_and_si(vMask7, vEdge2N);
#endif

        simdscalari vLit = _simd_or_si(vMask0, vMask1);
        simdscalari vLit2 = _simd_or_si(vMask2, vMask3);
        vLit = _simd_or_si(vLit, vLit2);

#if KNOB_VS_SIMD_WIDTH == 8
        simdscalari vLit3 = _simd_or_si(vMask4, vMask5);
        simdscalari vLit4 = _simd_or_si(vMask6, vMask7);
        vLit3 = _simd_or_si(vLit3, vLit4);
        vLit = _simd_or_si(vLit, vLit3);
#endif

        UINT maskLit = _simd_movemask_ps(_simd_castsi_ps(vLit));
        maskOneTile = maskLit & oneTileMask;
        UINT maskUnlit = ~maskLit & oneTileMask;
        UINT origMask = triMask;
        triMask &= ~maskUnlit;

        RDTSC_STOP(FEEarlyRast, _mm_popcnt_u32(origMask ^ triMask), 0);

        if (!triMask)
        {
            return;
        }

#if KNOB_VS_SIMD_WIDTH == 8
        // transpose and re-swizzle 8x8
        __m256 vMask[8];
        vTranspose8x8(vMask, vMask0, vMask3, vMask1, vMask2, vMask4, vMask7, vMask5, vMask6);

        aCoverageMask[0] = _mm256_movemask_ps(vMask[0]);
        aCoverageMask[1] = _mm256_movemask_ps(vMask[1]);
        aCoverageMask[2] = _mm256_movemask_ps(vMask[2]);
        aCoverageMask[3] = _mm256_movemask_ps(vMask[3]);
        aCoverageMask[4] = _mm256_movemask_ps(vMask[4]);
        aCoverageMask[5] = _mm256_movemask_ps(vMask[5]);
        aCoverageMask[6] = _mm256_movemask_ps(vMask[6]);
        aCoverageMask[7] = _mm256_movemask_ps(vMask[7]);
#else
        vTranspose(vMask0, vMask3, vMask1, vMask2);

        aCoverageMask[0] = _mm_movemask_ps(_mm_castsi128_ps(vMask0));
        aCoverageMask[1] = _mm_movemask_ps(_mm_castsi128_ps(vMask3));
        aCoverageMask[2] = _mm_movemask_ps(_mm_castsi128_ps(vMask1));
        aCoverageMask[3] = _mm_movemask_ps(_mm_castsi128_ps(vMask2));
#endif
    }
#endif

    // compute per tri macro tile range
    simdscalar vRcpDim = _simd_load_ps(&apiState.vScissorRcpDim[0]);
    simdscalari topMT = _simd_cvttps_epi32(_simd_mul_ps(_simd_cvtepi32_ps(bbox.top), _simd_shuffle_ps(vRcpDim, vRcpDim, _MM_SHUFFLE(0, 0, 0, 0))));
    simdscalari bottomMT = _simd_cvttps_epi32(_simd_mul_ps(_simd_cvtepi32_ps(bbox.bottom), _simd_shuffle_ps(vRcpDim, vRcpDim, _MM_SHUFFLE(1, 1, 1, 1))));
    simdscalari leftMT = _simd_cvttps_epi32(_simd_mul_ps(_simd_cvtepi32_ps(bbox.left), _simd_shuffle_ps(vRcpDim, vRcpDim, _MM_SHUFFLE(2, 2, 2, 2))));
    simdscalari rightMT = _simd_cvttps_epi32(_simd_mul_ps(_simd_cvtepi32_ps(bbox.right), _simd_shuffle_ps(vRcpDim, vRcpDim, _MM_SHUFFLE(3, 3, 3, 3))));

    OSALIGNSIMD(UINT) aMTLeft[KNOB_VS_SIMD_WIDTH], aMTRight[KNOB_VS_SIMD_WIDTH], aMTTop[KNOB_VS_SIMD_WIDTH], aMTBottom[KNOB_VS_SIMD_WIDTH];
    _simd_store_si((simdscalari *)aMTLeft, leftMT);
    _simd_store_si((simdscalari *)aMTRight, rightMT);
    _simd_store_si((simdscalari *)aMTTop, topMT);
    _simd_store_si((simdscalari *)aMTBottom, bottomMT);

    OSALIGNSIMD(float)aRecipW0[KNOB_VS_SIMD_WIDTH], aRecipW1[KNOB_VS_SIMD_WIDTH], aRecipW2[KNOB_VS_SIMD_WIDTH];
    _simd_store_ps(aRecipW0, vRecipW0);
    _simd_store_ps(aRecipW1, vRecipW1);
    _simd_store_ps(aRecipW2, vRecipW2);

    // compute per tri backface
    UINT backfaceMask = _simd_movemask_ps(_simd_cmpgt_ps(vDet, _simd_setzero_ps()));

#if KNOB_VERTICALIZED_BINNER == 0
// transpose verts needed for backend
// @todo modify BE to take non-transformed verts
#if KNOB_VS_SIMD_WIDTH == 4
    simdscalar newW[4] = { vRecipW0, vRecipW1, vRecipW2, _simd_setzero_ps() };
    vTranspose(vert[0].x, vert[1].x, vert[2].x, vert[3].x);
    vTranspose(vert[0].y, vert[1].y, vert[2].y, vert[3].y);
    vTranspose(vert[0].z, vert[1].z, vert[2].z, vert[3].z);
    vTranspose(newW[0], newW[1], newW[2], newW[3]);
#else
    __m128 vHorizX[8], vHorizY[8], vHorizZ[8], vHorizW[8];
    vTranspose3x8(vHorizX, vert[0].x, vert[1].x, vert[2].x);
    vTranspose3x8(vHorizY, vert[0].y, vert[1].y, vert[2].y);
    vTranspose3x8(vHorizZ, vert[0].z, vert[1].z, vert[2].z);
    vTranspose3x8(vHorizW, vRecipW0, vRecipW1, vRecipW2);
#endif
#endif

    DWORD triIndex = 0;
    // scan remaining valid triangles and bin each separately
    while (_BitScanForward(&triIndex, triMask))
    {
        UINT linkageCount = pDC->state.linkageCountFrontFace;
        UINT linkageMask = pDC->state.linkageMaskFrontFace;

        if ((backfaceMask >> triIndex) & 1)
        {
            linkageCount = pDC->state.linkageCountBackFace;
            linkageMask = pDC->state.linkageMaskBackFace;
        }

        UINT numScalarAttribs = linkageCount * 4;

        // clip and rebin any clipped triangles
        if (clipMask & (1 << triIndex))
        {
            OSALIGN(float, 16) inVerts[3 * 4];
            OSALIGN(float, 16) outVerts[6 * 4];
            OSALIGN(float, 16) inAttribs[3 * KNOB_NUM_ATTRIBUTES * 4];
            OSALIGN(float, 16) outAttribs[6 * KNOB_NUM_ATTRIBUTES * 4];

            // grab the triangle vertices and transpose to xyzw
            __m128 verts[3];
            PaAssembleSingle(pa, VS_SLOT_POSITION, triIndex, verts);
            _mm_store_ps(&inVerts[0], verts[0]);
            _mm_store_ps(&inVerts[4], verts[1]);
            _mm_store_ps(&inVerts[8], verts[2]);

            // grab the attribs
            int idx = 0;
            DWORD slot = 0;
            unsigned int tmpLinkage = linkageMask;
            while (_BitScanForward(&slot, tmpLinkage))
            {
                __m128 attrib[3]; // triangle attribs (always 4 wide)
                PaAssembleSingle(pa, slot, triIndex, attrib);
                _mm_store_ps(&inAttribs[idx], attrib[0]);
                _mm_store_ps(&inAttribs[idx + numScalarAttribs], attrib[1]);
                _mm_store_ps(&inAttribs[idx + numScalarAttribs * 2], attrib[2]);
                idx += 4;
                tmpLinkage &= ~(1 << slot);
            }

            int numVerts;
            Clip(inVerts, inAttribs, numScalarAttribs, outVerts, &numVerts, outAttribs);

            // construct a trifan out of the generated verts and bin
            if (numVerts >= 3)
            {
                int numTris = numVerts - 2;
                UINT idx0 = 0, idx1 = 1, idx2 = 2;

                for (int tri = 0; tri < numTris; ++tri)
                {
                    __m128 vX = _mm_load_ps(&outVerts[idx0 * 4]);
                    __m128 vY = _mm_load_ps(&outVerts[idx1 * 4]);
                    __m128 vZ = _mm_load_ps(&outVerts[idx2 * 4]);
                    __m128 vW = _mm_setzero_ps();

                    vTranspose(vX, vY, vZ, vW);
                    UINT index[3] = { idx0, idx1, idx2 };

                    BinTriangle<false>(pDC, vX, vY, vZ, vW, index, outAttribs, numScalarAttribs);

                    idx1++;
                    idx2++;
                }
            }

            triMask &= ~(1 << triIndex);
            continue;
        }

#if KNOB_VERTICALIZED_BINNER
        VERT_BE_WORK work;
#else
        BE_WORK work;
#endif
#if defined(_DEBUG)
        work.type = DRAW;
#endif

#if KNOB_VERTICALIZED_BINNER
        VERTICAL_TRIANGLE_DESC &desc = work.desc.tri;
#else
        TRIANGLE_WORK_DESC &desc = work.desc.tri;
#endif

        if ((maskOneTile >> triIndex) & 1)
        {
            work.pfnWork = rastOneTileTri;
            desc.triFlags.coverageMask = aCoverageMask[triIndex];
        }
        else if ((maskSmallTris >> triIndex) & 1)
        {
            work.pfnWork = rastSmallTri;
        }
        else
        {
            work.pfnWork = rastLargeTri;
        }

        float *pInterpBuffer = (float *)pDC->arena.AllocAligned(numScalarAttribs * 3 * sizeof(float), 16);
        float *pTempBuffer = pInterpBuffer;

#if KNOB_VERTICALIZED_BINNER == 0
        desc.pInterpBuffer = pInterpBuffer;
#endif

        DWORD slot = 0;

        while (_BitScanForward(&slot, linkageMask))
        {
            __m128 attrib[3]; // triangle attribs (always 4 wide)

            PaAssembleSingle(pa, slot, triIndex, attrib);

#if KNOB_VS_SIMD_WIDTH == 4
            __m128 v4RecipW0 = _mm_load1_ps(&aRecipW0[triIndex]);
            __m128 v4RecipW1 = _mm_load1_ps(&aRecipW1[triIndex]);
            __m128 v4RecipW2 = _mm_load1_ps(&aRecipW2[triIndex]);

            __m128 vInterpEdge0 = _mm_mul_ps(attrib[0], v4RecipW0);
            __m128 vInterpEdge1 = _mm_mul_ps(attrib[1], v4RecipW1);
            __m128 vInterpEdge2 = _mm_mul_ps(attrib[2], v4RecipW2);
#else
            __m128 v4RecipW0 = _mm_load1_ps(&aRecipW0[triIndex]);
            __m128 v4RecipW1 = _mm_load1_ps(&aRecipW1[triIndex]);
            __m128 v4RecipW2 = _mm_load1_ps(&aRecipW2[triIndex]);

            __m128 vInterpEdge0 = _mm_mul_ps(attrib[0], v4RecipW0);
            __m128 vInterpEdge1 = _mm_mul_ps(attrib[1], v4RecipW1);
            __m128 vInterpEdge2 = _mm_mul_ps(attrib[2], v4RecipW2);
#endif

            __m128 vInterpA = _mm_sub_ps(vInterpEdge0, vInterpEdge2);
            __m128 vInterpB = _mm_sub_ps(vInterpEdge1, vInterpEdge2);

            _mm_store_ps(pTempBuffer, vInterpA);
            _mm_store_ps(pTempBuffer += 4, vInterpB);
            _mm_store_ps(pTempBuffer += 4, vInterpEdge2);
            pTempBuffer += 4;

            linkageMask &= ~(1 << slot); // done with this bit.
        }

// store triangle vertex data
#if KNOB_VERTICALIZED_BINNER == 0
        desc.pTriBuffer = (float *)pDC->arena.AllocAligned(4 * 4 * sizeof(float), 16);

#if KNOB_VS_SIMD_WIDTH == 4
        _simd_store_ps(&desc.pTriBuffer[0], vert[triIndex].x);
        _simd_store_ps(&desc.pTriBuffer[4], vert[triIndex].y);
        _simd_store_ps(&desc.pTriBuffer[8], vert[triIndex].z);
        _simd_store_ps(&desc.pTriBuffer[12], newW[triIndex]);
#else
        _mm_store_ps(&desc.pTriBuffer[0], vHorizX[triIndex]);
        _mm_store_ps(&desc.pTriBuffer[4], vHorizY[triIndex]);
        _mm_store_ps(&desc.pTriBuffer[8], vHorizZ[triIndex]);
        _mm_store_ps(&desc.pTriBuffer[12], vHorizW[triIndex]);
#endif
#endif

        MacroTileMgr *pTileMgr = pDC->pTileMgr;
        for (UINT y = aMTTop[triIndex]; y <= aMTBottom[triIndex]; ++y)
        {
            for (UINT x = aMTLeft[triIndex]; x <= aMTRight[triIndex]; ++x)
            {
#ifndef KNOB_TOSS_SETUP_TRIS
                work.desc.tri.triFlags.macroX = x;
                work.desc.tri.triFlags.macroY = y;
#if KNOB_VERTICALIZED_BINNER
                bp.addTriangle(m, vert, triIndex, pInterpBuffer);
#else
                pTileMgr->enqueue(x, y, &work);
#endif
#endif
            }
        }

        triMask &= ~(1 << triIndex);
    }

    _ReadWriteBarrier();
}

#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////// HORIZONTAL ///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <PRIMITIVE_TOPOLOGY topology, bool isIndexed>
void PrimitiveAssembly(DRAW_CONTEXT *pDC, UINT numPrims, const UINT *pIndexBuffer, const float *pPos, const float *pAttribs, UINT numAttribs);

void ProcessDrawVertices(SWR_CONTEXT *pContext,
                         VS_WORK &work)
{
    DRAW_CONTEXT *pDC = work.pDC;
    SWR_FETCH_INFO fetchInfo = { { 0 }, 0, pDC };

    fetchInfo.pConstants = pDC->state.pFSConstantBufferAlloc->pData;

    for (UINT i = 0; i < KNOB_NUM_STREAMS; ++i)
    {
        if (pDC->state.ppVertexBufferAlloc[i])
        {
            fetchInfo.ppStreams[i] = (float *)pDC->state.ppVertexBufferAlloc[i]->pData;
        }
    }

    //OSALIGN(float, 16) pInVertices[SHADER_INPUT_SLOTX_MAX_SLOTS * KNOB_FLOATS_PER_ATTRIBUTE * 2]; //KNOB_ATTRIBUTES_PER_FETCH];
    OSALIGN(float, 16) pInVertices[12 * KNOB_FLOATS_PER_ATTRIBUTE * 2]; //KNOB_ATTRIBUTES_PER_FETCH];

    VERTEXINPUT vin;
    VERTEXOUTPUT vout;

    vin.pConstants = pDC->state.pVSConstantBufferAlloc->pData;
    vin.pAttribs = pInVertices;
    vout.pVertices = (FLOAT *)work.pOutVertices;
    vout.pAttribs = (FLOAT *)work.pOutAttributes;

    UINT numAttribs = pDC->state.linkageTotalCount * KNOB_FLOATS_PER_ATTRIBUTE;

    INT endVertex = work.startVertex + work.numVerts;
    for (INT i = work.startVertex; i < endVertex; ++i)
    {
        fetchInfo.pIndices = &i;
        RDTSC_START(FEFetchShader);
        pDC->state.pfnFetchFunc(fetchInfo, vin);
        RDTSC_STOP(FEFetchShader, 0, 0);

        RDTSC_START(FEVertexShader);
        pDC->state.pfnVertexFunc(vin, vout); // Call vertex shader
        RDTSC_STOP(FEVertexShader, 0, 0);

        vout.pVertices += KNOB_FLOATS_PER_ATTRIBUTE * KNOB_ATTRIBUTES_PER_FETCH;
        vout.pAttribs += numAttribs;
    }
}

void ProcessDrawIndexedVertices(SWR_CONTEXT *pContext,
                                VS_WORK &work)
{
    DRAW_CONTEXT *pDC = work.pDC;
    SWR_FETCH_INFO fetchInfo = { { 0 }, 0, pDC };

    fetchInfo.pConstants = pDC->state.pFSConstantBufferAlloc->pData;

    for (UINT i = 0; i < KNOB_NUM_STREAMS; ++i)
    {
        if (pDC->state.ppVertexBufferAlloc[i])
        {
            fetchInfo.ppStreams[i] = (float *)pDC->state.ppVertexBufferAlloc[i]->pData;
        }
    }

    //OSALIGN(float, 16) pInVertices[SHADER_INPUT_SLOTX_MAX_SLOTS * KNOB_FLOATS_PER_ATTRIBUTE * KNOB_ATTRIBUTES_PER_FETCH];
    OSALIGN(float, 16) pInVertices[12 * KNOB_FLOATS_PER_ATTRIBUTE * KNOB_ATTRIBUTES_PER_FETCH];

    VERTEXINPUT vin;
    VERTEXOUTPUT vout;
    vin.pConstants = pDC->state.pVSConstantBufferAlloc->pData;
    vin.pAttribs = pInVertices;
    vout.pVertices = (FLOAT *)work.pOutVertices;
    vout.pAttribs = (FLOAT *)work.pOutAttributes;

    UINT numAttribs = pDC->state.linkageTotalCount * KNOB_FLOATS_PER_ATTRIBUTE;

    for (INT i = 0; i < work.numVsIndices; i += KNOB_ATTRIBUTES_PER_FETCH)
    {
        fetchInfo.pIndices = work.pVsIndices + i;
        RDTSC_START(FEFetchShader);
        pDC->state.pfnFetchFunc(fetchInfo, vin);
        RDTSC_STOP(FEFetchShader, 0, 0);

        RDTSC_START(FEVertexShader);
        pDC->state.pfnVertexFunc(vin, vout); // Call vertex shader
        RDTSC_STOP(FEVertexShader, 0, 0);

        vout.pVertices += KNOB_FLOATS_PER_ATTRIBUTE * KNOB_ATTRIBUTES_PER_FETCH;
        vout.pAttribs += numAttribs;
    }
}

#define VERTEXCACHE_ENABLE 1
#define VERTEXCACHE_SIZE 4096
#define VERTEXCACHE_NULL_ENTRY -1 // @todo hookup

//#define SHOW_VCACHE_VERTS			1
#define VERTEX_CACHED_FLAG 0x80000000

struct VERTEXCACHE_ENTRY
{
    INT preIndex;
    INT postIndex;
    UINT instance;
};

struct VERTEXCACHE
{
    VERTEXCACHE_ENTRY cache[VERTEXCACHE_SIZE];
};

void CacheVertex(VERTEXCACHE *pVertexCache, INT preIndex, INT postIndex, UINT instance)
{
#ifdef VERTEXCACHE_ENABLE
    INT cacheIndex = preIndex & (VERTEXCACHE_SIZE - 1);
    pVertexCache->cache[cacheIndex].preIndex = preIndex;
    pVertexCache->cache[cacheIndex].instance = instance;
    pVertexCache->cache[cacheIndex].postIndex = postIndex;
#endif
}

INT CheckCache(VERTEXCACHE *pVertexCache, INT vtxIndex, UINT instance)
{
#ifdef VERTEXCACHE_ENABLE
    INT cacheIndex = vtxIndex & (VERTEXCACHE_SIZE - 1);
    if ((pVertexCache->cache[cacheIndex].preIndex == vtxIndex) && (pVertexCache->cache[cacheIndex].instance == instance))
    {
        //RDTSC_EVENT(FENumCacheHits, 1, 0);
        return pVertexCache->cache[cacheIndex].postIndex;
    }
#endif
    return -1;
}

void ProcessIndices(SWR_CONTEXT *pContext,
                    IA_WORK &work)
{
    DRAW_CONTEXT *pDC = work.pDC;
    VERTEXCACHE vcache = { -1 };

    work.numRsIndices = work.numIndices;

    work.pRsIndices = (INT *)pDC->arena.Alloc(work.numRsIndices * sizeof(INT));
    work.pVsIndices = (INT *)pDC->arena.Alloc(ALIGN_UP(work.numRsIndices, KNOB_ATTRIBUTES_PER_FETCH) * sizeof(INT));

    UINT rsIndex = 0;

    for (UINT i = 0; i < work.numIndices; ++i)
    {
        UINT appIndex = work.pIndices[i];

        // XXX: must be made instance aware
        // Check vertex cache here based on vtxIndex
        //RDTSC_START(FEVertexCheckCache);
        INT vcacheIndex = CheckCache(&vcache, appIndex, work.instance);
        //RDTSC_STOP(FEVertexCheckCache, 0);

        if (vcacheIndex < 0)
        {
            //RDTSC_START(FEVertexStoreCache);
            CacheVertex(&vcache, appIndex, rsIndex, work.instance);
            //RDTSC_STOP(FEVertexStoreCache, 0);

            // pVsIndices are indices to app vertices that we need to shade.
            // pRsIndices are indices into the post transform vb that we pass to rasterizer.
            work.pVsIndices[rsIndex] = appIndex;
            work.pRsIndices[i] = rsIndex;
            rsIndex++;
        }
        else
        {
#ifdef SHOW_VCACHE_VERTS
            vcacheIndex |= VERTEX_CACHED_FLAG;
#endif
            work.pRsIndices[i] = vcacheIndex;
        }
    }

    work.numVsIndices = rsIndex;
    work.numRsIndices = work.numIndices;
}

#if (KNOB_VERTICALIZED_FE == 0)
void ProcessDraw(SWR_CONTEXT *pContext, DRAW_CONTEXT *pDC, void *pUserData)
{
    DRAW_WORK &work = *(DRAW_WORK *)pUserData;

    DRAW_CONTEXT *pDC = work.pDC;

    UINT numVertices = work.numVerts;
    UINT numPrims = NumElementsGivenIndices(pDC->state.topology, numVertices);

    UINT numAttribs = pDC->state.linkageTotalCount * KNOB_FLOATS_PER_ATTRIBUTE;

    // @todo Allocating a bit more memory then we actually need.
    FLOAT *pOutVerticesBase = (FLOAT *)pDC->arena.AllocAligned(numVertices * sizeof(FLOAT) * 4, 16);
    FLOAT *pOutAttributesBase = (FLOAT *)pDC->arena.AllocAligned(numVertices * sizeof(FLOAT) * numAttribs, 16);
    FLOAT *pOutVertices = pOutVerticesBase;
    FLOAT *pOutAttributes = pOutAttributesBase;

#ifdef KNOB_TOSS_IA
    pDC->inUse = false;
    return; // Don't do the draw
#endif

#ifdef KNOB_TOSS_VS
    pDC->inUse = false;
    return; // Don't do VS
#endif

    VS_WORK vs_work;

    vs_work.pDC = pDC;
    vs_work.instance = 0;
    vs_work.numVerts = work.numVerts;
    vs_work.startVertex = work.startVertex;
    vs_work.pOutVertices = (FLOAT *)pOutVertices;
    vs_work.pOutAttributes = (FLOAT *)pOutAttributes;

    RDTSC_START(FEProcessDrawVertices);
    ProcessDrawVertices(pContext, vs_work);
    RDTSC_STOP(FEProcessDrawVertices, vs_work.numVsIndices, pDC->drawId);

    switch (pDC->state.topology)
    {
    case TOP_TRIANGLE_LIST:
        PrimitiveAssembly<TOP_TRIANGLE_LIST, false>(pDC, numPrims, NULL, (const float *)pOutVerticesBase, (float *)pOutAttributesBase, numAttribs);
        break;
    case TOP_TRIANGLE_STRIP:
        PrimitiveAssembly<TOP_TRIANGLE_STRIP, false>(pDC, numPrims, NULL, (const float *)pOutVerticesBase, (float *)pOutAttributesBase, numAttribs);
        break;
    case TOP_TRIANGLE_FAN:
        PrimitiveAssembly<TOP_TRIANGLE_FAN, false>(pDC, numPrims, NULL, (const float *)pOutVerticesBase, (float *)pOutAttributesBase, numAttribs);
        break;
    case TOP_QUAD_LIST:
        PrimitiveAssembly<TOP_QUAD_LIST, false>(pDC, numPrims, NULL, (const float *)pOutVerticesBase, (float *)pOutAttributesBase, numAttribs);
        break;
    case TOP_QUAD_STRIP:
        PrimitiveAssembly<TOP_QUAD_STRIP, false>(pDC, numPrims, NULL, (const float *)pOutVerticesBase, (float *)pOutAttributesBase, numAttribs);
        break;
    default:
        assert(0);
    }

    _ReadWriteBarrier();
    pDC->doneFE = true;
}

void ProcessDrawIndexed(SWR_CONTEXT *pContext, DRAW_CONTEXT *pDC, void *pUserData)
{
    DRAW_WORK &work = *(DRAW_WORK *)pUserData;

    DRAW_CONTEXT *pDC = work.pDC;

    UINT numVertices = work.numIndices;
    UINT numPrims = NumElementsGivenIndices(pDC->state.topology, numVertices);
    UINT numAttribs = pDC->state.linkageTotalCount * KNOB_FLOATS_PER_ATTRIBUTE;

    // @todo Allocating a bit more memory then we actually need.
    FLOAT *pOutVerticesBase = (FLOAT *)pDC->arena.AllocAligned(numVertices * sizeof(FLOAT) * 4, 16);
    FLOAT *pOutAttributesBase = (FLOAT *)pDC->arena.AllocAligned(numVertices * sizeof(FLOAT) * numAttribs, 16);
    FLOAT *pOutVertices = pOutVerticesBase;
    FLOAT *pOutAttributes = pOutAttributesBase;

#ifdef KNOB_TOSS_IA
    pDC->inUse = false;
    return; // Don't do the draw
#endif

    IA_WORK ia_work;
    ia_work.pDC = pDC;
    ia_work.numIndices = work.numIndices;
    ia_work.pIndices = (INT *)work.pIB;
    ia_work.instance = work.instance;

    RDTSC_START(FEProcessIndices);
    ProcessIndices(pContext, ia_work);
    RDTSC_STOP(FEProcessIndices, work.numIndices, pDC->drawId);

#ifdef KNOB_TOSS_VS
    pDC->inUse = false;
    return; // Don't do VS
#endif

    VS_WORK vs_work;
    INT *pVsIndices = ia_work.pVsIndices;

    vs_work.pDC = pDC;
    vs_work.instance = ia_work.instance;
    vs_work.numVsIndices = ia_work.numVsIndices;
    vs_work.pVsIndices = ia_work.pVsIndices;
    vs_work.pOutVertices = (FLOAT *)pOutVertices;
    vs_work.pOutAttributes = (FLOAT *)pOutAttributes;

    RDTSC_START(FEProcessDrawIndexedVertices);
    ProcessDrawIndexedVertices(pContext, vs_work);
    RDTSC_STOP(FEProcessDrawIndexedVertices, vs_work.numVsIndices, pDC->drawId);

    RDTSC_START(FEBinTriangles);

    switch (pDC->state.topology)
    {
    case TOP_TRIANGLE_LIST:
        PrimitiveAssembly<TOP_TRIANGLE_LIST, true>(pDC, numPrims, (const UINT *)ia_work.pRsIndices, (const float *)pOutVerticesBase, (float *)pOutAttributesBase, (pDC->state.numAttributes - 1) * 4);
        break;
    case TOP_QUAD_LIST:
        PrimitiveAssembly<TOP_QUAD_LIST, true>(pDC, numPrims, (const UINT *)ia_work.pRsIndices, (const float *)pOutVerticesBase, (float *)pOutAttributesBase, (pDC->state.numAttributes - 1) * 4);
        break;
    default:
        assert(0);
    }

    _ReadWriteBarrier();
    pDC->doneFE = true;

    RDTSC_STOP(FEBinTriangles, numPrims, pDC->drawId);
}
#endif

// rasterizer calls this to fetch the next triangle and transform in
INLINE
UINT *getNextTriangleIndexed(const UINT *pIndexBuffer, const float *pVertexBuffer, __m128 &vX, __m128 &vY, __m128 &vZ, __m128 &vW, UINT &i0, UINT &i1, UINT &i2)
{
    // assume tri list for now
    i0 = pIndexBuffer[0];
    i1 = pIndexBuffer[1];
    i2 = pIndexBuffer[2];

    // gather
    vX = _mm_load_ps(&pVertexBuffer[i0 * 4]);
    vY = _mm_load_ps(&pVertexBuffer[i1 * 4]);
    vZ = _mm_load_ps(&pVertexBuffer[i2 * 4]);
    vW = _mm_setzero_ps();

    // transform to soa
    vTranspose(vX, vY, vZ, vW);

    return (UINT *)(pIndexBuffer + 3);
}

INLINE
void getNextTriangle(UINT triIndex, const float *pVertexBuffer, __m128 &vX, __m128 &vY, __m128 &vZ, __m128 &vW, UINT &i0, UINT &i1, UINT &i2)
{
    // assume tri list for now
    i0 = triIndex * 3 + 0;
    i1 = triIndex * 3 + 1;
    i2 = triIndex * 3 + 2;

    // gather
    vX = _mm_load_ps(&pVertexBuffer[i0 * 4]);
    vY = _mm_load_ps(&pVertexBuffer[i1 * 4]);
    vZ = _mm_load_ps(&pVertexBuffer[i2 * 4]);
    vW = _mm_setzero_ps();

    // transform to soa
    vTranspose(vX, vY, vZ, vW);
}

void triangleSetupCInt(const __m128 vX, const __m128 vY, const __m128i vA, const __m128i &vB, __m128 &vC)
{
    __m128 vAi = fixedPointToFP(vA);
    __m128 vBi = fixedPointToFP(vB);

    triangleSetupC(vX, vY, vAi, vBi, vC);
}

float calcDeterminant(const __m128 vA, const __m128 vB)
{
    // (y1-y2)(x0-x2) + (x2-x1)*(y0-y2)
    //A1*B2 - B1*A2

    __m128 vAShuf = _mm_shuffle_ps(vA, vA, _MM_SHUFFLE(0, 3, 2, 1));
    __m128 vBShuf = _mm_shuffle_ps(vB, vB, _MM_SHUFFLE(0, 3, 1, 2));
    __m128 vMul = _mm_mul_ps(vAShuf, vBShuf);
    vMul = _mm_hsub_ps(vMul, vMul);

    float det;
    _mm_store_ss(&det, vMul);
    return det;
}

// Primitive assembly
template <PRIMITIVE_TOPOLOGY topology, bool isIndexed>
void PrimitiveAssembly(DRAW_CONTEXT *pDC, UINT numPrims, const UINT *pIndexBuffer, const float *pPos, const float *pAttribs, UINT numAttribs)
{
    const UINT numComponents = 4; // always should see X,Y,Z,W
    UINT baseIndex = 0;
    UINT index[6];
    UINT numIndices = 0;
    UINT indexIncrement = 0;
    UINT oddPrim = 1;

    for (UINT prim = 0; prim < numPrims; ++prim)
    {
        // compute indices for next prim
        switch (topology)
        {
        case TOP_TRIANGLE_LIST:
            numIndices = 3;
            indexIncrement = 3;
            index[0] = baseIndex;
            index[1] = baseIndex + 1;
            index[2] = baseIndex + 2;
            break;

        case TOP_TRIANGLE_STRIP:
            numIndices = 3;
            indexIncrement = 1;

            if (oddPrim)
            {
                index[0] = baseIndex;
                index[1] = baseIndex + 1;
                index[2] = baseIndex + 2;
            }
            else
            {
                index[0] = baseIndex + 1;
                index[1] = baseIndex;
                index[2] = baseIndex + 2;
            }
            oddPrim ^= 1;
            break;

        case TOP_TRIANGLE_FAN:
            numIndices = 3;
            indexIncrement = 0;

            index[0] = baseIndex;
            index[1] = baseIndex + prim + 1;
            index[2] = baseIndex + prim + 2;
            break;

        case TOP_QUAD_LIST:
            numIndices = 6;
            indexIncrement = 4;
            index[0] = baseIndex;
            index[1] = baseIndex + 1;
            index[2] = baseIndex + 3;
            index[3] = baseIndex + 1;
            index[4] = baseIndex + 2;
            index[5] = baseIndex + 3;
            break;

        case TOP_QUAD_STRIP:
            numIndices = 6;
            indexIncrement = 2;
            index[0] = baseIndex;
            index[1] = baseIndex + 1;
            index[2] = baseIndex + 3;

            index[3] = baseIndex;
            index[4] = baseIndex + 3;
            index[5] = baseIndex + 2;
            break;
        default:
            assert(0);
        }

        if (isIndexed)
        {
            for (UINT i = 0; i < numIndices; ++i)
            {
                index[i] = pIndexBuffer[index[i]];
            }
        }

        for (UINT i = 0; i < numIndices; i += 3)
        {
            // gather verts
            __m128 vX = _mm_load_ps(&pPos[index[i] * 4]);
            __m128 vY = _mm_load_ps(&pPos[index[i + 1] * 4]);
            __m128 vZ = _mm_load_ps(&pPos[index[i + 2] * 4]);
            __m128 vW = _mm_setzero_ps();

            // transform to soa
            vTranspose(vX, vY, vZ, vW);

// bin it!
#if (KNOB_VERTICALIZED_FE == 0)
            RDTSC_START(FEBinTriangles);
            BinTriangle<true>(pDC, vX, vY, vZ, vW, &index[i], pAttribs, numAttribs);
            RDTSC_STOP(FEBinTriangles, 1, 0);
#endif
        }

        baseIndex += indexIncrement;
    }
}

template <bool CullAndClip>
void BinTriangle(DRAW_CONTEXT *pDC, __m128 &vX, __m128 &vY, __m128 &vZ, __m128 &vW, UINT index[3], const float *pAttribs, UINT numAttribs)
{
    SWR_CONTEXT *pContext = pDC->pContext;
    const RASTSTATE &state = pDC->state.rastState;
    const API_STATE &apiState = pDC->state;

#ifdef KNOB_TOSS_SETUP_TRIS
    pDC->doneFE = true;
    return; // Don't do triangle setup
#endif

    if (CullAndClip)
    {
        int mask1, mask2, mask3, mask4, mask5, mask6, mask7;

        __m128 vNegW = _mm_mul_ps(vW, _mm_set1_ps(-1.0f));

        // cull anything completely outside viewport
        mask1 = _mm_movemask_ps(_mm_cmplt_ps(vX, vNegW));
        mask2 = _mm_movemask_ps(_mm_cmplt_ps(vY, vNegW));
        mask3 = _mm_movemask_ps(_mm_cmpgt_ps(vX, vW));
        mask4 = _mm_movemask_ps(_mm_cmpgt_ps(vY, vW));
        mask5 = _mm_movemask_ps(_mm_cmplt_ps(vZ, vNegW));
        mask6 = _mm_movemask_ps(_mm_cmpgt_ps(vZ, vW));
        mask7 = _mm_movemask_ps(_mm_cmplt_ps(vW, _mm_setzero_ps()));
        if (mask1 == 0x7 || mask2 == 0x7 || mask3 == 0x7 || mask4 == 0x7 || mask5 == 0x7 || mask6 == 0x7 || mask7 == 0x7)
        {
            RDTSC_EVENT(FEViewportCull, 1, 0);
            return;
        }

        // clip to guardband

        vNegW = _mm_mul_ps(vW, _mm_set1_ps(-KNOB_GUARDBAND_WIDTH));
        __m128 vPosW = _mm_mul_ps(vW, _mm_set1_ps(KNOB_GUARDBAND_WIDTH));
        mask1 = _mm_movemask_ps(_mm_cmpge_ps(vX, vNegW));
        mask2 = _mm_movemask_ps(_mm_cmple_ps(vX, vPosW));

        vNegW = _mm_mul_ps(vW, _mm_set1_ps(-KNOB_GUARDBAND_HEIGHT));
        vPosW = _mm_mul_ps(vW, _mm_set1_ps(KNOB_GUARDBAND_HEIGHT));
        mask3 = _mm_movemask_ps(_mm_cmpge_ps(vY, vNegW));
        mask4 = _mm_movemask_ps(_mm_cmple_ps(vY, vPosW));

        mask5 = _mm_movemask_ps(_mm_cmpge_ps(vW, _mm_setzero_ps()));
        int inside = (mask1 & mask2 & mask3 & mask4 & mask5);
        if ((inside & 0x7) != 0x7)
        {
            RDTSC_EVENT(FEGuardbandClip, 1, 0);

            OSALIGN(float, 16) inVerts[3 * 4];
            OSALIGN(float, 16) tempPts[6 * 4];

            const float *pOutAttribs = pAttribs;

            OSALIGN(float, 16) outAttribs[6 * 8];
            OSALIGN(float, 16) tempAttribs[6 * 8];

            // Copy attributes for triangle into tempAttribs for clipping.
            for (UINT i = 0; i < 3; ++i)
            {
                int idx = index[i];
                for (UINT a = 0; a < numAttribs; ++a)
                {
                    tempAttribs[i * numAttribs + a] = pAttribs[idx * numAttribs + a];
                }
            }

            pOutAttribs = &tempAttribs[0];

            // transpose to x,y,z,w
            __m128 v0 = vX, v1 = vY, v2 = vZ, v3 = vW;
            vTranspose(v0, v1, v2, v3);

            _mm_store_ps(&inVerts[0], v0);
            _mm_store_ps(&inVerts[4], v1);
            _mm_store_ps(&inVerts[8], v2);

            int NumOutPts = 0;
            Clip(inVerts, tempAttribs, numAttribs, tempPts, &NumOutPts, outAttribs);

            RDTSC_STOP(FEGuardbandClip, 1, 0);

            // call PA again with TRI_FAN on the new verts
            if (NumOutPts >= 3)
            {
                PrimitiveAssembly<TOP_TRIANGLE_FAN, false>(pDC, NumOutPts - 2, NULL, tempPts, pOutAttribs, numAttribs);
            }

            return;
        }
    }

    __m128 vOrigX = vX, vOrigY = vY, vOrigZ = vZ;

    // perspective divide
    __m128 vRecipW = _mm_div_ps(_mm_set1_ps(1.0f), vW); //_mm_rcp_ps(vW);
    vX = _mm_mul_ps(vX, vRecipW);
    vY = _mm_mul_ps(vY, vRecipW);
    vZ = _mm_mul_ps(vZ, vRecipW);

    // viewport transform to screen coords
    transform(vX, vY, vZ, state, pContext->driverType);

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

    // flip det for GL since Y is inverted
    if (pContext->driverType == GL)
    {
        det = -det;
    }

    // Cull zero-area and back-facing triangles
    if ((det == 0.0) ||
        (state.cullMode == CCW && det < 0.0) ||
        (state.cullMode == CW && det > 0.0))
    {
        RDTSC_EVENT(FECullZeroAreaAndBackface, 1, 0);
        return;
    }

    BE_WORK work;
#if defined(_DEBUG)
    work.type = DRAW;
#endif
    work.pfnWork = rastLargeTri;
    TRIANGLE_WORK_DESC &desc = work.desc.tri;

    // store face
    // @todo plumb front/back winding in rast state to know whether this is front or back
    work.desc.tri.triFlags.backFacing = (det > 0.0);

    // Calc bounding box of triangle
    OSALIGN(BBOX, 16) bbox;
    calcBoundingBoxInt(vXi, vYi, bbox);

    // small triangle optimization - for triangles within a single quad, check to see if the bbox
    // intersects the 4 pixel samples
    int left = (bbox.left + 127) & ~255;
    int right = (bbox.right + 128) & ~255;
    int top = (bbox.top + 127) & ~255;
    int bottom = (bbox.bottom + 128) & ~255;

    if (left == right || top == bottom)
    {
        RDTSC_EVENT(FECullZeroAreaAndBackface, 1, 0);
        return;
    }

    // convert to tiles
    bbox.left >>= KNOB_TILE_X_DIM_SHIFT + FIXED_POINT_WIDTH;
    bbox.right >>= KNOB_TILE_X_DIM_SHIFT + FIXED_POINT_WIDTH;
    bbox.top >>= KNOB_TILE_Y_DIM_SHIFT + FIXED_POINT_WIDTH;
    bbox.bottom >>= KNOB_TILE_Y_DIM_SHIFT + FIXED_POINT_WIDTH;

    // small tri check
    if (((bbox.right - bbox.left + 1) <= SMALL_TRI_X_TILES) &&
        ((bbox.bottom - bbox.top + 1) <= SMALL_TRI_Y_TILES))
    {
        work.pfnWork = rastSmallTri;
    }

    bbox.left = std::max(bbox.left, apiState.scissorInTiles.left);
    bbox.top = std::max(bbox.top, apiState.scissorInTiles.top);
    bbox.right = std::min(bbox.right, (int)apiState.scissorInTiles.right);
    bbox.bottom = std::min(bbox.bottom, (int)apiState.scissorInTiles.bottom);

    if ((bbox.left > bbox.right) || (bbox.top > bbox.bottom))
    {
        assert(0);
        return;
    }

    // set up attribs
    float *pAttribBuffer = NULL;
    if (numAttribs)
    {
        //float *pTempBuffer = desc.interpBuffer;
        desc.pInterpBuffer = (float *)pDC->arena.AllocAligned(numAttribs * 3 * sizeof(float), 16);
        float *pTempBuffer = desc.pInterpBuffer;

        // store attribs, 4 at a time
        // interp = (i0-i2)*i + (i1-i2)*j + i2
        for (UINT a = 0; a < numAttribs; a += 4)
        {
            __m128 vInterpEdge0 = _mm_load_ps(&pAttribs[index[0] * numAttribs + a]);
            __m128 vInterpEdge1 = _mm_load_ps(&pAttribs[index[1] * numAttribs + a]);
            __m128 vInterpEdge2 = _mm_load_ps(&pAttribs[index[2] * numAttribs + a]);

            vInterpEdge0 = _mm_mul_ps(vInterpEdge0, _mm_shuffle_ps(vRecipW, vRecipW, _MM_SHUFFLE(0, 0, 0, 0)));
            vInterpEdge1 = _mm_mul_ps(vInterpEdge1, _mm_shuffle_ps(vRecipW, vRecipW, _MM_SHUFFLE(1, 1, 1, 1)));
            vInterpEdge2 = _mm_mul_ps(vInterpEdge2, _mm_shuffle_ps(vRecipW, vRecipW, _MM_SHUFFLE(2, 2, 2, 2)));

            __m128 vInterpA = _mm_sub_ps(vInterpEdge0, vInterpEdge2);
            __m128 vInterpB = _mm_sub_ps(vInterpEdge1, vInterpEdge2);

            _mm_store_ps(pTempBuffer, vInterpA);
            _mm_store_ps(pTempBuffer += 4, vInterpB);
            _mm_store_ps(pTempBuffer += 4, vInterpEdge2);
            pTempBuffer += 4;
        }
    }

    desc.pTriBuffer = (float *)pDC->arena.AllocAligned(4 * 4 * sizeof(float), 16);

    _mm_store_ps(desc.pTriBuffer, vX);
    _mm_store_ps(desc.pTriBuffer + 4, vY);
    _mm_store_ps(desc.pTriBuffer + 8, vZ);
    _mm_store_ps(desc.pTriBuffer + 12, vRecipW);

    __m128 vRcpDim = _mm_load_ps((const float *)&apiState.vScissorRcpDim);
    __m128i vBBox = _mm_load_si128((const __m128i *)&bbox);
    vRcpDim = _mm_mul_ps(vRcpDim, _mm_cvtepi32_ps(vBBox));
    __m128i vRcpDimInt = _mm_cvttps_epi32(vRcpDim);

    OSALIGN(UINT, 16) vMT[4];
    _mm_store_si128((__m128i *)vMT, vRcpDimInt);

    UINT leftMacroTile = vMT[2];
    UINT rightMacroTile = vMT[3];
    UINT topMacroTile = vMT[0];
    UINT botMacroTile = vMT[1];

    MacroTileMgr *pTileMgr = pDC->pTileMgr;
    for (UINT y = topMacroTile; y <= botMacroTile; ++y)
    {
        for (UINT x = leftMacroTile; x <= rightMacroTile; ++x)
        {
#ifndef KNOB_TOSS_BIN_TRIS
#if KNOB_VERTICALIZED_BINNER == 0
            pTileMgr->enqueue(x, y, &work);
#endif
#endif
        }
    }
}
