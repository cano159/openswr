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

#ifndef __SWR_RESOURCE_H__
#define __SWR_RESOURCE_H__

#include <algorithm>
#include <assert.h>
#include <string.h>

#include "os.h"
#include "utils.h"
#include "formats.h"

#define RESOURCE_STATS 0
#if RESOURCE_STATS
#include <stdio.h>
#endif

#define MAX_ALIASES KNOB_MAX_DRAWS_IN_FLIGHT

struct SWR_CONTEXT;
struct DRAW_CONTEXT;

void WaitForDependencies(SWR_CONTEXT *pContext, DRAW_T drawId);
bool StillDrawing(SWR_CONTEXT *pContext, DRAW_CONTEXT *pDC);

void *Allocate(SWR_CONTEXT *pContext, UINT size, UINT align, UINT numaNode);
void DeAllocate(SWR_CONTEXT *pContext, void *pBuffer, UINT size);

void UpdateLastRetiredId(SWR_CONTEXT *pContext);

void WakeAllThreads(SWR_CONTEXT *pContext);

// Contains actual dependency tracking and memory backing for a Resource
struct Allocation
{
    DRAW_T readDep;
    DRAW_T writeDep;

    void *pData;
};

#if RESOURCE_STATS
static int numResources = 0;
static int numAllocations = 0;
static long long int totalAllocSize = 0;
#endif

// base class for all resources in SWR, such as constant buffers, textures, rendertargets
// manages dependency tracking, memory allocation and aliases
struct Resource
{
    SWR_CONTEXT *mpContext;
    Allocation mAllocRingBuffer[MAX_ALIASES];
    UINT mCurAlloc;
    UINT mSize;
    bool mIsUP;
    UINT mNumaNode;

    void Initialize(SWR_CONTEXT *pContext, UINT size, void *pUP)
    {
        mpContext = pContext;
        mSize = size;
        memset(mAllocRingBuffer, 0, sizeof(mAllocRingBuffer));
        mCurAlloc = 0;
        mIsUP = false;

        if (pUP)
        {
            mIsUP = true;
            Allocation *pAlloc = &mAllocRingBuffer[0];
            pAlloc->pData = pUP;
            pAlloc->readDep = 0;
            pAlloc->writeDep = 0;
        }
    }

    Allocation *GetCurrentAllocation()
    {
        Allocation *pAlloc = &mAllocRingBuffer[mCurAlloc];
        if (pAlloc->pData)
        {
            return pAlloc;
        }

#if RESOURCE_STATS
        numAllocations++;
        totalAllocSize += mSize;
        printf("%d allocations, total size %lld bytes\n", numAllocations, totalAllocSize);
#endif
        pAlloc->pData = Allocate(mpContext, mSize, 64, mNumaNode);
        pAlloc->readDep = 0;
        pAlloc->writeDep = 0;

        assert(pAlloc);
        return pAlloc;
    }

    Allocation *GetNextAllocation()
    {
        // invalid to ask for an alias with UP buffers
        assert(!mIsUP);
        if (mIsUP)
        {
            return NULL;
        }

        // find first available empty alias
        for (UINT i = 0; i < MAX_ALIASES; ++i)
        {
            mCurAlloc = i;
            Allocation *pCurAlloc = GetCurrentAllocation();
            if (!InUse())
            {
                return pCurAlloc;
            }
        }

        // all aliases in use, start over from 0 and wait
        mCurAlloc = 0;
        Allocation *pCurAlloc = GetCurrentAllocation();
        WaitForDependencies(pCurAlloc);

        return GetCurrentAllocation();
    }

    // @todo keep track of last retired draw id
    bool InUse();

    // Marks the current reader for this resource
    // Updates the callers dep with the last writer for this resource, unless tileable is set
    void AddReadDependency(DRAW_T *pUpdatedDep, DRAW_T dep, bool tileable = false)
    {
        Allocation *pAlloc = GetCurrentAllocation();
        pAlloc->readDep = dep;
        if (!tileable)
        {
            *pUpdatedDep = std::max<DRAW_T>(*pUpdatedDep, pAlloc->writeDep);
        }
    }

    // Marks the current writer for this resource
    // Updates the callers dep with the last reader
    void AddWriteDependency(DRAW_T *pUpdatedDep, DRAW_T dep)
    {
        Allocation *pAlloc = GetCurrentAllocation();
        *pUpdatedDep = std::max<DRAW_T>(*pUpdatedDep, pAlloc->readDep);
        pAlloc->writeDep = dep;
    }

    bool IsDependency(Allocation *pAlloc, DRAW_T drawId)
    {
        assert(pAlloc);
        UINT dep = std::max<UINT>(pAlloc->readDep, pAlloc->writeDep);
        return dep >= drawId;
    }

    void WaitForDependencies(Allocation *pAlloc)
    {
        if (pAlloc->pData)
        {
            UINT dep = std::max<UINT>(pAlloc->readDep, pAlloc->writeDep);
            ::WaitForDependencies(mpContext, dep);
        }
    }

    // Wait for either the current allocation or all allocations in use to complete
    void WaitForDependencies(bool allAllocs)
    {
        if (allAllocs)
        {
            for (UINT a = 0; a < MAX_ALIASES; ++a)
            {
                Allocation *pAlloc = &mAllocRingBuffer[a];
                WaitForDependencies(pAlloc);
            }
        }
        else
        {
            WaitForDependencies(GetCurrentAllocation());
        }
    }

    void Destroy()
    {
        WaitForDependencies(mIsUP == false);
        if (mIsUP)
        {
            return;
        }

        for (UINT a = 0; a < MAX_ALIASES; ++a)
        {
            Allocation *pAlloc = &mAllocRingBuffer[a];
            if (pAlloc->pData)
            {
                DeAllocate(mpContext, pAlloc->pData, mSize);
                pAlloc->pData = NULL;
#if RESOURCE_STATS
                numAllocations--;
                totalAllocSize -= mSize;
                printf("%d allocations, total size %lld bytes\n", numAllocations, totalAllocSize);
#endif
            }
        }
    }
};

struct Buffer : Resource
{
};

struct RENDERTARGET : Resource
{
    SWR_FORMAT format;
    UINT apiWidth;
    UINT apiHeight;
    UINT width;
    UINT height;
    UINT widthInBytes;
    UINT widthInTiles;
    BYTE *pTileData;
    UINT macroWidth;
    UINT macroHeight;
};

// @todo support resources other than render targets
RENDERTARGET *CreateRenderTarget(SWR_CONTEXT *pContext, UINT width, UINT height, SWR_FORMAT format);
void DestroyRenderTarget(SWR_CONTEXT *pContext, RENDERTARGET *pRenderTarget);

#endif //__SWR_RESOURCE_H__
