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

enum SWR_CLIPCODES
{
// Shift clip codes out of the mantissa to prevent denormalized values when used in float compare.
// Guardband is able to use a single high-bit with 4 separate LSBs, because it computes a union, rather than intersection, of clipcodes.
#define CLIPCODE_SHIFT 23
    FRUSTUM_LEFT = (0x01 << CLIPCODE_SHIFT),
    FRUSTUM_TOP = (0x02 << CLIPCODE_SHIFT),
    FRUSTUM_RIGHT = (0x04 << CLIPCODE_SHIFT),
    FRUSTUM_BOTTOM = (0x08 << CLIPCODE_SHIFT),

    FRUSTUM_NEAR = (0x10 << CLIPCODE_SHIFT),
    FRUSTUM_FAR = (0x20 << CLIPCODE_SHIFT),

    GUARDBAND_LEFT = (0x40 << CLIPCODE_SHIFT | 0x1),
    GUARDBAND_TOP = (0x40 << CLIPCODE_SHIFT | 0x2),
    GUARDBAND_RIGHT = (0x40 << CLIPCODE_SHIFT | 0x4),
    GUARDBAND_BOTTOM = (0x40 << CLIPCODE_SHIFT | 0x8)
};

#define FRUSTUM_CLIP_MASK (FRUSTUM_LEFT | FRUSTUM_TOP | FRUSTUM_RIGHT | FRUSTUM_BOTTOM | FRUSTUM_NEAR | FRUSTUM_FAR)
#define GUARDBAND_CLIP_MASK (FRUSTUM_NEAR | FRUSTUM_FAR | GUARDBAND_LEFT | GUARDBAND_TOP | GUARDBAND_RIGHT | GUARDBAND_BOTTOM)

void Clip(const float *pTriangle, const float *pAttribs, int numAttribs, float *pOutTriangles,
          int *numVerts, float *pOutAttribs);
