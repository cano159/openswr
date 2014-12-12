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

#include <set>
#include <unordered_map>
#include "formats.h"
#include "fifo.hpp"
#include "context.h"

struct MacroTile
{
    QUEUE<BE_WORK> m_Fifo;
    UINT m_WorkItemsFE;
    UINT m_WorkItemsBE;

    MacroTile()
    {
        m_Fifo.initialize();
        m_WorkItemsFE = 0;
        m_WorkItemsBE = 0;
    }

    ~MacroTile()
    {
    }
};

class MacroTileMgr
{
public:
    MacroTileMgr();
    ~MacroTileMgr()
    {
        for (auto &tile : m_Tiles)
        {
            tile.second.m_Fifo.destroy();
        }
    }

    void initialize(SWR_FORMAT format);
    INLINE UINT getTileWidth()
    {
        return m_TileWidth;
    }
    INLINE UINT getTileHeight()
    {
        return m_TileHeight;
    }
    INLINE std::vector<UINT> &getUsedTiles()
    {
        return m_UsedTiles;
    }
    INLINE MacroTile &getMacroTile(UINT id)
    {
        return m_Tiles[id];
    }
    bool markTileComplete(UINT id);

    INLINE bool isWorkComplete()
    {
        return m_WorkItemsProduced == m_WorkItemsConsumed;
    }

    void enqueue(UINT x, UINT y, BE_WORK *pWork);

    void *operator new(size_t size);
    void operator delete(void *p);

    static INLINE void getTileIndices(UINT tileID, UINT &x, UINT &y)
    {
        y = tileID & 0xffff;
        x = (tileID >> 16) & 0xffff;
    }

private:
    SWR_FORMAT m_Format;
    UINT m_TileWidth;
    UINT m_TileHeight;

    std::unordered_map<UINT, MacroTile> m_Tiles;
    std::vector<UINT> m_UsedTiles;

    OSALIGNLINE(LONG) m_WorkItemsProduced;
    OSALIGNLINE(volatile LONG) m_WorkItemsConsumed;
};
