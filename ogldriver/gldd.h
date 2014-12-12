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

#ifndef OGL_DD_HPP
#define OGL_DD_HPP

#include "formats.h"
#include "oglglobals.h"

#if defined(_WIN32)
#define _GDI32_

#include <windows.h>
#undef min
#undef max
#endif

namespace OGL
{

union VertexActiveAttributes;
union VertexAttributeFormats;
struct State;
}

typedef DDHANDLE (*DD_PFN_CREATE_CONTEXT)();
typedef void (*DD_PFN_BIND_CONTEXT)(DDHANDLE, DDHANDLE, DDHANDLE, DDHANDLE, INT32, INT32);
typedef void (*DD_PFN_DESTROY_CONTEXT)(DDHANDLE);

typedef void (*DD_PFN_PRESENT)(DDHANDLE);
typedef void (*DD_PFN_PRESENT2)(DDHANDLE, void *, UINT);
typedef void (*DD_PFN_SWAP_BUFFER)(DDHANDLE);
typedef void (*DD_PFN_SET_VIEWPORT)(DDHANDLE, INT32 x, INT32 y, UINT32 width, UINT32 height, float minZ, float maxZ, bool scissorEnable);
typedef void (*DD_PFN_SET_CULLMODE)(DDHANDLE, GLenum cullMode);
typedef void (*DD_PFN_CLEAR)(DDHANDLE, GLbitfield, FLOAT (&clr)[4], FLOAT, bool useScissor);
typedef void (*DD_PFN_SET_SCISSOR_RECT)(DDHANDLE, GLuint, GLuint, GLuint, GLuint);
typedef void (*DD_PFN_SETUP_VERTICES)(DDHANDLE, OGL::VertexActiveAttributes const &, OGL::VertexAttributeFormats const &, GLuint numBufs, DDHBUFFER *phBufs, DDHBUFFER hNIB);
typedef void (*DD_PFN_SETUP_SPARSE_VERTICES)(DDHANDLE, const OGL::State &, OGL::VertexActiveAttributes const &, GLuint numBufs, DDHBUFFER *phBufs, DDHBUFFER hNIB);
typedef void (*DD_PFN_VB_LAYOUT_INFO)(DDHANDLE, GLuint &permuteWidth);

typedef DDHBUFFER (*DD_PFN_CREATE_RENDERTARGET)(DDHANDLE, GLuint width, GLuint height, SWR_FORMAT format);
typedef void (*DD_PFN_DESTROY_RENDERTARGET)(DDHANDLE, DDHANDLE);
typedef DDHBUFFER (*DD_PFN_CREATE_BUFFER)(DDHANDLE, GLuint size, GLvoid *pData);
typedef void *(*DD_PFN_LOCK_BUFFER)(DDHANDLE, DDHBUFFER hBuf);
typedef void (*DD_PFN_UNLOCK_BUFFER)(DDHANDLE, DDHBUFFER hBuf);
typedef void (*DD_PFN_DESTROY_BUFFER)(DDHANDLE, DDHBUFFER hBuf);
typedef void (*DD_PFN_SET_CONSTANT_BUFFER)(DDHANDLE, DDHBUFFER hcBuf);
typedef void (*DD_PFN_SET_INDEX_BUFFER)(DDHANDLE, DDHBUFFER);
typedef void (*DD_PFN_SET_RENDER_TARGET)(DDHANDLE, DDHANDLE, DDHANDLE);

typedef DDHTEXTURE (*DD_PFN_CREATE_TEXTURE)(DDHANDLE, GLuint (&size)[3]);
typedef void (*DD_PFN_LOCK_TEXTURE)(DDHANDLE, DDHTEXTURE hTex);
typedef void (*DD_PFN_UNLOCK_TEXTURE)(DDHANDLE, DDHTEXTURE hTex);
typedef GLuint (*DD_PFN_GET_SUBTEXTURE_INDEX)(DDHANDLE, DDHTEXTURE hTex, GLuint plane, GLuint mipLevel);
typedef void (*DD_PFN_UPDATE_SUBTEXTURE)(DDHANDLE, DDHTEXTURE hTex, GLuint subtexIdx, GLenum format, GLenum type, GLuint xoffset, GLuint yoffset, GLuint width, GLuint height, const GLvoid *pData);
typedef void (*DD_PFN_DESTROY_TEXTURE)(DDHANDLE, DDHTEXTURE hTex);

typedef void (*DD_PFN_GEN_SHADERS)(DDHANDLE, OGL::State &, bool, GLenum);

typedef DDHANDLE (*DD_PFN_NEW_DRAW)(DDHANDLE);
typedef void (*DD_PFN_DRAW)(DDHANDLE, GLenum topology, GLuint startVertex, GLuint numVertices);
typedef void (*DD_PFN_DRAW_INDEXED)(DDHANDLE, GLenum topology, GLenum type, GLuint numVertices, GLuint indexOffset);

typedef void (*DD_PFN_COPY_RENDERTARGET)(DDHANDLE, const OGL::State &, DDHANDLE, GLvoid *, GLenum, GLenum, GLint, GLint, GLint, GLint, GLuint, GLuint, GLboolean);

typedef unsigned (*DD_PFN_GET_THREAD_COUNT)(DDHANDLE);

struct DDProcTable
{
    DD_PFN_CREATE_CONTEXT pfnCreateContext;
    DD_PFN_BIND_CONTEXT pfnBindContext;
    DD_PFN_DESTROY_CONTEXT pfnDestroyContext;

    DD_PFN_NEW_DRAW pfnNewDraw;

    DD_PFN_PRESENT pfnPresent;
    DD_PFN_PRESENT2 pfnPresent2;
    DD_PFN_SWAP_BUFFER pfnSwapBuffer;
    DD_PFN_SET_VIEWPORT pfnSetViewport;
    DD_PFN_SET_CULLMODE pfnSetCullMode;
    DD_PFN_CLEAR pfnClear;
    DD_PFN_SET_SCISSOR_RECT pfnSetScissorRect;
    DD_PFN_SETUP_VERTICES pfnSetupVertices;
    DD_PFN_SETUP_SPARSE_VERTICES pfnSetupSparseVertices;
    DD_PFN_VB_LAYOUT_INFO pfnVBLayoutInfo;

    DD_PFN_CREATE_RENDERTARGET pfnCreateRenderTarget;
    DD_PFN_DESTROY_RENDERTARGET pfnDestroyRenderTarget;
    DD_PFN_CREATE_BUFFER pfnCreateBuffer;
    DD_PFN_LOCK_BUFFER pfnLockBuffer;
    DD_PFN_LOCK_BUFFER pfnLockBufferNoOverwrite;
    DD_PFN_LOCK_BUFFER pfnLockBufferDiscard;
    DD_PFN_UNLOCK_BUFFER pfnUnlockBuffer;
    DD_PFN_DESTROY_BUFFER pfnDestroyBuffer;
    DD_PFN_SET_RENDER_TARGET pfnSetRenderTarget;

    DD_PFN_CREATE_TEXTURE pfnCreateTexture;
    DD_PFN_LOCK_TEXTURE pfnLockTexture;
    DD_PFN_UNLOCK_TEXTURE pfnUnlockTexture;
    DD_PFN_GET_SUBTEXTURE_INDEX pfnGetSubtextureIndex;
    DD_PFN_UPDATE_SUBTEXTURE pfnUpdateSubtexture;
    DD_PFN_DESTROY_TEXTURE pfnDestroyTexture;

    DD_PFN_SET_CONSTANT_BUFFER pfnSetPsBuffer;
    DD_PFN_SET_CONSTANT_BUFFER pfnSetVsBuffer;

    DD_PFN_SET_INDEX_BUFFER pfnSetIndexBuffer;

    DD_PFN_GEN_SHADERS pfnGenShaders;

    DD_PFN_DRAW pfnDraw;
    DD_PFN_DRAW_INDEXED pfnDrawIndexed;

    DD_PFN_COPY_RENDERTARGET pfnCopyRenderTarget;

    DD_PFN_GET_THREAD_COUNT pfnGetThreadCount;
};

bool DDInitProcTable(DDProcTable &procTable);

#endif //OGL_DD_HPP
