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

class Arena
{
public:
    Arena()
        : m_pCurBlock(NULL), m_pUsedBlocks(NULL), m_memUsed(0)
    {
    }

    ~Arena()
    {
    }

    VOID Init();

    VOID *AllocAligned(UINT size, UINT align);
    VOID *Alloc(UINT size);
    VOID Reset();

private:
    struct ArenaBlock
    {
        ArenaBlock()
            : pMem(NULL), blockSize(0), pNext(NULL)
        {
        }

        VOID *pMem;
        UINT blockSize;
        UINT offset;
        ArenaBlock *pNext;
    };

    ArenaBlock *m_pCurBlock;
    ArenaBlock *m_pUsedBlocks;

    UINT m_memUsed; // total bytes allocated since last reset.
};
