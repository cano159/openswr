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

#ifndef SWRLIB_CONTAINERS_HPP__
#define SWRLIB_CONTAINERS_HPP__

#include <functional>
#include <cassert>

namespace SWRL
{

template <typename T, int NUM_ELEMENTS>
struct UncheckedFixedVector
{
    UncheckedFixedVector()
        : mSize(0)
    {
    }

    UncheckedFixedVector(std::size_t size, T const &exemplar)
    {
        this->mSize = 0;
        for (std::size_t i = 0; i < size; ++i)
            this->push_back(exemplar);
    }

    template <typename Iter>
    UncheckedFixedVector(Iter fst, Iter lst)
    {
        this->mSize = 0;
        for (; fst != lst; ++fst)
            this->push_back(*fst);
    }

    UncheckedFixedVector(UncheckedFixedVector const &UFV)
    {
        this->mSize = 0;
        for (std::size_t i = 0, N = UFV.size(); i < N; ++i)
            (*this)[i] = UFV[i];
        this->mSize = UFV.size();
    }

    UncheckedFixedVector &operator=(UncheckedFixedVector const &UFV)
    {
        for (std::size_t i = 0, N = UFV.size(); i < N; ++i)
            (*this)[i] = UFV[i];
        this->mSize = UFV.size();
        return *this;
    }

    T *begin()
    {
        return &this->mElements[0];
    }
    T *end()
    {
        return &this->mElements[0] + this->mSize;
    }
    T const *begin() const
    {
        return &this->mElements[0];
    }
    T const *end() const
    {
        return &this->mElements[0] + this->mSize;
    }

    friend bool operator==(UncheckedFixedVector const &L, UncheckedFixedVector const &R)
    {
        if (L.size() != R.size())
            return false;
        for (std::size_t i = 0, N = L.size(); i < N; ++i)
        {
            if (L[i] != R[i])
                return false;
        }
        return true;
    }

    friend bool operator!=(UncheckedFixedVector const &L, UncheckedFixedVector const &R)
    {
        if (L.size() != R.size())
            return true;
        for (std::size_t i = 0, N = L.size(); i < N; ++i)
        {
            if (L[i] != R[i])
                return true;
        }
        return false;
    }

    T &operator[](std::size_t idx)
    {
        return this->mElements[idx];
    }
    T const &operator[](std::size_t idx) const
    {
        return this->mElements[idx];
    }
    void push_back(T const &t)
    {
        this->mElements[this->mSize] = t;
        ++this->mSize;
    }
    void pop_back()
    {
        assert(this->mSize > 0);
        --this->mSize;
    }
    T &back()
    {
        return this->mElements[this->mSize - 1];
    }
    T const &back() const
    {
        return this->mElements[this->mSize - 1];
    }
    bool empty() const
    {
        return this->mSize == 0;
    }
    std::size_t size() const
    {
        return this->mSize;
    }
    void resize(std::size_t sz)
    {
        this->mSize = sz;
    }
    void clear()
    {
        this->resize(0);
    }

private:
    std::size_t mSize;
    T mElements[NUM_ELEMENTS];
};

template <typename T, int NUM_ELEMENTS>
struct FixedStack : UncheckedFixedVector<T, NUM_ELEMENTS>
{
    FixedStack()
    {
    }

    void push(T const &t)
    {
        this->push_back(t);
    }

    void pop()
    {
        this->pop_back();
    }

    T &top()
    {
        return this->back();
    }

    T const &top() const
    {
        return this->back();
    }
};

} // end SWRL

namespace std
{

template <typename T, int N>
struct hash<SWRL::UncheckedFixedVector<T, N>>
{
    size_t operator()(SWRL::UncheckedFixedVector<T, N> const &v) const
    {
        if (v.size() == 0)
            return 0;
        std::hash<T> H;
        size_t x = H(v[0]);
        if (v.size() == 1)
            return x;
        for (size_t i = 1; i < v.size(); ++i)
            x ^= H(v[i]) + 0x9e3779b9 + (x << 6) + (x >> 2);
        return x;
    }
};

} // end std.

#endif //SWRLIB_CONTAINERS_HPP__
