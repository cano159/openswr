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

#include "knobs.h"
#include "api.h"
#include "shaders.h"
#include "shader_math.h"
#include "texture_unit.h"

#include "rdtsc.h"

#include <cmath>
#include <stdio.h>

#if (KNOB_VERTICALIZED_FE == 0)
void FetchStandard(SWR_FETCH_INFO &fetchInfo, VERTEXINPUT &out)
{
    const INT *pVbIndex = fetchInfo.pIndices;
    INT vsIndex = pVbIndex[0];
    const UINT NumIEDs = ((const UINT *)fetchInfo.pConstants)[0];
    const auto *pElems = &((const INPUT_ELEMENT_DESC *)fetchInfo.pConstants)[1];
    const UINT *pStrides = &((const UINT *)fetchInfo.pConstants)[NumIEDs + 1];

    for (UINT elem = 0; elem < NumIEDs; ++elem)
    {
        const INPUT_ELEMENT_DESC *pElem = &pElems[elem];
        UINT numComps = SwrNumComponents((FORMAT)pElem->Format);
        UINT elemOffset = (pElem->AlignedByteOffset / sizeof(FLOAT));

        FLOAT *pBuffer = out.pAttribs;
        UINT bufOffset = 0;
        FLOAT *pVertexBuffer = fetchInfo.ppStreams[pElem->StreamIndex];
        UINT vbOffset = (pStrides[pElem->StreamIndex] / sizeof(FLOAT)) * vsIndex;

        switch (pElem->Semantic)
        {
        case POSITION:
            bufOffset = SHADER_INPUT_SLOT_POSITION;
            break;
        case TEXCOORD:
            bufOffset = SHADER_INPUT_SLOT_TEXCOORD0;
            break;
        case NORMAL:
            bufOffset = SHADER_INPUT_SLOT_NORMAL;
            break;
        case COLOR:
            bufOffset = SHADER_INPUT_SLOT_COLOR0;
            break;
        default:
            break;
        };

        _mm_store_ps(pBuffer + bufOffset, _mm_set_ps(1.0f, 0.0f, 0.0f, 0.0f));

        // fetch and convert
        BYTE *pOffset = (BYTE *)&pVertexBuffer[vbOffset + elemOffset];
        switch (pElem->Format)
        {
        case RGBA8_UINT:
        case RGB8_UINT:
        case RG8_UINT:
        case R8_UINT:
            switch (numComps)
            {
            case 4:
                pBuffer[bufOffset + 3] = pOffset[3] / 255.0f;
            case 3:
                pBuffer[bufOffset + 2] = pOffset[2] / 255.0f;
            case 2:
                pBuffer[bufOffset + 1] = pOffset[1] / 255.0f;
            case 1:
                pBuffer[bufOffset + 0] = pOffset[0] / 255.0f;
            }
            break;
        case RGBA32_FLOAT:
        case RGB32_FLOAT:
        case RG32_FLOAT:
        case R32_FLOAT:
            switch (numComps)
            {
            case 4:
                pBuffer[bufOffset + 3] = ((float *)pOffset)[3];
            case 3:
                pBuffer[bufOffset + 2] = ((float *)pOffset)[2];
            case 2:
                pBuffer[bufOffset + 1] = ((float *)pOffset)[1];
            case 1:
                pBuffer[bufOffset + 0] = ((float *)pOffset)[0];
            }
            break;

        default:
            assert(0);
        }
    }
}
#endif
void FetchPosNormal(SWR_FETCH_INFO &fetchInfo, VERTEXINPUT &out)
{
    const INT *pVbIndex = fetchInfo.pIndices;
    INT vsIndex = pVbIndex[0];
    FLOAT *pVertexBuffer = fetchInfo.ppStreams[0];
    UINT vertexStride = 6; // pos + normal = 6 floats
    UINT vbOffset = vertexStride * vsIndex;
    UINT numComps = 3; // RGB32_FLOAT
    FLOAT *pBuffer = out.pAttribs;

    memcpy(&pBuffer[SHADER_INPUT_SLOT_POSITION + 0], &pVertexBuffer[vbOffset], sizeof(float) * numComps);
    out.pAttribs[SHADER_INPUT_SLOT_POSITION + 3] = 1.0f;

    const UINT normalElemOffset = 3; // Normal comes just after position.
    memcpy(&pBuffer[SHADER_INPUT_SLOT_NORMAL + 0], &pVertexBuffer[vbOffset + normalElemOffset], sizeof(float) * numComps);
    out.pAttribs[SHADER_INPUT_SLOT_NORMAL + 3] = 0.0f;
}

void VertexFuncPosOnly(
    const VERTEXINPUT &in, // AOS
    VERTEXOUTPUT &out)     // SOA
{
    const FLOAT *pWorldMat = (FLOAT *)in.pConstants;

    Mat4Vec3Multiply(out.pVertices, pWorldMat, (FLOAT *)in.pAttribs);
}

// handcoded vertex shader for viz.  expects the following constants:
// modelview matrix
// modelviewproj matrix
// normal matrix
// light0 pos
// light0 diffuse
// light0 ambient
// front material ambient
// front material diffuse
// back material ambient
// back material diffuse
//
// output are front and back color
#if (KNOB_VERTICALIZED_FE == 0)
void GLVSLightShader(
    const VERTEXINPUT &in,
    VERTEXOUTPUT &out)
{
    // output description
    const UINT FRONT_COLOR = 0;
    const UINT BACK_COLOR = 4;

    // compute position
    const FLOAT *pConstants = (FLOAT *)in.pConstants;
    const FLOAT *pMVP = &pConstants[GLVSLightConstants::MVP_MATRIX];
    Mat4Vec3Multiply(out.pVertices, pMVP, (FLOAT *)&in.pAttribs[SHADER_INPUT_SLOT_POSITION]);

    // Transform normals in attrib buffer by normal matrix.
    OSALIGNLINE(FLOAT) normals[4];
    Mat4Vec3Multiply(&normals[0], &pConstants[GLVSLightConstants::MV_MATRIX], &in.pAttribs[SHADER_INPUT_SLOT_NORMAL]);

    // Normalize normals
    //__m128 vNormal		= Vec3Normalize(&normals[0]);
    __m128 vNormal = _mm_load_ps(&normals[0]);

    // normalize light
    __m128 vLightDir = _mm_load_ps(&pConstants[GLVSLightConstants::LIGHT0_POS]);
    vLightDir = Vec3Normalize(vLightDir);

    // nDotL and clamp
    __m128 vNdotL0 = Vec3Diffuse(vNormal, vLightDir);

    // light diffuse * nDotL
    __m128 vLightDiffuse = _mm_load_ps(&pConstants[GLVSLightConstants::LIGHT0_DIFFUSE]);
    vLightDiffuse = _mm_mul_ps(vLightDiffuse, vNdotL0);

    // front color = front scene color + light_ambient * front_mat_ambient + light_diffuse * front_mat_diffuse;
    __m128 vFrontColor = _mm_load_ps(&pConstants[GLVSLightConstants::FRONT_SCENE_COLOR]);
    vFrontColor = _mm_add_ps(vFrontColor, _mm_mul_ps(vLightDiffuse, _mm_load_ps(&pConstants[GLVSLightConstants::FRONT_MAT_DIFFUSE])));
    __m128 vAmbient = _mm_mul_ps(_mm_load_ps(&pConstants[GLVSLightConstants::LIGHT0_AMBIENT]), _mm_load_ps(&pConstants[GLVSLightConstants::FRONT_MAT_AMBIENT]));
    vFrontColor = _mm_add_ps(vFrontColor, vAmbient);

    // clamp to 0..1
    vFrontColor = _mm_max_ps(vFrontColor, _mm_setzero_ps());
    vFrontColor = _mm_min_ps(vFrontColor, _mm_set1_ps(1.0));

    // store front color
    _mm_store_ps(&out.pAttribs[FRONT_COLOR], vFrontColor);

#if 1
    // back color
    // invert normal
    vNormal = _mm_mul_ps(vNormal, _mm_set1_ps(-1.0));

    // nDotL and clamp
    vNdotL0 = Vec3Diffuse(vNormal, vLightDir);

    // light diffuse * nDotL
    vLightDiffuse = _mm_load_ps(&pConstants[GLVSLightConstants::LIGHT0_DIFFUSE]);
    vLightDiffuse = _mm_mul_ps(vLightDiffuse, vNdotL0);

    // back color = back scene color + light_ambient * back_mat_ambient + light_diffuse * back_mat_diffuse;
    __m128 vBackColor = _mm_load_ps(&pConstants[GLVSLightConstants::BACK_SCENE_COLOR]);
    vBackColor = _mm_add_ps(vBackColor, _mm_mul_ps(vLightDiffuse, _mm_load_ps(&pConstants[GLVSLightConstants::BACK_MAT_DIFFUSE])));
    vAmbient = _mm_mul_ps(_mm_load_ps(&pConstants[GLVSLightConstants::LIGHT0_AMBIENT]), _mm_load_ps(&pConstants[GLVSLightConstants::BACK_MAT_AMBIENT]));
    vBackColor = _mm_add_ps(vBackColor, vAmbient);

    // clamp 0..1
    vBackColor = _mm_max_ps(vBackColor, _mm_setzero_ps());
    vBackColor = _mm_min_ps(vBackColor, _mm_set1_ps(1.0));

    // store back color
    _mm_store_ps(&out.pAttribs[BACK_COLOR], vBackColor);
#endif
}

void GLVSPosColor(
    const VERTEXINPUT &in,
    VERTEXOUTPUT &out)
{
    // Multiply pos by mvp
    const FLOAT *pMVP = (FLOAT *)in.pConstants;
    Mat4Vec3Multiply(out.pVertices, pMVP, (FLOAT *)&in.pAttribs[SHADER_INPUT_SLOT_POSITION]);

    // Pass color through
    __m128 vColor = _mm_load_ps(&in.pAttribs[SHADER_INPUT_SLOT_COLOR0]);
    _mm_store_ps(&out.pAttribs[0], vColor);
}

void GLVSPosColorTexCoordShader(const VERTEXINPUT &in, VERTEXOUTPUT &out)
{
    // Multiply pos by mvp
    const FLOAT *pMVP = (FLOAT *)in.pConstants;
    Mat4Vec3Multiply(out.pVertices, pMVP, (FLOAT *)&in.pAttribs[SHADER_INPUT_SLOT_POSITION]);

    // Pass color through
    __m128 vColor = _mm_load_ps(&in.pAttribs[SHADER_INPUT_SLOT_COLOR0]);
    _mm_store_ps(&out.pAttribs[0], vColor);

    // Pass texcoord through
    __m128 vTexCoord = _mm_load_ps(&in.pAttribs[SHADER_INPUT_SLOT_TEXCOORD0]);
    _mm_store_ps(&out.pAttribs[4], vTexCoord);
}
#endif

void VertexFuncPosAttrib(
    const VERTEXINPUT &in,
    VERTEXOUTPUT &out)
{
    const FLOAT *pWorldMat = (FLOAT *)in.pConstants;

    Mat4Vec3Multiply(out.pVertices, pWorldMat, (FLOAT *)in.pAttribs);

    __m128 vAttrib = _mm_load_ps(&in.pAttribs[4]);
    _mm_store_ps(&out.pAttribs[0], vAttrib);
}

void VertexFuncPosAttrib2(
    const VERTEXINPUT &in,
    VERTEXOUTPUT &out)
{
    const FLOAT *pWorldMat = (FLOAT *)in.pConstants;

    Mat4Vec3Multiply(out.pVertices, pWorldMat, (FLOAT *)in.pAttribs);

    _mm_store_ps(&out.pAttribs[0], _mm_load_ps(out.pVertices));
}

void VertexFuncConstColor(
    const VERTEXINPUT &in,
    VERTEXOUTPUT &out)
{
    const FLOAT *pWorldMat = (FLOAT *)in.pConstants;
    const FLOAT *pMatColor = (FLOAT *)in.pConstants + CONST_OFFSET_MAT_COLOR;

    Mat4Vec3Multiply(out.pVertices, pWorldMat, (FLOAT *)in.pAttribs);

    __m128 matColor = _mm_load_ps(pMatColor);
    _mm_store_ps(&out.pAttribs[0], matColor);
}

void VertexFuncSmooth(
    const VERTEXINPUT &in,
    VERTEXOUTPUT &out)
{
    const FLOAT *pWorldMat = (FLOAT *)in.pConstants;
    const FLOAT *pLightPos = (FLOAT *)in.pConstants + CONST_OFFSET_LIGHT_POSITION;
    const FLOAT *pMatColor = (FLOAT *)in.pConstants + CONST_OFFSET_MAT_COLOR;

    Mat4Vec3Multiply(out.pVertices, pWorldMat, (FLOAT *)in.pAttribs);

    // Transform normals in attrib buffer by world matrix.
    OSALIGNLINE(FLOAT) normals[4];
    Mat4Vec3Multiply(&normals[0], pWorldMat, &in.pAttribs[SHADER_INPUT_SLOT_NORMAL]);

    // Normalize normals
    __m128 normal = Vec3Normalize(&normals[0]);
    __m128 lightPos = _mm_load_ps(pLightPos);
    __m128 lightDir = _mm_load_ps(in.pAttribs);
    __m128 matColor = _mm_load_ps(pMatColor);

    // Compute a light direction vector for each vertex.
    //     lightDir = Normalize(lightPos - vertexPos)
    lightDir = _mm_sub_ps(lightPos, lightDir);
    lightDir = Vec3Normalize(lightDir);

    __m128 ndotl = Vec3Diffuse(normal, lightDir);
    __m128 result = _mm_mul_ps(matColor, ndotl);

    _mm_store_ps(&out.pAttribs[0], result);
}

void VertexFuncSmoothSphereMapAOS(
    const VERTEXINPUT &in,
    VERTEXOUTPUT &out)
{
    const FLOAT *pWorldMat = (FLOAT *)in.pConstants;
    const FLOAT *pLightPos = (FLOAT *)in.pConstants + CONST_OFFSET_LIGHT_POSITION;

    Mat4Vec3Multiply(out.pVertices, pWorldMat, (FLOAT *)in.pAttribs);

    // Transform normals in attrib buffer by world matrix.
    OSALIGNLINE(FLOAT) normals[4];
    Mat4Vec3Multiply(&normals[0], pWorldMat, &in.pAttribs[SHADER_INPUT_SLOT_NORMAL]);

    // Normalize normals
    __m128 normal = Vec3Normalize(&normals[0]);

    __m128 lightPos = _mm_load_ps(pLightPos);
    __m128 lightDir = _mm_load_ps(in.pAttribs);

    // Compute a light direction vector for each vertex.
    //     lightDir = Normalize(lightPos - vertexPos)
    lightDir = _mm_sub_ps(lightPos, lightDir);
    lightDir = Vec3Normalize(lightDir);

    __m128 ndotl0 = Vec3Diffuse(normal, lightDir);

    _mm_store_ps(&out.pAttribs[0], _mm_load_ps(&in.pAttribs[0]));

    _mm_store_ss(&out.pAttribs[0x2], ndotl0);
}

#define MASKTOVEC(i3, i2, i1, i0) \
    {                             \
        -i0, -i1, -i2, -i3        \
    }
const __m128 gMaskToVec[] = {
    MASKTOVEC(0, 0, 0, 0),
    MASKTOVEC(0, 0, 0, 1),
    MASKTOVEC(0, 0, 1, 0),
    MASKTOVEC(0, 0, 1, 1),
    MASKTOVEC(0, 1, 0, 0),
    MASKTOVEC(0, 1, 0, 1),
    MASKTOVEC(0, 1, 1, 0),
    MASKTOVEC(0, 1, 1, 1),
    MASKTOVEC(1, 0, 0, 0),
    MASKTOVEC(1, 0, 0, 1),
    MASKTOVEC(1, 0, 1, 0),
    MASKTOVEC(1, 0, 1, 1),
    MASKTOVEC(1, 1, 0, 0),
    MASKTOVEC(1, 1, 0, 1),
    MASKTOVEC(1, 1, 1, 0),
    MASKTOVEC(1, 1, 1, 1),
};
#undef MASKTOVEC

#define GL_MODEL_VIEW_MATRIX_OFFSET 0

#if 0
struct ColorInterp
{
	enum
	{
		NUM_INTERPOLANTS	= 1,
		NUM_ATTRIBUTES		= 3,
		DO_PERSPECTIVE		= 1,
	};

	static const UINT SIGNATURE[1];

	ColorInterp()
	{
	}
};

INLINE __m128 shade(ColorInterp const&, const SWR_TRIANGLE_DESC &work, WideVector<ColorInterp::NUM_ATTRIBUTES, __m128> const& pAttrs, BYTE*, BYTE*, UINT*)
{
	// convert float to unorm
	__m128i vBlueI = vFloatToUnorm(get<2>(pAttrs));
	__m128i vGreenI = vFloatToUnorm(get<1>(pAttrs));
	__m128i vRedI = vFloatToUnorm(get<0>(pAttrs));
	__m128i vAlpha = _mm_set1_epi32(0xff000000);

	// pack
	__m128i vPixel = vBlueI;
	vGreenI = _mm_slli_epi32(vGreenI, 8);
	vRedI = _mm_slli_epi32(vRedI, 16);

	vPixel = _mm_or_si128(vPixel, vGreenI);
	vPixel = _mm_or_si128(vPixel, vRedI);
	vPixel = _mm_or_si128(vPixel, vAlpha);

	return _mm_castsi128_ps(vPixel);
}

const UINT ColorInterp::SIGNATURE[1] = { 3 };

template<SWR_ZFUNCTION ZFunc, bool ZWrite>
void interpColorShader(const SWR_TRIANGLE_DESC &work, SWR_PIXELOUTPUT& pOut)
{
	ColorInterp clrInterp;
	GenericPixelShader<ColorInterp, ZFunc, ZWrite> gps;
	gps.run(work, pOut, clrInterp);
}

struct ColorInterpNoPerspective
{
	enum
	{
		NUM_INTERPOLANTS    = 1,
		NUM_ATTRIBUTES		= 3,
		DO_PERSPECTIVE		= 0,
	};

	static const UINT SIGNATURE[1];

	ColorInterpNoPerspective()
	{
	}
};

INLINE __m128 shade(ColorInterpNoPerspective const&, const SWR_TRIANGLE_DESC &work, WideVector<ColorInterpNoPerspective::NUM_ATTRIBUTES, __m128> const& pAttrs, BYTE*, BYTE*, UINT*)
{
	// convert float to unorm
	__m128i vBlueI, vGreenI, vRedI, vAlpha;
	{
		vBlueI = vFloatToUnorm(get<2>(pAttrs));
		vGreenI = vFloatToUnorm(get<1>(pAttrs));
		vRedI = vFloatToUnorm(get<0>(pAttrs));
		vAlpha = _mm_set1_epi32(0xff000000);
	}


	// pack
	__m128i vPixel = vBlueI;
	vGreenI = _mm_slli_epi32(vGreenI, 8);
	vRedI = _mm_slli_epi32(vRedI, 16);

	vPixel = _mm_or_si128(vPixel, vGreenI);
	vPixel = _mm_or_si128(vPixel, vRedI);
	vPixel = _mm_or_si128(vPixel, vAlpha);

	return _mm_castsi128_ps(vPixel);
}

const UINT ColorInterpNoPerspective::SIGNATURE[1] = { 3 };

void InterpColorShaderNoPerspective(const SWR_TRIANGLE_DESC &work, SWR_PIXELOUTPUT& pOut)
{
	ColorInterpNoPerspective clrInterp;
	GenericPixelShader<ColorInterpNoPerspective> gps;
	gps.run(work, pOut, clrInterp);
}

struct GLPSLightShaderStruct
{
	enum
	{
		NUM_INTERPOLANTS    = 2,
		NUM_ATTRIBUTES		= 6,
		DO_PERSPECTIVE		= 1,
	};

	static const UINT SIGNATURE[2];

	GLPSLightShaderStruct()
	{
	}
};

INLINE __m128 shade(GLPSLightShaderStruct const&, const SWR_TRIANGLE_DESC &work, WideVector<GLPSLightShaderStruct::NUM_ATTRIBUTES, __m128> const& pAttrs, BYTE*, BYTE*, UINT*)
{
	// convert float to unorm
	__m128i vBlueI, vGreenI, vRedI, vAlpha;
#if (KNOB_VERTICALIZED_FE == 0)
	if (work.triFlags.backFacing)
	{
		vBlueI = vFloatToUnorm(get<5>(pAttrs));
		vGreenI = vFloatToUnorm(get<4>(pAttrs));
		vRedI = vFloatToUnorm(get<3>(pAttrs));
		vAlpha = _mm_set1_epi32(0xff000000);
	}
	else
#endif
	{
		vBlueI = vFloatToUnorm(get<2>(pAttrs));
		vGreenI = vFloatToUnorm(get<1>(pAttrs));
		vRedI = vFloatToUnorm(get<0>(pAttrs));
		vAlpha = _mm_set1_epi32(0xff000000);
	}


	// pack
	__m128i vPixel = vBlueI;
	vGreenI = _mm_slli_epi32(vGreenI, 8);
	vRedI = _mm_slli_epi32(vRedI, 16);

	vPixel = _mm_or_si128(vPixel, vGreenI);
	vPixel = _mm_or_si128(vPixel, vRedI);
	vPixel = _mm_or_si128(vPixel, vAlpha);

	return _mm_castsi128_ps(vPixel);
}

const UINT GLPSLightShaderStruct::SIGNATURE[2] = { 3, 3 };

template <SWR_ZFUNCTION zFunc, bool zWrite>
void GLPSLightShader(const SWR_TRIANGLE_DESC &work, SWR_PIXELOUTPUT& pOut)
{
	GLPSLightShaderStruct clrInterp;
	GenericPixelShader<GLPSLightShaderStruct, zFunc, zWrite> gps;
	gps.run(work, pOut, clrInterp);
}

enum BlendFunc
{
	GL_ONE,
	GL_NONE,
	GL_SRC_ALPHA,
	GL_ONE_MINUS_SRC_ALPHA
};

struct BilinearSamplePos
{
	enum
	{
		NUM_INTERPOLANTS	= 2,
		NUM_ATTRIBUTES		= 6,
		DO_PERSPECTIVE		= 1,
	};

	static const UINT SIGNATURE[2];

	BlendFunc sFactor;
	BlendFunc dFactor;

	BilinearSamplePos()
	{
		sFactor = GL_ONE;
		dFactor = GL_NONE;
	}
};

const UINT BilinearSamplePos::SIGNATURE[2] = { 4, 2 };

INLINE __m128 shade(BilinearSamplePos const& bsp, const SWR_TRIANGLE_DESC & work, WideVector<BilinearSamplePos::NUM_ATTRIBUTES, __m128> const& pAttrs, BYTE* pBuffer, BYTE*, UINT*)
{
	TextureView *pTxv = (TextureView*)work.pTextureViews[KNOB_NUMBER_OF_TEXTURE_VIEWS + 0];
	Sampler *pSmp = (Sampler*)work.pSamplers[0];

	TexCoord tcidx;
	tcidx.U = get<4>(pAttrs);
	tcidx.V = get<5>(pAttrs);
	UINT mips[] = {0,0,0,0};
	WideColor color;
	SampleSimplePointRGBAF32(*pTxv, *pSmp, tcidx, mips, color);

	// modulate
	color.R = _mm_mul_ps(color.R, get<0>(pAttrs));
	color.G = _mm_mul_ps(color.G, get<1>(pAttrs));
	color.B = _mm_mul_ps(color.B, get<2>(pAttrs));
	color.A = _mm_mul_ps(color.A, get<3>(pAttrs));

	// convert float to unorm
	__m128i r = vFloatToUnorm( color.R );
	__m128i g = vFloatToUnorm( color.G );
	__m128i b = vFloatToUnorm( color.B );
	__m128i a = vFloatToUnorm( color.A );

	// pack
	__m128i vPixel = b;
	vPixel = _mm_or_si128(vPixel, _mm_slli_epi32(g, 8));
	vPixel = _mm_or_si128(vPixel, _mm_slli_epi32(r, 16));
	vPixel = _mm_or_si128(vPixel, _mm_slli_epi32(a, 24));

	// blend with GL_ONE and GL_ONE
	if (bsp.sFactor == GL_ONE && bsp.dFactor == GL_ONE)
	{ 
		__m128i vColorBuffer = _mm_load_si128((const __m128i*)pBuffer);
		vPixel = _mm_adds_epu8(vPixel, vColorBuffer);
	}
	
	if (bsp.sFactor == GL_SRC_ALPHA && bsp.dFactor == GL_ONE_MINUS_SRC_ALPHA)
	{
		const __m128i SHUF_ALPHA = _mm_set_epi32(0x8080800f, 0x8080800b, 0x80808007, 0x80808003);
		const __m128i SHUF_RED = _mm_set_epi32(0x8080800e, 0x8080800a, 0x80808006, 0x80808002);
		const __m128i SHUF_GREEN = _mm_set_epi32(0x8080800d, 0x80808009, 0x80808005, 0x80808001);
		const __m128i SHUF_BLUE = _mm_set_epi32(0x8080800c, 0x80808008, 0x80808004, 0x80808000);

		// mul by src_alpha
		__m128 vSrcRedF = _mm_mul_ps(color.R, color.A);
		__m128 vSrcGreenF = _mm_mul_ps(color.G, color.A);
		__m128 vSrcBlueF = _mm_mul_ps(color.B, color.A);

		// convert to int
		__m128i vSrcRed = vFloatToUnorm(vSrcRedF);
		__m128i vSrcGreen = vFloatToUnorm(vSrcGreenF);
		__m128i vSrcBlue = vFloatToUnorm(vSrcBlueF);
		__m128i vSrcAlpha = vFloatToUnorm(color.A);

		// pack
		__m128i vSrcPixel = vSrcBlue;
		vSrcPixel = _mm_or_si128(vSrcPixel, _mm_slli_epi32(vSrcGreen, 8));
		vSrcPixel = _mm_or_si128(vSrcPixel, _mm_slli_epi32(vSrcRed, 16));
		vSrcPixel = _mm_or_si128(vSrcPixel, _mm_slli_epi32(vSrcAlpha, 24));

		// shuffle dst R,G,B,A
		__m128i vColorBuffer = _mm_load_si128((const __m128i*)pBuffer);

		__m128i vDstAlpha = _mm_shuffle_epi8(vColorBuffer, SHUF_ALPHA);
		__m128i vDstRed = _mm_shuffle_epi8(vColorBuffer, SHUF_RED);
		__m128i vDstGreen = _mm_shuffle_epi8(vColorBuffer, SHUF_GREEN);
		__m128i vDstBlue = _mm_shuffle_epi8(vColorBuffer, SHUF_BLUE);

		// convert to float
		__m128 vDstAlphaF = _mm_cvtepi32_ps(vDstAlpha);
		__m128 vDstRedF = _mm_cvtepi32_ps(vDstRed);
		__m128 vDstGreenF = _mm_cvtepi32_ps(vDstGreen);
		__m128 vDstBlueF = _mm_cvtepi32_ps(vDstBlue);

		// mul by 1-src_alpha
		__m128 vOneMinusSrcAlphaF = _mm_sub_ps(_mm_set1_ps(1.0f), color.A);
		
		vDstAlphaF = _mm_mul_ps(vDstAlphaF, vOneMinusSrcAlphaF);
		vDstRedF = _mm_mul_ps(vDstRedF, vOneMinusSrcAlphaF);
		vDstGreenF = _mm_mul_ps(vDstGreenF, vOneMinusSrcAlphaF);
		vDstBlueF = _mm_mul_ps(vDstBlueF, vOneMinusSrcAlphaF);

		// convert to int
		vDstAlpha = _mm_cvtps_epi32(vDstAlphaF);
		vDstRed = _mm_cvtps_epi32(vDstRedF);
		vDstGreen = _mm_cvtps_epi32(vDstGreenF);
		vDstBlue = _mm_cvtps_epi32(vDstBlueF);

		// pack
		__m128i vDstPixel = vDstBlue;
		vDstPixel = _mm_or_si128(vDstPixel, _mm_slli_epi32(vDstGreen, 8));
		vDstPixel = _mm_or_si128(vDstPixel, _mm_slli_epi32(vDstRed, 16));
		vDstPixel = _mm_or_si128(vDstPixel, _mm_slli_epi32(vDstAlpha, 24));

		// final rgba = min(src + dst,255)
		vPixel = _mm_adds_epu8(vSrcPixel, vDstPixel);
	}

	return _mm_castsi128_ps(vPixel);
}

template <SWR_ZFUNCTION zFunc, bool zWrite>
void interpPosShaderSample(const SWR_TRIANGLE_DESC &work, SWR_PIXELOUTPUT& pOut)
{
	BilinearSamplePos bsp;
	GenericPixelShader<BilinearSamplePos, zFunc, zWrite> gps;
	gps.run(work, pOut, bsp);
}

template <SWR_ZFUNCTION zFunc, bool zWrite>
void interpPosShaderSampleBlend(const SWR_TRIANGLE_DESC &work, SWR_PIXELOUTPUT& pOut)
{
	BilinearSamplePos bsp;
	bsp.sFactor = GL_ONE;
	bsp.dFactor = GL_ONE;

	GenericPixelShader<BilinearSamplePos, zFunc, zWrite> gps;
	gps.run(work, pOut, bsp);
}

template <SWR_ZFUNCTION zFunc, bool zWrite>
void interpPosShaderSampleBlendAlpha(const SWR_TRIANGLE_DESC &work, SWR_PIXELOUTPUT& pOut)
{
	BilinearSamplePos bsp;
	bsp.sFactor = GL_SRC_ALPHA;
	bsp.dFactor = GL_ONE_MINUS_SRC_ALPHA;

	GenericPixelShader<BilinearSamplePos, zFunc, zWrite> gps;
	gps.run(work, pOut, bsp);
}

PFN_PIXEL_FUNC InterpColorShaderTable[NUM_ZFUNC][2] = {
	interpColorShader<ZFUNC_ALWAYS, false>,
	interpColorShader<ZFUNC_ALWAYS, true>,
	interpColorShader<ZFUNC_LE, false>,
	interpColorShader<ZFUNC_LE, true>,
	interpColorShader<ZFUNC_LT, false>,
	interpColorShader<ZFUNC_LT, true>,
	interpColorShader<ZFUNC_GT, false>,
	interpColorShader<ZFUNC_GT, true>,
	interpColorShader<ZFUNC_GE, false>,
	interpColorShader<ZFUNC_GE, true>,
	interpColorShader<ZFUNC_NEVER, false>,
	interpColorShader<ZFUNC_NEVER, true>,
	interpColorShader<ZFUNC_EQ, false>,
	interpColorShader<ZFUNC_EQ, true>,
};

PFN_PIXEL_FUNC InterpColorTexShaderTable[NUM_ZFUNC][2] = {
	interpPosShaderSample<ZFUNC_ALWAYS, false>,
	interpPosShaderSample<ZFUNC_ALWAYS, true>,
	interpPosShaderSample<ZFUNC_LE, false>,
	interpPosShaderSample<ZFUNC_LE, true>,
	interpPosShaderSample<ZFUNC_LT, false>,
	interpPosShaderSample<ZFUNC_LT, true>,
	interpPosShaderSample<ZFUNC_GT, false>,
	interpPosShaderSample<ZFUNC_GT, true>,
	interpPosShaderSample<ZFUNC_GE, false>,
	interpPosShaderSample<ZFUNC_GE, true>,
	interpPosShaderSample<ZFUNC_NEVER, false>,
	interpPosShaderSample<ZFUNC_NEVER, true>,
	interpPosShaderSample<ZFUNC_EQ, false>,
	interpPosShaderSample<ZFUNC_EQ, true>,
};

PFN_PIXEL_FUNC InterpColorTexBlendShaderTable[NUM_ZFUNC][2] = {
	interpPosShaderSampleBlend<ZFUNC_ALWAYS, false>,
	interpPosShaderSampleBlend<ZFUNC_ALWAYS, true>,
	interpPosShaderSampleBlend<ZFUNC_LE, false>,
	interpPosShaderSampleBlend<ZFUNC_LE, true>,
	interpPosShaderSampleBlend<ZFUNC_LT, false>,
	interpPosShaderSampleBlend<ZFUNC_LT, true>,
	interpPosShaderSampleBlend<ZFUNC_GT, false>,
	interpPosShaderSampleBlend<ZFUNC_GT, true>,
	interpPosShaderSampleBlend<ZFUNC_GE, false>,
	interpPosShaderSampleBlend<ZFUNC_GE, true>,
	interpPosShaderSampleBlend<ZFUNC_NEVER, false>,
	interpPosShaderSampleBlend<ZFUNC_NEVER, true>,
	interpPosShaderSampleBlend<ZFUNC_EQ, false>,
	interpPosShaderSampleBlend<ZFUNC_EQ, true>,
};

PFN_PIXEL_FUNC InterpColorTexBlendAlphaShaderTable[NUM_ZFUNC][2] = {
	interpPosShaderSampleBlendAlpha<ZFUNC_ALWAYS, false>,
	interpPosShaderSampleBlendAlpha<ZFUNC_ALWAYS, true>,
	interpPosShaderSampleBlendAlpha<ZFUNC_LE, false>,
	interpPosShaderSampleBlendAlpha<ZFUNC_LE, true>,
	interpPosShaderSampleBlendAlpha<ZFUNC_LT, false>,
	interpPosShaderSampleBlendAlpha<ZFUNC_LT, true>,
	interpPosShaderSampleBlendAlpha<ZFUNC_GT, false>,
	interpPosShaderSampleBlendAlpha<ZFUNC_GT, true>,
	interpPosShaderSampleBlendAlpha<ZFUNC_GE, false>,
	interpPosShaderSampleBlendAlpha<ZFUNC_GE, true>,
	interpPosShaderSampleBlendAlpha<ZFUNC_NEVER, false>,
	interpPosShaderSampleBlendAlpha<ZFUNC_NEVER, true>,
	interpPosShaderSampleBlendAlpha<ZFUNC_EQ, false>,
	interpPosShaderSampleBlendAlpha<ZFUNC_EQ, true>,
};

PFN_PIXEL_FUNC GLPSLightShaderTable[NUM_ZFUNC][2] = {
	GLPSLightShader<ZFUNC_ALWAYS, false>,
	GLPSLightShader<ZFUNC_ALWAYS, true>,
	GLPSLightShader<ZFUNC_LE, false>,
	GLPSLightShader<ZFUNC_LE, true>,
	GLPSLightShader<ZFUNC_LT, false>,
	GLPSLightShader<ZFUNC_LT, true>,
	GLPSLightShader<ZFUNC_GT, false>,
	GLPSLightShader<ZFUNC_GT, true>,
	GLPSLightShader<ZFUNC_GE, false>,
	GLPSLightShader<ZFUNC_GE, true>,
	GLPSLightShader<ZFUNC_NEVER, false>,
	GLPSLightShader<ZFUNC_NEVER, true>,
	GLPSLightShader<ZFUNC_EQ, false>,
	GLPSLightShader<ZFUNC_EQ, true>,
};

struct ConstantColor
{
	enum
	{
		NUM_INTERPOLANTS	= 0,
		NUM_ATTRIBUTES		= 0,
		DO_PERSPECTIVE		= 0,
	};

	static const UINT SIGNATURE[1];

	__m128 vConstantColor;

	ConstantColor()
	{
		vConstantColor = _mm_set1_ps(1.0f); // color is an ARGB integer but this makes red and is fine for me.
	}
};

const UINT ConstantColor::SIGNATURE[1] = { 0 };

INLINE __m128 shade(ConstantColor const& cc, const SWR_TRIANGLE_DESC&, WideVector<ConstantColor::NUM_ATTRIBUTES, __m128> const&, BYTE*, BYTE*, UINT*)
{
	return cc.vConstantColor;
}

void constantColorShader(const SWR_TRIANGLE_DESC &work, SWR_PIXELOUTPUT& pOut)
{
	ConstantColor cc;
	GenericPixelShader<ConstantColor> gps;
	gps.run(work, pOut, cc);
}

#endif
