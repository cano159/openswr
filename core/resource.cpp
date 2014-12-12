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

#if defined(__gnu_linux__) || defined(__linux__)
#include <numa.h>
#endif

#include <cmath>

#include "resource.h"
#include "context.h"

void WakeAllThreads(SWR_CONTEXT *pContext)
{
    std::unique_lock<std::mutex> lock(pContext->WaitLock);
    pContext->FifosNotEmpty.notify_all();
    lock.unlock();
}

void *Allocate(SWR_CONTEXT *pContext, UINT size, UINT align, UINT numaNode)
{
    if (pContext->numNumaNodes == 1)
    {
        return _aligned_malloc(size, align);
    }
    else
    {
#if defined(_WIN32)
        return VirtualAllocExNuma(
            GetCurrentProcess(),
            NULL,
            size,
            MEM_RESERVE | MEM_COMMIT,
            PAGE_READWRITE,
            numaNode);
#else
        static struct bitmask *nodemask = NULL;

        if (nodemask == NULL)
            nodemask = numa_bitmask_alloc(sizeof(unsigned long) * 8);

        numa_bitmask_clearall(nodemask);
        numa_bitmask_setbit(nodemask, numaNode);
        numa_set_membind(nodemask);

        void *result = numa_alloc_onnode(size, numaNode);
        assert(result);

        return result;
#endif
    }
}

void DeAllocate(SWR_CONTEXT *pContext, void *pBuffer, UINT size)
{
    if (pContext->numNumaNodes == 1)
    {
        _aligned_free(pBuffer);
    }
    else
    {
#if defined(_WIN32)
        VirtualFree(pBuffer, 0, MEM_RELEASE);
#else
        numa_free(pBuffer, size);
#endif
    }
}

void UpdateLastRetiredId(SWR_CONTEXT *pContext)
{
#if KNOB_SINGLE_THREADED
    pContext->LastRetiredId = pContext->nextDrawId - 1;
#else
    if (pContext->LastRetiredId == pContext->nextDrawId)
        return;

    pContext->LastRetiredId = std::max((INT)pContext->LastRetiredId, (INT)pContext->nextDrawId - KNOB_MAX_DRAWS_IN_FLIGHT);
    UINT head = (pContext->LastRetiredId + 1) % KNOB_MAX_DRAWS_IN_FLIGHT;
    UINT tail = pContext->nextDrawId % KNOB_MAX_DRAWS_IN_FLIGHT;

    DRAW_CONTEXT *pDC = &pContext->dcRing[head];
    while (head != tail && !StillDrawing(pContext, pDC))
    {
        pContext->LastRetiredId++;
        head = (head + 1) % KNOB_MAX_DRAWS_IN_FLIGHT;
        pDC = &pContext->dcRing[head];
    }
#endif
}

bool StillDrawing(SWR_CONTEXT *pContext, DRAW_CONTEXT *pDC)
{
#if KNOB_SINGLE_THREADED
    pDC->inUse = false;
    return false;
#else
    // Check if backend work is done. First make sure all triangles have been binned.
    if (pDC->doneFE == true)
    {
        // ensure workers have all moved passed this draw
        for (UINT i = 0; i < pContext->NumWorkerThreads; ++i)
        {
            if (pContext->WorkerFE[i] <= pDC->drawId)
            {
                return true;
            }

            if (pContext->WorkerBE[i] <= pDC->drawId)
            {
                return true;
            }
        }

        pDC->inUse = false; // all work is done.

        return false;
    }
    return (pDC->inUse) ? true : false;
#endif
}

void WaitForDependencies(SWR_CONTEXT *pContext, DRAW_T drawId)
{
#if !KNOB_SINGLE_THREADED
    while (drawId > pContext->LastRetiredId)
    {
        WakeAllThreads(pContext);
        UpdateLastRetiredId(pContext);
    }
#endif
}

bool Resource::InUse()
{
    Allocation *pAlloc = GetCurrentAllocation();
    UINT dep = std::max<UINT>(pAlloc->readDep, pAlloc->writeDep);

    return (dep > mpContext->LastRetiredId);
}

RENDERTARGET *CreateRenderTarget(SWR_CONTEXT *pContext, UINT width, UINT height, SWR_FORMAT format)
{
    RENDERTARGET *pRT = (RENDERTARGET *)_aligned_malloc(sizeof(RENDERTARGET), KNOB_VS_SIMD_WIDTH * 4);

    // Align dimensions to support macrotile alignment requirements
    UINT macroWidth = KNOB_MACROTILE_X_DIM;
    UINT macroHeight = KNOB_MACROTILE_Y_DIM;
    UINT alignedWidth = (width + (KNOB_MACROTILE_X_DIM - 1)) & ~(KNOB_MACROTILE_X_DIM - 1);
    UINT alignedHeight = (height + (KNOB_MACROTILE_Y_DIM - 1)) & ~(KNOB_MACROTILE_Y_DIM - 1);

    const SWR_FORMAT_INFO &info = GetFormatInfo(format);
    UINT Bpp = info.Bpp;

    pRT->format = format;
    pRT->apiWidth = width;
    pRT->apiHeight = height;
    pRT->width = alignedWidth;
    pRT->height = alignedHeight;
    pRT->widthInBytes = alignedWidth * Bpp;
    pRT->widthInTiles = alignedWidth >> KNOB_TILE_X_DIM_SHIFT;
    pRT->pTileData = (BYTE *)_aligned_malloc(alignedWidth * alignedHeight * Bpp, KNOB_VS_SIMD_WIDTH * 4);
    pRT->macroWidth = macroWidth << FIXED_POINT_WIDTH;
    pRT->macroHeight = macroHeight << FIXED_POINT_WIDTH;

    pRT->Initialize(pContext, 0, pRT->pTileData);
    return pRT;
}

// assumes dependencies have already been met
void DestroyRenderTarget(SWR_CONTEXT *pContext, RENDERTARGET *pRT)
{
    pRT->Destroy();

    _aligned_free(pRT->pTileData);
    _aligned_free(pRT);
}

Texture::Texture()
{
    mhContext = 0;
    mNumPlanes = 0;
    mNumMipLevels = 0;
    mElementSizeInBytes = 0;
}

Texture::Texture(HANDLE hContext, SWR_CREATETEXTURE const &args)
{
    Create(hContext, args);
}

Texture::~Texture()
{
    for (std::size_t i = 0, N = mhSubtextures.size(); i != N; ++i)
    {
        SwrDestroyBuffer(mhContext, mhSubtextures[i]);
        _aligned_free(mSubtextures[i]);
    }
}

void Texture::Create(HANDLE hContext, SWR_CREATETEXTURE const &args)
{
    mhContext = hContext;
    mNumPlanes = std::max(1U, args.planes);
    mNumMipLevels = std::max(1U, args.mipLevels);
    mElementSizeInBytes = args.eltSizeInBytes;

    mTexelHeight.push_back(args.height);
    mTexelWidth.push_back(args.width);
    mPhysicalHeight.push_back(ALIGN_UP(mTexelHeight[0] + 1, 8));
    mPhysicalWidth.push_back(ALIGN_UP(mTexelWidth[0] + 1, 8));

    UINT actualMipLevels = 1;
    for (UINT i = 1; i < mNumMipLevels; ++i, ++actualMipLevels)
    {
        // iterate until we reach 1x1
        if ((mTexelHeight[i - 1] >> 1) == 0 && (mTexelWidth[i - 1] >> 1) == 0)
        {
            break;
        }

        UINT texWidth = mTexelWidth[i - 1] >> 1;
        UINT texHeight = mTexelHeight[i - 1] >> 1;
        if (!texWidth)
        {
            texWidth = 1;
        }
        if (!texHeight)
        {
            texHeight = 1;
        }

        mTexelHeight.push_back(texHeight);
        mTexelWidth.push_back(texWidth);
        mPhysicalHeight.push_back(ALIGN_UP(mTexelHeight[i] + 1, TILE_ALIGN));
        mPhysicalWidth.push_back(ALIGN_UP(mTexelWidth[i] + 1, TILE_ALIGN));
    }

    mNumMipLevels = actualMipLevels;

    // There are mNumMipLevels * mNumPlanes Subtextures.
    for (UINT p = 0; p < mNumPlanes; ++p)
    {
        for (UINT m = 0; m < mNumMipLevels; ++m)
        {
            UINT size = mPhysicalWidth[m] * mPhysicalHeight[m] * mElementSizeInBytes;
            mSubtextures.push_back((data_t *)_aligned_malloc(size, DATA_ALIGN));
            mhSubtextures.push_back(SwrCreateBufferUP(mhContext, mSubtextures.back()));
        }
    }
}

void Texture::Lock(SWR_LOCK_FLAGS lockFlags)
{
    if (lockFlags == LOCK_DONTLOCK)
        return;
    for (std::size_t i = 0, N = mhSubtextures.size(); i != N; ++i)
    {
        SwrLockResource(mhContext, mhSubtextures[i], lockFlags);
    }
}

void Texture::Unlock()
{
    for (std::size_t i = 0, N = mhSubtextures.size(); i != N; ++i)
    {
        ::SwrUnlock(mhContext, mhSubtextures[i]);
    }
}

void Texture::SubtextureInfo(SWR_GETSUBTEXTUREINFO &info)
{
    if ((info.mipLevel == 0) && (info.plane == 0))
    {
        info.mipLevel = info.subtextureIndex % mNumMipLevels;
        info.plane = info.subtextureIndex / mNumMipLevels;
    }
    else
    {
        info.subtextureIndex = info.mipLevel + info.plane * mNumMipLevels;
    }

    info.physPlanes = mNumPlanes;
    info.physHeight = mPhysicalHeight[info.mipLevel];
    info.physWidth = mPhysicalWidth[info.mipLevel];
    info.texelPlanes = mNumPlanes;
    info.texelHeight = mTexelHeight[info.mipLevel];
    info.texelWidth = mTexelWidth[info.mipLevel];
    info.eltSizeInBytes = mElementSizeInBytes;

    Lock(info.lockFlags);
    if (info.lockFlags != LOCK_DONTLOCK)
    {
        info.pData = mSubtextures[info.subtextureIndex];
    }
}

HANDLE SwrCreateTexture(
    HANDLE hContext,
    SWR_CREATETEXTURE const &args)
{
    return new Texture(hContext, args);
}

void SwrDestroyTexture(
    HANDLE hContext,
    HANDLE hTexture)
{
    delete reinterpret_cast<Texture *>(hTexture);
}

void SwrLockTexture(
    HANDLE hContext,
    HANDLE hTexture,
    SWR_LOCK_FLAGS flags)
{
    reinterpret_cast<Texture *>(hTexture)->Lock(flags);
}

void SwrUnlockTexture(
    HANDLE hContext,
    HANDLE hTexture)
{
    reinterpret_cast<Texture *>(hTexture)->Unlock();
}

void SwrGetSubtextureInfo(
    HANDLE hTexture,
    SWR_GETSUBTEXTUREINFO &getSTI)
{
    reinterpret_cast<Texture *>(hTexture)->SubtextureInfo(getSTI);
}
