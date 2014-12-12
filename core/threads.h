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

#include <set>
#include <thread>
typedef std::thread *THREAD_PTR;

struct SWR_CONTEXT;

struct THREAD_DATA
{
    UINT threadId;
    UINT workerId;
    SWR_CONTEXT *pContext;
};

struct THREAD_POOL
{
    THREAD_PTR threads[KNOB_MAX_NUM_THREADS];
    UINT numThreads;
    volatile bool inThreadShutdown;
    THREAD_DATA *pThreadData;
};

void createThreadPool(SWR_CONTEXT *pContext, THREAD_POOL *pPool);
void destroyThreadPool(SWR_CONTEXT *pContext, THREAD_POOL *pPool);

// Expose FE and BE worker functions to the API thread if single threaded
#if KNOB_SINGLE_THREADED
void WorkOnFifoFE(SWR_CONTEXT *pContext, UINT workerId, volatile DRAW_T &curDrawFE, UCHAR numaNode);
void WorkOnFifoBE(SWR_CONTEXT *pContext, UINT workerId, DRAW_T curDrawFE, volatile DRAW_T &curDrawBE, std::set<UINT> &usedTiles);
#endif
