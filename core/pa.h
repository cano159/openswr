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

#define NUM_VERTEXOUTPUT 3

// The PA is a state machine that assembles triangles from vertex shader simd
// output. Here is the sequence
//	1. Execute FS/VS to generate a simd vertex (4 vertices for SSE simd and 8 for AVX simd).
//	2. Execute PA function to assemble and bin triangles.
//		a.	The PA function is a set of functions that collectively make up the
//			state machine for a given topology.
//				1.	We use a state index to track which PA function to call.
//		b. Often the PA function needs to 2 simd vertices in order to assemble the next triangle.
//				1.	We call this the current and previous simd vertex.
//				2.	The SSE simd is 4-wide which is not a multiple of 3 needed for triangles. In
//					order to assemble the second triangle, for a triangle list, we'll need the
//					last vertex from the previous simd and the first 2 vertices from the current simd.
//				3. At times the PA can assemble multiple triangles from the 2 simd vertices.
struct PA_STATE
{
    DRAW_CONTEXT *pDC;                   // draw context
    VERTEXOUTPUT vout[NUM_VERTEXOUTPUT]; // vertex shader output.
    VERTEXOUTPUT leadingVertex;          // For tri-fan
    UINT numPrims;                       // Total number of primitives for draw.
    UINT numPrimsComplete;               // Total number of complete primitives.

    UINT numSimdPrims; // Number of prims in current simd.

    UINT cur;   // index to current VS output.
    UINT prev;  // index to prev VS output. Not really needed in the state.
    UINT first; // index to first VS output. Used for trifan.

    UINT counter; // state counter
    bool reset;   // reset state

    typedef bool (*PFN_PA_FUNC)(PA_STATE &state, UINT slot, simdvector result[3]);
    typedef void (*PFN_PA_SINGLE_FUNC)(PA_STATE &pa, UINT slot, UINT triIndex, __m128 tri[3]);

    PFN_PA_FUNC pfnPaFunc;     // PA state machine function for assembling 4 triangles.
    PFN_PA_FUNC pfnPaNextFunc; // Next PA state machine function.

    PFN_PA_SINGLE_FUNC pfnPaSingleFunc; // PA state machine function for assembling single triangle.

    PA_STATE(DRAW_CONTEXT *pDC, UINT numPrims);
};

bool PaHasWork(PA_STATE &pa);
VERTEXOUTPUT &PaGetNextVsOutput(PA_STATE &pa);
bool PaAssemble(PA_STATE &pa, UINT slot, simdvector tri[3]);
void PaAssembleSingle(PA_STATE &pa, UINT slot, UINT triIndex, __m128 tri[3]);
bool PaNextPrim(PA_STATE &pa);
UINT PaNumTris(PA_STATE &pa);
