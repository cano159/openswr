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

#include "api.h"
#include "shaders.h"

#if KNOB_VERTICALIZED_FE

#if KNOB_VS_SIMD_WIDTH == 8
INLINE
void vTranspose8x4(simdvector &dst, __m128 &row0, __m128 &row1, __m128 &row2, __m128 &row3, __m128 &row4, __m128 &row5, __m128 &row6, __m128 &row7)
{
#if 1
    __m256 r0r4 = _mm256_insertf128_ps(_mm256_castps128_ps256(row0), row4, 0x1); // x0y0z0w0x4y4z4w4
    __m256 r2r6 = _mm256_insertf128_ps(_mm256_castps128_ps256(row2), row6, 0x1); // x2y2z2w2x6y6z6w6
    __m256 r1r5 = _mm256_insertf128_ps(_mm256_castps128_ps256(row1), row5, 0x1); // x1y1z1w1x5y5z5w5
    __m256 r3r7 = _mm256_insertf128_ps(_mm256_castps128_ps256(row3), row7, 0x1); // x3y3z3w3x7y7z7w7

    __m256 xyeven = _mm256_unpacklo_ps(r0r4, r2r6); // x0x2y0y2x4x6y4y6
    __m256 zweven = _mm256_unpackhi_ps(r0r4, r2r6); // z0z2w0w2z4z6w4w6
    __m256 xyodd = _mm256_unpacklo_ps(r1r5, r3r7);  // x1x3y1y3x5x7y5y7
    __m256 zwodd = _mm256_unpackhi_ps(r1r5, r3r7);  // z1z3w1w3z5z7w5w7

    dst.v[0] = _mm256_unpacklo_ps(xyeven, xyodd); // x0x1x2x3x4x5x6x7
    dst.v[1] = _mm256_unpackhi_ps(xyeven, xyodd); // y0y1y2y3y4y5y6y7
    dst.v[2] = _mm256_unpacklo_ps(zweven, zwodd); // z0z1z2z3z4z5z6z7
    dst.v[3] = _mm256_unpackhi_ps(zweven, zwodd); // w0w1w2w3w4w5w6w7
#else
    vTranspose(row0, row1, row2, row3);
    vTranspose(row4, row5, row6, row7);
    dst.v[0] = _mm256_castps128_ps256(row0);
    dst.v[1] = _mm256_castps128_ps256(row1);
    dst.v[2] = _mm256_castps128_ps256(row2);
    dst.v[3] = _mm256_castps128_ps256(row3);

    dst.v[0] = _mm256_insertf128_ps(dst.v[0], row4, 1);
    dst.v[1] = _mm256_insertf128_ps(dst.v[1], row5, 1);
    dst.v[2] = _mm256_insertf128_ps(dst.v[2], row6, 1);
    dst.v[3] = _mm256_insertf128_ps(dst.v[3], row7, 1);
#endif
}
INLINE
void vTranspose8x3(simdvector &dst, __m128 &row0, __m128 &row1, __m128 &row2, __m128 &row3, __m128 &row4, __m128 &row5)
{
    __m256 m03;
    __m256 m14;
    __m256 m25;

    m03 = _mm256_castps128_ps256(row0); // load lower halves
    m14 = _mm256_castps128_ps256(row1);
    m25 = _mm256_castps128_ps256(row2);
    m03 = _mm256_insertf128_ps(m03, row3, 1); // load upper halves
    m14 = _mm256_insertf128_ps(m14, row4, 1);
    m25 = _mm256_insertf128_ps(m25, row5, 1);

    __m256 xy = _mm256_shuffle_ps(m14, m25, _MM_SHUFFLE(2, 1, 3, 2)); // upper x's and y's
    __m256 yz = _mm256_shuffle_ps(m03, m14, _MM_SHUFFLE(1, 0, 2, 1)); // lower y's and z's
    dst.v[0] = _mm256_shuffle_ps(m03, xy, _MM_SHUFFLE(2, 0, 3, 0));
    dst.v[1] = _mm256_shuffle_ps(yz, xy, _MM_SHUFFLE(3, 1, 2, 0));
    dst.v[2] = _mm256_shuffle_ps(yz, m25, _MM_SHUFFLE(3, 0, 3, 1));
    dst.v[3] = _mm256_set1_ps(1.0f);
}

#endif

#if KNOB_JIT_FETCHSHADER_VIZ
void FetchStandard(SWR_FETCH_INFO &fetchInfo, VERTEXINPUT &out)
{
    const INT *pVbIndex = fetchInfo.pIndices;

    const INPUT_ELEMENT_DESC *pPosElem = &((const INPUT_ELEMENT_DESC *)fetchInfo.pConstants)[1];
    const INPUT_ELEMENT_DESC *pNormalElem = &((const INPUT_ELEMENT_DESC *)fetchInfo.pConstants)[2];

    FLOAT *pPositionVB = fetchInfo.ppStreams[pPosElem->StreamIndex];
    FLOAT *pNormalVB = fetchInfo.ppStreams[pNormalElem->StreamIndex];

    UINT posElemOffset = (pPosElem->AlignedByteOffset / sizeof(FLOAT));
    UINT normalElemOffset = (pNormalElem->AlignedByteOffset / sizeof(FLOAT));

    UINT vsIndex = pVbIndex[0];
    UINT vbPosOffset = (((UINT *)fetchInfo.pConstants)[3 + pPosElem->StreamIndex] / sizeof(FLOAT)) * vsIndex;
    UINT vbNormalOffset = (((UINT *)fetchInfo.pConstants)[3 + pNormalElem->StreamIndex] / sizeof(FLOAT)) * vsIndex;

    FLOAT *pPosVertex = pPositionVB + vbPosOffset + posElemOffset;
    FLOAT *pNormalVertex = pNormalVB + vbNormalOffset + normalElemOffset;

    __m128 tmp[KNOB_VS_SIMD_WIDTH];
    tmp[0] = _mm_loadu_ps(pPosVertex);
    tmp[1] = _mm_loadu_ps(pPosVertex + 4);
    tmp[2] = _mm_loadu_ps(pPosVertex + 8);
    tmp[3] = _mm_loadu_ps(pPosVertex + 12);
    tmp[4] = _mm_loadu_ps(pPosVertex + 16);
    tmp[5] = _mm_loadu_ps(pPosVertex + 20);
    //tmp[6] = _mm_loadu_ps(pPosVertex+24);
    //tmp[7] = _mm_loadu_ps(pPosVertex+28);
    //vTranspose8x4(out.vertex[VS_SLOT_POSITION], tmp[0], tmp[1], tmp[2], tmp[3], tmp[4], tmp[5], tmp[6], tmp[7]);
    vTranspose8x3(out.vertex[VS_SLOT_POSITION], tmp[0], tmp[1], tmp[2], tmp[3], tmp[4], tmp[5]);

    tmp[0] = _mm_loadu_ps(pNormalVertex);
    tmp[1] = _mm_loadu_ps(pNormalVertex + 4);
    tmp[2] = _mm_loadu_ps(pNormalVertex + 8);
    tmp[3] = _mm_loadu_ps(pNormalVertex + 12);
    tmp[4] = _mm_loadu_ps(pNormalVertex + 16);
    tmp[5] = _mm_loadu_ps(pNormalVertex + 20);
    //tmp[6] = _mm_loadu_ps(pNormalVertex+24);
    //tmp[7] = _mm_loadu_ps(pNormalVertex+28);
    //vTranspose8x4(out.vertex[VS_SLOT_NORMAL], tmp[0], tmp[1], tmp[2], tmp[3], tmp[4], tmp[5], tmp[6], tmp[7]);
    vTranspose8x3(out.vertex[VS_SLOT_NORMAL], tmp[0], tmp[1], tmp[2], tmp[3], tmp[4], tmp[5]);
}
#else
void FetchStandard(SWR_FETCH_INFO &fetchInfo, VERTEXINPUT &out)
{
    const INT *pVbIndex = fetchInfo.pIndices;
    const UINT NumIEDs = ((const UINT *)fetchInfo.pConstants)[0];
    const auto *pElems = &((const INPUT_ELEMENT_DESC *)fetchInfo.pConstants)[1];
    const UINT *pStrides = &((const UINT *)fetchInfo.pConstants)[NumIEDs + 1];

    for (UINT elem = 0; elem < NumIEDs; ++elem)
    {
        const INPUT_ELEMENT_DESC *pElem = &pElems[elem];

        UINT numComps = SwrNumComponents((SWR_FORMAT)pElem->Format);
        UINT elemOffset = (pElem->AlignedByteOffset / sizeof(FLOAT));
        UINT slot = 0;

        FLOAT *pVertexBuffer = fetchInfo.ppStreams[pElem->StreamIndex];

        switch (pElem->Semantic)
        {
        case POSITION:
            slot = VS_SLOT_POSITION;
            break;
        case NORMAL:
            slot = VS_SLOT_NORMAL;
            break;
        case COLOR:
            slot = VS_SLOT_COLOR0;
            break;
        case TEXCOORD:
            slot = VS_SLOT_TEXCOORD0;
            break;
        default:
            break;
        };

#if KNOB_VS_SIMD_WIDTH == 8
        __m128 tmp[KNOB_VS_SIMD_WIDTH];
#endif
        // Load a full simd vector
        for (UINT simd = 0; simd < KNOB_VS_SIMD_WIDTH; ++simd)
        {
            UINT vsIndex = pVbIndex[0] + simd; // grab Nth index for simd
            UINT vbOffset = (pStrides[pElem->StreamIndex] / sizeof(FLOAT)) * vsIndex;

#if KNOB_VS_SIMD_WIDTH == 4
            out.vertex[slot].v[simd] = _mm_loadu_ps(pVertexBuffer + vbOffset + elemOffset);
#else
            tmp[simd] = _mm_loadu_ps(pVertexBuffer + vbOffset + elemOffset);

            if (slot == VS_SLOT_COLOR0 && pElem->Format >= R8_UNORM)
            {
                __m128i tmpint = _mm_castps_si128(tmp[simd]);
                tmpint = _mm_cvtepu8_epi32(tmpint);
                tmp[simd] = _mm_cvtepi32_ps(tmpint);
                tmp[simd] = _mm_mul_ps(tmp[simd], _mm_set1_ps((float)(1.0 / 255.0)));
            }
#endif
        }

// Convert AOS to SOA
#if KNOB_VS_SIMD_WIDTH == 4
        _simdvec_transpose(out.vertex[slot]);
#else
        vTranspose8x4(out.vertex[slot + pElem->SemanticIndex], tmp[0], tmp[1], tmp[2], tmp[3], tmp[4], tmp[5], tmp[6], tmp[7]);
#endif

        // Handle default components for SOA-4.
        switch (numComps)
        {
        case 1:
            out.vertex[slot].v[1] = _simd_setzero_ps(); // initialize y's to 0
        case 2:
            out.vertex[slot].v[2] = _simd_setzero_ps(); // initialize z's to 0
        case 3:
            out.vertex[slot].v[3] = _simd_set1_ps(1.0f); // initialize w's to 1
        default:
            break;
        }
    }
}

#endif

void VertexFuncPos(
    const VERTEXINPUT &in,
    VERTEXOUTPUT &out)
{
    const FLOAT *pWorldMat = (FLOAT *)in.pConstants;

    _simd_mat4x4_vec3_w1_multiply(out.vertex[VS_SLOT_POSITION], pWorldMat, in.vertex[VS_SLOT_POSITION]);
}

// XXX: Put function in shader_math.h
INLINE
simdscalar _simdvec_diffuse_ps(const simdvector &normal, const simdvector &lightDir)
{
    simdscalar ndotl;
    _simdvec_dp3_ps(ndotl, normal, lightDir);
    return _simd_max_ps(ndotl, _simd_setzero_ps());
}

#if KNOB_JIT_VERTEXSHADER_VIZ
void GLVSLightShader(
    const VERTEXINPUT &in,
    VERTEXOUTPUT &out)
{
    const FLOAT *pConstants = (FLOAT *)in.pConstants;
    const FLOAT *pMVP = &pConstants[GLVSLightConstants::MVP_MATRIX];
    const FLOAT *pMV = &pConstants[GLVSLightConstants::MV_MATRIX];
    const FLOAT *pNormalMat = &pConstants[GLVSLightConstants::MV_MATRIX]; // XXX: Using model view for normal matrix.
    const FLOAT *pLightPos0 = &pConstants[GLVSLightConstants::LIGHT0_POS];
    const FLOAT *pLightDiffuse0 = &pConstants[GLVSLightConstants::LIGHT0_DIFFUSE];
    const FLOAT *pLightAmbient0 = &pConstants[GLVSLightConstants::LIGHT0_AMBIENT];

    const FLOAT *pFrontSceneColor = &pConstants[GLVSLightConstants::FRONT_SCENE_COLOR];
    const FLOAT *pFrontMatDiffuse = &pConstants[GLVSLightConstants::FRONT_MAT_DIFFUSE];
    const FLOAT *pFrontMatAmbient = &pConstants[GLVSLightConstants::FRONT_MAT_AMBIENT];

    const FLOAT *pBackSceneColor = &pConstants[GLVSLightConstants::BACK_SCENE_COLOR];
    const FLOAT *pBackMatDiffuse = &pConstants[GLVSLightConstants::BACK_MAT_DIFFUSE];
    const FLOAT *pBackMatAmbient = &pConstants[GLVSLightConstants::BACK_MAT_AMBIENT];

    // Compute position
    _simd_mat4x3_vec3_w1_multiply(out.vertex[VS_SLOT_POSITION], pMVP, in.vertex[VS_SLOT_POSITION]);

    // Transform normals
    simdvector vNormal;
    _simd_mat3x3_vec3_w0_multiply(vNormal, pNormalMat, in.vertex[VS_SLOT_NORMAL]);

    // Normalize the light. XXX shouldn't need to do this. The driver should normalize it.
    simdvector vLightDir;
    _simdvec_load_ps(vLightDir, pLightPos0); // LightPos contains the direction for infinite lights.
    //_simdvec_normalize_ps(vLightDir, vLightDir);

    // N.L and clamp
    simdscalar ndotl = _simdvec_diffuse_ps(vNormal, vLightDir);

    // Invert the normal
    _simdvec_mul_ps(vNormal, vNormal, _simd_set1_ps(-1.0f));
    simdscalar ndotl_invert = _simdvec_diffuse_ps(vNormal, vLightDir);

    // front color = front scene color + light_ambient * front_mat_ambient + light_diffuse * front_mat_diffuse;
    simdvector vFrontColor;
    _simdvec_load_ps(vFrontColor, pFrontSceneColor);

    //simdvector vMatAmbient;
    //_simdvec_load_ps(vMatAmbient, pFrontMatAmbient);

    //simdvector vLightAmbient;
    //_simdvec_load_ps(vLightAmbient, pLightAmbient0);

    //simdvector vAmbient;
    //_simdvec_mul_ps(vAmbient, vLightAmbient, vMatAmbient);

    // front color = front scene color + light_ambient * front_mat_ambient
    //_simdvec_add_ps(vFrontColor, vFrontColor, vAmbient);

    // Light diffuse * N.L
    simdvector vLightDiffuse;
    _simdvec_load_ps(vLightDiffuse, pLightDiffuse0);

    simdvector vLightDiffuseFront;
    simdvector vLightDiffuseBack;
    _simdvec_mul_ps(vLightDiffuseFront, vLightDiffuse, ndotl);
    _simdvec_mul_ps(vLightDiffuseBack, vLightDiffuse, ndotl_invert);

    simdvector vMatDiffuse;
    _simdvec_load_ps(vMatDiffuse, pFrontMatDiffuse);
    _simdvec_mul_ps(vLightDiffuseFront, vLightDiffuseFront, vMatDiffuse);
    _simdvec_mul_ps(vLightDiffuseBack, vLightDiffuseBack, vMatDiffuse);

    _simdvec_add_ps(vLightDiffuseFront, vLightDiffuseFront, vFrontColor);
    _simdvec_add_ps(vLightDiffuseBack, vLightDiffuseBack, vFrontColor);

    // Store output color
    _simdvec_max_ps(out.vertex[VS_SLOT_COLOR0], vLightDiffuseFront, _simd_setzero_ps());
    _simdvec_max_ps(out.vertex[VS_SLOT_COLOR1], vLightDiffuseBack, _simd_setzero_ps());
}
#else
void GLVSLightShader(
    const VERTEXINPUT &in,
    VERTEXOUTPUT &out)
{
    const FLOAT *pConstants = (FLOAT *)in.pConstants;
    const FLOAT *pMVP = &pConstants[GLVSLightConstants::MVP_MATRIX];
    const FLOAT *pNormalMat = &pConstants[GLVSLightConstants::MV_MATRIX]; // XXX: Using model view for normal matrix.
    const FLOAT *pLightPos0 = &pConstants[GLVSLightConstants::LIGHT0_POS];
    const FLOAT *pLightDiffuse0 = &pConstants[GLVSLightConstants::LIGHT0_DIFFUSE];
    const FLOAT *pLightAmbient0 = &pConstants[GLVSLightConstants::LIGHT0_AMBIENT];

    const FLOAT *pFrontSceneColor = &pConstants[GLVSLightConstants::FRONT_SCENE_COLOR];
    const FLOAT *pFrontMatDiffuse = &pConstants[GLVSLightConstants::FRONT_MAT_DIFFUSE];
    const FLOAT *pFrontMatAmbient = &pConstants[GLVSLightConstants::FRONT_MAT_AMBIENT];

    const FLOAT *pBackSceneColor = &pConstants[GLVSLightConstants::BACK_SCENE_COLOR];
    const FLOAT *pBackMatDiffuse = &pConstants[GLVSLightConstants::BACK_MAT_DIFFUSE];
    const FLOAT *pBackMatAmbient = &pConstants[GLVSLightConstants::BACK_MAT_AMBIENT];

    // Compute position
    _simd_mat4x3_vec3_w1_multiply(out.vertex[VS_SLOT_POSITION], pMVP, in.vertex[VS_SLOT_POSITION]);

    // Transform normals
    simdvector vNormal;
    _simd_mat3x3_vec3_w0_multiply(vNormal, pNormalMat, in.vertex[VS_SLOT_NORMAL]);

    // Normalize the light. XXX shouldn't need to do this. The driver should normalize it.
    simdvector vLightDir;
    _simdvec_load_ps(vLightDir, pLightPos0); // LightPos contains the direction for infinite lights.
    _simdvec_normalize_ps(vLightDir, vLightDir);

    // N.L and clamp
    simdscalar ndotl = _simdvec_diffuse_ps(vNormal, vLightDir);

    // Light diffuse * N.L
    simdvector vLightDiffuse;
    _simdvec_load_ps(vLightDiffuse, pLightDiffuse0);
    _simdvec_mul_ps(vLightDiffuse, vLightDiffuse, ndotl);

    // front color = front scene color + light_ambient * front_mat_ambient + light_diffuse * front_mat_diffuse;
    simdvector vFrontColor;
    _simdvec_load_ps(vFrontColor, pFrontSceneColor);

    simdvector vMatDiffuse;
    _simdvec_load_ps(vMatDiffuse, pFrontMatDiffuse);
    _simdvec_mul_ps(vLightDiffuse, vLightDiffuse, vMatDiffuse);
    _simdvec_add_ps(vFrontColor, vFrontColor, vLightDiffuse);

    simdvector vMatAmbient;
    _simdvec_load_ps(vMatAmbient, pFrontMatAmbient);

    simdvector vLightAmbient;
    _simdvec_load_ps(vLightAmbient, pLightAmbient0);

    simdvector vAmbient;
    _simdvec_mul_ps(vAmbient, vLightAmbient, vMatAmbient);
    _simdvec_add_ps(vFrontColor, vFrontColor, vAmbient);

    // Clamp to [0, 1]
    _simdvec_max_ps(vFrontColor, vFrontColor, _simd_setzero_ps());
    _simdvec_min_ps(vFrontColor, vFrontColor, _simd_set1_ps(1.0f));

    // Store front color
    _simdvec_mov(out.vertex[VS_SLOT_COLOR0], vFrontColor);

    // Back color
    // Invert the normal
    _simdvec_mul_ps(vNormal, vNormal, _simd_set1_ps(-1.0f));
    ndotl = _simdvec_diffuse_ps(vNormal, vLightDir);

    // Light diffuse * N.L
    _simdvec_load_ps(vLightDiffuse, pLightDiffuse0);
    _simdvec_mul_ps(vLightDiffuse, vLightDiffuse, ndotl);

    simdvector vBackColor;
    _simdvec_load_ps(vBackColor, pBackSceneColor);

    _simdvec_load_ps(vMatDiffuse, pBackMatDiffuse);
    _simdvec_mul_ps(vLightDiffuse, vLightDiffuse, vMatDiffuse);
    _simdvec_add_ps(vBackColor, vBackColor, vLightDiffuse);

    _simdvec_load_ps(vMatAmbient, pBackMatAmbient);

    _simdvec_load_ps(vLightAmbient, pLightAmbient0);

    _simdvec_mul_ps(vAmbient, vLightAmbient, vMatAmbient);
    _simdvec_add_ps(vBackColor, vBackColor, vAmbient);

    // Clamp to [0, 1]
    _simdvec_max_ps(vBackColor, vBackColor, _simd_setzero_ps());
    _simdvec_min_ps(vBackColor, vBackColor, _simd_set1_ps(1.0f));

    // Store back color
    _simdvec_mov(out.vertex[VS_SLOT_COLOR1], vBackColor);
}
#endif

void GLVSPosColor(
    const VERTEXINPUT &in,
    VERTEXOUTPUT &out)
{
    const FLOAT *pConstants = (FLOAT *)in.pConstants;
    const FLOAT *pMVP = &pConstants[GLVSLightConstants::MVP_MATRIX];

    // Compute position
    _simd_mat4x4_vec3_w1_multiply(out.vertex[VS_SLOT_POSITION], pMVP, in.vertex[VS_SLOT_POSITION]);

    // Pass-thru color and texcoord
    _simdvec_mov(out.vertex[VS_SLOT_COLOR0], in.vertex[VS_SLOT_COLOR0]);
}

void GLVSPosColorTexCoordShader(
    const VERTEXINPUT &in,
    VERTEXOUTPUT &out)
{
    const FLOAT *pConstants = (FLOAT *)in.pConstants;
    const FLOAT *pMVP = &pConstants[GLVSLightConstants::MVP_MATRIX];

    // Compute position
    _simd_mat4x4_vec3_w1_multiply(out.vertex[VS_SLOT_POSITION], pMVP, in.vertex[VS_SLOT_POSITION]);

    // Pass-thru color and texcoord
    _simdvec_mov(out.vertex[VS_SLOT_COLOR0], in.vertex[VS_SLOT_COLOR0]);
    _simdvec_mov(out.vertex[VS_SLOT_TEXCOORD0], in.vertex[VS_SLOT_TEXCOORD0]);
}

#endif
