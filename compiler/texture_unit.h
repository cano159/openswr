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

#ifndef __SWR_TEXTURE_UNIT_H__
#define __SWR_TEXTURE_UNIT_H__

#include <vector>
#include <string.h>

#include "api.h"

struct TextureView
{
    TextureView(SWR_TEXTUREVIEW const &txVw)
    {
        mpTexture = reinterpret_cast<Texture *>(txVw.hTexture);
        mFormat = txVw.format;
        mMipBias = txVw.mipBias;
        mMipMax = txVw.mipMax;
    }

    Texture *mpTexture;
    SWR_FORMAT mFormat;
    UINT mMipBias;
    UINT mMipMax;
};

struct Sampler
{
    Sampler(SWR_CREATESAMPLER const &smp)
    {
        mArrayMode = smp.arrayMode;
        mArraySpec = smp.arraySpec;
        mTilingFormat = smp.tilingFormat;
        memcpy(&mDefaultColor[0], &smp.defaultColor[0], sizeof(float) * 4);
    }

    SWR_ADDRESSING_MODE mArrayMode;
    SWR_ARRAY_SPEC mArraySpec;
    SWR_TILING_FORMAT mTilingFormat;
    float mDefaultColor[4];
};

void SampleDefaultColor(TextureView const &txView, Sampler const &smp, TexCoord const &tc, WideColor &color);
void SampleSimplePointQuadRGBAU8(TextureView const &txView, Sampler const &smp, TexCoord const &tc, WideColor &color);
void SampleSimplePointRGBAU8(TextureView const &txView, Sampler const &smp, TexCoord const &tc, UINT (&mips)[4], WideColor &color);
void SampleSimplePointQuadRGBAF32(TextureView const &txView, Sampler const &smp, TexCoord const &tc, WideColor &color);
void SampleSimplePointRGBAF32(TextureView const &txView, Sampler const &smp, TexCoord const &tc, UINT (&mips)[4], WideColor &color);
void SampleSimpleLinearQuadRGBAF32(TextureView const &txView, Sampler const &smp, TexCoord const &tc, WideColor &color);
void SampleSimpleLinearRGBAF32(TextureView const &txView, Sampler const &smp, TexCoord const &tc, UINT (&mips)[4], WideColor &color);

#endif //__SWR_TEXTURE_UNIT_H__
