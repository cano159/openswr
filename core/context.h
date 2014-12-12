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

#ifndef __SWR_CONTEXT_H__
#define __SWR_CONTEXT_H__

#include <condition_variable>
#include <emmintrin.h>
#include <immintrin.h>
#include <xmmintrin.h>

#include "api.h"
#include "arena.h"
#include "defs.h"
#include "fifo.hpp"
#include "knobs.h"
#include "resource.h"
#include "simdintrin.h"
#include "threads.h"

// define a small triangle as one that is <= 128 pixels in width and height.
// We can use faster 32bit fixed point math for these triangles.
#define SMALL_TRI_X_TILES (128 / KNOB_TILE_X_DIM)
#define SMALL_TRI_Y_TILES (128 / KNOB_TILE_Y_DIM)

// fixed point precision values
#define FIXED_POINT_WIDTH 8
#define FIXED_POINT_SIZE 256

typedef UINT DRAW_T;

struct SWR_CONTEXT;
struct DRAW_CONTEXT;

struct TRI_FLAGS
{
    UINT backFacing : 1;
    UINT coverageMask : (SIMD_TILE_X_DIM *SIMD_TILE_Y_DIM);
    UINT macroX : 8;
    UINT macroY : 8;
    UINT reserved : 32 - 8 - 8 - 1 - (SIMD_TILE_X_DIM * SIMD_TILE_Y_DIM);
};

struct TRIANGLE_DESC : SWR_TRIANGLE_DESC
{
    DRAW_CONTEXT *pDC;

    __m128i vStepQuad0, vStepQuad1, vStepQuad2;
    __m128i vA, vB;

    bool needScissor;

    TRI_FLAGS triFlags;
};

struct TRIANGLE_WORK_DESC
{
    float *pTriBuffer;
    float *pInterpBuffer;
    TRI_FLAGS triFlags;
};

struct VERTICAL_TRIANGLE_DESC
{
    UINT numTris;
    float *pTriBuffer;
    float *pInterpBuffer[KNOB_VS_SIMD_WIDTH];
};

struct CLEAR_FLAGS
{
    UINT mask : 2;
};

struct CLEAR_DESC
{
    CLEAR_FLAGS flags;
    UINT clearRTColor; // RGBA8
    float clearDepth;  // [0..1]
};

struct STORE_DESC
{
    void *pData;
    UINT pitch;
};

struct FLIP_DESC
{
};

struct COPY_DESC
{
    void *pData;
    SWR_RENDERTARGET_ATTACHMENT rt;
    INT pitch;
    INT srcX, srcY;
    INT dstX, dstY;
    UINT width, height;
    SWR_FORMAT dstFormat;
};

typedef void (*PFN_WORK_FUNC)(DRAW_CONTEXT *, UINT, void *);

enum WORK_TYPE
{
    DRAW,
    CLEAR,
    STORE,
    FLIP,
    COPY
};

struct BE_WORK
{
#if defined(_DEBUG)
    WORK_TYPE type;
#endif
    PFN_WORK_FUNC pfnWork;
    union
    {
        TRIANGLE_WORK_DESC tri;
        CLEAR_DESC clear;
        STORE_DESC store;
        FLIP_DESC flip;
        COPY_DESC copy;
    } desc;
};

OSALIGNLINE(struct) VERT_BE_WORK
{
    WORK_TYPE type;
    PFN_WORK_FUNC pfnWork;
    union
    {
        VERTICAL_TRIANGLE_DESC tri;
        CLEAR_DESC clear;
        STORE_DESC store;
        FLIP_DESC flip;
        COPY_DESC copy;
    } desc;
};

// IA_WORK
//		Given an index buffer the IA will return a set of indices that
//		we need to run the vertex shader on. This will be a subset of
//		the indices that the app passes in since we don't need to run
//		the vertex shader multiple times on an index.  As we run the
//		vertex shader on the VS indices the output vertices will be
//		written to a post-transform vertex buffer in order. The RS
//		indices are used to index into the post transform vertex buffer
//		by the rasterizer.
struct IA_WORK
{
    DRAW_CONTEXT *pDC;
    UINT numIndices;
    const INT *pIndices; // App supplied indices
    UINT instance;       // Which instance

    UINT numVsIndices;
    INT *pVsIndices; // [out] Indices for vertex shader

    UINT numRsIndices;
    INT *pRsIndices; // [out] Indices for rasterizer
};

struct DRAW_WORK
{
    DRAW_CONTEXT *pDC;
    union
    {
        UINT numIndices; // DrawIndexed: Number of indices for draw.
        UINT numVerts;   // Draw: Number of verts (triangles, lines, etc)
    };
    union
    {
        const INT *pIB;   // DrawIndexed: App supplied indices
        UINT startVertex; // Draw: Starting vertex in VB to render from.
    };
    UINT instance; // Which instance
    SWR_TYPE type; // index buffer type
};

typedef void (*PFN_FE_WORK_FUNC)(SWR_CONTEXT *, DRAW_CONTEXT *, void *);
struct FE_WORK
{
    WORK_TYPE type;
    PFN_FE_WORK_FUNC pfnWork;
    union
    {
        DRAW_WORK draw;
        CLEAR_DESC clear;
        STORE_DESC store;
        FLIP_DESC flip;
        COPY_DESC copy;
    } desc;
};

struct VS_WORK
{
    DRAW_CONTEXT *pDC;

    union
    {
        INT numVsIndices;
        UINT numVerts;
    };
    union
    {
        INT *pVsIndices;
        UINT startVertex;
    };
    UINT instance;

    FLOAT *pOutVertices;   // [out] out vertices
    FLOAT *pOutAttributes; // [out] out attributes
};

struct SWAP_CHAIN;
struct Buffer;
struct RENDERTARGET;

struct GUARDBAND
{
    float left, right, top, bottom;
};

OSALIGNLINE(struct) API_STATE
{
    // Attribute information.
    UINT numAttributes;

    // Vertex Buffers.
    Buffer *ppVertexBuffer[KNOB_NUM_STREAMS];
    Allocation *ppVertexBufferAlloc[KNOB_NUM_STREAMS];

    Buffer *pIndexBuffer;
    Allocation *pIndexBufferAlloc;

    // FS - Fetch Shader State
    PFN_FETCH_FUNC pfnFetchFunc;
    Buffer *pFSConstantBuffer;
    Allocation *pFSConstantBufferAlloc;

    // VS - Vertex Shader State
    Buffer *pVSConstantBuffer;
    Allocation *pVSConstantBufferAlloc;
    PFN_VERTEX_FUNC pfnVertexFunc;

    // Specifies which VS outputs are sent to PS.
    //	The front face mask is used to pick attributes for front facing triangles. Back face otherwise.
    UINT linkageMaskFrontFace;  // Mask of attributes from VS_ATTR_SLOT sent to PS.
    UINT linkageCountFrontFace; // Number of attributes sent to PS.
    UINT linkageMaskBackFace;   // Mask of attributes from VS_ATTR_SLOT sent to PS.
    UINT linkageCountBackFace;  // Number of attributes sent to PS.

    UINT linkageTotalCount; // Total number of unique attributes written by VS.

    PRIMITIVE_TOPOLOGY topology;

    // RS - Rasterizer State
    RASTSTATE rastState;
    GUARDBAND gbState;

    BBOX scissorRect;
    BBOX scissorInFixedPoint;
    BBOX scissorInTiles;
    UINT scissorMacroWidthInTiles;
    UINT scissorMacroHeightInTiles;

    OSALIGNSIMD(float)vScissorRcpDim[KNOB_VS_SIMD_WIDTH];

    // PS - Pixel shader state
    Buffer *pPSConstantBuffer;
    Allocation *pPSConstantBufferAlloc;

    PFN_PIXEL_FUNC pfnPixelFunc;

    // OM - Output Merger State
    RENDERTARGET *pRenderTargets[SWR_NUM_ATTACHMENTS];

    enum
    {
        NUM_TEXTURE_VIEWS = KNOB_NUMBER_OF_TEXTURE_VIEWS,
        NUM_SAMPLERS = KNOB_NUMBER_OF_TEXTURE_SAMPLERS
    };

    //@todo Each shader stage needs a separate copy of texture views, and samplers.
    HANDLE aTextureViews[NUM_GRAPHICS_SHADER_TYPES][NUM_TEXTURE_VIEWS]; // TXV - Texture Views
    HANDLE aSamplers[NUM_GRAPHICS_SHADER_TYPES][NUM_SAMPLERS];          // SMP - Samplers
};

typedef void (*PFN_CALLBACK_FUNC)(void *pData);
class MacroTileMgr;

// Draw Context
//	The api thread sets up a draw context that exists for the life of the draw.
//	This draw context maintains all of the state needed for the draw operation.
struct DRAW_CONTEXT
{
    SWR_CONTEXT *pContext;
    DRAW_T drawId;

    API_STATE state;

    FE_WORK FeWork;
    volatile OSALIGNLINE(UINT) FeLock;
    volatile OSALIGNLINE(bool) inUse;
    volatile OSALIGNLINE(bool) doneFE; // Is FE work done for this draw?

    DRAW_T dependency;
    bool depCompleteDraw; // This draw depends on the dependent draw to be fully complete

    MacroTileMgr *pTileMgr;

    PFN_CALLBACK_FUNC pfnCallbackFunc;

    Arena arena;
};

struct SWR_CONTEXT
{
    // Draw Context Ring
    //  In order to support threading we need to support multiple draws in flight. Each
    //  draw needs its own state since state can vary from draw to draw. In order to
    //  keep memory usage low we'll maintain N draw contexts.
    //
    //  1. State - When an application first sets state we'll request a new draw context to use.
    //     a. If there are no available draw contexts then we'll have to wait until one becomes free.
    //     b. If one is available then set pCurDrawContext to point to it and mark it in use.
    //     c. All state calls set state on pCurDrawContext.
    //  2. Draw - Creates submits a work item that is associated with current draw context.
    //     a. Set pPrevDrawContext = pCurDrawContext
    //     b. Set pCurDrawContext to NULL.
    //  3. State - When an applications sets state after draw
    //     a. Same as step 1.
    //     b. Didn't mention in step 1 that state is copied from prev draw context to current.
    DRAW_CONTEXT *dcRing;

    UINT dcIndex; // Index current draw context

    UINT NumWorkerThreads;

    DRAW_CONTEXT *pCurDrawContext;  // This is the context for an unsubmitted draw.
    DRAW_CONTEXT *pPrevDrawContext; // This is the previous context submitted that we can copy state from.

    THREAD_POOL threadPool; // Thread pool associated with this context

    SWAP_CHAIN *pSwapChain;

    std::condition_variable FifosNotEmpty;
    std::mutex WaitLock;

    // Draw Contexts will get a unique drawId generated from this
    DRAW_T nextDrawId;

    // Last retired drawId. Read/written only be API thread
    DRAW_T LastRetiredId;

    // most recent draw id enqueued by the API thread
    // written by api thread, read by multiple workers
    OSALIGNLINE(volatile DRAW_T) DrawEnqueued;

    // Current FE status of each worker.
    OSALIGNLINE(volatile DRAW_T) WorkerFE[KNOB_MAX_NUM_THREADS];
    OSALIGNLINE(volatile DRAW_T) WorkerBE[KNOB_MAX_NUM_THREADS];

    DRIVER_TYPE driverType;

    UINT numNumaNodes;

    BOOL dumpFPS;
    BOOL dumpPoolInfo;
};

void WaitForDependencies(SWR_CONTEXT *pContext, DRAW_T drawId);
void WakeAllThreads(SWR_CONTEXT *pContext);

#endif //__SWR_CONTEXT_H__
