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

#include "glim.hpp"
#include "glcl.hpp"
#include "glfz.hpp"
#include "serdeser.hpp"
#include "ogldisplaylist.hpp"
#include "rdtsc.h"

namespace OGL
{

DisplayList::DisplayList()
    : mHasDrawArray(0)
{
    RequestBufferAlloc();
}

void DisplayList::RequestBufferAlloc()
{
    // Add a new request buffer
    mChunks.push_back(DLChunk());
    mCursor = mChunks.back().begin();
    mEnd = mChunks.back().end();
}

GLuint DisplayList::AddVB()
{
    mVertexBuffersStore.push_back(VertexBuffer());
    mVertexBuffers.push_back(&mVertexBuffersStore.back());

    return (GLuint)(mVertexBuffers.size() - 1);
}

// returns
// false - use optimized display list
// true - use unoptimized display list
bool PickDLPredicate(State &s, DisplayList const &in)
{
    // XXX: this is a terrible predicate. We should be observing the
    // state of the GL and the DL and determining what to do. Not
    // hacks based on size.
    // don't optimize display lists with glDrawArrays
    if (in.mHasDrawArray)
    {
        return true;
    }

    // don't optimize 'small' display lists
    // FIXME: std::list::size() is freakin slow, especially on big display lists.
    // replace with vector or something else
    if (in.mChunks.size() < 16)
    {
        return true;
    }

    return false;
}

bool ExecuteDL(State &s, GLuint list)
{
    DisplayList const &in = *DisplayLists()[list];

    RDTSC_START(APIExecuteDisplayList);

    s.mExecutingDL.push(list);

    DisplayList const &exec = *DisplayLists()[list];
    for (auto itr = exec.mChunks.begin(), end = exec.mChunks.end(); itr != end; ++itr)
    {
        GLubyte const *pCursor = &*itr->begin();
        while (((GLuint *)pCursor)[0] != CMD_ENDOFCHUNK)
        {
            switch (((GLuint *)pCursor)[0])
            {
#include "executedl.inl"
            default:
                goto exit;
            }
        }
    }

exit:
    s.mExecutingDL.pop();
    RDTSC_STOP(APIExecuteDisplayList, 1, 0);
    return true;
}

void RecordState(OptState &OS, DisplayList &DL)
{
    glclSetTopologySWR(OS, OS.mpState->mTopology);
}

bool OptimizeDL(State &s, DisplayList &in)
{
    if (PickDLPredicate(s, in))
    {
        return false;
    }

    RDTSC_START(APIOptimizeDisplayList);

    // Save off saveable state
    SaveableState state = static_cast<OGL::SaveableState>(s);

    GLuint tempDL = glimGenLists(s, 1);
    DisplayLists()[tempDL] = new OGL::DisplayList();
    OptState os = { &s, DisplayLists()[tempDL] };

    s.mExecutingDL.push(tempDL);

    // "Execute" the original list, building into the optimized list.
    for (auto itr = in.mChunks.begin(), end = in.mChunks.end(); itr != end; ++itr)
    {
        GLubyte const *pCursor = &*itr->begin();
        while (((GLuint *)pCursor)[0] != CMD_ENDOFCHUNK)
        {
            COMMAND cmd = (COMMAND)((GLuint *)pCursor)[0];
            switch (cmd)
            {
#include "optimizedl.inl"
            default:
                goto exit;
            }
        }
    }

    // Upon successfully creating optimized list, swap contents with original list.
    // XXX Move this into a DisplayList method "swapList", or "copyList" or something.
    {
        DisplayList &tempList = *DisplayLists()[tempDL];
        in.mCursor = tempList.mCursor;
        in.mEnd = tempList.mEnd;
        in.mpNumVertices = tempList.mpNumVertices;
        in.mSafeAttributes = tempList.mSafeAttributes;
        in.mpAttrFormatsSeen = tempList.mpAttrFormatsSeen;
        in.mHasDrawArray = tempList.mHasDrawArray;
        in.mChunks.swap(tempList.mChunks);
        in.mVertexBuffers.swap(tempList.mVertexBuffers);
        in.mVertexBuffersStore.swap(tempList.mVertexBuffersStore);
    }

exit:
    // Delete temporary list
    DisplayLists().erase(tempDL);

    s.mExecutingDL.pop();
    // restore state
    static_cast<OGL::SaveableState>(s) = state;

    RDTSC_STOP(APIOptimizeDisplayList, 1, 0);
    return true;
}
}
