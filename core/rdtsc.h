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

#ifndef __SWR_RDTSC_H__
#define __SWR_RDTSC_H__

#include "os.h"
#include "utils.h"
#include "knobs.h"

#include <assert.h>

//#define ENABLE_THREAD_VIZ
#define THREAD_VIZ_START_FRAME 10
#define THREAD_VIZ_STOP_FRAME 12
#define THREAD_VIZ_MAX_EVENTS (8192 * 3)
#define MAX_BUCKETS 64

struct Bucket
{
    UINT64 start;
    UINT64 elapsed;
    UINT64 count, count2;
};

struct BucketViz
{
    UINT32 bucketId;
    UINT32 frame;
    UINT64 start;
    UINT64 stop;
    UINT64 drawId;
    UINT32 count;
};

struct BucketDef
{
    UINT level;
    UINT parentId;
    UINT numChildren;
    UINT children[MAX_BUCKETS];
    const char *name;
    bool threadViz;
};

extern Bucket g_Buckets[KNOB_MAX_NUM_THREADS + 2][MAX_BUCKETS];
extern BucketViz g_BucketViz[KNOB_MAX_NUM_THREADS + 2][THREAD_VIZ_MAX_EVENTS];
extern UINT32 g_BucketVizCurEvent[KNOB_MAX_NUM_THREADS + 2];
extern volatile UINT g_CurrentFrame;
extern BucketDef s_BucketDefs[MAX_BUCKETS];

extern THREAD UINT g_CurLevel;
extern THREAD UINT64 g_LevelStart[MAX_BUCKETS];

extern THREAD int tlsThreadId;

#undef DEF_BUCKET
#define DEF_BUCKET(level, bucket, viz) \
    const int RDTSC_##bucket = __LINE__;

#include "rdtsc_def.h"

#undef DEF_BUCKET

void rdtscInit(int threadId);
void rdtscStart(UINT bucketId);
void rdtscStop(UINT bucketId, UINT count, DRAW_T drawId);
void rdtscEvent(UINT bucketId, UINT count1, UINT count2);
void rdtscEndFrame();

#ifdef KNOB_ENABLE_RDTSC
#define RDTSC_INIT(threadId) rdtscInit(threadId)
#define RDTSC_START(bucket) rdtscStart(RDTSC_##bucket)
#define RDTSC_STOP(bucket, count, draw) rdtscStop(RDTSC_##bucket, count, draw)
#define RDTSC_EVENT(bucket, count1, count2) rdtscEvent(RDTSC_##bucket, count1, count2)
#define RDTSC_ENDFRAME() rdtscEndFrame()
#else
#define RDTSC_INIT(threadId)
#define RDTSC_START(bucket)
#define RDTSC_STOP(bucket, count, draw)
#define RDTSC_EVENT(bucket, count1, count2)
#define RDTSC_ENDFRAME()
#endif

INLINE
void rdtscStart(UINT bucketId)
{
    assert(tlsThreadId != -1);
    Bucket &bucket = g_Buckets[tlsThreadId][bucketId];
    bucket.start = __rdtsc();

#ifdef ENABLE_THREAD_VIZ
    if (g_CurrentFrame < THREAD_VIZ_START_FRAME)
        return;
    if (g_CurrentFrame > THREAD_VIZ_STOP_FRAME)
        return;
    if (g_BucketVizCurEvent[tlsThreadId] >= THREAD_VIZ_MAX_EVENTS)
        return;
    if (!s_BucketDefs[bucketId].threadViz)
        return;

    // record off start time
    g_LevelStart[g_CurLevel] = __rdtsc();

    // increment level
    g_CurLevel++;
#endif
}

INLINE
void rdtscStop(UINT bucketId, UINT count, DRAW_T drawId)
{
    assert(tlsThreadId != -1);
    Bucket &bucket = g_Buckets[tlsThreadId][bucketId];
    if (bucket.start == 0)
        return;
    UINT64 stop = __rdtsc();
    bucket.elapsed += stop - bucket.start;
    bucket.count++;
    bucket.count2 += count;

#ifdef ENABLE_THREAD_VIZ
    if (g_CurrentFrame < THREAD_VIZ_START_FRAME)
        return;
    if (g_CurrentFrame > THREAD_VIZ_STOP_FRAME)
        return;
    if (g_BucketVizCurEvent[tlsThreadId] >= THREAD_VIZ_MAX_EVENTS)
        return;
    if (g_CurLevel == 0)
        return;
    if (!s_BucketDefs[bucketId].threadViz)
        return;

    BucketViz &bucketViz = g_BucketViz[tlsThreadId][g_BucketVizCurEvent[tlsThreadId]];

    // decrement level
    g_CurLevel--;
    bucketViz.start = g_LevelStart[g_CurLevel];
    bucketViz.stop = __rdtsc();
    bucketViz.frame = g_CurrentFrame;
    bucketViz.bucketId = bucketId;
    bucketViz.count = count;
    bucketViz.drawId = drawId;

    g_BucketVizCurEvent[tlsThreadId]++;
#endif
}

INLINE
void rdtscEvent(UINT bucketId, UINT count1, UINT count2)
{
    assert(tlsThreadId != -1);
    Bucket &bucket = g_Buckets[tlsThreadId][bucketId];
    bucket.count += count1;
    bucket.count2 += count2;
}

#endif //__SWR_RDTSC_H__
