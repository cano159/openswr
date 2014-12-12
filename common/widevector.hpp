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

#ifndef __SWR_WIDEVECTOR_HPP__
#define __SWR_WIDEVECTOR_HPP__

#include <smmintrin.h>

INLINE simdscalar mm_add(simdscalar l, simdscalar r)
{
    return _simd_add_ps(l, r);
}
INLINE simdscalar mm_sub(simdscalar l, simdscalar r)
{
    return _simd_sub_ps(l, r);
}
INLINE simdscalar mm_mul(simdscalar l, simdscalar r)
{
    return _simd_mul_ps(l, r);
}
INLINE simdscalar mm_div(simdscalar l, simdscalar r)
{
    return _simd_div_ps(l, r);
}

INLINE simdscalari mm_add(simdscalari l, simdscalari r)
{
    return _simd_add_epi32(l, r);
}
INLINE simdscalari mm_sub(simdscalari l, simdscalari r)
{
    return _simd_sub_epi32(l, r);
}
INLINE simdscalari mm_mul(simdscalari l, simdscalari r)
{
    return _simd_mullo_epi32(l, r);
}

template <int N, typename VectorType>
struct WideVector
{
    typedef VectorType value_type;

    WideVector()
    {
    }
    WideVector(WideVector const &w)
    {
        m_element = w.m_element;
        m_elements = w.m_elements;
    }
    WideVector(VectorType vt)
    {
        m_element = vt;
        m_elements = vt;
    }
    WideVector &operator=(WideVector const &w)
    {
        m_element = w.m_element;
        m_elements = w.m_elements;

        return *this;
    }
    WideVector &operator=(VectorType vt)
    {
        m_element = vt;
        m_elements = vt;

        return *this;
    }

    WideVector &operator+=(WideVector const &w)
    {
        m_element = mm_add(m_element, w.m_element);
        m_elements += w.m_elements;

        return *this;
    }

    WideVector &operator+=(VectorType vt)
    {
        m_element = mm_add(m_element, vt);
        m_elements += vt;

        return *this;
    }

    WideVector &operator-=(WideVector const &w)
    {
        m_element = mm_sub(m_element, w.m_element);
        m_elements -= w.m_elements;

        return *this;
    }

    WideVector &operator-=(VectorType vt)
    {
        m_element = mm_sub(m_element, vt);
        m_elements -= vt;

        return *this;
    }

    WideVector &operator*=(WideVector const &w)
    {
        m_element = mm_mul(m_element, w.m_element);
        m_elements *= w.m_elements;

        return *this;
    }

    WideVector &operator*=(VectorType vt)
    {
        m_element = mm_mul(m_element, vt);
        m_elements *= vt;

        return *this;
    }

    WideVector &operator/=(WideVector const &w)
    {
        m_element = mm_div(m_element, w.m_element);
        m_elements /= w.m_elements;

        return *this;
    }

    WideVector &operator/=(VectorType vt)
    {
        m_element = mm_div(m_element, vt);
        m_elements /= vt;

        return *this;
    }

    VectorType m_element;
    WideVector<N - 1, VectorType> m_elements;
};

template <typename VectorType>
struct WideVector<1, VectorType>
{
    typedef VectorType value_type;

    WideVector()
    {
    }
    WideVector(WideVector const &w)
    {
        m_element = w.m_element;
    }
    WideVector(VectorType vt)
    {
        m_element = vt;
    }
    WideVector &operator=(WideVector const &w)
    {
        m_element = w.m_element;

        return *this;
    }
    WideVector &operator=(VectorType vt)
    {
        m_element = vt;

        return *this;
    }

    WideVector &operator+=(WideVector const &w)
    {
        m_element = mm_add(m_element, w.m_element);

        return *this;
    }

    WideVector &operator+=(VectorType vt)
    {
        m_element = mm_add(m_element, vt);

        return *this;
    }

    WideVector &operator-=(WideVector const &w)
    {
        m_element = mm_sub(m_element, w.m_element);

        return *this;
    }

    WideVector &operator-=(VectorType vt)
    {
        m_element = mm_sub(m_element, vt);

        return *this;
    }

    WideVector &operator*=(WideVector const &w)
    {
        m_element = mm_mul(m_element, w.m_element);

        return *this;
    }

    WideVector &operator*=(VectorType vt)
    {
        m_element = mm_mul(m_element, vt);

        return *this;
    }

    WideVector &operator/=(WideVector const &w)
    {
        m_element = mm_div(m_element, w.m_element);
        return *this;
    }

    WideVector &operator/=(VectorType vt)
    {
        m_element = mm_div(m_element, vt);
        return *this;
    }

    VectorType m_element;
};

template <typename VectorType>
struct WideVector<0, VectorType>
{
    typedef VectorType value_type;

    WideVector()
    {
    }
    WideVector(WideVector const &)
    {
    }
    WideVector(VectorType)
    {
    }
    WideVector &operator=(WideVector)
    {
        return *this;
    }
    WideVector &operator=(VectorType)
    {
        return *this;
    }

    WideVector &operator+=(WideVector const &)
    {
        return *this;
    }

    WideVector &operator+=(VectorType)
    {
        return *this;
    }

    WideVector &operator-=(WideVector const &)
    {
        return *this;
    }

    WideVector &operator-=(VectorType)
    {
        return *this;
    }

    WideVector &operator*=(WideVector const &)
    {
        return *this;
    }

    WideVector &operator*=(VectorType)
    {
        return *this;
    }

    WideVector &operator/=(WideVector const &)
    {
        return *this;
    }

    WideVector &operator/=(VectorType)
    {
        return *this;
    }
};

template <int N, typename VT>
WideVector<N, VT> operator+(WideVector<N, VT> const &L, WideVector<N, VT> const &R)
{
    return WideVector<N, VT>(L) += R;
}

template <int N, typename VT>
WideVector<N, VT> operator+(WideVector<N, VT> const &L, VT R)
{
    return WideVector<N, VT>(L) += R;
}

template <int N, typename VT>
WideVector<N, VT> operator-(WideVector<N, VT> const &L, WideVector<N, VT> const &R)
{
    return WideVector<N, VT>(L) -= R;
}

template <int N, typename VT>
WideVector<N, VT> operator-(WideVector<N, VT> const &L, VT R)
{
    return WideVector<N, VT>(L) -= R;
}

template <int N, typename VT>
WideVector<N, VT> operator*(WideVector<N, VT> const &L, WideVector<N, VT> const &R)
{
    return WideVector<N, VT>(L) *= R;
}

template <int N, typename VT>
WideVector<N, VT> operator*(WideVector<N, VT> const &L, VT R)
{
    return WideVector<N, VT>(L) *= R;
}

template <int N, typename VT>
WideVector<N, VT> operator/(WideVector<N, VT> const &L, VT R)
{
    return WideVector<N, VT>(L) /= R;
}

template <int N, int M, typename VT>
struct WideVectorGetter;

template <int M, typename VT>
struct WideVectorGetter<0, M, VT>
{
    static VT &Get(WideVector<M, VT> &wv)
    {
        return wv.m_element;
    }

    static VT const &Get(WideVector<M, VT> const &wv)
    {
        return wv.m_element;
    }
};

template <int N, int M, typename VT>
struct WideVectorGetter
{
    static VT &Get(WideVector<M, VT> &wv)
    {
        return WideVectorGetter<N - 1, M - 1, VT>::Get(wv.m_elements);
    }

    static VT const &Get(WideVector<M, VT> const &wv)
    {
        return WideVectorGetter<N - 1, M - 1, VT>::Get(wv.m_elements);
    }
};

template <int N, int M, typename VT>
VT &get(WideVector<M, VT> &WV)
{
    return WideVectorGetter<N, M, VT>::Get(WV);
}

template <int N, int M, typename VT>
VT const &get(WideVector<M, VT> const &WV)
{
    return WideVectorGetter<N, M, VT>::Get(WV);
}

template <typename VT>
VT &get(WideVector<0, VT> &WV, int k)
{
    static VT vt;
    return vt;
}

template <int M, typename VT>
VT &get(WideVector<M, VT> &WV, int k)
{
    return k == 0 ? WV.m_element : get(WV.m_elements, k - 1);
}

template <typename VT>
VT &get(WideVector<1, VT> &WV, int k)
{
    static VT vt;
    return k == 0 ? WV.m_element : vt;
}

template <typename VT>
VT const &get(WideVector<0, VT> const &WV, int k)
{
    static VT vt;
    return vt;
}

template <int M, typename VT>
VT const &get(WideVector<M, VT> const &WV, int k)
{
    return k == 0 ? WV.m_element : get(WV.m_elements, k - 1);
}

template <typename VT>
VT const &get(WideVector<1, VT> const &WV, int k)
{
    static VT vt;
    return k == 0 ? WV.m_element : vt;
}

#endif //__SWR_WIDEVECTOR_HPP__
