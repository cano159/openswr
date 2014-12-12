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

#include <cfloat>
#include <cmath>
#include <cstdio>

#if defined(__gnu_linux__) || defined(__linux__)
#include <numa.h>
#endif

#include "api.h"
#include "backend.h"
#include "context.h"
#include "frontend.h"
#include "os.h"
#include "rasterizer.h"
#include "rdtsc.h"
#include "simdintrin.h"
#include "threads.h"
#include "tilemgr.h"

UINT64 g_StartTimeStamp = 0;

void SetupDefaultState(SWR_CONTEXT *pContext);

#if defined(WIN32)
double GetFrequencyInMs()
{
    LARGE_INTEGER freq = { 0 };
    QueryPerformanceFrequency(&freq);
    return (double)freq.QuadPart;
}

#else
double GetFrequencyInMs()
{
    timespec ts = { 0 }, trem;
    ts.tv_nsec = 1000000;
    UINT64 start = __rdtsc();
    nanosleep(&ts, &trem);
    return __rdtsc() - start;
}
#endif

static double frequencyMs = GetFrequencyInMs();

// Get instantaneous fps
FLOAT GetInstantFps(UINT64 prevSample, UINT64 timestamp)
{
    FLOAT fps = 0.f;

    if (prevSample != 0)
    {
        float frameTime = (float)(double(timestamp - prevSample) / frequencyMs);
        fps = 1000.0f / frameTime;
    }

    return fps;
}

#ifdef KNOB_RUNNING_AVERAGE_FPS

FLOAT GetFps(UINT64 prevSample, UINT64 timeStamp)
{
    static FLOAT PreviousDiffs[KNOB_MAX_PREVIOUS_FRAMES_FOR_FPS_AVG] = { 0 };
    static FLOAT PreviousDiffs2[KNOB_MAX_PREVIOUS_FRAMES_FOR_FPS_AVG] = { 0 };
    static FLOAT RunningSum = 0;
    static FLOAT RunningSum2 = 0;
    static INT Marker = 0;
    static UINT NumFramesForAverage = 0;

    NumFramesForAverage = std::min(NumFramesForAverage + 1, KNOB_MAX_PREVIOUS_FRAMES_FOR_FPS_AVG);

    if (Marker >= KNOB_MAX_PREVIOUS_FRAMES_FOR_FPS_AVG)
    {
        Marker = 0;
    }

    RunningSum -= PreviousDiffs[Marker];
    RunningSum2 -= PreviousDiffs2[Marker];
    if (prevSample != 0)
    {
        PreviousDiffs[Marker] = (float)((timeStamp - prevSample) / frequencyMs);
        PreviousDiffs2[Marker] = PreviousDiffs[Marker] * PreviousDiffs[Marker];
    }
    RunningSum += PreviousDiffs[Marker];
    RunningSum2 += PreviousDiffs2[Marker];

    ++Marker;

    FLOAT avg = RunningSum / NumFramesForAverage;

    FLOAT dev = sqrt(RunningSum2 / NumFramesForAverage - avg * avg);

    return 1000.0f / avg;
}

#else

FLOAT GetFps(UINT64 prevSample, UINT64 timestamp)
{
    static const UINT MaxPrevFramesForFpsAvg = KNOB_MAX_PREVIOUS_FRAMES_FOR_FPS_AVG;
    static UINT MaxPrevFramesForFpsAvgCount = 0;

    MaxPrevFramesForFpsAvgCount = std::min(MaxPrevFramesForFpsAvgCount + 1, MaxPrevFramesForFpsAvg);
    static FLOAT prevFramesFps[MaxPrevFramesForFpsAvg] = { 0 }; // Track previous N frames to average fps for.
    FLOAT curFps = GetInstantFps(prevSample, timestamp);
    FLOAT avgFps = curFps + prevFramesFps[0];

    for (UINT i = 1; i < MaxPrevFramesForFpsAvg; ++i)
    {
        avgFps += prevFramesFps[i];
        prevFramesFps[i - 1] = prevFramesFps[i];
    }

    prevFramesFps[MaxPrevFramesForFpsAvg - 1] = curFps;

    return avgFps / (MaxPrevFramesForFpsAvgCount + 1); // average N prev frames + cur frame
}

#endif

#ifdef _WIN32
void DumpFPS(SWR_CONTEXT *pContext, UINT64 prevSample, UINT64 timestamp)
{
    static FLOAT minFps = FLT_MAX;
    static FLOAT maxFps = 0.0f;
    static UINT frameCounter = 0;

    // dump FPS info
    FLOAT fps = GetFps(prevSample, timestamp);

    if (fps > 0.0f)
    {
        char buf[255];

        if (frameCounter++ > 100)
        {
            minFps = (fps < minFps) ? fps : minFps;
            maxFps = (fps > maxFps) ? fps : maxFps;
            sprintf(buf, "AVG: %2.2f    MIN: %2.2f    MAX: %2.2f    FRAME: %u", fps, minFps, maxFps, frameCounter);
        }
        else
        {
            sprintf(buf, "AVG: %2.2f    FRAME: %u", fps, frameCounter);
        }

        pContext->pSwapChain->DrawString(10, 10, buf, (int)strlen(buf));
    }
}
#endif

// Given a triangle index and a topology, generates the indices for the triangle
UINT NumElementsGivenIndices(PRIMITIVE_TOPOLOGY mode, UINT numElements)
{
    switch (mode)
    {
    case TOP_POINTS:
        return numElements;
    case TOP_TRIANGLE_LIST:
        return numElements / 3;
    case TOP_TRIANGLE_STRIP:
        return numElements - 2;
    case TOP_TRIANGLE_FAN:
        return numElements - 2;
    case TOP_TRIANGLE_DISC:
        return numElements - 1;
    case TOP_QUAD_LIST:
        return numElements / 4;
    case TOP_QUAD_STRIP:
        return (numElements - 2) / 2;
    case TOP_LINE_STRIP:
        return numElements - 1;
    case TOP_LINE_LIST:
        return numElements / 2;
    }

    return 0;
}

UINT NumPrimitiveVerts(PRIMITIVE_TOPOLOGY t, UINT numPrims)
{
    switch (t)
    {
    case TOP_POINTS:
        return numPrims;
    case TOP_TRIANGLE_LIST:
        return numPrims * 3;
    case TOP_TRIANGLE_STRIP:
        return numPrims + 2;
    case TOP_TRIANGLE_FAN:
        return numPrims + 2;
    case TOP_QUAD_LIST:
        return numPrims * 4;
    case TOP_QUAD_STRIP:
        return 2 * numPrims + 2;
    case TOP_LINE_STRIP:
        return numPrims + 1;
    case TOP_LINE_LIST:
        return numPrims * 2;
    }
    return 0;
}

UINT PrimitiveIndex(UINT prim, UINT corner, PRIMITIVE_TOPOLOGY t)
{
    static const UINT TriStrip[2][3] =
        {
          { 0, 1, 2 },
          { 2, 1, 3 }
        };
    static const UINT QuadList[6] =
        {
          0, 3, 1, 3, 2, 1
        };
    static const UINT QuadStrip[2][6] =
        {
          { 1, 3, 0, 3, 2, 0 },
          { 1, 0, 3, 3, 0, 2 }
        };

    switch (t)
    {
    case TOP_TRIANGLE_LIST:
        return prim * 3 + corner;
    case TOP_TRIANGLE_STRIP:
        return TriStrip[prim & 0x1][corner];
    case TOP_TRIANGLE_DISC:
    case TOP_TRIANGLE_FAN:
        return corner == 0 ? 0 : prim + corner;
    case TOP_QUAD_LIST:
        return prim * 4 + QuadList[corner];
    case TOP_QUAD_STRIP:
        return prim * 2 + QuadStrip[prim & 0x1][corner];
    }

    return 0;
}

#ifdef _WIN32
void SwrCreateSwapChain(HANDLE hContext, HANDLE hDisplay, HANDLE hWnd, HANDLE hVisInfo, UINT numBuffers, UINT width, UINT height)
{
    SWR_CONTEXT *pContext = (SWR_CONTEXT *)hContext;

    if (pContext->pSwapChain != NULL)
    {
        pContext->pSwapChain->Destroy();
    }

    SWAP_CHAIN *pSwapChain = (SWAP_CHAIN *)malloc(sizeof(SWAP_CHAIN));
    pSwapChain->Initialize(pContext, hDisplay, hWnd, hVisInfo, width, height, numBuffers);

    pContext->pSwapChain = pSwapChain;
}

void SwrDestroySwapChain(HANDLE hContext)
{
    SWR_CONTEXT *pContext = (SWR_CONTEXT *)hContext;
    if (pContext->pSwapChain != NULL)
    {
        pContext->pSwapChain->Destroy();
        free(pContext->pSwapChain);
        pContext->pSwapChain = NULL;
    }
}

void SwrResizeSwapChain(HANDLE hContext, UINT width, UINT height)
{
    SWR_CONTEXT *pContext = (SWR_CONTEXT *)hContext;
    pContext->pSwapChain->Resize(width, height);
}

HANDLE SwrGetBackBuffer(HANDLE hContext, UINT index)
{
    SWR_CONTEXT *pContext = (SWR_CONTEXT *)hContext;
    return pContext->pSwapChain->GetBackBuffer(index);
}

HANDLE SwrGetDepthBuffer(HANDLE hContext)
{
    SWR_CONTEXT *pContext = (SWR_CONTEXT *)hContext;
    return pContext->pSwapChain->GetDepthBuffer();
}
#endif

HANDLE SwrCreateContext(DRIVER_TYPE driver)
{
    RDTSC_INIT(0);

    SWR_CONTEXT *pContext = (SWR_CONTEXT *)_aligned_malloc(sizeof(SWR_CONTEXT), KNOB_VS_SIMD_WIDTH * 4);

    memset(pContext, 0, sizeof(SWR_CONTEXT));

    pContext->driverType = driver;

    pContext->dcRing = (DRAW_CONTEXT *)_aligned_malloc(sizeof(DRAW_CONTEXT) * KNOB_MAX_DRAWS_IN_FLIGHT, 64);
    memset(pContext->dcRing, 0, sizeof(DRAW_CONTEXT) * KNOB_MAX_DRAWS_IN_FLIGHT);

    for (INT dc = 0; dc < KNOB_MAX_DRAWS_IN_FLIGHT; ++dc)
    {
        pContext->dcRing[dc].arena.Init();
        pContext->dcRing[dc].inUse = false;
        pContext->dcRing[dc].pTileMgr = new MacroTileMgr();
    }

#if KNOB_SINGLE_THREADED
    pContext->NumWorkerThreads = 1;
#else
    memset(&pContext->WaitLock, 0, sizeof(pContext->WaitLock));
    memset(&pContext->FifosNotEmpty, 0, sizeof(pContext->FifosNotEmpty));
    new (&pContext->WaitLock) std::mutex();
    new (&pContext->FifosNotEmpty) std::condition_variable();

    createThreadPool(pContext, &pContext->threadPool);
#endif

    pContext->nextDrawId = 0;

    pContext->dcIndex = 0; // draw id must match dc index

    // State setup AFTER context is fully initialized
    SetupDefaultState(pContext);

    pContext->numNumaNodes = 0;

#if !KNOB_SINGLE_THREADED
// Query NUMA topology
#if defined(_WIN32)
    SYSTEM_LOGICAL_PROCESSOR_INFORMATION procInfo[128];
    DWORD length = sizeof(procInfo);
    if (GetLogicalProcessorInformation(procInfo, &length))
    {
        UINT i = 0;
        while (length)
        {
            if (procInfo[i].Relationship == RelationNumaNode)
            {
                pContext->numNumaNodes++;
            }
            length -= sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
            i++;
        }
    }
#else
    pContext->numNumaNodes = numa_max_node() + 1;
#endif
#endif

#if !KNOB_ENABLE_NUMA
    pContext->numNumaNodes = 1;
#endif

    if (pContext->numNumaNodes == 0)
    {
        pContext->numNumaNodes = 1;
    }

    return (HANDLE)pContext;
}

void DebugInfoState(HANDLE hContext, UINT eDebugInfo, BOOL Flag)
{
    SWR_CONTEXT *pContext = (SWR_CONTEXT *)hContext;

    switch (eDebugInfo)
    {
    case DBGINFO_DUMP_FPS:
        pContext->dumpFPS = Flag;
        break;
    case DBGINFO_DUMP_POOL_INFO:
        pContext->dumpPoolInfo = Flag;
        break;
    default:
        break;
    }
}

void SwrSetDebugInfo(HANDLE hContext, UINT eDebugInfo)
{
    DebugInfoState(hContext, eDebugInfo, 1);
}

void SwrUnsetDebugInfo(HANDLE hContext, UINT eDebugInfo)
{
    DebugInfoState(hContext, eDebugInfo, 0);
}

void SwrDestroyContext(HANDLE hContext)
{
    SWR_CONTEXT *pContext = (SWR_CONTEXT *)hContext;
    destroyThreadPool(pContext, &pContext->threadPool);

    // free the fifos
    for (UINT i = 0; i < KNOB_MAX_DRAWS_IN_FLIGHT; ++i)
    {
        delete (pContext->dcRing[i].pTileMgr);
    }

#ifdef _WIN32
    SwrDestroySwapChain(pContext);
#endif

    _aligned_free(pContext->dcRing);

    _aligned_free((SWR_CONTEXT *)hContext);
}

void CopyState(DRAW_CONTEXT &dst, const DRAW_CONTEXT &src)
{
    memcpy(&dst.state, &src.state, sizeof(API_STATE));
}

void QueueDraw(SWR_CONTEXT *pContext)
{
    _ReadWriteBarrier();
    pContext->DrawEnqueued++;

#if KNOB_SINGLE_THREADED
    WorkOnFifoFE(pContext, 0, pContext->WorkerFE[0], 0);
    WorkOnFifoBE(pContext, 0, pContext->WorkerFE[0], pContext->WorkerBE[0], std::set<UINT>());
#else
    RDTSC_START(APIDrawWakeAllThreads);
    WakeAllThreads(pContext);
    RDTSC_STOP(APIDrawWakeAllThreads, 1, 0);
#endif

    // Set current draw context to NULL so that next state call forces a new draw context to be created and populated.
    pContext->pPrevDrawContext = pContext->pCurDrawContext;
    pContext->pCurDrawContext = NULL;
}

DRAW_CONTEXT *GetDrawContext(SWR_CONTEXT *pContext)
{
    RDTSC_START(APIGetDrawContext);
    // If current draw context is null then need to obtain a new draw context to use from ring.
    if (pContext->pCurDrawContext == NULL)
    {
        pContext->pCurDrawContext = &pContext->dcRing[pContext->dcIndex];

        // Update LastRetiredId
        UpdateLastRetiredId(pContext);

        // Need to wait until this draw context is available to use.
        while (StillDrawing(pContext, pContext->pCurDrawContext))
        {
            // Make sure workers are working.
            WakeAllThreads(pContext);

            _mm_pause();
        }

        // Move current
        pContext->dcIndex = (pContext->dcIndex + 1) % KNOB_MAX_DRAWS_IN_FLIGHT;

        // Copy previous state to current state.
        if (pContext->pPrevDrawContext)
        {
            CopyState(*pContext->pCurDrawContext, *pContext->pPrevDrawContext);
        }

        pContext->pCurDrawContext->pContext = pContext;

        pContext->pCurDrawContext->inUse = false;
        pContext->pCurDrawContext->arena.Reset(); // Reset memory.

        pContext->pCurDrawContext->doneFE = false;
        pContext->pCurDrawContext->FeLock = 0;
        pContext->pCurDrawContext->depCompleteDraw = false;

        pContext->pCurDrawContext->pfnCallbackFunc = NULL;

        // TODO grab format from state
        pContext->pCurDrawContext->pTileMgr->initialize(BGRA8_UNORM);

        // Assign unique drawId for this DC
        pContext->pCurDrawContext->drawId = pContext->nextDrawId++;
    }

    RDTSC_STOP(APIGetDrawContext, 0, 0);
    return pContext->pCurDrawContext;
}

API_STATE *GetDrawState(SWR_CONTEXT *pContext)
{
    DRAW_CONTEXT *pDC = GetDrawContext(pContext);
    return &pDC->state;
}

void SetupDefaultState(SWR_CONTEXT *pContext)
{
    API_STATE *pState = GetDrawState(pContext);

    pState->rastState.cullMode = NONE;
}

static INLINE SWR_CONTEXT *GetContext(HANDLE hContext)
{
    return (SWR_CONTEXT *)hContext;
}

void SwrSetNumAttributes(
    HANDLE hContext,
    UINT numAttrs)
{
    API_STATE *pState = GetDrawState(GetContext(hContext));

    pState->numAttributes = numAttrs;
}

UINT SwrGetNumAttributes(
    HANDLE hContext)
{
    API_STATE *pState = GetDrawState(GetContext(hContext));

    return pState->numAttributes;
}

UINT SwrGetPermuteWidth(
    HANDLE hContext)
{
    return KNOB_VS_SIMD_WIDTH;
}

void SwrSetVertexBuffer(
    HANDLE hContext,
    HANDLE hVertexBuffer)
{
    API_STATE *pState = GetDrawState(GetContext(hContext));
    Buffer *pVertexBuffer = (Buffer *)hVertexBuffer;
    pState->ppVertexBuffer[0] = pVertexBuffer;
}

void SwrSetVertexBuffers(
    HANDLE hContext,
    UINT indexStart,
    UINT numBuffers,
    const HANDLE *phVertexBuffers)
{
    API_STATE *pState = GetDrawState(GetContext(hContext));

    memset(pState->ppVertexBuffer, 0, sizeof(pState->ppVertexBuffer));
    for (UINT i = 0; i < numBuffers; ++i)
    {
        pState->ppVertexBuffer[i + indexStart] = (Buffer *)phVertexBuffers[i];
    }
}

void SwrSetIndexBuffer(
    HANDLE hContext,
    HANDLE hIndexBuffer)
{
    API_STATE *pState = GetDrawState(GetContext(hContext));

    pState->pIndexBuffer = (Buffer *)hIndexBuffer;
}

void SwrSetFetchFunc(
    HANDLE hContext,
    PFN_FETCH_FUNC pfnFetchFunc)
{
    API_STATE *pState = GetDrawState(GetContext(hContext));

    pState->pfnFetchFunc = pfnFetchFunc;
}

void SwrSetVertexFunc(
    HANDLE hContext,
    PFN_VERTEX_FUNC pfnVertexFunc)
{
    API_STATE *pState = GetDrawState(GetContext(hContext));

    pState->pfnVertexFunc = pfnVertexFunc;
}

void SwrSetPixelFunc(
    HANDLE hContext,
    PFN_PIXEL_FUNC pfnPixelFunc)
{
    API_STATE *pState = GetDrawState(GetContext(hContext));

    pState->pfnPixelFunc = pfnPixelFunc;
}

void SwrSetLinkageMaskFrontFace(
    HANDLE hContext,
    UINT linkageMask)
{
    API_STATE *pState = GetDrawState(GetContext(hContext));

    pState->linkageMaskFrontFace = linkageMask;
    pState->linkageCountFrontFace = 0; // A count of the attributes from mask.

    DWORD slot = 0;
    while (_BitScanForward(&slot, linkageMask))
    {
        pState->linkageCountFrontFace++;
        linkageMask &= ~(1 << slot); // done with this bit.
    }
}

void SwrSetLinkageMaskBackFace(
    HANDLE hContext,
    UINT linkageMask)
{
    API_STATE *pState = GetDrawState(GetContext(hContext));

    pState->linkageMaskBackFace = linkageMask;
    pState->linkageCountBackFace = 0; // A count of the attributes from mask.

    DWORD slot = 0;
    while (_BitScanForward(&slot, linkageMask))
    {
        pState->linkageCountBackFace++;
        linkageMask &= ~(1 << slot); // done with this bit.
    }
}

void SwrGetRastState(
    HANDLE hContext,
    RASTSTATE *pRastState)
{
    SWR_CONTEXT *pContext = GetContext(hContext);
    API_STATE *pState = GetDrawState(pContext);

    memcpy(pRastState, &pState->rastState, sizeof(RASTSTATE));
}

// update guardband multipliers for the viewport
void updateGuardband(API_STATE *pState)
{
    // guardband center is viewport center
    pState->gbState.left = KNOB_GUARDBAND_WIDTH / pState->rastState.vp.width;
    pState->gbState.right = KNOB_GUARDBAND_WIDTH / pState->rastState.vp.width;
    pState->gbState.top = KNOB_GUARDBAND_HEIGHT / pState->rastState.vp.height;
    pState->gbState.bottom = KNOB_GUARDBAND_HEIGHT / pState->rastState.vp.height;
}

void SwrSetRastState(
    HANDLE hContext,
    const RASTSTATE *pRastState)
{
    SWR_CONTEXT *pContext = GetContext(hContext);
    API_STATE *pState = GetDrawState(pContext);

    memcpy(&pState->rastState, pRastState, sizeof(RASTSTATE));

    updateGuardband(pState);
}

void SwrSetScissorRect(
    HANDLE hContext,
    UINT left, UINT top, UINT right, UINT bottom)
{
    API_STATE *pState = GetDrawState(GetContext(hContext));
    pState->scissorRect.top = top;
    pState->scissorRect.left = left;
    pState->scissorRect.right = right;
    pState->scissorRect.bottom = bottom;
};

void AddDependencies(DRAW_CONTEXT *pDC)
{
    API_STATE *pState = &pDC->state;

    // render target write
    for (UINT a = 0; a < SWR_NUM_ATTACHMENTS; ++a)
    {
        if (pState->pRenderTargets[a])
            pState->pRenderTargets[a]->AddWriteDependency(&pDC->dependency, pDC->drawId);
    }

    // vertex buffer read
    for (UINT i = 0; i < KNOB_NUM_STREAMS; ++i)
    {
        if (pState->ppVertexBuffer[i])
        {
            pState->ppVertexBuffer[i]->AddReadDependency(&pDC->dependency, pDC->drawId);
        }
    }

    // index buffer read
    if (pState->pIndexBuffer)
        pState->pIndexBuffer->AddReadDependency(&pDC->dependency, pDC->drawId);

    // constant buffers optional
    if (pState->pFSConstantBuffer)
        pState->pFSConstantBuffer->AddReadDependency(&pDC->dependency, pDC->drawId);
    if (pState->pVSConstantBuffer)
        pState->pVSConstantBuffer->AddReadDependency(&pDC->dependency, pDC->drawId);
    if (pState->pPSConstantBuffer)
        pState->pPSConstantBuffer->AddReadDependency(&pDC->dependency, pDC->drawId);

    // add texture dependencies
    // @todo only handles mip 0.  we really need to pack mips into a single allocation
    for (UINT i = 0; i < NUM_GRAPHICS_SHADER_TYPES; ++i)
    {
        for (UINT j = 0; j < API_STATE::NUM_TEXTURE_VIEWS; ++j)
        {
            if (pState->aTextureViews[i][j])
            {
                SWR_TEXTUREVIEW *pTextureView = (SWR_TEXTUREVIEW *)pState->aTextureViews[i][j];
                Texture *pTexture = (Texture *)pTextureView->hTexture;
                Resource *pTextureRes = (Resource *)pTexture->mhSubtextures[0];
                pTextureRes->AddReadDependency(&pDC->dependency, pDC->drawId);
            }
        }
    }

    // Add pointers to underlying resource allocations
    memset(pState->ppVertexBufferAlloc, 0x0, sizeof(pState->ppVertexBufferAlloc));
    for (UINT i = 0; i < KNOB_NUM_STREAMS; ++i)
    {
        if (pState->ppVertexBuffer[i])
        {
            pState->ppVertexBufferAlloc[i] = pState->ppVertexBuffer[i]->GetCurrentAllocation();
        }
    }

    if (pState->pIndexBuffer)
        pState->pIndexBufferAlloc = pState->pIndexBuffer->GetCurrentAllocation();

    if (pState->pFSConstantBuffer)
        pState->pFSConstantBufferAlloc = pState->pFSConstantBuffer->GetCurrentAllocation();
    if (pState->pVSConstantBuffer)
        pState->pVSConstantBufferAlloc = pState->pVSConstantBuffer->GetCurrentAllocation();
    if (pState->pPSConstantBuffer)
        pState->pPSConstantBufferAlloc = pState->pPSConstantBuffer->GetCurrentAllocation();
};

void AddDependenciesNonDraw(DRAW_CONTEXT *pDC)
{
    API_STATE *pState = &pDC->state;
    for (UINT a = 0; a < SWR_NUM_ATTACHMENTS; ++a)
    {
        if (pState->pRenderTargets[a])
            pState->pRenderTargets[a]->AddWriteDependency(&pDC->dependency, pDC->drawId);
    }
};

template <bool UseInstance>
void DrawIndexedInstance(
    HANDLE hContext,
    PRIMITIVE_TOPOLOGY topology,
    SWR_TYPE type,
    UINT numIndices,
    UINT whichInstance,
    UINT indexOffset);

void SwrDrawIndexed(
    HANDLE hContext,
    PRIMITIVE_TOPOLOGY topology,
    SWR_TYPE type,
    UINT numIndices,
    UINT indexOffset)
{
    DrawIndexedInstance<false>(hContext, topology, type, numIndices, 0, indexOffset);
}

void SwrDrawIndexedInstanced(
    HANDLE hContext,
    PRIMITIVE_TOPOLOGY topology,
    SWR_TYPE type,
    UINT numIndices,
    UINT numInstances,
    UINT indexOffset)
{
    for (UINT i = 0; i < numInstances; ++i)
    {
        DrawIndexedInstance<true>(hContext, topology, type, numIndices, i, indexOffset);
    }
}

void SetupMacroTileScissors(DRAW_CONTEXT *pDC)
{
    API_STATE *pState = &pDC->state;
    UINT left, right, top, bottom;

    // Set up scissor dimensions based on scissor or viewport
    if (pState->rastState.scissorEnable)
    {
        // scissor rect right/bottom edge are exclusive, core expects scissor dimensions to be inclusive, so subtract one pixel from right/bottom edges
        left = pState->scissorRect.left;
        right = pState->scissorRect.right - 1;
        top = pState->scissorRect.top;
        bottom = pState->scissorRect.bottom - 1;
    }
    else
    {
        left = (INT)pState->rastState.vp.x;
        right = (INT)pState->rastState.vp.x + (INT)pState->rastState.vp.width - 1;
        top = (INT)pState->rastState.vp.y;
        bottom = (INT)pState->rastState.vp.y + (INT)pState->rastState.vp.height - 1;
    }

    pState->scissorInTiles.left = left >> KNOB_TILE_X_DIM_SHIFT;
    pState->scissorInTiles.right = right >> KNOB_TILE_X_DIM_SHIFT;
    pState->scissorInTiles.top = top >> KNOB_TILE_Y_DIM_SHIFT;
    pState->scissorInTiles.bottom = bottom >> KNOB_TILE_Y_DIM_SHIFT;

    UINT width = right + 1;
    UINT height = bottom + 1;

    UINT macroWidth = pDC->pTileMgr->getTileWidth();
    UINT macroHeight = pDC->pTileMgr->getTileHeight();

    pState->scissorMacroWidthInTiles = macroWidth >> KNOB_TILE_X_DIM_SHIFT;
    pState->scissorMacroHeightInTiles = macroHeight >> KNOB_TILE_Y_DIM_SHIFT;

    // TODO this goes away
    pState->vScissorRcpDim[0] = 1.0f / pState->scissorMacroHeightInTiles;
    pState->vScissorRcpDim[1] = 1.0f / pState->scissorMacroHeightInTiles;
    pState->vScissorRcpDim[2] = 1.0f / pState->scissorMacroWidthInTiles;
    pState->vScissorRcpDim[3] = 1.0f / pState->scissorMacroWidthInTiles;

#if KNOB_VS_SIMD_WIDTH == 8
    pState->vScissorRcpDim[4] = 1.0f / pState->scissorMacroHeightInTiles;
    pState->vScissorRcpDim[5] = 1.0f / pState->scissorMacroHeightInTiles;
    pState->vScissorRcpDim[6] = 1.0f / pState->scissorMacroWidthInTiles;
    pState->vScissorRcpDim[7] = 1.0f / pState->scissorMacroWidthInTiles;
#endif
}

void InitDraw(
    DRAW_CONTEXT *pDC)
{
    API_STATE *pState = &pDC->state;

    // Set up dependencies
    AddDependencies(pDC);

    SetupMacroTileScissors(pDC);

    pDC->inUse = true; // We are using this one now.

    // XXX: This is temporary code. Will be deleted.
    UINT linkageMask = pDC->state.linkageMaskBackFace | pDC->state.linkageMaskFrontFace;
    pState->linkageTotalCount = 0;
    DWORD slot = 0;
    while (_BitScanForward(&slot, linkageMask))
    {
        pState->linkageTotalCount++;
        linkageMask &= ~(1 << slot); // done with this bit.
    }
}

void SwrDraw(
    HANDLE hContext,
    PRIMITIVE_TOPOLOGY topology,
    UINT startVertex,
    UINT primCount)
{
    RDTSC_START(APIDraw);

#ifdef KNOB_TOSS_DRAW
    return;
#endif

    SWR_CONTEXT *pContext = GetContext(hContext);

    INT maxVertsPerDraw = KNOB_MAX_PRIMS_PER_DRAW;
    INT remainingVerts = NumPrimitiveVerts(topology, primCount);

    int draw = 0;
    while (remainingVerts)
    {
        UINT numVertsForDraw = (remainingVerts < maxVertsPerDraw) ?
                                   remainingVerts :
                                   maxVertsPerDraw;

        DRAW_CONTEXT *pDC = GetDrawContext(pContext);
        API_STATE *pState = &pDC->state;
        pState->topology = topology;

        InitDraw(pDC);

        pDC->FeWork.type = DRAW;
        pDC->FeWork.pfnWork = ProcessDraw;
        pDC->FeWork.desc.draw.numVerts = numVertsForDraw;
        pDC->FeWork.desc.draw.startVertex = startVertex + draw * maxVertsPerDraw;

        //enqueue DC
        QueueDraw(pContext);

        remainingVerts -= numVertsForDraw;
        draw++;
    }
    RDTSC_STOP(APIDraw, (primCount * 3), 0);
}

template <bool UseInstance>
void DrawIndexedInstance(
    HANDLE hContext,
    PRIMITIVE_TOPOLOGY topology,
    SWR_TYPE type,
    UINT numIndices,
    UINT wInstance,
    UINT indexOffset)
{
    RDTSC_START(APIDrawIndexed);

    SWR_CONTEXT *pContext = GetContext(hContext);

    INT maxIndicesPerDraw = KNOB_MAX_PRIMS_PER_DRAW;
    INT remainingIndices = numIndices;
    UINT whichInstance = UseInstance ? wInstance : 0;

    int indexSize = 0;
    switch (type)
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

    int draw = 0;
    BYTE *pIB = (BYTE *)GetDrawContext(pContext)->state.pIndexBuffer->GetCurrentAllocation()->pData;
    pIB += indexOffset;

    while (remainingIndices)
    {
        UINT numIndicesForDraw = (remainingIndices < maxIndicesPerDraw) ?
                                     remainingIndices :
                                     maxIndicesPerDraw;

        DRAW_CONTEXT *pDC = GetDrawContext(pContext);
        API_STATE *pState = &pDC->state;
        pState->topology = topology;

        InitDraw(pDC);

        pDC->FeWork.type = DRAW;
        pDC->FeWork.pfnWork = ProcessDrawIndexed;
        pDC->FeWork.desc.draw.pDC = pDC;
        pDC->FeWork.desc.draw.numIndices = numIndicesForDraw;
        pDC->FeWork.desc.draw.pIB = (int *)pIB;
        pDC->FeWork.desc.draw.instance = whichInstance;
        pDC->FeWork.desc.draw.type = type;

        //enqueue DC
        QueueDraw(pContext);

        pIB += maxIndicesPerDraw * indexSize;
        remainingIndices -= numIndicesForDraw;
        draw++;
    }
    RDTSC_STOP(APIDrawIndexed, numIndices, 0);
}

void SwrDrawIndexedUP(
    HANDLE hContext,
    PRIMITIVE_TOPOLOGY topology,
    SWR_TYPE type,
    UINT numIndices,
    INT *pIndices,
    VOID *pVertices,
    UINT vertexStride,
    UINT indexOffset)
{
    SWR_CONTEXT *pContext = GetContext(hContext);

// fixme
#if 0
	DRAW_CONTEXT* pDC	  = GetDrawContext(pContext);
	API_STATE	*pState	  = &pDC->state;

	Buffer verts;
	verts.pData = pVertices;
	verts.dependency = pDC->drawId;

	Buffer indices;
	indices.pData = pIndices;
	indices.dependency = pDC->drawId;

	pState->ppVertexBuffer[0] = &verts;
	pState->aVertexStride[0] = vertexStride;
	pState->pIndexBuffer = &indices;
#endif

    SwrDrawIndexed(hContext, topology, type, numIndices, indexOffset);

    if (pContext->pPrevDrawContext)
    {
        while (pContext->pPrevDrawContext->doneFE == false)
        {
            _mm_pause();
            WakeAllThreads(pContext);
        }
    }
}

#ifdef _WIN32
void CBFlip(void *pDC)
{
    SWR_CONTEXT *pContext = ((DRAW_CONTEXT *)pDC)->pContext;

    pContext->pSwapChain->ReleaseOSBackBuffer();

    if (pContext->dumpFPS)
    {
        DumpFPS(pContext, g_StartTimeStamp, __rdtsc());
    }

    // mark start of next frame
    g_StartTimeStamp = __rdtsc();

    // present the back buffer for the next frame
    pContext->pSwapChain->Present();
}

void SwrPresent(HANDLE hContext)
{
    RDTSC_START(APIPresent);

    SWR_CONTEXT *pContext = (SWR_CONTEXT *)hContext;
    DRAW_CONTEXT *pDC = GetDrawContext(pContext);
    pDC->inUse = true;

#ifdef KNOB_ENABLE_ASYNC_FLIP
    // Wait for current front buffer to finish up so we can grab the DX
    // back buffer
    RENDERTARGET *pFrontBuffer = pContext->pSwapChain->GetFrontBuffer();
    pFrontBuffer->WaitForDependencies(false);

    void *pData = NULL;
    UINT pitch;
    pContext->pSwapChain->GetOSBackBuffer(&pData, &pitch);

    // no OS back buffer means this is an offscreen surface, so nothing to present
    if (pData)
    {
        // Add dependency
        AddDependenciesNonDraw(pDC);

        pDC->FeWork.type = STORE;
        pDC->FeWork.pfnWork = ProcessPresent;
        pDC->FeWork.desc.store.pData = pData;
        pDC->FeWork.desc.store.pitch = pitch;

        // Add callback to flip
        pDC->pfnCallbackFunc = CBFlip;

        //enqueue
        QueueDraw(pContext);
    }

    // flip the backbuffer so the next frame can start
    pContext->pSwapChain->Flip();

    RDTSC_STOP(APIPresent, 0, pDC->drawId);
#else
    // Wait for backbuffer to become available
    void *pData = NULL;
    UINT pitch;
    pContext->pSwapChain->GetOSBackBuffer(&pData, &pitch);

    // no OS back buffer means this is an offscreen surface, so nothing to present
    if (pData)
    {
        AddDependenciesNonDraw(pDC);

        pDC->FeWork.type = STORE;
        pDC->FeWork.pfnWork = ProcessPresent;
        pDC->FeWork.desc.store.pData = pData;
        pDC->FeWork.desc.store.pitch = pitch;

        //enqueue
        QueueDraw(pContext);

        WaitForDependencies(pContext, pDC->drawId);
    }

    RDTSC_STOP(APIPresent, 0, pDC->drawId);
    RDTSC_START(APIFlip);

    pContext->pSwapChain->ReleaseOSBackBuffer();

    if (pContext->dumpFPS)
    {
        DumpFPS(pContext, g_StartTimeStamp, __rdtsc());
    }

    pContext->pSwapChain->Present();

    // flip the SWR back buffer for the next frame
    pContext->pSwapChain->Flip();

    // mark start of next frame
    g_StartTimeStamp = __rdtsc();

    RDTSC_STOP(APIFlip, 0, pDC->drawId);

#endif

    RDTSC_ENDFRAME();
}
#else
void SwrPresent(HANDLE hContext)
{
    fprintf(stderr, "SWR: SwrPresent() stub called\n");
    return;
}
#endif

// Special present path which deswizzles tiled render target to BGRA8_UNORM linear surface
void SwrPresent2(HANDLE hContext, void *pData, UINT pitch)
{
    RDTSC_START(APIPresent);

    SWR_CONTEXT *pContext = (SWR_CONTEXT *)hContext;
    DRAW_CONTEXT *pDC = GetDrawContext(pContext);

    RENDERTARGET *pRT = pDC->state.pRenderTargets[SWR_ATTACHMENT_COLOR0];

    pDC->inUse = true;

    // Override viewport to full extent of render target
    RASTSTATE oldState = pDC->state.rastState;
    RASTSTATE newState = oldState;
    newState.vp.x = 0.0f;
    newState.vp.y = 0.0f;
    newState.vp.width = (float)pRT->apiWidth;
    newState.vp.height = (float)pRT->apiHeight;
    newState.vp.halfWidth = pRT->apiWidth / 2.0f;
    newState.vp.halfHeight = pRT->apiHeight / 2.0f;
    newState.scissorEnable = false;

    SwrSetRastState(hContext, &newState);
    SetupMacroTileScissors(pDC);

    AddDependenciesNonDraw(pDC);

    pDC->FeWork.type = STORE;
    pDC->FeWork.pfnWork = ProcessPresent;
    pDC->FeWork.desc.store.pData = pData;
    pDC->FeWork.desc.store.pitch = pitch;

    //enqueue
    QueueDraw(pContext);

    // restore viewport
    SwrSetRastState(hContext, &oldState);

    WaitForDependencies(pContext, pDC->drawId);

    // mark start of next frame
    g_StartTimeStamp = __rdtsc();

    RDTSC_STOP(APIPresent, 0, pDC->drawId);

    RDTSC_ENDFRAME();
}

void SwrSetRenderTargets(
    HANDLE hContext,
    HANDLE hRenderTarget,
    HANDLE hDepthTarget)
{
    API_STATE *pState = GetDrawState(GetContext(hContext));

    pState->pRenderTargets[SWR_ATTACHMENT_COLOR0] = (RENDERTARGET *)hRenderTarget;
    pState->pRenderTargets[SWR_ATTACHMENT_DEPTH] = (RENDERTARGET *)hDepthTarget;
}

void SwrClearRenderTarget(
    HANDLE hContext,
    HANDLE hRenderTarget,
    UINT clearMask,
    const FLOAT clearColor[4],
    float z,
    bool useScissor)
{
    RDTSC_START(APIClearRenderTarget);

    SWR_CONTEXT *pContext = (SWR_CONTEXT *)hContext;

    BYTE fmtClearColor[4];

    // Convert from RGBA to BGRA
    fmtClearColor[0] = (BYTE)(255.0f * clearColor[2]);
    fmtClearColor[1] = (BYTE)(255.0f * clearColor[1]);
    fmtClearColor[2] = (BYTE)(255.0f * clearColor[0]);
    fmtClearColor[3] = (BYTE)(255.0f * clearColor[3]);

    DRAW_CONTEXT *pDC = GetDrawContext(pContext);
    RASTSTATE oldState;

    RENDERTARGET *pRenderTarget = pDC->state.pRenderTargets[SWR_ATTACHMENT_COLOR0];

    if (!useScissor)
    {
        // Override viewport to full extent of render target
        oldState = pDC->state.rastState;
        RASTSTATE newState = oldState;
        newState.vp.x = 0.0f;
        newState.vp.y = 0.0f;
        newState.vp.width = (float)pRenderTarget->apiWidth;
        newState.vp.height = (float)pRenderTarget->apiHeight;
        newState.vp.halfWidth = pRenderTarget->apiWidth / 2.0f;
        newState.vp.halfHeight = pRenderTarget->apiHeight / 2.0f;
        newState.scissorEnable = false;

        SwrSetRastState(hContext, &newState);
    }

    SetupMacroTileScissors(pDC);

    // Set up dependencies
    AddDependenciesNonDraw(pDC);

    pDC->inUse = true;

    CLEAR_FLAGS flags;
    flags.mask = clearMask;

    pDC->FeWork.type = CLEAR;
    pDC->FeWork.pfnWork = ProcessClear;
    pDC->FeWork.desc.clear.flags = flags;
    pDC->FeWork.desc.clear.clearDepth = z;
    pDC->FeWork.desc.clear.clearRTColor = *(UINT *)&fmtClearColor;

    // enqueue draw
    QueueDraw(pContext);

    if (!useScissor)
    {
        // restore viewport
        SwrSetRastState(hContext, &oldState);
    }

    RDTSC_STOP(APIClearRenderTarget, 0, pDC->drawId);
}

void SwrSetFsConstantBuffer(
    HANDLE hContext,
    HANDLE hConstantBuffer)
{
    API_STATE *pState = GetDrawState(GetContext(hContext));

    pState->pFSConstantBuffer = (Buffer *)hConstantBuffer;
}

void SwrSetVsConstantBuffer(
    HANDLE hContext,
    HANDLE hConstantBuffer)
{
    API_STATE *pState = GetDrawState(GetContext(hContext));

    pState->pVSConstantBuffer = (Buffer *)hConstantBuffer;
}

void SwrSetPsConstantBuffer(
    HANDLE hContext,
    HANDLE hConstantBuffer)
{
    API_STATE *pState = GetDrawState(GetContext(hContext));

    pState->pPSConstantBuffer = (Buffer *)hConstantBuffer;
}

void SwrSetTextureView(
    HANDLE hContext,
    SWR_TEXTUREVIEWINFO const &args)
{
    API_STATE *pState = GetDrawState(GetContext(hContext));
    pState->aTextureViews[args.type][args.slot] = args.textureView;
}

void SwrSetSampler(
    HANDLE hContext,
    SWR_SAMPLERINFO const &args)
{
    API_STATE *pState = GetDrawState(GetContext(hContext));
    pState->aSamplers[args.type][args.slot] = args.sampler;
}

HANDLE SwrGetTextureView(
    HANDLE hContext,
    SWR_TEXTUREVIEWINFO const &args)
{
    API_STATE *pState = GetDrawState(GetContext(hContext));
    return pState->aTextureViews[args.type][args.slot];
}

HANDLE SwrGetSampler(
    HANDLE hContext,
    SWR_SAMPLERINFO const &args)
{
    API_STATE *pState = GetDrawState(GetContext(hContext));
    return pState->aSamplers[args.type][args.slot];
}

UINT SwrGetNumNumaNodes(
    HANDLE hContext)
{
    return GetContext(hContext)->numNumaNodes;
}

UINT SwrGetNumWorkerThreads(HANDLE hContext)
{
    return GetContext(hContext)->NumWorkerThreads;
}

HANDLE SwrCreateRenderTarget(HANDLE hContext, UINT width, UINT height, SWR_FORMAT format)
{
    SWR_CONTEXT *pContext = (SWR_CONTEXT *)hContext;
    return CreateRenderTarget(pContext, width, height, format);
}

void SwrDestroyRenderTarget(HANDLE hContext, HANDLE hrt)
{
    SWR_CONTEXT *pContext = (SWR_CONTEXT *)hContext;
    DestroyRenderTarget(pContext, (RENDERTARGET *)hrt);
}

HANDLE SwrCreateBuffer(HANDLE hContext, UINT size, UINT numaNode)
{
    Buffer *buf = (Buffer *)malloc(sizeof(Buffer));
    buf->Initialize((SWR_CONTEXT *)hContext, size, NULL);
    buf->mNumaNode = numaNode;

    return (HANDLE)buf;
}

HANDLE SwrCreateBufferUP(HANDLE hContext, void *pData)
{
    Buffer *buf = (Buffer *)malloc(sizeof(Buffer));
    buf->Initialize((SWR_CONTEXT *)hContext, 0, pData);

    return (HANDLE)buf;
}

void *SwrLockResource(HANDLE hContext, HANDLE hResource, SWR_LOCK_FLAGS flags)
{
    Resource *buf = (Resource *)hResource;
    if (flags & LOCK_NONE)
    {
        buf->WaitForDependencies(false);
        Allocation *pAlloc = buf->GetCurrentAllocation();
        return pAlloc->pData;
    }

    if (flags & LOCK_NOOVERWRITE)
    {
        Allocation *pAlloc = buf->GetCurrentAllocation();
        return pAlloc->pData;
    }

    if (flags & LOCK_DISCARD)
    {
        if (buf->InUse())
        {
            Allocation *pAlloc = buf->GetNextAllocation();
            return pAlloc->pData;
        }
        else
        {
            Allocation *pAlloc = buf->GetCurrentAllocation();
            return pAlloc->pData;
        }
    }

    assert(0);
    return NULL;
}

void SwrUnlock(HANDLE hContext, HANDLE hResource)
{
}

void SwrDestroyBuffer(HANDLE hContext, HANDLE hBuffer)
{
    Buffer *buf = (Buffer *)hBuffer;

    buf->WaitForDependencies(true);
    buf->Destroy();
    free(buf);
}

void SwrDestroyBufferUP(HANDLE hContext, HANDLE hBuffer)
{
    SwrDestroyBuffer(hContext, hBuffer);
}

void SwrCopyRenderTarget(HANDLE hContext, SWR_RENDERTARGET_ATTACHMENT rt, HANDLE hTexture, void *pixels, SWR_FORMAT dstFormat, INT dstPitch,
                         INT srcX, INT srcY, INT dstX, INT dstY, UINT width, UINT height)
{
    SWR_CONTEXT *pContext = (SWR_CONTEXT *)hContext;
    Texture *pDst = (Texture *)hTexture;
    DRAW_CONTEXT *pDC = GetDrawContext(pContext);

    assert(rt < SWR_NUM_ATTACHMENTS);
    RENDERTARGET *pSrc = pDC->state.pRenderTargets[rt];

    pDC->inUse = true;

    UINT pitch;
    void *pData;
    SWR_FORMAT format;

    // either copying to a bound texture resource or to memory
    if (hTexture)
    {
        // assume mip 0?
        Resource *pDstResource = (Resource *)pDst->mhSubtextures[0];

        // Add texture write dependency
        pDstResource->AddWriteDependency(&pDC->dependency, pDC->drawId);

        pitch = pDst->mPhysicalWidth[0] * pDst->mElementSizeInBytes;
        pData = pDstResource->GetCurrentAllocation()->pData;

        // textures are always RGBA float
        format = RGBA32_FLOAT;
    }
    else
    {
        assert(pixels);
        pitch = dstPitch;
        pData = pixels;
        format = dstFormat;
    }

    // Add render target tileable read dependency
    pSrc->AddReadDependency(&pDC->dependency, pDC->drawId, true);

    pDC->FeWork.type = COPY;
    pDC->FeWork.pfnWork = ProcessCopy;
    pDC->FeWork.desc.copy.pData = pData;
    pDC->FeWork.desc.copy.pitch = pitch;
    pDC->FeWork.desc.copy.rt = rt;
    pDC->FeWork.desc.copy.srcX = srcX;
    pDC->FeWork.desc.copy.srcY = srcY;
    pDC->FeWork.desc.copy.dstX = dstX;
    pDC->FeWork.desc.copy.dstY = dstY;
    pDC->FeWork.desc.copy.width = width;
    pDC->FeWork.desc.copy.height = height;
    pDC->FeWork.desc.copy.dstFormat = format;

    //enqueue
    QueueDraw(pContext);

    // need to sync with workers if destination is a user buffer
    if (pixels)
    {
        WaitForDependencies(pContext, pDC->drawId);
    }
}
