# Copyright 2014 Intel Corporation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 2.8)

set(TARGET_ARCH "CORE-AVX2" CACHE STRING "Target processor architecture")
set_property(CACHE TARGET_ARCH PROPERTY STRINGS "SSE4.2" "AVX" "CORE-AVX2")

find_program(LLVM_CONFIG "llvm-config")
set(LLVM_COMMAND ${LLVM_CONFIG} "--includedir")
execute_process(COMMAND ${LLVM_COMMAND} RESULT_VARIABLE ERROR OUTPUT_VARIABLE LLVM_INCLUDEDIR)
string(REGEX REPLACE "\n" "" LLVM_INCLUDEDIR ${LLVM_INCLUDEDIR})
set(LLVM_COMMAND ${LLVM_CONFIG} "--libdir")
execute_process(COMMAND ${LLVM_COMMAND} RESULT_VARIABLE ERROR OUTPUT_VARIABLE LLVM_LIBDIR)
string(REGEX REPLACE "\n" "" LLVM_LIBDIR ${LLVM_LIBDIR})
set(LLVM_COMMAND ${LLVM_CONFIG} "--libnames")
execute_process(COMMAND ${LLVM_COMMAND} RESULT_VARIABLE ERROR OUTPUT_VARIABLE LLVM_LIBNAMES)
string(REGEX REPLACE "\n" "" LLVM_LIBNAMES ${LLVM_LIBNAMES})
if(WIN32)
string(REGEX REPLACE "\\.a" "" LLVM_LIBNAMES ${LLVM_LIBNAMES})
string(REGEX REPLACE "lib" "" LLVM_LIBNAMES ${LLVM_LIBNAMES})
endif()
separate_arguments(LLVM_LIBNAMES)

if(WIN32)
elseif(UNIX)
	set(COMPILER "gcc" CACHE STRING "Compiler to use")
	set_property(CACHE COMPILER PROPERTY STRINGS icc gcc clang++)
else()
	message(FATAL_ERROR "Unsupported platform")
endif()

project(SWR)

if(${TARGET_ARCH} STREQUAL "SSE4.2")
	add_compile_options(-DKNOB_ARCH=KNOB_ARCH_SSE)
elseif(${TARGET_ARCH} STREQUAL "AVX")
	add_compile_options(-DKNOB_ARCH=KNOB_ARCH_AVX)
elseif(${TARGET_ARCH} STREQUAL "CORE-AVX2")
	add_compile_options(-DKNOB_ARCH=KNOB_ARCH_AVX2)
else()
	message(FATAL_ERROR "Unsupported architecture")
endif()

function(add_codegen arch)
	add_compile_options(-m64)
	if(${arch} STREQUAL "SSE4.2")
		add_compile_options(-march=corei7)
	elseif(${arch} STREQUAL "AVX")
		add_compile_options(-march=corei7-avx)
	elseif(${arch} STREQUAL "CORE-AVX2")
		add_compile_options(-march=core-avx2)
	endif()
endfunction(add_codegen)

if(WIN32)
	set(GL opengl32)
	link_directories(${LLVM_LIBDIR})
	add_compile_options(/arch:AVX)
	set(DEF ${CMAKE_CURRENT_BINARY_DIR}/ogldriver/driver.def)
elseif(UNIX)
	set(GL GL)
	link_directories(${LLVM_LIBDIR})

	add_compile_options(-std=c++0x -fPIC)
	add_compile_options(-D__STDC_CONSTANT_MACROS -D__STDC_LIMIT_MACROS)
	add_codegen(${TARGET_ARCH})

	if(${COMPILER} STREQUAL "icc")
		set(CMAKE_CXX_COMPILER "icpc")
		add_compile_options(-Wall -ipo -no-prec-div)
	elseif(${COMPILER} STREQUAL "gcc")
	elseif(${COMPILER} STREQUAL "clang++")
		set(CMAKE_CXX_COMPILER "clang++")
	endif()
else()
	message(FATAL_ERROR "Unsupported platform")
endif()

add_subdirectory(compiler)
add_subdirectory(core)
add_subdirectory(ogldriver)

add_library(${GL} SHARED
			$<TARGET_OBJECTS:core>
			$<TARGET_OBJECTS:compiler>
			$<TARGET_OBJECTS:ogldriver>
			${DEF}
			)
install(TARGETS ${GL}
	ARCHIVE DESTINATION lib
	LIBRARY DESTINATION lib
	RUNTIME DESTINATION bin
)

if(WIN32)
	set_source_files_properties(${DEF} PROPERTIES GENERATED 1)
	add_dependencies(${GL} generate_def)
endif()

if(NOT WIN32)
if(${COMPILER} STREQUAL "icc")
	set_target_properties(GL PROPERTIES LINK_FLAGS -static-intel)
endif()
endif()

set_target_properties(${GL} PROPERTIES SOVERSION 1)

# can't use check_symbol_exists because llvm defines it to 0 or 1
include(CheckCSourceCompiles)
set(CMAKE_REQUIRED_INCLUDES ${LLVM_INCLUDEDIR})
check_c_source_compiles("
	#include <llvm/Config/config.h>
	#if !defined(LLVM_USE_INTEL_JITEVENTS) || (LLVM_USE_INTEL_JITEVENTS != 1)
	#error
	#endif
	main(){}" HAVE_JITEVENTS)
unset(CMAKE_REQUIRED_INCLUDES)

if(HAVE_JITEVENTS)
	target_link_libraries(${GL} LLVMIntelJITEvents)
endif()

target_link_libraries(${GL} ${LLVM_LIBNAMES})

if(WIN32)
	target_link_libraries(${GL} gdiplus d3d9)
elseif(UNIX)
	target_link_libraries(${GL} Xext X11 pthread numa tinfo)
endif()

