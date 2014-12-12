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

include_directories(../core)
include_directories(../common)
include_directories(${LLVM_INCLUDEDIR})

set(HEADERS compiler.h PtWise.h shader_math.h shaders.h Simtize.h
	swrcwordcode.inl texture_unit.h GPUFPMath.h)

add_library(compiler OBJECT compiler.cpp PtWise.cpp shaders.cpp shaders_vs.cpp
	Simtize.cpp texture_unit.cpp GPUFPMath.cpp ${HEADERS})
