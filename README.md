OpenSWR
=======

Overview
--------

OpenOpenSWR is a high performance, highly scalable software rasterizer
which implements a subset of the OpenGL API.  Currently it implements
a subset of the OpenGL 1.4 specification, namely the old-style fixed
function pipeline.

OpenSWR will utilize all cores to parallelize work on both the vertex
and fragment processing.

OpenSWR is provided as a shared library which can be used as a dropped
in replacement for the system OpenGL on Linux (via LD_LIBRARY_PATH) or
Windows (by placing in the application directory).  It also provides
an OSMesa interface that can be used for offscreen rendering.

ALPHA
-----

This version is the first to be released to the public.  Please bear
with us if there are build or functionality issues.  The major
applications used for testing were ParaView and VisIt - if you are
trying another program you may encounter missing features.

This version is coming out as we are working on some major cleanups to
the code.  In order to keep our commitment to release this to the
community and to provide a well-tested version, we are releasing the
code that was used for the SC14 demonstrations.  The next major
release will contain the cleanups.

Issues
------

If you encounter problems, please file an issue on github:

  https://github.com/openswr

Dependencies
------------

* CMake 2.8+
* LLVM 3.4+
* Python 3+
* libnuma-dev (Linux)
* ICC, GCC, or clang (Linux)
* Visual Studio 2012 (Windows)

Building
--------

Linux:

* mkdir build
* cd build
* "cmake-gui .." or "ccmake .."
* select a "Release" build and desired compiler.  Note that you do not
  want to select the GLSL option - this is currently work in progress
  and requires a modified LLVM.
* select the target architecture (SSE4.2, AVX, or CORE-AVX2) appropriate
  for your CPU.  The default is CORE-AVX2 for 4th Gen. Core processors.
* make

Windows:

* Run cmake GUI, configure as above
* Open generated solution file in Visual Studio
* Build

Using
-----

Linux:

The build creates a libGL.so.1 which you can use instead of the system
OpenGL implementation, by pointing to it with LD_LIBRARY_PATH or
LD_PRELOAD before starting your application.

Windows:

Due to the way ICD OpenGL drivers are loaded, the opengl32.dll must be
renamed to the same as your system's OpenGL DLL, then placed in the
application working directory.
