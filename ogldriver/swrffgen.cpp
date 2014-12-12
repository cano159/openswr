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
#include "compiler.h"
#include "swrffgen.h"
#undef APIENTRY
#include "gl/gl.h"
#undef APIENTRY
#include "glim.hpp"
#include "oglstate.hpp"
#include "shaders.h"

namespace
{

struct FFGen
{
    enum
    {
        FF_ONE,
        FF_NEGONE,
        FF_ZERO,
        FF_TWO,
        FF_HALF,

        FF_VONE,
        FF_VNEGONE,
        FF_VZERO,
        FF_VTWO,
        FF_VHALF,

        FF_RED,
        FF_GREEN,
        FF_BLUE,
        FF_ALPHA,

        FF_BLACK,
        FF_JASON,

        FF_NUM_IMMEDIATES,
    };

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif

    enum
    {
        // XXX: we have to scale down to sizeof(GLfloat); really, these should
        // be bytes and the compiler *shouldn't* scale.
        // input description
        GLFF_MVP_MATRIX = offsetof(OGL::State, mModelViewProjection),

        GLFF_NORMAL_MATRIX = offsetof(OGL::State, mNormalMatrix),
        GLFF_MV_MATRIX = offsetof(OGL::State, mModelViewMatrix),

        GLFF_LIGHT0_POS = (offsetof(OGL::State, mLightSource) + offsetof(OGL::LightSourceParameters, mPosition)),
        GLFF_LIGHT0_OMPOS = (offsetof(OGL::State, mLightSource) + offsetof(OGL::LightSourceParameters, mOMPosition)),
        GLFF_LIGHT0_DIFFUSE = (offsetof(OGL::State, mLightSource) + offsetof(OGL::LightSourceParameters, mDiffuse)),
        GLFF_LIGHT0_AMBIENT = (offsetof(OGL::State, mLightSource) + offsetof(OGL::LightSourceParameters, mAmbient)),
        GLFF_LIGHT0_SPECULAR = (offsetof(OGL::State, mLightSource) + offsetof(OGL::LightSourceParameters, mSpecular)),

        GLFF_FRONT_MAT_AMBIENT = (offsetof(OGL::State, mFrontMaterial) + offsetof(OGL::MaterialParameters, mAmbient)),
        GLFF_FRONT_MAT_DIFFUSE = (offsetof(OGL::State, mFrontMaterial) + offsetof(OGL::MaterialParameters, mDiffuse)),
        GLFF_FRONT_SPECULAR_COLOR = (offsetof(OGL::State, mFrontMaterial) + offsetof(OGL::MaterialParameters, mSpecular)),

        GLFF_BACK_MAT_AMBIENT = (offsetof(OGL::State, mBackMaterial) + offsetof(OGL::MaterialParameters, mAmbient)),
        GLFF_BACK_MAT_DIFFUSE = (offsetof(OGL::State, mBackMaterial) + offsetof(OGL::MaterialParameters, mDiffuse)),
        GLFF_BACK_SPECULAR_COLOR = (offsetof(OGL::State, mBackMaterial) + offsetof(OGL::MaterialParameters, mSpecular)),

        GLFF_FRONT_SCENE_COLOR = offsetof(OGL::State, mFrontSceneColor),
        GLFF_BACK_SCENE_COLOR = offsetof(OGL::State, mBackSceneColor),

        GLFF_NORMAL_SCALE = offsetof(OGL::State, mNormalScale),

        GLFF_TEXTURE_MATRIX = offsetof(OGL::State, mTexMatrix),

        GLFF_FOG_START = offsetof(OGL::State, mFog) + offsetof(OGL::FogParameters, mStart),
        GLFF_FOG_END = offsetof(OGL::State, mFog) + offsetof(OGL::FogParameters, mEnd),
        GLFF_FOG_COLOR = offsetof(OGL::State, mFog) + offsetof(OGL::FogParameters, mColor)
    };

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

    FFGen(_simd_crcint Name, HANDLE hCompiler, HANDLE hContext, OGL::State &state, SWR_SHADER_TYPE sTy, SWRFF_OPTIONS options)
        : mCrcName(Name), mSTy(sTy), mhCompiler(hCompiler), mhContext(hContext), mState(state), mOptions(options), mFrontLinkageMask(0), mBackLinkageMask(0)
    {
        // XXX: if the user sets 'optLevel == 3', the compiler will break.
        // I would fix it, but I feel it is the user's responsibility to be
        // aware of the actual features vs. the documented features. Or, I'm
        // lazy. Not sure which.
        mLevel = mOptions.optLevel;
    }

    ~FFGen()
    {
    }

    SWRFF_PIPE_RESULT MakeFF()
    {
        mpfnVS = 0;
        mpfnPS = 0;
        MakeVS();
// XXX: turn this on to generate PS.
#if defined(KNOB_SWRC_PS)
        MakePS();
#endif

        SWRFF_PIPE_RESULT result = { mpfnVS, mpfnPS, mFrontLinkageMask, mBackLinkageMask };
        return result;
    }

    void MakePS()
    {
        // Set up the assembling context.
        mpAsm = swrcCreateAssembler(mhCompiler, SHADER_PIXEL);

#if defined(KNOB_SWRC_TRACING)
        FILE *fd = fopen("tracing.txt", "a");
        swrcSetTracing(mpAsm, fd);
#endif

        swrcSetOption(mpAsm, SWRC_BROADCAST_CONSTANTS);
        // This sets the size of the number of microtiles that are rasterized
        // in a single call to the PS. The larger these numbers, the less the
        // overhead for rasterization, but the larger the expected triangle size
        // will need to be.
        swrcSetOption(mpAsm, SWRC_TILE_ITERATIONS, 1, 1);
        swrcSetOption(mpAsm, SWRC_DO_PERSPECTIVE);

        // Determine information about the current GL state.
        SetupGLPSStateInfo();

        // Set up the USER entry point in the assembling.
        auto BB = swrcAddBlock(mpAsm);
        swrcSetEntryBlock(mpAsm, BB);
        swrcSetCurrentBlock(mpAsm, BB);

        swrcAddNote(mpAsm, "Setup Immediates");
        SetupImmediates();

        swrcAddNote(mpAsm, "Setup Immediates");
        SetupImmediates();

        swrcAddNote(mpAsm, "Setup Constants");
        SetupPSConstants();

        swrcAddNote(mpAsm, "Setup Inputs");
        SetupPSInputs();

        swrcAddNote(mpAsm, "Setup Outputs");
        SetupPSOutputs();

        swrcAddInstr(mpAsm, SWRC_MOV, mglColorRT, mImmediates[FF_JASON]);

        // Finish the shader.
        swrcAddInstr(mpAsm, SWRC_RET);

        HANDLE pfn = 0;
        pfn = swrcAssemble(mpAsm);

#if defined(KNOB_SWRC_TRACING)
        fflush(fd);
        fclose(fd);
        fd = 0;
#endif

        swrcDestroyAssembler(mpAsm);

        mpfnPS = (PFN_PIXEL_FUNC)pfn;
    }

    void MakeVS()
    {
        /// !!!
        //     For optimization purposes we do a "poor man's" constants tracking, ie, consider 'alpha fade'.
        //         Method 1:
        //             swrcAddOp(MOV, AF, CONST_1F);
        //         Method 2:
        //             AF = CONST_1F
        //     Later in the code we can check "AF == CONST_1F" to detect constants values, only if we use
        //     Method 2. Method 1 will rely on the JIT's constant prop pass. While our JIT (usually) gets
        //     this right, we can make things cheaper, and guarantee a peace-of-mind, by doing it this way.
        /// !!!

        // Set up the assembling context.
        mpAsm = swrcCreateAssembler(mhCompiler, SHADER_VERTEX);
#if defined(KNOB_SWRC_TRACING)
        FILE *fd = fopen("tracing.txt", "a");
        swrcSetTracing(mpAsm, fd);
#endif
        swrcSetOption(mpAsm, SWRC_BROADCAST_CONSTANTS);

        // Determine information about the current GL state.
        SetupGLVSStateInfo();

        // Set up the USER entry point in the assembling.
        auto BB = swrcAddBlock(mpAsm);
        swrcSetEntryBlock(mpAsm, BB);
        swrcSetCurrentBlock(mpAsm, BB);

        swrcAddNote(mpAsm, "Setup Immediates");
        SetupImmediates();

        swrcAddNote(mpAsm, "Setup Constants");
        SetupVSConstants();

        swrcAddNote(mpAsm, "Setup Inputs");
        SetupVSInputs();

        swrcAddNote(mpAsm, "Setup Outputs");
        SetupVSOutputs();

        // Alpha fade.
        mAlphaFade = mImmediates[FF_VONE];

        // Eye Coordinate Position.
        swrcAddNote(mpAsm, "Eye Coord Position");
        mEyeCoordPosition = swrcAddInstr(mpAsm, SWRC_MKVEC,
                                         swrcAddInstr(mpAsm, SWRC_FDOT4, mglMV[0], mglVertex),
                                         swrcAddInstr(mpAsm, SWRC_FDOT4, mglMV[1], mglVertex),
                                         swrcAddInstr(mpAsm, SWRC_FDOT4, mglMV[2], mglVertex),
                                         swrcAddInstr(mpAsm, SWRC_FDOT4, mglMV[3], mglVertex));

        // Transform input vertex to output position.
        swrcAddNote(mpAsm, "ftransform");
        swrcAddInstr(mpAsm, SWRC_MOV, mglPosition, ftransform());

        if (mUseNormal)
        {
            // Get the transformed normal.
            swrcAddNote(mpAsm, "fnormal");
            mNormal = fnormal();
        }

        // Lights.
        if ((mState.mCaps.lighting & 0x1) != 0)
        {
            swrcAddNote(mpAsm, "lighting");
            flight();
        }
        else
        {
            swrcAddInstr(mpAsm, SWRC_MOV, mglFrontColor, mglInColor);
        }

        if (mState.mCaps.fog)
        {
            GenFog();
        }

        // Textures.
        GenTextures();

        // Debugging modes.
        if (mOptions.colorIsZ)
        {
            auto z = swrcAddInstr(mpAsm, SWRC_GETELT, mglPosition, (SWRC_WORDCODE)2);
            auto w = swrcAddInstr(mpAsm, SWRC_GETELT, mglPosition, (SWRC_WORDCODE)3);
            w = swrcAddInstr(mpAsm, SWRC_MKVEC, w, w, w, w);
            auto zColor = swrcAddInstr(mpAsm, SWRC_MKVEC, z, z, z, z);
            zColor = swrcAddInstr(mpAsm, SWRC_FDIV, zColor, w);
            zColor = swrcAddInstr(mpAsm, SWRC_FMUL, zColor, mImmediates[FF_VHALF]);
            zColor = swrcAddInstr(mpAsm, SWRC_FADD, zColor, mImmediates[FF_VHALF]);
            zColor = swrcAddInstr(mpAsm, SWRC_MOVCMP, (SWRC_WORDCODE)SWRC_CMP_LT, zColor, mImmediates[FF_VONE], zColor, mImmediates[FF_RED]);
            zColor = swrcAddInstr(mpAsm, SWRC_MOVCMP, (SWRC_WORDCODE)SWRC_CMP_LT, mImmediates[FF_VZERO], zColor, zColor, mImmediates[FF_BLUE]);
            swrcAddInstr(mpAsm, SWRC_MOV, mglFrontColor, zColor);
            if (mState.mCaps.lighting && mState.mCaps.twoSided)
            {
                swrcAddInstr(mpAsm, SWRC_MOV, mglBackColor, zColor);
            }
        }

        if (mOptions.colorCodeFace && mState.mCaps.lighting && mState.mCaps.twoSided)
        {
            auto Clr = swrcAddInstr(mpAsm, SWRC_MOV, mglFrontColor);
            Clr = swrcAddInstr(mpAsm, SWRC_SETELT, Clr, (SWRC_WORDCODE)1, mImmediates[FF_ZERO]);
            Clr = swrcAddInstr(mpAsm, SWRC_SETELT, Clr, (SWRC_WORDCODE)2, mImmediates[FF_ZERO]);
            swrcAddInstr(mpAsm, SWRC_MOV, mglFrontColor, Clr);
            if (mState.mCaps.lighting && mState.mCaps.twoSided)
            {
                Clr = swrcAddInstr(mpAsm, SWRC_MOV, mglBackColor);
                Clr = swrcAddInstr(mpAsm, SWRC_SETELT, Clr, (SWRC_WORDCODE)0, mImmediates[FF_ZERO]);
                Clr = swrcAddInstr(mpAsm, SWRC_SETELT, Clr, (SWRC_WORDCODE)2, mImmediates[FF_ZERO]);
                swrcAddInstr(mpAsm, SWRC_MOV, mglBackColor, Clr);
            }
        }

        // Finish the shader.
        swrcAddInstr(mpAsm, SWRC_RET);

        HANDLE pfn = 0;

        if (!mOptions.deferVS)
        {
            pfn = swrcAssemble(mpAsm);
        }
        else
        {
            UINT slot = mOptions.positionOnly ? VS_SLOT_POSITION : VS_SLOT_COLOR0;
            pfn = swrcAssembleWith(mpAsm, 1, &slot);
        }

#if defined(KNOB_SWRC_TRACING)
        fflush(fd);
        fclose(fd);
        fd = 0;
#endif

        swrcDestroyAssembler(mpAsm);

        mpfnVS = (PFN_VERTEX_FUNC)pfn;
    }

    void GenFog()
    {
        // only support linear fog
        if (mState.mFog.mMode != GL_LINEAR)
        {
            assert(0 && "Unimplemented fog mode");
            return;
        }

        swrcAddNote(mpAsm, "Generate Fog");

        // constants
        auto start = swrcAddConstant(mpAsm, GLFF_FOG_START, SWRC_FP32);
        auto end = swrcAddConstant(mpAsm, GLFF_FOG_END, SWRC_FP32);
        auto fogColor = swrcAddConstant(mpAsm, GLFF_FOG_COLOR, SWRC_V4FP32);

        // compute length of position in eye coordinates
        auto x = swrcAddInstr(mpAsm, SWRC_GETELT, mEyeCoordPosition, (SWRC_WORDCODE)0);
        auto y = swrcAddInstr(mpAsm, SWRC_GETELT, mEyeCoordPosition, (SWRC_WORDCODE)1);
        auto z = swrcAddInstr(mpAsm, SWRC_GETELT, mEyeCoordPosition, (SWRC_WORDCODE)2);

        x = swrcAddInstr(mpAsm, SWRC_FMUL, x, x);
        y = swrcAddInstr(mpAsm, SWRC_FMUL, y, y);
        z = swrcAddInstr(mpAsm, SWRC_FMUL, z, z);

        auto sum = swrcAddInstr(mpAsm, SWRC_FADD, x, y);
        sum = swrcAddInstr(mpAsm, SWRC_FADD, sum, z);
        sum = swrcAddInstr(mpAsm, SWRC_FSQRT, sum);

        // linear fog factor = (end - length)/(end - start)
        auto numerator = swrcAddInstr(mpAsm, SWRC_FSUB, end, sum);
        auto denom = swrcAddInstr(mpAsm, SWRC_FSUB, end, start);
        auto factor = swrcAddInstr(mpAsm, SWRC_FDIV, numerator, denom);

        // clamp factor [0,1]
        factor = swrcAddInstr(mpAsm, SWRC_FMIN, factor, mImmediates[FF_ONE]);
        factor = swrcAddInstr(mpAsm, SWRC_FMAX, factor, mImmediates[FF_ZERO]);

        auto vecfactor = swrcAddInstr(mpAsm, SWRC_MKVEC, factor, factor, factor, mImmediates[FF_ONE]);

        // apply factor to color
        // color = factor * color + (1 - factor) * fogColor
        auto prod1 = swrcAddInstr(mpAsm, SWRC_FMUL, vecfactor, mglFrontColor);
        auto oneMinusFog = swrcAddInstr(mpAsm, SWRC_FSUB, mImmediates[FF_VONE], vecfactor);
        auto prod2 = swrcAddInstr(mpAsm, SWRC_FMUL, oneMinusFog, fogColor);
        prod1 = swrcAddInstr(mpAsm, SWRC_FADD, prod1, prod2);
        swrcAddInstr(mpAsm, SWRC_MOV, mglFrontColor, prod1);
    }

    void GenTextures()
    {
        swrcAddNote(mpAsm, "Generate Textures");
        for (UINT i = 0, N = OGL::NUM_TEXTURES; i < N; ++i)
        {
            if (mState.mCaps.textures & (0x1 << i))
            {
                auto tex = mglInTexCoord[i];
                if (!mState.mMatrixInfo[OGL::TEXTURE_BASE + i].mIdentity)
                {
                    auto glTexM0 = swrcAddConstant(mpAsm, GLFF_TEXTURE_MATRIX + 64 * i + 0 * 4 * 4, SWRC_V4FP32);
                    auto glTexM1 = swrcAddConstant(mpAsm, GLFF_TEXTURE_MATRIX + 64 * i + 1 * 4 * 4, SWRC_V4FP32);
                    auto glTexM2 = swrcAddConstant(mpAsm, GLFF_TEXTURE_MATRIX + 64 * i + 2 * 4 * 4, SWRC_V4FP32);
                    auto glTexM3 = swrcAddConstant(mpAsm, GLFF_TEXTURE_MATRIX + 64 * i + 3 * 4 * 4, SWRC_V4FP32);
                    auto tex0 = swrcAddInstr(mpAsm, SWRC_FDOT4, glTexM0, mglInTexCoord[i]);
                    auto tex1 = swrcAddInstr(mpAsm, SWRC_FDOT4, glTexM1, mglInTexCoord[i]);
                    auto tex2 = swrcAddInstr(mpAsm, SWRC_FDOT4, glTexM2, mglInTexCoord[i]);
                    auto tex3 = swrcAddInstr(mpAsm, SWRC_FDOT4, glTexM3, mglInTexCoord[i]);
                    tex = swrcAddInstr(mpAsm, SWRC_MKVEC, tex0, tex1, tex2, tex3);
                }
                swrcAddInstr(mpAsm, SWRC_MOV, mglOutTexCoord[i], tex);
            }
        }
    }

    void flight()
    {
        if (!mOptions.deferVS)
        {
            mpMaterial = &mFrontMaterial;

            // Front facing.
            mAmbient = mImmediates[FF_VZERO];
            mDiffuse = mImmediates[FF_VZERO];
            mSpecular = mImmediates[FF_VZERO];
            mpColorOut = &mglFrontColor;
            swrcAddNote(mpAsm, "AccumulateLights");
            AccumulateLights();

            if (mState.mCaps.twoSided)
            {
                mpMaterial = &mBackMaterial;

                swrcAddNote(mpAsm, "Two-Sided Lighting");
                // Rear facing.
                mAmbient = mImmediates[FF_VZERO];
                mDiffuse = mImmediates[FF_VZERO];
                mSpecular = mImmediates[FF_VZERO];
                mpColorOut = &mglBackColor;

                // Invert the normal.
                mNormal = swrcAddInstr(mpAsm, SWRC_FSUB, mImmediates[FF_VZERO], mNormal);

                swrcAddNote(mpAsm, "AccumulateLights (back side)");
                AccumulateLights();
            }
        }
        else
        {
            mDeferredMaterial.ambient = swrcAddInstr(mpAsm, SWRC_MOVC, mglFaceMask, mFrontMaterial.ambient, mBackMaterial.ambient);
            mDeferredMaterial.diffuse = swrcAddInstr(mpAsm, SWRC_MOVC, mglFaceMask, mFrontMaterial.diffuse, mBackMaterial.diffuse);
            mDeferredMaterial.specular = swrcAddInstr(mpAsm, SWRC_MOVC, mglFaceMask, mFrontMaterial.specular, mBackMaterial.specular);
            mDeferredMaterial.sceneColor = swrcAddInstr(mpAsm, SWRC_MOVC, mglFaceMask, mFrontMaterial.sceneColor, mBackMaterial.sceneColor);
            mpMaterial = &mDeferredMaterial;

            mAmbient = mImmediates[FF_VZERO];
            mDiffuse = mImmediates[FF_VZERO];
            mSpecular = mImmediates[FF_VZERO];
            mpColorOut = &mglFrontColor;

            // Inverse normal.
            auto iNormal = swrcAddInstr(mpAsm, SWRC_FSUB, mImmediates[FF_VZERO], mNormal);
            mNormal = swrcAddInstr(mpAsm, SWRC_MOVC, mglFaceMask, mNormal, iNormal);

            swrcAddNote(mpAsm, "AccumulateLights Deferred.");
            AccumulateLights();
        }
    }

    void AccumulateLights()
    {
        GLfloat *v = 0;
        GLfloat spotCut;
        for (UINT i = 0; i < OGL::NUM_LIGHTS; ++i)
        {
            if ((mState.mCaps.light & (0x1 << (i - GL_LIGHT0))) != 0)
            {
                v = &mState.mLightSource[i].mPosition[0];
                spotCut = mState.mLightSource[i].mSpotCutoff;

                if (v[3] == 0.0f)
                {
                    // Directional Lights
                    if (spotCut == OGL::DEFAULT_SPOT_CUT)
                    {
                        // Spotlight at infinity, attenuation applies
                        swrcAddNote(mpAsm, "infinite spot light");
                        infiniteSpotLight(i);
                    }
                    else
                    {
                        // Pure directional, no attenuation.
                        // XXX: directionalLight
                        assert(false && "directionalLight");
                    }
                }
                else
                {
                    // Positional Lights: Point or Spot, attenuation applies
                    // XXX: pointLight; spotLight
                    //assert(false && "pointLight or spotLight");
                }
            }
        }

        swrcAddNote(mpAsm, "set output color");
        // color = lightDiffuse * material_diffuse + SceneColor
        //    SceneColor = MaterialEmissive + lightAmbient * MaterialAmbient (done in SetupVSConstants)
        //    mAmbient, mDiffuse are the accumulated results over all active lights.
        // BMCXXX: (ambient and diffuse should be attenuated by light distance)
        auto color = swrcAddInstr(mpAsm, SWRC_FMULADD, mAmbient, mpMaterial->ambient, mpMaterial->sceneColor);
        color = swrcAddInstr(mpAsm, SWRC_FMULADD, mDiffuse, mpMaterial->diffuse, color);

        if (mSeparateSpec == GL_SEPARATE_SPECULAR_COLOR)
        {
            // XXX: implement.
            assert(false && "separate specular color");
        }
        else
        {
            if (mpMaterial->specular != mImmediates[FF_VZERO])
            {
                color = swrcAddInstr(mpAsm, SWRC_FMULADD, mSpecular, mpMaterial->specular, color);
            }
        }

        swrcAddNote(mpAsm, "clamp color");
        // Clamp.
        color = swrcAddInstr(mpAsm, SWRC_FMIN, color, mImmediates[FF_VONE]);
        color = swrcAddInstr(mpAsm, SWRC_FMAX, color, mImmediates[FF_VZERO]);

        if (mAlphaFade != mImmediates[FF_VONE])
        {
            // XXX: extract the alpha value and scale by the alpha fade.
            // BMCDEBUG: Could this be code for point parameters?  If so, where to apply?  I think it applies regardless of lighting.
            // a := GETELT color, 3
            // a *= AF
            // SETELT color, 3, a
            assert(false && "alpha fade");
        }

        // Set color.alpha = diffuse_material.alpha
        // BMCXXX: Optimization, only add instruction if diffuse material alpha < 1.0.  How to get correct state?
        auto alpha = swrcAddInstr(mpAsm, SWRC_GETELT, mpMaterial->diffuse, (SWRC_WORDCODE)3);
        color = swrcAddInstr(mpAsm, SWRC_SETELT, color, (SWRC_WORDCODE)3, alpha);

        // Move to output.
        swrcAddInstr(mpAsm, SWRC_MOV, *mpColorOut, color);
    }

    void infiniteSpotLight(UINT light)
    {
        // Invariant: mglLightSource[light].position *must* be normalized.
        auto lightDir = mglLightSource[light].omPosition;

        // nDotL
        auto nDotL = swrcAddInstr(mpAsm, SWRC_FDOT3, mNormal, lightDir);
        nDotL = swrcAddInstr(mpAsm, SWRC_FMAX, mImmediates[FF_ZERO], nDotL);

        auto spotAttenuation = mImmediates[FF_VONE];
        auto pf = mImmediates[FF_VZERO];

        // XXX: properly calculate spotAttenuation.
        // XXX: properly calculate specular power fade.

        if (spotAttenuation == mImmediates[FF_VONE])
        {
            nDotL = swrcAddInstr(mpAsm, SWRC_MKVEC, nDotL, nDotL, nDotL, nDotL);
            mAmbient = swrcAddInstr(mpAsm, SWRC_FADD, mglLightSource[light].ambient, mAmbient);
            mDiffuse = swrcAddInstr(mpAsm, SWRC_FMULADD, mglLightSource[light].diffuse, nDotL, mDiffuse);
            if ((mpMaterial->specular != mImmediates[FF_VZERO]) && (pf != mImmediates[FF_VZERO]))
            {
                mSpecular = swrcAddInstr(mpAsm, SWRC_FMULADD, mglLightSource[light].specular, pf, mSpecular);
            }
        }
        else
        {
            // XXX: implement.
        }
    }

    SWRC_WORDCODE ftransform()
    {
        if (mOptions.deferVS && !mOptions.positionOnly)
        {
            return mglVertex;
        }

        if (mState.mMatrixInfoMVP.mIdentity)
        {
            return mglVertex;
        }

        if (mState.mMatrixInfoMVP.mAffine)
        {
            if (mState.mMatrixInfoMVP.mScale && !mState.mMatrixInfoMVP.mNoTranslation)
            {
                auto glPos0 = swrcAddInstr(mpAsm, SWRC_FDOT4, mglMVP[0], mglVertex);
                auto glPos1 = swrcAddInstr(mpAsm, SWRC_FDOT4, mglMVP[1], mglVertex);
                auto glPos2 = swrcAddInstr(mpAsm, SWRC_FDOT4, mglMVP[2], mglVertex);
                auto glPos3 = swrcAddInstr(mpAsm, SWRC_GETELT, mglVertex, (SWRC_WORDCODE)3);
                return swrcAddInstr(mpAsm, SWRC_MKVEC, glPos0, glPos1, glPos2, glPos3);
            }
            else
            {
                auto mvpT0 = swrcAddInstr(mpAsm, SWRC_GETELT, mglMVP[0], (SWRC_WORDCODE)3);
                auto mvpT1 = swrcAddInstr(mpAsm, SWRC_GETELT, mglMVP[1], (SWRC_WORDCODE)3);
                auto mvpT2 = swrcAddInstr(mpAsm, SWRC_GETELT, mglMVP[2], (SWRC_WORDCODE)3);
                auto glPos0 = swrcAddInstr(mpAsm, SWRC_FDOT3, mglMVP[0], mglVertex);
                auto glPos1 = swrcAddInstr(mpAsm, SWRC_FDOT3, mglMVP[1], mglVertex);
                auto glPos2 = swrcAddInstr(mpAsm, SWRC_FDOT3, mglMVP[2], mglVertex);
                swrcAddInstr(mpAsm, SWRC_FADD, glPos0, glPos0, mvpT0);
                swrcAddInstr(mpAsm, SWRC_FADD, glPos1, glPos1, mvpT1);
                swrcAddInstr(mpAsm, SWRC_FADD, glPos2, glPos2, mvpT2);
                return swrcAddInstr(mpAsm, SWRC_MKVEC, glPos0, glPos1, glPos2, mImmediates[FF_ONE]);
            }
        }
        else
        {
            auto glPos0 = swrcAddInstr(mpAsm, SWRC_FDOT4, mglMVP[0], mglVertex);
            auto glPos1 = swrcAddInstr(mpAsm, SWRC_FDOT4, mglMVP[1], mglVertex);
            auto glPos2 = swrcAddInstr(mpAsm, SWRC_FDOT4, mglMVP[2], mglVertex);
            auto glPos3 = swrcAddInstr(mpAsm, SWRC_FDOT4, mglMVP[3], mglVertex);
            return swrcAddInstr(mpAsm, SWRC_MKVEC, glPos0, glPos1, glPos2, glPos3);
        }
    }

    SWRC_WORDCODE fnormal()
    {
        auto normal = mglNormal;

        if (mState.mCaps.normalize)
        {
            normal = normalize(normal, 3);
        }

        if (mState.mCaps.rescaleNormal)
        {
            normal = swrcAddInstr(mpAsm, SWRC_FMUL, normal, mglNormalScale);
        }

        return normal;
    }

    SWRC_WORDCODE normalize(SWRC_WORDCODE vec, UINT width)
    {
        SWRC_WORDCODE FDOT;
        switch (width)
        {
        case 4:
            FDOT = SWRC_FDOT4;
            break;
        case 3:
            FDOT = SWRC_FDOT3;
            break;
        }
        auto length = swrcAddInstr(mpAsm, FDOT, vec, vec);
        swrcAddInstr(mpAsm, SWRC_FRSQRT, length, length);
        length = swrcAddInstr(mpAsm, SWRC_MKVEC, length, length, length, length);
        return swrcAddInstr(mpAsm, SWRC_FMUL, vec, length);
    }

    void SetupImmediates()
    {
        mImmediates[FF_VZERO] = swrcAddImm(mpAsm, 0.0f, 0.0f, 0.0f, 0.0f);
        mImmediates[FF_VONE] = swrcAddImm(mpAsm, 1.0f, 1.0f, 1.0f, 1.0f);
        mImmediates[FF_VNEGONE] = swrcAddImm(mpAsm, -1.0f, -1.0f, -1.0f, -1.0f);
        mImmediates[FF_VTWO] = swrcAddImm(mpAsm, 2.0f, 2.0f, 2.0f, 2.0f);
        mImmediates[FF_VHALF] = swrcAddImm(mpAsm, 0.5f, 0.5f, 0.5f, 0.5f);
        mImmediates[FF_BLACK] = swrcAddImm(mpAsm, 0.0f, 0.0f, 0.0f, 1.0f);
        mImmediates[FF_JASON] = swrcAddImm(mpAsm, 1.0f, 0.0f, 1.0f, 1.0f);

        mImmediates[FF_ZERO] = swrcAddImm(mpAsm, 0.0f);
        mImmediates[FF_ONE] = swrcAddImm(mpAsm, 1.0f);
        mImmediates[FF_NEGONE] = swrcAddImm(mpAsm, -1.0f);
        mImmediates[FF_TWO] = swrcAddImm(mpAsm, 2.0f);
        mImmediates[FF_HALF] = swrcAddImm(mpAsm, 0.5f);

        mImmediates[FF_RED] = swrcAddImm(mpAsm, 1.0f, 0.0, 0.0, 0.0);
        mImmediates[FF_GREEN] = swrcAddImm(mpAsm, 0.0f, 1.0, 0.0, 0.0);
        mImmediates[FF_BLUE] = swrcAddImm(mpAsm, 0.0f, 0.0, 1.0, 0.0);
        mImmediates[FF_ALPHA] = swrcAddImm(mpAsm, 0.0f, 0.0, 0.0, 1.0);
    }

    void SetupGLVSStateInfo()
    {
        // XXX: should be on if texturing is on, as well
        mUseNormal = mState.mCaps.lighting != 0;
        ;

        mSeparateSpec = (GLint)(mState.mCaps.specularColor ? GL_SEPARATE_SPECULAR_COLOR : GL_SINGLE_COLOR);
        //mSeparateSpec			= mState.mCaps.twoSided;        //BMCDEBUG: ??? Why mSeparateSpec = twoSided, makes no sense!
        mLocalView = mState.mCaps.localViewer;
    }

    void SetupGLPSStateInfo()
    {
    }

    void SetupVSConstants()
    {
        swrcAddNote(mpAsm, "    Constant - Model View Projection Matrix");
        mglMVP[0] = swrcAddConstant(mpAsm, GLFF_MVP_MATRIX + 0 * 4 * 4, SWRC_V4FP32);
        mglMVP[1] = swrcAddConstant(mpAsm, GLFF_MVP_MATRIX + 1 * 4 * 4, SWRC_V4FP32);
        mglMVP[2] = swrcAddConstant(mpAsm, GLFF_MVP_MATRIX + 2 * 4 * 4, SWRC_V4FP32);
        mglMVP[3] = swrcAddConstant(mpAsm, GLFF_MVP_MATRIX + 3 * 4 * 4, SWRC_V4FP32);

        swrcAddNote(mpAsm, "    Constant - Model View Matrix");
        mglMV[0] = swrcAddConstant(mpAsm, GLFF_MV_MATRIX + 0 * 4 * 4, SWRC_V4FP32);
        mglMV[1] = swrcAddConstant(mpAsm, GLFF_MV_MATRIX + 1 * 4 * 4, SWRC_V4FP32);
        mglMV[2] = swrcAddConstant(mpAsm, GLFF_MV_MATRIX + 2 * 4 * 4, SWRC_V4FP32);
        mglMV[3] = swrcAddConstant(mpAsm, GLFF_MV_MATRIX + 3 * 4 * 4, SWRC_V4FP32);

        mglNM[0] = swrcAddConstant(mpAsm, GLFF_NORMAL_MATRIX + 0 * 4 * 4, SWRC_V4FP32);
        mglNM[1] = swrcAddConstant(mpAsm, GLFF_NORMAL_MATRIX + 1 * 4 * 4, SWRC_V4FP32);
        mglNM[2] = swrcAddConstant(mpAsm, GLFF_NORMAL_MATRIX + 2 * 4 * 4, SWRC_V4FP32);

        if (mState.mCaps.rescaleNormal)
        {
            auto normalScale = swrcAddConstant(mpAsm, GLFF_NORMAL_SCALE, SWRC_FP32);
            mglNormalScale = swrcAddInstr(mpAsm, SWRC_MKVEC, normalScale, normalScale, normalScale, normalScale);
        }

        if (mState.mCaps.lighting)
        {
            for (UINT i = 0; i < OGL::NUM_LIGHTS; ++i)
            {
                if ((mState.mCaps.light & (0x1 << i)) != 0)
                {
                    switch (mLevel)
                    {
                    case 0:
                    case 1:
                        mglLightSource[i].omPosition = swrcAddConstant(mpAsm, GLFF_LIGHT0_OMPOS + i * sizeof(OGL::LightSourceParameters), SWRC_V4FP32);
                        mglLightSource[i].position = swrcAddConstant(mpAsm, GLFF_LIGHT0_POS + i * sizeof(OGL::LightSourceParameters), SWRC_V4FP32);
                        mglLightSource[i].spotDirection = mImmediates[FF_VZERO]; //XXX: swrcAddConstant(mpAsm, GLFF_LIGHT0_SPOT_DIR + i, SWRC_V4FP32);
                        mglLightSource[i].spotExponent = mImmediates[FF_ZERO];   //XXX: swrcAddConstant(mpAsm, GLFF_LIGHT0_SPOT_EXP + i, SWRC_V4FP32);
                        mglLightSource[i].ambient = swrcAddConstant(mpAsm, GLFF_LIGHT0_AMBIENT + i * sizeof(OGL::LightSourceParameters), SWRC_V4FP32);
                        mglLightSource[i].diffuse = swrcAddConstant(mpAsm, GLFF_LIGHT0_DIFFUSE + i * sizeof(OGL::LightSourceParameters), SWRC_V4FP32);
                        mglLightSource[i].specular = swrcAddConstant(mpAsm, GLFF_LIGHT0_SPECULAR + i * sizeof(OGL::LightSourceParameters), SWRC_V4FP32);
                        break;
                    case 2:
                        mglLightSource[i].omPosition = swrcAddImm(mpAsm, mState.mLightSource[i].mOMPosition[0], mState.mLightSource[i].mOMPosition[1], mState.mLightSource[i].mOMPosition[2], mState.mLightSource[i].mOMPosition[3]);
                        mglLightSource[i].position = swrcAddImm(mpAsm, mState.mLightSource[i].mPosition[0], mState.mLightSource[i].mPosition[1], mState.mLightSource[i].mPosition[2], mState.mLightSource[i].mPosition[3]);
                        mglLightSource[i].spotDirection = mImmediates[FF_VZERO]; //XXX: swrcAddConstant(mpAsm, GLFF_LIGHT0_SPOT_DIR + i, SWRC_V4FP32);
                        mglLightSource[i].spotExponent = mImmediates[FF_ZERO];   //XXX: swrcAddConstant(mpAsm, GLFF_LIGHT0_SPOT_EXP + i, SWRC_V4FP32);
                        mglLightSource[i].ambient = swrcAddImm(mpAsm, mState.mLightSource[i].mAmbient[0], mState.mLightSource[i].mAmbient[1], mState.mLightSource[i].mAmbient[2], mState.mLightSource[i].mAmbient[3]);
                        mglLightSource[i].diffuse = swrcAddImm(mpAsm, mState.mLightSource[i].mDiffuse[0], mState.mLightSource[i].mDiffuse[1], mState.mLightSource[i].mDiffuse[2], mState.mLightSource[i].mDiffuse[3]);
                        mglLightSource[i].specular = swrcAddImm(mpAsm, mState.mLightSource[i].mSpecular[0], mState.mLightSource[i].mSpecular[1], mState.mLightSource[i].mSpecular[2], mState.mLightSource[i].mSpecular[3]);
                        ;
                        break;
                    }
                }
            }
        }

        GLfloat *materialAmbient = 0;
        GLfloat *materialEmission = 0;
        GLfloat *materialDiffuse = 0;
        GLfloat *materialSpecular = 0;
        GLfloat *modelAmbient = 0;
        OSALIGNLINE(GLfloat) sceneColor[4] = { 0 };

        switch (mLevel)
        {
        case 0:
            mFrontMaterial.sceneColor = swrcAddConstant(mpAsm, GLFF_FRONT_SCENE_COLOR, SWRC_V4FP32);
            mFrontMaterial.ambient = swrcAddConstant(mpAsm, GLFF_FRONT_MAT_AMBIENT, SWRC_V4FP32);
            mFrontMaterial.diffuse = swrcAddConstant(mpAsm, GLFF_FRONT_MAT_DIFFUSE, SWRC_V4FP32);
            mFrontMaterial.specular = swrcAddConstant(mpAsm, GLFF_FRONT_SPECULAR_COLOR, SWRC_V4FP32);

            mBackMaterial.sceneColor = swrcAddConstant(mpAsm, GLFF_BACK_SCENE_COLOR, SWRC_V4FP32);
            mBackMaterial.ambient = swrcAddConstant(mpAsm, GLFF_BACK_MAT_AMBIENT, SWRC_V4FP32);
            mBackMaterial.diffuse = swrcAddConstant(mpAsm, GLFF_BACK_MAT_DIFFUSE, SWRC_V4FP32);
            mBackMaterial.specular = swrcAddConstant(mpAsm, GLFF_BACK_SPECULAR_COLOR, SWRC_V4FP32);
            break;
        case 1:
        case 2:
            materialAmbient = &mState.mFrontMaterial.mAmbient[0];
            materialEmission = &mState.mFrontMaterial.mEmission[0];
            materialDiffuse = &mState.mFrontMaterial.mDiffuse[0];
            materialSpecular = &mState.mFrontMaterial.mSpecular[0];
            modelAmbient = &mState.mLightModel.mAmbient[0];

            _mm_store_ps(&sceneColor[0], _mm_add_ps(_mm_loadu_ps(&materialEmission[0]),
                                                    _mm_mul_ps(_mm_loadu_ps(&materialAmbient[0]),
                                                               _mm_loadu_ps(&modelAmbient[0]))));

            mFrontMaterial.sceneColor = swrcAddImm(mpAsm, sceneColor[0], sceneColor[1], sceneColor[2], sceneColor[3]);
            mFrontMaterial.ambient = swrcAddImm(mpAsm, materialAmbient[0], materialAmbient[1], materialAmbient[2], materialAmbient[3]);
            mFrontMaterial.diffuse = swrcAddImm(mpAsm, materialDiffuse[0], materialDiffuse[1], materialDiffuse[2], materialDiffuse[3]);
            mFrontMaterial.specular = swrcAddImm(mpAsm, materialSpecular[0], materialSpecular[1], materialSpecular[2], materialSpecular[3]);

            materialAmbient = &mState.mBackMaterial.mAmbient[0];
            materialEmission = &mState.mBackMaterial.mEmission[0];
            materialDiffuse = &mState.mBackMaterial.mDiffuse[0];
            materialSpecular = &mState.mBackMaterial.mSpecular[0];

            _mm_store_ps(&sceneColor[0], _mm_add_ps(_mm_loadu_ps(&materialEmission[0]),
                                                    _mm_mul_ps(_mm_loadu_ps(&materialAmbient[0]),
                                                               _mm_loadu_ps(&modelAmbient[0]))));

            mBackMaterial.sceneColor = swrcAddImm(mpAsm, sceneColor[0], sceneColor[1], sceneColor[2], sceneColor[3]);
            mBackMaterial.ambient = swrcAddImm(mpAsm, materialAmbient[0], materialAmbient[1], materialAmbient[2], materialAmbient[3]);
            mBackMaterial.diffuse = swrcAddImm(mpAsm, materialDiffuse[0], materialDiffuse[1], materialDiffuse[2], materialDiffuse[3]);
            mBackMaterial.specular = swrcAddImm(mpAsm, materialSpecular[0], materialSpecular[1], materialSpecular[2], materialSpecular[3]);
            break;
        default:
            break;
        }
    }

    void SetupPSConstants()
    {
    }

    void SetupVSInputs()
    {
        mglVertex = swrcAddDecl(mpAsm, VS_SLOT_POSITION, SWRC_IN);

        if (mUseNormal)
        {
            mglNormal = swrcAddDecl(mpAsm, VS_SLOT_NORMAL, SWRC_IN);
            // XXX: transform the normal.
        }

        for (UINT i = 0, N = OGL::NUM_TEXTURES; i < N; ++i)
        {
            if (mState.mCaps.textures & (0x1 << i))
            {
                mglInTexCoord[i] = swrcAddDecl(mpAsm, VS_SLOT_TEXCOORD0 + i, SWRC_IN);
            }
        }

        if (!mState.mCaps.lighting)
        {
            mglInColor = swrcAddDecl(mpAsm, VS_SLOT_COLOR0, SWRC_IN);
        }

        if (mOptions.deferVS)
        {
            mglFaceMask = swrcAddImm(mpAsm, 0.0f);
        }
    }

    void SetupVSOutputs()
    {
        mglPosition = swrcAddDecl(mpAsm, VS_SLOT_POSITION, SWRC_OUT);
        // XXX: nice. Don't setup position in the linkage masks.
        //mFrontLinkageMask		|= VS_ATTR_MASK(VS_SLOT_POSITION);
        //mBackLinkageMask		|= VS_ATTR_MASK(VS_SLOT_POSITION);

        for (UINT i = 0, N = OGL::NUM_TEXTURES; i < N; ++i)
        {
            if (mState.mCaps.textures & (0x1 << i))
            {
                mglOutTexCoord[i] = swrcAddDecl(mpAsm, VS_SLOT_TEXCOORD0 + i, SWRC_OUT);
                mFrontLinkageMask |= VS_ATTR_MASK(VS_SLOT_TEXCOORD0 + i);
                mBackLinkageMask |= VS_ATTR_MASK(VS_SLOT_TEXCOORD0 + i);
            }
        }

        if (mState.mCaps.lighting)
        {
            mglFrontColor = swrcAddDecl(mpAsm, VS_SLOT_COLOR0, SWRC_OUT);
            mFrontLinkageMask |= VS_ATTR_MASK(VS_SLOT_COLOR0);
            if (mState.mCaps.twoSided)
            {
                mglBackColor = swrcAddDecl(mpAsm, VS_SLOT_COLOR1, SWRC_OUT);
                mBackLinkageMask |= VS_ATTR_MASK(VS_SLOT_COLOR1);
            }
            else
            {
                mBackLinkageMask |= VS_ATTR_MASK(VS_SLOT_COLOR0);
            }
        }
        else
        {
            // Front color is an alias for 'gl_Color'.
            mglFrontColor = swrcAddDecl(mpAsm, VS_SLOT_COLOR0, SWRC_OUT);
            mFrontLinkageMask |= VS_ATTR_MASK(VS_SLOT_COLOR0);
            mBackLinkageMask |= VS_ATTR_MASK(VS_SLOT_COLOR0);
        }
    }

    void SetupPSInputs()
    {
        mglFragColor = swrcAddDecl(mpAsm, 0, SWRC_IN, 4);
    }

    void SetupPSOutputs()
    {
        mglColorRT = swrcAddDecl(mpAsm, 0, SWRC_OUT, SWRC_V4UN8);
        mglZRT = swrcAddDecl(mpAsm, 1, SWRC_OUT, SWRC_FP32);
    }

    _simd_crcint mCrcName;
    SWR_SHADER_TYPE mSTy;
    HANDLE mhCompiler;
    HANDLE mhContext;
    UINT mLevel;
    OGL::State &mState;
    SWRC_ASM *mpAsm;
    SWRFF_OPTIONS mOptions;
    PFN_VERTEX_FUNC mpfnVS;
    PFN_PIXEL_FUNC mpfnPS;
    UINT mFrontLinkageMask;
    UINT mBackLinkageMask;

    // Computed GL state info.
    GLboolean mUseNormal;
    GLint mSeparateSpec;
    GLint mLocalView;

    // Immediates.
    SWRC_WORDCODE mImmediates[FF_NUM_IMMEDIATES];

    // Intermediate light accumulators.
    SWRC_WORDCODE mAmbient;
    SWRC_WORDCODE mDiffuse;
    SWRC_WORDCODE mSpecular;

    // Light materials.
    struct Material
    {
        SWRC_WORDCODE sceneColor;
        SWRC_WORDCODE ambient;
        SWRC_WORDCODE diffuse;
        SWRC_WORDCODE specular;
    };
    Material mFrontMaterial;
    Material mBackMaterial;
    Material mDeferredMaterial;
    Material *mpMaterial;
    SWRC_WORDCODE *mpColorOut;

    // Inputs.
    SWRC_WORDCODE mglVertex;
    SWRC_WORDCODE mglNormal;
    SWRC_WORDCODE mglFaceMask;
    SWRC_WORDCODE mglInColor;
    SWRC_WORDCODE mglInTexCoord[OGL::NUM_TEXTURES];
    SWRC_WORDCODE mglFragColor;

    // Outputs.
    SWRC_WORDCODE mglPosition;
    SWRC_WORDCODE mglFrontColor;
    SWRC_WORDCODE mglBackColor;
    SWRC_WORDCODE mglOutTexCoord[OGL::NUM_TEXTURES];

    // Constants.
    SWRC_WORDCODE mglMVP[4];
    SWRC_WORDCODE mglMV[4];
    SWRC_WORDCODE mglNM[3];
    SWRC_WORDCODE mglNormalScale;
    struct LightSource
    {
        SWRC_WORDCODE position;
        SWRC_WORDCODE omPosition;
        SWRC_WORDCODE spotDirection;
        SWRC_WORDCODE spotExponent;
        SWRC_WORDCODE ambient;
        SWRC_WORDCODE diffuse;
        SWRC_WORDCODE specular;
    };
    LightSource mglLightSource[OGL::NUM_LIGHTS];

    // Intermediate values.
    SWRC_WORDCODE mNormal;
    SWRC_WORDCODE mEyeCoordPosition;
    SWRC_WORDCODE mAlphaFade;

    // Render Targets.
    SWRC_WORDCODE mglColorRT;
    SWRC_WORDCODE mglZRT;
};
}

SWRFF_PIPE_RESULT swrffGen(_simd_crcint Name, HANDLE hCompiler, HANDLE hContext, OGL::State &state, SWRFF_OPTIONS options)
{
    FFGen ffG(Name, hCompiler, hContext, state, SHADER_VERTEX, options);

    return ffG.MakeFF();
}
