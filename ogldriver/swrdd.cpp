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

#define _CRT_SECURE_NO_WARNINGS

#include "api.h"
#include "context.h"
#include "shaders.h"

#include "gldd.h"
#include "oglstate.hpp"
#include "compiler.h"

#include "simdintrin.h"

#include "swrffgen.h"

#include <map>
#include <unordered_map>
#include <queue>

#define MAX_SHADER_QUEUE_SIZE 256

struct DDFetchInfo
{
    INPUT_ELEMENT_DESC mIEDs[KNOB_NUM_STREAMS];
    UINT mNumIEDs;
    PFN_FETCH_FUNC mpfnFetchFunc;
};

typedef std::unordered_map<UINT, DDFetchInfo> FetchMap;

struct PipeInfo
{
    _simd_crcint name;
    PFN_FETCH_FUNC FS;
    PFN_VERTEX_FUNC VS;
    PFN_PIXEL_FUNC PS;
    UINT frontLinkageMask;
    UINT backLinkageMask;
};

typedef std::unordered_map<_simd_crcint, PipeInfo> PipeMap;

struct DDPrivateData
{
    HANDLE mhContext;

    HANDLE mhRenderTarget;

    UINT32 mNumIEDs;
    INPUT_ELEMENT_DESC mIEDs[KNOB_NUM_ATTRIBUTES];
    UINT32 mVBStrides[KNOB_NUM_STREAMS];

    HANDLE mhNIB8;
    HANDLE mhFSConst;
    HANDLE mhVSConst;
    HANDLE mhPSConst;

    HANDLE mhCompiler;

    PipeMap mPipeCache;

    UINT mNumNumaNodes;
    UINT mCurrentNumaNode;
};

struct DDTextureInfo
{
    HANDLE mhTexture;
    HANDLE mhTextureView;
    HANDLE mhSampler;
};

DDHANDLE DDCreateContext()
{
    DDPrivateData *ddPD = new DDPrivateData();
    ddPD->mhContext = SwrCreateContext(GL);

    ddPD->mhCompiler = swrcCreateCompiler(KNOB_VS_SIMD_WIDTH);
    ddPD->mhFSConst = SwrCreateBuffer(ddPD->mhContext, OGL::NUM_ATTRIBUTES * 4 * sizeof(float)); // enough to hold the vertex attributes
#if defined(KNOB_FULL_SWRFF)
    ddPD->mhVSConst = SwrCreateBuffer(ddPD->mhContext, sizeof(OGL::SaveableState));
#else
    ddPD->mhVSConst = SwrCreateBuffer(ddPD->mhContext, 336);
#endif
    ddPD->mhPSConst = SwrCreateBuffer(ddPD->mhContext, 256);

    SwrSetDebugInfo(ddPD->mhContext, DBGINFO_DUMP_FPS);

    ddPD->mNumNumaNodes = SwrGetNumNumaNodes(ddPD->mhContext);
    ddPD->mCurrentNumaNode = 0;

    return (DDHANDLE)ddPD;
}

void DDDestroyContext(DDHANDLE hddPD)
{
    DDPrivateData *ddPD = reinterpret_cast<DDPrivateData *>(hddPD);

    SwrDestroyBuffer(ddPD->mhContext, ddPD->mhFSConst);
    SwrDestroyBuffer(ddPD->mhContext, ddPD->mhVSConst);
    SwrDestroyBuffer(ddPD->mhContext, ddPD->mhPSConst);
    swrcDestroyCompiler(ddPD->mhCompiler);
    SwrDestroyContext(ddPD->mhContext);

    delete ddPD;
}

void DDBindContext(DDHANDLE hddPD, DDHANDLE hDisplay, DDHANDLE HWND, DDHANDLE hVisInfo, INT32 width, INT32 height)
{
    DDPrivateData &ddPD = *reinterpret_cast<DDPrivateData *>(hddPD);

#ifdef _WIN32
    SwrCreateSwapChain(ddPD.mhContext, hDisplay, HWND, hVisInfo, 2, width, height);

    ddPD.mhRenderTarget = SwrGetBackBuffer(ddPD.mhContext, 0);
    HANDLE hDepth = SwrGetDepthBuffer(ddPD.mhContext);

    SwrSetRenderTargets(ddPD.mhContext, ddPD.mhRenderTarget, hDepth);
#endif
}

void DDPresent(DDHANDLE hddPD)
{
    DDPrivateData &ddPD = *reinterpret_cast<DDPrivateData *>(hddPD);

    SwrPresent(ddPD.mhContext);
}

void DDPresent2(DDHANDLE hddPD, void *pData, UINT pitch)
{
    DDPrivateData &ddPD = *reinterpret_cast<DDPrivateData *>(hddPD);

    SwrPresent2(ddPD.mhContext, pData, pitch);
}

void DDSwapBuffer(DDHANDLE hddPD)
{
    DDPrivateData &ddPD = *reinterpret_cast<DDPrivateData *>(hddPD);

#ifdef _WIN32
    ddPD.mhRenderTarget = SwrGetBackBuffer(ddPD.mhContext, 0);
    HANDLE hDepth = SwrGetDepthBuffer(ddPD.mhContext);
    SwrSetRenderTargets(ddPD.mhContext, ddPD.mhRenderTarget, hDepth);
#endif
}

void DDSetViewport(DDHANDLE hddPD, INT32 x, INT32 y, UINT32 width, UINT32 height, float minZ, float maxZ, bool scissorEnable)
{
    DDPrivateData &ddPD = *reinterpret_cast<DDPrivateData *>(hddPD);

    RASTSTATE rast;
    SwrGetRastState(ddPD.mhContext, &rast);

    rast.vp.x = (FLOAT)x;
    rast.vp.y = (FLOAT)y;
    rast.vp.width = (FLOAT)width;
    rast.vp.height = (FLOAT)height;
    rast.vp.halfWidth = rast.vp.width / 2;
    rast.vp.halfHeight = rast.vp.height / 2;
    rast.vp.minZ = minZ;
    rast.vp.maxZ = maxZ;
    rast.scissorEnable = scissorEnable;

    SwrSetRastState(ddPD.mhContext, &rast);

#ifdef _WIN32
    // Resize the swap chain if needed
    SwrResizeSwapChain(ddPD.mhContext, width, height);

    // reset render target
    ddPD.mhRenderTarget = SwrGetBackBuffer(ddPD.mhContext, 0);
    HANDLE hDepth = SwrGetDepthBuffer(ddPD.mhContext);

    SwrSetRenderTargets(ddPD.mhContext, ddPD.mhRenderTarget, hDepth);
#endif
}

void DDSetCullMode(DDHANDLE hddPD, GLenum cullMode)
{
    DDPrivateData &ddPD = *reinterpret_cast<DDPrivateData *>(hddPD);

    RASTSTATE rast;
    SwrGetRastState(ddPD.mhContext, &rast);

    switch (cullMode)
    {
    case GL_CCW:
        rast.cullMode = CCW;
        break;
    case GL_CW:
        rast.cullMode = CW;
        break;
    case GL_NONE:
        rast.cullMode = NONE;
        break;
    }

    SwrSetRastState(ddPD.mhContext, &rast);
}

void DDClear(DDHANDLE hddPD, GLbitfield mask, FLOAT (&clr)[4], FLOAT z, bool useScissor)
{
    DDPrivateData &ddPD = *reinterpret_cast<DDPrivateData *>(hddPD);

    UINT swrFlags = 0;
    if (mask & GL_COLOR_BUFFER_BIT)
    {
        swrFlags |= CLEAR_COLOR;
    }

    if (mask & GL_DEPTH_BUFFER_BIT)
    {
        swrFlags |= CLEAR_DEPTH;
    }

    if (swrFlags)
    {
        SwrClearRenderTarget(ddPD.mhContext, ddPD.mhRenderTarget, swrFlags, clr, z, useScissor);
    }
}

void DDSetScissorRect(DDHANDLE hddPD, GLuint x, GLuint y, GLuint width, GLuint height)
{
    DDPrivateData &ddPD = *reinterpret_cast<DDPrivateData *>(hddPD);
    SwrSetScissorRect(ddPD.mhContext, x, y, x + width, y + height);
}

DDHBUFFER DDCreateRenderTarget(DDHANDLE hddPD, GLuint width, GLuint height, SWR_FORMAT format)
{
    DDPrivateData &ddPD = *reinterpret_cast<DDPrivateData *>(hddPD);
    return (DDHBUFFER)SwrCreateRenderTarget(ddPD.mhContext, width, height, format);
}

void DDDestroyRenderTarget(DDHANDLE hddPD, DDHANDLE hrt)
{
    DDPrivateData &ddPD = *reinterpret_cast<DDPrivateData *>(hddPD);
    SwrDestroyRenderTarget(ddPD.mhContext, hrt);
}

DDHBUFFER DDCreateBuffer(DDHANDLE hddPD, GLuint size, GLvoid *pData)
{
    DDPrivateData &ddPD = *reinterpret_cast<DDPrivateData *>(hddPD);

    if (pData)
    {
        return SwrCreateBufferUP(ddPD.mhContext, pData);
    }
    else
    {
        return SwrCreateBuffer(ddPD.mhContext, size, ddPD.mCurrentNumaNode);
    }
}

void *DDLockBuffer(DDHANDLE hddPD, DDHBUFFER ddhB)
{
    DDPrivateData &ddPD = *reinterpret_cast<DDPrivateData *>(hddPD);

    return SwrLockResource(ddPD.mhContext, ddhB, LOCK_NONE);
}

void *DDLockBufferNoOverwrite(DDHANDLE hddPD, DDHBUFFER ddhB)
{
    DDPrivateData &ddPD = *reinterpret_cast<DDPrivateData *>(hddPD);

    return SwrLockResource(ddPD.mhContext, ddhB, LOCK_NOOVERWRITE);
}

void *DDLockBufferDiscard(DDHANDLE hddPD, DDHBUFFER ddhB)
{
    DDPrivateData &ddPD = *reinterpret_cast<DDPrivateData *>(hddPD);

    return SwrLockResource(ddPD.mhContext, ddhB, LOCK_DISCARD);
}

void DDUnlockBuffer(DDHANDLE hddPD, DDHBUFFER ddhB)
{
    DDPrivateData &ddPD = *reinterpret_cast<DDPrivateData *>(hddPD);

    SwrUnlock(ddPD.mhContext, ddhB);
}

void DDDestroyBuffer(DDHANDLE hddPD, DDHBUFFER ddhB)
{
    DDPrivateData &ddPD = *reinterpret_cast<DDPrivateData *>(hddPD);

    SwrDestroyBuffer(ddPD.mhContext, ddhB);
}

static const SWR_FORMAT FormatTable[8] = {
    R32_FLOAT,
    RG32_FLOAT,
    RGB32_FLOAT,
    RGBA32_FLOAT,

    R8_UNORM,
    RG8_UNORM,
    RGB8_UNORM,
    RGBA8_UNORM,
};

static const GLuint FormatWidths[8] =
    {
      1 * sizeof(GLfloat), 2 * sizeof(GLfloat), 3 * sizeof(GLfloat), 4 * sizeof(GLfloat),
      1 * sizeof(GLfloat), 2 * sizeof(GLfloat), 3 * sizeof(GLfloat), 4 * sizeof(GLfloat),
    };

void DDSetupVertices(DDHANDLE hddPD, OGL::VertexActiveAttributes const &vAttrs, OGL::VertexAttributeFormats const &attrFmts, GLuint numBufs, DDHBUFFER *phBufs, DDHBUFFER hNIB8)
{
    DDPrivateData &ddPD = *reinterpret_cast<DDPrivateData *>(hddPD);

    ddPD.mhNIB8 = hNIB8;

    INPUT_ELEMENT_DESC(&IEDs)[KNOB_NUM_ATTRIBUTES] = ddPD.mIEDs;
    GLuint(&strides)[KNOB_NUM_STREAMS] = ddPD.mVBStrides;
    UINT32 &iedx = ddPD.mNumIEDs;
    iedx = 0;

    // The streams are guaranteed to be packed together.
    IEDs[iedx].AlignedByteOffset = 0;
    IEDs[iedx].Format = FormatTable[attrFmts.position];
    IEDs[iedx].Semantic = POSITION;
    IEDs[iedx].SemanticIndex = 0;
    IEDs[iedx].StreamIndex = iedx;
    IEDs[iedx].Constant = 0;
    strides[iedx] = FormatWidths[attrFmts.position];

    if (vAttrs.normal)
    {
        ++iedx;
        IEDs[iedx].AlignedByteOffset = 0;
        IEDs[iedx].Format = FormatTable[attrFmts.normal];
        IEDs[iedx].Semantic = NORMAL;
        IEDs[iedx].SemanticIndex = 0;
        IEDs[iedx].StreamIndex = iedx;
        IEDs[iedx].Constant = 0;
        strides[iedx] = FormatWidths[attrFmts.normal];
    }

    if (vAttrs.fog)
    {
        ++iedx;
        IEDs[iedx].AlignedByteOffset = 0;
        IEDs[iedx].Format = FormatTable[attrFmts.fog];
        IEDs[iedx].Semantic = COLOR;
        IEDs[iedx].SemanticIndex = 2;
        IEDs[iedx].StreamIndex = iedx;
        IEDs[iedx].Constant = 0;
        strides[iedx] = FormatWidths[attrFmts.fog];
    }

    for (int i = 0; i < OGL::NUM_COLORS; ++i)
    {
        if ((vAttrs.color & (0x1ULL << i)) != 0)
        {
            ++iedx;
            auto fmt = (attrFmts.color >> (OGL::AttrFmtShift * i)) & (OGL::AttrFmtMask);
            IEDs[iedx].AlignedByteOffset = 0;
            IEDs[iedx].Format = FormatTable[fmt];
            IEDs[iedx].Semantic = COLOR;
            IEDs[iedx].SemanticIndex = i;
            IEDs[iedx].StreamIndex = iedx;
            IEDs[iedx].Constant = 0;
            strides[iedx] = FormatWidths[fmt];
        }
    }

    for (int i = 0; i < OGL::NUM_TEXTURES; ++i)
    {
        if ((vAttrs.texCoord & (0x1ULL << i)) != 0)
        {
            ++iedx;
            auto fmt = (attrFmts.texCoord >> (OGL::AttrFmtShift * i)) & (OGL::AttrFmtMask);
            IEDs[iedx].AlignedByteOffset = 0;
            IEDs[iedx].Format = FormatTable[fmt];
            IEDs[iedx].Semantic = TEXCOORD;
            IEDs[iedx].SemanticIndex = i;
            IEDs[iedx].StreamIndex = iedx;
            IEDs[iedx].Constant = 0;
            strides[iedx] = FormatWidths[fmt];
        }
    }

    // Convert iedx from an index to a size.
    ++iedx;

    SwrSetFsConstantBuffer(ddPD.mhContext, ddPD.mhFSConst);

    SwrSetNumAttributes(ddPD.mhContext, iedx);

    // Regular FetchShader path.
    SwrSetVertexBuffers(ddPD.mhContext, 0, numBufs, phBufs);
}

// @todo make this better
INLINE
SWR_FORMAT GLToSWRFormat(GLuint numComps, GLenum format, GLenum type, bool normalized)
{
    SWR_TYPE swrType;
    switch (type)
    {
    case GL_FLOAT:
        swrType = SWR_TYPE_FLOAT;
        break;
    case GL_BYTE:
        swrType = (normalized ? SWR_TYPE_SNORM8 : SWR_TYPE_UNKNOWN);
        break;
    case GL_UNSIGNED_BYTE:
    case GL_UNSIGNED_INT_8_8_8_8_REV:
        swrType = (normalized ? SWR_TYPE_UNORM8 : SWR_TYPE_UNKNOWN);
        break;
    case GL_SHORT:
        swrType = (normalized ? SWR_TYPE_SNORM16 : SWR_TYPE_SINT16);
        break;
    case GL_UNSIGNED_SHORT:
        swrType = (normalized ? SWR_TYPE_UNORM16 : SWR_TYPE_UNKNOWN);
        break;
    default:
        swrType = SWR_TYPE_FLOAT;
        assert(0 && "Unsupported type");
        break;
    }

    INT rpos, gpos, bpos, apos;
    switch (format)
    {
    case GL_RGB:
    case GL_RGBA:
        rpos = 0;
        gpos = 1;
        bpos = 2;
        apos = 3;
        break;

    case GL_BGR:
    case GL_BGRA:
        rpos = 2;
        gpos = 1;
        bpos = 0;
        apos = 3;
        break;

    case GL_ALPHA:
        apos = 0;
        break;

    case GL_DEPTH_COMPONENT:
    case GL_STENCIL_INDEX:
        rpos = 0;
        break;
    default:
        assert(0 && "Unsupported type");
        break;
    }

    if (format == GL_ALPHA)
    {
        rpos = gpos = bpos = -1;
    }
    else
    {
        switch (numComps)
        {
        case 1:
            gpos = -1;
        case 2:
            bpos = -1;
        case 3:
            apos = -1;
            break;
        }
    }

    SWR_FORMAT swrFormat = GetMatchingFormat(swrType, numComps, rpos, gpos, bpos, apos);
    assert(swrFormat != NULL_FORMAT);

    return (swrFormat);
}

#if defined(WIN32)
#pragma warning(disable : 4244)
#endif
void DDSetupSparseVertices(DDHANDLE hddPD, const OGL::State &s, OGL::VertexActiveAttributes const &vAttrs, GLuint numBufs, DDHBUFFER *phBufs, DDHANDLE hNIB8)
{
    DDPrivateData &ddPD = *reinterpret_cast<DDPrivateData *>(hddPD);

    ddPD.mhNIB8 = hNIB8;

    INPUT_ELEMENT_DESC(&IEDs)[KNOB_NUM_ATTRIBUTES] = ddPD.mIEDs;
    GLuint(&strides)[KNOB_NUM_STREAMS] = ddPD.mVBStrides;
    UINT32 &iedx = ddPD.mNumIEDs;
    iedx = 0;

    IEDs[iedx].AlignedByteOffset = 0;
    IEDs[iedx].Format = GLToSWRFormat(s.mArrayPointers.mVertex.mNumCoordsPerAttribute, GL_RGBA, s.mArrayPointers.mVertex.mType, false);
    IEDs[iedx].Semantic = POSITION;
    IEDs[iedx].SemanticIndex = 0;
    IEDs[iedx].StreamIndex = iedx;
    IEDs[iedx].Constant = 0;
    strides[iedx] = s.mArrayPointers.mVertex.mStride;

    // active VBOs need to fix up the offset
    if (s.mActiveVBOs.vertex)
    {
        IEDs[iedx].AlignedByteOffset = reinterpret_cast<uintptr_t>(s.mArrayPointers.mVertex.mpElements);
    }

    // Dump the current vertex attributes to the constant buffer if needed
    BYTE *pFSConst = (BYTE *)DDLockBufferDiscard(hddPD, ddPD.mhFSConst);

    if (vAttrs.normal)
    {
        ++iedx;
        IEDs[iedx].AlignedByteOffset = 0;
        IEDs[iedx].Semantic = NORMAL;
        IEDs[iedx].SemanticIndex = 0;
        IEDs[iedx].StreamIndex = iedx;

        if (s.mCaps.normalArray)
        {
            IEDs[iedx].Format = GLToSWRFormat(s.mArrayPointers.mNormal.mNumCoordsPerAttribute, GL_RGBA, s.mArrayPointers.mNormal.mType, true);
            strides[iedx] = s.mArrayPointers.mNormal.mStride;
            IEDs[iedx].Constant = 0;

            // active VBOs need to fix up the offset
            if (s.mActiveVBOs.normal)
            {
                IEDs[iedx].AlignedByteOffset = reinterpret_cast<uintptr_t>(s.mArrayPointers.mNormal.mpElements);
            }
        }
        else
        {
            IEDs[iedx].Format = RGBA32_FLOAT;
            strides[iedx] = 16;
            IEDs[iedx].Constant = 1;
            __m128 vAttrib = _mm_loadu_ps((const float *)&s.mNormal);
            _mm_store_ps((float *)pFSConst, vAttrib);
            pFSConst += 16;
        }
    }

    if (vAttrs.fog)
    {
        ++iedx;
        IEDs[iedx].AlignedByteOffset = 0;
        IEDs[iedx].Semantic = COLOR;
        IEDs[iedx].SemanticIndex = 2;
        IEDs[iedx].StreamIndex = iedx;

        if (s.mCaps.fogArray)
        {
            IEDs[iedx].Format = GLToSWRFormat(s.mArrayPointers.mFog.mNumCoordsPerAttribute, GL_RGBA, s.mArrayPointers.mFog.mType, false);
            strides[iedx] = s.mArrayPointers.mFog.mStride;
            IEDs[iedx].Constant = 0;

            // active VBOs need to fix up the offset
            if (s.mActiveVBOs.fog)
            {
                IEDs[iedx].AlignedByteOffset = reinterpret_cast<uintptr_t>(s.mArrayPointers.mFog.mpElements);
            }
        }
        else
        {
            IEDs[iedx].Format = RGBA32_FLOAT;
            strides[iedx] = 16;
            IEDs[iedx].Constant = 1;
            __m128 vAttrib = _mm_loadu_ps((const float *)&s.mFog.mColor);
            _mm_store_ps((float *)pFSConst, vAttrib);
            pFSConst += 16;
        }
    }

    for (int i = 0; i < OGL::NUM_COLORS; ++i)
    {
        if ((vAttrs.color & (0x1ULL << i)) != 0)
        {
            ++iedx;
            IEDs[iedx].AlignedByteOffset = 0;
            IEDs[iedx].Semantic = COLOR;
            IEDs[iedx].SemanticIndex = i;
            IEDs[iedx].StreamIndex = iedx;

            if (s.mCaps.colorArray & (0x1ULL << i))
            {
                IEDs[iedx].Format = GLToSWRFormat(s.mArrayPointers.mColor[i].mNumCoordsPerAttribute, GL_RGBA, s.mArrayPointers.mColor[i].mType, true);
                strides[iedx] = s.mArrayPointers.mColor[i].mStride;
                IEDs[iedx].Constant = 0;

                // active VBOs need to fix up the offset
                if (s.mActiveVBOs.color[i])
                {
                    IEDs[iedx].AlignedByteOffset = reinterpret_cast<uintptr_t>(s.mArrayPointers.mColor[i].mpElements);
                }
            }
            else
            {
                IEDs[iedx].Format = RGBA32_FLOAT;
                strides[iedx] = 16;
                IEDs[iedx].Constant = 1;
                __m128 vAttrib = _mm_loadu_ps((const float *)&s.mColor[i]);
                _mm_store_ps((float *)pFSConst, vAttrib);
                pFSConst += 16;
            }
        }
    }

    for (int i = 0; i < OGL::NUM_TEXTURES; ++i)
    {
        if ((vAttrs.texCoord & (0x1ULL << i)) != 0)
        {
            ++iedx;
            IEDs[iedx].AlignedByteOffset = 0;
            IEDs[iedx].Semantic = TEXCOORD;
            IEDs[iedx].SemanticIndex = i;
            IEDs[iedx].StreamIndex = iedx;

            if (s.mCaps.texCoordArray & (1ULL << i))
            {
                IEDs[iedx].Format = GLToSWRFormat(s.mArrayPointers.mTexCoord[i].mNumCoordsPerAttribute, GL_RGBA, s.mArrayPointers.mTexCoord[i].mType, false);
                strides[iedx] = s.mArrayPointers.mTexCoord[i].mStride;
                IEDs[iedx].Constant = 0;

                // active VBOs need to fix up the offset
                if (s.mActiveVBOs.texcoord[i])
                {
                    IEDs[iedx].AlignedByteOffset = reinterpret_cast<uintptr_t>(s.mArrayPointers.mTexCoord[i].mpElements);
                }
            }
            else
            {
                IEDs[iedx].Format = RGBA32_FLOAT;
                strides[iedx] = 16;
                IEDs[iedx].Constant = 1;
                __m128 vAttrib = _mm_loadu_ps((const float *)&s.mTexCoord[i]);
                _mm_store_ps((float *)pFSConst, vAttrib);
                pFSConst += 16;
            }
        }
    }

    // Convert iedx from an index to a size.
    ++iedx;

    DDUnlockBuffer(hddPD, ddPD.mhFSConst);
    SwrSetFsConstantBuffer(ddPD.mhContext, ddPD.mhFSConst);

    SwrSetNumAttributes(ddPD.mhContext, iedx);

    // Regular FetchShader path.
    SwrSetVertexBuffers(ddPD.mhContext, 0, numBufs, phBufs);
}
#if defined(WIN32)
#pragma warning(default : 4244)
#endif

void DDSetPsBuffer(DDHANDLE hddPD, DDHBUFFER ddhPS)
{
    DDPrivateData &ddPD = *reinterpret_cast<DDPrivateData *>(hddPD);

    SwrSetPsConstantBuffer(ddPD.mhContext, ddhPS);
}

void DDSetVsBuffer(DDHANDLE hddPD, DDHBUFFER ddhVS)
{
    DDPrivateData &ddPD = *reinterpret_cast<DDPrivateData *>(hddPD);

    SwrSetVsConstantBuffer(ddPD.mhContext, ddhVS);
}

void DDSetIndexBuffer(DDHANDLE hddPD, DDHBUFFER hIndexBuffer)
{
    DDPrivateData &ddPD = *reinterpret_cast<DDPrivateData *>(hddPD);

    SwrSetIndexBuffer(ddPD.mhContext, hIndexBuffer);
}

SWR_ZFUNCTION GLDepthFuncToSWR(GLenum depthFunc)
{
    switch (depthFunc)
    {
    case GL_LESS:
        return ZFUNC_LT;
    case GL_EQUAL:
        return ZFUNC_EQ;
    case GL_ALWAYS:
        return ZFUNC_ALWAYS;
    case GL_LEQUAL:
        return ZFUNC_LE;
    case GL_GREATER:
        return ZFUNC_GT;
    case GL_GEQUAL:
        return ZFUNC_GE;
    case GL_NEVER:
        return ZFUNC_NEVER;
    default:
        assert(0);
    }

    return ZFUNC_LE;
}

extern void visitSplat(const SWR_TRIANGLE_DESC &work, SWR_PIXELOUTPUT &pOut);

PFN_PIXEL_FUNC ChoosePixelShader(const OGL::State &s, UINT numTextures)
{
    UINT depthFunc, depthMask;
    PFN_PIXEL_FUNC *psTable;

    // choose depth
    if (s.mCaps.depthTest == 0)
    {
        depthFunc = ZFUNC_ALWAYS;
        depthMask = 0;
    }
    else
    {
        depthFunc = GLDepthFuncToSWR(s.mDepthFunc);
        depthMask = s.mDepthMask;
    }

// Choose pixel shader table based on combination of lighting and texturing
#if KNOB_USE_UBER_FRAG_SHADER
    if ((numTextures == 1) && s.mTexUnit[0].mTexEnv.mMode == GL_MODULATE &&
        !s.mCaps.alphatest &&
        s.mBlendFuncSFactor == GL_SRC_ALPHA &&
        s.mBlendFuncDFactor == GL_ONE_MINUS_SRC_ALPHA &&
        s.mColorMask.red &&
        s.mColorMask.green &&
        s.mColorMask.blue &&
        s.mColorMask.alpha)
    {
        return visitSplat;
    }

    (void)psTable;
    return GLFragFFTable[numTextures][depthFunc][depthMask];
#else
    if (s.mCaps.textures)
    {
        if (s.mCaps.blend)
        {
            if (s.mBlendFuncSFactor == GL_ONE && s.mBlendFuncDFactor == GL_ONE)
            {
                psTable = &InterpColorTexBlendShaderTable[0][0];
            }
            else
            {
                psTable = &InterpColorTexBlendAlphaShaderTable[0][0];
            }
        }
        else
        {
            psTable = &InterpColorTexShaderTable[0][0];
        }
    }
    else
    {
        psTable = &GLPSLightShaderTable[0][0];
    }

    return *(psTable + depthFunc * 2 + depthMask);
#endif
}

void DDGenShaders(DDHANDLE hddPD, OGL::State &s, bool isIndexed, GLenum idxType)
{
    DDPrivateData &ddPD = *reinterpret_cast<DDPrivateData *>(hddPD);

    // pack active textures
    UINT curSlot = 0;
    for (UINT i = 0; i < KNOB_NUMBER_OF_TEXTURE_VIEWS; ++i)
    {
        if (s.mCaps.textures & (1 << i))
        {
            OGL::TextureUnit &texUnit = s.mTexUnit[i];

            // set up texture state
            OGL::TexParameters &texParams = s.mTexParameters[texUnit.mNamedTexture2d];
            DDTextureInfo *texInfo = (DDTextureInfo *)texParams.mhTexture;
            SWR_TEXTUREVIEWINFO viewInfo;
            viewInfo.textureView = texInfo->mhTextureView;
            viewInfo.slot = curSlot;
            viewInfo.type = SHADER_PIXEL;
            SwrSetTextureView(ddPD.mhContext, viewInfo);

            SWR_SAMPLERINFO smpInfo = { 0 };
            smpInfo.sampler = texInfo->mhSampler;
            smpInfo.slot = curSlot;
            smpInfo.type = SHADER_PIXEL;
            SwrSetSampler(ddPD.mhContext, smpInfo);

            s.mTexMatrix[i] = s.mMatrices[OGL::TEXTURE_BASE + i].top();

            curSlot++;
        }
    }

    // unbind unused texture slots
    SWR_TEXTUREVIEWINFO viewInfo = { 0 };
    viewInfo.type = SHADER_PIXEL;

    SWR_SAMPLERINFO smpInfo = { 0 };
    smpInfo.type = SHADER_PIXEL;
    for (UINT i = curSlot; i < KNOB_NUMBER_OF_TEXTURE_VIEWS; ++i)
    {
        viewInfo.slot = i;
        SwrSetTextureView(ddPD.mhContext, viewInfo);

        smpInfo.slot = i;
        SwrSetSampler(ddPD.mhContext, smpInfo);
    }

    // Maintain invariants.
    s.mFrontSceneColor[0] = s.mFrontMaterial.mEmission[0] + s.mFrontMaterial.mAmbient[0] * s.mLightModel.mAmbient[0];
    s.mFrontSceneColor[1] = s.mFrontMaterial.mEmission[1] + s.mFrontMaterial.mAmbient[1] * s.mLightModel.mAmbient[1];
    s.mFrontSceneColor[2] = s.mFrontMaterial.mEmission[2] + s.mFrontMaterial.mAmbient[2] * s.mLightModel.mAmbient[2];
    s.mFrontSceneColor[3] = s.mFrontMaterial.mEmission[3] + s.mFrontMaterial.mAmbient[3] * s.mLightModel.mAmbient[3];

    s.mBackSceneColor[0] = s.mBackMaterial.mEmission[0] + s.mBackMaterial.mAmbient[0] * s.mLightModel.mAmbient[0];
    s.mBackSceneColor[1] = s.mBackMaterial.mEmission[1] + s.mBackMaterial.mAmbient[1] * s.mLightModel.mAmbient[1];
    s.mBackSceneColor[2] = s.mBackMaterial.mEmission[2] + s.mBackMaterial.mAmbient[2] * s.mLightModel.mAmbient[2];
    s.mBackSceneColor[3] = s.mBackMaterial.mEmission[3] + s.mBackMaterial.mAmbient[3] * s.mLightModel.mAmbient[3];

    MaintainMatrixInfoInvariant(s);

    s.mModelViewMatrix = s.mMatrices[OGL::MODELVIEW].top();
    auto MV = SWRL::mget3x3(s.mModelViewMatrix);
    auto MVinv = SWRL::minvert(MV);
    auto Norm = SWRL::minsert3x3(MVinv);
    auto Check = SWRL::mmult(Norm, s.mModelViewMatrix);
    s.mNormalMatrix = s.mModelViewMatrix; //Norm;
    s.mNormalScale = 1.0f;                // Should be: sqrt(||model-view^1_j||) for j == column 3

    auto _44 = SWRL::minvert(s.mModelViewMatrix);
    for (GLint i = 0; i < OGL::NUM_LIGHTS; ++i)
    {
        s.mLightSource[i].mOMPosition = SWRL::mvmult(_44, s.mLightSource[i].mPosition);
        __m128 omp = _mm_loadu_ps(&s.mLightSource[i].mOMPosition[0]);
        omp = _mm_div_ps(omp, _mm_sqrt_ps(_mm_dp_ps(omp, omp, 0xFF)));
        _mm_storeu_ps(&s.mLightSource[i].mOMPosition[0], omp);
    }

    // copy state into VS constant buffer
    GLfloat *pVSConst = (GLfloat *)DDLockBufferDiscard(hddPD, ddPD.mhVSConst);
    memcpy(pVSConst, static_cast<OGL::SaveableState *>(&s), sizeof(OGL::SaveableState));
    OGL::SaveableState *vsS = reinterpret_cast<OGL::SaveableState *>(pVSConst);

    SWRC_WORDCODE ibType = isIndexed ? SWRC_DISCONTIGUOUS_IB : SWRC_CONTIGUOUS_IB;
    SWR_TYPE indexType = (idxType == GL_UNSIGNED_INT) ? SWR_TYPE_UINT32 : SWR_TYPE_UINT16;

    // XXX: we need to allow multiple invocations of the compiler for L0 and L1 states.
    _simd_crcint L0, L1, L2;
    _simd_crcint seed = ddPD.mNumIEDs;
    for (GLuint i = 0; i < ddPD.mNumIEDs; ++i)
    {
        UINT *pIED = (UINT *)&ddPD.mIEDs[i];
        assert(sizeof(INPUT_ELEMENT_DESC) == 8);
        seed = _simd_crc32(seed, pIED[0]);
        seed = _simd_crc32(seed, pIED[1]);
        seed = _simd_crc32(seed, ddPD.mVBStrides[i]);
    }
    seed = _simd_crc32(seed, ibType);
    seed = _simd_crc32(seed, indexType);
    OGL::CacheState(*vsS, seed, L0, L1, L2);

    DDUnlockBuffer(hddPD, ddPD.mhVSConst);
    DDSetVsBuffer(hddPD, ddPD.mhVSConst);

    auto vsItr = ddPD.mPipeCache.find(L1);
    if (vsItr == ddPD.mPipeCache.end())
    {
        PipeInfo pipeInfo = { 0 };
        SWRFF_OPTIONS opts = { 0 };
        opts.deferVS = 0;
        opts.positionOnly = 0;
        opts.optLevel = 1;
        opts.colorIsZ = 0;
        opts.colorCodeFace = 0;
        auto ff = swrffGen(L1, ddPD.mhCompiler, ddPD.mhContext, s, opts);
        pipeInfo.name = L1;

        pipeInfo.FS = swrcCreateFetchShader(ddPD.mhCompiler, ddPD.mhContext, ddPD.mNumIEDs, ddPD.mIEDs, ddPD.mVBStrides, ibType, indexType);
        pipeInfo.VS = ff.pfnVS;
        pipeInfo.PS = ChoosePixelShader(s, curSlot);
        pipeInfo.backLinkageMask = ff.backLinkageMask;
        pipeInfo.frontLinkageMask = ff.frontLinkageMask;
        vsItr = ddPD.mPipeCache.insert(std::make_pair(L1, pipeInfo)).first;
    }

    SwrSetFetchFunc(ddPD.mhContext, vsItr->second.FS);
    SwrSetVertexFunc(ddPD.mhContext, vsItr->second.VS);
    SwrSetPixelFunc(ddPD.mhContext, vsItr->second.PS);
    SwrSetLinkageMaskFrontFace(ddPD.mhContext, vsItr->second.frontLinkageMask);
    SwrSetLinkageMaskBackFace(ddPD.mhContext, vsItr->second.backLinkageMask);
}

// @todo maybe these should match
PRIMITIVE_TOPOLOGY GLTopoToSWRTopo(GLenum topology)
{
    switch (topology)
    {
    case GL_POINTS:
        return TOP_POINTS;
    case GL_TRIANGLES:
        return TOP_TRIANGLE_LIST;
    case GL_TRIANGLE_STRIP:
        return TOP_TRIANGLE_STRIP;
    case GL_TRIANGLE_FAN:
        return TOP_TRIANGLE_FAN;
    case GL_QUADS:
        return TOP_QUAD_LIST;
    case GL_QUAD_STRIP:
        return TOP_QUAD_STRIP;
    case GL_LINES:
        return TOP_LINE_LIST;
    case GL_LINE_STRIP:
        return TOP_LINE_STRIP;
    case GL_POLYGON:
        return TOP_TRIANGLE_FAN;
    default:
        assert(0);
    }

    return TOP_TRIANGLE_LIST;
}

SWR_TYPE GLTypeToSWRType(GLenum type)
{
    switch (type)
    {
    case GL_UNSIGNED_INT:
        return SWR_TYPE_UINT32;
    case GL_UNSIGNED_SHORT:
        return SWR_TYPE_UINT16;
    default:
        assert(0 && "Unsupported type");
    }

    return SWR_TYPE_UINT32;
}

void DDDraw(DDHANDLE hddPD, GLenum topology, GLuint startVertex, GLuint numVertices)
{
    DDPrivateData &ddPD = *reinterpret_cast<DDPrivateData *>(hddPD);

    if (topology == GL_LINE_LOOP)
        return;

    PRIMITIVE_TOPOLOGY swrTopo = GLTopoToSWRTopo(topology);
    if (ddPD.mhNIB8 == 0)
    {
        SwrDraw(ddPD.mhContext, swrTopo, startVertex, NumElementsGivenIndices(swrTopo, numVertices));
    }
    else
    {
        SwrDraw(ddPD.mhContext, swrTopo, startVertex, NumElementsGivenIndices(swrTopo, numVertices));
    }
}

void DDDrawIndexed(DDHANDLE hddPD, GLenum topology, GLenum type, GLuint numVertices, GLuint indexOffset)
{
    DDPrivateData &ddPD = *reinterpret_cast<DDPrivateData *>(hddPD);

    PRIMITIVE_TOPOLOGY swrTopo = GLTopoToSWRTopo(topology);
    SWR_TYPE swrType = GLTypeToSWRType(type);
    SwrDrawIndexed(ddPD.mhContext, swrTopo, swrType, numVertices, indexOffset);
}

DDHTEXTURE DDCreateTexture(DDHANDLE hddPD, GLuint (&size)[3])
{
    DDPrivateData &ddPD = *reinterpret_cast<DDPrivateData *>(hddPD);

    SWR_CREATETEXTURE ct = { 0 };
    ct.eltSizeInBytes = 4 * sizeof(GLfloat);
    ct.width = size[0];
    ct.height = size[1];
    ct.planes = size[2];
    ct.mipLevels = 1000;
    ct.lockFlags = LOCK_NONE;

    DDTextureInfo *pTxI = new DDTextureInfo();

    pTxI->mhTexture = SwrCreateTexture(ddPD.mhContext, ct);

    SWR_TEXTUREVIEW TxVwArgs = { pTxI->mhTexture, RGBA32_FLOAT, 0, 1000 }; // BTW: this "1000" isn't arbitrary. It is set by the OGL1 spec.
    pTxI->mhTextureView = SwrCreateTextureView(ddPD.mhContext, TxVwArgs);

    SWR_CREATESAMPLER SmpArgs = { SWR_AM_CLAMP, AS_2D, TF_Linear, { 0, 0, 0, 0 } };
    pTxI->mhSampler = SwrCreateSampler(ddPD.mhContext, SmpArgs);

    return pTxI;
}

void DDLockTexture(DDHANDLE hddPD, DDHTEXTURE hTex)
{
    DDPrivateData &ddPD = *reinterpret_cast<DDPrivateData *>(hddPD);

    DDTextureInfo *pTex = reinterpret_cast<DDTextureInfo *>(hTex);
    SwrLockTexture(ddPD.mhContext, pTex->mhTexture, LOCK_NONE);
}

void DDUnlockTexture(DDHANDLE hddPD, DDHTEXTURE hTex)
{
    DDPrivateData &ddPD = *reinterpret_cast<DDPrivateData *>(hddPD);

    SwrUnlockTexture(ddPD.mhContext, reinterpret_cast<DDTextureInfo *>(hTex)->mhTexture);
}

GLuint DDGetSubtextureIndex(DDHANDLE hddPD, DDHTEXTURE hTex, GLuint plane, GLuint mipLevel)
{
    DDPrivateData &ddPD = *reinterpret_cast<DDPrivateData *>(hddPD);

    SWR_GETSUBTEXTUREINFO GSTI = { LOCK_DONTLOCK, 0 };
    GSTI.plane = plane;
    GSTI.mipLevel = mipLevel;

    SwrGetSubtextureInfo(reinterpret_cast<DDTextureInfo *>(hTex)->mhTexture, GSTI);

    return GSTI.subtextureIndex;
}

void DDUpdateSubtexture(DDHANDLE hddPD, DDHTEXTURE hTex, GLuint subtexIdx, GLenum format, GLenum type, GLuint xoffset, GLuint yoffset, GLuint width, GLuint height, const GLvoid *pvData)
{
    DDPrivateData &ddPD = *reinterpret_cast<DDPrivateData *>(hddPD);

    SWR_GETSUBTEXTUREINFO GSTI = { LOCK_NONE, 0 };
    GSTI.plane = 0;
    GSTI.mipLevel = 0;
    GSTI.subtextureIndex = subtexIdx;

    SwrGetSubtextureInfo(reinterpret_cast<DDTextureInfo *>(hTex)->mhTexture, GSTI);

    int numComps = 0;
    switch (format)
    {
    case GL_RGBA:
    case GL_BGRA:
        numComps = 4;
        break;
    case GL_RGB:
    case GL_BGR:
        numComps = 3;
        break;
    case GL_ALPHA:
        numComps = 1;
        break;
    default:
        assert(0 && "unsupported format");
    }

    SWR_FORMAT swrFormat = GLToSWRFormat(numComps, format, type, true);

    int Bpp = GetFormatInfo(swrFormat).Bpp;

    const BYTE *pData = (const BYTE *)pvData;

    // get pointer to texture data including any offsets
    float *outData = (float *)GSTI.pData + yoffset * GSTI.physWidth * 4 + xoffset * 4;

    for (UINT y = 0; y < height; ++y, outData += GSTI.physWidth * 4)
    {
        float *pRow = outData;
        for (UINT x = 0; x < width; ++x, pRow += 4, pData += Bpp)
        {
            ConvertPixel(swrFormat, (void *)pData, RGBA32_FLOAT, pRow);
        }
    }
}

void DDDestroyTexture(DDHANDLE hddPD, DDHTEXTURE hTex)
{
    DDPrivateData &ddPD = *reinterpret_cast<DDPrivateData *>(hddPD);

    SwrDestroyTexture(ddPD.mhContext, reinterpret_cast<DDTextureInfo *>(hTex)->mhTexture);
    SwrDestroyTextureView(ddPD.mhContext, reinterpret_cast<DDTextureInfo *>(hTex)->mhTextureView);
    SwrDestroySampler(ddPD.mhContext, reinterpret_cast<DDTextureInfo *>(hTex)->mhSampler);

    delete reinterpret_cast<DDTextureInfo *>(hTex);
}

DDHANDLE DDNewDraw(DDHANDLE hddPD)
{
    DDPrivateData &ddPD = *reinterpret_cast<DDPrivateData *>(hddPD);

    ddPD.mCurrentNumaNode = (ddPD.mCurrentNumaNode + 1) % ddPD.mNumNumaNodes;

    return (DDHANDLE)(size_t) ddPD.mCurrentNumaNode;
}

void DDVBLayoutInfo(DDHANDLE hddPD, GLuint &permuteWidth)
{
    DDPrivateData &ddPD = *reinterpret_cast<DDPrivateData *>(hddPD);

    permuteWidth = SwrGetPermuteWidth(ddPD.mhContext);
}

void DDCopyRenderTarget(DDHANDLE hddPD, const OGL::State &s, DDHTEXTURE hTex, GLvoid *pixels, GLenum format, GLenum type, GLint srcX, GLint srcY, GLint dstX, GLint dstY, GLuint width, GLuint height, GLboolean mirrorY)
{
    DDPrivateData &ddPD = *reinterpret_cast<DDPrivateData *>(hddPD);

    SWR_RENDERTARGET_ATTACHMENT rt = (format == GL_DEPTH_COMPONENT ? SWR_ATTACHMENT_DEPTH : SWR_ATTACHMENT_COLOR0);

    if (hTex)
    {
        DDTextureInfo *pTxI = (DDTextureInfo *)hTex;

        SwrCopyRenderTarget(ddPD.mhContext, rt, pTxI->mhTexture, NULL, RGBA32_FLOAT, 0, srcX, srcY, dstX, dstY, width, height);
    }
    else
    {
        assert(pixels);
        UINT numComps;

        switch (format)
        {
        case GL_RGB:
        case GL_BGR:
            numComps = 3;
            break;
        case GL_RGBA:
        case GL_BGRA:
            numComps = 4;
            break;
        case GL_DEPTH_COMPONENT:
        case GL_STENCIL_INDEX:
            numComps = 1;
            break;
        default:
            assert(0 && "Unsupported format");
        }

        SWR_FORMAT swrFormat = GLToSWRFormat(numComps, format, type, true);
        const SWR_FORMAT_INFO &info = GetFormatInfo(swrFormat);
        // Use PixelStore packed row length if set, otherwise use the readPixels width.
        INT pitch = (s.mPixelStore.mPackRowLength ? s.mPixelStore.mPackRowLength : width) * info.Bpp;
        if (mirrorY)
        {
            pitch = -pitch;
        }

        SwrCopyRenderTarget(ddPD.mhContext, rt, NULL, pixels, swrFormat, pitch, srcX, srcY, dstX, dstY, width, height);
    }
}

void DDSetRenderTarget(DDHANDLE hddPD, DDHANDLE rt, DDHANDLE depth)
{
    DDPrivateData &ddPD = *reinterpret_cast<DDPrivateData *>(hddPD);
    SwrSetRenderTargets(ddPD.mhContext, rt, depth);
}

unsigned DDGetThreadCount(DDHANDLE hddPD)
{

    DDPrivateData &ddPD = *reinterpret_cast<DDPrivateData *>(hddPD);
    return SwrGetNumWorkerThreads(ddPD.mhContext);
}

bool DDInitProcTable(DDProcTable &procTable)
{
    procTable.pfnCreateContext = &DDCreateContext;
    procTable.pfnBindContext = &DDBindContext;
    procTable.pfnDestroyContext = &DDDestroyContext;

    procTable.pfnNewDraw = &DDNewDraw;

    procTable.pfnPresent = &DDPresent;
    procTable.pfnPresent2 = &DDPresent2;
    procTable.pfnSwapBuffer = &DDSwapBuffer;
    procTable.pfnSetViewport = &DDSetViewport;
    procTable.pfnSetCullMode = &DDSetCullMode;
    procTable.pfnClear = &DDClear;
    procTable.pfnSetScissorRect = &DDSetScissorRect;
    procTable.pfnSetupVertices = &DDSetupVertices;
    procTable.pfnSetupSparseVertices = &DDSetupSparseVertices;
    procTable.pfnVBLayoutInfo = &DDVBLayoutInfo;

    procTable.pfnCreateRenderTarget = &DDCreateRenderTarget;
    procTable.pfnDestroyRenderTarget = &DDDestroyRenderTarget;
    procTable.pfnCreateBuffer = &DDCreateBuffer;
    procTable.pfnLockBuffer = &DDLockBuffer;
    procTable.pfnLockBufferNoOverwrite = &DDLockBufferNoOverwrite;
    procTable.pfnLockBufferDiscard = &DDLockBufferDiscard;
    procTable.pfnUnlockBuffer = &DDUnlockBuffer;
    procTable.pfnDestroyBuffer = &DDDestroyBuffer;
    procTable.pfnSetRenderTarget = &DDSetRenderTarget;

    procTable.pfnCreateTexture = &DDCreateTexture;
    procTable.pfnLockTexture = &DDLockTexture;
    procTable.pfnUnlockTexture = &DDUnlockTexture;
    procTable.pfnGetSubtextureIndex = &DDGetSubtextureIndex;
    procTable.pfnUpdateSubtexture = &DDUpdateSubtexture;
    procTable.pfnDestroyTexture = &DDDestroyTexture;

    procTable.pfnSetPsBuffer = &DDSetPsBuffer;
    procTable.pfnSetVsBuffer = &DDSetVsBuffer;

    procTable.pfnSetIndexBuffer = &DDSetIndexBuffer;

    procTable.pfnGenShaders = &DDGenShaders;

    procTable.pfnDraw = &DDDraw;
    procTable.pfnDrawIndexed = &DDDrawIndexed;

    procTable.pfnCopyRenderTarget = &DDCopyRenderTarget;

    procTable.pfnGetThreadCount = &DDGetThreadCount;
    return true;
}
