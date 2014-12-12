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

#include <smmintrin.h>

#include "rdtsc.h"
#include "backend.h"
#include "tilemgr.h"

void ClearTile(RENDERTARGET *pRenderTarget, UINT tileX, UINT tileY, BYTE *pValue)
{
    UINT x = tileX << KNOB_TILE_X_DIM_SHIFT;
    UINT y = tileY << KNOB_TILE_Y_DIM_SHIFT;

    // assume bpp is 4B so we can use simd
    assert(GetFormatInfo(pRenderTarget->format).Bpp == 4);

    BYTE *pTileBuffer = pRenderTarget->pTileData + y * pRenderTarget->widthInBytes + x * KNOB_TILE_Y_DIM * 4;

    simdscalar vClear = _simd_load1_ps((const float *)pValue);

    for (UINT yy = 0; yy < KNOB_TILE_Y_DIM / SIMD_TILE_Y_DIM; ++yy)
    {
        for (UINT xx = 0; xx < KNOB_TILE_X_DIM / SIMD_TILE_X_DIM; ++xx)
        {
            _simd_store_ps((float *)pTileBuffer, vClear);
            pTileBuffer += KNOB_VS_SIMD_WIDTH * 4;
        }
    }
}

INLINE void ClearMacroTile(DRAW_CONTEXT *pDC, RENDERTARGET *pRT, UINT macroTileX, UINT macroTileY, BYTE *pValue)
{
    int top = pDC->state.scissorMacroHeightInTiles * macroTileY;
    int bottom = top + pDC->state.scissorMacroHeightInTiles - 1;
    int left = pDC->state.scissorMacroWidthInTiles * macroTileX;
    int right = left + pDC->state.scissorMacroWidthInTiles - 1;

    // intersect with scissor
    top = std::max(top, pDC->state.scissorInTiles.top);
    left = std::max(left, pDC->state.scissorInTiles.left);
    bottom = std::min(bottom, pDC->state.scissorInTiles.bottom);
    right = std::min(right, pDC->state.scissorInTiles.right);

    for (int y = top; y <= bottom; ++y)
    {
        for (int x = left; x <= right; ++x)
        {
            ClearTile(pRT, x, y, pValue);
        }
    }
}

void ProcessClearBE(DRAW_CONTEXT *pDC, UINT macroTile, void *pUserData)
{
    CLEAR_DESC *pClear = (CLEAR_DESC *)pUserData;

    RDTSC_START(BEClear);

    UINT x, y;
    MacroTileMgr::getTileIndices(macroTile, x, y);

    if (pClear->flags.mask & CLEAR_COLOR)
    {
        ClearMacroTile(pDC, pDC->state.pRenderTargets[SWR_ATTACHMENT_COLOR0], x, y, (BYTE *)&pClear->clearRTColor);
    }

    if (pClear->flags.mask & CLEAR_DEPTH)
    {
        ClearMacroTile(pDC, pDC->state.pRenderTargets[SWR_ATTACHMENT_DEPTH], x, y, (BYTE *)&pClear->clearDepth);
    }

    RDTSC_STOP(BEClear, 0, 0);
}

// Deswizzles and stores 1 tile to memory, 1 SIMD tile at a time
void storeTile(DRIVER_TYPE driver, UINT tileX, UINT tileY, RENDERTARGET *pRenderTarget, void *pData, UINT pitch)
{
    UINT x = tileX << KNOB_TILE_X_DIM_SHIFT;
    UINT y = tileY << KNOB_TILE_Y_DIM_SHIFT;

    UINT swizzledY;
    INT swizzledPitch;

    if (driver == DX)
    {
        swizzledY = y;
        swizzledPitch = pitch;
    }
    else
    {
        swizzledY = pRenderTarget->apiHeight - y - 1;
        swizzledPitch = -(INT)pitch;
    }

    const SWR_FORMAT_INFO &format = GetFormatInfo(pRenderTarget->format);

    BYTE *pTileBuffer = pRenderTarget->pTileData + y * pRenderTarget->widthInBytes + tileX * KNOB_TILE_X_DIM * KNOB_TILE_Y_DIM * 4;
    BYTE *pBuffer = (BYTE *)pData + swizzledY * pitch + x * format.Bpp;

    BYTE *pRow0 = pBuffer;
    BYTE *pRow1 = pBuffer + swizzledPitch;

    for (UINT row = 0; row < KNOB_TILE_Y_DIM / SIMD_TILE_Y_DIM; ++row)
    {
        BYTE *pStartRow0 = pRow0;
        BYTE *pStartRow1 = pRow1;

        for (UINT col = 0; col < KNOB_TILE_X_DIM / SIMD_TILE_X_DIM; ++col)
        {
            __m128i *pZRow01 = (__m128i *)pTileBuffer;

            __m128i vQuad00 = _mm_load_si128(pZRow01);
            __m128i vQuad01 = _mm_load_si128(pZRow01 + 1);

            __m128i vRow00 = _mm_unpacklo_epi64(vQuad00, vQuad01);
            __m128i vRow10 = _mm_unpackhi_epi64(vQuad00, vQuad01);

            _mm_storeu_si128((__m128i *)pRow0, vRow00);
            _mm_storeu_si128((__m128i *)pRow1, vRow10);

            pRow0 += 16;
            pRow1 += 16;
            pTileBuffer += 16 * 2;
        }

        pRow0 = pStartRow0 + 2 * swizzledPitch;
        pRow1 = pStartRow1 + 2 * swizzledPitch;
    }
}

void storeTilePartial(DRIVER_TYPE driver, UINT tileX, UINT tileY, UINT sizeX, UINT sizeY, RENDERTARGET *pRT, void *pData, UINT pitch)
{
    UINT x = tileX << KNOB_TILE_X_DIM_SHIFT;
    UINT y = tileY << KNOB_TILE_Y_DIM_SHIFT;

    UINT swizzledY;
    INT swizzledPitch;

    if (driver == DX)
    {
        swizzledY = y;
        swizzledPitch = pitch;
    }
    else
    {
        swizzledY = pRT->apiHeight - y - 1;
        swizzledPitch = -(INT)pitch;
    }

    const SWR_FORMAT_INFO &format = GetFormatInfo(pRT->format);
    UINT *pTileBuffer = (UINT *)(pRT->pTileData + y * pRT->widthInBytes + tileX * KNOB_TILE_X_DIM * KNOB_TILE_Y_DIM * 4);
    BYTE *pBuffer = (BYTE *)pData + swizzledY * pitch + x * format.Bpp;

    for (UINT row = 0; row < sizeY; ++row)
    {
        BYTE *pStartRow = pBuffer;
        for (UINT col = 0; col < sizeX; ++col)
        {
            UINT tx = col * 4;
            UINT ty = row;

            tx = ((tx << 1) & 0x30) + (tx & 0x07);
            ty = ((ty << 5) & 0xC0) + ((ty << 3) & 0x08);

            UINT tileOffsetInBytes = tx | ty;

            UINT *pSrcAddr = (UINT *)((BYTE *)pTileBuffer + tileOffsetInBytes);

            *(UINT *)pBuffer = *pSrcAddr;

            pBuffer += 4;
        }
        pBuffer = pStartRow + swizzledPitch;
    }
}

void ProcessStoreTileBE(DRAW_CONTEXT *pDC, UINT macroTile, void *pData)
{
    RDTSC_START(BEStoreTiles);
    STORE_DESC *pDesc = (STORE_DESC *)pData;
    SWR_CONTEXT *pContext = pDC->pContext;

    RENDERTARGET *pRT = pDC->state.pRenderTargets[SWR_ATTACHMENT_COLOR0];
    UINT pitch = pDesc->pitch;

    UINT x, y;
    MacroTileMgr::getTileIndices(macroTile, x, y);

    int top = pDC->state.scissorMacroHeightInTiles * y;
    int bottom = top + pDC->state.scissorMacroHeightInTiles;
    int left = pDC->state.scissorMacroWidthInTiles * x;
    int right = left + pDC->state.scissorMacroWidthInTiles;

    int apiWidthInWholeTiles = pRT->apiWidth >> KNOB_TILE_X_DIM_SHIFT;
    int apiHeightInWholeTiles = pRT->apiHeight >> KNOB_TILE_Y_DIM_SHIFT;

    right = std::min(right, apiWidthInWholeTiles);
    bottom = std::min(bottom, apiHeightInWholeTiles);

    UINT partialX = pRT->apiWidth & (KNOB_TILE_X_DIM - 1);
    UINT partialY = pRT->apiHeight & (KNOB_TILE_Y_DIM - 1);
    UINT numTiles = 0;

    // store whole tiles
    for (int y = top; y < bottom; ++y)
    {
        for (int x = left; x < right; ++x)
        {
#if KNOB_VS_SIMD_WIDTH == 4
            storeTilePartial(pContext->driverType, x, y, KNOB_TILE_X_DIM, KNOB_TILE_Y_DIM, pRT, pDesc->pData, pitch);
#elif KNOB_VS_SIMD_WIDTH == 8
            storeTile(pContext->driverType, x, y, pRT, pDesc->pData, pitch);
#else
#error Unsupported arch
#endif
            numTiles++;
        }
    }

    // store partial tiles, if any
    if (partialX)
    {
        // store partial tiles along right edge
        for (int y = top; y < bottom; ++y)
        {
            storeTilePartial(pContext->driverType, apiWidthInWholeTiles, y, partialX, KNOB_TILE_Y_DIM, pRT, pDesc->pData, pitch);
        }
    }

    if (partialY > 0)
    {
        // store partial tiles along bottom edge
        for (int x = left; x < right; ++x)
        {
            storeTilePartial(pContext->driverType, x, apiHeightInWholeTiles, KNOB_TILE_X_DIM, partialY, pRT, pDesc->pData, pitch);
        }
    }

    // store partial tile at bottom/right corner
    if (partialX > 0 && partialY > 0)
    {
        storeTilePartial(pContext->driverType, apiWidthInWholeTiles, apiHeightInWholeTiles, partialX, partialY, pRT, pDesc->pData, pitch);
    }

    RDTSC_STOP(BEStoreTiles, numTiles, pDC->drawId);
}

void ProcessCopyBE(DRAW_CONTEXT *pDC, UINT macroTile, void *pData)
{
    RDTSC_START(BEProcessCopy);

    COPY_DESC *pCopy = (COPY_DESC *)pData;

    UINT x, y;
    MacroTileMgr::getTileIndices(macroTile, x, y);

    assert(pCopy->rt < SWR_NUM_ATTACHMENTS);
    RENDERTARGET *pRT = pDC->state.pRenderTargets[pCopy->rt];

    INT mtLeft = x * pDC->pTileMgr->getTileWidth();
    INT mtRight = mtLeft + pDC->pTileMgr->getTileWidth();
    INT mtTop = y * pDC->pTileMgr->getTileHeight();
    INT mtBottom = mtTop + pDC->pTileMgr->getTileHeight();

    // intersect macro tile with copy rect
    INT srcLeft = std::max(pCopy->srcX, mtLeft);
    INT srcRight = std::min(pCopy->srcX + (INT)pCopy->width, mtRight);
    INT srcTop = std::max(pCopy->srcY, mtTop);
    INT srcBot = std::min(pCopy->srcY + (INT)pCopy->height, mtBottom);

    INT dstX = pCopy->dstX + (srcLeft - pCopy->srcX);
    INT dstY = pCopy->dstY + (srcTop - pCopy->srcY);

    const SWR_FORMAT_INFO &format = GetFormatInfo(pRT->format);
    const UINT Bpp = format.Bpp;
    const UINT tileSizeInBytes = KNOB_TILE_X_DIM * KNOB_TILE_Y_DIM * Bpp;
    const UINT tilePitch = pRT->widthInTiles * tileSizeInBytes;

    const SWR_FORMAT_INFO &dstFormatInfo = GetFormatInfo(pCopy->dstFormat);

    for (INT sy = srcTop, dy = dstY; sy < srcBot; ++sy, ++dy)
    {
        BYTE *pDstAddr = (BYTE *)pCopy->pData + dy * pCopy->pitch + dstX * dstFormatInfo.Bpp;

        for (INT sx = srcLeft, dx = dstX; sx < srcRight; ++sx, ++dx)
        {
            // calculate src addr
            UINT tileX = sx >> KNOB_TILE_X_DIM_SHIFT;
            UINT tileY = sy >> KNOB_TILE_Y_DIM_SHIFT;
            UINT offsetX = sx & (KNOB_TILE_X_DIM - 1);
            UINT offsetY = sy & (KNOB_TILE_Y_DIM - 1);

#if KNOB_TILE_X_DIM == 2 && KNOB_TILE_Y_DIM == 2
            UINT tileOffsetInBytes = offsetY * KNOB_TILE_Y_DIM * Bpp + offsetX * Bpp;
#else
            // 2x2 tile swizzle
            UINT tx = offsetX * Bpp;
            UINT ty = offsetY;

            tx = ((tx << 1) & 0x30) + (tx & 0x07);
            ty = ((ty << 5) & 0xC0) + ((ty << 3) & 0x08);

            UINT tileOffsetInBytes = tx | ty;
#endif

            UINT *pSrcAddr = (UINT *)((BYTE *)pRT->pTileData + tileY * tilePitch + tileX * tileSizeInBytes + tileOffsetInBytes);

            ConvertPixel(pRT->format, pSrcAddr, pCopy->dstFormat, pDstAddr);

            pDstAddr += dstFormatInfo.Bpp;
        }
    }

    RDTSC_STOP(BEProcessCopy, (srcBot - srcTop) * (srcRight - srcLeft), 0);
}
