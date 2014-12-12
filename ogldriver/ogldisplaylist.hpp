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

#ifndef OGL_DISPLAY_LIST_HPP
#define OGL_DISPLAY_LIST_HPP

#include "oglstate.hpp"
#include <list>
#include <vector>

namespace OGL
{

struct State;

enum COMMAND
{
    CMD_ENDOFCHUNK,
    CMD_ENDOFSTREAM,

#include "glcmds.inl"

    CMD_NUM_COMMANDS,
};

typedef std::array<GLubyte, 6 * 4 * 1024> DLChunk;

// An attribute is 'safe' if it is set before
// the first call to glVertex* in each draw
// call in a DL. All attributes are considered
// "unsafe" unless proven "safe".
union SafeAttributes
{
    UINT64 mask;
    struct
    {
        UINT64 position : 1;
        UINT64 normal : 1;
        UINT64 fog : 1;
        UINT64 color : NUM_COLORS;
        UINT64 texCoord : NUM_TEXCOORDS;
    };
};

struct DisplayList
{
    std::list<DLChunk> mChunks;
    DLChunk::iterator mCursor;
    DLChunk::iterator mEnd;
    GLuint *mpNumVertices;
    SafeAttributes mSafeAttributes;
    VertexAttributeFormats *mpAttrFormatsSeen;
    std::vector<VertexBuffer *> mVertexBuffers;
    std::list<VertexBuffer> mVertexBuffersStore;
    GLuint mHasDrawArray;

    DisplayList();
    GLubyte *RequestBuffer(GLsizei length);
    void RequestBufferAlloc();
    GLuint AddVB();
};

struct OptState
{
    State *mpState;
    DisplayList *mpDL;
};

bool ExecuteDL(State &, GLuint list);
bool OptimizeDL(State &, DisplayList &in);
void RecordState(OptState &s, DisplayList &DL);

inline GLubyte *DisplayList::RequestBuffer(GLsizei length)
{
    // Quick check
    if ((length + sizeof(OGL::COMMAND) + sizeof(CMD_ENDOFCHUNK)) >= (std::size_t)(mEnd - mCursor))
    {
        // End previous command
        OGL::COMMAND *lastCmd = (OGL::COMMAND *)&(*mCursor);
        lastCmd[0] = CMD_ENDOFCHUNK;

        // Allocate more space.
        RequestBufferAlloc();
    }

    GLubyte *newCursor = (GLubyte *)&(*mCursor);
    mCursor += length;
    return newCursor;
}
}

#endif //OGL_DISPLAY_LIST_HPP
