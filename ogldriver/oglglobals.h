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

#ifndef OGL_GLOBALS_H
#define OGL_GLOBALS_H

#define GL_GLEXT_LEGACY
#undef APIENTRY
#include "gl/gl.h"
#undef APIENTRY
#include "os.h"
#include <cassert>
#include "gl/glext.h"

typedef void *DDHANDLE;
typedef void *DDHBUFFER;
typedef void *DDHTEXTURE;
struct DDProcTable;

namespace OGL
{

// This is 'namespace' state: DO NOT NAME!
enum
{
    NUM_TEXTURES = 8,
    NUM_ATTRIBUTES = 12,     // 1 norm, 1 fog, 2 color, 8 texture
    NUM_SIDE_ATTRIBUTES = 1, // 1 normalized norm
    MAX_DL_STACK_DEPTH = 64,
    MAX_MATRIX_STACK_DEPTH = 32,
    VERTEX_BUFFER_COUNT = 1024 * 6,
    INDEX_BUFFER_COUNT = 1024 * 6,
    NUM_LIGHTS = 8,
    NUM_COLORS = 2,
    NUM_TEXCOORDS = 8,
    DEFAULT_SPOT_CUT = 180,
};

struct Global;
DDProcTable &GetDDProcTable();
DDHANDLE GetDDHandle();
void DestroyGlobals();

enum EnumTy
{
};
enum BitfieldTy
{
};

enum AttributeFormat
{
    OGL_R32_FLOAT,
    OGL_RG32_FLOAT,
    OGL_RGB32_FLOAT,
    OGL_RGBA32_FLOAT,

    OGL_R8_UNORM,
    OGL_RG8_UNORM,
    OGL_RGB8_UNORM,
    OGL_RGBA8_UNORM,
};

enum IndexBufferKind
{
    OGL_NO_INDEX_BUFFER_GENERATION,
    OGL_GENERATE_INDEX_BUFFER,
    OGL_GENERATE_NEGATIVE_INDEX_BUFFER_8,
    OGL_GENERATE_NEGATIVE_INDEX_BUFFER_16,
};
}

//#define ASSERT_STUB(ARG) assert(ARG)
#define ASSERT_STUB(ARG)
//#define OGL_LOGGING
#ifdef OGL_LOGGING
#define OGL_TRACE(NAME, STATE, ...) OGL::glTrace(#NAME, STATE, ##__VA_ARGS__)
#else
#define OGL_TRACE(...)
#endif

#endif //OGL_GLOBALS_H
