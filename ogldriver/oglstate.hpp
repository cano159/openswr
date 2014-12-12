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

#ifndef OGL_STATE_HPP
#define OGL_STATE_HPP

#include <array>
#include <cstring>
#include <list>
#include <map>
#include <stack>
#include <unordered_map>
#include <vector>
#include <cstdio>

#include <nmmintrin.h>

#include <containers.hpp>
#include <algebra.hpp>

#include "oglglobals.h"
#include "simdintrin.h"

#ifdef SWR_GLSL
#include "glsl.hpp"
#endif

namespace OGL
{

std::map<OGL::EnumTy, const char *> const &GetEnumTyMap();

struct DisplayList;

enum
{
    AttrFmtShift = 3ULL,
    AttrFmtMask = (0x1ULL << AttrFmtShift) - 1
};
typedef unsigned long long UINT64;

enum
{
    NUM_MATRIX_TYPES = 2 + NUM_TEXTURES
}; // projection, mv, and NUM_TEXTURES textures

enum
{
    MODELVIEW = 0,
    PROJECTION = 1,
    TEXTURE_BASE = 2,
};

struct MatrixClass
{
    GLuint mAffine : 1;
    GLuint mNoTranslation : 1;
    GLuint mScale : 1;
    GLuint mIdentity : 1;
};

union VertexAttributeFormats
{
    struct
    {
        UINT64 position : AttrFmtShift * 1;
        UINT64 normal : AttrFmtShift * 1;
        UINT64 fog : AttrFmtShift * 1;
        UINT64 color : AttrFmtShift *NUM_COLORS;
        UINT64 texCoord : AttrFmtShift *NUM_TEXTURES;
    };
    UINT64 mask;
};

void InitializeVertexAttrFmts(VertexAttributeFormats &vAttrFormats);

union VertexActiveAttributes
{
    struct
    {
        GLuint normal : 1;
        GLuint fog : 1;
        GLuint color : NUM_COLORS;
        GLuint texCoord : NUM_TEXTURES;
    };
    GLuint mask;
};

INLINE bool operator==(VertexActiveAttributes const &lhs, VertexActiveAttributes const &rhs)
{
    return (lhs.normal == rhs.normal) &&
           (lhs.fog == rhs.fog) &&
           (lhs.color == rhs.color) &&
           (lhs.texCoord == rhs.texCoord) &&
           1;
}

struct VBIEDLayout
{
    struct
    {
        GLuint vertex;
        GLuint normal;
        GLuint fog;
        GLuint color[NUM_COLORS];
        GLuint texCoord[NUM_TEXCOORDS];
    } position;

    struct
    {
        GLuint normal;
    } sideBandPosition;
};

struct VertexBuffer
{

    VertexBuffer();
    ~VertexBuffer();

    // keeps track of buffer sizes and will either reuse or destroy and recreate for larger allocation
    void getBuffer(int index, int size);

    VBIEDLayout mVBIEDLayout;

    GLuint mNumVertices;
    GLuint mNumAttributes;
    GLuint mNumSideAttributes;

    VertexActiveAttributes mAttributes;
    VertexAttributeFormats mAttrFormats;
    VertexAttributeFormats mAttrFormatsSeen;
    DDHANDLE mhBuffers[NUM_ATTRIBUTES];
    GLfloat *mpfBuffers[NUM_ATTRIBUTES];

    UINT mCurrentSize[NUM_ATTRIBUTES];

    DDHANDLE mhSideBuffers[NUM_SIDE_ATTRIBUTES];
    GLfloat *mpfSideBuffers[NUM_SIDE_ATTRIBUTES];

    DDHANDLE mhBuffersUP[NUM_ATTRIBUTES];

    DDHANDLE mhIndexBuffer;
    GLuint *mpIndexBuffer;
    GLint mNumIndices;

    DDHANDLE mhNIB8;
    GLubyte *mpNIB8;
};

struct VertexBufferObject
{
    VertexBufferObject()
    {
        mSize = 0;
        mUsage = GL_STATIC_DRAW_ARB;
        mAccess = GL_READ_WRITE_ARB;
        mMapped = GL_FALSE;
        mMappedPointer = NULL;
        mHWBuffer = NULL;
    }

    GLuint mSize;
    GLenum mUsage;
    GLenum mAccess;
    GLboolean mMapped;
    GLvoid *mMappedPointer;
    HANDLE mHWBuffer;
};

struct ActiveVBOBindings
{
    GLuint vertex;
    GLuint normal;
    GLuint fog;
    GLuint color[NUM_COLORS];
    GLuint texcoord[NUM_TEXCOORDS];
    GLuint index;
};

struct LightSourceParameters
{
    SWRL::v4f mAmbient;
    SWRL::v4f mDiffuse;
    SWRL::v4f mSpecular;
    SWRL::v4f mPosition;
    SWRL::v4f mOMPosition;
    SWRL::v4f mSpotDirection;
    GLfloat mSpotExponent;
    GLfloat mSpotCutoff;
    GLfloat mConstantAttenuation;
    GLfloat mLinearAttenuation;
    GLfloat mQuadraticAttenuation;
};

struct MaterialParameters
{
    SWRL::v4f mEmission;
    SWRL::v4f mAmbient;
    SWRL::v4f mDiffuse;
    SWRL::v4f mSpecular;
    GLfloat mShininess;
};

struct LightModelProducts
{
    SWRL::v4f mAmbient;
};

struct LightProducts
{
    SWRL::v4f mAmbient;
    SWRL::v4f mDiffuse;
    SWRL::v4f mSpecular;
};

struct FogParameters
{
    GLenum mMode;
    GLfloat mDensity;
    GLfloat mStart;
    GLfloat mEnd;
    SWRL::v4f mColor;
    GLenum mCoordSrc;
};

struct ArrayPointerParameters
{
    GLint mNumCoordsPerAttribute;
    GLenum mType;
    GLsizei mStride;
    const GLvoid *mpElements;
};

struct ArrayPointers
{
    ArrayPointerParameters mVertex;
    union
    {
        struct
        {
            ArrayPointerParameters mNormal;
            ArrayPointerParameters mFog;
            ArrayPointerParameters mColor[NUM_COLORS];
            ArrayPointerParameters mTexCoord[NUM_TEXTURES];
        };
        ArrayPointerParameters mArrays[NUM_ATTRIBUTES];
    };
};

struct TexParameters
{
    TexParameters()
    {
        mMinFilter = GL_NEAREST_MIPMAP_LINEAR;
        mMagFilter = GL_LINEAR;
        mMinLOD = -1000;
        mMaxLOD = 1000;
        mBaseLevel = 0;
        mMaxLevel = 1000;
        mBorderMode[0] = GL_REPEAT;
        mBorderMode[1] = GL_REPEAT;
        mBorderMode[2] = GL_REPEAT;
        mBorderColor[0] = 0.0f;
        mBorderColor[1] = 0.0f;
        mBorderColor[2] = 0.0f;
        mBorderColor[3] = 0.0f;
        mPriority = 0.0f;
        mCompareMode = GL_NONE;
        mCompareFunc = GL_LEQUAL;
        mDepthMode = GL_LUMINANCE;
        mGenMIPs = GL_FALSE;
        mWidth = 0;
        mHeight = 0;

        mhTexture = 0;
    }

    // TexParam.
    GLenum mMinFilter;
    GLenum mMagFilter;
    GLint mMinLOD;
    GLint mMaxLOD;
    GLint mBaseLevel;
    GLint mMaxLevel;
    GLenum mBorderMode[3];
    GLfloat mBorderColor[4];
    GLfloat mPriority;
    GLenum mCompareMode;
    GLenum mCompareFunc;
    GLenum mDepthMode;
    GLboolean mGenMIPs;
    GLuint mWidth;
    GLuint mHeight;

    // Texture
    DDHTEXTURE mhTexture;

    // TexGen.
    // XXX: holy crap, no?
};

struct TexEnvParameters
{
    // TexEnv.
    GLenum mMode;
    GLfloat mColor[4];
};

struct TextureUnit
{
    TexEnvParameters mTexEnv;
    GLuint mNamedTexture1d;
    GLuint mNamedTexture2d;
    GLuint mNamedTextureCube;
    GLuint mNamedTexture3d;
};

struct ColorMask
{
    GLboolean red, green, blue, alpha;
};

struct Caps
{
    union
    {
        struct
        {
            GLuint normalArray : 1;
            GLuint fogArray : 1;
            GLuint colorArray : NUM_COLORS;
            GLuint texCoordArray : NUM_TEXTURES;
        };
        GLuint attribArrayMask;
    };

    GLuint alphatest : 1;
    GLuint blend : 1;
    GLuint clipPlanes : 6;
    GLuint cullFace : 1;
    GLuint depthTest : 1;
    GLuint fog : 1;
    GLuint indexArray : 1;
    GLuint lighting : 1;
    GLuint light : NUM_LIGHTS;
    GLuint localViewer : 1;
    GLuint multisample : 1;
    GLuint normalize : 1;
    GLuint rescaleNormal : 1;
    GLuint scissorTest : 1;
    GLuint specularColor : 1; // 0 == GL_SINGLE_COLOR
    GLuint texGenS : NUM_TEXTURES;
    GLuint texGenT : NUM_TEXTURES;
    GLuint texGenR : NUM_TEXTURES;
    GLuint texGenQ : NUM_TEXTURES;
    GLuint textures : NUM_TEXTURES;
    GLuint twoSided : 1;
    GLuint vertexArray : 1;
};

struct VBLayoutInfo
{
    GLuint permuteWidth;
    GLuint permuteShift;
    GLuint permuteMask;
};

struct ViewPort
{
    GLint x;
    GLint y;
    GLsizei width;
    GLsizei height;
    GLfloat zNear;
    GLfloat zFar;
};

struct Scissor
{
    GLint x;
    GLint y;
    GLsizei width;
    GLsizei height;
};

struct PixelStoreParameters
{
    GLint mPackRowLength;
};

template <GLuint NUM_ELEMENTS>
struct RedundancyMap
{
    typedef SWRL::UncheckedFixedVector<GLfloat, NUM_ELEMENTS> value_type;
    enum
    {
        OFFSET = 2166136261U,
        PRIME = 16777619,
    };

    RedundancyMap(GLuint depth = 256)
    {
        Initialize(depth);
    }

    void Initialize(GLuint depth)
    {
        mHits = 0;
        mMisses = 0;
        mDepth = depth;
        mHash.resize(mDepth);
        mAttr.resize(mDepth);
        mIndex.resize(mDepth);
    }

    GLuint GetEarliestIndex(value_type const &key, GLuint curIndex)
    {
        union
        {
            float F;
            GLuint H;
        } U;

        GLuint hash = OFFSET;
        for (std::size_t i = 0; i < key.size(); ++i)
        {
            U.F = key[i];
            hash ^= U.H;
            hash *= PRIME;
        }

        // Maintain key/hash state invariant.
        GLuint killIndex = curIndex % mDepth;
        mHash[killIndex] = hash;
        mAttr[killIndex] = key;
        mIndex[killIndex] = curIndex;

        // Starting at the further index from ours,
        // try to find a copy of our value.
        for (GLuint i = 1; i <= mDepth; ++i)
        {
            GLuint lookIndex = (killIndex + i) % mDepth;
            if ((mHash[lookIndex] == hash) && (mAttr[lookIndex] == key))
            {
                if (i < mDepth)
                    ++mHits;
                else
                    ++mMisses;
                return mIndex[lookIndex];
            }
        }

        assert(false && "This is unreachable.");
        return 0;
    }

    GLuint mHits;
    GLuint mMisses;
    GLuint mDepth;
    std::vector<GLuint> mHash;
    std::vector<value_type> mAttr;
    std::vector<GLuint> mIndex;
};

OSALIGNLINE(struct) CacheableL0
{
    // Caps.
    Caps mCaps;

    // Blend functions
    GLenum mBlendFuncSFactor;
    GLenum mBlendFuncDFactor;

    // Matrices.
    MatrixClass mMatrixInfo[NUM_MATRIX_TYPES];
    MatrixClass mMatrixInfoMVP;

    // Vertex state.
    GLenum mShadeModel;

    // Rasterizer state.
    GLenum mTopology;
    GLenum mDepthFunc;
    GLboolean mDepthMask;

    // Fragment state.
    GLenum mAlphaFunc;
    GLclampf mAlphaRef;
    ColorMask mColorMask;

    // Render target state.
};

OSALIGNLINE(struct) CacheableL1
{
    // Light state.
    LightModelProducts mLightModel;
    MaterialParameters mFrontMaterial;
    MaterialParameters mBackMaterial;
    SWRL::v4f mFrontSceneColor;
    SWRL::v4f mBackSceneColor;
};

OSALIGNLINE(struct) CacheableL2
{
    // Light state.
    LightSourceParameters mLightSource[NUM_LIGHTS];

    // Clear state.
    SWRL::v4f mClearColor;
    GLclampd mClearDepth;
    GLbitfield mClearMask;

    // Rasterizer state.
    ViewPort mViewport;
    Scissor mScissor;
    GLenum mCullFace;
    GLenum mFrontFace;
    GLenum mPolygonMode[2]; // GL_FRONT / GL_BACK

    // Texture state.
    TextureUnit mTexUnit[NUM_TEXTURES];
};

OSALIGNLINE(struct) Uncacheable
{
    // Vertex state.
    VBLayoutInfo mVBLayoutInfo;
    SWRL::v4f mNormal;
    SWRL::v4f mNormalizedNormal;

    // Fog state.
    FogParameters mFog;

    // Color state.
    SWRL::v4f mColor[NUM_COLORS];

    // Texcoord state.
    SWRL::v4f mTexCoord[NUM_TEXCOORDS];

    // Light state.
    GLenum mCurrentLight;

    // Display list state.
    GLenum mDisplayListMode;

    // Vertex state.
    ArrayPointers mArrayPointers;

    // Matrices.
    OSALIGNSIMD(SWRL::m44f) mModelViewProjection;
    OSALIGNSIMD(SWRL::m44f) mModelViewMatrix;
    OSALIGNSIMD(SWRL::m44f) mNormalMatrix;
    OSALIGNSIMD(SWRL::m44f) mTexMatrix[NUM_TEXTURES];

    GLfloat mNormalScale;

    // Matrix mode state.
    GLenum mMatrixMode;

    // Pointer-array info.
    GLint mArraysLocked;
    GLint mLockedCount;

    // Texture state.
    //	Textures are odd ducks. There are (at the top-level) three
    //	parameters that work together to identify a texture:
    //		1. The texture unit currently bound (active texture);
    //		2. The kind of the texture unit, ie, 1d, 2d, Cube, 3d (target); and,
    //		3. The texture object (name).
    //	I think the idea was to try to differentiate between a -sampler-
    //	decripton (active-texture/texture-target), and a bound texture resource.
    //	However, the terminology (and object model) are very confusing.
    GLenum mActiveTexture;
    GLenum mClientActiveTexture; // XXX: You read that right. These are different.
    GLenum mTextureTarget;
    GLuint mLastUsedTextureName;

    // VBO
    std::unordered_map<GLuint, VertexBufferObject> mVBOs;
    GLuint mActiveArrayVBO;
    GLuint mActiveElementVBO;
    GLuint mLastUsedVBO;
    ActiveVBOBindings mActiveVBOs;

#ifdef SWR_GLSL
    // GLSL
    std::unordered_map<GLuint, Shader> mShaders;
    std::unordered_map<GLuint, Program> mPrograms;
    GLuint mLastUsedShader;
    GLuint mLastUsedProgram;
#endif

    PixelStoreParameters mPixelStore;
};

struct SaveableState : CacheableL0, CacheableL1, CacheableL2, Uncacheable
{
};

typedef std::pair<GLbitfield, SaveableState *> AttribState;

typedef void (*PFN_RESIZE)(GLuint width, GLuint height);

struct State : SaveableState
{
    bool mNoExecute;

    // Error.
    GLenum mLastError;

    // Display list state.
    GLuint mCompilingDL;
    std::stack<GLuint> mExecutingDL;

    // Vertex state.
    RedundancyMap<64> mRedundancyMap;
    VertexBuffer _mVertexBuffer;
    VertexBuffer *mpDrawingVB;

// Matrix mode state.
// matrix stacks for 1 MV, 1 Proj, and NUM_TEXTURES textures
#ifdef _WIN32
    __declspec(align(16)) SWRL::FixedStack<SWRL::m44f, MAX_MATRIX_STACK_DEPTH> mMatrices[NUM_MATRIX_TYPES];
#else
    SWRL::FixedStack<SWRL::m44f, MAX_MATRIX_STACK_DEPTH> __attribute__((aligned(16))) mMatrices[NUM_MATRIX_TYPES];
#endif

    // Per-texture parameter info.
    std::unordered_map<GLuint, TexParameters> mTexParameters;

    // Attribs. (Good luck with this.)
    std::stack<AttribState> mAttribStack;

    // Logging mechanism.
    FILE *mpLog;

    // Driver level callbacks from glX or wgl layer
    PFN_RESIZE pfnResize;

    State()
    {
    }

private:
    State(const State &that)
    {
        assert(0 && "State copies not allowed");
    }

    State &operator=(const State &that)
    {
        assert(0 && "State assignment not allowed");
        return *this;
    }
};

void MaintainMatrixInfoInvariant(State &state);
void CacheState(SaveableState &state, _simd_crcint seed, _simd_crcint &L0, _simd_crcint &L1, _simd_crcint &L2);
void Initialize(State &);
void Destroy(State &);
void AnalyzeVertexAttributes(State &s);

// State interface.
INLINE bool IsCompilingDL(State &s)
{
    return s.mDisplayListMode == GL_COMPILE;
}

INLINE bool IsExecutingDL(State &s)
{
    return s.mDisplayListMode == GL_COMPILE_AND_EXECUTE;
}

INLINE SWRL::FixedStack<SWRL::m44f, MAX_MATRIX_STACK_DEPTH> &CurrentMatrixStack(State &s)
{
    if (s.mMatrixMode == GL_TEXTURE)
    {
        int activeTextureIndex = s.mActiveTexture - GL_TEXTURE0;
        return s.mMatrices[OGL::TEXTURE_BASE + activeTextureIndex];
    }

    return s.mMatrices[s.mMatrixMode - GL_MODELVIEW];
}

INLINE SWRL::m44f &CurrentMatrix(State &s)
{
    return CurrentMatrixStack(s).top();
}

INLINE void UpdateCurrentMatrix(State &s, SWRL::m44f const &mtx)
{
    CurrentMatrix(s) = mtx;
}

INLINE void PushMatrix(State &s, SWRL::m44f const &m)
{
    CurrentMatrixStack(s).push(m);
}

INLINE void PopMatrix(State &s)
{
    CurrentMatrixStack(s).pop();
}

INLINE VertexBuffer &GetVB(State &s)
{
    return *s.mpDrawingVB;
}

} // namespace OGL

OGL::State &GetOGL();
void SetOGL(OGL::State *);
void UpdateCompilingDL();
OGL::DisplayList &CompilingDL();
OGL::DisplayList &ExecutingDL(OGL::State &);
std::unordered_map<GLuint, OGL::DisplayList *> &DisplayLists();
GLuint ReserveLists(GLsizei);

#endif //OGL_STATE_HPP
