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

#ifndef SWRLIB_ALGEBRA_HPP__
#define SWRLIB_ALGEBRA_HPP__

#include <array>
#include "utils.h"

namespace SWRL
{

typedef std::array<float, 4> v4f;
typedef std::array<v4f, 4> m44f;
typedef std::array<double, 4> v4d;
typedef std::array<v4d, 4> m44d;

template <typename T>
inline std::array<T, 4> MakeV4(T a, T b, T c, T d)
{
    std::array<T, 4> A;
    A[0] = a;
    A[1] = b;
    A[2] = c;
    A[3] = d;
    return A;
}

template <typename T>
inline std::array<T, 4> MakeV4(T a, T b, T c)
{
    std::array<T, 4> A;
    A[0] = a;
    A[1] = b;
    A[2] = c;
    return A;
}

template <typename T>
inline std::array<T, 4> MakeV4(T a, T b)
{
    std::array<T, 4> A;
    A[0] = a;
    A[1] = b;
    return A;
}

template <typename T>
inline std::array<T, 4> MakeV4(T a)
{
    std::array<T, 4> A;
    A[0] = a;
    return A;
}

template <typename Ty>
std::array<std::array<Ty, 4>, 4> M44Id()
{
    std::array<std::array<Ty, 4>, 4> id;
    for (int i = 0; i < 4; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            id[i][j] = i == j ? (Ty)1 : (Ty)0;
        }
    }

    return id;
}

inline bool mequals(const m44f &a, const m44f &b)
{
    __m128 va = _mm_load_ps((const float *)&a[0][0]);
    __m128 vb = _mm_load_ps((const float *)&b[0][0]);
    __m128 vcmp = _mm_cmpeq_ps(va, vb);

    va = _mm_load_ps((const float *)&a[1][0]);
    vb = _mm_load_ps((const float *)&b[1][0]);
    vcmp = _mm_and_ps(vcmp, _mm_cmpeq_ps(va, vb));

    va = _mm_load_ps((const float *)&a[2][0]);
    vb = _mm_load_ps((const float *)&b[2][0]);
    vcmp = _mm_and_ps(vcmp, _mm_cmpeq_ps(va, vb));

    va = _mm_load_ps((const float *)&a[3][0]);
    vb = _mm_load_ps((const float *)&b[3][0]);
    vcmp = _mm_and_ps(vcmp, _mm_cmpeq_ps(va, vb));

    int mask = _mm_movemask_ps(vcmp);
    return (mask == 0xf);
}

template <typename T>
std::array<std::array<T, 3>, 3> minvert(std::array<std::array<T, 3>, 3> const &m)
{
    //	det A	=	+ a00 a11 a22 + a10 a21 a02 + a20 a01 a12
    //				- a00 a12 a12 - a20 a11 a02 - a10 a01 a22

    auto result = m;

    double A = +m[0][0] * m[1][1] * m[2][2] + m[1][0] * m[2][1] * m[0][2] + m[2][0] * m[0][1] * m[1][2] - m[0][0] * m[1][2] * m[1][2] - m[2][0] * m[1][1] * m[0][2] + m[1][0] * m[0][1] * m[2][2];
    A = 1 / A;

    result[0][0] = (T)((m[1][1] * m[2][2] - m[1][2] * m[2][1]) * A);
    result[0][1] = (T)((m[0][2] * m[2][1] - m[0][1] * m[2][2]) * A);
    result[0][2] = (T)((m[0][1] * m[1][2] - m[0][2] * m[1][1]) * A);

    result[1][0] = (T)((m[1][2] * m[2][0] - m[1][0] * m[2][2]) * A);
    result[1][1] = (T)((m[0][0] * m[2][2] - m[0][2] * m[2][0]) * A);
    result[1][2] = (T)((m[0][2] * m[1][0] - m[0][0] * m[1][2]) * A);

    result[2][0] = (T)((m[1][0] * m[2][1] - m[1][1] * m[2][0]) * A);
    result[2][1] = (T)((m[0][1] * m[2][0] - m[0][0] * m[2][1]) * A);
    result[2][2] = (T)((m[0][0] * m[1][1] - m[0][1] * m[1][0]) * A);

    return result;
}

inline std::array<std::array<float, 4>, 4> minvert(std::array<std::array<float, 4>, 4> const &m)
{
    // From http://www.intel.com/design/pentiumiii/sml/245043.htm

    __m128 minor0, minor1, minor2, minor3;
    __m128 row0, row1, row2, row3;
    __m128 det, tmp1;

    tmp1 = _mm_loadh_pi(_mm_loadl_pi(_mm_undefined_ps(), (__m64 *)(&m[0][0])), (__m64 *)(&m[1][0]));
    row1 = _mm_loadh_pi(_mm_loadl_pi(_mm_undefined_ps(), (__m64 *)(&m[2][0])), (__m64 *)(&m[3][0]));

    row0 = _mm_shuffle_ps(tmp1, row1, 0x88);
    row1 = _mm_shuffle_ps(row1, tmp1, 0xDD);

    tmp1 = _mm_loadh_pi(_mm_loadl_pi(_mm_undefined_ps(), (__m64 *)(&m[0][2])), (__m64 *)(&m[1][2]));
    row3 = _mm_loadh_pi(_mm_loadl_pi(_mm_undefined_ps(), (__m64 *)(&m[2][2])), (__m64 *)(&m[3][2]));

    row2 = _mm_shuffle_ps(tmp1, row3, 0x88);
    row3 = _mm_shuffle_ps(row3, tmp1, 0xDD);

    // -----------------------------------------------

    tmp1 = _mm_mul_ps(row2, row3);
    tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0xB1);

    minor0 = _mm_mul_ps(row1, tmp1);
    minor1 = _mm_mul_ps(row0, tmp1);

    tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0x4E);

    minor0 = _mm_sub_ps(_mm_mul_ps(row1, tmp1), minor0);
    minor1 = _mm_sub_ps(_mm_mul_ps(row0, tmp1), minor1);
    minor1 = _mm_shuffle_ps(minor1, minor1, 0x4E);

    // -----------------------------------------------

    tmp1 = _mm_mul_ps(row1, row2);
    tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0xB1);

    minor0 = _mm_add_ps(_mm_mul_ps(row3, tmp1), minor0);
    minor3 = _mm_mul_ps(row0, tmp1);

    tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0x4E);

    minor0 = _mm_sub_ps(minor0, _mm_mul_ps(row3, tmp1));
    minor3 = _mm_sub_ps(_mm_mul_ps(row0, tmp1), minor3);
    minor3 = _mm_shuffle_ps(minor3, minor3, 0x4E);

    // -----------------------------------------------

    tmp1 = _mm_mul_ps(_mm_shuffle_ps(row1, row1, 0x4E), row3);
    tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0xB1);
    row2 = _mm_shuffle_ps(row2, row2, 0x4E);

    minor0 = _mm_add_ps(_mm_mul_ps(row2, tmp1), minor0);
    minor2 = _mm_mul_ps(row0, tmp1);

    tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0x4E);

    minor0 = _mm_sub_ps(minor0, _mm_mul_ps(row2, tmp1));
    minor2 = _mm_sub_ps(_mm_mul_ps(row0, tmp1), minor2);
    minor2 = _mm_shuffle_ps(minor2, minor2, 0x4E);

    // -----------------------------------------------

    tmp1 = _mm_mul_ps(row0, row1);

    tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0xB1);

    minor2 = _mm_add_ps(_mm_mul_ps(row3, tmp1), minor2);
    minor3 = _mm_sub_ps(_mm_mul_ps(row2, tmp1), minor3);

    tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0x4E);

    minor2 = _mm_sub_ps(_mm_mul_ps(row3, tmp1), minor2);
    minor3 = _mm_sub_ps(minor3, _mm_mul_ps(row2, tmp1));

    // -----------------------------------------------

    tmp1 = _mm_mul_ps(row0, row3);
    tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0xB1);

    minor1 = _mm_sub_ps(minor1, _mm_mul_ps(row2, tmp1));
    minor2 = _mm_add_ps(_mm_mul_ps(row1, tmp1), minor2);

    tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0x4E);

    minor1 = _mm_add_ps(_mm_mul_ps(row2, tmp1), minor1);
    minor2 = _mm_sub_ps(minor2, _mm_mul_ps(row1, tmp1));

    // -----------------------------------------------

    tmp1 = _mm_mul_ps(row0, row2);
    tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0xB1);

    minor1 = _mm_add_ps(_mm_mul_ps(row3, tmp1), minor1);
    minor3 = _mm_sub_ps(minor3, _mm_mul_ps(row1, tmp1));

    tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0x4E);

    minor1 = _mm_sub_ps(minor1, _mm_mul_ps(row3, tmp1));
    minor3 = _mm_add_ps(_mm_mul_ps(row1, tmp1), minor3);

    // -----------------------------------------------

    det = _mm_mul_ps(row0, minor0);
    det = _mm_add_ps(_mm_shuffle_ps(det, det, 0x4E), det);
    det = _mm_add_ss(_mm_shuffle_ps(det, det, 0xB1), det);
    tmp1 = _mm_rcp_ss(det);

    det = _mm_sub_ss(_mm_add_ss(tmp1, tmp1), _mm_mul_ss(det, _mm_mul_ss(tmp1, tmp1)));
    det = _mm_shuffle_ps(det, det, 0x00);

    std::array<std::array<float, 4>, 4> result;

    minor0 = _mm_mul_ps(det, minor0);
    _mm_storel_pi((__m64 *)(&result[0][0]), minor0);
    _mm_storeh_pi((__m64 *)(&result[0][2]), minor0);

    minor1 = _mm_mul_ps(det, minor1);
    _mm_storel_pi((__m64 *)(&result[1][0]), minor1);
    _mm_storeh_pi((__m64 *)(&result[1][2]), minor1);

    minor2 = _mm_mul_ps(det, minor2);
    _mm_storel_pi((__m64 *)(&result[2][0]), minor2);
    _mm_storeh_pi((__m64 *)(&result[2][2]), minor2);

    minor3 = _mm_mul_ps(det, minor3);
    _mm_storel_pi((__m64 *)(&result[3][0]), minor3);
    _mm_storeh_pi((__m64 *)(&result[3][2]), minor3);

    return result;
}

template <typename T>
std::array<std::array<T, 3>, 3> mget3x3(std::array<std::array<T, 4>, 4> const &m)
{
    std::array<std::array<T, 3>, 3> result;

    for (int r = 0; r < 3; ++r)
    {
        for (int c = 0; c < 3; ++c)
        {
            result[r][c] = m[r][c];
        }
    }

    return result;
}

template <typename T>
std::array<std::array<T, 4>, 4> minsert3x3(std::array<std::array<T, 3>, 3> const &m)
{
    auto result = M44Id<T>();

    for (int r = 0; r < 3; ++r)
    {
        for (int c = 0; c < 3; ++c)
        {
            result[r][c] = m[r][c];
        }
    }

    return result;
}

template <typename T, int N>
std::array<std::array<T, N>, N> mtranspose(std::array<std::array<T, N>, N> const &m)
{
    std::array<std::array<T, N>, N> result;

    for (int r = 0; r < N; ++r)
    {
        for (int c = 0; c < N; ++c)
        {
            result[r][c] = m[c][r];
        }
    }

    return result;
}

template <typename T, typename U>
std::array<std::array<T, 4>, 4> mmult(std::array<std::array<T, 4>, 4> const &lhs, std::array<std::array<U, 4>, 4> const &rhs)
{
    std::array<std::array<T, 4>, 4> result;
    for (int r = 0; r < 4; ++r)
    {
        for (int c = 0; c < 4; ++c)
        {
            result[r][c] = (T)(lhs[r][0] * rhs[0][c]);
            for (int k = 1; k < 4; ++k)
            {
                result[r][c] += (T)(lhs[r][k] * rhs[k][c]);
            }
        }
    }

    return result;
}

template <typename T, typename U>
std::array<T, 4> mvmult(std::array<std::array<T, 4>, 4> const &M, std::array<U, 4> const &V)
{
    std::array<T, 4> result;
    for (int r = 0; r < 4; ++r)
    {
        result[r] = (T)(M[r][0] * V[0]);
        for (int c = 1; c < 4; ++c)
        {
            result[r] += (T)(M[r][c] * V[c]);
        }
    }
    return result;
}

template <typename T>
std::array<std::array<T, 4>, 4> trans44(std::array<std::array<T, 4>, 4> const &m)
{
    std::array<std::array<T, 4>, 4> result;
    for (int i = 0; i < 4; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            result[j][i] = m[i][j];
        }
    }
    return result;
}

template <typename T>
T clamp(T val, T min, T max)
{
    return (std::min<T>(std::max<T>(val, min), max));
}

} // end namespace SWRL

#endif //SWRLIB_ALGEBRA_HPP__
