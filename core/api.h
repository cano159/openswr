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

#ifndef __SWR_API_H__
#define __SWR_API_H__

#include "os.h"

#include <assert.h>
#include <vector>

#include "defs.h"
#include "simdintrin.h"
#include "formats.h"

#if defined(FORCE_LINUX)
typedef struct HWND__ *HWND;
#endif

// XXX: place all the API functions into the 'swr' namespace.
// XXX: move all API data (enums, structs, functions) into here, and put into 'swr' or 'SWR' namespace
//		for instance, ARRAY_SPEC => SWR_ARRAY_SPEC; AS_TexCube => SWR_AS_TEXCUBE, et al

/// ENUMS

enum SWR_PIPE_INFO
{
    PI_MAX_TEXTURE_VIEWS = KNOB_NUMBER_OF_TEXTURE_VIEWS,
    PI_MAX_OF_TEXTURE_SAMPLERS = KNOB_NUMBER_OF_TEXTURE_SAMPLERS,
    PI_MAX_STREAMS = KNOB_NUM_STREAMS,
    PI_MAX_RENDERTARGETS = KNOB_NUM_RENDERTARGETS,
    PI_MAX_ATTRIBUTES = KNOB_NUM_ATTRIBUTES,
};

enum SWR_LOCK_FLAGS
{
    LOCK_NONE = 1 << 0,
    LOCK_NOOVERWRITE = 1 << 1,
    LOCK_DISCARD = 1 << 2,
    LOCK_DONTLOCK = 1 << 3,
};

enum SWR_SHADER_TYPE
{
    SHADER_VERTEX,
    SHADER_PIXEL,

    NUM_GRAPHICS_SHADER_TYPES,

    SHADER_FETCH,
    SHADER_VS_LINKAGE,
    SHADER_PS_LINKAGE,
};

enum SEMANTIC
{
    INVALID,
    POSITION,
    TEXCOORD,
    NORMAL, // combine the next three?
    COLOR,
    ATTRIBUTE,
};

enum SWR_ADDRESSING_MODE
{
    SWR_AM_CLAMP,   // clamp to the edge value if out of bounds
    SWR_AM_DEFAULT, // use a default color if out of bounds
    SWR_AM_WRAP,    // u % 1.0f
    SWR_AM_MIRROR,  // u % 2.0f
};

enum SWR_BLEND_MODE
{
    BLEND_ONE,
    BLEND_ZERO,
    BLEND_SECONDARY,
    BLEND_ONE_MINUS_SECONDARY,
    BLEND_SOURCE_ALPHA,
    BLEND_ONE_MINUS_SOURCE_ALPHA,
};

enum SWR_BLEND_OP
{
    BLEND_ADD,
    BLEND_SUB,
    BLEND_RSUB,
    BLEND_MIN,
    BLEND_MAX,
};

enum SWR_ZFUNCTION
{
    ZFUNC_ALWAYS,
    ZFUNC_LE,
    ZFUNC_LT,
    ZFUNC_GT,
    ZFUNC_GE,
    ZFUNC_NEVER,
    ZFUNC_EQ,
    NUM_ZFUNC
};

enum SWR_TILING_FORMAT
{
    TF_Linear,
    TF_Tile,
    TF_Z,
    TF_TileZ,
};

enum SWR_ARRAY_SPEC
{
    AS_1D,
    AS_2D,
    AS_TexCube,
    AS_Volume,
};

enum SWR_MIP_KIND
{
    MIP_None,
    MIP_Point,
    MIP_Linear,
};

enum SWR_FILTER_KIND
{
    FLTR_Point,
    FLTR_Linear,
};

enum SWR_DEBUG_INFO
{
    DBGINFO_DUMP_FPS,
    DBGINFO_DUMP_POOL_INFO,
};

// You can do anything you want here, however, for now
// SWR only recognizes RGBX-FLOAT for POSITION-0 and
// COLOR-0; everything else will be ignored.
struct INPUT_ELEMENT_DESC
{
    UINT32 AlignedByteOffset;

    UINT32 Semantic : 3;
    UINT32 SemanticIndex : 5;
    UINT32 Format : 4;
    UINT32 StreamIndex : 4;
    UINT32 Constant : 1; // Indicates the fetch shader should grab this input from the constant buffer
    UINT32 _reserved : 15;
};

// XXX: remove these, IEDs, et al from SWR.
enum VS_ATTRIBUTE_SLOT
{
    VS_SLOT_POSITION = 0,
    VS_SLOT_COLOR0 = 1,
    VS_SLOT_COLOR1 = 2,
    VS_SLOT_TEXCOORD0 = 3,
    VS_SLOT_TEXCOORD1 = 4,
    VS_SLOT_NORMAL = 5,
    VS_SLOT_MAX = 6,

    // XXX: these exist to allow the FF to compile.
    VS_SLOT_FOG_FRAG_COORD,
    VS_SLOT_FOG_COORD,
    VS_SLOT_CLIPVERTEX,
    VS_SLOT_COLOR0_2,
    VS_SLOT_COLOR1_2,
};

enum SWR_PS_ATTRIBUTE_SLOT
{
    SWR_PS_SLOT_COLOR0,
    SWR_PS_SLOT_COLOR1,
    SWR_PS_SLOT_TEXCOORD0,
    SWR_PS_SLOT_TEXCOORD7 = SWR_PS_SLOT_TEXCOORD0 + 7,
    SWR_PS_SLOT_POSITION,
};

enum SWR_PS_RENDERTARGET_SLOT
{
    SWR_PS_RT_COLOR0,
    SWR_PS_RT_COLOR3 = SWR_PS_RT_COLOR0 + 3,
    SWR_PS_RT_DEPTH0,
    SWR_PS_RT_DEPTH4 = SWR_PS_RT_DEPTH0 + 3,
};

enum SWR_RENDERTARGET_ATTACHMENT
{
    SWR_ATTACHMENT_COLOR0,
    SWR_ATTACHMENT_DEPTH,

    SWR_NUM_ATTACHMENTS
};

#define VS_ATTR_MASK(slot) (1 << (slot))

enum PRIMITIVE_TOPOLOGY
{
    TOP_TRIANGLE_LIST,
    TOP_TRIANGLE_STRIP,
    TOP_TRIANGLE_FAN,
    TOP_TRIANGLE_DISC,
    TOP_QUAD_LIST,
    TOP_QUAD_STRIP,
    TOP_LINE_LIST,
    TOP_LINE_STRIP,
    TOP_POINTS
};

enum SWR_CULLMODE
{
    CCW,
    CW,
    NONE
};

/// STRUCTURE arguments

struct TexCoord
{
    simdscalar U;
    simdscalar V;
    simdscalar W;
};

struct WideColor
{
    simdscalar R;
    simdscalar G;
    simdscalar B;
    simdscalar A;
};

struct Gradients
{
    __m128 dudx;
    __m128 dudy;
    __m128 dvdx;
    __m128 dvdy;
    __m128 dwdx;
    __m128 dwdy;
};

struct SWR_CREATETEXTURE
{
    // IN.
    UINT eltSizeInBytes;
    UINT width;
    UINT height;
    UINT planes;              // depth or number of array elements; =0 means "do the right thing for 2D"
    UINT mipLevels;           // 0 means "add all the mip levels necessary"
    SWR_LOCK_FLAGS lockFlags; // create in lock state
};

struct SWR_GETSUBTEXTUREINFO
{
    // IN.
    SWR_LOCK_FLAGS lockFlags;

    // IN/OUT.
    // if plane = 0 & mipLevel = 0 then
    //		set plane & mipLevel as OUT based off of subTxIdx
    // else
    //		set subTxIdx as OUT based off of plane & mipLevel
    UINT plane;
    UINT mipLevel;
    UINT subtextureIndex;

    // OUT.
    UINT physWidth;
    UINT physHeight;
    UINT physPlanes;
    UINT texelWidth;
    UINT texelHeight;
    UINT texelPlanes;
    UINT eltSizeInBytes;
    void *pData;
};

struct SWR_TEXTUREVIEW
{
    HANDLE hTexture;
    SWR_FORMAT format;
    UINT mipBias;
    UINT mipMax;
};

struct SWR_CREATESAMPLER
{
    SWR_ADDRESSING_MODE arrayMode;
    SWR_ARRAY_SPEC arraySpec;
    SWR_TILING_FORMAT tilingFormat;
    float defaultColor[4];
};

struct SWR_SAMPLERINFO
{
    HANDLE sampler;
    UINT slot;
    SWR_SHADER_TYPE type;
};

struct SWR_TEXTUREVIEWINFO
{
    HANDLE textureView;
    UINT slot;
    SWR_SHADER_TYPE type;
};

struct SWR_VIEWPORT
{
    float x;
    float y;
    float width;
    float height;
    float halfWidth;
    float halfHeight;
    float minZ;
    float maxZ;
};

struct RASTSTATE
{
    SWR_CULLMODE cullMode;
    SWR_VIEWPORT vp;
    BOOL scissorEnable;
};

// Input to vertex shader
struct VERTEXINPUT
{
#ifdef KNOB_VERTICALIZED_FE
    simdvector vertex[VS_SLOT_MAX];
#endif
    const void *pConstants; // vertex shader constant buffer
    FLOAT *pAttribs;        // position + 4 attributes (4 component each)
};

struct VERTEXOUTPUT
{
#ifdef KNOB_VERTICALIZED_FE
    simdvector vertex[VS_SLOT_MAX];
#endif
    FLOAT *pVertices; // 4 vertices (position only)
    FLOAT *pAttribs;  // 4 attributes (4 component each)
};

struct SWR_FETCH_INFO
{
    FLOAT *ppStreams[KNOB_NUM_STREAMS];
    const INT *pIndices;
    const void *pConstants;
};

struct SWR_TEXTURE_VIEW
{
    unsigned char **pData;
    unsigned int *pTexelWidths;
    unsigned int *pTexelHeights;
    unsigned int *pPhysicalWidths;
    unsigned int *pPhysicalHeights;
    unsigned int numPlanes;
    unsigned int numMipLevels;
    unsigned int eltSizeInBytes;

    unsigned int mipBias;
    unsigned int mipMax;

    float defaultColor[4];
};

struct Texture
{
    enum
    {
        TILE_ALIGN = 8,
        DATA_ALIGN = 16,
    };
    typedef BYTE data_t;

    Texture();
    Texture(HANDLE hContext, SWR_CREATETEXTURE const &args);
    ~Texture();
    void Create(HANDLE hContext, SWR_CREATETEXTURE const &args);
    void Lock(SWR_LOCK_FLAGS lockFlags);
    void Unlock();
    void SubtextureInfo(SWR_GETSUBTEXTUREINFO &info);

    HANDLE mhContext;

    // Geometry.
    UINT mNumPlanes;
    UINT mNumMipLevels;
    std::vector<UINT> mTexelHeight;
    std::vector<UINT> mPhysicalHeight;
    std::vector<UINT> mTexelWidth;
    std::vector<UINT> mPhysicalWidth;
    UINT mElementSizeInBytes;

    // Storage.
    std::vector<HANDLE> mhSubtextures;
    std::vector<data_t *> mSubtextures;
};

struct SWR_TEXCOORD
{
    simdvector tc;
};

struct SWR_MIPS
{
    simdscalar mipLevels;
};

struct SWR_COLOR
{
    simdvector color;
};

/// CIRCULAR DEPENDENCIES with context.h
struct SWR_PIXELINPUT
{
    simdvector vertex[VS_SLOT_MAX];
    const void *pConstants; // vertex shader constant buffer
};

struct SWR_PIXELOUTPUT
{
    void *pRenderTargets[KNOB_NUM_RENDERTARGETS];
};

struct SWR_TRIANGLE_DESC
{
#if KNOB_VERTICALIZED_BE
    // This has bit-rotted.
    __m256i tileX;
    __m256i tileY;
    __m256 I[3];
    __m256 J[3];
    __m256 K[3];
    __m256 recipDet;
    __m256 zStepX;
    __m256 zStepY;
    __m256 oneOverW;
    // Pixels are prelit and packed,
    // so we only have a coverage mask
    // when issuing two quads.
    __m256i coverageMaskTile;
    SWR_PIXELINPUT pixelInput;
#else
    OSALIGNSIMD(float) I[3];
    OSALIGNSIMD(float) J[3];
    OSALIGNSIMD(float) Z[3];
    OSALIGNSIMD(float) OneOverW[3];

    float zStepX, zStepY;
    float recipDet;

    float *pInterpBuffer;

    UINT32 tileX;
    UINT32 tileY;

    UINT widthInBytes;

    UINT64 coverageMask;

    const void *pConstants;

    const HANDLE *pTextureViews;
    const HANDLE *pSamplers;
#endif
};

/// POINTER TO FUNCTIONS

typedef void (*PFN_VERTEX_FUNC)(const VERTEXINPUT &in, VERTEXOUTPUT &out);
typedef void (*PFN_FETCH_FUNC)(SWR_FETCH_INFO &fetchInfo, VERTEXINPUT &out);
typedef void (*PFN_PIXEL_FUNC)(const SWR_TRIANGLE_DESC &, SWR_PIXELOUTPUT &);
typedef void (*PFN_TEXTURE_SAMPLE_INSTR)(SWR_TEXTURE_VIEW *, SWR_TEXCOORD *, SWR_MIPS *, SWR_COLOR *);

/// INTERFACE

HANDLE SwrCreateContext(DRIVER_TYPE driver);

void SwrDestroyContext(
    HANDLE hContext);

void SwrSetDebugInfo(
    HANDLE hContext,
    UINT eDebugInfo);

void SwrUnsetDebugInfo(
    HANDLE hContext,
    UINT eDebugInfo);

#ifdef _WIN32
void SwrCreateSwapChain(
    HANDLE hContext,
    HANDLE hDisplay,
    HANDLE hWnd,
    HANDLE hVisInfo,
    UINT numBuffers,
    UINT width,
    UINT height);

void SwrDestroySwapChain(
    HANDLE hContext);

void SwrResizeSwapChain(
    HANDLE hContext,
    UINT width,
    UINT height);

HANDLE SwrGetBackBuffer(
    HANDLE hContext,
    UINT index);

HANDLE SwrGetDepthBuffer(
    HANDLE hContext);
#endif

void SwrSetNumAttributes(
    HANDLE hContext,
    UINT numAttrs);

UINT SwrGetNumAttributes(
    HANDLE hContext);

UINT SwrGetPermuteWidth(
    HANDLE hContext);

void SwrSetVertexBuffer(
    HANDLE hContext,
    HANDLE hVertexBuffer);

void SwrSetVertexBuffers(
    HANDLE hContext,
    UINT indexStart,
    UINT numBuffers,
    const HANDLE *phVertexBuffers);

void SwrSetIndexBuffer(
    HANDLE hContext,
    HANDLE pIndexBuffer);

void SwrSetFetchFunc(
    HANDLE hContext,
    PFN_FETCH_FUNC pfnFetchFunc);

void SwrSetVertexFunc(
    HANDLE hContext,
    PFN_VERTEX_FUNC pfnVertexFunc);

void SwrSetPixelFunc(
    HANDLE hContext,
    PFN_PIXEL_FUNC pfnPixelFunc);

void SwrSetLinkageMaskFrontFace(
    HANDLE hContext,
    UINT mask);

void SwrSetLinkageMaskBackFace(
    HANDLE hContext,
    UINT mask);

void SwrDraw(
    HANDLE hContext,
    PRIMITIVE_TOPOLOGY topology,
    UINT startVertex,
    UINT primCount);

void SwrDrawIndexed(
    HANDLE hContext,
    PRIMITIVE_TOPOLOGY topology,
    SWR_TYPE type,
    UINT numIndices,
    UINT indexOffset);

void SwrDrawIndexedInstanced(
    HANDLE hContext,
    PRIMITIVE_TOPOLOGY topology,
    SWR_TYPE type,
    UINT numIndices,
    UINT numInstances,
    UINT indexOffset);

void SwrDrawIndexedUP(
    HANDLE hContext,
    PRIMITIVE_TOPOLOGY topology,
    UINT numIndices,
    INT *pIndices,
    VOID *pVertices,
    UINT vertexStride,
    UINT indexOffset);

void SwrPresent(
    HANDLE hContext);

void SwrPresent2(
    HANDLE hContext,
    void *pData,
    UINT pitch);

void SwrSetRenderTargets(
    HANDLE hContext,
    HANDLE hRenderTarget,
    HANDLE hDepthTarget);

void SwrClearRenderTarget(
    HANDLE hContext,
    HANDLE hRenderTarget,
    UINT clearMask,
    const FLOAT clearColor[4],
    float z,
    bool useScissor);

void SwrSetFsConstantBuffer(
    HANDLE hContext,
    HANDLE hConstantBuffer);

void SwrSetVsConstantBuffer(
    HANDLE hContext,
    HANDLE hConstantBuffer);

void SwrSetPsConstantBuffer(
    HANDLE hContext,
    HANDLE hConstantBuffer);

void SwrGetRastState(
    HANDLE hContext,
    RASTSTATE *pRastState);

void SwrSetRastState(
    HANDLE hContext,
    const RASTSTATE *pRastState);

void SwrSetScissorRect(
    HANDLE hContext,
    UINT left, UINT top, UINT right, UINT bottom);

HANDLE SwrCreateRenderTarget(
    HANDLE hContext,
    UINT width,
    UINT height,
    SWR_FORMAT format);

void SwrDestroyRenderTarget(
    HANDLE hContext,
    HANDLE hrt);

HANDLE SwrCreateBuffer(
    HANDLE hContext,
    UINT size,
    UINT numaNode = 0);

HANDLE SwrCreateBufferUP(
    HANDLE hContext,
    void *pData);

void *SwrLockResource(
    HANDLE hContext,
    HANDLE hResource,
    SWR_LOCK_FLAGS flags);

void SwrUnlock(
    HANDLE hContext,
    HANDLE hResource);

void SwrDestroyBuffer(
    HANDLE hContext,
    HANDLE hBuffer);

void SwrDestroyBufferUP(
    HANDLE hContext,
    HANDLE hBuffer);

// Texture, TextureView, and Sampler API.
HANDLE SwrCreateTexture(
    HANDLE hContext,
    SWR_CREATETEXTURE const &);

void SwrDestroyTexture(
    HANDLE hContext,
    HANDLE hTexture);

void SwrLockTexture(
    HANDLE hContext,
    HANDLE hTexture,
    SWR_LOCK_FLAGS flags);

void SwrUnlockTexture(
    HANDLE hContext,
    HANDLE hTexture);

void SwrGetSubtextureInfo(
    HANDLE hTexture,
    SWR_GETSUBTEXTUREINFO &getSTI);

HANDLE SwrCreateTextureView(
    HANDLE hContext,
    SWR_TEXTUREVIEW const &args);

void SwrDestroyTextureView(
    HANDLE hContext,
    HANDLE hTextureView);

void SwrSetTextureView(
    HANDLE hContext,
    SWR_TEXTUREVIEWINFO const &);

HANDLE SwrGetTextureView(
    HANDLE hContext,
    SWR_TEXTUREVIEWINFO const &);

HANDLE SwrCreateSampler(
    HANDLE hContext,
    SWR_CREATESAMPLER const &args);

void SwrDestroySampler(
    HANDLE hContext,
    HANDLE hSampler);

void SwrSetSampler(
    HANDLE hContext,
    SWR_SAMPLERINFO const &);

HANDLE SwrGetSampler(
    HANDLE hContext,
    SWR_SAMPLERINFO const &);

UINT SwrGetNumNumaNodes(
    HANDLE hContext);

unsigned SwrGetNumWorkerThreads(HANDLE hContext);

// copy a sub rect of the currently bound render target into a texture resource
void SwrCopyRenderTarget(
    HANDLE hContext,
    SWR_RENDERTARGET_ATTACHMENT rt,
    HANDLE hTexture,
    void *pixels,
    SWR_FORMAT dstFormat,
    INT dstPitch,
    INT srcX, INT srcY,
    INT dstX, INT dstY,
    UINT width, UINT height);

INT SwrNumBytes(SWR_FORMAT format);
INT SwrNumComponents(SWR_FORMAT format);
SWR_TYPE SwrFormatType(SWR_FORMAT forma);

// @todo put this somewhere smart
UINT NumPrimitiveVerts(PRIMITIVE_TOPOLOGY t, UINT numPrims);
UINT NumElementsGivenIndices(PRIMITIVE_TOPOLOGY mode, UINT numElements);

#endif //__SWR_API_H__
