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

include_directories(../compiler)
include_directories(../core)
include_directories(../common)
include_directories(.)
include_directories(./gl)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

if(WIN32)
	set(PYTHON python)
else()
	set(PYTHON python3)
endif()

ADD_CUSTOM_COMMAND(
	OUTPUT glcmds.inl executedl.inl generatedl.inl optimizedl.inl glfz.hpp
			glfz.inl glsx.hpp glim.hpp glst.hpp glcl.cpp glcl.hpp gl.inl
			stubs.cpp dispatch.h dispatch.cpp
			${DEF}
	COMMAND ${PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/ogl_generate.py --output-dir ${CMAKE_CURRENT_BINARY_DIR}
	DEPENDS ogl_generate.py
	COMMENT "Generating OpenGL files..."
)

if(WIN32)
	ADD_CUSTOM_TARGET(generate_def DEPENDS ${DEF})
endif()

ADD_CUSTOM_COMMAND(
	OUTPUT serdeser.hpp
	COMMAND ${PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/ogl_serialize.py --output-dir ${CMAKE_CURRENT_BINARY_DIR} --max-args 17
	DEPENDS ogl_serialize.py
	COMMENT "Generating OpenGL serialization..."
)

if(CMAKE_BUILD_TYPE MATCHES Release)
	if(UNIX AND ICC)
		add_compile_options(-finline -inline-level=1 -inline-forceinline
			                -no-inline-max-total-size -no-inline-max-size)
	endif()
endif()

if(WIN32)
	set(PLATFORM dllmain.cpp wgl.cpp)
else()
	set(PLATFORM glx.cpp)
endif()

set(HEADERS gldd.h oglglobals.h swrffgen.h
	ogldisplaylist.hpp oglstate.hpp
	enumMap.inl gltrace.inl
	gl/glext.h gl/gl.h gl/osmesa.h)

if(WIN32)
	set(PLATFORM_HEADERS gl/wglext.h gl/wgl.h)
else()
	set(PLATFORM_HEADERS gl/glx.h)
endif()

if(WIN32)
    set(DISPATCH dispatch.cpp)
endif()

add_library(ogldriver OBJECT fragff.cpp glfz.cpp glim.cpp
	glst.cpp glsx.cpp ogldisplaylist.cpp
	oglglobals.cpp oglstate.cpp osmesa.cpp swrdd.cpp swrffgen.cpp
    ${DISPATCH}
	${PLATFORM}
	${HEADERS}
	${PLATFORM_HEADERS}
	${CMAKE_CURRENT_BINARY_DIR}/serdeser.hpp
	${CMAKE_CURRENT_BINARY_DIR}/glcl.cpp
	${CMAKE_CURRENT_BINARY_DIR}/stubs.cpp)

install(FILES gl/glext.h gl/gl.h gl/osmesa.h ${PLATFORM_HEADERS}
	DESTINATION include/GL)
