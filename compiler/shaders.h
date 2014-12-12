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

//@todo Add projection matrix
enum CONST_OFFSETS
{
    CONST_OFFSET_WORLD_MATRIX = 0,
    CONST_OFFSET_LIGHT_POSITION = 16,
    CONST_OFFSET_MAT_COLOR = 20,
    CONST_OFFSET_TEX_SCALE = 24,
};

// @todo We should combine pVertices and pAttributes and add position to SHADER_INPUT_SLOT.
enum SHADER_INPUT_SLOT
{
    // SHADER_INPUT_POSITION needs to go in here.
    SHADER_INPUT_SLOT_POSITION = 0,
    SHADER_INPUT_SLOT_NORMAL = 4,
    SHADER_INPUT_SLOT_COLOR0 = 8,
    SHADER_INPUT_SLOT_TEXCOORD0 = 12,
    SHADER_INPUT_MAX_SLOTS = SHADER_INPUT_SLOT_TEXCOORD0 + 4, // 12 floats for attributes
};

// @todo We need to review this. POSITION is necessary.
enum SHADER_INPUT_SLOTX
{
    // Sizes of each slot.
    SHADER_INPUT_SLOTX_NUM_POS = 1,
    SHADER_INPUT_SLOTX_NUM_COLOR = 4,
    SHADER_INPUT_SLOTX_NUM_NORMAL = 4,
    SHADER_INPUT_SLOTX_NUM_TEXCOORD = 8,
    SHADER_INPUT_SLOTX_NUM_ATTRIBUTE = 15,
    SHADER_INPUT_SLOTX_NUM_ATTR = 31,

    // Position of each slot.
    SHADER_INPUT_SLOTX_POSITION = 0,
    SHADER_INPUT_SLOTX_NORMAL = SHADER_INPUT_SLOTX_POSITION + SHADER_INPUT_SLOTX_NUM_POS,
    SHADER_INPUT_SLOTX_COLOR = SHADER_INPUT_SLOTX_NORMAL + SHADER_INPUT_SLOTX_NUM_NORMAL,
    SHADER_INPUT_SLOTX_TEXCOORD = SHADER_INPUT_SLOTX_COLOR + SHADER_INPUT_SLOTX_NUM_COLOR,
    SHADER_INPUT_SLOTX_ATTRIBUTE = SHADER_INPUT_SLOTX_TEXCOORD + SHADER_INPUT_SLOTX_NUM_TEXCOORD,

    SHADER_INPUT_SLOTX_ATTR = SHADER_INPUT_SLOTX_POSITION + SHADER_INPUT_SLOTX_NUM_POS,

    SHADER_INPUT_SLOTX_MAX_SLOTS = SHADER_INPUT_SLOTX_ATTR + SHADER_INPUT_SLOTX_NUM_ATTR,

    SHADER_INPUT_SLOTX_BAD_SLOT,
};

struct GLVSLightConstants
{
    // input description
    static const UINT MVP_MATRIX = 0;
    static const UINT NORMAL_MATRIX = 16;
    static const UINT MV_MATRIX = 32;
    static const UINT LIGHT0_POS = 48;
    static const UINT LIGHT0_DIFFUSE = 52;
    static const UINT LIGHT0_AMBIENT = 56;
    static const UINT FRONT_MAT_AMBIENT = 60;
    static const UINT FRONT_MAT_DIFFUSE = 64;
    static const UINT BACK_MAT_AMBIENT = 68;
    static const UINT BACK_MAT_DIFFUSE = 72;
    static const UINT FRONT_SCENE_COLOR = 76;
    static const UINT BACK_SCENE_COLOR = 80;

    // XXX: unsupported options
    static const UINT FRONT_SPECULAR_COLOR = 84;
    static const UINT BACK_SPECULAR_COLOR = 88;
    static const UINT NORMAL_SCALE = 92;
    static const UINT LIGHT0_SPECULAR = 96;
};

// Fetch shaders
void FetchStandard(SWR_FETCH_INFO &fetchInfo, VERTEXINPUT &in);
void FetchPosNormal(SWR_FETCH_INFO &fetchInfo, VERTEXINPUT &in);

// Vertex Shaders
void GLVSPosColor(const VERTEXINPUT &in, VERTEXOUTPUT &out);
void GLVSLightShader(const VERTEXINPUT &in, VERTEXOUTPUT &out);
void GLVSPosColorTexCoordShader(const VERTEXINPUT &in, VERTEXOUTPUT &out);

void VertexFuncPosOnly(const VERTEXINPUT &in, VERTEXOUTPUT &out);
void VertexFuncPosAttrib(const VERTEXINPUT &in, VERTEXOUTPUT &out);
void VertexFuncConstColor(const VERTEXINPUT &in, VERTEXOUTPUT &out);
void VertexFuncSmooth(const VERTEXINPUT &in, VERTEXOUTPUT &out);
void VertexFuncSmoothSphereMapAOS(const VERTEXINPUT &in, VERTEXOUTPUT &out);
void VertexFuncPosAttrib2(const VERTEXINPUT &in, VERTEXOUTPUT &out);

// Pixel Shaders
void constantColorShader(const SWR_TRIANGLE_DESC &work, SWR_PIXELOUTPUT &pOut);
void interpColorShaderSample(const SWR_TRIANGLE_DESC &work, SWR_PIXELOUTPUT &pOut);
void interpPosShaderSample(const SWR_TRIANGLE_DESC &work, SWR_PIXELOUTPUT &pOut);
void InterpColorShaderNoPerspective(const SWR_TRIANGLE_DESC &work, SWR_PIXELOUTPUT &pOut);
void GLPSColorTextureShader(const SWR_TRIANGLE_DESC &work, SWR_PIXELOUTPUT &pOut);

#if (KNOB_VERTICALIZED_FE != 0)

// Position Only
void VertexFuncPos(const VERTEXINPUT &in, VERTEXOUTPUT &out);
#endif

extern PFN_PIXEL_FUNC InterpColorShaderTable[NUM_ZFUNC][2];
extern PFN_PIXEL_FUNC InterpColorTexShaderTable[NUM_ZFUNC][2];
extern PFN_PIXEL_FUNC InterpColorTexBlendShaderTable[NUM_ZFUNC][2];
extern PFN_PIXEL_FUNC InterpColorTexBlendAlphaShaderTable[NUM_ZFUNC][2];
extern PFN_PIXEL_FUNC GLPSLightShaderTable[NUM_ZFUNC][2];
extern PFN_PIXEL_FUNC GLFragFFTable[KNOB_NUMBER_OF_TEXTURE_VIEWS + 1][NUM_ZFUNC][2];
