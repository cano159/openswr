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

#include "gldd.h"
#include "glim.hpp"
#include "glst.hpp"
#include "ogldisplaylist.hpp"
#include "oglglobals.h"
#include "oglstate.hpp"
#include "rdtsc.h"

#include <array>
#include <cassert>
#include <cmath>
#include <climits>

namespace OGL
{

__m128 _glimAddArrayElementSWR(ArrayPointerParameters const &app, GLint index)
{
    // Somehow, the GL1 spec managed to make the fast path SLOWER.
    OSALIGN(GLfloat, 16) attribute[4] = { 0, 0, 0, 1 };

    for (int i = 0; i < app.mNumCoordsPerAttribute; ++i)
    {
        switch (app.mType)
        {
        case GL_UNSIGNED_BYTE:
            attribute[i] = (GLfloat)((GLubyte *)((GLubyte *)app.mpElements + app.mStride * index))[i] / 255.0f;
            break;
        case GL_BYTE:
            attribute[i] = (GLfloat)((GLbyte *)((GLubyte *)app.mpElements + app.mStride * index))[i] / 255.0f;
            break;
        case GL_SHORT:
            attribute[i] = (GLfloat)((GLshort *)((GLubyte *)app.mpElements + app.mStride * index))[i];
            break;
        case GL_INT:
            attribute[i] = (GLfloat)((GLint *)((GLubyte *)app.mpElements + app.mStride * index))[i];
            break;
        case GL_FLOAT:
            attribute[i] = (GLfloat)((GLfloat *)((GLubyte *)app.mpElements + app.mStride * index))[i];
            break;
        case GL_DOUBLE:
            attribute[i] = (GLfloat)((GLdouble *)((GLubyte *)app.mpElements + app.mStride * index))[i];
            break;
        default:
            assert(0);
        }
    }

    return _mm_load_ps(&attribute[0]);
}

GLuint _glimFormatToNumComponents(unsigned long long fmt)
{
    static const GLuint numComponents[] =
        {
          1, 2, 3, 4,
          1, 2, 3, 4,
        };
    return numComponents[(GLuint)fmt];
}

AttributeFormat _glimNumComponentsToFormat(GLuint nc)
{
    static const AttributeFormat formats[] =
        {
          OGL_R32_FLOAT,
          OGL_RG32_FLOAT,
          OGL_RGB32_FLOAT,
          OGL_RGBA32_FLOAT,
        };
    return formats[nc - 1];
}

void _glimPacketStore(GLfloat *stream, GLfloat x, GLfloat y, GLfloat z, GLfloat w, GLuint permWidth, GLuint aosPacket, GLuint aosIndex, GLuint numCom)
{
    const GLuint packetWidth = numCom * permWidth;
    const GLuint packetOffset = packetWidth * aosPacket;
    GLfloat *pPacket = stream + packetOffset;
    switch (numCom)
    {
    case 4:
        (pPacket + 3 * permWidth)[aosIndex] = w;
    case 3:
        (pPacket + 2 * permWidth)[aosIndex] = z;
    case 2:
        (pPacket + 1 * permWidth)[aosIndex] = y;
    case 1:
        (pPacket + 0 * permWidth)[aosIndex] = x;
    }
}

void _glimUpdateRedundancyKey(SWRL::UncheckedFixedVector<GLfloat, 64> &key, GLfloat x, GLfloat y, GLfloat z, GLfloat w, GLuint numCom)
{
    switch (numCom)
    {
    case 4:
        key[key.size() + 3] = w;
    case 3:
        key[key.size() + 2] = z;
    case 2:
        key[key.size() + 1] = y;
    case 1:
        key[key.size() + 0] = x;
    }
    key.resize(key.size() + numCom);
}

void _glimUpdateRedundancyMap(State &s, SWRL::UncheckedFixedVector<GLfloat, 64> const &key, GLuint currentIndex, IndexBufferKind IBK)
{
    // XXX: Too slow.
    switch (IBK)
    {
    case OGL_GENERATE_INDEX_BUFFER:
        break;
    case OGL_GENERATE_NEGATIVE_INDEX_BUFFER_8:
    {
        GLubyte *pNIB8 = GetVB(s).mpNIB8;
        GLuint oldIndex = s.mRedundancyMap.GetEarliestIndex(key, currentIndex);
        GLubyte relOffset = currentIndex - oldIndex;
        pNIB8[currentIndex] = relOffset;
    }
    break;
    default:
        break;
    }
}

void _glimSetupVertexLayoutSWR(State &s, VBIEDLayout &L)
{
    // Builds an array that gives an index into the array of buffers
    // for each attribute or side-band attribute.

    memset(&L, 0xFFFFFFFF, sizeof(L));
    GLuint iedx = 0;
    GLuint sbiedx = 0;

    auto &vb = GetVB(s);

    // Add the vertex.
    L.position.vertex = iedx;
    ++iedx;

    if (vb.mAttributes.normal)
    {
        L.position.normal = iedx;
        L.sideBandPosition.normal = sbiedx;
        ++iedx;
        ++sbiedx;
    }

    if (vb.mAttributes.fog)
    {
        L.position.fog = iedx;
        ++iedx;
    }

    if (vb.mAttributes.color != 0)
    {
        for (int i = 0; i < NUM_COLORS; ++i)
        {
            if ((vb.mAttributes.color & (0x1ULL << i)) != 0)
            {
                L.position.color[i] = iedx;
                ++iedx;
            }
        }
    }

    if (vb.mAttributes.texCoord != 0)
    {
        for (int i = 0; i < NUM_TEXTURES; ++i)
        {
            if ((vb.mAttributes.texCoord & (0x1ULL << i)) != 0)
            {
                L.position.texCoord[i] = iedx;
                ++iedx;
            }
        }
    }
}

void glimActiveTexture(State &s, GLenum which)
{
    glstActiveTexture(s, which);
}

void glimAddVertexSWR(State &s, GLdouble x, GLdouble y, GLdouble z, GLdouble w, GLint index, GLboolean permutedVBs, GLenum genIBKind)
{
    // XXX: use the VBIEDLayout rather than computing iedx/sbiedx on the fly, each time.
    SWRL::UncheckedFixedVector<GLfloat, 64> key;
    auto &vb = GetVB(s);
    GLuint numVerts = vb.mNumVertices;
    auto const &vAttrFmts = vb.mAttrFormats;
    GLuint iedx = 0;
    GLuint sbiedx = 0;
    __m128 attribute;
    const GLuint permWidth = s.mVBLayoutInfo.permuteWidth; // need an API to get the permute width
    GLuint aosPacket = numVerts >> s.mVBLayoutInfo.permuteShift;
    GLuint aosIndex = numVerts & s.mVBLayoutInfo.permuteMask;

#ifdef KNOB_TOSS_VERTICES
    return;
#endif

    // Add the vertex.
    if ((index >= 0) && s.mCaps.vertexArray && (s.mArrayPointers.mVertex.mpElements != 0))
    {
        attribute = _glimAddArrayElementSWR(s.mArrayPointers.mVertex, index);
    }
    else
    {
        attribute = _mm_set_ps((GLfloat)w, (GLfloat)z, (GLfloat)y, (GLfloat)x);
    }
    if (!permutedVBs)
    {
        _mm_storeu_ps(vb.mpfBuffers[iedx] + numVerts * _glimFormatToNumComponents(vAttrFmts.position), attribute);
    }
    else
    {
        _glimPacketStore(vb.mpfBuffers[iedx], (GLfloat)x, (GLfloat)y, (GLfloat)z, (GLfloat)w, permWidth, aosPacket, aosIndex, _glimFormatToNumComponents(vAttrFmts.position));
    }
    _glimUpdateRedundancyKey(key, (GLfloat)x, (GLfloat)y, (GLfloat)z, (GLfloat)w, _glimFormatToNumComponents(vAttrFmts.position));
    ++iedx;

    if (vb.mAttributes.normal)
    {
        if ((index >= 0) && s.mCaps.normalArray && (s.mArrayPointers.mNormal.mpElements != 0))
        {
            attribute = _glimAddArrayElementSWR(s.mArrayPointers.mNormal, index);
        }
        else
        {
            attribute = _mm_set_ps(s.mNormal[3], s.mNormal[2], s.mNormal[1], s.mNormal[0]);
        }
        __m128 nattr = s.mCaps.normalize ? _mm_mul_ps(attribute, _mm_rsqrt_ps(_mm_dp_ps(attribute, attribute, 0xFF))) : attribute;
        if (!permutedVBs)
        {
            _mm_storeu_ps(vb.mpfBuffers[iedx] + numVerts * _glimFormatToNumComponents(vAttrFmts.normal), attribute);
            _mm_storeu_ps(vb.mpfSideBuffers[sbiedx] + numVerts * _glimFormatToNumComponents(vAttrFmts.normal), nattr);
        }
        else
        {
            OSALIGNLINE(float)fnattr[4];
            _mm_store_ps(&fnattr[0], nattr);
            _glimPacketStore(vb.mpfBuffers[iedx], s.mNormal[0], s.mNormal[1], s.mNormal[2], 0.0f, permWidth, aosPacket, aosIndex, _glimFormatToNumComponents(vAttrFmts.normal));
            _glimPacketStore(vb.mpfSideBuffers[sbiedx], fnattr[0], fnattr[1], fnattr[2], 0.0f, permWidth, aosPacket, aosIndex, _glimFormatToNumComponents(vAttrFmts.normal));
        }
        _glimUpdateRedundancyKey(key, s.mNormal[0], s.mNormal[1], s.mNormal[2], 0.0f, _glimFormatToNumComponents(vAttrFmts.normal));
        ++iedx;
        ++sbiedx;
    }

    // XXX: these attributes need to be updated to store a minimal width. Otherwise they are INCORRECT.
    if (vb.mAttributes.fog)
    {
        attribute = _mm_load_ps(&s.mFog.mColor[0]);
        _mm_storeu_ps(vb.mpfBuffers[iedx] + numVerts * _glimFormatToNumComponents(vAttrFmts.fog), attribute);
        _glimUpdateRedundancyKey(key, s.mFog.mColor[0], s.mFog.mColor[1], s.mFog.mColor[2], s.mFog.mColor[3], _glimFormatToNumComponents(vAttrFmts.fog));
        ++iedx;
    }

    if (vb.mAttributes.color != 0)
    {
        for (int i = 0; i < NUM_COLORS; ++i)
        {
            if ((vb.mAttributes.color & (0x1ULL << i)) != 0)
            {
                if ((index >= 0) && ((s.mCaps.colorArray && (0x1 << i)) != 0) && (s.mArrayPointers.mColor[i].mpElements != 0))
                {
                    attribute = _glimAddArrayElementSWR(s.mArrayPointers.mColor[i], index);
                }
                else
                {
                    attribute = _mm_set_ps(s.mColor[i][3], s.mColor[i][2], s.mColor[i][1], s.mColor[i][0]);
                }
                unsigned long long fmt = (vAttrFmts.color >> (AttrFmtShift * i)) & AttrFmtMask;
                _mm_storeu_ps(vb.mpfBuffers[iedx] + numVerts * _glimFormatToNumComponents(fmt), attribute);
                _glimUpdateRedundancyKey(key, s.mColor[i][0], s.mColor[i][1], s.mColor[i][2], s.mColor[i][3], _glimFormatToNumComponents(fmt));
                ++iedx;
            }
        }
    }

    if (vb.mAttributes.texCoord != 0)
    {
        for (int i = 0; i < NUM_TEXTURES; ++i)
        {
            if ((vb.mAttributes.texCoord & (0x1ULL << i)) != 0)
            {
                if ((index >= 0) && ((s.mCaps.texCoordArray && (0x1 << i)) != 0) && (s.mArrayPointers.mTexCoord[i].mpElements != 0))
                {
                    attribute = _glimAddArrayElementSWR(s.mArrayPointers.mTexCoord[i], index);
                }
                else
                {
                    attribute = _mm_set_ps(s.mTexCoord[i][3], s.mTexCoord[i][2], s.mTexCoord[i][1], s.mTexCoord[i][0]);
                }
                unsigned long long fmt = (vAttrFmts.texCoord >> (AttrFmtShift * i)) & AttrFmtMask;
                _mm_storeu_ps(vb.mpfBuffers[iedx] + numVerts * _glimFormatToNumComponents(fmt), attribute);
                _glimUpdateRedundancyKey(key, s.mTexCoord[i][0], s.mTexCoord[i][1], s.mTexCoord[i][2], s.mTexCoord[i][3], _glimFormatToNumComponents(fmt));
                ++iedx;
            }
        }
    }

    _glimUpdateRedundancyMap(s, key, numVerts, (OGL::IndexBufferKind)genIBKind);

    ++GetVB(s).mNumVertices;
}

void glimAlphaFunc(State &s, GLenum func, GLclampf ref)
{
    glstAlphaFunc(s, func, ref);
}

void glimArrayElement(State &s, GLint index)
{
    // If the context is undefined, exit early.
    if (s.mNoExecute)
    {
        return;
    }

    if (GetVB(s).mNumVertices > VERTEX_BUFFER_COUNT)
    {
        s.mLastError = GL_OUT_OF_MEMORY;
        return;
    }

    if (index < 0)
    {
        s.mLastError = GL_INVALID_OPERATION;
        return;
    }

    // We have to flush the buffers --- they're full.
    if (GetVB(s).mNumVertices == VERTEX_BUFFER_COUNT)
    {
        glimDrawSWR(s);

        // Now lock discard to get the buffers back.
        for (GLuint i = 0; i < GetVB(s).mNumAttributes; ++i)
        {
            GetVB(s).mpfBuffers[i] = (GLfloat *)GetDDProcTable().pfnLockBufferDiscard(GetDDHandle(), GetVB(s).mhBuffers[i]);
            assert(GetVB(s).mpfBuffers[i]);
        }
        for (GLuint i = 0; i < GetVB(s).mNumSideAttributes; ++i)
        {
            GetVB(s).mpfSideBuffers[i] = (GLfloat *)GetDDProcTable().pfnLockBufferDiscard(GetDDHandle(), GetVB(s).mhSideBuffers[i]);
            assert(GetVB(s).mpfSideBuffers[i]);
        }
        GetVB(s).mNumVertices = 0;
    }

    glimAddVertexSWR(s, 0, 0, 0, 0, index, false, (GLenum)OGL_NO_INDEX_BUFFER_GENERATION);
}

void glimBegin(State &s, GLenum topology)
{
    // If the context is undfined, exit early.
    if (s.mNoExecute)
    {
        return;
    }

    auto &vb = GetVB(s);

    // Reset the attributes seen. We use this to determine the -actual- maximum width
    // of the attributes called between begin/end. For optimized DLs this will let us
    // minimize the size of the VB.
    InitializeVertexAttrFmts(vb.mAttrFormatsSeen);

    glimSetTopologySWR(s, topology);

    // If previously used, lock discard to reset the buffers we need to use.
    if (vb.mNumVertices > 0)
    {
        for (GLuint i = 0; i < vb.mNumAttributes; ++i)
        {
            vb.mpfBuffers[i] = (GLfloat *)GetDDProcTable().pfnLockBufferDiscard(GetDDHandle(), vb.mhBuffers[i]);
        }
        for (GLuint i = 0; i < vb.mNumSideAttributes; ++i)
        {
            vb.mpfSideBuffers[i] = (GLfloat *)GetDDProcTable().pfnLockBufferDiscard(GetDDHandle(), vb.mhSideBuffers[i]);
        }
        vb.mNumVertices = 0;
    }

    glimSetUpDrawSWR(s, GL_NONE);
}

void glimBindTexture(State &s, GLenum target, GLuint texture)
{
    glstBindTexture(s, target, texture);
}

void glimBlendFunc(State &s, GLenum sfactor, GLenum dfactor)
{
    glstBlendFunc(s, sfactor, dfactor);
}

void glimCallList(State &s, GLuint list)
{
    // Check if we can use a fast display list,
    // or if we need to compile a fast DL.
    if (s.mExecutingDL.size() >= OGL::MAX_DL_STACK_DEPTH)
    {
        return;
    }

    OGL::ExecuteDL(s, list);
}

void glimCallLists(State &s, GLsizei n, GLenum type, const GLvoid *lists)
{

    for (int i = 0; i < n; ++i)
    {
        GLuint list;
        switch (type)
        {
        case GL_BYTE:
            list = ((GLbyte *)lists)[i];
            break;
        case GL_UNSIGNED_BYTE:
            list = ((GLubyte *)lists)[i];
            break;
        case GL_SHORT:
            list = ((GLshort *)lists)[i];
            break;
        case GL_UNSIGNED_SHORT:
            list = ((GLushort *)lists)[i];
            break;
        case GL_INT:
            list = ((GLint *)lists)[i];
            break;
        case GL_UNSIGNED_INT:
            list = ((GLuint *)lists)[i];
            break;
        case GL_FLOAT:
            list = (GLint)((GLfloat *)lists)[i];
            break;
        default:
            assert(0 && "Unimplemented display list type");
        }

        glimCallList(s, list);
    }
}

void glimClear(State &s, GLbitfield mask)
{
    // If the context is undefined, exit early.
    if (s.mNoExecute)
    {
        return;
    }

    // check depth mask
    if (!s.mDepthMask)
    {
        mask &= ~GL_DEPTH_BUFFER_BIT;
    }

    // TODO handle color mask

    glimSetClearMaskSWR(s, mask);

    if (mask & GL_ACCUM_BUFFER_BIT)
    {
        // XXX: clear accum.
    }

    if (mask & GL_STENCIL_BUFFER_BIT)
    {
        // XXX: clear stencil.
    }

    GetDDProcTable().pfnSetScissorRect(GetDDHandle(), s.mScissor.x, s.mScissor.y, s.mScissor.width, s.mScissor.height);
    GetDDProcTable().pfnSetViewport(GetDDHandle(), s.mViewport.x, s.mViewport.y, s.mViewport.width, s.mViewport.height, s.mViewport.zNear, s.mViewport.zFar, s.mCaps.scissorTest);
    FLOAT color[4] = { s.mClearColor[0], s.mClearColor[1], s.mClearColor[2], s.mClearColor[3] };
    GetDDProcTable().pfnClear(GetDDHandle(), s.mClearMask, color, (FLOAT)s.mClearDepth, s.mCaps.scissorTest);
}

void glimClearColor(State &s, GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
    glstClearColor(s, red, green, blue, alpha);
}

void glimClearDepth(State &s, GLclampd depth)
{
    glstClearDepth(s, depth);
}

void glimClientActiveTexture(State &s, GLenum texture)
{
    glstClientActiveTexture(s, texture);
}

void glimColor(State &s, GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
    glstColor(s, red, green, blue, alpha);
}

void glimColorMask(State &s, GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
{
    glstColorMask(s, red, green, blue, alpha);
}

void glimColorPointer(State &s, GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
    glstColorPointer(s, size, type, stride, pointer);
}

void glimCopyTexSubImage2D(State &s, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
    assert(target == GL_TEXTURE_2D);

    TextureUnit &texUnit = s.mTexUnit[s.mActiveTexture - GL_TEXTURE0];
    TexParameters &texParams = s.mTexParameters[texUnit.mNamedTexture2d];

    GetDDProcTable().pfnCopyRenderTarget(GetDDHandle(), s, texParams.mhTexture, NULL, 0, 0, x, y, xoffset, yoffset, width, height, false);
}

void glimCullFace(State &s, GLenum cullFace)
{
    glstCullFace(s, cullFace);
}

void glimDeleteTextures(State &s, GLsizei n, const GLuint *textures)
{
    for (GLsizei i = 0; i < n; ++i)
    {
        std::unordered_map<GLuint, TexParameters>::iterator it = s.mTexParameters.find(textures[i]);
        if (it != s.mTexParameters.end())
        {
            TexParameters &texParams = (*it).second;
            //If handle is null, then texture wasn't fully created (ex, PROXY).
            if (texParams.mhTexture)
            {
                GetDDProcTable().pfnDestroyTexture(GetDDHandle(), texParams.mhTexture);
            }
            s.mTexParameters.erase(textures[i]);
        }
    }
}

void glimDeleteLists(State &s, GLuint list, GLsizei range)
{
    glstDeleteLists(s, list, range);
}

void glimDepthFunc(State &s, GLenum func)
{
    glstDepthFunc(s, func);
}

void glimDepthMask(State &s, GLboolean flag)
{
    glstDepthMask(s, flag);
}

void glimDepthRange(State &s, GLclampd zNear, GLclampd zFar)
{
    glstDepthRange(s, zNear, zFar);
}

void glimDisable(State &s, GLenum cap)
{
    if (s.mNoExecute)
    {
        return;
    }

    glstDisable(s, cap);
}

void glimDisableClientState(State &s, GLenum cap)
{
    glimDisable(s, cap);
}

void glimDrawSWR(State &s)
{
    // If the context is undefined, then exit early.
    if (s.mNoExecute)
    {
        return;
    }

    if (GetVB(s).mNumVertices == 0)
    {
        return; // Fastest draw in the west.
    }

#if 1 //XXX this hacks in support for PolygonMode (GL_FILL is the default).
    // Lines aren't completely right and culling isn't handled correctly.  But it's not as broken as it was.
    switch (s.mPolygonMode[0])
    {
    case GL_POINT:
        s.mTopology = GL_POINTS;
        break;
    case GL_LINE:
        s.mTopology = GL_LINE_STRIP;
        break;
    default:
        break;
    }
#endif
    GetDDProcTable().pfnDraw(GetDDHandle(), s.mTopology, 0, GetVB(s).mNumVertices);
}

GLboolean _isActiveVBO(State &s, GLuint index)
{
    switch (index)
    {
    case 0:
        return s.mActiveVBOs.normal != 0;
        break;
    case 1:
        return s.mActiveVBOs.fog != 0;
        break;
    case 2:
        return s.mActiveVBOs.color[0] != 0;
        break;
    case 3:
        return s.mActiveVBOs.color[1] != 0;
        break;
    case 4:
        return s.mActiveVBOs.texcoord[0] != 0;
        break;
    case 5:
        return s.mActiveVBOs.texcoord[1] != 0;
        break;
    case 6:
        return s.mActiveVBOs.texcoord[2] != 0;
        break;
    case 7:
        return s.mActiveVBOs.texcoord[3] != 0;
        break;
    case 8:
        return s.mActiveVBOs.texcoord[4] != 0;
        break;
    case 9:
        return s.mActiveVBOs.texcoord[5] != 0;
        break;
    case 10:
        return s.mActiveVBOs.texcoord[6] != 0;
        break;
    case 11:
        return s.mActiveVBOs.texcoord[7] != 0;
        break;
    default:
        assert(0 && "Invalid array");
    }
    return GL_FALSE;
}

HANDLE _getActiveVBO(State &s, GLuint index)
{
    GLuint boundBuffer;
    switch (index)
    {
    case 0:
        boundBuffer = s.mActiveVBOs.normal;
        break;
    case 1:
        boundBuffer = s.mActiveVBOs.fog;
        break;
    case 2:
        boundBuffer = s.mActiveVBOs.color[0];
        break;
    case 3:
        boundBuffer = s.mActiveVBOs.color[1];
        break;
    case 4:
        boundBuffer = s.mActiveVBOs.texcoord[0];
        break;
    case 5:
        boundBuffer = s.mActiveVBOs.texcoord[1];
        break;
    case 6:
        boundBuffer = s.mActiveVBOs.texcoord[2];
        break;
    case 7:
        boundBuffer = s.mActiveVBOs.texcoord[3];
        break;
    case 8:
        boundBuffer = s.mActiveVBOs.texcoord[4];
        break;
    case 9:
        boundBuffer = s.mActiveVBOs.texcoord[5];
        break;
    case 10:
        boundBuffer = s.mActiveVBOs.texcoord[6];
        break;
    case 11:
        boundBuffer = s.mActiveVBOs.texcoord[7];
        break;
    default:
        assert(0 && "Invalid array");
    }

    VertexBufferObject &vbo = s.mVBOs[boundBuffer];
    return vbo.mHWBuffer;
}

void glimDrawArrays(State &s, GLenum mode, GLint offset, GLsizei count)
{
    // If the context is undfined, exit early.
    if (s.mNoExecute)
    {
        return;
    }

    GetDDProcTable().pfnNewDraw(GetDDHandle());

    VertexBuffer &vb = GetVB(s);

    vb.mNumVertices = count;

    glimSetTopologySWR(s, mode);

    UINT idx = 0;
    DDHANDLE vBuffers[NUM_ATTRIBUTES];

    // vertex arrays can be locked or unlocked, active or unactive.  In addition, the
    // underlying attribute can be active or unactive.
    //
    // - if array is locked, assume active and pass locked buffer directly
    // - if array is not locked and active and attrib is active, copy the active buffer contents
    // - if array is not locked and active and attrib is not active, copy the underyling state value
    //   into the buffer

    // vertex buffers are sparse, and indexed by their attribute type, we pack them into a packed array before
    // sending to SWR
    DWORD index = 0;
    if (s.mArraysLocked & 1)
    {
        vBuffers[idx] = vb.mhBuffers[index];
    }
    else if (s.mActiveVBOs.vertex)
    {
        VertexBufferObject &vbo = s.mVBOs[s.mActiveVBOs.vertex];
        vBuffers[idx] = vbo.mHWBuffer;
    }
    else
    {
        int size = count * s.mArrayPointers.mVertex.mStride;
        vb.getBuffer(index, size);
        vb.mpfBuffers[index] = (GLfloat *)GetDDProcTable().pfnLockBufferDiscard(GetDDHandle(), vb.mhBuffers[index]);
        memcpy(vb.mpfBuffers[index], (GLvoid *)s.mArrayPointers.mVertex.mpElements, size);
        GetDDProcTable().pfnUnlockBuffer(GetDDHandle(), vb.mhBuffers[index]);
        vb.mpfBuffers[index] = NULL;
        vBuffers[idx] = vb.mhBuffers[index];
    }
    ++idx;

    // attributes next
    UINT64 lockedAttribs = s.mArraysLocked >> 1;
    // @todo mAttributes.mask should be 32 bit
    UINT activeAttribs = (UINT)vb.mAttributes.mask;
    UINT activeArrays = s.mCaps.attribArrayMask;

    while (_BitScanForward(&index, activeAttribs))
    {
        int vbIndex = index + 1;
        if (s.mArraysLocked & (1 << vbIndex))
        {
            vBuffers[idx] = vb.mhBuffers[vbIndex];
        }
        else if (_isActiveVBO(s, index))
        {
            vBuffers[idx] = _getActiveVBO(s, index);
        }
        else
        {
            // if attribute is active, but array is not, GenShaders will pull the attribute from the constant buffer instead
            if (activeArrays & (1 << index))
            {
                int size = count * s.mArrayPointers.mArrays[index].mStride;
                vb.getBuffer(vbIndex, size);
                vb.mpfBuffers[vbIndex] = (GLfloat *)GetDDProcTable().pfnLockBufferDiscard(GetDDHandle(), vb.mhBuffers[vbIndex]);
                memcpy(vb.mpfBuffers[vbIndex], (GLvoid *)s.mArrayPointers.mArrays[index].mpElements, size);
                GetDDProcTable().pfnUnlockBuffer(GetDDHandle(), vb.mhBuffers[vbIndex]);
                vb.mpfBuffers[vbIndex] = NULL;
                vBuffers[idx] = vb.mhBuffers[vbIndex];
            }
            else
            {
                vBuffers[idx] = NULL;
            }
        }
        ++idx;
        activeAttribs &= ~(1 << index);
    }

    if (s.mCaps.cullFace)
    {
        switch (s.mCullFace)
        {
        case GL_FRONT:
            GetDDProcTable().pfnSetCullMode(GetDDHandle(), s.mFrontFace);
            break;
        case GL_BACK:
            GetDDProcTable().pfnSetCullMode(GetDDHandle(), (s.mFrontFace == GL_CW) ? GL_CCW : GL_CW);
            break;
        default:
            s.mLastError = GL_INVALID_ENUM;
        }
    }
    else
    {
        GetDDProcTable().pfnSetCullMode(GetDDHandle(), GL_NONE);
    }

    GetDDProcTable().pfnSetScissorRect(GetDDHandle(), s.mScissor.x, s.mScissor.y, s.mScissor.width, s.mScissor.height);
    GetDDProcTable().pfnSetViewport(GetDDHandle(), s.mViewport.x, s.mViewport.y, s.mViewport.width, s.mViewport.height, s.mViewport.zNear, s.mViewport.zFar, s.mCaps.scissorTest);

    GetDDProcTable().pfnSetupSparseVertices(GetDDHandle(), s, vb.mAttributes, vb.mNumAttributes, &vBuffers[0], 0);

    // Tell the DD to generate shaders based on the current state.
    GetDDProcTable().pfnGenShaders(GetDDHandle(), s, false, GL_UNSIGNED_INT);

    // draw it!
    GetDDProcTable().pfnDraw(GetDDHandle(), mode, offset, count);
}

#if defined(WIN32)
#pragma warning(disable : 4244)
#endif

void glimDrawElements(State &s, GLenum mode, GLsizei count, GLenum type, const GLvoid *indices)
{
    // If the context is undfined, exit early.
    if (s.mNoExecute)
    {
        return;
    }

    GetDDProcTable().pfnNewDraw(GetDDHandle());

    // only support GL_TRIANGLES for now
    assert(mode == GL_TRIANGLES);

    VertexBuffer &vb = GetVB(s);

    glimSetTopologySWR(s, mode);

    UINT idx = 0;
    DDHANDLE vBuffers[NUM_ATTRIBUTES];

    idx = 0;

    HANDLE savedIdxBuffer = vb.mhIndexBuffer;
    UINT indexOffset = 0;

    if (s.mActiveElementVBO)
    {
        indexOffset = reinterpret_cast<uintptr_t>(indices);
        VertexBufferObject &vbo = s.mVBOs[s.mActiveElementVBO];
        vb.mhIndexBuffer = vbo.mHWBuffer;
        assert(vb.mhIndexBuffer != NULL);
    }
    else
    {
        // setup index buffer
        if (count > vb.mNumIndices)
        {
            GetDDProcTable().pfnDestroyBuffer(GetDDHandle(), vb.mhIndexBuffer);
            vb.mhIndexBuffer = GetDDProcTable().pfnCreateBuffer(GetDDHandle(), count * sizeof(UINT) + KNOB_VS_SIMD_WIDTH * sizeof(UINT), NULL);
            vb.mNumIndices = count;
            savedIdxBuffer = vb.mhIndexBuffer;
        }

        vb.mpIndexBuffer = (GLuint *)GetDDProcTable().pfnLockBufferDiscard(GetDDHandle(), vb.mhIndexBuffer);
        memset(vb.mpIndexBuffer, 0x0, count * sizeof(UINT) + KNOB_VS_SIMD_WIDTH * sizeof(UINT));

        // assume index buffer type is unsigned int or short
        assert(type == GL_UNSIGNED_INT || type == GL_UNSIGNED_SHORT);

        UINT size = type == GL_UNSIGNED_INT ? count * sizeof(UINT) : count * sizeof(short);
        memcpy(vb.mpIndexBuffer, indices, size);
        GetDDProcTable().pfnUnlockBuffer(GetDDHandle(), vb.mpIndexBuffer);
    }

    GetDDProcTable().pfnSetIndexBuffer(GetDDHandle(), vb.mhIndexBuffer);

    // 4 scenarios for the vertex arrays:
    // 1- array is locked - pass the locked buffer directly
    // 2- array is not locked but pos array is - lock discard and copy the contents
    // 3- array is not locked and pos array is not locked - pass the UP pointer directly
    // 4- VBOs are in use, pass the VBO handle directly

    bool upArrayExists = false;

    if (s.mArraysLocked & 1)
    {
        vBuffers[idx] = vb.mhBuffers[idx];
    }
    else if (s.mActiveVBOs.vertex)
    {
        VertexBufferObject &vbo = s.mVBOs[s.mActiveVBOs.vertex];
        vBuffers[idx] = vbo.mHWBuffer;
    }
    else
    {
        // destroy and recreate buffer
        if (vb.mhBuffersUP[idx])
        {
            GetDDProcTable().pfnDestroyBuffer(GetDDHandle(), vb.mhBuffersUP[idx]);
        }
        vb.mhBuffersUP[idx] = GetDDProcTable().pfnCreateBuffer(GetDDHandle(), 0, (GLvoid *)s.mArrayPointers.mVertex.mpElements);
        vBuffers[idx] = vb.mhBuffersUP[idx];
        upArrayExists = true;
    }
    ++idx;

    // attributes next
    UINT64 lockedAttribs = s.mArraysLocked >> 1;
    UINT activeAttribs = vb.mAttributes.mask;
    UINT activeArrays = s.mCaps.attribArrayMask;
    DWORD index;

    while (_BitScanForward(&index, activeAttribs))
    {
        // if client array is not active, no VB for this slot
        if (!(activeArrays & (1 << index)))
        {
            vBuffers[idx] = NULL;
        }
        else if (s.mArraysLocked & (1 << index))
        {
            vBuffers[idx] = vb.mhBuffers[idx];
        }
        else if (_isActiveVBO(s, index))
        {
            vBuffers[idx] = _getActiveVBO(s, index);
        }
        else if (s.mArraysLocked & 1)
        {
            UINT size = (s.mLockedCount) * s.mArrayPointers.mArrays[index].mStride;

            void *pBuffer = GetDDProcTable().pfnLockBufferDiscard(GetDDHandle(), vb.mhBuffers[idx]);
            memcpy(pBuffer, (GLubyte *)s.mArrayPointers.mArrays[index].mpElements, size);
            GetDDProcTable().pfnUnlockBuffer(GetDDHandle(), vb.mhBuffers[idx]);
            vBuffers[idx] = vb.mhBuffers[idx];
        }
        else
        {
            if (vb.mhBuffersUP[idx])
            {
                GetDDProcTable().pfnDestroyBuffer(GetDDHandle(), vb.mhBuffersUP[idx]);
            }
            vb.mhBuffersUP[idx] = GetDDProcTable().pfnCreateBuffer(GetDDHandle(), 0, (GLvoid *)s.mArrayPointers.mArrays[index].mpElements);
            vBuffers[idx] = vb.mhBuffersUP[idx];
            upArrayExists = true;
        }
        ++idx;
        activeAttribs &= ~(1 << index);
    }

    if (s.mCaps.cullFace)
    {
        switch (s.mCullFace)
        {
        case GL_FRONT:
            GetDDProcTable().pfnSetCullMode(GetDDHandle(), s.mFrontFace);
            break;
        case GL_BACK:
            GetDDProcTable().pfnSetCullMode(GetDDHandle(), (s.mFrontFace == GL_CW) ? GL_CCW : GL_CW);
            break;
        default:
            s.mLastError = GL_INVALID_ENUM;
        }
    }
    else
    {
        GetDDProcTable().pfnSetCullMode(GetDDHandle(), GL_NONE);
    }

    GetDDProcTable().pfnSetScissorRect(GetDDHandle(), s.mScissor.x, s.mScissor.y, s.mScissor.width, s.mScissor.height);
    GetDDProcTable().pfnSetViewport(GetDDHandle(), s.mViewport.x, s.mViewport.y, s.mViewport.width, s.mViewport.height, s.mViewport.zNear, s.mViewport.zFar, s.mCaps.scissorTest);

    GetDDProcTable().pfnSetupSparseVertices(GetDDHandle(), s, vb.mAttributes, vb.mNumAttributes, &vBuffers[0], 0);

    // Tell the DD to generate shaders based on the current state.
    GetDDProcTable().pfnGenShaders(GetDDHandle(), s, true, type);

    // draw it!
    GetDDProcTable().pfnDrawIndexed(GetDDHandle(), mode, type, count, indexOffset);

    // Synchronize if we created any UP buffer
    if (upArrayExists)
    {
        for (UINT i = 0; i < idx; ++i)
        {
            if (vb.mhBuffersUP[i])
            {
                GetDDProcTable().pfnDestroyBuffer(GetDDHandle(), vb.mhBuffersUP[i]);
                vb.mhBuffersUP[i] = NULL;
            }
        }
    }

    // restore main index buffer
    vb.mhIndexBuffer = savedIdxBuffer;
}

#if defined(WIN32)
#pragma warning(default : 4244)
#endif

void glimEnable(State &s, GLenum cap)
{
    if (s.mNoExecute)
    {
        return;
    }

    glstEnable(s, cap);
}

void glimEnableClientState(State &s, GLenum cap)
{
    glimEnable(s, cap);
}

void glimEnd(State &s)
{
    glimDrawSWR(s);
}

void glimEndList(State &s)
{

    OptimizeDL(s, CompilingDL());
    s.mDisplayListMode = GL_NONE;
    s.mCompilingDL = 0;
}

void glimFogfv(State &s, GLenum pname, const GLfloat *params)
{
    glstFogfv(s, pname, params);
}

void glimFogf(State &s, GLenum pname, GLfloat param)
{
    glstFogf(s, pname, param);
}

void glimFogi(State &s, GLenum pname, GLint param)
{
    glstFogi(s, pname, param);
}

void glimFrontFace(State &s, GLenum frontFace)
{
    glstFrontFace(s, frontFace);
}

void glimFrustum(State &s, GLdouble xmin, GLdouble xmax, GLdouble ymin, GLdouble ymax, GLdouble zNear, GLdouble zFar)
{
    glstFrustum(s, xmin, xmax, ymin, ymax, zNear, zFar);
}

GLuint glimGenLists(State &, GLsizei range)
{
    return ReserveLists(range);
}

void glimGenTextures(State &s, GLsizei num, GLuint *textures)
{
    for (int i = 0; i < num; ++i)
    {
        textures[i] = s.mLastUsedTextureName;
        s.mTexParameters[textures[i]] = TexParameters();
        ++s.mLastUsedTextureName;
    }
}

void glimGetBooleanv(State &s, GLenum pname, GLboolean *params)
{
    glstGetBooleanv(s, pname, params);
}

void glimGetDoublev(State &s, GLenum pname, GLdouble *params)
{
    glstGetDoublev(s, pname, params);
}

GLenum glimGetError(State &s)
{
    return GL_NO_ERROR;
}

void glimGetFloatv(State &s, GLenum pname, GLfloat *params)
{
    glstGetFloatv(s, pname, params);
}

void glimGetIntegerv(State &s, GLenum pname, GLint *params)
{
    glstGetIntegerv(s, pname, params);
}

void glimGetLightfv(State &s, GLenum light, GLenum pname, GLfloat *params)
{
    glstGetLightfv(s, light, pname, params);
}

void glimGetLightiv(State &s, GLenum light, GLenum pname, GLint *params)
{
    glstGetLightiv(s, light, pname, params);
}

void glimGetMaterialfv(State &s, GLenum face, GLenum pname, GLfloat *params)
{
    glstGetMaterialfv(s, face, pname, params);
}

void glimGetMaterialiv(State &s, GLenum face, GLenum pname, GLint *params)
{
    glstGetMaterialiv(s, face, pname, params);
}

const GLubyte *glimGetString(State &s, GLenum name)
{
    return glstGetString(s, name);
}

void glimGetTexLevelParameterfv(State &s, GLenum target, GLint level, GLenum pname, GLfloat *params)
{
    glstGetTexLevelParameterfv(s, target, level, pname, params);
}
void glimGetTexLevelParameteriv(State &s, GLenum target, GLint level, GLenum pname, GLint *params)
{
    glstGetTexLevelParameteriv(s, target, level, pname, params);
}

void glimHint(State &, GLenum, GLenum)
{
    // We ignore this.
}

GLboolean glimIsEnabled(State &s, GLenum cap)
{
    return glstIsEnabled(s, cap);
}

void glimLight(State &s, GLenum light, GLenum pname, std::array<GLfloat, 4> const &params, bool isScalar)
{
    glstLight(s, light, pname, params, isScalar);
}

void glimLightModel(State &s, GLenum pname, std::array<GLfloat, 4> const &params, bool isScalar)
{
    glstLightModel(s, pname, params, isScalar);
}

void glimLoadIdentity(State &s)
{
    glstLoadIdentity(s);
}

void glimLoadMatrix(State &s, SWRL::m44f const &m)
{
    glstLoadMatrix(s, m);
}

void glimLockArraysEXT(State &s, GLint first, GLsizei count)
{
    // copy active arrays directly into the active VBs
    VertexBuffer &vb = GetVB(s);

    UINT idx = 0;

    // set up pos
    assert(s.mCaps.vertexArray);

    // fetch shader always tries to load 4 components per attribute per simd width, so pad out allocation to this
    UINT padding = 4 * 4 * KNOB_VS_SIMD_WIDTH;
    UINT offset = 0;
    UINT size = (count + first) * s.mArrayPointers.mVertex.mStride;

    vb.getBuffer(0, size + padding);
    void *pBuffer = GetDDProcTable().pfnLockBufferDiscard(GetDDHandle(), vb.mhBuffers[idx]);
    memcpy(pBuffer, (GLubyte *)s.mArrayPointers.mVertex.mpElements + offset, size);
    GetDDProcTable().pfnUnlockBuffer(GetDDHandle(), vb.mhBuffers[idx]);
    ++idx;
    s.mArraysLocked = 1;

    // set up attribute arrays
    UINT enabledArrays = s.mCaps.attribArrayMask;
    s.mArraysLocked = 1 | (enabledArrays << 1);

    DWORD index;
    while (_BitScanForward(&index, enabledArrays))
    {
        int vbIndex = index + 1;
        UINT offset = 0;
        UINT size = (count + first) * s.mArrayPointers.mArrays[index].mStride;

        vb.getBuffer(vbIndex, size + padding);
        void *pBuffer = GetDDProcTable().pfnLockBufferDiscard(GetDDHandle(), vb.mhBuffers[vbIndex]);
        memcpy(pBuffer, (GLubyte *)s.mArrayPointers.mArrays[index].mpElements + offset, size);
        GetDDProcTable().pfnUnlockBuffer(GetDDHandle(), vb.mhBuffers[vbIndex]);
        ++idx;
        enabledArrays &= ~(1 << index);
    }

    s.mLockedCount = (first + count);
}

void glimSetLockedArraysSWR(State &s, GLint lockedArrays)
{
    s.mArraysLocked = lockedArrays;
}

void glimMaterial(State &s, GLenum face, GLenum pname, std::array<GLfloat, 4> const &params, bool isScalar)
{
    glstMaterial(s, face, pname, params, isScalar);
}

void glimMatrixMode(State &s, GLenum mode)
{
    glstMatrixMode(s, mode);
}

void glimMultMatrix(State &s, SWRL::m44f const &m)
{
    glstMultMatrix(s, m);
}

void glimNewList(State &s, GLuint list, GLenum mode)
{
    if (list == 0)
    {
        s.mLastError = GL_INVALID_VALUE;
        return;
    }

    switch (mode)
    {
    case GL_COMPILE:
    case GL_COMPILE_AND_EXECUTE:
        break;
    default:
        s.mLastError = GL_INVALID_ENUM;
        return;
    }

    // delete existing list
    glimDeleteLists(s, list, 1);

    s.mDisplayListMode = mode;
    s.mCompilingDL = list;

    // Set up the display list.
    DisplayLists()[list] = new OGL::DisplayList();

    UpdateCompilingDL();
}

void glimNormal(State &s, GLfloat nx, GLfloat ny, GLfloat nz)
{
    glstNormal(s, nx, ny, nz);
}

void glimNormalPointer(State &s, GLenum type, GLsizei stride, const GLvoid *pointer)
{
    glstNormalPointer(s, type, stride, pointer);
}

void glimNumVerticesSWR(State &s, GLuint num, VertexAttributeFormats vAttrFmts)
{
    // Ignore hint.
}

void glimOrtho(State &s, GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar)
{
    glstOrtho(s, left, right, bottom, top, zNear, zFar);
}

void glimPixelStoref(State &s, GLenum pname, GLfloat param)
{
    glstPixelStorei(s, pname, (GLint)param);
}

void glimPixelStorei(State &s, GLenum pname, GLint param)
{
    glstPixelStorei(s, pname, param);
}

void glimPolygonMode(State &s, GLenum face, GLenum mode)
{
    glstPolygonMode(s, face, mode);
}

void glimPopAttrib(State &s)
{
    glstPopAttrib(s);
}

void glimPopMatrix(State &s)
{
    glstPopMatrix(s);
}

void glimPushAttrib(State &s, GLbitfield mask)
{
    glstPushAttrib(s, mask);
}

void glimPushMatrix(State &s)
{
    glstPushMatrix(s);
}

void glimReadPixels(State &s, GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels)
{
    RDTSC_START(APIReadPixels);

    // only support formats GL_RGB, GL_RGBA, GL_BGR_EXT, and GL_BGRA_EXT
    // only support type GL_UNSIGNED_BYTE, or GL_FLOAT
    //
    if (format != GL_RGB && format != GL_RGBA && format != GL_BGR && format != GL_BGRA && format != GL_DEPTH_COMPONENT)
    {
        s.mLastError = GL_OUT_OF_MEMORY;
        return;
    }

    if (type != GL_UNSIGNED_BYTE && type != GL_FLOAT)
    {
        s.mLastError = GL_OUT_OF_MEMORY;
        return;
    }

    GetDDProcTable().pfnCopyRenderTarget(GetDDHandle(), s, NULL, pixels, format, type, x, y, 0, 0, width, height, false);

    RDTSC_STOP(APIReadPixels, 0, 0);
}

void glimResetMainVBSWR(State &s)
{
    glstResetMainVBSWR(s);
}

void glimRotate(State &s, GLdouble O, GLdouble x, GLdouble y, GLdouble z)
{
    glstRotate(s, O, x, y, z);
}

void glimScale(State &s, GLdouble x, GLdouble y, GLdouble z)
{
    glstScale(s, x, y, z);
}

void glimScissor(State &s, GLint x, GLint y, GLsizei width, GLsizei height)
{
    glstScissor(s, x, y, width, height);
}

void glimSetClearMaskSWR(State &s, GLbitfield mask)
{
    glstSetClearMaskSWR(s, mask);
}

void glimSetTopologySWR(State &s, GLenum topology)
{
    glstSetTopologySWR(s, topology);
    AnalyzeVertexAttributes(s);
    _glimSetupVertexLayoutSWR(s, GetVB(s).mVBIEDLayout);
}

void glimSetUpDrawSWR(State &s, GLenum drawType)
{
    // If the context is undefined, then exit early.
    if (s.mNoExecute)
    {
        return;
    }

    GetDDProcTable().pfnNewDraw(GetDDHandle());

    if (s.mCaps.cullFace)
    {
        switch (s.mCullFace)
        {
        case GL_FRONT:
            GetDDProcTable().pfnSetCullMode(GetDDHandle(), s.mFrontFace);
            break;
        case GL_BACK:
            GetDDProcTable().pfnSetCullMode(GetDDHandle(), (s.mFrontFace == GL_CW) ? GL_CCW : GL_CW);
            break;
        default:
            s.mLastError = GL_INVALID_ENUM;
        }
    }
    else
    {
        GetDDProcTable().pfnSetCullMode(GetDDHandle(), GL_NONE);
    }

    GetDDProcTable().pfnSetScissorRect(GetDDHandle(), s.mScissor.x, s.mScissor.y, s.mScissor.width, s.mScissor.height);
    GetDDProcTable().pfnSetViewport(GetDDHandle(), s.mViewport.x, s.mViewport.y, s.mViewport.width, s.mViewport.height, s.mViewport.zNear, s.mViewport.zFar, s.mCaps.scissorTest);

    // Unlock and set the vertex buffers.
    for (GLuint i = 0; i < GetVB(s).mNumAttributes; ++i)
    {
        GetDDProcTable().pfnUnlockBuffer(GetDDHandle(), GetVB(s).mhBuffers[i]);
    }
    for (GLuint i = 0; i < GetVB(s).mNumSideAttributes; ++i)
    {
        GetDDProcTable().pfnUnlockBuffer(GetDDHandle(), GetVB(s).mhSideBuffers[i]);
    }

    auto &vtxAttrFmts = GetVB(s).mAttrFormats;
    GLuint strides[NUM_ATTRIBUTES] = { 0 };

    if (drawType == GL_NONE)
    {
    }
    else if (drawType == GL_VERTEX_ARRAY)
    {
        // Draw through array pointers.
        UINT64 activeAttribs = GetVB(s).mAttributes.mask;
        unsigned long index = 0;

        vtxAttrFmts.position = (s.mArrayPointers.mVertex.mNumCoordsPerAttribute - 1) * (s.mArrayPointers.mVertex.mType == GL_FLOAT ? 0 : 1);
        vtxAttrFmts.normal = (s.mArrayPointers.mNormal.mNumCoordsPerAttribute - 1) * (s.mArrayPointers.mNormal.mType == GL_FLOAT ? 0 : 1);
        vtxAttrFmts.fog = (s.mArrayPointers.mFog.mNumCoordsPerAttribute - 1) * (s.mArrayPointers.mFog.mType == GL_FLOAT ? 0 : 1);
        for (UINT i = 0; i < NUM_COLORS; ++i)
        {
            vtxAttrFmts.color |= ((s.mArrayPointers.mColor[i].mNumCoordsPerAttribute - 1) * (s.mArrayPointers.mColor[i].mType == GL_FLOAT ? 0 : 1)) << (i * AttrFmtShift);
        }
        for (UINT i = 0; i < NUM_TEXCOORDS; ++i)
        {
            vtxAttrFmts.texCoord |= ((s.mArrayPointers.mTexCoord[i].mNumCoordsPerAttribute - 1) * (s.mArrayPointers.mTexCoord[i].mType == GL_FLOAT ? 0 : 1)) << (i * AttrFmtShift);
        }
    }
    else
    {
        assert(false && "That draw type is not supported!");
    }

    // If we have a display list running, and normalize normals is on, then swap out
    // the normalized normal buffer for the regular normal buffer, and turn off
    // normal normalization in the shader.
    DDHANDLE vBuffers[NUM_ATTRIBUTES];
    memcpy(&vBuffers[0], &GetVB(s).mhBuffers[0], sizeof(DDHANDLE) * GetVB(s).mNumAttributes);

    bool resetNormalize = false;
    if (!s.mExecutingDL.empty())
    {
        if (GetVB(s).mAttributes.normal && s.mCaps.normalize)
        {
            resetNormalize = true;
            s.mCaps.normalize = 0;
            auto &vb = GetVB(s);
            auto &vbIEDlayout = vb.mVBIEDLayout;
            auto niedx = vbIEDlayout.position.normal;
            auto sbniedx = vbIEDlayout.sideBandPosition.normal;
            vBuffers[niedx] = vb.mhSideBuffers[sbniedx];
            ++sbniedx;
        }
    }

    GetDDProcTable().pfnSetupVertices(GetDDHandle(), GetVB(s).mAttributes, vtxAttrFmts, GetVB(s).mNumAttributes, vBuffers, GetVB(s).mhNIB8);

    // Tell the DD to generate shaders based on the current state.
    GetDDProcTable().pfnGenShaders(GetDDHandle(), s, false, GL_UNSIGNED_INT);

    if (resetNormalize)
    {
        s.mCaps.normalize = 1;
    }
}

void glimShadeModel(State &s, GLenum mode)
{
    glstShadeModel(s, mode);
}

void glimSubstituteVBSWR(State &s, GLsizei which)
{
    glstSubstituteVBSWR(s, which);
}

void glimTexCoord(State &state, GLenum texCoord, GLfloat s, GLfloat t, GLfloat r, GLfloat q)
{
    glstTexCoord(state, texCoord, s, t, r, q);
}

void glimTexCoordPointer(State &s, GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
    glstTexCoordPointer(s, size, type, stride, pointer);
}

void glimTexEnv(State &s, GLenum target, GLenum pname, SWRL::v4f const &params)
{
    glstTexEnv(s, target, pname, params);
}

void glimTexImage2D(State &s, GLenum target, GLint mipLevel, GLint internalFormat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *data)
{
    TextureUnit &texUnit = s.mTexUnit[s.mActiveTexture - GL_TEXTURE0];
    TexParameters &texParams = s.mTexParameters[texUnit.mNamedTexture2d];

    GLenum errorReturn = GL_INVALID_ENUM;

    // According to 1.5 spec 3.8.11: If request isn't supported, set proxy texture params to 0, but don't set error.
    if (target == GL_PROXY_TEXTURE_2D)
    {
        texParams.mWidth = 0;
        texParams.mHeight = 0;
        errorReturn = GL_NO_ERROR;
    }

    if (data == NULL)
    {
        return;
    }

    if ((target != GL_TEXTURE_2D) && (target != GL_PROXY_TEXTURE_2D))
    {
        // XXX: we might need to support CUBE_MAP*
        s.mLastError = errorReturn;
        return;
    }

    if (0) //internalFormat > 4)
    {
        // XXX: forget all the exotic crap.
        s.mLastError = errorReturn;
        return;
    }

    if ((format != GL_RGBA) && (format != GL_BGRA) && (format != GL_RGB) && (format != GL_BGR) && (format != GL_ALPHA))
    {
        // XXX: we can support other formats through manual swizzling.
        s.mLastError = errorReturn;
        return;
    }

    if ((type != GL_UNSIGNED_BYTE) && (type != GL_FLOAT) && (type != GL_UNSIGNED_INT_8_8_8_8_REV))
    {
        // XXX: we can support other type through up-conversion to BYTE or FLOAT.
        s.mLastError = errorReturn;
        return;
    }

    RDTSC_START(APITexImage);

    UINT size[3] = { UINT(width), UINT(height), 1 };

    if (texParams.mhTexture == 0)
    {
        assert(mipLevel == 0);
        texParams.mhTexture = GetDDProcTable().pfnCreateTexture(GetDDHandle(), size);
        texParams.mWidth = width;
        texParams.mHeight = height;
    }
    else
    {
        // If the texture is rebound, destroy and recreate a new texture object
        // Destroy will stall until the resource is no longer in use
        if (mipLevel == 0 && (texParams.mWidth != width || texParams.mHeight != height))
        {
            GetDDProcTable().pfnDestroyTexture(GetDDHandle(), texParams.mhTexture);
            texParams.mhTexture = GetDDProcTable().pfnCreateTexture(GetDDHandle(), size);
            texParams.mWidth = width;
            texParams.mHeight = height;
        }
    }

    if (target != GL_PROXY_TEXTURE_2D)
    {
        GetDDProcTable().pfnLockTexture(GetDDHandle(), texParams.mhTexture);

        UINT subtexIdx = GetDDProcTable().pfnGetSubtextureIndex(GetDDHandle(), texParams.mhTexture, 0, mipLevel);

        GetDDProcTable().pfnUpdateSubtexture(GetDDHandle(), texParams.mhTexture, subtexIdx, format, type, 0, 0, width, height, data);

        GetDDProcTable().pfnUnlockTexture(GetDDHandle(), texParams.mhTexture);

        RDTSC_STOP(APITexImage, 0, 0);

#if 0
		char filename[256];
		static int idx = 0;
		sprintf(filename, "texture_%d_%dx%d.rgba", texUnit.mNamedTexture2d, width, height);
		FILE *f = fopen(filename, "wb");
		if (format == GL_RGBA)
		{
			fwrite(data, width*height*4, 1, f);
		}
		else
		{
			for (UINT y = 0; y < height; ++y)
			{
				for (UINT x = 0; x < width; ++x)
				{
					unsigned int *pixel = (unsigned int*)((BYTE*)data + y * width * 4 + x * 4);
					unsigned char b = *(pixel) & 0xff;
					unsigned char g = (*(pixel + 1) & 0xff00) >> 8;
					unsigned char r = (*(pixel + 2) & 0xff0000) >> 16;
					unsigned char a = 0xff; //*(pixel + 3) * 255.0;
					fwrite(&r, 1, 1, f);
					fwrite(&g, 1, 1, f);
					fwrite(&b, 1, 1, f);
					fwrite(&a, 1, 1, f);
				}
			}
		}
		fclose(f);
#endif
    }
}

void glimTexSubImage2D(State &s, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels)
{
    TextureUnit &texUnit = s.mTexUnit[s.mActiveTexture - GL_TEXTURE0];
    TexParameters &params = s.mTexParameters[texUnit.mNamedTexture2d];

    assert(params.mhTexture != NULL);
    if (!params.mhTexture)
    {
        return;
    }

    GetDDProcTable().pfnLockTexture(GetDDHandle(), params.mhTexture);

    UINT subtexIdx = GetDDProcTable().pfnGetSubtextureIndex(GetDDHandle(), params.mhTexture, 0, level);

    GetDDProcTable().pfnUpdateSubtexture(GetDDHandle(), params.mhTexture, subtexIdx, format, type, xoffset, yoffset, width, height, pixels);

    GetDDProcTable().pfnUnlockTexture(GetDDHandle(), params.mhTexture);
}

void glimTexParameter(State &s, GLenum target, GLenum pname, SWRL::v4f const &params)
{
    glstTexParameter(s, target, pname, params);
}

void glimTranslate(State &s, GLdouble x, GLdouble y, GLdouble z)
{
    glstTranslate(s, x, y, z);
}

void glimUnlockArraysEXT(State &s)
{
    glstUnlockArraysEXT(s);
}

void glimVertex(State &s, GLfloat x, GLfloat y, GLfloat z, GLfloat w, GLuint count)
{
    // If the context is undefined, exit early.
    if (s.mNoExecute)
    {
        return;
    }

    GetVB(s).mAttrFormatsSeen.position = std::max((AttributeFormat)GetVB(s).mAttrFormatsSeen.position, _glimNumComponentsToFormat(count));

    if (GetVB(s).mNumVertices > VERTEX_BUFFER_COUNT)
    {
        s.mLastError = GL_OUT_OF_MEMORY;
        return;
    }

    // We have to flush the buffers --- they're full.
    if (GetVB(s).mNumVertices == VERTEX_BUFFER_COUNT)
    {
        glimDrawSWR(s);

        // Now lock discard to get the buffers back.
        for (GLuint i = 0; i < GetVB(s).mNumAttributes; ++i)
        {
            GetVB(s).mpfBuffers[i] = (GLfloat *)GetDDProcTable().pfnLockBufferDiscard(GetDDHandle(), GetVB(s).mhBuffers[i]);
            assert(GetVB(s).mpfBuffers[i]);
        }
        for (GLuint i = 0; i < GetVB(s).mNumSideAttributes; ++i)
        {
            GetVB(s).mpfSideBuffers[i] = (GLfloat *)GetDDProcTable().pfnLockBufferDiscard(GetDDHandle(), GetVB(s).mhSideBuffers[i]);
            assert(GetVB(s).mpfSideBuffers[i]);
        }
        GetVB(s).mNumVertices = 0;
    }

    glimAddVertexSWR(s, x, y, z, w, -1, false, (GLenum)OGL_NO_INDEX_BUFFER_GENERATION);
}

void glimVertexPointer(State &s, GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
    glstVertexPointer(s, size, type, stride, pointer);
}

void glimViewport(State &s, GLint x, GLint y, GLsizei width, GLsizei height)
{
    glstViewport(s, x, y, width, height);

#if !defined(_WIN32)
    // Tell the driver of the viewport change incase the render targets need to be resized
    s.pfnResize(width, height);
#endif
}

// VBO extension API
void glimBindBufferARB(State &s, GLenum target, GLuint buffer)
{
    // new VBO
    if (s.mVBOs.find(buffer) == s.mVBOs.end())
    {
        s.mVBOs[buffer] = VertexBufferObject();
    }

    switch (target)
    {
    case GL_ARRAY_BUFFER_ARB:
        s.mActiveArrayVBO = buffer;
        break;
    case GL_ELEMENT_ARRAY_BUFFER_ARB:
        s.mActiveElementVBO = buffer;
        break;
    default:
        assert(0 && "Invalid VBO target specified");
    }

    return;
}

void glimDeleteBuffersARB(State &s, GLsizei n, const GLuint *buffers)
{
    for (GLint i = 0; i < n; ++i)
    {
        s.mVBOs.erase(buffers[i]);
    }
}

void glimGenBuffersARB(State &s, GLsizei n, GLuint *buffers)
{
    for (GLint i = 0; i < n; ++i)
    {
        while (s.mVBOs.find(++s.mLastUsedVBO) != s.mVBOs.end())
            ;
        buffers[i] = s.mLastUsedVBO;
    }
}

void glimBufferDataARB(State &s, GLenum target, GLsizeiptrARB size, const GLvoid *data, GLenum usage)
{
    GLuint boundBuffer;
    switch (target)
    {
    case GL_ARRAY_BUFFER_ARB:
        boundBuffer = s.mActiveArrayVBO;
        break;
    case GL_ELEMENT_ARRAY_BUFFER_ARB:
        boundBuffer = s.mActiveElementVBO;
        break;
    default:
        assert(0 && "Invalid VBO target specified.");
    }

    assert(boundBuffer != 0);

    VertexBufferObject &vbo = s.mVBOs[boundBuffer];

    void *pData;

    // pad size out for simd loads
    UINT newsize = (GLuint)size + sizeof(float) * 32;

    // if vbo already exists and new size is smaller, do a lock discard.  otherwise destroy and recreate
    if (vbo.mHWBuffer)
    {
        if (newsize <= vbo.mSize)
        {
            pData = GetDDProcTable().pfnLockBufferDiscard(GetDDHandle(), vbo.mHWBuffer);
        }
        else
        {
            GetDDProcTable().pfnDestroyBuffer(GetDDHandle(), vbo.mHWBuffer);
            vbo.mHWBuffer = GetDDProcTable().pfnCreateBuffer(GetDDHandle(), newsize, NULL);
            pData = GetDDProcTable().pfnLockBuffer(GetDDHandle(), vbo.mHWBuffer);
        }
    }
    else
    {
        vbo.mHWBuffer = GetDDProcTable().pfnCreateBuffer(GetDDHandle(), newsize, NULL);
        pData = GetDDProcTable().pfnLockBuffer(GetDDHandle(), vbo.mHWBuffer);
    }

    vbo.mSize = newsize;
    vbo.mUsage = usage;
    vbo.mAccess = GL_READ_WRITE_ARB;
    vbo.mMapped = GL_FALSE;
    vbo.mMappedPointer = NULL;

    memset(pData, 0x0, newsize);

    if (data)
    {
        memcpy(pData, data, size);
    }
    GetDDProcTable().pfnUnlockBuffer(GetDDHandle(), vbo.mHWBuffer);
}

void glimBufferSubDataARB(State &s, GLenum target, GLintptrARB offset, GLsizeiptrARB size, const GLvoid *data)
{
    GLuint boundBuffer;
    switch (target)
    {
    case GL_ARRAY_BUFFER_ARB:
        boundBuffer = s.mActiveArrayVBO;
        break;
    case GL_ELEMENT_ARRAY_BUFFER_ARB:
        boundBuffer = s.mActiveElementVBO;
        break;
    default:
        assert(0 && "Invalid VBO target specified.");
    }

    assert(boundBuffer != 0);

    VertexBufferObject &vbo = s.mVBOs[boundBuffer];

    void *pData = GetDDProcTable().pfnLockBuffer(GetDDHandle(), vbo.mHWBuffer);
    memcpy((BYTE *)pData + offset, data, size);
    GetDDProcTable().pfnUnlockBuffer(GetDDHandle(), vbo.mHWBuffer);
}

GLvoid *glimMapBufferARB(State &s, GLenum target, GLenum access)
{
    GLuint boundBuffer;
    switch (target)
    {
    case GL_ARRAY_BUFFER_ARB:
        boundBuffer = s.mActiveArrayVBO;
        break;
    case GL_ELEMENT_ARRAY_BUFFER_ARB:
        boundBuffer = s.mActiveElementVBO;
        break;
    default:
        assert(0 && "Invalid VBO target specified.");
    }

    assert(boundBuffer != 0);

    VertexBufferObject &vbo = s.mVBOs[boundBuffer];

    if (vbo.mMapped)
    {
        s.mLastError = GL_INVALID_OPERATION;
        return NULL;
    }

    vbo.mAccess = access;
    vbo.mMapped = GL_TRUE;
    vbo.mMappedPointer = GetDDProcTable().pfnLockBuffer(GetDDHandle(), vbo.mHWBuffer);

    return vbo.mMappedPointer;
}

GLboolean glimUnmapBufferARB(State &s, GLenum target)
{
    GLuint boundBuffer;
    switch (target)
    {
    case GL_ARRAY_BUFFER_ARB:
        boundBuffer = s.mActiveArrayVBO;
        break;
    case GL_ELEMENT_ARRAY_BUFFER_ARB:
        boundBuffer = s.mActiveElementVBO;
        break;
    default:
        assert(0 && "Invalid VBO target specified.");
    }

    assert(boundBuffer != 0);

    VertexBufferObject &vbo = s.mVBOs[boundBuffer];

    if (!vbo.mMapped)
    {
        s.mLastError = GL_INVALID_OPERATION;
        return GL_FALSE;
    }

    vbo.mMapped = GL_FALSE;
    vbo.mMappedPointer = NULL;
    GetDDProcTable().pfnUnlockBuffer(GetDDHandle(), vbo.mHWBuffer);

    return GL_TRUE;
}

GLboolean glimIsBufferARB(State &s, GLuint buffer)
{
    if (buffer == 0)
        return GL_FALSE;
    return (s.mVBOs.find(buffer) != s.mVBOs.end());
}

void glimBindBuffer(State &s, GLenum target, GLuint buffer)
{
    glimBindBufferARB(s, target, buffer);
}

void glimDeleteBuffers(State &s, GLsizei n, const GLuint *buffers)
{
    glimDeleteBuffersARB(s, n, buffers);
}

void glimGenBuffers(State &s, GLsizei n, GLuint *buffers)
{
    glimGenBuffersARB(s, n, buffers);
}

void glimBufferData(State &s, GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage)
{
    glimBufferDataARB(s, target, size, data, usage);
}

void glimBufferSubData(State &s, GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid *data)
{
    glimBufferSubDataARB(s, target, offset, size, data);
}

GLvoid *glimMapBuffer(State &s, GLenum target, GLenum access)
{
    return glimMapBufferARB(s, target, access);
}

GLboolean glimUnmapBuffer(State &s, GLenum target)
{
    return glimUnmapBufferARB(s, target);
}

GLboolean glimIsBuffer(State &s, GLuint buffer)
{
    return glimIsBufferARB(s, buffer);
}

} // namespace OGL
