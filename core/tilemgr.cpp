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

#include <unordered_map>

#include "fifo.hpp"
#include "tilemgr.h"

#define TILE_ID(x, y) ((x << 16 | y))

// override new/delete for alignment
void *MacroTileMgr::operator new(size_t size)
{
    return _aligned_malloc(size, 64);
}

void MacroTileMgr::operator delete(void *p)
{
    _aligned_free(p);
}

MacroTileMgr::MacroTileMgr()
{
}

void MacroTileMgr::initialize(SWR_FORMAT format)
{
    // TODO size macrotiles based on format
    m_TileWidth = KNOB_MACROTILE_X_DIM;
    m_TileHeight = KNOB_MACROTILE_Y_DIM;

    m_WorkItemsProduced = 0;
    m_WorkItemsConsumed = 0;

    m_UsedTiles.clear();
}

void MacroTileMgr::enqueue(UINT x, UINT y, BE_WORK *pWork)
{
    UINT id = TILE_ID(x, y);

    MacroTile &tile = m_Tiles[id];
    tile.m_WorkItemsFE++;

    if (tile.m_WorkItemsFE == 1)
    {
        m_UsedTiles.push_back(id);
    }

    m_WorkItemsProduced++;
    tile.m_Fifo.enqueue_try_nosync(pWork);
}

bool MacroTileMgr::markTileComplete(UINT id)
{
    assert(m_Tiles.find(id) != m_Tiles.end());
    MacroTile &tile = m_Tiles[id];
    UINT numTiles = tile.m_WorkItemsFE;
    LONG totalWorkItemsConsumedBE =
        InterlockedExchangeAdd(&m_WorkItemsConsumed, numTiles) + numTiles;

    _ReadWriteBarrier();
    tile.m_WorkItemsBE += numTiles;

    // clear out tile
    tile.m_Fifo.clear();
    tile.m_WorkItemsFE = 0;
    tile.m_WorkItemsBE = 0;

    // returns true if all tiles are complete
    return totalWorkItemsConsumedBE == m_WorkItemsProduced;
}