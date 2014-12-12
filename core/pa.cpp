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

#if (KNOB_VS_SIMD_WIDTH == 4)
INLINE simdscalar _simd_shuffle_0321(const simdscalar &a)
{
    return _mm_shuffle_ps(a, a, _MM_SHUFFLE(1, 2, 3, 0));
}
INLINE simdscalar _simd_shuffle_1032(const simdscalar &a)
{
    return _mm_shuffle_ps(a, a, _MM_SHUFFLE(2, 3, 0, 1));
}
INLINE simdscalar _simd_shuffle_2103(const simdscalar &a)
{
    return _mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 0, 1, 2));
}

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

bool PaLineList0(PA_STATE &pa, UINT slot, simdvector result[3]);
void PaLineListSingle0(PA_STATE &pa, UINT slot, UINT triIndex, __m128 tri[3]);

bool PaLineStrip0(PA_STATE &pa, UINT slot, simdvector result[3]);
bool PaLineStrip1(PA_STATE &pa, UINT slot, simdvector result[3]);
void PaLineStripSingle0(PA_STATE &pa, UINT slot, UINT triIndex, __m128 tri[3]);
void PaLineStripSingle1(PA_STATE &pa, UINT slot, UINT triIndex, __m128 tri[3]);

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
void PaAssembleSingle(PA_STATE &pa, UINT slot, UINT triIndex, simdscalar tri[3])
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
               (4 - (pa.numPrimsComplete - pa.numPrims)) :
               4;
}

INLINE
void SetNextPaState(PA_STATE &pa, PA_STATE::PFN_PA_FUNC pfnPaNextFunc, PA_STATE::PFN_PA_SINGLE_FUNC pfnPaNextSingleFunc, UINT numSimdPrims = 0)
{
    pa.pfnPaNextFunc = pfnPaNextFunc;
    pa.pfnPaSingleFunc = pfnPaNextSingleFunc;
    pa.numSimdPrims = numSimdPrims;
}

bool PaTriStrip0(PA_STATE &pa, UINT slot, simdvector tri[3])
{
    SetNextPaState(pa, PaTriStrip1, PaTriStripSingle0);
    return false; // Not enough vertices to assemble 4 or 8 triangles.
}

bool PaTriStrip1(PA_STATE &pa, UINT slot, simdvector tri[3])
{
    simdvector &a = PaGetSimdVector(pa, pa.prev, slot);
    simdvector &b = PaGetSimdVector(pa, pa.cur, slot);

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
        v2[i] = _simd_shuffle_ps(a0, b0, _MM_SHUFFLE(1, 0, 3, 2));

        simdvector &v0 = tri[0];
        v0[i] = _simd_shuffle_ps(a0, v2[i], _MM_SHUFFLE(2, 0, 2, 0));
    }

    SetNextPaState(pa, PaTriStrip1, PaTriStripSingle0);
    pa.numPrimsComplete += 4;
    return true;
}

void PaTriStripSingle0(PA_STATE &pa, UINT slot, UINT triIndex, __m128 triverts[3])
{
    // We have 12 simdscalars contained within 3 simdvectors which
    // hold at least 4 triangles worth of data. We want to assemble a single
    // triangle with data in horizontal form.
    simdvector &a = PaGetSimdVector(pa, pa.prev, slot);
    simdvector &b = PaGetSimdVector(pa, pa.cur, slot);
    simdscalar tmp0;
    simdscalar tmp1;

    // Convert from vertical to horizontal.
    switch (triIndex)
    {
    case 0:
        // 0, 1, 2
        tmp0 = _mm_unpacklo_ps(a.x, a.y); // r0 g0 r1 g1
        tmp1 = _mm_unpacklo_ps(a.z, a.w); // b0 a0 b1 a1

        triverts[0] = _mm_movelh_ps(tmp0, tmp1); // r0 g0 b0 a0
        triverts[1] = _mm_movehl_ps(tmp1, tmp0); // r1 g1 b1 a1

        tmp0 = _mm_unpackhi_ps(a.x, a.y); // r2 g2 r3 g3
        tmp1 = _mm_unpackhi_ps(a.z, a.w); // b2 a2 b3 a3

        triverts[2] = _mm_movelh_ps(tmp0, tmp1); // r2 g2 b2 a2
        break;
    case 1:
        // 2, 1, 3
        tmp0 = _mm_unpacklo_ps(a.x, a.y); // r0 g0 r1 g1
        tmp1 = _mm_unpacklo_ps(a.z, a.w); // b0 a0 b1 a1

        triverts[1] = _mm_movehl_ps(tmp1, tmp0); // r1 g1 b1 a1

        tmp0 = _mm_unpackhi_ps(a.x, a.y); // r2 g2 r3 g3
        tmp1 = _mm_unpackhi_ps(a.z, a.w); // b2 a2 b3 a3

        triverts[0] = _mm_movelh_ps(tmp0, tmp1); // r2 g2 b2 a2
        triverts[2] = _mm_movehl_ps(tmp1, tmp0); // r3 g3 b3 a3
        break;
    case 2:
        // 2, 3, 4
        tmp0 = _mm_unpackhi_ps(a.x, a.y); // r2 g2 r3 g3
        tmp1 = _mm_unpackhi_ps(a.z, a.w); // b2 a2 b3 a3

        triverts[0] = _mm_movelh_ps(tmp0, tmp1); // r2 g2 b2 a2
        triverts[1] = _mm_movehl_ps(tmp1, tmp0); // r3 g3 b3 a3

        tmp0 = _mm_unpacklo_ps(b.x, b.y); // r4 g4 r5 g5
        tmp1 = _mm_unpacklo_ps(b.z, b.w); // b4 a4 b5 a5

        triverts[2] = _mm_movelh_ps(tmp0, tmp1); // r4 g4 b4 a4
        break;
    case 3:
        // 4, 3, 5
        tmp0 = _mm_unpackhi_ps(a.x, a.y); // r2 g2 r3 g3
        tmp1 = _mm_unpackhi_ps(a.z, a.w); // b2 a2 b3 a3

        triverts[1] = _mm_movehl_ps(tmp1, tmp0); // r3 g3 b3 a3

        tmp0 = _mm_unpacklo_ps(b.x, b.y); // r4 g4 r5 g5
        tmp1 = _mm_unpacklo_ps(b.z, b.w); // b4 a4 b5 a5

        triverts[0] = _mm_movelh_ps(tmp0, tmp1); // r4 g4 b4 a4
        triverts[2] = _mm_movehl_ps(tmp1, tmp0); // r5 g5 b5 a5
        break;
    };
}

bool PaTriFan0(PA_STATE &pa, UINT slot, simdvector tri[3])
{
    simdvector &a = PaGetSimdVector(pa, pa.cur, slot);

    // Extract vertex 0 to every lane of first vector
    for (int i = 0; i < 4; ++i)
    {
        simdscalar a0 = a[i];
        simdvector &v0 = tri[0];
        v0[i] = _simd_shuffle_ps(a[i], a[i], _MM_SHUFFLE(0, 0, 0, 0));
    }
    SetNextPaState(pa, PaTriFan1, PaTriFanSingle0);
    return false; // Not enough vertices to assemble 8 triangles.
}

bool PaTriFan1(PA_STATE &pa, UINT slot, simdvector tri[3])
{
    simdvector &a = PaGetSimdVector(pa, pa.prev, slot);
    simdvector &b = PaGetSimdVector(pa, pa.cur, slot);

    // only need to fill vectors 1/2 with new verts
    for (int i = 0; i < 4; ++i)
    {
        simdscalar a0 = a[i];
        simdscalar b0 = b[i];

        simdvector &v2 = tri[2];
        v2[i] = _simd_shuffle_ps(a0, b0, _MM_SHUFFLE(1, 0, 3, 2));

        simdvector &v1 = tri[1];
        v1[i] = _simd_shuffle_ps(a0, v2[i], _MM_SHUFFLE(2, 1, 2, 1));
    }

    SetNextPaState(pa, PaTriFan1, PaTriFanSingle0);
    pa.numPrimsComplete += KNOB_VS_SIMD_WIDTH;
    return true;
}

void PaTriFanSingle0(PA_STATE &pa, UINT slot, UINT triIndex, __m128 triverts[3])
{
}

bool PaQuadList0(PA_STATE &pa, UINT slot, simdvector tri[3])
{
    SetNextPaState(pa, PaQuadList1, PaQuadListSingle0);
    return false; // Not enough vertices to assemble 4 or 8 triangles.
}

bool PaQuadList1(PA_STATE &pa, UINT slot, simdvector tri[3])
{
    simdvector &a = PaGetSimdVector(pa, 0, slot);
    simdvector &b = PaGetSimdVector(pa, 1, slot);

    for (int i = 0; i < 4; ++i)
    {
        simdvector &v0 = tri[0];
        v0[i] = _mm_shuffle_ps(a[i], b[i], _MM_SHUFFLE(0, 0, 0, 0));

        simdvector &v1 = tri[1];
        v1[i] = _mm_shuffle_ps(a[i], b[i], _MM_SHUFFLE(2, 1, 2, 1));

        simdvector &v2 = tri[2];
        v2[i] = _mm_shuffle_ps(a[i], b[i], _MM_SHUFFLE(3, 2, 3, 2));
    }

    SetNextPaState(pa, PaQuadList0, PaQuadListSingle0);
    pa.reset = true;
    pa.numPrimsComplete += 4;
    return true;
}

void PaQuadListSingle0(PA_STATE &pa, UINT slot, UINT triIndex, __m128 triverts[3])
{
    // We have 12 simdscalars contained within 3 simdvectors which
    // hold at least 4 triangles worth of data. We want to assemble a single
    // triangle with data in horizontal form.
    simdvector &a = PaGetSimdVector(pa, 0, slot);
    simdvector &b = PaGetSimdVector(pa, 1, slot);
    simdscalar tmp0;
    simdscalar tmp1;

    // Convert from vertical to horizontal.
    switch (triIndex)
    {
    case 0:
        // Grab vertex 0 from lane 0 and store it in tri[0]
        tmp0 = _mm_unpacklo_ps(a.x, a.y);
        tmp1 = _mm_unpacklo_ps(a.z, a.w);
        triverts[0] = _mm_movelh_ps(tmp0, tmp1);

        // Grab vertex 1 from lane 1 and store it in tri[1]
        triverts[1] = _mm_movehl_ps(tmp1, tmp0);

        // Grab vertex 2 from lane 2 and store it in tri[2]
        tmp0 = _mm_unpackhi_ps(a.x, a.y);
        tmp1 = _mm_unpackhi_ps(a.z, a.w);
        triverts[2] = _mm_movelh_ps(tmp0, tmp1);
        break;
    case 1:
        // Grab vertex 0 from lane 0 and store it in tri[0]
        tmp0 = _mm_unpacklo_ps(a.x, a.y);
        tmp1 = _mm_unpacklo_ps(a.z, a.w);
        triverts[0] = _mm_movelh_ps(tmp0, tmp1);

        // Grab vertex 1 from lane 2 and store it in tri[1]
        tmp0 = _mm_unpackhi_ps(a.x, a.y);
        tmp1 = _mm_unpackhi_ps(a.z, a.w);
        triverts[1] = _mm_movelh_ps(tmp0, tmp1);

        // Grab vertex 2 from lane 3 and store it in tri[2]
        triverts[2] = _mm_movehl_ps(tmp1, tmp0);
        break;
    case 2:
        // Grab vertex 0 from lane 0 and store it in tri[0]
        tmp0 = _mm_unpacklo_ps(b.x, b.y);
        tmp1 = _mm_unpacklo_ps(b.z, b.w);
        triverts[0] = _mm_movelh_ps(tmp0, tmp1);

        // Grab vertex 1 from lane 1 and store it in tri[1]
        triverts[1] = _mm_movehl_ps(tmp1, tmp0);

        // Grab vertex 2 from lane 2 and store it in tri[2]
        tmp0 = _mm_unpackhi_ps(b.x, b.y);
        tmp1 = _mm_unpackhi_ps(b.z, b.w);
        triverts[2] = _mm_movelh_ps(tmp0, tmp1);
        break;
    case 3:
        // Grab vertex 0 from lane 0 and store it in tri[0]
        tmp0 = _mm_unpacklo_ps(b.x, b.y);
        tmp1 = _mm_unpacklo_ps(b.z, b.w);
        triverts[0] = _mm_movelh_ps(tmp0, tmp1);

        // Grab vertex 1 from lane 2 and store it in tri[1]
        tmp0 = _mm_unpackhi_ps(b.x, b.y);
        tmp1 = _mm_unpackhi_ps(b.z, b.w);
        triverts[1] = _mm_movelh_ps(tmp0, tmp1);

        // Grab vertex 2 from lane 3 and store it in tri[2]
        triverts[2] = _mm_movehl_ps(tmp1, tmp0);
        break;
    };
}

bool PaTriList0(PA_STATE &pa, UINT slot, simdvector tri[3])
{
    SetNextPaState(pa, PaTriList1, PaTriListSingle0);
    return false; // Not enough vertices to assemble 4 or 8 triangles.
}

bool PaTriList1(PA_STATE &pa, UINT slot, simdvector tri[3])
{
    SetNextPaState(pa, PaTriList2, PaTriListSingle0);
    return false; // Not enough vertices to assemble 4 or 8 triangles.
}

bool PaTriList2(PA_STATE &pa, UINT slot, simdvector tri[3])
{
    simdvector &a = PaGetSimdVector(pa, 0, slot);
    simdvector &b = PaGetSimdVector(pa, 1, slot);
    simdvector &c = PaGetSimdVector(pa, 2, slot);

    for (int i = 0; i < 4; ++i)
    {
        simdvector &v0 = tri[0];
        v0[i] = _simd_blend_ps(a[i], b[i], 0x4);
        v0[i] = _simd_blend_ps(v0[i], c[i], 0x2);
        v0[i] = _simd_shuffle_0321(v0[i]);

        simdvector &v1 = tri[1];
        v1[i] = _simd_blend_ps(a[i], b[i], 0x9);
        v1[i] = _simd_blend_ps(v1[i], c[i], 0x4);
        v1[i] = _simd_shuffle_1032(v1[i]);

        simdvector &v2 = tri[2];
        v2[i] = _simd_blend_ps(a[i], b[i], 0x2);
        v2[i] = _simd_blend_ps(v2[i], c[i], 0x9);
        v2[i] = _simd_shuffle_2103(v2[i]);
    }

    SetNextPaState(pa, PaTriList0, PaTriListSingle0);
    pa.reset = true;
    pa.numPrimsComplete += 4;
    return true;
}

void PaTriListSingle0(PA_STATE &pa, UINT slot, UINT triIndex, __m128 triverts[3])
{
    // We have 12 simdscalars contained within 3 simdvectors which
    // hold at least 4 triangles worth of data. We want to assemble a single
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
        tmp0 = _mm_unpacklo_ps(a.x, a.y);
        tmp1 = _mm_unpacklo_ps(a.z, a.w);
        triverts[0] = _mm_movelh_ps(tmp0, tmp1);

        // Grab vertex 1 from lane 1 and store it in tri[1]
        triverts[1] = _mm_movehl_ps(tmp1, tmp0);

        // Grab vertex 2 from lane 2 and store it in tri[2]
        tmp0 = _mm_unpackhi_ps(a.x, a.y);
        tmp1 = _mm_unpackhi_ps(a.z, a.w);
        triverts[2] = _mm_movelh_ps(tmp0, tmp1);
        break;
    case 1:
        // Grab vertex 0 from lane 3 from 'a' and store it in tri[0]
        tmp0 = _mm_unpackhi_ps(a.x, a.y);
        tmp1 = _mm_unpackhi_ps(a.z, a.w);
        triverts[0] = _mm_movehl_ps(tmp1, tmp0);

        // Grab vertex 1 from lane 0 from 'b' and store it in tri[1]
        tmp0 = _mm_unpacklo_ps(b.x, b.y);
        tmp1 = _mm_unpacklo_ps(b.z, b.w);
        triverts[1] = _mm_movelh_ps(tmp0, tmp1);

        // Grab vertex 2 from lane 1 from 'b' and store it in tri[1]
        triverts[2] = _mm_movehl_ps(tmp1, tmp0);
        break;
    case 2:
        // Grab vertex 0 from lane 2 from 'b' and store it in tri[0]n
        tmp0 = _mm_unpackhi_ps(b.x, b.y);
        tmp1 = _mm_unpackhi_ps(b.z, b.w);
        triverts[0] = _mm_movelh_ps(tmp0, tmp1);

        // Grab vertex 1 from lane 3 from 'b' and store it in tri[1]
        triverts[1] = _mm_movehl_ps(tmp1, tmp0);

        // Grab vertex 2 from lane 0 from 'c' and store it in tri[0]
        tmp0 = _mm_unpacklo_ps(c.x, c.y);
        tmp1 = _mm_unpacklo_ps(c.z, c.w);
        triverts[2] = _mm_movelh_ps(tmp0, tmp1);

        break;
    case 3:
        // Grab vertex 0 from lane 1 from 'c' and store it in tri[0]
        tmp0 = _mm_unpacklo_ps(c.x, c.y);
        tmp1 = _mm_unpacklo_ps(c.z, c.w);
        triverts[0] = _mm_movehl_ps(tmp1, tmp0);

        // Grab vertex 1 from lane 2 from 'c' and store it in tri[1]
        tmp0 = _mm_unpackhi_ps(c.x, c.y);
        tmp1 = _mm_unpackhi_ps(c.z, c.w);
        triverts[1] = _mm_movelh_ps(tmp0, tmp1);

        // Grab vertex 2 from lane 3 from 'c' and store it in tri[2]
        triverts[2] = _mm_movehl_ps(tmp1, tmp0);
        break;
    };
}

bool PaLineStrip0(PA_STATE &pa, UINT slot, simdvector tri[3])
{
    // assemble tris from lines
    //
    // lines		  tri sets						SIMD Vectors
    // -----------	  -------------------------		------------
    // a(v0) a(v1) -> [ v0 v0 v1 ] [ v1 v1 v0 ]	->	v0v1v1v2
    // a(v1) a(v2) -> [ v1 v1 v2 ] [ v2 v2 v1 ]		v0v1v1v2
    //												v1v0v2v1

    simdvector &a = PaGetSimdVector(pa, pa.cur, slot);

    for (int i = 0; i < 4; ++i)
    {
        simdvector &v0 = tri[0];
        v0[i] = _mm_shuffle_ps(a[i], a[i], _MM_SHUFFLE(2, 1, 1, 0));
        simdvector &v1 = tri[1];
        v1[i] = _mm_shuffle_ps(a[i], a[i], _MM_SHUFFLE(2, 1, 1, 0));
        simdvector &v2 = tri[2];
        v2[i] = _mm_shuffle_ps(a[i], a[i], _MM_SHUFFLE(1, 2, 0, 1));
    }
    pa.numPrimsComplete += 4;

    SetNextPaState(pa, PaLineStrip1, PaLineStripSingle0);
    return true;
}

bool PaLineStrip1(PA_STATE &pa, UINT slot, simdvector tri[3])
{
    // assemble tris from lines
    //
    // lines		  tri sets						SIMD Vectors
    // -----------	  -------------------------		------------
    // a(v2) a(v3) -> [ v2 v2 v3 ] [ v3 v3 v2 ]	->	v2v3v3v0
    // a(v3) b(v0) -> [ v3 v3 v0 ] [ v0 v0 v3 ]		v2v3v3v0
    //												v3v2v0v3
    // then back to PaLineStrip0(...)

    simdvector &a = PaGetSimdVector(pa, pa.prev, slot); // get previous simd vec (still unfinished)
    simdvector &b = PaGetSimdVector(pa, pa.cur, slot);  // get current simd vec

    for (int i = 0; i < 4; ++i)
    {
        simdvector &v0 = tri[0];
        v0[i] = _mm_shuffle_ps(a[i], a[i], _MM_SHUFFLE(0, 3, 3, 2));
        simdvector &v1 = tri[1];
        v1[i] = _mm_shuffle_ps(a[i], a[i], _MM_SHUFFLE(0, 3, 3, 2));
        simdvector &v2 = tri[2];
        v2[i] = _mm_shuffle_ps(b[i], b[i], _MM_SHUFFLE(3, 0, 2, 3)); // could use a _mm_load1_ps?
        v2[i] = _mm_blend_ps(v2[i], a[i], 0x1);
    }
    pa.numPrimsComplete += 4;

    SetNextPaState(pa, PaLineStrip0, PaLineStripSingle1);
    return true;
}

void PaLineStripSingle0(PA_STATE &pa, UINT slot, UINT triIndex, __m128 triverts[3])
{
    // We have 12 simdscalars contained within 3 simdvectors which
    // hold at least 4 triangles worth of data (from 2 lines strips). We want to
    // assemble a single triangle with data in horizontal form.
    simdvector &a = PaGetSimdVector(pa, pa.prev, slot);
    simdscalar tmp0;
    simdscalar tmp1;

    // Convert from vertical to horizontal.
    switch (triIndex)
    {
    case 0:
        // 0, 0, 1
        tmp0 = _mm_unpacklo_ps(a.x, a.y); // r0 g0 r1 g1
        tmp1 = _mm_unpacklo_ps(a.z, a.w); // b0 a0 b1 a1

        triverts[0] = _mm_movelh_ps(tmp0, tmp1); // r0 g0 b0 a0
        triverts[1] = triverts[0];
        triverts[2] = _mm_movehl_ps(tmp1, tmp0); // r1 g1 b1 a1
        break;
    case 1:
        // 1, 1, 0
        tmp0 = _mm_unpacklo_ps(a.x, a.y); // r0 g0 r1 g1
        tmp1 = _mm_unpacklo_ps(a.z, a.w); // b0 a0 b1 a1

        triverts[0] = _mm_movehl_ps(tmp1, tmp0); // r1 g1 b1 a1
        triverts[1] = triverts[0];
        triverts[2] = _mm_movelh_ps(tmp0, tmp1); // r0 g0 b0 a0
        break;
    case 2:
        // 1, 1, 2
        tmp0 = _mm_unpacklo_ps(a.x, a.y); // r0 g0 r1 g1
        tmp1 = _mm_unpacklo_ps(a.z, a.w); // b0 a0 b1 a1

        triverts[0] = _mm_movehl_ps(tmp1, tmp0); // r1 g1 b1 a1
        triverts[1] = triverts[0];

        tmp0 = _mm_unpackhi_ps(a.x, a.y); // r2 g2 r3 g3
        tmp1 = _mm_unpackhi_ps(a.z, a.w); // b2 a2 b3 a3

        triverts[2] = _mm_movelh_ps(tmp0, tmp1); // r2 g2 b2 a2
        break;
    case 3:
        // 2, 2, 1
        tmp0 = _mm_unpackhi_ps(a.x, a.y); // r2 g2 r3 g3
        tmp1 = _mm_unpackhi_ps(a.z, a.w); // b2 a2 b3 a3

        triverts[0] = _mm_movelh_ps(tmp0, tmp1); // r2 g3 b2 a2
        triverts[1] = triverts[0];

        tmp0 = _mm_unpacklo_ps(a.x, a.y); // r0 g0 r1 g1
        tmp1 = _mm_unpacklo_ps(a.z, a.w); // b0 a0 b1 a1

        triverts[2] = _mm_movehl_ps(tmp1, tmp0); // r1 g1 b1 a1
        break;
    };
}

void PaLineStripSingle1(PA_STATE &pa, UINT slot, UINT triIndex, __m128 triverts[3])
{
    // We have 12 simdscalars contained within 3 simdvectors which
    // hold at least 4 triangles worth of data (from 2 lines strips). We want to
    // assemble a single triangle with data in horizontal form.
    simdvector &a = PaGetSimdVector(pa, pa.prev, slot);
    simdvector &b = PaGetSimdVector(pa, pa.cur, slot);
    simdscalar tmp0;
    simdscalar tmp1;

    // Convert from vertical to horizontal.
    switch (triIndex)
    {
    case 0:
        // 2, 2, 3
        tmp0 = _mm_unpackhi_ps(a.x, a.y); // r2 g2 r3 g3
        tmp1 = _mm_unpackhi_ps(a.z, a.w); // b2 a2 b3 a3

        triverts[0] = _mm_movelh_ps(tmp0, tmp1); // r2 g2 b2 a2
        triverts[1] = triverts[0];
        triverts[2] = _mm_movehl_ps(tmp1, tmp0); // r3 g3 b3 a3
        break;
    case 1:
        // 3, 3, 2
        tmp0 = _mm_unpackhi_ps(a.x, a.y); // r2 g2 r3 g3
        tmp1 = _mm_unpackhi_ps(a.z, a.w); // b2 a2 b3 a3

        triverts[0] = _mm_movehl_ps(tmp1, tmp0); // r3 g3 b3 a3
        triverts[1] = triverts[0];
        triverts[2] = _mm_movelh_ps(tmp0, tmp1); // r2 g2 b2 a2
        break;
    case 2:
        // 3, 3, 0
        tmp0 = _mm_unpackhi_ps(a.x, a.y); // r2 g2 r3 g3
        tmp1 = _mm_unpackhi_ps(a.z, a.w); // b2 a2 b3 a3

        triverts[0] = _mm_movehl_ps(tmp1, tmp0); // r3 g3 b3 a3
        triverts[1] = triverts[0];

        tmp0 = _mm_unpacklo_ps(b.x, b.y); // r0 g0 r1 g1
        tmp1 = _mm_unpacklo_ps(b.z, b.w); // b0 a0 b1 a1

        triverts[2] = _mm_movelh_ps(tmp0, tmp1); // r0 g0 b0 a0
        break;
    case 3:
        // 0, 0, 3
        tmp0 = _mm_unpacklo_ps(b.x, b.y); // r0 g0 r1 g1
        tmp1 = _mm_unpacklo_ps(b.z, b.w); // b0 a0 b1 a1

        triverts[0] = _mm_movelh_ps(tmp0, tmp1); // r0 g0 b0 a0
        triverts[1] = triverts[0];

        tmp0 = _mm_unpackhi_ps(a.x, a.y); // r2 g2 r3 g3
        tmp1 = _mm_unpackhi_ps(a.z, a.w); // b2 a2 b3 a3

        triverts[2] = _mm_movehl_ps(tmp1, tmp0); // r3 g3 b3 a3
        break;
    };
}

bool PaLineList0(PA_STATE &pa, UINT slot, simdvector tri[3])
{
    // assemble tris from lines
    //
    // lines		  tri sets						SIMD Vectors
    // -----------	  -------------------------		------------
    // a(v0) a(v1) -> [ v0 v0 v1 ] [ v1 v1 v0 ]	->	v0v1v2v3
    // a(v2) a(v3) -> [ v2 v2 v3 ] [ v3 v3 v2 ]		v0v1v2v3
    //												v1v0v3v2

    simdvector &a = PaGetSimdVector(pa, 0, slot);

    for (int i = 0; i < 4; ++i)
    {
        simdvector &v0 = tri[0];
        v0[i] = _mm_shuffle_ps(a[i], a[i], _MM_SHUFFLE(3, 2, 1, 0));
        simdvector &v1 = tri[1];
        v1[i] = _mm_shuffle_ps(a[i], a[i], _MM_SHUFFLE(3, 2, 1, 0));
        simdvector &v2 = tri[2];
        v2[i] = _mm_shuffle_ps(a[i], a[i], _MM_SHUFFLE(2, 3, 0, 1));
    }

    SetNextPaState(pa, PaLineList0, PaLineListSingle0);

    pa.numPrimsComplete += 4;
    return true;
}

void PaLineListSingle0(PA_STATE &pa, UINT slot, UINT triIndex, __m128 triverts[3])
{
    // We have 12 simdscalars contained within 3 simdvectors which
    // hold at least 4 triangles worth of data (from 2 lines strips). We want to
    // assemble a single triangle with data in horizontal form.
    simdvector &a = PaGetSimdVector(pa, pa.prev, slot);
    simdscalar tmp0;
    simdscalar tmp1;

    // Convert from vertical to horizontal.
    switch (triIndex)
    {
    case 0:
        // 0, 0, 1
        tmp0 = _mm_unpacklo_ps(a.x, a.y); // r0 g0 r1 g1
        tmp1 = _mm_unpacklo_ps(a.z, a.w); // b0 a0 b1 a1

        triverts[0] = _mm_movelh_ps(tmp0, tmp1); // r0 g0 b0 a0
        triverts[1] = triverts[0];
        triverts[2] = _mm_movehl_ps(tmp1, tmp0); // r1 g1 b1 a1
        break;
    case 1:
        // 1, 1, 0
        tmp0 = _mm_unpacklo_ps(a.x, a.y); // r0 g0 r1 g1
        tmp1 = _mm_unpacklo_ps(a.z, a.w); // b0 a0 b1 a1

        triverts[0] = _mm_movehl_ps(tmp1, tmp0); // r1 g1 b1 a1
        triverts[1] = triverts[0];
        triverts[2] = _mm_movelh_ps(tmp0, tmp1); // r0 g0 b0 a0
        break;
    case 2:
        // 2, 2, 3
        tmp0 = _mm_unpackhi_ps(a.x, a.y); // r2 g2 r3 g3
        tmp1 = _mm_unpackhi_ps(a.z, a.w); // b2 a2 b3 a3

        triverts[0] = _mm_movelh_ps(tmp0, tmp1); // r2 g2 b2 a2
        triverts[1] = triverts[0];
        triverts[2] = _mm_movehl_ps(tmp1, tmp0); // r3 g3 b3 a3
        break;
    case 3:
        // 3, 3, 2
        tmp0 = _mm_unpackhi_ps(a.x, a.y); // r2 g2 r3 g3
        tmp1 = _mm_unpackhi_ps(a.z, a.w); // b2 a2 b3 a3

        triverts[0] = _mm_movehl_ps(tmp1, tmp0); // r3 g3 b3 a3
        triverts[1] = triverts[0];
        triverts[2] = _mm_movelh_ps(tmp0, tmp1); // r2 g2 b2 a2
        break;
    };
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
        this->numPrims = in_numPrims * 2; // Convert line primitives into triangles
        break;
    case TOP_LINE_STRIP:
        this->pfnPaFunc = PaLineStrip0;
        this->numPrims = in_numPrims * 2; // Convert line primitives into triangles
        break;
    default:
        assert(0); // not supported
        break;
    };
}
#endif