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

#include "oglstate.hpp"
#include "texture_unit.h"
#include "shader_math.h"
#include "widevector.hpp"

void GenerateBlendFactor(GLenum func, WideColor &src, WideColor &dst, WideColor &result)
{
    switch (func)
    {
    case GL_ZERO:
        result.R = _simd_setzero_ps();
        result.G = _simd_setzero_ps();
        result.B = _simd_setzero_ps();
        result.A = _simd_setzero_ps();
        break;
    case GL_ONE:
        result.R = _simd_set1_ps(1.0);
        result.G = _simd_set1_ps(1.0);
        result.B = _simd_set1_ps(1.0);
        result.A = _simd_set1_ps(1.0);
        break;
    case GL_SRC_COLOR:
        result = src;
        break;

    case GL_DST_COLOR:
        result = dst;
        break;

    case GL_ONE_MINUS_SRC_COLOR:
        result.R = _simd_sub_ps(_simd_set1_ps(1.0), src.R);
        result.G = _simd_sub_ps(_simd_set1_ps(1.0), src.G);
        result.B = _simd_sub_ps(_simd_set1_ps(1.0), src.B);
        result.A = _simd_sub_ps(_simd_set1_ps(1.0), src.A);
        break;

    case GL_ONE_MINUS_DST_COLOR:
        result.R = _simd_sub_ps(_simd_set1_ps(1.0), dst.R);
        result.G = _simd_sub_ps(_simd_set1_ps(1.0), dst.G);
        result.B = _simd_sub_ps(_simd_set1_ps(1.0), dst.B);
        result.A = _simd_sub_ps(_simd_set1_ps(1.0), dst.A);
        break;

    case GL_SRC_ALPHA:
        result.R = src.A;
        result.G = src.A;
        result.B = src.A;
        result.A = src.A;
        break;

    case GL_ONE_MINUS_SRC_ALPHA:
    {
        simdscalar oneMinusSrcA = _simd_sub_ps(_simd_set1_ps(1.0), src.A);
        result.R = oneMinusSrcA;
        result.G = oneMinusSrcA;
        result.B = oneMinusSrcA;
        result.A = oneMinusSrcA;
    }
    break;

    case GL_DST_ALPHA:
        result.R = dst.A;
        result.G = dst.A;
        result.B = dst.A;
        result.A = dst.A;
        break;

    case GL_ONE_MINUS_DST_ALPHA:
    {
        simdscalar oneMinusDstA = _simd_sub_ps(_simd_set1_ps(1.0), dst.A);
        result.R = oneMinusDstA;
        result.G = oneMinusDstA;
        result.B = oneMinusDstA;
        result.A = oneMinusDstA;
    }
    break;

    case GL_SRC_ALPHA_SATURATE:
    {
        simdscalar sat = _simd_min_ps(src.A, _simd_sub_ps(_simd_set1_ps(1.0), dst.A));
        result.R = sat;
        result.G = sat;
        result.B = sat;
        result.A = _simd_set1_ps(1.0);
    }
    break;
    default:
        assert(0);
    }
}

// Implements the complete OGL1 fragment fixed function pipeline
template <UINT NUM_TEXTURES>
struct FragFF
{
    enum
    {
        NUM_INTERPOLANTS = 1 + NUM_TEXTURES,   // 1 color, NUM_TEXTURES textures
        NUM_ATTRIBUTES = 4 + 2 * NUM_TEXTURES, // 4 color, 2 * NUM_TEXTURES textures
        DO_PERSPECTIVE = 1,
    };

    typedef WideVector<FragFF<NUM_TEXTURES>::NUM_ATTRIBUTES, simdscalar> WV;

    static const UINT SIGNATURE[NUM_INTERPOLANTS];

    FragFF()
    {
    }
};

template <>
const UINT FragFF<0>::SIGNATURE[1] = { 4 };
template <>
const UINT FragFF<1>::SIGNATURE[2] = { 4, 2 };
template <>
const UINT FragFF<2>::SIGNATURE[3] = { 4, 2, 2 };
template <>
const UINT FragFF<3>::SIGNATURE[4] = { 4, 2, 2, 2 };
template <>
const UINT FragFF<4>::SIGNATURE[5] = { 4, 2, 2, 2, 2 };
template <>
const UINT FragFF<5>::SIGNATURE[6] = { 4, 2, 2, 2, 2, 2 };
template <>
const UINT FragFF<6>::SIGNATURE[7] = { 4, 2, 2, 2, 2, 2, 2 };
template <>
const UINT FragFF<7>::SIGNATURE[8] = { 4, 2, 2, 2, 2, 2, 2, 2 };
template <>
const UINT FragFF<8>::SIGNATURE[9] = { 4, 2, 2, 2, 2, 2, 2, 2, 2 };

// assumptions
// textures are RGBA
//
//
//

template <UINT NUM_TEXTURES, UINT INDEX = NUM_TEXTURES>
struct MySample
{
    typedef WideVector<FragFF<NUM_TEXTURES>::NUM_ATTRIBUTES, simdscalar> WV;

    static INLINE WideColor sample(const SWR_TRIANGLE_DESC &work, const OGL::SaveableState &state, WV const &pAttrs, WideColor &fragColor)
    {
        fragColor = MySample<NUM_TEXTURES, INDEX - 1>::sample(work, state, pAttrs, fragColor);

        TexCoord tcidx;
        const TextureView &tv = *(const TextureView *)work.pTextureViews[INDEX - 1];
        const Sampler &samp = *(const Sampler *)work.pSamplers[INDEX - 1];

        const OGL::TextureUnit &texUnit = state.mTexUnit[INDEX - 1];
        tcidx.U = get<4 + (INDEX - 1) * 2>(pAttrs);
        tcidx.V = get<5 + (INDEX - 1) * 2>(pAttrs);
        UINT mips[] = { 0, 0, 0, 0 };
        WideColor texColor;
        SampleSimplePointRGBAF32(tv, samp, tcidx, mips, texColor);

        switch (texUnit.mTexEnv.mMode)
        {
        case GL_REPLACE:
            fragColor = texColor;
            break;
        case GL_MODULATE:
            fragColor.R = _simd_mul_ps(fragColor.R, texColor.R);
            fragColor.G = _simd_mul_ps(fragColor.G, texColor.G);
            fragColor.B = _simd_mul_ps(fragColor.B, texColor.B);
            fragColor.A = _simd_mul_ps(fragColor.A, texColor.A);
            break;
        case GL_DECAL:
        {
            simdscalar oneMinusAlpha = _simd_sub_ps(_simd_set1_ps(1.0), texColor.A);
            simdscalar RsAs = _simd_mul_ps(texColor.R, texColor.A);
            simdscalar GsAs = _simd_mul_ps(texColor.G, texColor.A);
            simdscalar BsAs = _simd_mul_ps(texColor.B, texColor.A);
            fragColor.R = _simd_mul_ps(fragColor.R, oneMinusAlpha);
            fragColor.G = _simd_mul_ps(fragColor.G, oneMinusAlpha);
            fragColor.B = _simd_mul_ps(fragColor.B, oneMinusAlpha);
            fragColor.R = _simd_add_ps(fragColor.R, RsAs);
            fragColor.G = _simd_add_ps(fragColor.G, GsAs);
            fragColor.B = _simd_add_ps(fragColor.B, BsAs);
        }
        /* fragColor.A = fragColor.A; */
        break;
        default:
            assert(0);
        }

        return fragColor;
    }
};

template <UINT NUM_TEXTURES>
struct MySample<NUM_TEXTURES, 0>
{
    typedef WideVector<FragFF<NUM_TEXTURES>::NUM_ATTRIBUTES, simdscalar> WV;

    static INLINE WideColor sample(const SWR_TRIANGLE_DESC &work, const OGL::SaveableState &state, WV const &pAttrs, WideColor &fragColor)
    {
        return fragColor;
    }
};

template <UINT NUM_TEXTURES>
INLINE simdscalar shade(FragFF<NUM_TEXTURES> const &fragFF, const SWR_TRIANGLE_DESC &work, WideVector<FragFF<NUM_TEXTURES>::NUM_ATTRIBUTES, simdscalar> const &pAttrs, BYTE *pBuffer, BYTE *, UINT *outMask)
{
#if KNOB_VS_SIMD_WIDTH == 4
    const __m128i SHUF_ALPHA = _mm_set_epi32(0x8080800f, 0x8080800b, 0x80808007, 0x80808003);
    const __m128i SHUF_RED = _mm_set_epi32(0x8080800e, 0x8080800a, 0x80808006, 0x80808002);
    const __m128i SHUF_GREEN = _mm_set_epi32(0x8080800d, 0x80808009, 0x80808005, 0x80808001);
    const __m128i SHUF_BLUE = _mm_set_epi32(0x8080800c, 0x80808008, 0x80808004, 0x80808000);
#elif KNOB_VS_SIMD_WIDTH == 8
    const __m256i SHUF_ALPHA = _mm256_set_epi32(0x8080800f, 0x8080800b, 0x80808007, 0x80808003, 0x8080800f, 0x8080800b, 0x80808007, 0x80808003);
    const __m256i SHUF_RED = _mm256_set_epi32(0x8080800e, 0x8080800a, 0x80808006, 0x80808002, 0x8080800e, 0x8080800a, 0x80808006, 0x80808002);
    const __m256i SHUF_GREEN = _mm256_set_epi32(0x8080800d, 0x80808009, 0x80808005, 0x80808001, 0x8080800d, 0x80808009, 0x80808005, 0x80808001);
    const __m256i SHUF_BLUE = _mm256_set_epi32(0x8080800c, 0x80808008, 0x80808004, 0x80808000, 0x8080800c, 0x80808008, 0x80808004, 0x80808000);
#endif

    const OGL::SaveableState &state = *(const OGL::SaveableState *)work.pConstants;

    // Interpolated primary fragment color
    WideColor fragColor;
    fragColor.R = get<0>(pAttrs);
    fragColor.G = get<1>(pAttrs);
    fragColor.B = get<2>(pAttrs);
    fragColor.A = get<3>(pAttrs);

    // Sample
    fragColor = MySample<NUM_TEXTURES>::sample(work, state, pAttrs, fragColor);

    simdscalar vCoverage = _simd_set1_ps(-1.0);

    // Scissor
    if (state.mCaps.scissorTest)
    {
    }

    // Alpha test
    // @todo spec states alpha test should be done in fixed point (whatever!)
    if (state.mCaps.alphatest)
    {
        simdscalar vRef = _simd_set1_ps(state.mAlphaRef);

        switch (state.mAlphaFunc)
        {
        case GL_NEVER:
            vCoverage = _simd_setzero_ps();
            break;
        case GL_ALWAYS:
            vCoverage = vCoverage;
            break;
        case GL_LESS:
            vCoverage = _simd_and_ps(vCoverage, _simd_cmplt_ps(fragColor.A, vRef));
            break;
        case GL_LEQUAL:
            vCoverage = _simd_and_ps(vCoverage, _simd_cmple_ps(fragColor.A, vRef));
            break;
        case GL_EQUAL:
            vCoverage = _simd_and_ps(vCoverage, _simd_cmpeq_ps(fragColor.A, vRef));
            break;
        case GL_GEQUAL:
            vCoverage = _simd_and_ps(vCoverage, _simd_cmpge_ps(fragColor.A, vRef));
            break;
        case GL_GREATER:
            vCoverage = _simd_and_ps(vCoverage, _simd_cmpgt_ps(fragColor.A, vRef));
            break;
        case GL_NOTEQUAL:
            vCoverage = _simd_and_ps(vCoverage, _simd_cmpneq_ps(fragColor.A, vRef));
            break;
        default:
            assert(0);
        }
    }

    *outMask = _simd_movemask_ps(vCoverage);

    // Stencil test

    // Blend
    if (state.mCaps.blend)
    {
        WideColor src, dst;

        // read frag from framebuffer, unpack, and convert to float
        simdscalari vColorBuffer = _simd_load_si((const simdscalari *)pBuffer);

        simdscalari vDstAlpha = _simd_shuffle_epi8(vColorBuffer, SHUF_ALPHA);
        simdscalari vDstRed = _simd_shuffle_epi8(vColorBuffer, SHUF_RED);
        simdscalari vDstGreen = _simd_shuffle_epi8(vColorBuffer, SHUF_GREEN);
        simdscalari vDstBlue = _simd_shuffle_epi8(vColorBuffer, SHUF_BLUE);

        // convert to float
        WideColor dstColor;
        dstColor.R = vUnormToFloat(vDstRed);
        dstColor.G = vUnormToFloat(vDstGreen);
        dstColor.B = vUnormToFloat(vDstBlue);
        dstColor.A = vUnormToFloat(vDstAlpha);

        GenerateBlendFactor(state.mBlendFuncSFactor, fragColor, dstColor, src);
        GenerateBlendFactor(state.mBlendFuncDFactor, fragColor, dstColor, dst);

        fragColor.R = _simd_min_ps(_simd_fmadd_ps(src.R, fragColor.R, _simd_mul_ps(dst.R, dstColor.R)), _simd_set1_ps(1.0));
        fragColor.G = _simd_min_ps(_simd_fmadd_ps(src.G, fragColor.G, _simd_mul_ps(dst.G, dstColor.G)), _simd_set1_ps(1.0));
        fragColor.B = _simd_min_ps(_simd_fmadd_ps(src.B, fragColor.B, _simd_mul_ps(dst.B, dstColor.B)), _simd_set1_ps(1.0));
        fragColor.A = _simd_min_ps(_simd_fmadd_ps(src.A, fragColor.A, _simd_mul_ps(dst.A, dstColor.A)), _simd_set1_ps(1.0));
    }

    // Dithering (not)

    // Logical Op (not)

    // Pack fragment
    simdscalari vFragR = vFloatToUnorm(fragColor.R);
    simdscalari vFragG = vFloatToUnorm(fragColor.G);
    simdscalari vFragB = vFloatToUnorm(fragColor.B);
    simdscalari vFragA = vFloatToUnorm(fragColor.A);

    // ColorMask
    // read frag from framebuffer, unpack, and convert to float
    simdscalari vColorBuffer = _simd_load_si((const simdscalari *)pBuffer);
    if (!state.mColorMask.red)
    {
        simdscalari vDstRed = _simd_shuffle_epi8(vColorBuffer, SHUF_RED);
        vFragR = vDstRed;
    }
    if (!state.mColorMask.green)
    {
        simdscalari vDstGreen = _simd_shuffle_epi8(vColorBuffer, SHUF_GREEN);
        vFragG = vDstGreen;
    }
    if (!state.mColorMask.blue)
    {
        simdscalari vDstBlue = _simd_shuffle_epi8(vColorBuffer, SHUF_BLUE);
        vFragB = vDstBlue;
    }
    if (!state.mColorMask.alpha)
    {
        simdscalari vDstAlpha = _simd_shuffle_epi8(vColorBuffer, SHUF_ALPHA);
        vFragA = vDstAlpha;
    }

    // pack
    simdscalari vDstPixel = vFragB;
    vDstPixel = _simd_or_si(vDstPixel, _simd_slli_epi32(vFragG, 8));
    vDstPixel = _simd_or_si(vDstPixel, _simd_slli_epi32(vFragR, 16));
    vDstPixel = _simd_or_si(vDstPixel, _simd_slli_epi32(vFragA, 24));

    return _simd_castsi_ps(vDstPixel);
}

template <UINT NUM_TEXTURES, SWR_ZFUNCTION zFunc, bool zWrite>
void GLFragFF(const SWR_TRIANGLE_DESC &work, SWR_PIXELOUTPUT &pOut)
{
    FragFF<NUM_TEXTURES> ff;
    GenericPixelShader<FragFF<NUM_TEXTURES>, zFunc, zWrite> gps;
    gps.run(work, pOut, ff);
}

PFN_PIXEL_FUNC GLFragFFTable[KNOB_NUMBER_OF_TEXTURE_VIEWS + 1][NUM_ZFUNC][2] = {
    GLFragFF<0, ZFUNC_ALWAYS, false>,
    GLFragFF<0, ZFUNC_ALWAYS, true>,
    GLFragFF<0, ZFUNC_LE, false>,
    GLFragFF<0, ZFUNC_LE, true>,
    GLFragFF<0, ZFUNC_LT, false>,
    GLFragFF<0, ZFUNC_LT, true>,
    GLFragFF<0, ZFUNC_GT, false>,
    GLFragFF<0, ZFUNC_GT, true>,
    GLFragFF<0, ZFUNC_GE, false>,
    GLFragFF<0, ZFUNC_GE, true>,
    GLFragFF<0, ZFUNC_NEVER, false>,
    GLFragFF<0, ZFUNC_NEVER, true>,
    GLFragFF<0, ZFUNC_EQ, false>,
    GLFragFF<0, ZFUNC_EQ, true>,
    GLFragFF<1, ZFUNC_ALWAYS, false>,
    GLFragFF<1, ZFUNC_ALWAYS, true>,
    GLFragFF<1, ZFUNC_LE, false>,
    GLFragFF<1, ZFUNC_LE, true>,
    GLFragFF<1, ZFUNC_LT, false>,
    GLFragFF<1, ZFUNC_LT, true>,
    GLFragFF<1, ZFUNC_GT, false>,
    GLFragFF<1, ZFUNC_GT, true>,
    GLFragFF<1, ZFUNC_GE, false>,
    GLFragFF<1, ZFUNC_GE, true>,
    GLFragFF<1, ZFUNC_NEVER, false>,
    GLFragFF<1, ZFUNC_NEVER, true>,
    GLFragFF<1, ZFUNC_EQ, false>,
    GLFragFF<1, ZFUNC_EQ, true>,
    GLFragFF<2, ZFUNC_ALWAYS, false>,
    GLFragFF<2, ZFUNC_ALWAYS, true>,
    GLFragFF<2, ZFUNC_LE, false>,
    GLFragFF<2, ZFUNC_LE, true>,
    GLFragFF<2, ZFUNC_LT, false>,
    GLFragFF<2, ZFUNC_LT, true>,
    GLFragFF<2, ZFUNC_GT, false>,
    GLFragFF<2, ZFUNC_GT, true>,
    GLFragFF<2, ZFUNC_GE, false>,
    GLFragFF<2, ZFUNC_GE, true>,
    GLFragFF<2, ZFUNC_NEVER, false>,
    GLFragFF<2, ZFUNC_NEVER, true>,
    GLFragFF<2, ZFUNC_EQ, false>,
    GLFragFF<2, ZFUNC_EQ, true>,
};

struct SplatFF
{
    enum
    {
        NUM_INTERPOLANTS = 2,
        NUM_ATTRIBUTES = 6,
        DO_PERSPECTIVE = 1,
    };

    static const UINT SIGNATURE[2];

    SplatFF()
    {
    }
};

const UINT SplatFF::SIGNATURE[2] = { 4, 2 };

INLINE simdscalar shade(SplatFF const &splatFF, const SWR_TRIANGLE_DESC &work, WideVector<SplatFF::NUM_ATTRIBUTES, simdscalar> const &pAttrs, BYTE *pBuffer, BYTE *, UINT *outMask)
{
#if KNOB_VS_SIMD_WIDTH == 4
    const __m128i SHUF_ALPHA = _mm_set_epi32(0x8080800f, 0x8080800b, 0x80808007, 0x80808003);
    const __m128i SHUF_RED = _mm_set_epi32(0x8080800e, 0x8080800a, 0x80808006, 0x80808002);
    const __m128i SHUF_GREEN = _mm_set_epi32(0x8080800d, 0x80808009, 0x80808005, 0x80808001);
    const __m128i SHUF_BLUE = _mm_set_epi32(0x8080800c, 0x80808008, 0x80808004, 0x80808000);
#elif KNOB_VS_SIMD_WIDTH == 8
    const __m256i SHUF_ALPHA = _mm256_set_epi32(0x8080800f, 0x8080800b, 0x80808007, 0x80808003, 0x8080800f, 0x8080800b, 0x80808007, 0x80808003);
    const __m256i SHUF_RED = _mm256_set_epi32(0x8080800e, 0x8080800a, 0x80808006, 0x80808002, 0x8080800e, 0x8080800a, 0x80808006, 0x80808002);
    const __m256i SHUF_GREEN = _mm256_set_epi32(0x8080800d, 0x80808009, 0x80808005, 0x80808001, 0x8080800d, 0x80808009, 0x80808005, 0x80808001);
    const __m256i SHUF_BLUE = _mm256_set_epi32(0x8080800c, 0x80808008, 0x80808004, 0x80808000, 0x8080800c, 0x80808008, 0x80808004, 0x80808000);
#endif

    const OGL::SaveableState &state = *(const OGL::SaveableState *)work.pConstants;

    // Interpolated primary fragment color
    WideColor fragColor;
    fragColor.R = get<0>(pAttrs);
    fragColor.G = get<1>(pAttrs);
    fragColor.B = get<2>(pAttrs);
    fragColor.A = get<3>(pAttrs);

    // Sample
    TexCoord tcidx;
    const TextureView &tv = *(const TextureView *)work.pTextureViews[0];
    const Sampler &samp = *(const Sampler *)work.pSamplers[0];
    const OGL::TextureUnit &texUnit = state.mTexUnit[0];
    tcidx.U = get<4>(pAttrs);
    tcidx.V = get<5>(pAttrs);
    UINT mips[] = { 0, 0, 0, 0 };
    WideColor texColor;
    SampleSimplePointRGBAF32(tv, samp, tcidx, mips, texColor);
    fragColor.A = _simd_mul_ps(fragColor.A, texColor.A);

    *outMask = _simd_movemask_ps(_simd_set1_ps(-1.0));

    //// read frag from framebuffer, unpack, and convert to float
    simdscalari vColorBuffer = _simd_load_si((const simdscalari *)pBuffer);

    simdscalari vDstAlpha = _simd_shuffle_epi8(vColorBuffer, SHUF_ALPHA);
    simdscalari vDstRed = _simd_shuffle_epi8(vColorBuffer, SHUF_RED);
    simdscalari vDstGreen = _simd_shuffle_epi8(vColorBuffer, SHUF_GREEN);
    simdscalari vDstBlue = _simd_shuffle_epi8(vColorBuffer, SHUF_BLUE);

    // convert to float
    WideColor dstColor;
    dstColor.R = vUnormToFloat(vDstRed);
    dstColor.G = vUnormToFloat(vDstGreen);
    dstColor.B = vUnormToFloat(vDstBlue);
    dstColor.A = vUnormToFloat(vDstAlpha);

    // s * a + d * (1 - a) = a * (s - d) + d
    fragColor.R = _simd_fmadd_ps(fragColor.A, _simd_sub_ps(fragColor.R, dstColor.R), dstColor.R);
    fragColor.G = _simd_fmadd_ps(fragColor.A, _simd_sub_ps(fragColor.G, dstColor.G), dstColor.G);
    fragColor.B = _simd_fmadd_ps(fragColor.A, _simd_sub_ps(fragColor.B, dstColor.B), dstColor.B);
    fragColor.A = _simd_fmadd_ps(fragColor.A, _simd_sub_ps(fragColor.A, dstColor.A), dstColor.A);

    // Pack fragment
    simdscalari vFragR = vFloatToUnorm(fragColor.R);
    simdscalari vFragG = vFloatToUnorm(fragColor.G);
    simdscalari vFragB = vFloatToUnorm(fragColor.B);
    simdscalari vFragA = vFloatToUnorm(fragColor.A);

    // pack
    simdscalari vDstPixel = vFragB;
    vDstPixel = _simd_or_si(vDstPixel, _simd_slli_epi32(vFragG, 8));
    vDstPixel = _simd_or_si(vDstPixel, _simd_slli_epi32(vFragR, 16));
    vDstPixel = _simd_or_si(vDstPixel, _simd_slli_epi32(vFragA, 24));

    return _simd_castsi_ps(vDstPixel);
}

void visitSplat(const SWR_TRIANGLE_DESC &work, SWR_PIXELOUTPUT &pOut)
{
    SplatFF bsp;

    GenericPixelShader<SplatFF, ZFUNC_LE, false> gps;
    gps.run(work, pOut, bsp);
}
