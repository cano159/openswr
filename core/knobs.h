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

#ifndef __SWR_KNOBS_H__
#define __SWR_KNOBS_H__

#define KNOB_ARCH_SSE 0
#define KNOB_ARCH_AVX 1
#define KNOB_ARCH_AVX2 2
#define KNOB_ARCH_AVX512 3

///////////////////////////////////////////////////////////////////////////////
// Performance knobs
///////////////////////////////////////////////////////////////////////////////

// system max threads (actual thread count picked dynamically)
#define KNOB_MAX_NUM_THREADS 40

#define KNOB_MAX_DRAWS_IN_FLIGHT 160
#define KNOB_MAX_PRIMS_PER_DRAW 49140

#define KNOB_FLOATS_PER_ATTRIBUTE 4
#define KNOB_ATTRIBUTES_PER_FETCH 1

#define KNOB_MIN_WORK_THREADS 2
#define KNOB_WORKER_THREAD_OFFSET 1
#define KNOB_USE_HYPERTHREAD_AS_WORKER

#define KNOB_ENABLE_ASYNC_FLIP

///////////////////////////////////////////////////////////////////////////////
// Architecture validation
///////////////////////////////////////////////////////////////////////////////
#if (KNOB_ARCH == KNOB_ARCH_SSE)
#define KNOB_VS_SIMD_WIDTH 4
#elif(KNOB_ARCH == KNOB_ARCH_AVX)
#define KNOB_VS_SIMD_WIDTH 8
#elif(KNOB_ARCH == KNOB_ARCH_AVX2)
#define KNOB_VS_SIMD_WIDTH 8
#elif(KNOB_ARCH == KNOB_ARCH_AVX512)
#define KNOB_VS_SIMD_WIDTH 16
#error "AVX512 not yet supported"
#else
#error "Unknown architecture"
#endif

///////////////////////////////////////////////////////////////////////////////
// Configuration knobs
///////////////////////////////////////////////////////////////////////////////
#define KNOB_NUMBER_OF_TEXTURE_VIEWS 2
#define KNOB_NUMBER_OF_TEXTURE_SAMPLERS 2
#define KNOB_NUM_STREAMS 16
#define KNOB_NUM_RENDERTARGETS 16 // includes Z, stencil, etc.
#define KNOB_NUM_ATTRIBUTES 4
#define KNOB_VERTICALIZED_FE 1
#define KNOB_VERTICALIZED_BINNER 0
#define KNOB_VERTICALIZED_BE 0

#define KNOB_GUARDBAND_WIDTH 4096.0f
#define KNOB_GUARDBAND_HEIGHT 2048.0f

#define KNOB_FE_BACKOFF_COUNT 3

#define KNOB_WORKER_SPIN_LOOP_COUNT 5000

#define KNOB_ENABLE_NUMA 0

#define KNOB_MACROTILE_X_DIM 128
#define KNOB_MACROTILE_Y_DIM 128

#define KNOB_TILE_X_DIM 4
#define KNOB_TILE_X_DIM_SHIFT 2
#define KNOB_TILE_Y_DIM 2
#define KNOB_TILE_Y_DIM_SHIFT 1

#if KNOB_VS_SIMD_WIDTH == 8 && KNOB_TILE_X_DIM < 4
#error "incompatible width/tile dimensions"
#endif

#if KNOB_VS_SIMD_WIDTH == 4
#define SIMD_TILE_X_DIM 2
#define SIMD_TILE_Y_DIM 2
#elif KNOB_VS_SIMD_WIDTH == 8
#define SIMD_TILE_X_DIM 4
#define SIMD_TILE_Y_DIM 2
#else
#error "Invalid simd width"
#endif

///////////////////////////////////////////////////////////////////////////////
// JIT knobs - these knobs will be removed as things get JITed
///////////////////////////////////////////////////////////////////////////////
#define KNOB_JIT_EARLY_RAST 1
#define KNOB_JIT_FETCHSHADER_VIZ 0
#define KNOB_JIT_VERTEXSHADER_VIZ 0

///////////////////////////////////////////////////////////////////////////////
// Debug knobs
///////////////////////////////////////////////////////////////////////////////
//#define KNOB_ENABLE_RDTSC
#define KNOB_BUCKETS_START_FRAME 100
#define KNOB_BUCKETS_END_FRAME 200
//#define KNOB_VISUALIZE_MACRO_TILES
//#define KNOB_TOSS_VERTICES				1
//#define KNOB_TOSS_DRAW					1
//#define KNOB_TOSS_QUEUE_FE				1
//#define KNOB_TOSS_FETCH					1
//#define KNOB_TOSS_IA						1
//#define KNOB_TOSS_VS						1
//#define KNOB_TOSS_SETUP_TRIS				1
//#define KNOB_TOSS_BIN_TRIS				1
//#define KNOB_TOSS_RS						1
#define KNOB_MAX_PREVIOUS_FRAMES_FOR_FPS_AVG 32
//#define KNOB_RUNNING_AVERAGE_FPS

// one and only one thread
#define KNOB_SINGLE_THREADED 0

#if KNOB_SINGLE_THREADED
#undef KNOB_MAX_DRAWS_IN_FLIGHT
#undef KNOB_MAX_NUM_THREADS
#define KNOB_MAX_DRAWS_IN_FLIGHT 1
#define KNOB_MAX_NUM_THREADS 1
#endif

///////////////////////////////////////////////////////////////////////////////
// SWRC knobs
///////////////////////////////////////////////////////////////////////////////
//#define KNOB_SWRC_TRACING
//#define KNOB_SWRC_PS

///////////////////////////////////////////////////////////////////////////////
// GL knobs
///////////////////////////////////////////////////////////////////////////////
#define KNOB_FULL_SWRFF
//#define KNOB_GL_TRACE
#define KNOB_USE_UBER_FRAG_SHADER 1
#endif //__SWR_KNOBS_H__
