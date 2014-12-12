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
#include "pa.h"

#if (KNOB_VS_SIMD_WIDTH == 8)

bool PaTriList0(PA_STATE &pa, UINT slot, simdvector result[3]);
bool PaTriList1(PA_STATE &pa, UINT slot, simdvector result[3]);
bool PaTriList2(PA_STATE &pa, UINT slot, simdvector result[3]);
void PaTriListSingle0(PA_STATE &pa, UINT slot, UINT triIndex, __m128 tri[3]);

bool PaTriStrip0(PA_STATE &pa, UINT slot, simdvector result[3]);
bool PaTriStrip1(PA_STATE &pa, UINT slot, simdvector result[3]);
void PaTriStripSingle0(PA_STATE &pa, UINT slot, UINT triIndex, __m128 tri[3]);

bool PaTriFan0(PA_STATE &pa, UINT slot, simdvector result[3]);
bool PaTriFan1(PA_STATE &pa, UINT slot, simdvector result[3]);
void PaTriFanSingle0(PA_STATE &pa, UINT slot, UINT triIndex, __m128 tri[3]);

bool PaQuadList0(PA_STATE &pa, UINT slot, simdvector result[3]);
bool PaQuadList1(PA_STATE &pa, UINT slot, simdvector result[3]);
void PaQuadListSingle0(PA_STATE &pa, UINT slot, UINT triIndex, __m128 tri[3]);

bool PaLineList0(PA_STATE &pa, UINT slot, simdvector tri[3]);
void PaLineListSingle0(PA_STATE &pa, UINT slot, UINT triIndex, __m128 tri[3]);

bool PaLineStrip0(PA_STATE &pa, UINT slot, simdvector result[3]);
bool PaLineStrip1(PA_STATE &pa, UINT slot, simdvector result[3]);

bool PaPoints0(PA_STATE &pa, UINT slot, simdvector result[3]);
bool PaPoints1(PA_STATE &pa, UINT slot, simdvector result[3]);
void PaPointsSingle0(PA_STATE &pa, UINT slot, UINT triIndex, __m128 tri[3]);
void PaPointsSingle1(PA_STATE &pa, UINT slot, UINT triIndex, __m128 tri[3]);

bool PaHasWork(PA_STATE &pa)
{
    return (pa.numPrimsComplete < pa.numPrims) ? true : false;
}

INLINE
simdvector &PaGetSimdVector(PA_STATE &pa, UINT index, UINT slot)
{
    return pa.vout[index].vertex[slot];
}

// Assembles 4 triangles. Each simdvector is a single vertex from 4
// triangles (xxxx yyyy zzzz wwww) and there are 3 verts per triangle.
bool PaAssemble(PA_STATE &pa, UINT slot, simdvector result[3])
{
    return pa.pfnPaFunc(pa, slot, result);
}

// Assembles 1 triangle. Each simdscalar is a vertex (xyzw).
void PaAssembleSingle(PA_STATE &pa, UINT slot, UINT triIndex, __m128 tri[3])
{
    return pa.pfnPaSingleFunc(pa, slot, triIndex, tri);
}

bool PaNextPrim(PA_STATE &pa)
{
    bool morePrims = false;

    if (pa.numSimdPrims > 0)
    {
        morePrims = true;
        pa.numSimdPrims--;
    }
    else
    {
        pa.counter = (pa.reset) ? 0 : (pa.counter + 1);
        pa.reset = false;
    }

    pa.pfnPaFunc = pa.pfnPaNextFunc;

    if (!PaHasWork(pa))
    {
        morePrims = false; // no more to do
    }

    return morePrims;
}

VERTEXOUTPUT &PaGetNextVsOutput(PA_STATE &pa)
{
    // increment cur and prev indices
    pa.prev = pa.cur; // prev is undefined for first state.
    pa.cur = pa.counter % NUM_VERTEXOUTPUT;

    return pa.vout[pa.cur];
}

UINT PaNumTris(PA_STATE &pa)
{
    return (pa.numPrimsComplete > pa.numPrims) ?
               (KNOB_VS_SIMD_WIDTH - (pa.numPrimsComplete - pa.numPrims)) :
               KNOB_VS_SIMD_WIDTH;
}

INLINE
void SetNextPaState(PA_STATE &pa, PA_STATE::PFN_PA_FUNC pfnPaNextFunc, PA_STATE::PFN_PA_SINGLE_FUNC pfnPaNextSingleFunc, UINT numSimdPrims = 0)
{
    pa.pfnPaNextFunc = pfnPaNextFunc;
    pa.pfnPaSingleFunc = pfnPaNextSingleFunc;
    pa.numSimdPrims = numSimdPrims;
}

bool PaTriList0(PA_STATE &pa, UINT slot, simdvector tri[3])
{
    SetNextPaState(pa, PaTriList1, PaTriListSingle0);
    return false; // Not enough vertices to assemble 4 or 8 triangles.
}

bool PaTriList1(PA_STATE &pa, UINT slot, simdvector tri[3])
{
    SetNextPaState(pa, PaTriList2, PaTriListSingle0);
    return false; // Not enough vertices to assemble 8 triangles.
}

bool PaTriList2(PA_STATE &pa, UINT slot, simdvector tri[3])
{
    simdvector &a = PaGetSimdVector(pa, 0, slot);
    simdvector &b = PaGetSimdVector(pa, 1, slot);
    simdvector &c = PaGetSimdVector(pa, 2, slot);
    simdscalar s;

    for (int i = 0; i < 4; ++i)
    {
        simdvector &v0 = tri[0];
        v0[i] = _simd_blend_ps(a[i], b[i], 0x92);
        v0[i] = _simd_blend_ps(v0[i], c[i], 0x24);
        v0[i] = _mm256_permute_ps(v0[i], 0x6C);
        s = _mm256_permute2f128_ps(v0[i], v0[i], 0x21);
        v0[i] = _simd_blend_ps(v0[i], s, 0x44);

        simdvector &v1 = tri[1];
        v1[i] = _simd_blend_ps(a[i], b[i], 0x24);
        v1[i] = _simd_blend_ps(v1[i], c[i], 0x49);
        v1[i] = _mm256_permute_ps(v1[i], 0xB1);
        s = _mm256_permute2f128_ps(v1[i], v1[i], 0x21);
        v1[i] = _simd_blend_ps(v1[i], s, 0x66);

        simdvector &v2 = tri[2];
        v2[i] = _simd_blend_ps(a[i], b[i], 0x49);
        v2[i] = _simd_blend_ps(v2[i], c[i], 0x92);
        v2[i] = _mm256_permute_ps(v2[i], 0xC6);
        s = _mm256_permute2f128_ps(v2[i], v2[i], 0x21);
        v2[i] = _simd_blend_ps(v2[i], s, 0x22);
    }

    SetNextPaState(pa, PaTriList0, PaTriListSingle0);
    pa.reset = true;
    pa.numPrimsComplete += KNOB_VS_SIMD_WIDTH;
    return true;
}

void PaTriListSingle0(PA_STATE &pa, UINT slot, UINT triIndex, __m128 triverts[3])
{
    // We have 12 simdscalars contained within 3 simdvectors which
    // hold at least 8 triangles worth of data. We want to assemble a single
    // triangle with data in horizontal form.
    simdvector &a = PaGetSimdVector(pa, 0, slot);
    simdvector &b = PaGetSimdVector(pa, 1, slot);
    simdvector &c = PaGetSimdVector(pa, 2, slot);
    simdscalar tmp0;
    simdscalar tmp1;

    // Convert from vertical to horizontal.
    switch (triIndex)
    {
    case 0:
        // Grab vertex 0 from lane 0 and store it in tri[0]
        tmp0 = _mm256_unpacklo_ps(a.x, a.z);
        tmp1 = _mm256_unpacklo_ps(a.y, a.w);
        triverts[0] = _mm256_extractf128_ps(_mm256_unpacklo_ps(tmp0, tmp1), 0);

        // Grab vertex 1 from lane 1 and store it in tri[1]
        tmp0 = _mm256_unpacklo_ps(a.x, a.z);
        tmp1 = _mm256_unpacklo_ps(a.y, a.w);
        triverts[1] = _mm256_extractf128_ps(_mm256_unpackhi_ps(tmp0, tmp1), 0);

        // Grab vertex 2 from lane 2 and store it in tri[2]
        tmp0 = _mm256_unpackhi_ps(a.x, a.z);
        tmp1 = _mm256_unpackhi_ps(a.y, a.w);
        triverts[2] = _mm256_extractf128_ps(_mm256_unpacklo_ps(tmp0, tmp1), 0);

        break;
    case 1:
        // Grab vertex 0 from lane 3 from 'a' and store it in tri[0]
        tmp0 = _mm256_unpackhi_ps(a.x, a.z);
        tmp1 = _mm256_unpackhi_ps(a.y, a.w);
        triverts[0] = _mm256_extractf128_ps(_mm256_unpackhi_ps(tmp0, tmp1), 0);

        // Grab vertex 1 from lane 4 from 'a' and store it in tri[1]
        tmp0 = _mm256_unpacklo_ps(a.x, a.z);
        tmp1 = _mm256_unpacklo_ps(a.y, a.w);
        triverts[1] = _mm256_extractf128_ps(_mm256_unpacklo_ps(tmp0, tmp1), 1);

        // Grab vertex 2 from lane 5 from 'a' and store it in tri[2]
        tmp0 = _mm256_unpacklo_ps(a.x, a.z);
        tmp1 = _mm256_unpacklo_ps(a.y, a.w);
        triverts[2] = _mm256_extractf128_ps(_mm256_unpackhi_ps(tmp0, tmp1), 1);

        break;
    case 2:
        // Grab vertex 0 from lane 6 from 'a' and store it in tri[0]
        tmp0 = _mm256_unpackhi_ps(a.x, a.z);
        tmp1 = _mm256_unpackhi_ps(a.y, a.w);
        triverts[0] = _mm256_extractf128_ps(_mm256_unpacklo_ps(tmp0, tmp1), 1);

        // Grab vertex 1 from lane 7 from 'a' and store it in tri[1]
        tmp0 = _mm256_unpackhi_ps(a.x, a.z);
        tmp1 = _mm256_unpackhi_ps(a.y, a.w);
        triverts[1] = _mm256_extractf128_ps(_mm256_unpackhi_ps(tmp0, tmp1), 1);

        // Grab vertex 2 from lane 0 from 'b' and store it in tri[2]
        tmp0 = _mm256_unpacklo_ps(b.x, b.z);
        tmp1 = _mm256_unpacklo_ps(b.y, b.w);
        triverts[2] = _mm256_extractf128_ps(_mm256_unpacklo_ps(tmp0, tmp1), 0);

        break;
    case 3:
        // Grab vertex 0 from lane 1 from 'b' and store it in tri[0]
        tmp0 = _mm256_unpacklo_ps(b.x, b.z);
        tmp1 = _mm256_unpacklo_ps(b.y, b.w);
        triverts[0] = _mm256_extractf128_ps(_mm256_unpackhi_ps(tmp0, tmp1), 0);

        // Grab vertex 1 from lane 2 from 'b' and store it in tri[1]
        tmp0 = _mm256_unpackhi_ps(b.x, b.z);
        tmp1 = _mm256_unpackhi_ps(b.y, b.w);
        triverts[1] = _mm256_extractf128_ps(_mm256_unpacklo_ps(tmp0, tmp1), 0);

        // Grab vertex 2 from lane 3 from 'b' and store it in tri[2]
        tmp0 = _mm256_unpackhi_ps(b.x, b.z);
        tmp1 = _mm256_unpackhi_ps(b.y, b.w);
        triverts[2] = _mm256_extractf128_ps(_mm256_unpackhi_ps(tmp0, tmp1), 0);

        break;

    case 4:
        // Grab vertex 0 from lane 4 from 'b' and store it in tri[0]
        tmp0 = _mm256_unpacklo_ps(b.x, b.z);
        tmp1 = _mm256_unpacklo_ps(b.y, b.w);
        triverts[0] = _mm256_extractf128_ps(_mm256_unpacklo_ps(tmp0, tmp1), 1);

        // Grab vertex 1 from lane 5 from 'b' and store it in tri[1]
        tmp0 = _mm256_unpacklo_ps(b.x, b.z);
        tmp1 = _mm256_unpacklo_ps(b.y, b.w);
        triverts[1] = _mm256_extractf128_ps(_mm256_unpackhi_ps(tmp0, tmp1), 1);

        // Grab vertex 2 from lane 6 from 'b' and store it in tri[2]
        tmp0 = _mm256_unpackhi_ps(b.x, b.z);
        tmp1 = _mm256_unpackhi_ps(b.y, b.w);
        triverts[2] = _mm256_extractf128_ps(_mm256_unpacklo_ps(tmp0, tmp1), 1);

        break;
    case 5:
        // Grab vertex 0 from lane 7 from 'b' and store it in tri[0]
        tmp0 = _mm256_unpackhi_ps(b.x, b.z);
        tmp1 = _mm256_unpackhi_ps(b.y, b.w);
        triverts[0] = _mm256_extractf128_ps(_mm256_unpackhi_ps(tmp0, tmp1), 1);

        // Grab vertex 1 from lane 0 from 'c' and store it in tri[1]
        tmp0 = _mm256_unpacklo_ps(c.x, c.z);
        tmp1 = _mm256_unpacklo_ps(c.y, c.w);
        triverts[1] = _mm256_extractf128_ps(_mm256_unpacklo_ps(tmp0, tmp1), 0);

        // Grab vertex 2 from lane 1 from 'c' and store it in tri[2]
        tmp0 = _mm256_unpacklo_ps(c.x, c.z);
        tmp1 = _mm256_unpacklo_ps(c.y, c.w);
        triverts[2] = _mm256_extractf128_ps(_mm256_unpackhi_ps(tmp0, tmp1), 0);
        break;
    case 6:
        // Grab vertex 0 from lane 2 from 'c' and store it in tri[0]
        tmp0 = _mm256_unpackhi_ps(c.x, c.z);
        tmp1 = _mm256_unpackhi_ps(c.y, c.w);
        triverts[0] = _mm256_extractf128_ps(_mm256_unpacklo_ps(tmp0, tmp1), 0);

        // Grab vertex 1 from lane 3 from 'c' and store it in tri[1]
        tmp0 = _mm256_unpackhi_ps(c.x, c.z);
        tmp1 = _mm256_unpackhi_ps(c.y, c.w);
        triverts[1] = _mm256_extractf128_ps(_mm256_unpackhi_ps(tmp0, tmp1), 0);

        // Grab vertex 2 from lane 4 from 'c' and store it in tri[2]
        tmp0 = _mm256_unpacklo_ps(c.x, c.z);
        tmp1 = _mm256_unpacklo_ps(c.y, c.w);
        triverts[2] = _mm256_extractf128_ps(_mm256_unpacklo_ps(tmp0, tmp1), 1);

        break;
    case 7:
        // Grab vertex 0 from lane 5 from 'c' and store it in tri[0]
        tmp0 = _mm256_unpacklo_ps(c.x, c.z);
        tmp1 = _mm256_unpacklo_ps(c.y, c.w);
        triverts[0] = _mm256_extractf128_ps(_mm256_unpackhi_ps(tmp0, tmp1), 1);

        // Grab vertex 1 from lane 6 from 'c' and store it in tri[1]
        tmp0 = _mm256_unpackhi_ps(c.x, c.z);
        tmp1 = _mm256_unpackhi_ps(c.y, c.w);
        triverts[1] = _mm256_extractf128_ps(_mm256_unpacklo_ps(tmp0, tmp1), 1);

        // Grab vertex 2 from lane 7 from 'c' and store it in tri[2]
        tmp0 = _mm256_unpackhi_ps(c.x, c.z);
        tmp1 = _mm256_unpackhi_ps(c.y, c.w);
        triverts[2] = _mm256_extractf128_ps(_mm256_unpackhi_ps(tmp0, tmp1), 1);
        break;
    };
}

bool PaTriStrip0(PA_STATE &pa, UINT slot, simdvector tri[3])
{
    SetNextPaState(pa, PaTriStrip1, PaTriStripSingle0);
    return false; // Not enough vertices to assemble 8 triangles.
}

bool PaTriStrip1(PA_STATE &pa, UINT slot, simdvector tri[3])
{
    simdvector &a = PaGetSimdVector(pa, pa.prev, slot);
    simdvector &b = PaGetSimdVector(pa, pa.cur, slot);
    simdscalar s;

    for (int i = 0; i < 4; ++i)
    {
        simdscalar a0 = a[i];
        simdscalar b0 = b[i];
        /* Tri Pattern
          v0 -> 02244668
          v1 -> 11335577
          v2 -> 23456789
        */
        simdvector &v1 = tri[1];
        v1[i] = _simd_shuffle_ps(a0, a0, _MM_SHUFFLE(3, 3, 1, 1));

        simdvector &v2 = tri[2];
        s = _mm256_permute2f128_ps(a0, b0, 0x21);
        v2[i] = _simd_shuffle_ps(a0, s, _MM_SHUFFLE(1, 0, 3, 2));

        simdvector &v0 = tri[0];
        v0[i] = _simd_shuffle_ps(a0, v2[i], _MM_SHUFFLE(2, 0, 2, 0));
    }

    SetNextPaState(pa, PaTriStrip1, PaTriStripSingle0);
    pa.numPrimsComplete += KNOB_VS_SIMD_WIDTH;
    return true;
}

INLINE __m128 swizzleLane0(simdvector &a)
{
    simdscalar tmp0 = _mm256_unpacklo_ps(a.x, a.z);
    simdscalar tmp1 = _mm256_unpacklo_ps(a.y, a.w);
    return _mm256_extractf128_ps(_mm256_unpacklo_ps(tmp0, tmp1), 0);
}

INLINE __m128 swizzleLane1(simdvector &a)
{
    simdscalar tmp0 = _mm256_unpacklo_ps(a.x, a.z);
    simdscalar tmp1 = _mm256_unpacklo_ps(a.y, a.w);
    return _mm256_extractf128_ps(_mm256_unpackhi_ps(tmp0, tmp1), 0);
}

INLINE __m128 swizzleLane2(simdvector &a)
{
    simdscalar tmp0 = _mm256_unpackhi_ps(a.x, a.z);
    simdscalar tmp1 = _mm256_unpackhi_ps(a.y, a.w);
    return _mm256_extractf128_ps(_mm256_unpacklo_ps(tmp0, tmp1), 0);
}

INLINE __m128 swizzleLane3(simdvector &a)
{
    simdscalar tmp0 = _mm256_unpackhi_ps(a.x, a.z);
    simdscalar tmp1 = _mm256_unpackhi_ps(a.y, a.w);
    return _mm256_extractf128_ps(_mm256_unpackhi_ps(tmp0, tmp1), 0);
}

INLINE __m128 swizzleLane4(simdvector &a)
{
    simdscalar tmp0 = _mm256_unpacklo_ps(a.x, a.z);
    simdscalar tmp1 = _mm256_unpacklo_ps(a.y, a.w);
    return _mm256_extractf128_ps(_mm256_unpacklo_ps(tmp0, tmp1), 1);
}

INLINE __m128 swizzleLane5(simdvector &a)
{
    simdscalar tmp0 = _mm256_unpacklo_ps(a.x, a.z);
    simdscalar tmp1 = _mm256_unpacklo_ps(a.y, a.w);
    return _mm256_extractf128_ps(_mm256_unpackhi_ps(tmp0, tmp1), 1);
}

INLINE __m128 swizzleLane6(simdvector &a)
{
    simdscalar tmp0 = _mm256_unpackhi_ps(a.x, a.z);
    simdscalar tmp1 = _mm256_unpackhi_ps(a.y, a.w);
    return _mm256_extractf128_ps(_mm256_unpacklo_ps(tmp0, tmp1), 1);
}

INLINE __m128 swizzleLane7(simdvector &a)
{
    simdscalar tmp0 = _mm256_unpackhi_ps(a.x, a.z);
    simdscalar tmp1 = _mm256_unpackhi_ps(a.y, a.w);
    return _mm256_extractf128_ps(_mm256_unpackhi_ps(tmp0, tmp1), 1);
}

INLINE __m128 swizzleLaneN(simdvector &a, int lane)
{
    switch (lane)
    {
    case 0:
        return swizzleLane0(a);
    case 1:
        return swizzleLane1(a);
    case 2:
        return swizzleLane2(a);
    case 3:
        return swizzleLane3(a);
    case 4:
        return swizzleLane4(a);
    case 5:
        return swizzleLane5(a);
    case 6:
        return swizzleLane6(a);
    case 7:
        return swizzleLane7(a);
    default:
        return _mm_setzero_ps();
    }
}

void PaTriStripSingle0(PA_STATE &pa, UINT slot, UINT triIndex, __m128 triverts[3])
{
    simdvector &a = PaGetSimdVector(pa, pa.prev, slot);
    simdvector &b = PaGetSimdVector(pa, pa.cur, slot);
    simdscalar tmp0;
    simdscalar tmp1;

    // Convert from vertical to horizontal.
    switch (triIndex)
    {
    case 0:
        // Grab vertex 0 from lane 0 and store it in tri[0]
        tmp0 = _mm256_unpacklo_ps(a.x, a.z);
        tmp1 = _mm256_unpacklo_ps(a.y, a.w);
        triverts[0] = _mm256_extractf128_ps(_mm256_unpacklo_ps(tmp0, tmp1), 0);

        // Grab vertex 1 from lane 1 and store it in tri[1]
        tmp0 = _mm256_unpacklo_ps(a.x, a.z);
        tmp1 = _mm256_unpacklo_ps(a.y, a.w);
        triverts[1] = _mm256_extractf128_ps(_mm256_unpackhi_ps(tmp0, tmp1), 0);

        // Grab vertex 2 from lane 2 and store it in tri[2]
        tmp0 = _mm256_unpackhi_ps(a.x, a.z);
        tmp1 = _mm256_unpackhi_ps(a.y, a.w);
        triverts[2] = _mm256_extractf128_ps(_mm256_unpacklo_ps(tmp0, tmp1), 0);

        break;
    case 1:
        // Grab vertex 2 from lane 2 and store it in tri[2]
        tmp0 = _mm256_unpackhi_ps(a.x, a.z);
        tmp1 = _mm256_unpackhi_ps(a.y, a.w);
        triverts[0] = _mm256_extractf128_ps(_mm256_unpacklo_ps(tmp0, tmp1), 0);

        // Grab vertex 1 from lane 1 and store it in tri[1]
        tmp0 = _mm256_unpacklo_ps(a.x, a.z);
        tmp1 = _mm256_unpacklo_ps(a.y, a.w);
        triverts[1] = _mm256_extractf128_ps(_mm256_unpackhi_ps(tmp0, tmp1), 0);

        // Grab vertex 0 from lane 3 from 'a' and store it in tri[0]
        tmp0 = _mm256_unpackhi_ps(a.x, a.z);
        tmp1 = _mm256_unpackhi_ps(a.y, a.w);
        triverts[2] = _mm256_extractf128_ps(_mm256_unpackhi_ps(tmp0, tmp1), 0);
        break;
    case 2:
        // Grab vertex 2 from lane 2 and store it in tri[2]
        tmp0 = _mm256_unpackhi_ps(a.x, a.z);
        tmp1 = _mm256_unpackhi_ps(a.y, a.w);
        triverts[0] = _mm256_extractf128_ps(_mm256_unpacklo_ps(tmp0, tmp1), 0);

        // Grab vertex 0 from lane 3 from 'a' and store it in tri[0]
        tmp0 = _mm256_unpackhi_ps(a.x, a.z);
        tmp1 = _mm256_unpackhi_ps(a.y, a.w);
        triverts[1] = _mm256_extractf128_ps(_mm256_unpackhi_ps(tmp0, tmp1), 0);

        // Grab vertex 1 from lane 4 from 'a' and store it in tri[1]
        tmp0 = _mm256_unpacklo_ps(a.x, a.z);
        tmp1 = _mm256_unpacklo_ps(a.y, a.w);
        triverts[2] = _mm256_extractf128_ps(_mm256_unpacklo_ps(tmp0, tmp1), 1);
        break;
    case 3:
        // Grab vertex 1 from lane 4 from 'a' and store it in tri[1]
        tmp0 = _mm256_unpacklo_ps(a.x, a.z);
        tmp1 = _mm256_unpacklo_ps(a.y, a.w);
        triverts[0] = _mm256_extractf128_ps(_mm256_unpacklo_ps(tmp0, tmp1), 1);

        // Grab vertex 0 from lane 3 from 'a' and store it in tri[0]
        tmp0 = _mm256_unpackhi_ps(a.x, a.z);
        tmp1 = _mm256_unpackhi_ps(a.y, a.w);
        triverts[1] = _mm256_extractf128_ps(_mm256_unpackhi_ps(tmp0, tmp1), 0);

        // Grab vertex 2 from lane 5 from 'a' and store it in tri[2]
        tmp0 = _mm256_unpacklo_ps(a.x, a.z);
        tmp1 = _mm256_unpacklo_ps(a.y, a.w);
        triverts[2] = _mm256_extractf128_ps(_mm256_unpackhi_ps(tmp0, tmp1), 1);
        break;

    case 4:
        // Grab vertex 1 from lane 4 from 'a' and store it in tri[1]
        tmp0 = _mm256_unpacklo_ps(a.x, a.z);
        tmp1 = _mm256_unpacklo_ps(a.y, a.w);
        triverts[0] = _mm256_extractf128_ps(_mm256_unpacklo_ps(tmp0, tmp1), 1);

        // Grab vertex 2 from lane 5 from 'a' and store it in tri[2]
        tmp0 = _mm256_unpacklo_ps(a.x, a.z);
        tmp1 = _mm256_unpacklo_ps(a.y, a.w);
        triverts[1] = _mm256_extractf128_ps(_mm256_unpackhi_ps(tmp0, tmp1), 1);

        // Grab vertex 0 from lane 6 from 'a' and store it in tri[0]
        tmp0 = _mm256_unpackhi_ps(a.x, a.z);
        tmp1 = _mm256_unpackhi_ps(a.y, a.w);
        triverts[2] = _mm256_extractf128_ps(_mm256_unpacklo_ps(tmp0, tmp1), 1);
        break;
    case 5:
        // Grab vertex 0 from lane 6 from 'a' and store it in tri[0]
        tmp0 = _mm256_unpackhi_ps(a.x, a.z);
        tmp1 = _mm256_unpackhi_ps(a.y, a.w);
        triverts[0] = _mm256_extractf128_ps(_mm256_unpacklo_ps(tmp0, tmp1), 1);

        // Grab vertex 2 from lane 5 from 'a' and store it in tri[2]
        tmp0 = _mm256_unpacklo_ps(a.x, a.z);
        tmp1 = _mm256_unpacklo_ps(a.y, a.w);
        triverts[1] = _mm256_extractf128_ps(_mm256_unpackhi_ps(tmp0, tmp1), 1);

        // Grab vertex 1 from lane 7 from 'a' and store it in tri[1]
        tmp0 = _mm256_unpackhi_ps(a.x, a.z);
        tmp1 = _mm256_unpackhi_ps(a.y, a.w);
        triverts[2] = _mm256_extractf128_ps(_mm256_unpackhi_ps(tmp0, tmp1), 1);

        break;
    case 6:
        // Grab vertex 0 from lane 6 from 'a' and store it in tri[0]
        tmp0 = _mm256_unpackhi_ps(a.x, a.z);
        tmp1 = _mm256_unpackhi_ps(a.y, a.w);
        triverts[0] = _mm256_extractf128_ps(_mm256_unpacklo_ps(tmp0, tmp1), 1);

        // Grab vertex 1 from lane 7 from 'a' and store it in tri[1]
        tmp0 = _mm256_unpackhi_ps(a.x, a.z);
        tmp1 = _mm256_unpackhi_ps(a.y, a.w);
        triverts[1] = _mm256_extractf128_ps(_mm256_unpackhi_ps(tmp0, tmp1), 1);

        // Grab vertex 2 from lane 0 from 'b' and store it in tri[2]
        tmp0 = _mm256_unpacklo_ps(b.x, b.z);
        tmp1 = _mm256_unpacklo_ps(b.y, b.w);
        triverts[2] = _mm256_extractf128_ps(_mm256_unpacklo_ps(tmp0, tmp1), 0);
        break;
    case 7:
        // Grab vertex 2 from lane 0 from 'b' and store it in tri[2]
        tmp0 = _mm256_unpacklo_ps(b.x, b.z);
        tmp1 = _mm256_unpacklo_ps(b.y, b.w);
        triverts[0] = _mm256_extractf128_ps(_mm256_unpacklo_ps(tmp0, tmp1), 0);

        // Grab vertex 1 from lane 7 from 'a' and store it in tri[1]
        tmp0 = _mm256_unpackhi_ps(a.x, a.z);
        tmp1 = _mm256_unpackhi_ps(a.y, a.w);
        triverts[1] = _mm256_extractf128_ps(_mm256_unpackhi_ps(tmp0, tmp1), 1);

        // Grab vertex 0 from lane 1 from 'b' and store it in tri[0]
        tmp0 = _mm256_unpacklo_ps(b.x, b.z);
        tmp1 = _mm256_unpacklo_ps(b.y, b.w);
        triverts[2] = _mm256_extractf128_ps(_mm256_unpackhi_ps(tmp0, tmp1), 0);
        break;
    };
}

bool PaTriFan0(PA_STATE &pa, UINT slot, simdvector tri[3])
{
    simdvector &a = PaGetSimdVector(pa, pa.cur, slot);

    // Extract vertex 0 to every lane of first vector
    for (int i = 0; i < 4; ++i)
    {
        __m256 a0 = a[i];
        simdvector &v0 = tri[0];
        v0[i] = _simd_shuffle_ps(a0, a0, _MM_SHUFFLE(0, 0, 0, 0));
        v0[i] = _mm256_permute2f128_ps(v0[i], a0, 0x00);
    }

    // store off leading vertex for attributes
    pa.leadingVertex = pa.vout[pa.cur];

    SetNextPaState(pa, PaTriFan1, PaTriFanSingle0);
    return false; // Not enough vertices to assemble 8 triangles.
}

bool PaTriFan1(PA_STATE &pa, UINT slot, simdvector tri[3])
{
    simdvector &a = PaGetSimdVector(pa, pa.prev, slot);
    simdvector &b = PaGetSimdVector(pa, pa.cur, slot);
    simdscalar s;

    // only need to fill vectors 1/2 with new verts
    for (int i = 0; i < 4; ++i)
    {
        simdscalar a0 = a[i];
        simdscalar b0 = b[i];

        simdvector &v2 = tri[2];
        s = _mm256_permute2f128_ps(a0, b0, 0x21);
        v2[i] = _simd_shuffle_ps(a0, s, _MM_SHUFFLE(1, 0, 3, 2));

        simdvector &v1 = tri[1];
        v1[i] = _simd_shuffle_ps(a0, v2[i], _MM_SHUFFLE(2, 1, 2, 1));
    }

    SetNextPaState(pa, PaTriFan1, PaTriFanSingle0);
    pa.numPrimsComplete += KNOB_VS_SIMD_WIDTH;
    return true;
}

void PaTriFanSingle0(PA_STATE &pa, UINT slot, UINT triIndex, __m128 triverts[3])
{
    // vert 0 from leading vertex
    simdvector &lead = pa.leadingVertex.vertex[slot];
    triverts[0] = swizzleLane0(lead);

    simdvector &a = PaGetSimdVector(pa, pa.prev, slot);
    simdvector &b = PaGetSimdVector(pa, pa.cur, slot);

    // vert 1
    if (triIndex < 7)
    {
        triverts[1] = swizzleLaneN(a, triIndex + 1);
    }
    else
    {
        triverts[1] = swizzleLane0(b);
    }

    // vert 2
    if (triIndex < 6)
    {
        triverts[2] = swizzleLaneN(a, triIndex + 2);
    }
    else
    {
        triverts[2] = swizzleLaneN(b, triIndex - 6);
    }
}

bool PaQuadList0(PA_STATE &pa, UINT slot, simdvector tri[3])
{
    SetNextPaState(pa, PaQuadList1, PaQuadListSingle0);
    return false; // Not enough vertices to assemble 8 triangles.
}

bool PaQuadList1(PA_STATE &pa, UINT slot, simdvector tri[3])
{
    simdvector &a = PaGetSimdVector(pa, 0, slot);
    simdvector &b = PaGetSimdVector(pa, 1, slot);
    simdscalar s1, s2;

    for (int i = 0; i < 4; ++i)
    {
        simdscalar a0 = a[i];
        simdscalar b0 = b[i];

        s1 = _mm256_permute2f128_ps(a0, b0, 0x20);
        s2 = _mm256_permute2f128_ps(a0, b0, 0x31);

        simdvector &v0 = tri[0];
        v0[i] = _simd_shuffle_ps(s1, s2, _MM_SHUFFLE(0, 0, 0, 0));

        simdvector &v1 = tri[1];
        v1[i] = _simd_shuffle_ps(s1, s2, _MM_SHUFFLE(2, 1, 2, 1));

        simdvector &v2 = tri[2];
        v2[i] = _simd_shuffle_ps(s1, s2, _MM_SHUFFLE(3, 2, 3, 2));
    }

    SetNextPaState(pa, PaQuadList0, PaQuadListSingle0);
    pa.reset = true;
    pa.numPrimsComplete += KNOB_VS_SIMD_WIDTH;
    return true;
}

void PaQuadListSingle0(PA_STATE &pa, UINT slot, UINT triIndex, __m128 triverts[3])
{
    simdvector &a = PaGetSimdVector(pa, 0, slot);
    simdvector &b = PaGetSimdVector(pa, 1, slot);

    switch (triIndex)
    {
    case 0:
        // triangle 0 - 0 1 2
        triverts[0] = swizzleLane0(a);
        triverts[1] = swizzleLane1(a);
        triverts[2] = swizzleLane2(a);
        break;

    case 1:
        // triangle 1 - 0 2 3
        triverts[0] = swizzleLane0(a);
        triverts[1] = swizzleLane2(a);
        triverts[2] = swizzleLane3(a);
        break;

    case 2:
        // triangle 2 - 4 5 6
        triverts[0] = swizzleLane4(a);
        triverts[1] = swizzleLane5(a);
        triverts[2] = swizzleLane6(a);
        break;

    case 3:
        // triangle 3 - 4 6 7
        triverts[0] = swizzleLane4(a);
        triverts[1] = swizzleLane6(a);
        triverts[2] = swizzleLane7(a);
        break;

    case 4:
        // triangle 4 - 8 9 10 (0 1 2)
        triverts[0] = swizzleLane0(b);
        triverts[1] = swizzleLane1(b);
        triverts[2] = swizzleLane2(b);
        break;

    case 5:
        // triangle 1 - 0 2 3
        triverts[0] = swizzleLane0(b);
        triverts[1] = swizzleLane2(b);
        triverts[2] = swizzleLane3(b);
        break;

    case 6:
        // triangle 2 - 4 5 6
        triverts[0] = swizzleLane4(b);
        triverts[1] = swizzleLane5(b);
        triverts[2] = swizzleLane6(b);
        break;

    case 7:
        // triangle 3 - 4 6 7
        triverts[0] = swizzleLane4(b);
        triverts[1] = swizzleLane6(b);
        triverts[2] = swizzleLane7(b);
        break;
    }
}

void PaLineStrip0Common(PA_STATE &pa, UINT slot, simdvector tri[3])
{
    simdvector &a = PaGetSimdVector(pa, pa.cur, slot);

    for (int i = 0; i < 4; ++i)
    {
        simdscalar a0 = a[i];

        // index 0
        simdvector &v0 = tri[0];

        // 01234 -> 01122334
        __m128 vLow = _mm256_extractf128_ps(a0, 0);
        __m128 vHigh = _mm256_extractf128_ps(a0, 1);

        // 0123 -> 0112
        // 0123 -> 233x
        __m128 vOutLow = _mm_shuffle_ps(vLow, vLow, _MM_SHUFFLE(2, 1, 1, 0));
        __m128 vOutHigh = _mm_shuffle_ps(vLow, vLow, _MM_SHUFFLE(3, 3, 3, 2));

        float f;
        _MM_EXTRACT_FLOAT(f, vHigh, 0);
        vOutHigh = _mm_insert_ps(vOutHigh, _mm_set1_ps(f), 0xf0);

        v0[i] = _mm256_insertf128_ps(v0[i], vOutLow, 0);
        v0[i] = _mm256_insertf128_ps(v0[i], vOutHigh, 1);

        // index 1 is same as index 0, but position needs to be adjusted to bloat the line
        // into 2 tris 1 pixel wide
        simdvector &v1 = tri[1];
        v1[i] = v0[i];

        // index 2
        // 01234 -> 10213243
        simdvector &v2 = tri[2];

        // 0123 -> 1021
        // 0123 -> 32x3
        vOutLow = _mm_shuffle_ps(vLow, vLow, _MM_SHUFFLE(1, 2, 0, 1));
        vOutHigh = _mm_shuffle_ps(vLow, vLow, _MM_SHUFFLE(3, 3, 2, 3));
        vOutHigh = _mm_insert_ps(vOutHigh, _mm_set1_ps(f), 0xa0);

        v2[i] = _mm256_insertf128_ps(v2[i], vOutLow, 0);
        v2[i] = _mm256_insertf128_ps(v2[i], vOutHigh, 1);
    }
}

void PaLineStripSingle0(PA_STATE &pa, UINT slot, UINT triIndex, __m128 triverts[3])
{
    simdvector tri[3];
    PaLineStrip0Common(pa, slot, tri);

    __m128 vHoriz0[8], vHoriz1[8], vHoriz2[8];
    vTranspose4x8(vHoriz0, tri[0].x, tri[0].y, tri[0].z, tri[0].w);
    vTranspose4x8(vHoriz1, tri[1].x, tri[1].y, tri[1].z, tri[1].w);
    vTranspose4x8(vHoriz2, tri[2].x, tri[2].y, tri[2].z, tri[2].w);

    triverts[0] = vHoriz0[triIndex];
    triverts[1] = vHoriz1[triIndex];
    triverts[2] = vHoriz2[triIndex];
}

// 8 tris from first 4 lines cur (v0 - v4)
bool PaLineStrip0(PA_STATE &pa, UINT slot, simdvector tri[3])
{
    PaLineStrip0Common(pa, slot, tri);
    SetNextPaState(pa, PaLineStrip1, PaLineStripSingle0);
    pa.numPrimsComplete += KNOB_VS_SIMD_WIDTH;
    return true;
}

void PaLineStrip1Common(PA_STATE &pa, UINT slot, simdvector tri[3])
{
    simdvector &a = PaGetSimdVector(pa, pa.prev, slot);
    simdvector &b = PaGetSimdVector(pa, pa.cur, slot);

    for (int i = 0; i < 4; ++i)
    {
        simdscalar a0 = a[i];
        simdscalar b0 = b[i];

        // index 0
        simdvector &v0 = tri[0];

        // 45670 -> 45566770
        __m128 vPrevHigh = _mm256_extractf128_ps(a0, 1);
        __m128 vOutLow = _mm_shuffle_ps(vPrevHigh, vPrevHigh, _MM_SHUFFLE(2, 1, 1, 0));
        __m128 vOutHigh = _mm_shuffle_ps(vPrevHigh, vPrevHigh, _MM_SHUFFLE(3, 3, 3, 2));

        __m128 vCurLow = _mm256_extractf128_ps(b0, 0);
        float f;
        _MM_EXTRACT_FLOAT(f, vCurLow, 0);
        vOutHigh = _mm_insert_ps(vOutHigh, _mm_set1_ps(f), 0xf0);

        v0[i] = _mm256_insertf128_ps(v0[i], vOutLow, 0);
        v0[i] = _mm256_insertf128_ps(v0[i], vOutHigh, 1);

        // index 1
        // 45670 -> 45566770
        // index 1 same as index 0
        simdvector &v1 = tri[1];
        v1[i] = v0[i];

        // index 2
        // 45670 -> 54657607
        simdvector &v2 = tri[2];
        vOutLow = _mm_shuffle_ps(vPrevHigh, vPrevHigh, _MM_SHUFFLE(1, 2, 0, 1));
        vOutHigh = _mm_shuffle_ps(vPrevHigh, vPrevHigh, _MM_SHUFFLE(3, 3, 2, 3));
        vOutHigh = _mm_insert_ps(vOutHigh, _mm_set1_ps(f), 0xa0);

        v2[i] = _mm256_insertf128_ps(v2[i], vOutLow, 0);
        v2[i] = _mm256_insertf128_ps(v2[i], vOutHigh, 1);
    }
}

void PaLineStripSingle1(PA_STATE &pa, UINT slot, UINT triIndex, __m128 triverts[3])
{
    simdvector tri[3];
    PaLineStrip1Common(pa, slot, tri);

    __m128 vHoriz0[8], vHoriz1[8], vHoriz2[8];
    vTranspose4x8(vHoriz0, tri[0].x, tri[0].y, tri[0].z, tri[0].w);
    vTranspose4x8(vHoriz1, tri[1].x, tri[1].y, tri[1].z, tri[1].w);
    vTranspose4x8(vHoriz2, tri[2].x, tri[2].y, tri[2].z, tri[2].w);

    triverts[0] = vHoriz0[triIndex];
    triverts[1] = vHoriz1[triIndex];
    triverts[2] = vHoriz2[triIndex];
}

// 8 tris from prev v4 to cur v0
bool PaLineStrip1(PA_STATE &pa, UINT slot, simdvector tri[3])
{
    PaLineStrip1Common(pa, slot, tri);
    SetNextPaState(pa, PaLineStrip0, PaLineStripSingle1, 1);
    pa.numPrimsComplete += KNOB_VS_SIMD_WIDTH;
    return true;
}

// each line generates two tris
// input simd : p0 p1 p2 p3 p4 p5 p6 p7 == 4 lines, 8 tris
// output phase 0:
//   tri[0] : p0 p1 p2 p3 p4 p5 p6 p7
//   tri[1] : p0 p1 p2 p3 p4 p5 p6 p7
//   tri[2] : p1 p0 p3 p2 p5 p4 p7 p6

bool PaLineList0(PA_STATE &pa, UINT slot, simdvector tri[3])
{
    simdvector &a = PaGetSimdVector(pa, pa.cur, slot);
    for (UINT i = 0; i < 4; ++i)
    {
        tri[0].v[i] = tri[1].v[i] = a.v[i];
        tri[2].v[i] = _mm256_shuffle_ps(a.v[i], a.v[i], _MM_SHUFFLE(2, 3, 0, 1));
    }
    SetNextPaState(pa, PaLineList0, PaLineListSingle0);
    pa.numPrimsComplete += KNOB_VS_SIMD_WIDTH;
    return true;
}

void PaLineListSingle0(PA_STATE &pa, UINT slot, UINT triIndex, __m128 triverts[3])
{
    simdvector &a = PaGetSimdVector(pa, pa.cur, slot);
    switch (triIndex)
    {
    case 0:
        triverts[0] = swizzleLane0(a);
        triverts[1] = swizzleLane0(a);
        triverts[2] = swizzleLane1(a);
        break;
    case 1:
        triverts[0] = swizzleLane1(a);
        triverts[1] = swizzleLane1(a);
        triverts[2] = swizzleLane0(a);
        break;
    case 2:
        triverts[0] = swizzleLane2(a);
        triverts[1] = swizzleLane2(a);
        triverts[2] = swizzleLane3(a);
        break;
    case 3:
        triverts[0] = swizzleLane3(a);
        triverts[1] = swizzleLane3(a);
        triverts[2] = swizzleLane2(a);
        break;
    case 4:
        triverts[0] = swizzleLane4(a);
        triverts[1] = swizzleLane4(a);
        triverts[2] = swizzleLane5(a);
        break;
    case 5:
        triverts[0] = swizzleLane5(a);
        triverts[1] = swizzleLane5(a);
        triverts[2] = swizzleLane4(a);
        break;
    case 6:
        triverts[0] = swizzleLane6(a);
        triverts[1] = swizzleLane6(a);
        triverts[2] = swizzleLane7(a);
        break;
    case 7:
        triverts[0] = swizzleLane7(a);
        triverts[1] = swizzleLane7(a);
        triverts[2] = swizzleLane6(a);
        break;
    }
}

// each point generates two tris
// primitive assembly broadcasts each point to the 3 vertices of the 2 tris
// binner will bloat each point
//
// input simd : p0 p1 p2 p3 p4 p5 p6 p7 == 8 points, 16 tris
// output phase 0:
//   tri[0] : p0 p0 p1 p1 p2 p2 p3 p3
//   tri[1] : p0 p0 p1 p1 p2 p2 p3 p3
//   tri[2] : p0 p0 p1 p1 p2 p2 p3 p3
//
// output phase 1:
//   tri[0] : p4 p4 p5 p5 p6 p6 p7 p7
//   tri[1] : p4 p4 p5 p5 p6 p6 p7 p7
//   tri[2] : p4 p4 p5 p5 p6 p6 p7 p7

// 0 1 2 3 4 5 6 7

bool PaPoints0(PA_STATE &pa, UINT slot, simdvector tri[3])
{
    simdvector &a = PaGetSimdVector(pa, pa.cur, slot);

    for (UINT i = 0; i < 4; ++i)
    {
        __m256 vLow128 = _mm256_unpacklo_ps(a.v[i], a.v[i]);                // 0 0 1 1 4 4 5 5
        __m256 vHigh128 = _mm256_unpackhi_ps(a.v[i], a.v[i]);               // 2 2 3 3 6 6 7 7
        __m256 vCombined = _mm256_permute2f128_ps(vLow128, vHigh128, 0x20); // 0 0 1 1 2 2 3 3

        tri[0].v[i] = tri[1].v[i] = tri[2].v[i] = vCombined;
    }

    SetNextPaState(pa, PaPoints1, PaPointsSingle0, 1);
    pa.numPrimsComplete += KNOB_VS_SIMD_WIDTH;
    return true;
}

bool PaPoints1(PA_STATE &pa, UINT slot, simdvector tri[3])
{
    simdvector &a = PaGetSimdVector(pa, pa.cur, slot);

    for (UINT i = 0; i < 4; ++i)
    {
        __m256 vLow128 = _mm256_unpacklo_ps(a.v[i], a.v[i]);                // 0 0 1 1 4 4 5 5
        __m256 vHigh128 = _mm256_unpackhi_ps(a.v[i], a.v[i]);               // 2 2 3 3 6 6 7 7
        __m256 vCombined = _mm256_permute2f128_ps(vLow128, vHigh128, 0x31); // 4 4 5 5 6 6 7 7

        tri[0].v[i] = tri[1].v[i] = tri[2].v[i] = vCombined;
    }

    SetNextPaState(pa, PaPoints0, PaPointsSingle1, 0);
    pa.numPrimsComplete += KNOB_VS_SIMD_WIDTH;
    return true;
}

void PaPointsSingle0(PA_STATE &pa, UINT slot, UINT triIndex, __m128 triverts[3])
{

    simdvector &a = PaGetSimdVector(pa, pa.cur, slot);

    switch (triIndex)
    {
    case 0:
    case 1:
        triverts[0] = triverts[1] = triverts[2] = swizzleLane0(a);
        break;
    case 2:
    case 3:
        triverts[0] = triverts[1] = triverts[2] = swizzleLane1(a);
        break;
    case 4:
    case 5:
        triverts[0] = triverts[1] = triverts[2] = swizzleLane2(a);
        break;
    case 6:
    case 7:
        triverts[0] = triverts[1] = triverts[2] = swizzleLane3(a);
        break;
    }
}

void PaPointsSingle1(PA_STATE &pa, UINT slot, UINT triIndex, __m128 triverts[3])
{

    simdvector &a = PaGetSimdVector(pa, pa.cur, slot);

    switch (triIndex)
    {
    case 0:
    case 1:
        triverts[0] = triverts[1] = triverts[2] = swizzleLane4(a);
        break;
    case 2:
    case 3:
        triverts[0] = triverts[1] = triverts[2] = swizzleLane5(a);
        break;
    case 4:
    case 5:
        triverts[0] = triverts[1] = triverts[2] = swizzleLane6(a);
        break;
    case 6:
    case 7:
        triverts[0] = triverts[1] = triverts[2] = swizzleLane7(a);
        break;
    }
}

PA_STATE::PA_STATE(DRAW_CONTEXT *in_pDC, UINT in_numPrims)
    : pDC(in_pDC), numPrims(in_numPrims), numPrimsComplete(0), numSimdPrims(0), cur(0), prev(0), first(0), counter(0), reset(false), pfnPaFunc(NULL)
{
    switch (pDC->state.topology)
    {
    case TOP_TRIANGLE_LIST:
        this->pfnPaFunc = PaTriList0;
        break;
    case TOP_TRIANGLE_STRIP:
        this->pfnPaFunc = PaTriStrip0;
        break;
    case TOP_TRIANGLE_FAN:
        this->pfnPaFunc = PaTriFan0;
        break;
    case TOP_QUAD_LIST:
        this->pfnPaFunc = PaQuadList0;
        this->numPrims = in_numPrims * 2; // Convert quad primitives into triangles
        break;
    case TOP_QUAD_STRIP:
        // quad strip pattern when decomposed into triangles is the same as tri strips
        this->pfnPaFunc = PaTriStrip0;
        this->numPrims = in_numPrims * 2; // Convert quad primitives into triangles
        break;
    case TOP_LINE_LIST:
        this->pfnPaFunc = PaLineList0;
        this->numPrims = in_numPrims * 2;
        break;
    case TOP_LINE_STRIP:
        this->pfnPaFunc = PaLineStrip0;
        this->numPrims = in_numPrims * 2; // 1 line generates 2 tris
        break;
    case TOP_POINTS:
        this->pfnPaFunc = PaPoints0;
        this->numPrims = in_numPrims * 2; // 1 point generates 2 tris
        break;
    default:
        assert(0);
        break;
    };
}
#endif
