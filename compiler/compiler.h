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

#ifndef _SWR_DRIVER_LEVEL_COMPILER_H_
#define _SWR_DRIVER_LEVEL_COMPILER_H_

#include "api.h"
#include <vector>
#include <cstdio>

// Assembling API.
enum SWRC_WORDCODE
{
#define ENTRY(NAME, ...) SWRC_##NAME,
#include "swrcwordcode.inl"
#undef ENTRY
};

enum SWRC_CMPOP
{
    SWRC_CMP_EQ,
    SWRC_CMP_NE,
    SWRC_CMP_LT,
    SWRC_CMP_LE,
    SWRC_CMP_GT,
    SWRC_CMP_GE,
};

HANDLE swrcCreateCompiler(UINT scalarizedWidth);

void swrcDestroyCompiler(
    HANDLE hCompiler);

PFN_FETCH_FUNC swrcCreateFetchShader(
    HANDLE hCompiler,
    HANDLE hContext,
    UINT numElements,
    INPUT_ELEMENT_DESC (&inputLayout)[KNOB_NUM_ATTRIBUTES],
    UINT (&inputStrides)[KNOB_NUM_STREAMS],
    SWRC_WORDCODE contiguity,
    SWR_TYPE indexType);

PFN_TEXTURE_SAMPLE_INSTR swrcCreateTextureSampleInstruction(
    HANDLE hCompiler,
    HANDLE hTx,
    HANDLE hTxV,
    HANDLE hSmp);

struct SWRC_ASM;
SWRC_ASM *swrcCreateAssembler(
    HANDLE hCompiler,
    SWR_SHADER_TYPE sType);

void swrcDestroyAssembler(
    SWRC_ASM *&);

void swrcSetTracing(
    SWRC_ASM *,
    FILE *);

HANDLE swrcAssemble(
    SWRC_ASM *);

// This is for deferred attributes. Will call this for VS with position only and for the
// attribute shader with slots specifying attribs.
HANDLE swrcAssembleWith(
    SWRC_ASM *,
    UINT numOutSlots,
    const UINT *outSlots);

// Used for tracing. SWRFF uses this everywhere.
void swrcAddNote(
    SWRC_ASM *,
    const char *pNote);

// Local SWRC compiler options. Not passed to LLVM.
void swrcSetOption(
    SWRC_ASM *,
    SWRC_WORDCODE);

// Single arg options
void swrcSetOption(
    SWRC_ASM *,
    SWRC_WORDCODE,
    UINT a);

void swrcSetOption(
    SWRC_ASM *,
    SWRC_WORDCODE,
    UINT a,
    UINT b);

// Standard decl for VS or PS inputs and outputs. Not used for RT dest outputs.
SWRC_WORDCODE swrcAddDecl(
    SWRC_ASM *,
    UINT slot,
    SWRC_WORDCODE inOrOut);

// This is used for PS RT. The type argument would specify destination format.
SWRC_WORDCODE swrcAddDecl(
    SWRC_ASM *,
    UINT slot,
    SWRC_WORDCODE inOrOut,
    SWRC_WORDCODE type);

// This is for packed inputs for PS. E.g. Rasterizer passes just xy or xyz and this unpacks this to
// xy00 or xyz0 into the register file. This should assert if unrasterize attributes are used by shader.
SWRC_WORDCODE swrcAddDecl(
    SWRC_ASM *,
    UINT slot,
    SWRC_WORDCODE inOrOut,
    UINT subSet);

// Constant buffers contain arbitrary data (e.g. various formats and widths) For bindless textures the function pointers
// to the sampler will be in the CB. This generates an instruction that fetches from the CB. Note there is only 1 CB and
// 1 register file.
// SIMTIZE: Relies on AddDecl and AddConstant on how data is loaded. Decls are just loaded but add constants are broadcast.
SWRC_WORDCODE swrcAddConstant(
    SWRC_ASM *,
    UINT wordOffset,
    SWRC_WORDCODE type);

// Loads immediate ui32 (mov r0, 5)
SWRC_WORDCODE swrcAddImm(
    SWRC_ASM *,
    UINT val);
// Loads vector immediate v4_ui32
SWRC_WORDCODE swrcAddImm(
    SWRC_ASM *,
    UINT v0, UINT v1, UINT v2, UINT v3);
// Float version
SWRC_WORDCODE swrcAddImm(
    SWRC_ASM *,
    float val);
// Float vector version (xyzw vector)
SWRC_WORDCODE swrcAddImm(
    SWRC_ASM *,
    float v0, float v1, float v2, float v3);
// Standard compiler block stuff
SWRC_WORDCODE swrcAddBlock(
    SWRC_ASM *);
// This is the program entry point block
void swrcSetEntryBlock(
    SWRC_ASM *,
    SWRC_WORDCODE block);
// This is the context block that the other functions will add instructions to.
// Note: there are no flow control instructions.
void swrcSetCurrentBlock(
    SWRC_ASM *,
    SWRC_WORDCODE block);

SWRC_WORDCODE swrcAddInstr(
    SWRC_ASM *,
    SWRC_WORDCODE op);
SWRC_WORDCODE swrcAddInstr(
    SWRC_ASM *,
    SWRC_WORDCODE op,
    SWRC_WORDCODE arg0);
SWRC_WORDCODE swrcAddInstr(
    SWRC_ASM *,
    SWRC_WORDCODE op,
    SWRC_WORDCODE arg0,
    SWRC_WORDCODE arg1);
SWRC_WORDCODE swrcAddInstr(
    SWRC_ASM *,
    SWRC_WORDCODE op,
    SWRC_WORDCODE arg0,
    SWRC_WORDCODE arg1,
    SWRC_WORDCODE arg2);
SWRC_WORDCODE swrcAddInstr(
    SWRC_ASM *,
    SWRC_WORDCODE op,
    SWRC_WORDCODE arg0,
    SWRC_WORDCODE arg1,
    SWRC_WORDCODE arg2,
    SWRC_WORDCODE arg3);
SWRC_WORDCODE swrcAddInstr(
    SWRC_ASM *,
    SWRC_WORDCODE op,
    SWRC_WORDCODE arg0,
    SWRC_WORDCODE arg1,
    SWRC_WORDCODE arg2,
    SWRC_WORDCODE arg3,
    SWRC_WORDCODE arg4);
SWRC_WORDCODE swrcAddInstr(
    SWRC_ASM *,
    SWRC_WORDCODE op,
    std::vector<SWRC_WORDCODE> const &args);

#endif //_SWR_DRIVER_LEVEL_COMPILER_H_
