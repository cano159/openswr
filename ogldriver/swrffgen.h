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

#ifndef OGL1_SWR_FF_GEN_H
#define OGL1_SWR_FF_GEN_H

#include "compiler.h"
#include "oglstate.hpp"

struct SWRFF_OPTIONS
{
    UINT deferVS : 1;      // is this a deferred shader?
    UINT positionOnly : 1; // if deferred, is it the position or attribute?
UINT:
    0;                 // optimization options. force alignment.
    UINT optLevel : 2; // -O0 ... -O3
UINT:
    0;                      // debugging options. force alignment.
    UINT colorIsZ : 1;      // pos.z => color
    UINT colorCodeFace : 1; // front face == red, back face == blue
};

struct SWRFF_PIPE_RESULT
{
    PFN_VERTEX_FUNC pfnVS;
    PFN_PIXEL_FUNC pfnPS;
    UINT frontLinkageMask;
    UINT backLinkageMask;
};

SWRFF_PIPE_RESULT swrffGen(_simd_crcint Name, HANDLE hCompiler, HANDLE hContext, OGL::State &, SWRFF_OPTIONS swrffOpt);

#endif //OGL1_SWR_FF_GEN_H
