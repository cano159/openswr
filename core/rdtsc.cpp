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

#include "rdtsc.h"

#include <stdio.h>
#include <string.h>

Bucket g_Buckets[KNOB_MAX_NUM_THREADS + 2][MAX_BUCKETS] = {};
BucketViz g_BucketViz[KNOB_MAX_NUM_THREADS + 2][THREAD_VIZ_MAX_EVENTS] = {};
UINT32 g_BucketVizCurEvent[KNOB_MAX_NUM_THREADS + 2] = { 0 };

const char *g_BucketNames[MAX_BUCKETS] = { 0 };

BucketDef s_BucketDefs[MAX_BUCKETS] = {};

volatile UINT g_CurrentFrame = 0;
const UINT g_StartFrame = KNOB_BUCKETS_START_FRAME;
const UINT g_EndFrame = KNOB_BUCKETS_END_FRAME;
const char *g_Filename = "rdtsc.txt";
const char *g_ThreadVizFilename = "rdtsc_viz.csv";

THREAD int tlsThreadId = -1;

THREAD UINT g_CurLevel = 0;
THREAD UINT64 g_LevelStart[MAX_BUCKETS] = { 0 };

void defBucket(UINT level, UINT id, const char *name, bool threadViz)
{
    static UINT lastBucket[MAX_BUCKETS] = { 0 };
    s_BucketDefs[id].name = name;
    s_BucketDefs[id].level = level;
    s_BucketDefs[id].threadViz = threadViz;

    // all non zero levels are children
    if (level != 0)
    {
        UINT parent = level - 1;
        UINT parentId = lastBucket[parent];
        BucketDef &def = s_BucketDefs[parentId];
        def.children[def.numChildren] = id;
        def.numChildren++;

        s_BucketDefs[id].parentId = parentId;
    }

    lastBucket[level] = id;
}

const char *arrows[] = {
    "",
    "|-> ",
    "    |-> ",
    "        |-> ",
    "            |-> ",
    "                |-> ",
    "                    |-> "
};

void rdtscInit(int threadId)
{
    if (threadId == 0)
    {
#define DEF_BUCKET(level, bucket, viz) defBucket(level, RDTSC_##bucket, #bucket, viz);
#include "rdtsc_def.h"
    }
    tlsThreadId = threadId;
}

void rdtscPrint()
{
    FILE *f = fopen(g_Filename, "w");

    UINT numFrames = g_EndFrame - g_StartFrame;

    for (UINT t = 0; t < KNOB_MAX_NUM_THREADS + 2; ++t)
    {
        fprintf(f, "\nThread %u\n", t);
        fprintf(f, " %%Tot   %%Par  Cycles     CPE        NumEvent   CPE2       NumEvent2  Bucket\n");

        // calc total cycles from all level 0 buckets
        UINT64 totalCycles = 0;
        for (UINT i = 1; i < MAX_BUCKETS; ++i)
        {
            const Bucket &bucket = g_Buckets[t][i];
            const BucketDef &def = s_BucketDefs[i];

            if (def.level == 0)
            {
                totalCycles += bucket.elapsed;
            }
        }

        for (UINT i = 1; i < MAX_BUCKETS; ++i)
        {
            const Bucket &bucket = g_Buckets[t][i];
            const BucketDef &def = s_BucketDefs[i];

            // get children's counts
            // my count = my total - sum(children)
            if (bucket.count)
            {
                // percent of total
                float percentTotal = (float)((double)bucket.elapsed / (double)totalCycles * 100.0);

                // percent of parent
                float percentParent = 100.0;
                char hier[80];
                strcpy(hier, def.name);

                if (def.level != 0)
                {
                    const Bucket &parent = g_Buckets[t][def.parentId];
                    percentParent = (float)((double)bucket.elapsed / (double)parent.elapsed * 100.0);
                    strcpy(hier, arrows[def.level]);
                    strcat(hier, def.name);
                }

                UINT64 CPE2 = 0;
                if (bucket.count2)
                {
                    CPE2 = bucket.elapsed / bucket.count2;
                }
                fprintf(f, "%6.2f %6.2f %-10llu %-10llu %-10u %-10llu %-10u %s\n", percentTotal, percentParent, (unsigned long long)(bucket.elapsed / numFrames), (unsigned long long)(bucket.elapsed / bucket.count), (UINT32)(bucket.count / numFrames), (unsigned long long)CPE2, (UINT32)(bucket.count2 / numFrames), hier);
            }
        }
    }

    fclose(f);

#ifdef ENABLE_THREAD_VIZ
    f = fopen(g_ThreadVizFilename, "w");
    for (UINT i = 0; i < KNOB_MAX_NUM_THREADS + 2; ++i)
    {
        for (UINT j = 0; j < g_BucketVizCurEvent[i]; ++j)
        {
            BucketViz &bucket = g_BucketViz[i][j];
            fprintf(f, "%u,%u,%lu,%lu,%u,%lu,", bucket.bucketId, bucket.frame, bucket.start, bucket.stop, bucket.count, bucket.drawId);
        }
        fprintf(f, "\n");
    }

    fclose(f);
#endif
}

void rdtscEndFrame()
{
    g_CurrentFrame++;
    if (g_CurrentFrame == g_StartFrame)
    {
        memset(g_Buckets, 0, sizeof(g_Buckets));
    }

    if (g_CurrentFrame == g_EndFrame)
    {
        rdtscPrint();
    }
}
