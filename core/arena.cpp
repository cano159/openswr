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

#include "context.h"
#include "arena.h"

#include <cmath>

VOID Arena::Init()
{
    m_memUsed = 0;
    m_pCurBlock = NULL;
    m_pUsedBlocks = NULL;
}

VOID *Arena::AllocAligned(UINT size, UINT align)
{
    if (m_pCurBlock)
    {
        ArenaBlock *pCurBlock = m_pCurBlock;
        pCurBlock->offset = ALIGN_UP(pCurBlock->offset, align);

        if ((pCurBlock->offset + size) < pCurBlock->blockSize)
        {
            BYTE *pMem = (BYTE *)pCurBlock->pMem + pCurBlock->offset;
            pCurBlock->offset += size;
            return pMem;
        }

        // Not enough memory in this arena so lets move to a new block.
        pCurBlock->pNext = m_pUsedBlocks;
        m_pUsedBlocks = pCurBlock;
        m_pCurBlock = NULL;
    }

    static const UINT ArenaBlockSize = 1024 * 1024;
    UINT defaultBlockSize = ArenaBlockSize;
    if (m_pUsedBlocks == NULL)
    {
        // First allocation after reset. Let's make the first block be the total
        // memory allocated during last set of allocations prior to reset.
        defaultBlockSize = std::max(m_memUsed, defaultBlockSize);
        m_memUsed = 0;
    }

    UINT blockSize = std::max(size, defaultBlockSize);
    blockSize = ALIGN_UP(blockSize, KNOB_VS_SIMD_WIDTH * 4);

    VOID *pMem = _aligned_malloc(blockSize, KNOB_VS_SIMD_WIDTH * 4); // Arena blocks are always simd byte aligned.

    m_pCurBlock = (ArenaBlock *)malloc(sizeof(ArenaBlock));
    m_pCurBlock->pMem = pMem;
    m_pCurBlock->blockSize = blockSize;
    m_pCurBlock->offset = size;

    m_memUsed += blockSize;

    return pMem;
}

VOID *Arena::Alloc(UINT size)
{
    return AllocAligned(size, 1);
}

VOID Arena::Reset()
{
    if (m_pCurBlock)
    {
        m_pCurBlock->offset = 0;

        // If we needed to allocate used blocks then reset current.
        // The next time we allocate we'll grow the current block
        // to match all the memory allocated this for this frame.
        if (m_pUsedBlocks)
        {
            m_pCurBlock->pNext = m_pUsedBlocks;
            m_pUsedBlocks = m_pCurBlock;
            m_pCurBlock = NULL;
        }
    }

    while (m_pUsedBlocks)
    {
        ArenaBlock *pBlock = m_pUsedBlocks;
        m_pUsedBlocks = pBlock->pNext;

        _aligned_free(pBlock->pMem);
        free(pBlock);
    }
}