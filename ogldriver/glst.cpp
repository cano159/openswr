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

#if !defined(_WIN32)
#include <math.h>
#include <limits.h>
#endif // _WIN32
#include "glst.hpp"
#include "algebra.hpp"
#include "ogldisplaylist.hpp"
#include "utils.h" // XXX: remove ASAP.
#include "gldd.h"

#if defined(_WIN32)
#define snprintf _snprintf
#endif

#define SWR_UNIMPLEMENTED assert(0 && "Unimplemented");

namespace OGL
{

GLuint _glstEnumTypeToSize(GLenum eType)
{
    switch (eType)
    {
    case GL_UNSIGNED_BYTE:
    case GL_BYTE:
        return sizeof(GLbyte);
    case GL_UNSIGNED_SHORT:
    case GL_SHORT:
        return sizeof(GLshort);
    case GL_UNSIGNED_INT:
    case GL_INT:
        return sizeof(GLint);
    case GL_FLOAT:
        return sizeof(GLfloat);
    case GL_DOUBLE:
        return sizeof(GLdouble);
    }

    return 0;
}

template <typename GLTy>
void _glstGetGLTyv(State &s, GLenum pname, GLTy *params)
{
    __m128 row0, row1, row2, row3;

    //XXX BMCFIX: More conversion rules need to be added!!!  Just casting to the destination type isn't always right.

    switch (pname)
    {
    case GL_MAJOR_VERSION:
        params[0] = 1;
        break;

    case GL_MINOR_VERSION:
        params[0] = 3;
        break;

    case GL_COLOR_CLEAR_VALUE:
        params[0] = (GLTy)s.mClearColor[0];
        params[1] = (GLTy)s.mClearColor[1];
        params[2] = (GLTy)s.mClearColor[2];
        params[3] = (GLTy)s.mClearColor[3];
        break;
    case GL_LIGHT_MODEL_COLOR_CONTROL:
        params[0] = (GLTy)(s.mCaps.specularColor ? GL_SEPARATE_SPECULAR_COLOR : GL_SINGLE_COLOR);
        break;
    case GL_LIGHT_MODEL_TWO_SIDE:
        params[0] = (GLTy)s.mCaps.twoSided;
        break;
    case GL_LIGHT_MODEL_LOCAL_VIEWER:
        params[0] = (GLTy)s.mCaps.localViewer;
        break;
    case GL_FOG_MODE:
        params[0] = (GLTy)s.mFog.mMode;
        break;
    case GL_FOG_DENSITY:
        params[0] = (GLTy)s.mFog.mDensity;
        break;
    case GL_LIGHT_MODEL_AMBIENT:
        params[0] = (GLTy)s.mLightModel.mAmbient[0];
        params[1] = (GLTy)s.mLightModel.mAmbient[1];
        params[2] = (GLTy)s.mLightModel.mAmbient[2];
        params[3] = (GLTy)s.mLightModel.mAmbient[3];
        break;
    case GL_RED_BITS:
    case GL_BLUE_BITS:
    case GL_GREEN_BITS:
    case GL_ALPHA_BITS:
        params[0] = 8;
        break;
    case GL_DEPTH_BITS:
        params[0] = 24;
        break;
    case GL_MATRIX_MODE:
        params[0] = (GLTy)s.mMatrixMode;
        break;
    case GL_VIEWPORT:
        params[0] = (GLTy)s.mViewport.x;
        params[1] = (GLTy)s.mViewport.y;
        params[2] = (GLTy)s.mViewport.width;
        params[3] = (GLTy)s.mViewport.height;
        break;
    case GL_PROJECTION_MATRIX:
    {
        SWRL::m44f &mat = s.mMatrices[OGL::PROJECTION].top();
        row0 = _mm_loadu_ps((const float *)&mat[0]);
        row1 = _mm_loadu_ps((const float *)&mat[1]);
        row2 = _mm_loadu_ps((const float *)&mat[2]);
        row3 = _mm_loadu_ps((const float *)&mat[3]);
        vTranspose(row0, row1, row2, row3);
        _mm_storeu_ps((float *)params, row0);
        _mm_storeu_ps((float *)(params + 4), row1);
        _mm_storeu_ps((float *)(params + 8), row2);
        _mm_storeu_ps((float *)(params + 12), row3);
        break;
    }
    case GL_TEXTURE_MATRIX:
    {
        int activeTextureIndex = s.mActiveTexture - GL_TEXTURE0;
        SWRL::m44f &mat = s.mMatrices[OGL::TEXTURE_BASE + activeTextureIndex].top();
        row0 = _mm_loadu_ps((const float *)&mat[0]);
        row1 = _mm_loadu_ps((const float *)&mat[1]);
        row2 = _mm_loadu_ps((const float *)&mat[2]);
        row3 = _mm_loadu_ps((const float *)&mat[3]);
        vTranspose(row0, row1, row2, row3);
        _mm_storeu_ps((float *)params, row0);
        _mm_storeu_ps((float *)(params + 4), row1);
        _mm_storeu_ps((float *)(params + 8), row2);
        _mm_storeu_ps((float *)(params + 12), row3);
        break;
    }

    case GL_MODELVIEW_MATRIX:
    {
        SWRL::m44f &mat = s.mMatrices[OGL::MODELVIEW].top();
        row0 = _mm_loadu_ps((const float *)&mat[0]);
        row1 = _mm_loadu_ps((const float *)&mat[1]);
        row2 = _mm_loadu_ps((const float *)&mat[2]);
        row3 = _mm_loadu_ps((const float *)&mat[3]);
        vTranspose(row0, row1, row2, row3);
        _mm_storeu_ps((float *)params, row0);
        _mm_storeu_ps((float *)(params + 4), row1);
        _mm_storeu_ps((float *)(params + 8), row2);
        _mm_storeu_ps((float *)(params + 12), row3);
        break;
    }
    case GL_SCISSOR_BOX:
        params[0] = (GLTy)s.mScissor.x;
        params[1] = (GLTy)s.mScissor.y;
        params[2] = (GLTy)s.mScissor.width;
        params[3] = (GLTy)s.mScissor.height;
        break;
    case GL_SCISSOR_TEST:
        params[0] = (s.mCaps.scissorTest != 0);
        break;
    case GL_POLYGON_MODE:
        params[0] = (GLTy)s.mPolygonMode[0];
        params[1] = (GLTy)s.mPolygonMode[1];
        break;
    case GL_MAX_PROJECTION_STACK_DEPTH:
    case GL_MAX_MODELVIEW_STACK_DEPTH:
    case GL_MAX_TEXTURE_STACK_DEPTH:
        params[0] = MAX_MATRIX_STACK_DEPTH;
        break;
    case GL_PROJECTION_STACK_DEPTH:
        params[0] = (GLTy)s.mMatrices[OGL::PROJECTION].size();
        break;
    case GL_MODELVIEW_STACK_DEPTH:
        params[0] = (GLTy)s.mMatrices[OGL::MODELVIEW].size();
        break;
    case GL_TEXTURE_STACK_DEPTH:
    {
        int activeTextureIndex = s.mActiveTexture - GL_TEXTURE0;
        params[0] = (GLTy)s.mMatrices[OGL::TEXTURE_BASE + activeTextureIndex].size();
        break;
    }
    case GL_DRAW_BUFFER:
        params[0] = (GLTy)GL_BACK;
        break;
    case GL_MAX_TEXTURE_SIZE:
        params[0] = (GLTy)4096; //?
        break;

    case GL_MAX_TEXTURE_UNITS:
        params[0] = 8;
        break;

    case GL_MAX_VIEWPORT_DIMS:
        params[0] = (GLTy)(int) KNOB_GUARDBAND_WIDTH;
        params[1] = (GLTy)(int) KNOB_GUARDBAND_HEIGHT;
        break;

    // weird unknokwn params requested by minecraft
    case 0x8b4a:
    case 0x8b49:
        break;

    case GL_SAMPLES:
        params[0] = 0;
        break;

    case GL_STEREO:
        params[0] = GL_FALSE;
        break;

    // VBO extension
    case GL_ARRAY_BUFFER_BINDING_ARB:
        params[0] = (GLTy)s.mActiveArrayVBO;
        break;

    case GL_ELEMENT_ARRAY_BUFFER_BINDING_ARB:
        params[0] = (GLTy)s.mActiveElementVBO;
        break;

    case GL_VERTEX_ARRAY_BUFFER_BINDING_ARB:
        params[0] = (GLTy)s.mActiveVBOs.vertex;
        break;

    case GL_NORMAL_ARRAY_BUFFER_BINDING_ARB:
        params[0] = (GLTy)s.mActiveVBOs.normal;
        break;

    case GL_COLOR_ARRAY_BUFFER_BINDING_ARB:
        params[0] = (GLTy)s.mActiveVBOs.color[0];
        break;

    case GL_INDEX_ARRAY_BUFFER_BINDING_ARB:
        params[0] = (GLTy)s.mActiveVBOs.index;
        break;

    case GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING_ARB:
        params[0] = (GLTy)s.mActiveVBOs.texcoord[0];
        break;

    case GL_SECONDARY_COLOR_ARRAY_BUFFER_BINDING_ARB:
        params[0] = (GLTy)s.mActiveVBOs.color[1];
        break;

    case GL_STENCIL_BITS:
        params[0] = (GLTy)0;
        break;

    case GL_AUX_BUFFERS:
        params[0] = (GLTy)0;
        break;

    case GL_ACCUM_RED_BITS:
    case GL_ACCUM_GREEN_BITS:
    case GL_ACCUM_BLUE_BITS:
    case GL_ACCUM_ALPHA_BITS:
        params[0] = (GLTy)8;
        break;

    case GL_CURRENT_COLOR:
        params[0] = (GLTy)s.mColor[0][0];
        params[1] = (GLTy)s.mColor[0][1];
        params[2] = (GLTy)s.mColor[0][2];
        params[3] = (GLTy)s.mColor[0][3];
        break;

    case GL_CURRENT_INDEX:
        params[0] = 1;

        SWR_UNIMPLEMENTED;
        break;

    case GL_CURRENT_NORMAL:
        params[0] = (GLTy)s.mNormal[0];
        params[1] = (GLTy)s.mNormal[1];
        params[2] = (GLTy)s.mNormal[2];
        break;

    case GL_CURRENT_TEXTURE_COORDS:
        params[0] = (GLTy)s.mTexCoord[0][0];
        params[1] = (GLTy)s.mTexCoord[0][1];
        params[2] = (GLTy)s.mTexCoord[0][2];
        params[3] = (GLTy)s.mTexCoord[0][3];
        break;

    // raster pos unimplemented
    case GL_CURRENT_RASTER_POSITION:
        params[0] = 0;
        params[1] = 0;
        params[2] = 0;
        params[3] = 1;

        SWR_UNIMPLEMENTED;
        break;

    case GL_CURRENT_RASTER_DISTANCE:
        params[0] = 0;

        SWR_UNIMPLEMENTED;
        break;

    case GL_CURRENT_RASTER_COLOR:
        params[0] = 1;
        params[1] = 1;
        params[2] = 1;
        params[3] = 1;

        SWR_UNIMPLEMENTED;
        break;

    case GL_CURRENT_RASTER_INDEX:
        params[0] = 1;

        SWR_UNIMPLEMENTED;
        break;

    case GL_CURRENT_RASTER_TEXTURE_COORDS:
        params[0] = 0;
        params[1] = 0;
        params[2] = 0;
        params[3] = 1;

        SWR_UNIMPLEMENTED;
        break;

    case GL_CURRENT_RASTER_POSITION_VALID:
        params[0] = 1;

        SWR_UNIMPLEMENTED;
        break;

    case GL_EDGE_FLAG:
        params[0] = 1;

        SWR_UNIMPLEMENTED;
        break;

    case GL_VERTEX_ARRAY:
        params[0] = (GLTy)s.mCaps.vertexArray;
        break;

    case GL_VERTEX_ARRAY_SIZE:
        params[0] = (GLTy)s.mArrayPointers.mVertex.mNumCoordsPerAttribute;
        break;

    case GL_VERTEX_ARRAY_TYPE:
        params[0] = (GLTy)s.mArrayPointers.mVertex.mType;
        break;

    case GL_VERTEX_ARRAY_STRIDE:
        params[0] = (GLTy)s.mArrayPointers.mVertex.mStride;
        break;

    case GL_NORMAL_ARRAY:
        params[0] = (GLTy)s.mCaps.normalArray;
        break;

    case GL_NORMAL_ARRAY_TYPE:
        params[0] = (GLTy)s.mArrayPointers.mNormal.mType;
        break;

    case GL_NORMAL_ARRAY_STRIDE:
        params[0] = (GLTy)s.mArrayPointers.mNormal.mStride;
        break;

    case GL_COLOR_ARRAY:
        params[0] = (GLTy)(s.mCaps.colorArray & 1);
        break;

    case GL_COLOR_ARRAY_SIZE:
        params[0] = (GLTy)s.mArrayPointers.mColor[0].mNumCoordsPerAttribute;
        break;

    case GL_COLOR_ARRAY_TYPE:
        params[0] = (GLTy)s.mArrayPointers.mColor[0].mType;
        break;

    case GL_COLOR_ARRAY_STRIDE:
        params[0] = (GLTy)s.mArrayPointers.mColor[0].mStride;
        break;

    case GL_SECONDARY_COLOR_ARRAY:
        params[0] = (GLTy)(s.mCaps.colorArray & 2);
        break;

    case GL_SECONDARY_COLOR_ARRAY_SIZE:
        params[0] = (GLTy)s.mArrayPointers.mColor[1].mNumCoordsPerAttribute;
        break;

    case GL_SECONDARY_COLOR_ARRAY_TYPE:
        params[0] = (GLTy)s.mArrayPointers.mColor[1].mType;
        break;

    case GL_SECONDARY_COLOR_ARRAY_STRIDE:
        params[0] = (GLTy)s.mArrayPointers.mColor[1].mStride;
        break;

    case GL_TEXTURE_COORD_ARRAY:
        params[0] = (GLTy)(s.mCaps.texCoordArray & 1);
        break;

    case GL_TEXTURE_COORD_ARRAY_SIZE:
        params[0] = (GLTy)s.mArrayPointers.mTexCoord[0].mNumCoordsPerAttribute;
        break;

    case GL_TEXTURE_COORD_ARRAY_TYPE:
        params[0] = (GLTy)s.mArrayPointers.mTexCoord[0].mType;
        break;

    case GL_TEXTURE_COORD_ARRAY_STRIDE:
        params[0] = (GLTy)s.mArrayPointers.mTexCoord[0].mStride;
        break;

    case GL_CLIENT_ACTIVE_TEXTURE:
        params[0] = (GLTy)s.mClientActiveTexture;
        break;

    case GL_LINE_WIDTH_RANGE:
        params[0] = (GLTy)1;
        params[1] = (GLTy)1;
        break;

    case GL_RENDER_MODE:
        params[0] = (GLTy)GL_RENDER; //BMCTODO: add full support.  glRenderMode is a stub, and don't currently support anything other than GL_RENDER.  ParaView checks this state A LOT!
        break;

    case GL_PACK_ROW_LENGTH:
        params[0] = (GLTy)s.mPixelStore.mPackRowLength;
        break;

    case GL_DRAW_FRAMEBUFFER_BINDING:
        params[0] = 0; // XXX: for apitrace replay
        break;

    case GL_ATTRIB_STACK_DEPTH:
        params[0] = (GLTy)s.mAttribStack.size();
        break;

    case GL_CULL_FACE_MODE:
        params[0] = s.mCullFace;
        break;

    case GL_FRONT_FACE:
        params[0] = s.mFrontFace;
        break;

    default:
        printf("unknown parameter %d (0x%x)\n", pname, pname);
        assert(0);
        break;
    }
}

template <typename GLTy>
void _glstGetTexLevelParameterTyv(State &s, GLenum target, GLint level, GLenum pname, GLTy *params)
{
    TextureUnit &texUnit = s.mTexUnit[s.mActiveTexture - GL_TEXTURE0];
    TexParameters &texParams = s.mTexParameters[texUnit.mNamedTexture2d];

    // level < 0 is always an error.
    // SWR doesn't currently support mip levels, so anthing != 0 is an error.
    if (level != 0)
    {
        s.mLastError = GL_INVALID_VALUE;
        return;
    }

    // SWR currently only supports 2D and Proxy 2D targets
    switch (target)
    {
    case GL_TEXTURE_2D:
    case GL_PROXY_TEXTURE_2D:
        // Initialized above texParams	= s.mTexParameters[texUnit.mNamedTexture2d];
        break;
    case GL_TEXTURE_1D:
    case GL_PROXY_TEXTURE_1D:
    case GL_TEXTURE_3D:
    case GL_PROXY_TEXTURE_3D:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
    case GL_PROXY_TEXTURE_CUBE_MAP:
    default:
        s.mLastError = GL_INVALID_VALUE;
        return;
    }

    switch (pname)
    {
    case GL_TEXTURE_WIDTH: // params returns a single value, the width of the texture image.  This value includes the border of the texture image. The initial value is 0.
        params[0] = (GLTy)texParams.mWidth;
        break;
    case GL_TEXTURE_HEIGHT: // params returns a single value, the height of the texture image.  This value includes the border of the texture image. The initial value is 0.
        params[0] = (GLTy)texParams.mHeight;
        break;
    case GL_TEXTURE_DEPTH: // params returns a single value, the depth of the texture image.  This value includes the border of the texture image. The initial value is 0.
        params[0] = 0;
        break;
    case GL_TEXTURE_INTERNAL_FORMAT: // params returns a single value, the internal format of the texture image.
    case GL_TEXTURE_BORDER:          // params returns a single value, the width in pixels of the border of the texture image. The initial value is 0.
    case GL_TEXTURE_RED_SIZE:        // The internal storage resolution of an individual component.  The resolution chosen by the GL will be a close match for the resolution requested by the user with the component argument of glTexImage1D, glTexImage2D, glTexImage3D, glCopyTexImage1D, and glCopyTexImage2D. The initial value is 0.
    case GL_TEXTURE_GREEN_SIZE:
    case GL_TEXTURE_BLUE_SIZE:
    case GL_TEXTURE_ALPHA_SIZE:
    case GL_TEXTURE_LUMINANCE_SIZE:
    case GL_TEXTURE_INTENSITY_SIZE:
    case GL_TEXTURE_DEPTH_SIZE:
    case GL_TEXTURE_COMPRESSED:            // params returns a single boolean value indicating if the texture image is stored in a compressed internal format.  The initiali value is GL_FALSE.
    case GL_TEXTURE_COMPRESSED_IMAGE_SIZE: // params returns a single integer value, the number of unsigned bytes of the compressed texture image that would be returned from glGetCompressedTexImage.
    default:
        s.mLastError = GL_INVALID_ENUM;
        return;
    }
}

void glstActiveTexture(State &s, GLenum which)
{
    s.mActiveTexture = which;
}

void glstAddVertexSWR(State &s, GLdouble x, GLdouble y, GLdouble z, GLdouble w, GLint index, GLboolean permuteVBs, GLenum genIBKind)
{
}

void glstAlphaFunc(State &s, GLenum func, GLclampf ref)
{
    s.mAlphaFunc = func;
    s.mAlphaRef = SWRL::clamp(ref, 0.0f, 1.0f);
}

void glstArrayElement(State &, GLint index)
{
}

void glstBegin(State &, GLenum topology)
{
}

void glstBindTexture(State &s, GLenum target, GLuint texture)
{
    if (texture >= s.mLastUsedTextureName)
    {
        s.mLastUsedTextureName = texture + 1;
    }

    TextureUnit &texUnit = s.mTexUnit[s.mActiveTexture - GL_TEXTURE0];

    switch (target)
    {
    case GL_TEXTURE_1D:
        texUnit.mNamedTexture1d = texture;
        break;
    case GL_TEXTURE_2D:
        texUnit.mNamedTexture2d = texture;
        break;
    case GL_TEXTURE_CUBE_MAP:
        texUnit.mNamedTextureCube = texture;
        break;
    case GL_TEXTURE_3D:
        texUnit.mNamedTexture3d = texture;
        break;
    }
}

void glstBlendFunc(State &s, GLenum sfactor, GLenum dfactor)
{
    s.mBlendFuncSFactor = sfactor;
    s.mBlendFuncDFactor = dfactor;
}

void glstCallList(State &, GLuint list)
{
}

void glstClear(State &, GLbitfield mask)
{
}

void glstClearColor(State &s, GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
    s.mClearColor[0] = SWRL::clamp(red, 0.0f, 1.0f);
    s.mClearColor[1] = SWRL::clamp(green, 0.0f, 1.0f);
    s.mClearColor[2] = SWRL::clamp(blue, 0.0f, 1.0f);
    s.mClearColor[3] = SWRL::clamp(alpha, 0.0f, 1.0f);
}

void glstClearDepth(State &s, GLclampd depth)
{
    s.mClearDepth = SWRL::clamp(depth, 0.0, 1.0);
}

void glstClientActiveTexture(State &s, GLenum texture)
{
    s.mClientActiveTexture = texture;
}

void glstColor(State &s, GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
    s.mColor[0][0] = (GLfloat)red;
    s.mColor[0][1] = (GLfloat)green;
    s.mColor[0][2] = (GLfloat)blue;
    s.mColor[0][3] = (GLfloat)alpha;
}

void glstColorMask(State &s, GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
{
    s.mColorMask.red = red;
    s.mColorMask.green = green;
    s.mColorMask.blue = blue;
    s.mColorMask.alpha = alpha;
}

void glstColorPointer(State &s, GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
    s.mArrayPointers.mColor[0].mStride = stride == 0 ? size * _glstEnumTypeToSize(type) : stride;
    s.mArrayPointers.mColor[0].mNumCoordsPerAttribute = size;
    s.mArrayPointers.mColor[0].mType = type;
    s.mArrayPointers.mColor[0].mpElements = pointer;

    s.mActiveVBOs.color[0] = s.mActiveArrayVBO;
}

void glstCullFace(State &s, GLenum cullFace)
{
    s.mCullFace = cullFace;
}

void glstDeleteLists(State &s, GLuint list, GLsizei range)
{
    for (GLsizei i = (GLsizei)list; i < (GLsizei)list + range; ++i)
    {
        if (DisplayLists().find(i) != DisplayLists().end())
        {
            delete DisplayLists()[i];
            DisplayLists().erase(i);
        }
    }
}

void glstDepthFunc(State &s, GLenum func)
{
    s.mDepthFunc = func;
}

void glstDepthMask(State &s, GLboolean flag)
{
    s.mDepthMask = flag;
}

void glstDepthRange(State &s, GLclampd zNear, GLclampd zFar)
{
    s.mViewport.zNear = (GLfloat)SWRL::clamp(zNear, 0.0, 1.0);
    s.mViewport.zFar = (GLfloat)SWRL::clamp(zFar, 0.0, 1.0);
}

void glstDisable(State &s, GLenum cap)
{
    switch (cap)
    {
    case GL_ALPHA_TEST:
        s.mCaps.alphatest = 0;
        break;
    case GL_BLEND:
        s.mCaps.blend = 0;
        break;
    case GL_CLIP_PLANE0:
    case GL_CLIP_PLANE1:
    case GL_CLIP_PLANE2:
    case GL_CLIP_PLANE3:
    case GL_CLIP_PLANE4:
    case GL_CLIP_PLANE5:
        s.mCaps.clipPlanes &= ~(0x1 << (cap - GL_CLIP_PLANE0));
        break;
    case GL_COLOR_ARRAY:
        s.mCaps.colorArray &= ~0x1;
        break;
    case GL_CULL_FACE:
        s.mCaps.cullFace = 0;
        break;
    case GL_DEPTH_TEST:
        s.mCaps.depthTest = 0;
        break;
    case GL_FOG:
        s.mCaps.fog = 0;
        break;
    //case GL_FOG_COORD_ARRAY			: s.mCaps.fogArray	= 0; break;
    case GL_LIGHTING:
        s.mCaps.lighting = 0;
        break;
    case GL_LIGHT0:
    case GL_LIGHT1:
    case GL_LIGHT2:
    case GL_LIGHT3:
    case GL_LIGHT4:
    case GL_LIGHT5:
    case GL_LIGHT6:
    case GL_LIGHT7:
        s.mCaps.light &= ~(0x1 << (cap - GL_LIGHT0));
        break;
    case GL_NORMAL_ARRAY:
        s.mCaps.normalArray = 0;
        break;
    case GL_NORMALIZE:
        s.mCaps.normalize = 0;
        break;
    case GL_SCISSOR_TEST:
        s.mCaps.scissorTest = 0;
        break;
    case GL_TEXTURE_COORD_ARRAY:
        s.mCaps.texCoordArray &= ~(0x1 << (s.mClientActiveTexture - GL_TEXTURE0));
        break;
    case GL_VERTEX_ARRAY:
        s.mCaps.vertexArray = 0;
        break;
    case GL_TEXTURE_2D:
        s.mCaps.textures &= ~(0x1 << (s.mActiveTexture - GL_TEXTURE0));
        break;

#ifdef GL_VERSION_1_4
    case GL_SECONDARY_COLOR_ARRAY:
        s.mCaps.colorArray &= ~0x2;
        break;
#endif

    default:
        s.mLastError = GL_INVALID_ENUM;
        break;
    }
}

void glstDisableClientState(State &, GLenum cap)
{
}

void glstDrawArrays(State &, GLenum mode, GLint first, GLsizei count)
{
}

void glstDrawElements(State &, GLenum mode, GLsizei count, GLenum type, const GLvoid *indices)
{
}

void glstDrawSWR(State &)
{
}

void glstEnable(State &s, GLenum cap)
{
    switch (cap)
    {
    case GL_ALPHA_TEST:
        s.mCaps.alphatest = 1;
        break;
    case GL_BLEND:
        s.mCaps.blend = 1;
        break;
    case GL_CLIP_PLANE0:
    case GL_CLIP_PLANE1:
    case GL_CLIP_PLANE2:
    case GL_CLIP_PLANE3:
    case GL_CLIP_PLANE4:
    case GL_CLIP_PLANE5:
        s.mCaps.clipPlanes |= (0x1 << (cap - GL_CLIP_PLANE0));
        break;
    case GL_COLOR_ARRAY:
        s.mCaps.colorArray |= 0x1;
        break;
    case GL_CULL_FACE:
        s.mCaps.cullFace = 1;
        break;
    case GL_DEPTH_TEST:
        s.mCaps.depthTest = 1;
        break;
    case GL_FOG:
        s.mCaps.fog = 1;
        break;
    //case GL_FOG_COORD_ARRAY			: s.mCaps.fogArray	= 1; break;
    case GL_LIGHTING:
        s.mCaps.lighting = 1;
        break;
    case GL_LIGHT0:
    case GL_LIGHT1:
    case GL_LIGHT2:
    case GL_LIGHT3:
    case GL_LIGHT4:
    case GL_LIGHT5:
    case GL_LIGHT6:
    case GL_LIGHT7:
        s.mCaps.light |= 0x1 << (cap - GL_LIGHT0);
        break;
    case GL_NORMAL_ARRAY:
        s.mCaps.normalArray = 1;
        break;
    case GL_NORMALIZE:
        s.mCaps.normalize = 1;
        break;
    case GL_SCISSOR_TEST:
        s.mCaps.scissorTest = 1;
        break;
    case GL_TEXTURE_2D:
        s.mCaps.textures |= (0x1 << (s.mActiveTexture - GL_TEXTURE0));
        break;
    case GL_TEXTURE_COORD_ARRAY:
        s.mCaps.texCoordArray |= (0x1 << (s.mClientActiveTexture - GL_TEXTURE0));
        break;
    case GL_VERTEX_ARRAY:
        s.mCaps.vertexArray = 1;
        break;

#ifdef GL_VERSION_1_4
    case GL_SECONDARY_COLOR_ARRAY:
        s.mCaps.colorArray |= 0x2;
        break;
#endif

    default:
        s.mLastError = GL_INVALID_ENUM;
        break;
    }
}

void glstEnableClientState(State &, GLenum cap)
{
}

void glstEnd(State &)
{
}

void glstEndList(State &)
{
}

void glstFogfv(State &s, GLenum pname, const GLfloat *params)
{
    switch (pname)
    {
    case GL_FOG_COLOR:
        s.mFog.mColor[0] = params[0];
        s.mFog.mColor[1] = params[1];
        s.mFog.mColor[2] = params[2];
        s.mFog.mColor[3] = params[3];
        break;
    }
}

void glstFogf(State &s, GLenum pname, GLfloat param)
{
    switch (pname)
    {
    case GL_FOG_START:
        s.mFog.mStart = param;
        break;
    case GL_FOG_END:
        s.mFog.mEnd = param;
        break;
    }
}

void glstFogi(State &s, GLenum pname, GLint param)
{
    switch (pname)
    {
    case GL_FOG_MODE:
        s.mFog.mMode = param;
        break;
    }
}

void glstFrontFace(State &s, GLenum frontFace)
{
    s.mFrontFace = frontFace;
}

void glstFrustum(State &s, GLdouble xmin, GLdouble xmax, GLdouble ymin, GLdouble ymax, GLdouble zNear, GLdouble zFar)
{
    SWRL::m44d F;

    F[0][0] = (2 * zNear) / (xmax - xmin);
    F[1][0] = 0.0f;
    F[2][0] = 0.0f;
    F[3][0] = 0.0f;

    F[0][1] = 0.0f;
    F[1][1] = (2 * zNear) / (ymax - ymin);
    F[2][1] = 0.0f;
    F[3][1] = 0.0f;

    F[0][2] = (xmax + xmin) / (xmax - xmin);
    F[1][2] = (ymax + ymin) / (ymax - ymin);
    F[2][2] = -(zFar + zNear) / (zFar - zNear);
    F[3][2] = -1.0f,

    F[0][3] = 0.0f;
    F[1][3] = 0.0f;
    F[2][3] = -(2 * zFar * zNear) / (zFar - zNear);
    F[3][3] = 0.0f;

    UpdateCurrentMatrix(s, SWRL::mmult(CurrentMatrix(s), F));
}

GLuint glstGenLists(State &, GLsizei range)
{
    return 0;
}

void glstGenTextures(State &s, GLsizei num, GLuint *textures)
{
}

void glstGetBooleanv(State &s, GLenum pname, GLboolean *params)
{
    // _glstGetGLTyv doesn't handle anything but GLfloat matrices correctly!!!
    switch (pname)
    {
    case GL_PROJECTION_MATRIX:
    case GL_TEXTURE_MATRIX:
    case GL_MODELVIEW_MATRIX:
        GLfloat temp_matrix[16];
        _glstGetGLTyv(s, pname, temp_matrix);
        for (auto i = 0; i < 16; i++)
            params[i] = (GLboolean)(temp_matrix[i] == 0.0 ? GL_FALSE : GL_TRUE); // This isn't a real performance case here!
        break;
    default:
        _glstGetGLTyv(s, pname, params);
        break;
    }
}

void glstGetDoublev(State &s, GLenum pname, GLdouble *params)
{
    // _glstGetGLTyv doesn't handle anything but GLfloat matrices correctly!!!
    switch (pname)
    {
    case GL_PROJECTION_MATRIX:
    case GL_TEXTURE_MATRIX:
    case GL_MODELVIEW_MATRIX:
        GLfloat temp_matrix[16];
        _glstGetGLTyv(s, pname, temp_matrix);
        for (auto i = 0; i < 16; i++)
            params[i] = (GLdouble)temp_matrix[i];
        break;
    default:
        _glstGetGLTyv(s, pname, params);
        break;
    }
}

void glstGetFloatv(State &s, GLenum pname, GLfloat *params)
{
    _glstGetGLTyv(s, pname, params);
}

void glstGetIntegerv(State &s, GLenum pname, GLint *params)
{
    // _glstGetGLTyv doesn't handle anything but GLfloat matrices correctly!!!
    switch (pname)
    {
    case GL_PROJECTION_MATRIX:
    case GL_TEXTURE_MATRIX:
    case GL_MODELVIEW_MATRIX:
        GLfloat temp_matrix[16];
        _glstGetGLTyv(s, pname, temp_matrix);
        for (auto i = 0; i < 16; i++)
            params[i] = (GLint)(temp_matrix[i] * INT_MAX);
        break;
    default:
        _glstGetGLTyv(s, pname, params);
        break;
    }
}

void glstGetLightfv(State &s, GLenum light, GLenum pname, GLfloat *params)
{
    LightSourceParameters &rLight = s.mLightSource[light - GL_LIGHT0];

    switch (pname)
    {
    case GL_AMBIENT:
        params[0] = rLight.mAmbient[0];
        params[1] = rLight.mAmbient[1];
        params[2] = rLight.mAmbient[2];
        params[3] = rLight.mAmbient[3];
        break;
    case GL_DIFFUSE:
        params[0] = rLight.mDiffuse[0];
        params[1] = rLight.mDiffuse[1];
        params[2] = rLight.mDiffuse[2];
        params[3] = rLight.mDiffuse[3];
        break;
    case GL_SPECULAR:
        params[0] = rLight.mSpecular[0];
        params[1] = rLight.mSpecular[1];
        params[2] = rLight.mSpecular[2];
        params[3] = rLight.mSpecular[3];
        break;
    case GL_POSITION:
        params[0] = rLight.mPosition[0];
        params[1] = rLight.mPosition[1];
        params[2] = rLight.mPosition[2];
        params[3] = rLight.mPosition[3];
        break;
    case GL_SPOT_DIRECTION:
        params[0] = rLight.mSpotDirection[0];
        params[1] = rLight.mSpotDirection[1];
        params[2] = rLight.mSpotDirection[2];
        break;
    case GL_SPOT_EXPONENT:
        params[0] = rLight.mSpotExponent;
        break;
    case GL_SPOT_CUTOFF:
        params[0] = rLight.mSpotCutoff;
        break;
    case GL_CONSTANT_ATTENUATION:
        params[0] = rLight.mConstantAttenuation;
        break;
    case GL_LINEAR_ATTENUATION:
        params[0] = rLight.mLinearAttenuation;
        break;
    case GL_QUADRATIC_ATTENUATION:
        params[0] = rLight.mQuadraticAttenuation;
        break;
    }
}

void glstGetLightiv(State &s, GLenum light, GLenum pname, GLint *params)
{
    GLfloat fParams[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    glstGetLightfv(s, light, pname, &fParams[0]);

    switch (pname)
    {
    case GL_AMBIENT:
    case GL_DIFFUSE:
    case GL_SPECULAR:
    case GL_POSITION:
    case GL_SPOT_DIRECTION:
        params[0] = (GLint)(fParams[0] * INT_MAX);
        params[1] = (GLint)(fParams[1] * INT_MAX);
        params[2] = (GLint)(fParams[2] * INT_MAX);
        params[3] = (GLint)(fParams[3] * INT_MAX);
        break;
    case GL_SPOT_EXPONENT:
    case GL_SPOT_CUTOFF:
    case GL_CONSTANT_ATTENUATION:
    case GL_LINEAR_ATTENUATION:
    case GL_QUADRATIC_ATTENUATION:
        params[0] = (GLint)fParams[0];
        params[1] = (GLint)fParams[1];
        params[2] = (GLint)fParams[2];
        params[3] = (GLint)fParams[3];
        break;
    }
}

void glstGetMaterialfv(State &s, GLenum face, GLenum pname, GLfloat *params)
{
    OGL::MaterialParameters &material = (face == GL_FRONT) ? s.mFrontMaterial : s.mBackMaterial;

    switch (pname)
    {
    case GL_AMBIENT:
        params[0] = material.mAmbient[0];
        params[1] = material.mAmbient[1];
        params[2] = material.mAmbient[2];
        params[3] = material.mAmbient[3];
        break;
    case GL_DIFFUSE:
        params[0] = material.mDiffuse[0];
        params[1] = material.mDiffuse[1];
        params[2] = material.mDiffuse[2];
        params[3] = material.mDiffuse[3];
        break;
    case GL_SPECULAR:
        params[0] = material.mSpecular[0];
        params[1] = material.mSpecular[1];
        params[2] = material.mSpecular[2];
        params[3] = material.mSpecular[3];
        break;
    case GL_EMISSION:
        params[0] = material.mEmission[0];
        params[1] = material.mEmission[1];
        params[2] = material.mEmission[2];
        params[3] = material.mEmission[3];
        break;
    case GL_SHININESS:
        params[0] = material.mSpecular[0];
        break;
    default:
        break;
    }
}

void glstGetMaterialiv(State &s, GLenum face, GLenum pname, GLint *params)
{
    GLfloat fParams[4] = { 0 };
    glstGetMaterialfv(s, face, pname, &fParams[0]);

    switch (pname)
    {
    case GL_AMBIENT:
    case GL_DIFFUSE:
    case GL_SPECULAR:
    case GL_EMISSION:
        params[0] = (GLint)(fParams[0] * INT_MAX);
        params[1] = (GLint)(fParams[1] * INT_MAX);
        params[2] = (GLint)(fParams[2] * INT_MAX);
        params[3] = (GLint)(fParams[3] * INT_MAX);
        break;
    case GL_SHININESS:
        params[0] = (GLint)(fParams[0]);
        break;
    default:
        break;
    }
}

#define MAX_VERSION_STRING 256
static GLubyte gVersionString[MAX_VERSION_STRING];

const GLubyte *glstGetString(State &, GLenum name)
{
    switch (name)
    {
    case GL_VENDOR:
        return (const GLubyte *)"Intel";
    case GL_RENDERER:
        return (const GLubyte *)"SWR";
    case GL_VERSION:
    {
        snprintf((char *)gVersionString, MAX_VERSION_STRING,
                 "1.3 %u:%u:%u:%s%s",
                 GetDDProcTable().pfnGetThreadCount(GetDDHandle()),
                 KNOB_MAX_DRAWS_IN_FLIGHT,
                 KNOB_MAX_PRIMS_PER_DRAW,
                 KNOB_VS_SIMD_WIDTH == 4 ? "SSE" : "AVX",
                 KNOB_ENABLE_NUMA ? ":NUMA" : "");
        return gVersionString;
    }
    case GL_EXTENSIONS:
        return (const GLubyte *)"GL_EXT_compiled_vertex_array GL_ARB_vertex_buffer_object";
    default:
        assert(0);
    }
    return (const GLubyte *)"";
}

void glstGetTexLevelParameterfv(State &s, GLenum target, GLint level, GLenum pname, GLfloat *params)
{
    _glstGetTexLevelParameterTyv(s, target, level, pname, params);
}

void glstGetTexLevelParameteriv(State &s, GLenum target, GLint level, GLenum pname, GLint *params)
{
    _glstGetTexLevelParameterTyv(s, target, level, pname, params);
}

void glstHint(State &, GLenum target, GLenum mode)
{
}

GLboolean glstIsEnabled(State &s, GLenum cap)
{
    switch (cap)
    {
    case GL_ALPHA_TEST:
        return (s.mCaps.alphatest) != 0;
    case GL_BLEND:
        return (s.mCaps.blend) != 0;
    case GL_CLIP_PLANE0:
    case GL_CLIP_PLANE1:
    case GL_CLIP_PLANE2:
    case GL_CLIP_PLANE3:
    case GL_CLIP_PLANE4:
    case GL_CLIP_PLANE5:
        return (s.mCaps.clipPlanes & (0x1 << (cap - GL_CLIP_PLANE0))) != 0;
    case GL_CULL_FACE:
        return (s.mCaps.cullFace) != 0;
    case GL_DEPTH_TEST:
        return (s.mCaps.depthTest) != 0;
    case GL_FOG:
        return (s.mCaps.fog & 0x1) != 0;
    case GL_LIGHT0:
    case GL_LIGHT1:
    case GL_LIGHT2:
    case GL_LIGHT3:
    case GL_LIGHT4:
    case GL_LIGHT5:
    case GL_LIGHT6:
    case GL_LIGHT7:
        return (s.mCaps.light & (0x1 << (cap - GL_LIGHT0))) != 0;
    case GL_LIGHTING:
        return (s.mCaps.lighting & 0x1) != 0;
    case GL_MULTISAMPLE:
        return (s.mCaps.multisample & 0x1) != 0;
    case GL_NORMALIZE:
        return (s.mCaps.normalize & 0x1) != 0;
    case GL_RESCALE_NORMAL:
        return (s.mCaps.rescaleNormal & 0x1) != 0;
    case GL_TEXTURE_2D:
        return (s.mCaps.textures & (0x1 << (s.mActiveTexture - GL_TEXTURE0))) != 0;
    case GL_TEXTURE_GEN_S:
        return (s.mCaps.texGenS & (0x1 << (s.mActiveTexture - GL_TEXTURE0))) != 0;
    case GL_TEXTURE_GEN_T:
        return (s.mCaps.texGenT & (0x1 << (s.mActiveTexture - GL_TEXTURE0))) != 0;
    case GL_TEXTURE_GEN_R:
        return (s.mCaps.texGenR & (0x1 << (s.mActiveTexture - GL_TEXTURE0))) != 0;
    case GL_TEXTURE_GEN_Q:
        return (s.mCaps.texGenQ & (0x1 << (s.mActiveTexture - GL_TEXTURE0))) != 0;
    default:
        s.mLastError = GL_INVALID_ENUM;
        break;
    }

    return 0;
}

void glstLight(State &s, GLenum light, GLenum pname, std::array<GLfloat, 4> const &params, bool isScalar)
{
    LightSourceParameters &rLight = s.mLightSource[light - GL_LIGHT0];

    switch (pname)
    {
    case GL_AMBIENT:
        rLight.mAmbient[0] = (GLfloat)params[0];
        rLight.mAmbient[1] = (GLfloat)params[1];
        rLight.mAmbient[2] = (GLfloat)params[2];
        rLight.mAmbient[3] = (GLfloat)params[3];
        break;
    case GL_DIFFUSE:
        rLight.mDiffuse[0] = (GLfloat)params[0];
        rLight.mDiffuse[1] = (GLfloat)params[1];
        rLight.mDiffuse[2] = (GLfloat)params[2];
        rLight.mDiffuse[3] = (GLfloat)params[3];
        break;
    case GL_SPECULAR:
        rLight.mSpecular[0] = (GLfloat)params[0];
        rLight.mSpecular[1] = (GLfloat)params[1];
        rLight.mSpecular[2] = (GLfloat)params[2];
        rLight.mSpecular[3] = (GLfloat)params[3];
        break;
    case GL_POSITION:
    {
        std::array<GLfloat, 4> p;
        memcpy(&p[0], &params[0], sizeof(GLfloat) * 4);
        auto transformedP = SWRL::mvmult(s.mMatrices[OGL::MODELVIEW].top(), p);
        memcpy(&rLight.mPosition[0], &transformedP[0], sizeof(transformedP));

        // for directional light src, need to normalize the light vector
        if (params[3] == 0.0)
        {
            float length = sqrt(rLight.mPosition[0] * rLight.mPosition[0] + rLight.mPosition[1] * rLight.mPosition[1] +
                                rLight.mPosition[2] * rLight.mPosition[2]);
            rLight.mPosition[0] /= length;
            rLight.mPosition[1] /= length;
            rLight.mPosition[2] /= length;
        }
    }
    break;
    case GL_SPOT_DIRECTION:
        rLight.mSpotDirection[0] = (GLfloat)params[0];
        rLight.mSpotDirection[1] = (GLfloat)params[1];
        rLight.mSpotDirection[2] = (GLfloat)params[2];
        rLight.mSpotDirection[3] = (GLfloat)params[3];
        break;
    case GL_SPOT_EXPONENT:
        rLight.mSpotExponent = (GLfloat)params[0];
        break;
    case GL_SPOT_CUTOFF:
        rLight.mSpotCutoff = (GLfloat)params[0];
        break;
    case GL_CONSTANT_ATTENUATION:
        rLight.mConstantAttenuation = (GLfloat)params[0];
        break;
    case GL_LINEAR_ATTENUATION:
        rLight.mLinearAttenuation = (GLfloat)params[0];
        break;
    case GL_QUADRATIC_ATTENUATION:
        rLight.mQuadraticAttenuation = (GLfloat)params[0];
        break;
    }
}

void glstLightModel(State &s, GLenum pname, std::array<GLfloat, 4> const &params, bool isScalar)
{
    switch (pname)
    {
    case GL_LIGHT_MODEL_AMBIENT:
        s.mLightModel.mAmbient[0] = (GLfloat)params[0];
        s.mLightModel.mAmbient[1] = (GLfloat)params[1];
        s.mLightModel.mAmbient[2] = (GLfloat)params[2];
        s.mLightModel.mAmbient[3] = (GLfloat)params[3];
        break;
    case GL_LIGHT_MODEL_COLOR_CONTROL:
        s.mCaps.specularColor = GL_SINGLE_COLOR == params[0] ? 0 : 1;
        break;
    case GL_LIGHT_MODEL_LOCAL_VIEWER:
        s.mCaps.localViewer = params[0] == 0 ? 0 : 1;
        break;
    case GL_LIGHT_MODEL_TWO_SIDE:
        s.mCaps.twoSided = params[0] == 0 ? 0 : 1;
        break;
    }
}

void glstLoadIdentity(State &s)
{
    UpdateCurrentMatrix(s, SWRL::M44Id<GLfloat>());
}

void glstLoadMatrix(State &s, SWRL::m44f const &m)
{
    UpdateCurrentMatrix(s, m);
}

void glstLockArraysEXT(State &s, GLint first, GLsizei count)
{
}

void glstMaterial(State &s, GLenum face, GLenum pname, std::array<GLfloat, 4> const &params, bool isScalar)
{
    OGL::MaterialParameters *pMPs = 0;
    switch (face)
    {
    case GL_FRONT:
        pMPs = &s.mFrontMaterial;
        break;
    case GL_BACK:
        pMPs = &s.mBackMaterial;
        break;
    case GL_FRONT_AND_BACK:
        glstMaterial(s, GL_FRONT, pname, params, isScalar);
        glstMaterial(s, GL_BACK, pname, params, isScalar);
        return;
    }

    switch (pname)
    {
    case GL_AMBIENT:
        pMPs->mAmbient[0] = (GLfloat)params[0];
        pMPs->mAmbient[1] = (GLfloat)params[1];
        pMPs->mAmbient[2] = (GLfloat)params[2];
        pMPs->mAmbient[3] = (GLfloat)params[3];
        break;
    case GL_DIFFUSE:
        pMPs->mDiffuse[0] = (GLfloat)params[0];
        pMPs->mDiffuse[1] = (GLfloat)params[1];
        pMPs->mDiffuse[2] = (GLfloat)params[2];
        pMPs->mDiffuse[3] = (GLfloat)params[3];
        break;
    case GL_SPECULAR:
        pMPs->mSpecular[0] = (GLfloat)params[0];
        pMPs->mSpecular[1] = (GLfloat)params[1];
        pMPs->mSpecular[2] = (GLfloat)params[2];
        pMPs->mSpecular[3] = (GLfloat)params[3];
        break;
    case GL_EMISSION:
        pMPs->mEmission[0] = (GLfloat)params[0];
        pMPs->mEmission[1] = (GLfloat)params[1];
        pMPs->mEmission[2] = (GLfloat)params[2];
        pMPs->mEmission[3] = (GLfloat)params[3];
        break;
    case GL_SHININESS:
        pMPs->mShininess = (GLfloat)params[0];
        break;
    case GL_AMBIENT_AND_DIFFUSE:
        glstMaterial(s, face, GL_AMBIENT, params, isScalar);
        glstMaterial(s, face, GL_DIFFUSE, params, isScalar);
        break;
    case GL_COLOR_INDEXES:
        // XXX: Oh, OGL1! Thou art so kerazee!
        break;
    }
}

void glstMatrixMode(State &s, GLenum mode)
{
    if (mode == GL_MODELVIEW || mode == GL_PROJECTION || mode == GL_TEXTURE || mode == GL_COLOR)
    {
        s.mMatrixMode = mode;
    }
}

void glstMultMatrix(State &s, SWRL::m44f const &m)
{
    UpdateCurrentMatrix(s, SWRL::mmult(CurrentMatrix(s), m));
}

void glstNewList(State &, GLuint list, GLenum mode)
{
}

void glstNormal(State &s, GLfloat nx, GLfloat ny, GLfloat nz)
{
    s.mNormal[0] = (GLfloat)nx;
    s.mNormal[1] = (GLfloat)ny;
    s.mNormal[2] = (GLfloat)nz;
    s.mNormal[3] = 0.0f;

    assert(s.mCaps.normalize == 0);
    // s.mNormalizedNormal not used
    //	_mm_storeu_ps(&s.mNormalizedNormal[0], _mm_div_ps(N, _mm_sqrt_ps(_mm_dp_ps(N, N, 0xFF))));
}

void glstNormalPointer(State &s, GLenum type, GLsizei stride, const GLvoid *pointer)
{
    s.mArrayPointers.mNormal.mStride = stride == 0 ? 3 * _glstEnumTypeToSize(type) : stride;
    s.mArrayPointers.mNormal.mNumCoordsPerAttribute = 3;
    s.mArrayPointers.mNormal.mType = type;
    s.mArrayPointers.mNormal.mpElements = pointer;

    s.mActiveVBOs.normal = s.mActiveArrayVBO;
}

void glstNumVerticesSWR(State &, GLuint num, VertexAttributeFormats count)
{
}

void glstOrtho(State &s, GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar)
{
    SWRL::m44d M;

    M[0][0] = 2.0f / (right - left);
    M[0][1] = 0.0f;
    M[0][2] = 0.0f;
    M[0][3] = -(right + left) / (right - left);

    M[1][0] = 0.0;
    M[1][1] = 2.0f / (top - bottom);
    M[1][2] = 0.0;
    M[1][3] = -(top + bottom) / (top - bottom);

    M[2][0] = 0.0f;
    M[2][1] = 0.0f;
    M[2][2] = -2.0f / (zFar - zNear);
    M[2][3] = -(zFar + zNear) / (zFar - zNear);

    M[3][0] = 0.0f;
    M[3][1] = 0.0f;
    M[3][2] = 0.0f;
    M[3][3] = 1.0f;

    UpdateCurrentMatrix(s, SWRL::mmult(CurrentMatrix(s), M));
}

void glstPixelStorei(State &s, GLenum pname, GLint param)
{
    // XXX, only supports GL_PACK_ROW_LENGTH for now
    switch (pname)
    {
    case GL_PACK_ROW_LENGTH:
        if (param < 0)
        {
            s.mLastError = GL_INVALID_VALUE;
            break;
        }
        s.mPixelStore.mPackRowLength = param;
        break;
    case GL_PACK_SWAP_BYTES:
    case GL_PACK_LSB_FIRST:
    case GL_PACK_IMAGE_HEIGHT:
    case GL_PACK_SKIP_ROWS:
    case GL_PACK_SKIP_PIXELS:
    case GL_PACK_SKIP_IMAGES:
    case GL_PACK_ALIGNMENT:
    case GL_UNPACK_SWAP_BYTES:
    case GL_UNPACK_LSB_FIRST:
    case GL_UNPACK_ROW_LENGTH:
    case GL_UNPACK_IMAGE_HEIGHT:
    case GL_UNPACK_SKIP_ROWS:
    case GL_UNPACK_SKIP_PIXELS:
    case GL_UNPACK_SKIP_IMAGES:
    case GL_UNPACK_ALIGNMENT:
        break;
    default:
        s.mLastError = GL_INVALID_ENUM;
        assert(0);
        break;
    }
}

void glstPolygonMode(State &s, GLenum face, GLenum mode)
{
    switch (face)
    {
    case GL_FRONT:
        s.mPolygonMode[0] = mode;
        break;
    case GL_BACK:
        s.mPolygonMode[1] = mode;
        break;
    case GL_FRONT_AND_BACK:
        s.mPolygonMode[0] = mode;
        s.mPolygonMode[1] = mode;
        break;
    default:
        break;
    }
}

void glstPopAttrib(State &s)
{
    if (s.mAttribStack.empty())
    {
        s.mLastError = GL_STACK_UNDERFLOW;
        return;
    }

    auto ss = s.mAttribStack.top(); // Get pushed attribute state.
    s.mAttribStack.pop();

    GLbitfield mask = ss.first; // Mask that attribs were pushed with.

#undef GPACPY
#undef GPACPYX
#define GPACPY(EXPR) s.EXPR = ss.second->EXPR
#define GPACPYX(EXPR) memcpy(&s.EXPR[0], &ss.second->EXPR[0], sizeof(s.EXPR))

    if (mask & GL_ACCUM_BUFFER_BIT)
    {
        SWR_UNIMPLEMENTED;
    }
    if (mask & GL_COLOR_BUFFER_BIT)
    {
        GPACPY(mCaps.alphatest);
        GPACPY(mAlphaFunc);
        GPACPY(mAlphaRef);
        GPACPY(mCaps.blend);
        GPACPY(mBlendFuncDFactor);
        GPACPY(mBlendFuncSFactor);
    }
    if (mask & GL_CURRENT_BIT)
    {
        GPACPYX(mColor);
        GPACPY(mNormal);
        GPACPYX(mTexCoord[0]);
    }
    if (mask & GL_DEPTH_BUFFER_BIT)
    {
        GPACPY(mCaps.depthTest);
        GPACPY(mDepthFunc);
        GPACPY(mDepthMask);
        GPACPY(mClearDepth);
    }
    if (mask & GL_ENABLE_BIT)
    {
        GPACPY(mCaps.alphatest);
        GPACPY(mCaps.blend);
        GPACPY(mCaps.clipPlanes);
        GPACPY(mCaps.cullFace);
        GPACPY(mCaps.depthTest);
        GPACPY(mCaps.fog);
        GPACPY(mCaps.lighting);
        GPACPY(mCaps.light);
        GPACPY(mCaps.multisample);
        GPACPY(mCaps.normalize);
        GPACPY(mCaps.scissorTest);
        GPACPY(mCaps.texGenS);
        GPACPY(mCaps.texGenT);
        GPACPY(mCaps.texGenR);
        GPACPY(mCaps.texGenQ);
        GPACPY(mCaps.textures);
    }
    if (mask & GL_LIGHTING_BIT)
    {
        GPACPY(mLightModel);
        GPACPY(mCaps.localViewer);
        GPACPY(mCaps.twoSided);
        GPACPY(mCaps.lighting);
        GPACPY(mCaps.light);
        GPACPYX(mLightSource);
        GPACPY(mFrontMaterial);
        GPACPY(mBackMaterial);
        GPACPY(mCaps.specularColor);
        GPACPY(mShadeModel);
    }
    if (mask & GL_EVAL_BIT)
    {
        SWR_UNIMPLEMENTED;
    }
    if (mask & GL_FOG_BIT)
    {
        GPACPY(mCaps.fog);
        GPACPY(mFog); // Struct includes mMode, mDensity, mStart, mEnd, mColor, mCoordSrc,
    }
    if (mask & GL_HINT_BIT)
    {
        SWR_UNIMPLEMENTED;
    }
    if (mask & GL_LINE_BIT)
    {
        SWR_UNIMPLEMENTED;
    }
    if (mask & GL_LIST_BIT)
    {
        SWR_UNIMPLEMENTED;
    }
    if (mask & GL_MULTISAMPLE_BIT)
    {
        SWR_UNIMPLEMENTED;
    }
    if (mask & GL_PIXEL_MODE_BIT)
    {
        SWR_UNIMPLEMENTED;
        /* BMCTODO XXX, add these
		GL_RED_BIAS and GL_RED_SCALE settings
		GL_GREEN_BIAS and GL_GREEN_SCALE values
		GL_BLUE_BIAS and GL_BLUE_SCALE
		GL_ALPHA_BIAS and GL_ALPHA_SCALE
		GL_DEPTH_BIAS and GL_DEPTH_SCALE
		GL_INDEX_OFFSET and GL_INDEX_SHIFT values
		GL_MAP_COLOR and GL_MAP_STENCIL flags
		GL_ZOOM_X and GL_ZOOM_Y factors
		GL_READ_BUFFER setting
		*/
    }
    if (mask & GL_POINT_BIT)
    {
        SWR_UNIMPLEMENTED;
    }
    if (mask & GL_POLYGON_BIT)
    {
        GPACPY(mCaps.cullFace);
        GPACPY(mCullFace);
        GPACPY(mFrontFace);
        GPACPYX(mPolygonMode);
    }
    if (mask & GL_POLYGON_STIPPLE_BIT)
    {
        SWR_UNIMPLEMENTED;
    }
    if (mask & GL_SCISSOR_BIT)
    {
        GPACPY(mCaps.scissorTest);
        GPACPY(mScissor);
    }
    if (mask & GL_STENCIL_BUFFER_BIT)
    {
        SWR_UNIMPLEMENTED;
    }
    if (mask & GL_TEXTURE_BIT)
    {
        // Probably stuff missing.
        GPACPY(mCaps.texGenS);
        GPACPY(mCaps.texGenT);
        GPACPY(mCaps.texGenR);
        GPACPY(mCaps.texGenQ);
        GPACPYX(mTexUnit);
    }
    if (mask & GL_TRANSFORM_BIT)
    {
        // Stuff missing.
        GPACPY(mCaps.clipPlanes);
        GPACPY(mMatrixMode);
        GPACPY(mCaps.normalize);
        GPACPY(mCaps.rescaleNormal);
    }
    if (mask & GL_VIEWPORT_BIT)
    {
        GPACPY(mViewport);
    }
#undef GPACPY
#undef GPACPYX

    ss.second->~SaveableState();
    _aligned_free(ss.second);
}

void glstPopMatrix(State &s)
{
    PopMatrix(s);
}

void glstPushAttrib(State &s, GLbitfield mask)
{
    void *mem = _aligned_malloc(sizeof(SaveableState), KNOB_VS_SIMD_WIDTH * 4);
    SaveableState *state = new (mem) SaveableState(static_cast<SaveableState &>(s));
    s.mAttribStack.push(std::make_pair(mask, state));
}

void glstPushMatrix(State &s)
{
    PushMatrix(s, CurrentMatrix(s));
}

void glstResetMainVBSWR(State &s)
{
    s.mpDrawingVB = &s._mVertexBuffer;
}

void glstRotate(State &s, GLdouble O, GLdouble x, GLdouble y, GLdouble z)
{
    SWRL::m44d R;

    O = (float)3.1415926535897932384626433832795028841971 * O / 180;

    GLdouble norm = sqrt(x * x + y * y + z * z);
    x /= norm;
    y /= norm;
    z /= norm;

    R[0][0] = x * x * (1 - cos(O)) + cos(O);
    R[1][0] = x * y * (1 - cos(O)) + z * sin(O);
    R[2][0] = x * z * (1 - cos(O)) - y * sin(O);
    R[3][0] = 0.0;

    R[0][1] = y * x * (1 - cos(O)) - z * sin(O);
    R[1][1] = y * y * (1 - cos(O)) + cos(O);
    R[2][1] = y * z * (1 - cos(O)) + x * sin(O);
    R[3][1] = 0.0;

    R[0][2] = x * z * (1 - cos(O)) + y * sin(O);
    R[1][2] = y * z * (1 - cos(O)) - x * sin(O);
    R[2][2] = z * z * (1 - cos(O)) + cos(O);
    R[3][2] = 0.0;

    R[0][3] = 0.0;
    R[1][3] = 0.0;
    R[2][3] = 0.0;
    R[3][3] = 1.0;

    UpdateCurrentMatrix(s, SWRL::mmult(CurrentMatrix(s), R));
}

void glstScale(State &s, GLdouble x, GLdouble y, GLdouble z)
{
    SWRL::m44f sm = SWRL::M44Id<GLfloat>();
    ;
    sm[0][0] = (GLfloat)x;
    sm[1][1] = (GLfloat)y;
    sm[2][2] = (GLfloat)z;
    sm[3][3] = 1.0;

    UpdateCurrentMatrix(s, SWRL::mmult(CurrentMatrix(s), sm));
}

void glstScissor(State &s, GLint x, GLint y, GLsizei width, GLsizei height)
{
    GLint maxVP[2];
    glstGetIntegerv(s, GL_MAX_VIEWPORT_DIMS, maxVP);

    s.mScissor.x = x;
    s.mScissor.y = y;
    s.mScissor.width = SWRL::clamp(width, 0, maxVP[0]);
    s.mScissor.height = SWRL::clamp(height, 0, maxVP[1]);
}

void glstSetClearMaskSWR(State &s, GLbitfield mask)
{
    s.mClearMask = mask;
}

void glstSetTopologySWR(State &s, GLenum topology)
{
    s.mTopology = topology;
}

void glstSetUpDrawSWR(State &s, GLenum drawType)
{
}

void glstShadeModel(State &s, GLenum mode)
{
    s.mShadeModel = mode;
}

void glstSubstituteVBSWR(State &s, GLsizei which)
{
    s.mpDrawingVB = ExecutingDL(s).mVertexBuffers[which];
}

void glstTexCoord(State &state, GLenum texCoord, GLfloat s, GLfloat t, GLfloat r, GLfloat q)
{
    state.mTexCoord[texCoord - GL_TEXTURE0][0] = (GLfloat)s;
    state.mTexCoord[texCoord - GL_TEXTURE0][1] = (GLfloat)t;
    state.mTexCoord[texCoord - GL_TEXTURE0][2] = (GLfloat)r;
    state.mTexCoord[texCoord - GL_TEXTURE0][3] = (GLfloat)q;
}

void glstTexCoordPointer(State &s, GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
    s.mArrayPointers.mTexCoord[s.mClientActiveTexture - GL_TEXTURE0].mStride = stride == 0 ? size * _glstEnumTypeToSize(type) : stride;
    s.mArrayPointers.mTexCoord[s.mClientActiveTexture - GL_TEXTURE0].mNumCoordsPerAttribute = size;
    s.mArrayPointers.mTexCoord[s.mClientActiveTexture - GL_TEXTURE0].mType = type;
    s.mArrayPointers.mTexCoord[s.mClientActiveTexture - GL_TEXTURE0].mpElements = pointer;

    s.mActiveVBOs.texcoord[0] = s.mActiveArrayVBO;
}

void glstTexEnv(State &s, GLenum target, GLenum pname, std::array<GLfloat, 4> const &params)
{
    assert(target == GL_TEXTURE_ENV);

    TextureUnit &texUnit = s.mTexUnit[s.mActiveTexture - GL_TEXTURE0];
    switch (pname)
    {
    case GL_TEXTURE_ENV_MODE:
        texUnit.mTexEnv.mMode = (GLenum)params[0];
        break;
    case GL_TEXTURE_ENV_COLOR:
        memcpy(&texUnit.mTexEnv.mColor[0], &params[0], sizeof(GLfloat) * 4);
        break;
    default:
        assert(0 && "Unknown TexEnv pname");
    }
}

void glstTexImage2D(State &s, GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *data)
{
}

void glstTexParameter(State &s, GLenum target, GLenum pname, std::array<GLfloat, 4> const &params)
{
    assert(target == GL_TEXTURE_2D);

    TextureUnit &texUnit = s.mTexUnit[s.mActiveTexture - GL_TEXTURE0];
    TexParameters &txpm = s.mTexParameters[texUnit.mNamedTexture2d];
    switch (pname)
    {
    case GL_TEXTURE_MIN_FILTER:
        txpm.mMinFilter = (GLenum)params[0];
        break;
    case GL_TEXTURE_MAG_FILTER:
        txpm.mMagFilter = (GLenum)params[0];
        break;
    case GL_TEXTURE_MIN_LOD:
        txpm.mMinLOD = (GLint)params[0];
        break;
    case GL_TEXTURE_MAX_LOD:
        txpm.mMaxLOD = (GLint)params[0];
        break;
    case GL_TEXTURE_BASE_LEVEL:
        txpm.mBaseLevel = (GLint)params[0];
        break;
    case GL_TEXTURE_MAX_LEVEL:
        txpm.mMaxLevel = (GLint)params[0];
        break;
    case GL_TEXTURE_WRAP_S:
        txpm.mBorderMode[0] = (GLenum)params[0];
        break;
    case GL_TEXTURE_WRAP_T:
        txpm.mBorderMode[1] = (GLenum)params[0];
        break;
    case GL_TEXTURE_WRAP_R:
        txpm.mBorderMode[2] = (GLenum)params[0];
        break;
    case GL_TEXTURE_BORDER_COLOR:
        memcpy(&txpm.mBorderColor[0], &params[0], sizeof(GLfloat) * 4);
        break;
    case GL_TEXTURE_PRIORITY:
        txpm.mPriority = params[0];
        break;
#ifdef GL_VERSION_1_4
    case GL_TEXTURE_COMPARE_MODE:
        txpm.mCompareMode = (GLenum)params[0];
        break;
    case GL_TEXTURE_COMPARE_FUNC:
        txpm.mCompareFunc = (GLenum)params[0];
        break;
    case GL_DEPTH_TEXTURE_MODE:
        txpm.mDepthMode = (GLenum)params[0];
        break;
    case GL_GENERATE_MIPMAP:
        txpm.mGenMIPs = (GLboolean)params[0];
        break;
#endif
    default:
        s.mLastError = GL_INVALID_ENUM;
    }
}

void glstTexSubImage2D(State &, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels)
{
}

void glstTranslate(State &s, GLdouble x, GLdouble y, GLdouble z)
{
    SWRL::m44f T = SWRL::M44Id<GLfloat>();

    // Column major ... grrr.
    T[0][3] = (GLfloat)x;
    T[1][3] = (GLfloat)y;
    T[2][3] = (GLfloat)z;

    UpdateCurrentMatrix(s, SWRL::mmult(CurrentMatrix(s), T));
}

void glstUnlockArraysEXT(State &s)
{
    s.mArraysLocked = 0;
    s.mLockedCount = 0;
}

void glstVertex(State &, GLfloat x, GLfloat y, GLfloat z, GLfloat w, GLuint count)
{
}

void glstVertexPointer(State &s, GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
    s.mArrayPointers.mVertex.mStride = stride == 0 ? size * _glstEnumTypeToSize(type) : stride;
    s.mArrayPointers.mVertex.mNumCoordsPerAttribute = size;
    s.mArrayPointers.mVertex.mType = type;
    s.mArrayPointers.mVertex.mpElements = pointer;

    s.mActiveVBOs.vertex = s.mActiveArrayVBO;
}

void glstViewport(State &s, GLint x, GLint y, GLsizei width, GLsizei height)
{
    GLint maxVP[2];
    glstGetIntegerv(s, GL_MAX_VIEWPORT_DIMS, maxVP);

    s.mViewport.x = x;
    s.mViewport.y = y;
    s.mViewport.width = SWRL::clamp(width, 0, maxVP[0]);
    s.mViewport.height = SWRL::clamp(height, 0, maxVP[1]);
}

} // end namespace OGL
