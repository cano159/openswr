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

#pragma once

#include "os.h"
#include "simdintrin.h"

enum SWR_TYPE
{
    SWR_TYPE_UNKNOWN,
    SWR_TYPE_UNORM8,
    SWR_TYPE_SNORM8,
    SWR_TYPE_UNORM16,
    SWR_TYPE_SNORM16,
    SWR_TYPE_UINT8,
    SWR_TYPE_SINT8,
    SWR_TYPE_UINT16,
    SWR_TYPE_SINT16,
    SWR_TYPE_UINT32,
    SWR_TYPE_FLOAT,
};

enum SWR_FORMAT
{
    NULL_FORMAT,

    A32_FLOAT,

    R32_FLOAT,
    RG32_FLOAT,
    RGB32_FLOAT,
    RGBA32_FLOAT,

    R8_UNORM,
    RG8_UNORM,
    RGB8_UNORM,
    RGBA8_UNORM,

    BGR8_UNORM,
    BGRA8_UNORM,

    RGB8_SNORM,

    RG16_SINT,

    NUM_SWR_FORMATS
};

struct SWR_FORMAT_INFO
{
    SWR_TYPE type;
    UINT numComps;
    UINT Bpp; // bytes per pixel
    UINT Bpc; // bytes per component

    INT rpos, gpos, bpos, apos;

    const OSALIGN(UINT, 16) unpackToRGBA[4]; // shuffle vector to unpack format to RGBA, one component per lane
    const OSALIGN(UINT, 16) packFromRGBA[4];

    const OSALIGN(UINT, 16) vDefaults[4]; // default values for the format
    const OSALIGN(UINT, 16) vDefaultsMask[4];
};

extern const SWR_FORMAT_INFO gFormatInfo[];

INLINE
const SWR_FORMAT_INFO &GetFormatInfo(SWR_FORMAT format)
{
    return gFormatInfo[format];
}

SWR_FORMAT GetMatchingFormat(SWR_TYPE type, UINT numComps, INT rpos, INT gpos, INT bpos, INT apos);
void ConvertPixel(SWR_FORMAT srcFormat, void *pSrc, SWR_FORMAT dstFormat, void *pDst);
