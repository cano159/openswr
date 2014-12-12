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

#include <stdio.h>
#include <assert.h>
#include <bitset>
#include <thread>

#if defined(__linux__) || defined(__gnu_linux__)
#include <numa.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#endif

#include "context.h"
#include "frontend.h"
#include "backend.h"
#include "rasterizer.h"
#include "rdtsc.h"
#include "tilemgr.h"

void bindThread(UINT threadId)
{
#if defined(_WIN32)
    DWORD_PTR mask = 1 << threadId;
    DWORD_PTR result = SetThreadAffinityMask(GetCurrentThread(), mask);
#else
    cpu_set_t cpuset;
    pthread_t thread = pthread_self();
    CPU_ZERO(&cpuset);
    CPU_SET(threadId, &cpuset);

    pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
#endif
}

INLINE
DRAW_T GetEnqueuedDraw(SWR_CONTEXT *pContext)
{
    //UINT64 result = _InterlockedCompareExchange64((volatile __int64*)&pContext->DrawEnqueued, 0, 0);
    //return result;
    return pContext->DrawEnqueued;
}

INLINE
DRAW_CONTEXT *GetDC(SWR_CONTEXT *pContext, DRAW_T drawId)
{
    return &pContext->dcRing[(drawId - 1) % KNOB_MAX_DRAWS_IN_FLIGHT];
}

// returns true if dependency not met
INLINE
bool CheckDependency(SWR_CONTEXT *pContext, DRAW_CONTEXT *pDC, DRAW_T lastRetiredDraw)
{
#if KNOB_SINGLE_THREADED
    return false;
#else
    if (pDC->drawId == 0 || pDC->dependency == 0)
    {
        return false;
    }
    else
    {
        return (pDC->dependency > lastRetiredDraw);
    }
#endif
}

void WorkOnFifoBE(SWR_CONTEXT *pContext, UINT workerId, DRAW_T curDrawFE, volatile DRAW_T &curDrawBE, std::set<UINT> &usedTiles)
{
    // increment our current draw id to the first incomplete draw
    DRAW_T drawEnqueued = GetEnqueuedDraw(pContext);
    while (curDrawBE < drawEnqueued)
    {
        DRAW_CONTEXT *pDC = &pContext->dcRing[curDrawBE % KNOB_MAX_DRAWS_IN_FLIGHT];

        if (!pDC->doneFE)
            break;

        if (pDC->pTileMgr->isWorkComplete())
        {
            curDrawBE++;
        }
        else
        {
            break;
        }
    }

    if (curDrawBE >= drawEnqueued)
        return;

    DRAW_T lastRetiredDraw = pContext->dcRing[curDrawBE % KNOB_MAX_DRAWS_IN_FLIGHT].drawId - 1;

    // try to work on each draw in order of the available draws in flight
    // rules:
    // 1. if we're on curDrawBE, we can work on any macro tile that is available
    // 2. if we're trying to work on draws after curDrawBE, we are restricted to
    //    working on those macro tiles that are known to be complete in the prior draw to
    //    maintain order
    BBOX prevScissorInTiles(0, 0, 0, 0);
    usedTiles.clear();
    for (DRAW_T i = curDrawBE; i < GetEnqueuedDraw(pContext); ++i)
    {
        DRAW_CONTEXT *pDC = &pContext->dcRing[i % KNOB_MAX_DRAWS_IN_FLIGHT];
        if (!pDC->doneFE)
            break;

        // check dependencies
        if (CheckDependency(pContext, pDC, lastRetiredDraw))
        {
            return;
        }

        // can't move on to this draw if scissor/viewport rectangle has changed
        if (prevScissorInTiles != BBOX(0, 0, 0, 0) && prevScissorInTiles != pDC->state.scissorInTiles)
        {
            return;
        }

        // loop across all used macro tiles
        std::vector<UINT> &macroTiles = pDC->pTileMgr->getUsedTiles();

        for (UINT idx = 0; idx < macroTiles.size(); ++idx)
        {
            UINT tileID = macroTiles[idx];
            MacroTile &tile = pDC->pTileMgr->getMacroTile(tileID);

            // is macro tile complete?
            if (tile.m_WorkItemsBE == tile.m_WorkItemsFE)
            {
                usedTiles.insert(tileID);
                continue;
            }

            // has this thread completed this tile in previous draws?
            if ((i != curDrawBE) && (usedTiles.find(tileID) == usedTiles.end()))
            {
                continue;
            }

            if (tile.m_Fifo.getNumQueued() && tile.m_Fifo.tryLock())
            {

#if KNOB_VERTICALIZED_BINNER
                VERT_BE_WORK *pWork;
#else
                BE_WORK *pWork;
#endif

                usedTiles.insert(tileID);

                // this solves a race condition where a worker thread 'clears' a macrotile
                // which resets the lock, and another thread now sees a cleared lock and
                // is able to lock it again.  Once locked, check if there is any actual
                // work and if not, free the lock and move on.
                if (tile.m_Fifo.getNumQueued() == 0)
                {
                    tile.m_Fifo.mLock = 0;
                    continue;
                }

                RDTSC_START(WorkerFoundWork);

                UINT numWorkItems = tile.m_Fifo.getNumQueued();
                while ((pWork = tile.m_Fifo.peek()) != NULL)
                {
                    pWork->pfnWork(pDC, tileID, &pWork->desc);
                    tile.m_Fifo.dequeue_noinc();
                }
                RDTSC_STOP(WorkerFoundWork, numWorkItems, pDC->drawId);

                _ReadWriteBarrier();

                // is the draw complete?
                if (pDC->pTileMgr->markTileComplete(tileID))
                {
                    // we completed the draw, call end of draw callback
                    if (pDC->pfnCallbackFunc)
                    {
                        pDC->pfnCallbackFunc(pDC);
                    }

                    _ReadWriteBarrier();

                    // increment current BE if we're the oldest draw
                    // we can also safely move on to the next draw
                    if (curDrawBE == i)
                    {
                        curDrawBE++;
                        lastRetiredDraw++;

                        usedTiles.clear();
                        break;
                    }
                }
            }
            else
            {
                // tried to lock a tile we previously worked on, but it's already taken.
                // remove from our used tiles set
                usedTiles.erase(tileID);
            }
        }
        prevScissorInTiles = pDC->state.scissorInTiles;
    }
}

#if 0
void WorkOnFifoBETest(SWR_CONTEXT *pContext, UINT threadId, DRAW_T curDrawFE, volatile DRAW_T &curDrawBE)
{
	// pick a fifo to work on
	UINT64 attemptedTilesBitSet = 0;
	assert(NUM_MACRO_TILES <= 64);

	// increment our current draw id to the first incomplete draw
	DRAW_T drawEnqueued = GetEnqueuedDraw(pContext);
	while (curDrawBE < drawEnqueued)
	{
		DRAW_CONTEXT *pDC = &pContext->dcRing[curDrawBE % KNOB_MAX_DRAWS_IN_FLIGHT];

		if (!pDC->doneFE) break;

		UINT numWorkBE = 0, numWorkFE = 0;
		for (UINT i = 0; i < NUM_MACRO_TILES; ++i)
		{
			numWorkFE += pDC->totalWorkItemsFE[i];
			numWorkBE += pDC->totalWorkItemsBE[i];
		}
		if (numWorkFE == numWorkBE)
		{
			curDrawBE ++;
		}
		else
		{
			break;
		}
	}

	if (curDrawBE >= drawEnqueued) return;

	// try to work on each draw in order of the available draws in flight
	// rules:
	// 1. if we're on curDrawBE, we can work on any macro tile that is available
	// 2. if we're trying to work on draws after curDrawBE, we are restricted to 
	//    working on those macro tiles that are known to be complete in the prior draw to
	//    maintain order

	// find a macro tile to work on

	for (DRAW_T i = curDrawBE; i < drawEnqueued; ++i)
	{
		DRAW_CONTEXT *pDC = &pContext->dcRing[i % KNOB_MAX_DRAWS_IN_FLIGHT];
		if (!pDC->doneFE) break;

		for (UINT t = 0; t < NUM_MACRO_TILES; ++t)
		{
			// is macro tile complete?
			if ((pDC->totalWorkItemsFE[t] == pDC->totalWorkItemsBE[t]) ||
				(pDC->totalWorkItemsFE[t] == 0))
			{
				continue;
			}

			DRAW_T j = i;
			while (j < GetEnqueuedDraw(pContext))
			{
				DRAW_CONTEXT *pNewDC = &pContext->dcRing[j % KNOB_MAX_DRAWS_IN_FLIGHT];
				if (!pNewDC->doneFE) return;

				if (pNewDC->drawFifo[t].getNumQueued() && pNewDC->drawFifo[t].tryLock())
				{
					RDTSC_START(WorkOnFifoBE);
					BE_WORK *pWork;
					UINT numTiles = 0;
					while ((pWork = pNewDC->drawFifo[t].peek()) != NULL)
					{
						pWork->pfnWork(t, &pWork->desc);
						pNewDC->drawFifo[t].dequeue_noinc();
						numTiles ++;
					}
					RDTSC_STOP(WorkOnFifoBE, numTiles, pNewDC->drawId);

					// is the draw complete?
					UINT numWorkBE = 0, numWorkFE = 0;
					for (UINT i = 0; i < NUM_MACRO_TILES; ++i)
					{
						numWorkFE += pNewDC->totalWorkItemsFE[i];
						numWorkBE += pNewDC->totalWorkItemsBE[i];
					}
					if (numWorkFE == numWorkBE)
					{
						curDrawBE ++;
					}

					pNewDC->drawFifo[t].unlock();
				}
				else
				{
					return;
				}
				++j;
			}
			return;
		}
	}
}
#endif

THREAD UINT tls_FeWorkBackoffCounter = 0;

UINT GetPreferredNumaNode(DRAW_CONTEXT *pDC, UINT numaNode)
{
    if (pDC->FeWork.type == DRAW)
    {
        if (tls_FeWorkBackoffCounter <= KNOB_FE_BACKOFF_COUNT)
        {
            return (pDC->state.ppVertexBuffer[0]->mNumaNode);
        }
    }
    return numaNode;
}

void WorkOnFifoFE(SWR_CONTEXT *pContext, UINT workerId, volatile DRAW_T &curDrawFE, UCHAR numaNode)
{
    // Try to grab the next DC from the ring
    DRAW_T drawEnqueued = GetEnqueuedDraw(pContext);
    while (curDrawFE < drawEnqueued)
    {
        UINT dcSlot = curDrawFE % KNOB_MAX_DRAWS_IN_FLIGHT;
        DRAW_CONTEXT *pDC = &pContext->dcRing[dcSlot];
        if (pDC->doneFE || pDC->FeLock)
        {
            curDrawFE++;
        }
        else
        {
            break;
        }
    }

    tls_FeWorkBackoffCounter++;

    DRAW_T curDraw = curDrawFE;
    while (curDraw < drawEnqueued)
    {
        UINT dcSlot = curDraw % KNOB_MAX_DRAWS_IN_FLIGHT;
        DRAW_CONTEXT *pDC = &pContext->dcRing[dcSlot];

        if (!pDC->FeLock)
        {
            // Prefer working on draws tied to this thread's numa node
            if (pContext->numNumaNodes == 1 || GetPreferredNumaNode(pDC, numaNode) == numaNode)
            {
                UINT initial = InterlockedCompareExchange((volatile UINT *)&pDC->FeLock, 1, 0);
                if (initial == 0)
                {
                    // successfully grabbed the DC, now run the FE
                    pDC->FeWork.pfnWork(pContext, pDC, &pDC->FeWork.desc);
                    tls_FeWorkBackoffCounter = 0;
                }
            }
        }
        curDraw++;
    }

// single threaded requires the FE to complete the current draw and update it's WorkerFE status
#if KNOB_SINGLE_THREADED
    curDrawFE = curDraw;
#endif
}

#if defined(__linux__) || defined(__gnu_linux__)
void GetNumaProcessorNode(int threadId, UCHAR *numaNode)
{
    *numaNode = numa_node_of_cpu(threadId);
}
#endif

DWORD workerThread(LPVOID pData)
{
    THREAD_DATA *pThreadData = (THREAD_DATA *)pData;
    SWR_CONTEXT *pContext = pThreadData->pContext;
    UINT threadId = pThreadData->threadId;
    UINT workerId = pThreadData->workerId;

    bindThread(threadId);

    RDTSC_INIT(threadId);

#if KNOB_ENABLE_NUMA && (defined(__linux__) || defined(__gnu_linux__))
    int numaNode = threadId % pContext->numNumaNodes;
    numa_run_on_node(numaNode);
#else
    // query NUMA node for this thread
    UCHAR numaNode;
    GetNumaProcessorNode(threadId, &numaNode);
#endif

    std::set<UINT> usedTiles;

    // each worker has the ability to work on any of the queued draws as long as certain
    // conditions are met. the data associated
    // with a draw is guaranteed to be active as long as a worker hasn't signaled that he
    // has moved on to the next draw when he determines there is no more work to do. The api
    // thread will not increment the head of the dc ring until all workers have moved past the
    // current head.
    // the logic to determine what to work on is:
    // 1- try to work on the FE any draw that is queued. For now there are no dependencies
    //    on the FE work, so any worker can grab any FE and process in parallel.  Eventually
    //    we'll need dependency tracking to force serialization on FEs.  The worker will try
    //    to pick an FE by atomically incrementing a counter in the swr context.  he'll keep
    //    trying until he reaches the tail.
    // 2- BE work must be done in strict order. we accomplish this today by pulling work off
    //    the oldest draw (ie the head) of the dcRing. the worker can determine if there is
    //    any work left by comparing the total # of binned work items and the total # of completed
    //    work items. If they are equal, then there is no more work to do for this draw, and
    //    the worker can safely increment its oldestDraw counter and move on to the next draw.
    std::unique_lock<std::mutex> lock(pContext->WaitLock);
    lock.unlock();
    while (pContext->threadPool.inThreadShutdown == false)
    {
        int loop = 0;
        while (loop++ < KNOB_WORKER_SPIN_LOOP_COUNT && pContext->WorkerBE[workerId] == pContext->DrawEnqueued)
        {
            _mm_pause();
        }

        if (pContext->WorkerBE[workerId] == pContext->DrawEnqueued)
        {
            RDTSC_START(WorkerWaitForThreadEvent);
            lock.lock();

            if (pContext->threadPool.inThreadShutdown)
            {
                lock.unlock();
                break;
            }

            pContext->FifosNotEmpty.wait(lock);
            lock.unlock();

            RDTSC_STOP(WorkerWaitForThreadEvent, 0, 0);

            if (pContext->threadPool.inThreadShutdown)
            {
                break;
            }
        }

        RDTSC_START(WorkerWorkOnFifoBE);
        WorkOnFifoBE(pContext, workerId, pContext->WorkerFE[workerId], pContext->WorkerBE[workerId], usedTiles);
        RDTSC_STOP(WorkerWorkOnFifoBE, 0, 0);

        WorkOnFifoFE(pContext, workerId, pContext->WorkerFE[workerId], numaNode);
    }
    return 0;
}

void createThreadPool(SWR_CONTEXT *pContext, THREAD_POOL *pPool)
{
    // main api thread affinitized to core 0 always
    bindThread(0);

    // This returns zero on Centos 6.5
    UINT numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0)
    {
#ifdef _WIN32
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        numThreads = sysinfo.dwNumberOfProcessors;
#else
        numThreads = sysconf(_SC_NPROCESSORS_ONLN);
#endif
    }

#ifndef KNOB_USE_HYPERTHREAD_AS_WORKER
    pPool->numThreads /= 2;
#endif

    if (numThreads > KNOB_MAX_NUM_THREADS)
    {
        printf("WARNING: system thread count %u exceeds max %u, "
               "performance will be degraded\n",
               numThreads, KNOB_MAX_NUM_THREADS);
    }

    pPool->numThreads = std::max((UINT)KNOB_MIN_WORK_THREADS,
                                 std::min(numThreads - KNOB_WORKER_THREAD_OFFSET,
                                          (UINT)KNOB_MAX_NUM_THREADS));

    if (getenv("SWR_WORKER_THREADS"))
    {
        unsigned swrThreads;
        if (sscanf(getenv("SWR_WORKER_THREADS"), "%u", &swrThreads) == 1)
            pPool->numThreads = std::max((UINT)KNOB_MIN_WORK_THREADS,
                                         swrThreads);
        else
            printf("WARNING: SWR_WORKER_THREADS could not be parsed\n");
    }

    pContext->NumWorkerThreads = pPool->numThreads;

    pPool->inThreadShutdown = false;
    pPool->pThreadData = (THREAD_DATA *)malloc(pPool->numThreads * sizeof(THREAD_DATA));
    for (UINT i = 0; i < pPool->numThreads; ++i)
    {
        pPool->pThreadData[i].workerId = i;
#ifdef KNOB_USE_HYPERTHREAD_AS_WORKER
        pPool->pThreadData[i].threadId = i + KNOB_WORKER_THREAD_OFFSET;
#else
        pPool->pThreadData[i].threadId = i * 2 + KNOB_WORKER_THREAD_OFFSET;
#endif
        pPool->pThreadData[i].pContext = pContext;
        pPool->threads[i] = new std::thread(workerThread, &pPool->pThreadData[i]);

        if (pContext->dumpPoolInfo)
        {
            printf("Created worker thread %u\n", pPool->pThreadData[i].threadId);
        }
    }
}

void destroyThreadPool(SWR_CONTEXT *pContext, THREAD_POOL *pPool)
{
    // Inform threads to finish up
    std::unique_lock<std::mutex> lock(pContext->WaitLock);
    pPool->inThreadShutdown = true;
    _mm_mfence();
    pContext->FifosNotEmpty.notify_all();
    lock.unlock();

    // Wait for threads to finish and destroy them
    for (UINT t = 0; t < pPool->numThreads; ++t)
    {
        pPool->threads[t]->join();
        delete (pPool->threads[t]);
    }

    // Clean up data used by threads
    free(pPool->pThreadData);
}
