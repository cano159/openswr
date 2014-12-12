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

include_directories(../common)
include_directories(.)

if(CMAKE_BUILD_TYPE MATCHES Release)
	if(UNIX AND ICC)
		add_compile_options(-finline -inline-level=1 -inline-forceinline
			                 -no-inline-max-total-size -no-inline-max-size)
	endif()
endif()

set(HEADERS api.h arena.h backend.h clip.h context.h defs.h
	fifo.hpp formats.h frontend.h knobs.h pa.h rasterizer.h
	rdtsc_def.h rdtsc.h resource.h threads.h tilemgr.h utils.h ../common/algebra.hpp
    ../common/containers.hpp ../common/os.h ../common/simdintrin.h ../common/widevector.hpp)

add_library(core OBJECT api.cpp arena.cpp backend.cpp clip.cpp formats.cpp
	frontend.cpp pa_avx.cpp pa.cpp rasterizer.cpp rdtsc.cpp resource.cpp
	threads.cpp tilemgr.cpp utils.cpp ${HEADERS})


