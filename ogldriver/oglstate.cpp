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

#include "os.h"
#include "oglstate.hpp"
#include "ogldisplaylist.hpp"
#include "glcl.hpp"
#include "glim.hpp"
#include "oglglobals.h"
#include "gldd.h"

#include <climits>
#include <cmath>
#include <cstdio>
#include <limits>
#include <map>
#include <set>
#include <unordered_map>

#if !defined(_WIN32)
#include <unistd.h>
#endif

namespace OGL
{

VertexBuffer::VertexBuffer()
{
    mNumVertices = 0;
    mNumAttributes = 0;
    mNumSideAttributes = 0;
    mAttributes.mask = 0;
    InitializeVertexAttrFmts(mAttrFormats);
    mAttrFormatsSeen.mask = 0;
    memset(&mhBuffers[0], 0, sizeof(mhBuffers));
    memset(&mpfBuffers[0], 0, sizeof(mpfBuffers));
    memset(&mhSideBuffers[0], 0, sizeof(mhSideBuffers));
    memset(&mpfSideBuffers[0], 0, sizeof(mpfSideBuffers));
    memset(&mhBuffersUP[0], 0, sizeof(mhBuffersUP));
    memset(&mCurrentSize[0], 0, sizeof(mCurrentSize));

    mhIndexBuffer = 0;
    mpIndexBuffer = 0;
    mNumIndices = 0;

    mhNIB8 = 0;
    mpNIB8 = 0;
}

VertexBuffer::~VertexBuffer()
{
    // destroy all SWR buffers
    for (int i = 0; i < NUM_ATTRIBUTES; ++i)
    {
        if (mhBuffers[i])
        {
            GetDDProcTable().pfnDestroyBuffer(GetDDHandle(), mhBuffers[i]);
        }
    }

    for (int i = 0; i < NUM_SIDE_ATTRIBUTES; ++i)
    {
        if (mhSideBuffers[i])
        {
            GetDDProcTable().pfnDestroyBuffer(GetDDHandle(), mhSideBuffers[i]);
        }
    }

    if (mhIndexBuffer)
    {
        GetDDProcTable().pfnDestroyBuffer(GetDDHandle(), mhIndexBuffer);
        mhIndexBuffer = 0;
    }

    if (mhNIB8)
    {
        GetDDProcTable().pfnDestroyBuffer(GetDDHandle(), mhNIB8);
        mhNIB8 = 0;
    }

    memset(&mhBuffers[0], 0, sizeof(mhBuffers));
    memset(&mhSideBuffers[0], 0, sizeof(mhSideBuffers));
}

// keeps track of buffer sizes and will either reuse or destroy and recreate for larger allocation
void VertexBuffer::getBuffer(int index, int size)
{
    if (mhBuffers[index] == NULL)
    {
        mhBuffers[index] = GetDDProcTable().pfnCreateBuffer(GetDDHandle(), size, NULL);
        mCurrentSize[index] = size;
        return;
    }

    if (mCurrentSize[index] < (UINT)size)
    {
        GetDDProcTable().pfnDestroyBuffer(GetDDHandle(), mhBuffers[index]);
        mhBuffers[index] = GetDDProcTable().pfnCreateBuffer(GetDDHandle(), size, NULL);
        mCurrentSize[index] = size;
    }

    return;
}

static OSALIGNSIMD(SWRL::m44f) sIdentity = SWRL::M44Id<GLfloat>();

void DetermineMatrixClasses(SWRL::m44f const &m44, MatrixClass &cls)
{
    cls.mAffine = (m44[3][0] == 0.0f) &&
                  (m44[3][1] == 0.0f) &&
                  (m44[3][2] == 0.0f) &&
                  (m44[3][3] == 1.0f) &&
                  1;
    cls.mNoTranslation = (m44[0][3] == 0.0f) &&
                         (m44[1][3] == 0.0f) &&
                         (m44[2][3] == 0.0f) &&
                         (m44[3][3] == 1.0f) &&
                         1;
    cls.mScale = (m44[0][1] == 0.0f) &&
                 (m44[0][2] == 0.0f) &&
                 (m44[1][0] == 0.0f) &&
                 (m44[1][2] == 0.0f) &&
                 (m44[2][0] == 0.0f) &&
                 (m44[2][1] == 0.0f) &&
                 1;
    cls.mIdentity = SWRL::mequals(m44, sIdentity);
}

void MaintainMatrixInfoInvariant(State &state)
{
    // Determine which matrices are affine, and maintain flag invariant.
    for (GLuint i = 0, N = NUM_MATRIX_TYPES; i < N; ++i)
    {
        if (state.mMatrices[i].empty())
            continue;
        auto m44 = state.mMatrices[i].top();
        DetermineMatrixClasses(m44, state.mMatrixInfo[i]);
    }
    if (!state.mMatrices[OGL::PROJECTION].empty() && !state.mMatrices[OGL::MODELVIEW].empty())
    {
        state.mModelViewProjection = SWRL::mmult(state.mMatrices[OGL::PROJECTION].top(), state.mMatrices[OGL::MODELVIEW].top());
        DetermineMatrixClasses(state.mModelViewProjection, state.mMatrixInfoMVP);
    }
}

template <typename T>
void _CacheState(T *t, _simd_crcint &CRC)
{
    union
    {
        T *t;
        _simd_crcint *I;
    } U;
    U.t = t;

    for (GLuint i = 0, N = sizeof(T) / sizeof(_simd_crcint); i < N; ++i)
    {
        CRC = _simd_crc32(CRC, U.I[i]);
    }
}

void CacheState(SaveableState &state, _simd_crcint seed, _simd_crcint &L0, _simd_crcint &L1, _simd_crcint &L2)
{
    L0 = seed;
    _CacheState(static_cast<CacheableL0 *>(&state), L0);
    L1 = L0;
    _CacheState(static_cast<CacheableL1 *>(&state), L1);
    L2 = L1;
    _CacheState(static_cast<CacheableL2 *>(&state), L2);
}

void Initialize(State &state)
{
    state.mNoExecute = false;
    state.mLastError = GL_NONE;

    GetDDProcTable().pfnVBLayoutInfo(GetDDHandle(), state.mVBLayoutInfo.permuteWidth);
    switch (state.mVBLayoutInfo.permuteWidth)
    {
    case 4:
        state.mVBLayoutInfo.permuteShift = 2;
        state.mVBLayoutInfo.permuteMask = 0x3;
        break;
    case 8:
        state.mVBLayoutInfo.permuteShift = 3;
        state.mVBLayoutInfo.permuteMask = 0x7;
        break;
    case 16:
        state.mVBLayoutInfo.permuteShift = 4;
        state.mVBLayoutInfo.permuteMask = 0xf;
        break;
    default:
        assert(0);
    }

    state.mCaps.alphatest = 0;
    state.mCaps.blend = 0;
    state.mCaps.clipPlanes = 0;
    state.mCaps.cullFace = 0;
    state.mCaps.colorArray = 0;
    state.mCaps.depthTest = 0;
    state.mCaps.fog = 0;
    state.mCaps.indexArray = 0;
    state.mCaps.light = 0;
    state.mCaps.lighting = 0;
    state.mCaps.localViewer = 0;
    state.mCaps.multisample = 0;
    state.mCaps.normalArray = 0;
    state.mCaps.normalize = 0;
    state.mCaps.rescaleNormal = 0;
    state.mCaps.scissorTest = 0;
    state.mCaps.specularColor = 0;
    state.mCaps.twoSided = 0;
    state.mCaps.texCoordArray = 0;
    state.mCaps.texGenS = 0;
    state.mCaps.texGenT = 0;
    state.mCaps.texGenR = 0;
    state.mCaps.texGenQ = 0;
    state.mCaps.textures = 0;
    state.mCaps.vertexArray = 0;
    state.mCaps.attribArrayMask = 0;

    state.mDisplayListMode = GL_NONE;
    state.mCompilingDL = 0;
    state.mExecutingDL = std::stack<GLuint>();

    state.mRedundancyMap.Initialize(32); // XXX: should request from SWR its cache depth.
    state.mpDrawingVB = &state._mVertexBuffer;
    // XXX: we're adding two SIMD lanes of padding, to simplify our lives in SWR =)
    for (int i = 0; i < NUM_ATTRIBUTES; ++i)
    {
        GetVB(state).getBuffer(i, VERTEX_BUFFER_COUNT * 4 * sizeof(GLfloat) + KNOB_VS_SIMD_WIDTH * 2);
        GetVB(state).mpfBuffers[i] = (GLfloat *)GetDDProcTable().pfnLockBufferNoOverwrite(GetDDHandle(), GetVB(state).mhBuffers[i]);
        GetVB(state).mhBuffersUP[i] = 0;
    }
    for (int i = 0; i < NUM_SIDE_ATTRIBUTES; ++i)
    {
        GetVB(state).mhSideBuffers[i] = GetDDProcTable().pfnCreateBuffer(GetDDHandle(), VERTEX_BUFFER_COUNT * 4 * sizeof(GLfloat) + KNOB_VS_SIMD_WIDTH * 2, NULL);
        GetVB(state).mpfSideBuffers[i] = (GLfloat *)GetDDProcTable().pfnLockBufferNoOverwrite(GetDDHandle(), GetVB(state).mhSideBuffers[i]);
    }

    GetVB(state).mhIndexBuffer = GetDDProcTable().pfnCreateBuffer(GetDDHandle(), INDEX_BUFFER_COUNT * sizeof(GLint) + KNOB_VS_SIMD_WIDTH * sizeof(GLint), NULL);
    GetVB(state).mNumIndices = INDEX_BUFFER_COUNT;

    GetVB(state).mNumVertices = 0;

    for (int i = 0; i < 4; ++i)
    {
        //state.mLightModel.ambientColor[i]	= i != 3 ? 0.2f : 1.0f;
        for (int j = 0; j < 2; ++j)
            state.mColor[j][i] = 1.0f;
        state.mNormal[i] = i != 2 ? 0.0f : 1.0f;
        for (int j = 0; j < 8; ++j)
            state.mTexCoord[j][i] = i != 3 ? 0.0f : 1.0f;

        state.mClearColor[i] = 0.0f;
    }

    state.mBlendFuncSFactor = GL_ONE;
    state.mBlendFuncDFactor = GL_ZERO;

    state.mColorMask.red = GL_TRUE;
    state.mColorMask.green = GL_TRUE;
    state.mColorMask.blue = GL_TRUE;
    state.mColorMask.alpha = GL_TRUE;

    state.mFog.mMode = GL_EXP;
    state.mFog.mDensity = 1.0f;
    state.mFog.mStart = 0.0f;
    state.mFog.mEnd = 1.0f;
    state.mFog.mColor[0] = 0.0f;
    state.mFog.mColor[1] = 0.0f;
    state.mFog.mColor[2] = 0.0f;
    state.mFog.mColor[3] = 0.0f;
#ifdef GL_VERSION_1_4
    state.mFog.mCoordSrc = GL_FRAGMENT_DEPTH;
#endif

    state.mShadeModel = GL_SMOOTH;

    state.mClearDepth = 1.0;

    GetVB(state).mAttributes.color = 0;
    GetVB(state).mAttributes.fog = 0;
    GetVB(state).mAttributes.normal = 0;
    GetVB(state).mAttributes.texCoord = 0;

    state.mArrayPointers.mVertex.mNumCoordsPerAttribute = 4;
    state.mArrayPointers.mVertex.mType = GL_FLOAT;
    state.mArrayPointers.mVertex.mStride = 0;
    state.mArrayPointers.mVertex.mpElements = 0;

    state.mArrayPointers.mFog.mNumCoordsPerAttribute = 1;
    state.mArrayPointers.mFog.mType = GL_FLOAT;
    state.mArrayPointers.mFog.mStride = 0;
    state.mArrayPointers.mFog.mpElements = 0;

    state.mArrayPointers.mNormal.mNumCoordsPerAttribute = 3;
    state.mArrayPointers.mNormal.mType = GL_FLOAT;
    state.mArrayPointers.mNormal.mStride = 0;
    state.mArrayPointers.mNormal.mpElements = 0;

    for (int i = 0; i < NUM_COLORS; ++i)
    {
        state.mArrayPointers.mColor[i].mNumCoordsPerAttribute = 4;
        state.mArrayPointers.mColor[i].mType = GL_FLOAT;
        state.mArrayPointers.mColor[i].mStride = 0;
        state.mArrayPointers.mColor[i].mpElements = 0;
    }

    for (int i = 0; i < NUM_TEXTURES; ++i)
    {
        state.mArrayPointers.mTexCoord[i].mNumCoordsPerAttribute = 4;
        state.mArrayPointers.mTexCoord[i].mType = GL_FLOAT;
        state.mArrayPointers.mTexCoord[i].mStride = 0;
        state.mArrayPointers.mTexCoord[i].mpElements = 0;
    }

    state.mMatrixMode = GL_MODELVIEW;
    for (int i = 0, N = NUM_MATRIX_TYPES; i < N; ++i)
    {
        state.mMatrices[i].clear();
        state.mMatrices[i].push(SWRL::M44Id<GLfloat>());
    }

    MaintainMatrixInfoInvariant(state);

    for (int i = 0; i < NUM_LIGHTS; ++i)
    {
        state.mLightSource[i].mAmbient[0] = 0;
        state.mLightSource[i].mAmbient[1] = 0;
        state.mLightSource[i].mAmbient[2] = 0;
        state.mLightSource[i].mAmbient[3] = 1;

        state.mLightSource[i].mDiffuse[0] = (GLfloat)(i == 0 ? 1 : 0);
        state.mLightSource[i].mDiffuse[1] = (GLfloat)(i == 0 ? 1 : 0);
        state.mLightSource[i].mDiffuse[2] = (GLfloat)(i == 0 ? 1 : 0);
        state.mLightSource[i].mDiffuse[3] = (GLfloat)(i == 0 ? 1 : 1);

        state.mLightSource[i].mSpecular[0] = (GLfloat)(i == 0 ? 1 : 0);
        state.mLightSource[i].mSpecular[1] = (GLfloat)(i == 0 ? 1 : 0);
        state.mLightSource[i].mSpecular[2] = (GLfloat)(i == 0 ? 1 : 0);
        state.mLightSource[i].mSpecular[3] = (GLfloat)(i == 0 ? 1 : 1);

        state.mLightSource[i].mPosition[0] = 0;
        state.mLightSource[i].mPosition[1] = 0;
        state.mLightSource[i].mPosition[2] = 1;
        state.mLightSource[i].mPosition[3] = 0;

        state.mLightSource[i].mSpotDirection[0] = 0;
        state.mLightSource[i].mSpotDirection[1] = 0;
        state.mLightSource[i].mSpotDirection[2] = -1;
        state.mLightSource[i].mSpotDirection[3] = 0;

        state.mLightSource[i].mSpotExponent = 0;
        state.mLightSource[i].mSpotCutoff = DEFAULT_SPOT_CUT;
        state.mLightSource[i].mConstantAttenuation = 1;
        state.mLightSource[i].mLinearAttenuation = 0;
        state.mLightSource[i].mQuadraticAttenuation = 0;
    }

    state.mLightModel.mAmbient[0] = 0.2f;
    state.mLightModel.mAmbient[1] = 0.2f;
    state.mLightModel.mAmbient[2] = 0.2f;
    state.mLightModel.mAmbient[3] = 1.0f;

    state.mFrontMaterial.mAmbient[0] = 0.2f;
    state.mFrontMaterial.mAmbient[1] = 0.2f;
    state.mFrontMaterial.mAmbient[2] = 0.2f;
    state.mFrontMaterial.mAmbient[3] = 1.0f;
    state.mFrontMaterial.mDiffuse[0] = 0.8f;
    state.mFrontMaterial.mDiffuse[1] = 0.8f;
    state.mFrontMaterial.mDiffuse[2] = 0.8f;
    state.mFrontMaterial.mDiffuse[3] = 1.0f;
    state.mFrontMaterial.mSpecular[0] = 0.0f;
    state.mFrontMaterial.mSpecular[1] = 0.0f;
    state.mFrontMaterial.mSpecular[2] = 0.0f;
    state.mFrontMaterial.mSpecular[3] = 1.0f;
    state.mFrontMaterial.mEmission[0] = 0.0f;
    state.mFrontMaterial.mEmission[1] = 0.0f;
    state.mFrontMaterial.mEmission[2] = 0.0f;
    state.mFrontMaterial.mEmission[3] = 1.0f;
    state.mFrontMaterial.mShininess = 0.0f;

    state.mBackMaterial.mAmbient[0] = 0.2f;
    state.mBackMaterial.mAmbient[1] = 0.2f;
    state.mBackMaterial.mAmbient[2] = 0.2f;
    state.mBackMaterial.mAmbient[3] = 1.0f;
    state.mBackMaterial.mDiffuse[0] = 0.8f;
    state.mBackMaterial.mDiffuse[1] = 0.8f;
    state.mBackMaterial.mDiffuse[2] = 0.8f;
    state.mBackMaterial.mDiffuse[3] = 1.0f;
    state.mBackMaterial.mSpecular[0] = 0.0f;
    state.mBackMaterial.mSpecular[1] = 0.0f;
    state.mBackMaterial.mSpecular[2] = 0.0f;
    state.mBackMaterial.mSpecular[3] = 1.0f;
    state.mBackMaterial.mEmission[0] = 0.0f;
    state.mBackMaterial.mEmission[1] = 0.0f;
    state.mBackMaterial.mEmission[2] = 0.0f;
    state.mBackMaterial.mEmission[3] = 1.0f;
    state.mBackMaterial.mShininess = 0.0f;

    state.mTopology = GL_NONE;
    state.mDepthFunc = GL_LESS;
    state.mDepthMask = GL_TRUE;
    state.mViewport.x = 0;
    state.mViewport.y = 0;
    state.mViewport.width = 128;
    state.mViewport.height = 128;
    state.mViewport.zNear = 0;
    state.mViewport.zFar = 1;
    state.mScissor.x = 0;
    state.mScissor.y = 0;
    state.mScissor.width = KNOB_GUARDBAND_WIDTH;
    state.mScissor.height = KNOB_GUARDBAND_HEIGHT;
    state.mCullFace = GL_BACK;
    state.mFrontFace = GL_CCW;
    state.mPolygonMode[0] = GL_FILL;
    state.mPolygonMode[1] = GL_FILL;

    state.mAlphaFunc = GL_ALWAYS;
    state.mAlphaRef = 0.0;
    state.mActiveTexture = GL_TEXTURE0;
    state.mClientActiveTexture = GL_TEXTURE0;
    state.mTextureTarget = GL_NONE;
    state.mLastUsedTextureName = 1;

    for (GLuint i = 0; i < NUM_TEXTURES; ++i)
    {
        state.mTexUnit[i].mTexEnv.mMode = GL_MODULATE;
        state.mTexUnit[i].mTexEnv.mColor[0] = 0.0;
        state.mTexUnit[i].mTexEnv.mColor[1] = 0.0;
        state.mTexUnit[i].mTexEnv.mColor[2] = 0.0;
        state.mTexUnit[i].mTexEnv.mColor[3] = 0.0;
        state.mTexUnit[i].mNamedTexture1d = 0;
        state.mTexUnit[i].mNamedTexture2d = 0;
        state.mTexUnit[i].mNamedTexture3d = 0;
        state.mTexUnit[i].mNamedTextureCube = 0;
    }

    state.mArraysLocked = 0;
    state.mLockedCount = 0;

    state.mVBOs[0] = VertexBufferObject();
    state.mActiveArrayVBO = 0;
    state.mActiveElementVBO = 0;
    state.mLastUsedVBO = 0;

#ifdef SWR_GLSL
    state.mLastUsedShader = 0;
    state.mLastUsedProgram = 0;
#endif

#ifdef OGL_LOGGING
#if defined(_WIN32)
#define getpid() GetCurrentProcessId()
#endif
    char str[255];
    sprintf(str, "ogltrace_%u.txt", getpid());
    state.mpLog = fopen(str, "w");
#else
    state.mpLog = 0;
#endif
}

void Destroy(State &state)
{
    // Destroy textures
    for (std::unordered_map<GLuint, TexParameters>::iterator it = state.mTexParameters.begin(); it != state.mTexParameters.end(); ++it)
    {
        TexParameters &texParams = (*it).second;
        if (texParams.mhTexture)
        {
            GetDDProcTable().pfnDestroyTexture(GetDDHandle(), texParams.mhTexture);
        }
    };
    state.mTexParameters.clear();

    // Destroy display lists
    DisplayLists().clear();
}

void InitializeVertexAttrFmts(VertexAttributeFormats &vAttrFormats)
{
    vAttrFormats.mask = 0;
    vAttrFormats.position = OGL_RGBA32_FLOAT;
    vAttrFormats.normal = OGL_RGB32_FLOAT;
    for (GLuint i = 0; i < NUM_COLORS; ++i)
    {
        vAttrFormats.color |= (OGL_RGBA32_FLOAT << (AttrFmtShift * i));
    }
    for (GLuint i = 0; i < NUM_TEXCOORDS; ++i)
    {
        vAttrFormats.texCoord |= (OGL_RGBA32_FLOAT << (AttrFmtShift * i));
    }
}
}

static OGL::State *gpOglState = 0;
std::unordered_map<GLuint, OGL::DisplayList *> PrepDLs()
{
    std::unordered_map<GLuint, OGL::DisplayList *> dls;
    dls[0] = new OGL::DisplayList(); // off limits!
    return dls;
}
static std::unordered_map<GLuint, OGL::DisplayList *> gOglDLs = PrepDLs();
static OGL::DisplayList *gpCompilingDL = 0;
static GLuint gDefinedDLs = 1;

void UpdateCompilingDL()
{
    auto &ogl = GetOGL();
    auto &dl = gOglDLs[ogl.mCompilingDL];
    gpCompilingDL = dl;
}

OGL::DisplayList &CompilingDL()
{
    return *gpCompilingDL; //gOglDLs[GetOGL().mCompilingDL];
}

OGL::DisplayList &ExecutingDL(OGL::State &s)
{
    return *gOglDLs[s.mExecutingDL.top()];
}

std::unordered_map<GLuint, OGL::DisplayList *> &DisplayLists()
{
    return gOglDLs;
}

GLuint ReserveLists(GLsizei number)
{
    GLuint rangeStart = gDefinedDLs;
    gDefinedDLs += number;
    return rangeStart;
}

namespace OGL
{

void AnalyzeVertexAttributes(State &s)
{
    bool useColor = true;

    GetVB(s).mAttributes.mask = 0;
    GetVB(s).mNumAttributes = 0;
    GetVB(s).mNumSideAttributes = 0;

    if (glimIsEnabled(s, GL_LIGHTING))
    {
        useColor = false;
        GetVB(s).mAttributes.normal = 1;
        ++GetVB(s).mNumAttributes;
        ++GetVB(s).mNumSideAttributes;
    }

    if (glimIsEnabled(s, GL_FOG) && s.mFog.mCoordSrc == GL_FOG_COORD)
    {
        // useFog
        GetVB(s).mAttributes.fog = 1;
        ++GetVB(s).mNumAttributes;
    }

    if (useColor)
    {
        GetVB(s).mAttributes.color |= 0x1;
        ++GetVB(s).mNumAttributes;
    }

    GLint sepSpecColor = 0;
    glimGetIntegerv(s, GL_LIGHT_MODEL_COLOR_CONTROL, &sepSpecColor);
    if (sepSpecColor == GL_SEPARATE_SPECULAR_COLOR)
    {
        GetVB(s).mAttributes.color |= 0x2;
        ++GetVB(s).mNumAttributes;
    }

    for (int i = 0; i < NUM_TEXTURES; ++i)
    {
        if (s.mCaps.textures & (0x1 << i))
        {
            GetVB(s).mAttributes.texCoord |= (0x1ULL << i);
            ++GetVB(s).mNumAttributes;
        }
    }

    // Convert from index to size.
    ++GetVB(s).mNumAttributes;
}

std::map<OGL::EnumTy, const char *> const &GetEnumTyMap()
{
    static std::map<OGL::EnumTy, const char *> gMap;
    if (gMap.empty())
    {
#define ENUM_MAP_ENTRY(ENTRY) gMap[(OGL::EnumTy)ENTRY] = #ENTRY;
#include "enumMap.inl"
#undef ENUM_MAP_ENTRY
    }

    return gMap;
}

#include "gltrace.inl"

} // end namespace OGL

OGL::State &GetOGL()
{
    return *gpOglState;
}

void SetOGL(OGL::State *pOglState)
{
    gpOglState = pOglState;
}

OGL::DisplayList &CompilingDL(OGL::State &s)
{
    return *gOglDLs[s.mCompilingDL];
}

GLfloat LightScaler(GLint i)
{
    return ((GLfloat)i) / INT_MAX;
}

GLfloat LightScaler(GLfloat f)
{
    return f;
}

template <typename GLty>
std::array<GLfloat, 4> ConvertLightArgs(GLenum name, GLenum pname, GLty param)
{
    std::array<GLfloat, 4> R;
    R[0] = (GLfloat)param;
    return R;
}

template <typename GLty>
std::array<GLfloat, 4> ConvertLightArgs(GLenum name, GLenum pname, const GLty *params)
{
    std::array<GLfloat, 4> R;

    switch (pname)
    {
    case GL_AMBIENT:
    case GL_DIFFUSE:
    case GL_SPECULAR:
        R[0] = (GLfloat)LightScaler(params[0]);
        R[1] = (GLfloat)LightScaler(params[1]);
        R[2] = (GLfloat)LightScaler(params[2]);
        R[3] = (GLfloat)LightScaler(params[3]);
        break;
    case GL_POSITION:
    case GL_SPOT_DIRECTION:
        R[0] = (GLfloat)params[0];
        R[1] = (GLfloat)params[1];
        R[2] = (GLfloat)params[2];
        if (pname != GL_SPOT_DIRECTION)
        {
            R[3] = (GLfloat)params[3];
        }
        break;
    case GL_SPOT_CUTOFF:
    case GL_CONSTANT_ATTENUATION:
    case GL_LINEAR_ATTENUATION:
    case GL_QUADRATIC_ATTENUATION:
        R[0] = (GLfloat)params[0];
        break;
    default:
        break;
    }

    return R;
}

template <typename GLty>
std::array<GLfloat, 4> ConvertLightModelArgs(GLenum pname, GLty param)
{
    std::array<GLfloat, 4> R;
    R[0] = (GLfloat)param;
    return R;
}

template <typename GLty>
std::array<GLfloat, 4> ConvertLightModelArgs(GLenum pname, const GLty *params)
{
    std::array<GLfloat, 4> R;

    switch (pname)
    {
    case GL_LIGHT_MODEL_AMBIENT:
        R[0] = (GLfloat)LightScaler(params[0]);
        R[1] = (GLfloat)LightScaler(params[1]);
        R[2] = (GLfloat)LightScaler(params[2]);
        R[3] = (GLfloat)LightScaler(params[3]);
        break;
    default:
        R[0] = (GLfloat)params[0];
        break;
    }

    return R;
}

template <typename GLty>
std::array<GLfloat, 4> ConvertMaterialArgs(GLenum name, GLenum pname, GLty param)
{
    std::array<GLfloat, 4> R;
    R[0] = (GLfloat)param;
    return R;
}

template <typename GLty>
std::array<GLfloat, 4> ConvertMaterialArgs(GLenum name, GLenum pname, const GLty *params)
{
    std::array<GLfloat, 4> R;

    switch (pname)
    {
    case GL_AMBIENT:
    case GL_DIFFUSE:
    case GL_SPECULAR:
    case GL_EMISSION:
    case GL_AMBIENT_AND_DIFFUSE:
        R[0] = (GLfloat)LightScaler(params[0]);
        R[1] = (GLfloat)LightScaler(params[1]);
        R[2] = (GLfloat)LightScaler(params[2]);
        R[3] = (GLfloat)LightScaler(params[3]);
        break;
    case GL_SHININESS:
    case GL_COLOR_INDEXES:
        R[0] = (GLfloat)params[0];
        if (pname == GL_COLOR_INDEXES)
        {
            R[1] = (GLfloat)params[1];
            R[2] = (GLfloat)params[2];
        }
        break;
    }

    return R;
}

template <typename GLty>
SWRL::m44f ConvertMatrixArgs(const GLty *m)
{
    SWRL::m44f M;
    for (int i = 0, n = 0; i < 4; ++i)
    {
        for (int j = 0; j < 4; ++j, ++n)
        {
            M[j][i] = (GLfloat)m[n];
        }
    }
    return M;
}

template <typename GLty>
SWRL::v4f ConvertTexEnvArgs(GLenum pname, GLty param)
{
    SWRL::v4f result;
    result[0] = (GLfloat)param;
    return result;
}

template <typename GLty>
SWRL::v4f ConvertTexEnvArgs(GLenum pname, const GLty *params)
{
    SWRL::v4f result;

    for (int i = 0; i < 4; ++i)
    {
        result[i] = (GLfloat)params[i];
    }

    return result;
}

template <typename GLty>
SWRL::v4f ConvertTexParameterArgs(GLenum pname, GLty param)
{
    SWRL::v4f result;
    result[0] = (GLfloat)param;
    return result;
}

template <typename GLty>
SWRL::v4f ConvertTexParameterArgs(GLenum pname, const GLty *params)
{
    SWRL::v4f result;

    if (GL_TEXTURE_BORDER_COLOR)
    {
        for (int i = 0; i < 4; ++i)
        {
            result[i] = LightScaler(params[i]);
        }
    }
    else
    {
        result[0] = (GLfloat)params[0];
    }

    return result;
}

template <typename GLty>
float ConvertColorValue(GLty v)
{
    if (std::numeric_limits<GLty>::is_integer)
    {
        float f = (float)v;
        f /= (float)std::numeric_limits<GLty>::max();
        return (float)f;
    }
    return (float)v;
}

extern "C" {

#define MAKE_OGL_CALL(NAME, ...)                             \
    OGL_TRACE(NAME, GetOGL(), ##__VA_ARGS__);                \
    if (!IsCompilingDL(GetOGL()) || IsExecutingDL(GetOGL())) \
        return OGL::glim##NAME(GetOGL(), ##__VA_ARGS__);     \
    if (IsCompilingDL(GetOGL()) || IsExecutingDL(GetOGL()))  \
        OGL::glcl##NAME(CompilingDL(), ##__VA_ARGS__);

#define MAKE_OGL_CALL_NOCL(NAME, ...)                          \
    OGL_TRACE(NAME, GetOGL(), ##__VA_ARGS__);                  \
    if (GetOGL().mpLog)                                        \
        fprintf(GetOGL().mpLog, "%s\n", #NAME, ##__VA_ARGS__); \
    if (IsCompilingDL(GetOGL()) || IsExecutingDL(GetOGL()))    \
        GetOGL().mLastError = GL_INVALID_OPERATION;            \
    else                                                       \
    return OGL::glim##NAME(GetOGL(), ##__VA_ARGS__)

#define MAKE_OGL_CALL_IGNORECL(NAME, ...)                      \
    OGL_TRACE(NAME, GetOGL(), ##__VA_ARGS__);                  \
    if (GetOGL().mpLog)                                        \
        fprintf(GetOGL().mpLog, "%s\n", #NAME, ##__VA_ARGS__); \
    return OGL::glim##NAME(GetOGL(), ##__VA_ARGS__)

#include "gl.inl"

#undef MAKE_OGL_CALL

void __stdcall gluPerspective(GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar)
{
    //GLfloat xmin, xmax, ymin, ymax;

    GLdouble f = 1.0 / std::tan(fovy * 3.151592653589 / 360.0);

    GLdouble M[] =
        {
          (f / aspect), 0.0f, 0.0f, 0.0f,
          0.0f, f, 0.0f, 0.0f,
          0.0f, 0.0f, (zFar + zNear) / (zNear - zFar), -1.0f,
          0.0f, 0.0f, (2 * zFar * zNear) / (zNear - zFar), 0.0f,
        };

    glMultMatrixd(M);
}
}
