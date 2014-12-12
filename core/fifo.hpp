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

#ifndef __SWR_FIFO_HPP__
#define __SWR_FIFO_HPP__

#include "os.h"
#include <vector>
#include <cassert>

//#define ENABLE_STATS

template <class T>
struct VECTOR
{
    std::vector<T> mData;
    OSALIGNLINE(volatile UINT) mLock;
    UINT mCurElement;

    void initialize()
    {
        mCurElement = 0;
        mLock = 0;
        //mData.reserve(1024);
    }

    void clear()
    {
        mCurElement = 0;
        mLock = 0;
        mData.clear();
    }

    UINT getNumQueued()
    {
        return mData.size();
    }

    bool tryLock()
    {
        if (mLock)
        {
            return false;
        }

        // try to lock the FIFO
        LONG initial = InterlockedCompareExchange(&mLock, 1, 0);
        return (initial == 0);
    }

    void unlock()
    {
        mLock = 0;
    }

    T *peek()
    {
        if (mData.empty())
        {
            return NULL;
        }
        if (mCurElement == mData.size())
        {
            return NULL;
        }
        return &mData[mCurElement];
    }

    void dequeue_noinc()
    {
        mCurElement++;
    }

    bool enqueue_try_nosync(const T *entry)
    {
        mData.push_back(*entry);
        return true;
    }
};

template <class T>
struct MYVECTOR
{
    T *mData;
    OSALIGNLINE(volatile UINT) mLock;
    UINT mCurElement;
    UINT mTail;
    UINT size;

    void initialize()
    {
        mCurElement = 0;
        mLock = 0;
        mTail = 0;
        size = 512;
        mData = (T *)malloc(sizeof(T) * size);
    }

    void clear()
    {
        mCurElement = 0;
        mLock = 0;
        mTail = 0;
    }

    UINT getNumQueued()
    {
        return mTail;
    }

    bool tryLock()
    {
        if (mLock)
        {
            return false;
        }

        // try to lock the FIFO
        LONG initial = InterlockedCompareExchange(&mLock, 1, 0);
        return (initial == 0);
    }

    void unlock()
    {
        mLock = 0;
    }

    T *peek()
    {
        if (mTail == 0)
        {
            return NULL;
        }
        if (mCurElement == mTail)
        {
            return NULL;
        }
        return &mData[mCurElement];
    }

    void dequeue_noinc()
    {
        mCurElement++;
    }

    bool enqueue_try_nosync(const T *entry)
    {
        mData[mTail] = *entry;

        mTail++;
        if (mTail == size)
        {
            UINT newSize = size << 1;
            T *newBuffer = (T *)malloc(sizeof(T) * newSize);
            memcpy(newBuffer, mData, sizeof(T) * size);
            free(mData);
            mData = newBuffer;
            size = newSize;
        }

        return true;
    }
};

template <class T>
struct QUEUE
{
    OSALIGNLINE(volatile UINT) mLock;
    OSALIGNLINE(volatile UINT) mNumEntries;
    std::vector<T *> mBlocks;
    T *mCurBlock;
    UINT mHead;
    UINT mTail;
    UINT mCurBlockIdx;

    // power of 2
    static const UINT mBlockSizeShift = 6;
    static const UINT mBlockSize = 1 << mBlockSizeShift;

    void initialize()
    {
        mLock = 0;
        mHead = 0;
        mTail = 0;
        mNumEntries = 0;
        mCurBlock = (T *)malloc(mBlockSize * sizeof(T));
        mBlocks.push_back(mCurBlock);
        mCurBlockIdx = 0;
    }

    void clear()
    {
        mHead = 0;
        mTail = 0;
        mCurBlock = mBlocks[0];
        mCurBlockIdx = 0;

        mNumEntries = 0;
        _ReadWriteBarrier();
        mLock = 0;
    }

    UINT getNumQueued()
    {
        return mNumEntries;
    }

    bool tryLock()
    {
        if (mLock)
        {
            return false;
        }

        // try to lock the FIFO
        LONG initial = InterlockedCompareExchange(&mLock, 1, 0);
        return (initial == 0);
    }

    void unlock()
    {
        mLock = 0;
    }

    T *peek()
    {
        if (mNumEntries == 0)
        {
            return NULL;
        }
        UINT block = mHead >> mBlockSizeShift;
        return &mBlocks[block][mHead & (mBlockSize - 1)];
    }

    void dequeue_noinc()
    {
        mHead++;
        mNumEntries--;
    }

    bool enqueue_try_nosync(const T *entry)
    {
        memcpy(&mCurBlock[mTail], entry, sizeof(T));

        mTail++;
        if (mTail == mBlockSize)
        {
            if (++mCurBlockIdx < mBlocks.size())
            {
                mCurBlock = mBlocks[mCurBlockIdx];
            }
            else
            {
                T *newBlock = (T *)malloc(sizeof(T) * mBlockSize);
                assert(newBlock);

                mBlocks.push_back(newBlock);
                mCurBlock = newBlock;
            }

            mTail = 0;
        }

        mNumEntries++;
        return true;
    }

    void destroy()
    {
        for (int i = 0; i < mBlocks.size(); ++i)
        {
            free(mBlocks[i]);
        }
    }
};

#endif //__SWR_FIFO_HPP__
