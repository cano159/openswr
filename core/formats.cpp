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

#include "formats.h"

// order must match SWR_FORMAT
const SWR_FORMAT_INFO gFormatInfo[] = {
    { SWR_TYPE_UNKNOWN, 0, 0, 0, 0, 0, 0, 0, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 } }, // NULL

    { SWR_TYPE_FLOAT, 1, 4, 4, 0, 0, 0, 0, { 0x80808080, 0x80808080, 0x80808080, 0x03020100 }, { 0x0f0e0d0c, 0x80808080, 0x80808080, 0x80808080 },
      // Spec value (not used for GL_MODULATE workaround)			{0, 0, 0, 0x3f800000},
      { 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000 },
      { 0x80808080, 0x80808080, 0x80808080, 0x00000000 } }, // A32_FLOAT

    { SWR_TYPE_FLOAT, 1, 4, 4, 0, 0, 0, 0, { 0x03020100, 0x07060504, 0x0b0a0908, 0x0f0e0d0c }, { 0x03020100, 0x80808080, 0x80808080, 0x80808080 }, { 0, 0, 0, 0x3f800000 }, { 0x00000000, 0x80808080, 0x80808080, 0x80808080 } },  // R32_FLOAT
    { SWR_TYPE_FLOAT, 2, 8, 4, 0, 1, 0, 0, { 0x03020100, 0x07060504, 0x0b0a0908, 0x0f0e0d0c }, { 0x03020100, 0x07060504, 0x80808080, 0x80808080 }, { 0, 0, 0, 0x3f800000 }, { 0x00000000, 0x00000000, 0x80808080, 0x80808080 } },  // RG32_FLOAT
    { SWR_TYPE_FLOAT, 3, 12, 4, 0, 1, 2, 0, { 0x03020100, 0x07060504, 0x0b0a0908, 0x0f0e0d0c }, { 0x03020100, 0x07060504, 0x0b0a0908, 0x80808080 }, { 0, 0, 0, 0x3f800000 }, { 0x00000000, 0x00000000, 0x00000000, 0x80808080 } }, // RGB32_FLOAT
    { SWR_TYPE_FLOAT, 4, 16, 4, 0, 1, 2, 3, { 0x03020100, 0x07060504, 0x0b0a0908, 0x0f0e0d0c }, { 0x03020100, 0x07060504, 0x0b0a0908, 0x0f0e0d0c }, { 0, 0, 0, 0x3f800000 }, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 } }, // RGBA32_FLOAT

    { SWR_TYPE_UNORM8, 1, 1, 1, 0, 0, 0, 0, { 0x80808000, 0x80808001, 0x80808002, 0x80808003 }, { 0x80808000, 0x80808080, 0x80808080, 0x80808080 }, { 0, 0, 0, 1 }, { 0x00000000, 0x80808080, 0x80808080, 0x80808080 } }, // R8_UNORM
    { SWR_TYPE_UNORM8, 2, 2, 1, 0, 1, 0, 0, { 0x80808000, 0x80808001, 0x80808002, 0x80808003 }, { 0x80800400, 0x80808080, 0x80808080, 0x80808080 }, { 0, 0, 0, 1 }, { 0x00000000, 0x00000000, 0x80808080, 0x80808080 } }, // RG8_UNORM
    { SWR_TYPE_UNORM8, 3, 3, 1, 0, 1, 2, 0, { 0x80808000, 0x80808001, 0x80808002, 0x80808003 }, { 0x80080400, 0x80808080, 0x80808080, 0x80808080 }, { 0, 0, 0, 1 }, { 0x00000000, 0x00000000, 0x00000000, 0x80808080 } }, // RGB8_UNORM
    { SWR_TYPE_UNORM8, 4, 4, 1, 0, 1, 2, 3, { 0x80808000, 0x80808001, 0x80808002, 0x80808003 }, { 0x0c080400, 0x80808080, 0x80808080, 0x80808080 }, { 0, 0, 0, 1 }, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 } }, // RGBA8_UNORM

    { SWR_TYPE_UNORM8, 3, 3, 1, 2, 1, 0, 0, { 0x80808002, 0x80808001, 0x80808000, 0x80808003 }, { 0x80000408, 0x80808080, 0x80808080, 0x80808080 }, { 0, 0, 0, 1 }, { 0x00000000, 0x00000000, 0x00000000, 0x80808080 } }, // BGR8_UNORM
    { SWR_TYPE_UNORM8, 4, 4, 1, 2, 1, 0, 3, { 0x80808002, 0x80808001, 0x80808000, 0x80808003 }, { 0x0c000408, 0x80808080, 0x80808080, 0x80808080 }, { 0, 0, 0, 1 }, { 0x00000000, 0x00000000, 0x00000000, 0x00000000 } }, // BGRA8_UNORM

    { SWR_TYPE_SNORM8, 3, 3, 1, 0, 1, 2, 0, { 0x80808000, 0x80808001, 0x80808002, 0x80808003 }, { 0x80808000, 0x80808080, 0x80808080, 0x80808080 }, { 0, 0, 0, 1 }, { 0x00000000, 0x00000000, 0x00000000, 0x80808080 } }, //RGB8_SNORM

    { SWR_TYPE_SINT16, 2, 4, 2, 0, 1, 0, 0, { 0x80800100, 0x80800302, 0x80808080, 0x80808080 }, { 0x07060100, 0x80808080, 0x80808080, 0x80808080 }, { 0, 0, 0, 1 }, { 0x00000000, 0x00000000, 0x80808080, 0x80808080 } }, // RG16_SINT
};

INT SwrNumBytes(SWR_FORMAT format)
{
    return GetFormatInfo(format).Bpc;
}

SWR_TYPE SwrFormatType(SWR_FORMAT format)
{
    return GetFormatInfo(format).type;
}

INT SwrNumComponents(SWR_FORMAT format)
{
    return GetFormatInfo(format).numComps;
}

SWR_FORMAT GetMatchingFormat(SWR_TYPE type, UINT numComps, INT rpos, INT gpos, INT bpos, INT apos)
{
    for (UINT i = 1; i < NUM_SWR_FORMATS; ++i)
    {
        const SWR_FORMAT_INFO &info = GetFormatInfo((SWR_FORMAT)i);
        if (info.type == type && info.numComps == numComps)
        {
            if ((rpos == -1 || info.rpos == rpos) &&
                (gpos == -1 || info.gpos == gpos) &&
                (bpos == -1 || info.bpos == bpos) &&
                (apos == -1 || info.apos == apos))
            {
                return (SWR_FORMAT)i;
            }
        }
    }
    return (NULL_FORMAT);
}

void ConvertPixel(SWR_FORMAT srcFormat, void *pSrc, SWR_FORMAT dstFormat, void *pDst)
{
    const SWR_FORMAT_INFO &srcFmtInfo = GetFormatInfo(srcFormat);
    const SWR_FORMAT_INFO &dstFmtInfo = GetFormatInfo(dstFormat);

// fast path - src and dst formats are the same, just copy the pixel data
// @todo this fast path isn't any faster than the code below
#if 0
	if (srcFormat == dstFormat)
	{
		__m128i vSrc = _mm_loadu_si128((const __m128i*)pSrc);
		__m128i vDstOrg;
		if (dstFmtInfo.Bpp <= 4)
		{
			vDstOrg = _mm_castps_si128(_mm_load1_ps((const float*)pDst));
		}
		else
		{
			vDstOrg = _mm_loadu_si128((const __m128i*)pDst);
		}
		__m128i vDst = _mm_blendv_epi8(vSrc, vDstOrg, _mm_load_si128((__m128i*)&dstFmtInfo.packFromRGBA));
		_mm_storeu_si128((__m128i*)pDst, vDst);
		return;
	}
#endif

    // slower fast path - src and dst format types are the same, just need to swizzle memory
    if (srcFmtInfo.type == dstFmtInfo.type)
    {
        assert(srcFmtInfo.Bpc == dstFmtInfo.Bpc);

        __m128i vSrc = _mm_loadu_si128((const __m128i *)pSrc);

        __m128i vSrcRGBA = _mm_shuffle_epi8(vSrc, _mm_load_si128((__m128i *)&srcFmtInfo.unpackToRGBA));

        // handle default values
        __m128i vMask = _mm_load_si128((const __m128i *)&srcFmtInfo.vDefaultsMask);
        __m128i vDefaults = _mm_load_si128((const __m128i *)&srcFmtInfo.vDefaults);
        vSrcRGBA = _mm_blendv_epi8(vSrcRGBA, vDefaults, vMask);

        __m128i vDst = _mm_shuffle_epi8(vSrcRGBA, _mm_load_si128((__m128i *)&dstFmtInfo.packFromRGBA));

        //@todo really need packstore!
        // loadu can be very expensive.  Simple fast path to use load1 instead of loadu if destination
        // pixel size is <= 4B
        __m128i vDstOrg;
        if (dstFmtInfo.Bpp <= 4)
        {
            vDstOrg = _mm_castps_si128(_mm_load1_ps((const float *)pDst));
            vDst = _mm_blendv_epi8(vDst, vDstOrg, _mm_load_si128((__m128i *)&dstFmtInfo.packFromRGBA));
            _mm_store_ss((float *)pDst, _mm_castsi128_ps(vDst));
        }
        else
        {
            vDstOrg = _mm_loadu_si128((const __m128i *)pDst);
            vDst = _mm_blendv_epi8(vDst, vDstOrg, _mm_load_si128((__m128i *)&dstFmtInfo.packFromRGBA));
            _mm_storeu_si128((__m128i *)pDst, vDst);
        }

        return;
    }

    // slow path - unpack and convert src to float, swizzle, and repack to dst format
    __m128i vSrcI = _mm_loadu_si128((const __m128i *)pSrc);

    // unpack to RGBA float
    __m128i vSwizzledI = _mm_shuffle_epi8(vSrcI, _mm_load_si128((__m128i *)&srcFmtInfo.unpackToRGBA));

    // handle default values
    __m128i vMask = _mm_load_si128((const __m128i *)&srcFmtInfo.vDefaultsMask);
    __m128i vDefaults = _mm_load_si128((const __m128i *)&srcFmtInfo.vDefaults);
    vSwizzledI = _mm_blendv_epi8(vSwizzledI, vDefaults, vMask);

    __m128 vConverted;
    __m128i vConvertedI;

    switch (srcFmtInfo.type)
    {
    case SWR_TYPE_UNORM8:
        vConverted = _mm_cvtepi32_ps(vSwizzledI);
        vConverted = _mm_mul_ps(vConverted, _mm_set1_ps((float)(1.0 / 255.0)));
        break;

    case SWR_TYPE_FLOAT:
        vConverted = _mm_castsi128_ps(vSwizzledI);
        break;
    default:
        assert(0 && "Unsupprted type");
    }

    // type conversion
    switch (dstFmtInfo.type)
    {
    case SWR_TYPE_UNORM8:
        vConverted = _mm_mul_ps(vConverted, _mm_set1_ps(255.0));
        vConvertedI = _mm_cvtps_epi32(vConverted);
        break;

    case SWR_TYPE_FLOAT:
        vConvertedI = _mm_castps_si128(vConverted);
        break;
    default:
        assert(0 && "Unsupported type");
    }

    // swizzle and pack
    vConvertedI = _mm_shuffle_epi8(vConvertedI, _mm_load_si128((__m128i *)&dstFmtInfo.packFromRGBA));

    // store
    //@todo really need packstore!
    if (dstFmtInfo.Bpp <= 4)
    {
        __m128i vDstOrg = _mm_castps_si128(_mm_load1_ps((const float *)pDst));
        vConvertedI = _mm_blendv_epi8(vConvertedI, vDstOrg, _mm_load_si128((__m128i *)&dstFmtInfo.packFromRGBA));
        _mm_store_ss((float *)pDst, _mm_castsi128_ps(vConvertedI));
    }
    else
    {
        __m128i vDstOrg = _mm_loadu_si128((const __m128i *)pDst);
        vConvertedI = _mm_blendv_epi8(vConvertedI, vDstOrg, _mm_load_si128((__m128i *)&dstFmtInfo.packFromRGBA));
        _mm_storeu_si128((__m128i *)pDst, vConvertedI);
    }
}
