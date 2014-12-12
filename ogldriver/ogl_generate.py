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

import sys
import os
from argparse import ArgumentParser, RawTextHelpFormatter

# fix for python 2/3 compatibility
if sys.version_info < (3,):
        range = xrange

tyVoid      = "void"
tyCPVoid    = "const GLvoid*"
tyBoolean   = "GLboolean"
tyPBoolean  = "GLboolean*"
tyByte      = "GLbyte"
tyEnum      = "GLenum"
tyUint      = "GLuint"
tyCPUint    = "const GLuint*"
tyUbyte     = "GLubyte"
tyCPUbyte   = "const GLubyte*"
tyUshort    = "GLushort"
tyCPUshort  = "const GLushort*"
tyShort     = "GLshort"
tyBitfield  = "GLbitfield"
tyFloat     = "GLfloat"
tyDouble    = "GLdouble"
tyClampf        = "GLclampf"
tyClampd    = "GLclampd"
tySizei     = "GLsizei"
tyInt       = "GLint"
tyPInt      = "GLint*"
tyPUint     = "GLuint*"
tyCPByte    = "const GLbyte*"
tyCPUbyte   = "const GLubyte*"
tyCPInt     = "const GLint*"
tyArrInt4   = "std::array<GLint, 4> const&"
tyPDouble   = "GLdouble*"
tyPFloat    = "GLfloat*"
tyCPDouble  = "const GLdouble*"
tyCPFloat   = "const GLfloat*"
tyCPShort   = "const GLshort*"
tyArrD4     = "std::array<GLdouble, 4> const&"
tyArrF4     = "std::array<GLfloat, 4> const&"
tyM44       = "SWRL::m44f"
tyV4        = "SWRL::v4f"
tyCRM44D    = "SWRL::m44d const&"
tyCRM44F    = "SWRL::m44f const&"
tyCPVoid    = "const GLvoid*"
tyPVoid     = "GLvoid*"
tyULL       = "VertexAttributeFormats"
tyIntPtrARB = "GLintptrARB"
tySizeIptrARB = "GLsizeiptrARB"
tyIntPtr        = "GLintptr"
tySizeIptr      = "GLsizeiptr"
tyPChar         = "GLchar*"
tyCPChar        = "const GLchar*"
tyCPPChar       = "const GLchar**"

class GLFN:
        def __init__(self):
                self.ExtName    = ""
                self.ExtParams  = []
                self.CallName   = ""
                self.CallParams = []
                self.CallArgs   = []
                self.Exported   = False
                self.Legality   = None

#
#-------------- BIG GL DEFINITION TABLE ------------------
#

StdColor4fParams = [
        (tyFloat, "red", None, None, None),
        (tyFloat, "green", None, None, None),
        (tyFloat, "blue", None, None, None),
        (tyFloat, "alpha", None, None, None)]


functions = [
# ExtName               CallName    Exported    Legality        AutoFZ      RType       ExtParams (ExtType, ExtName, CallType, CallName, CallArg)*
("ActiveTexture",       None,       True,       "Always",       True,       tyVoid,     [(tyEnum, "which", None, None, None)]),
("AddVertexSWR",        None,       True,       "Always",       True,       tyVoid,     [(tyDouble, "x", None, None, None),
                                                                                                                                                                                 (tyDouble, "y", None, None, None),
                                                                                                                                                                                 (tyDouble, "z", None, None, None),
                                                                                                                                                                                 (tyDouble, "w", None, None, None),
                                                                                                                                                                                 (tyInt, "index", None, None, None),
                                                                                                                                                                                 (tyBoolean, "permuteVBs", None, None, None),
                                                                                                                                                                                 (tyEnum, "genIBKind", None, None, None)]),
("AlphaFunc",                   None,           True,           "Always",               True,           tyVoid,         [(tyEnum, "func", None, None, None),
                                                                                                                                                                                 (tyClampf, "ref", None, None, None)]),
("ArrayElement",        None,       True,       "SpecialCL",    False,      tyVoid,     [(tyInt, "index", None, None, None)]),
("Begin",               None,       True,       "SpecialCL",    False,      tyVoid,     [(tyEnum, "topology", None, None, None)]),
("BindTexture",         None,       True,       "Always",       True,       tyVoid,     [(tyEnum, "target", None, None, None),
                                                                                                                                                                                 (tyUint, "texture", None, None, None)]),
("BlendFunc",           None,       True,       "Always",       True,       tyVoid,     [(tyEnum, "sfactor", None, None, None),
                                                                                                                                                                                 (tyEnum, "dfactor", None, None, None)]),
("CallList",            None,       True,       "Always",       True,       tyVoid,     [(tyUint, "list", None, None, None)]),
("CallLists",           None,       True,       "Always",       True,       tyVoid,     [(tySizei, "n", None, None, None),
                                                                                                                                                                                 (tyEnum, "type", None, None, None),
                                                                                                                                                                                 (tyCPVoid, "lists", None, None, None)]),
("Clear",               None,       True,       "Always",       False,      tyVoid,     [(tyBitfield, "mask", None, None, None)]),
("ClearColor",          None,       True,       "Always",       True,       tyVoid,     StdColor4fParams),
("ClearDepth",          None,       True,       "Always",       True,       tyVoid,     [(tyClampd, "depth", None, None, None)]),
("ClientActiveTexture", None,       True,       "SpecialCL",    True,       tyVoid,     [(tyEnum, "texture", None, None, None)]),
# Color is handled below
("ColorMask",                   None,           True,           "Always",               True,           tyVoid,         [(tyBoolean, "red", None, None, None),
                                                                                                                                                                                 (tyBoolean, "green", None, None, None),
                                                                                                                                                                                 (tyBoolean, "blue", None, None, None),
                                                                                                                                                                                 (tyBoolean, "alpha", None, None, None)]),
("CopyTexSubImage2D",   None,           True,           "NOCL",                 True,           tyVoid,         [(tyEnum, "target", None, None, None),
                                                                                                                                                                                 (tyInt, "level", None, None, None),
                                                                                                                                                                                 (tyInt, "xoffset", None, None, None),
                                                                                                                                                                                 (tyInt, "yoffset", None, None, None),
                                                                                                                                                                                 (tyInt, "x", None, None, None),
                                                                                                                                                                                 (tyInt, "y", None, None, None),
                                                                                                                                                                                 (tySizei, "width", None, None, None),
                                                                                                                                                                                 (tySizei, "height", None, None, None)]),
("CullFace",            None,       True,       "Always",       True,       tyVoid,     [(tyEnum, "mode", None, None, None)]),
("DeleteLists",         None,       True,       "IgnoreCL",     True,       tyVoid,     [(tyUint, "list", None, None, None),
                                                                                                                                                                                 (tySizei, "range", None, None, None)]),
("DeleteTextures",              None,           True,           "NOCL",                 True,           tyVoid,         [(tySizei, "n", None, None, None),
                                                                                                                                                                                 (tyCPUint, "textures", None, None, None)]),
("DepthFunc",           None,       True,       "Always",       True,       tyVoid,     [(tyEnum, "func", None, None, None)]),
("DepthMask",           None,       True,       "Always",       True,       tyVoid,     [(tyBoolean, "flag", None, None, None)]),
("DepthRange",                  None,           True,           "Always",               True,           tyVoid,         [(tyClampd, "zNear", None, None, None),
                                                                                                                                                                                 (tyClampd, "zFar", None, None, None)]),
("Disable",             None,       True,       "Always",       True,       tyVoid,     [(tyEnum, "cap", None, None, None)]),
("DisableClientState",  None,       True,       "SpecialCL",     True,       tyVoid,     [(tyEnum, "cap", None, None, None)]),
("DrawArrays",          None,       True,       "InsteadCL",    True,       tyVoid,     [(tyEnum, "mode", None, None, None),
                                                                                                                                                                                 (tyInt, "first", None, None, None),
                                                                                                                                                                                 (tySizei, "count", None, None, None)]),
("DrawElements",        None,       True,       "Always",       True,       tyVoid,     [(tyEnum, "mode", None, None, None),
                                                                                                                                                                                 (tySizei, "count", None, None, None),
                                                                                                                                                                                 (tyEnum, "type", None, None, None),
                                                                                                                                                                                 (tyCPVoid, "indices", None, None, None)]),
("SetUpDrawSWR",        None,       False,      "Always",               True,       tyVoid,     [(tyEnum, "drawType", None, None, None)]),
("DrawSWR",             None,       False,      "Always",       False,      tyVoid,     []),
("Enable",              None,       True,       "Always",       True,       tyVoid,     [(tyEnum, "cap", None, None, None)]),
("EnableClientState",   None,       True,       "SpecialCL",    True,       tyVoid,     [(tyEnum, "cap", None, None, None)]),
("End",                 None,       True,       "SpecialCL",    False,      tyVoid,     []),
("EndList",             None,       True,       "IgnoreCL",     True,       tyVoid,     []),
("Fogf",                None,       True,       "Always",       True,       tyVoid,     [(tyEnum, "pname", None, None, None),
                                                                                         (tyFloat, "param", None, None,None)]),
("Fogi",                None,       True,       "Always",       True,       tyVoid,     [(tyEnum, "pname", None, None, None),
                                                                                         (tyInt, "param", None, None,None)]),
("Fogfv",               None,       True,       "Always",       True,       tyVoid,     [(tyEnum, "pname", None, None, None),
                                                                                         (tyCPFloat, "params", None, None, None)]),
("FrontFace",           None,       True,       "Always",       True,       tyVoid,     [(tyEnum, "mode", None, None, None)]),
("Frustum",             None,       True,       "Always",       True,       tyVoid,     [(tyDouble, "left", None, None, None),
                                                                                                                                                                                 (tyDouble, "right", None, None, None),
                                                                                                                                                                                 (tyDouble, "bottom", None, None, None),
                                                                                                                                                                                 (tyDouble, "top", None, None, None),
                                                                                                                                                                                 (tyDouble, "zNear", None, None, None),
                                                                                                                                                                                 (tyDouble, "zFar", None, None, None)]),
("GenLists",            None,       True,       "IgnoreCL",     True,       tyUint,     [(tySizei, "range", None, None, None)]),
("GenTextures",         None,       True,       "NOCL",         True,       tyVoid,     [(tySizei, "num", None, None, None),
                                                                                                                                                                                 (tyPUint, "textures", None, None, None)]),
("GetBooleanv",         None,       True,       "IgnoreCL",     True,       tyVoid,     [(tyEnum, "pname", None, None, None),
                                                                                                                                                                                 (tyPBoolean, "params", None, None, None)]),
("GetDoublev",          None,       True,       "IgnoreCL",     True,       tyVoid,     [(tyEnum, "pname", None, None, None),
                                                                                                                                                                                 (tyPDouble, "params", None, None, None)]),
("GetError",                    None,           True,           "Always",               True,           tyEnum,         []),
("GetFloatv",           None,       True,       "IgnoreCL",     True,       tyVoid,     [(tyEnum, "pname", None, None, None),
                                                                                                                                                                                 (tyPFloat, "params", None, None, None)]),
("GetIntegerv",         None,       True,       "IgnoreCL",     True,       tyVoid,     [(tyEnum, "pname", None, None, None),
                                                                                                                                                                                 (tyPInt, "params", None, None, None)]),
("GetLightfv",          None,       True,       "IgnoreCL",     True,       tyVoid,     [(tyEnum, "light", None, None, None),
                                                                                                                                                                                 (tyEnum, "pname", None, None, None),
                                                                                                                                                                                 (tyPFloat, "params", None, None, None)]),
("GetLightiv",          None,       True,       "IgnoreCL",     True,       tyVoid,     [(tyEnum, "light", None, None, None),
                                                                                                                                                                                 (tyEnum, "pname", None, None, None),
                                                                                                                                                                                 (tyPInt, "params", None, None, None)]),
# GetLight is handled below
("GetMaterialfv",       None,       True,       "IgnoreCL",     True,       tyVoid,     [(tyEnum, "face", None, None, None),
                                                                                                                                                                                 (tyEnum, "pname", None, None, None),
                                                                                                                                                                                 (tyPFloat, "params", None, None, None)]),
("GetMaterialiv",       None,       True,       "IgnoreCL",     True,       tyVoid,     [(tyEnum, "face", None, None, None),
                                                                                                                                                                                 (tyEnum, "pname", None, None, None),
                                                                                                                                                                                 (tyPInt, "params", None, None, None)]),
("GetString",           None,       True,       "IgnoreCL",       True,       tyCPUbyte,  [(tyEnum, "name", None, None, None)]),
("GetTexLevelParameterfv", None,        True,           "NOCL",                 True,           tyVoid,         [(tyEnum, "target", None, None, None),
                                                                                                                                                                                (tyInt, "level", None,  None,  None),
                                                                                                                                                                                (tyEnum, "pname", None,  None,  None),
                                                                                                                                                                                (tyPFloat, "params", None,  None,  None),]),
("GetTexLevelParameteriv", None,        True,           "NOCL",                 True,           tyVoid,         [(tyEnum, "target", None, None, None),
                                                                                                                                                                                (tyInt, "level", None,  None,  None),
                                                                                                                                                                                (tyEnum, "pname", None,  None,  None),
                                                                                                                                                                                (tyPInt, "params", None,  None,  None),]),
("Hint",                None,       True,       "Always",       True,       tyVoid,     [(tyEnum, "target", None, None, None),
                                                                                                                                                                                 (tyEnum, "mode", None, None, None)]),
("IsEnabled",           None,       True,       "IgnoreCL",     True,       tyBoolean,  [(tyEnum, "cap", None, None, None)]),
# LightModel is handled below
# Light is handled below
("LoadIdentity",        None,       True,       "Always",       True,       tyVoid,     []),
# LoadMatrix is handled below
# Material is handled below
("LockArraysEXT",       None,       True,       "Always",       True,       tyVoid,     [(tyInt, "first", None, None, None),
                                                                                                                                                                                 (tySizei, "count", None, None, None)]),
("SetLockedArraysSWR",  None,       False,      "Always",       True,       tyVoid,     [(tyInt, "lockedArrays", None, None, None)]),
("MatrixMode",          None,       True,       "Always",       True,       tyVoid,     [(tyEnum, "mode", None, None, None)]),
# MultMatrix is handled below
("NewList",             None,       True,       "NOCL",         True,       tyVoid,     [(tyUint, "list", None, None, None),
                                                                                                                                                                                 (tyEnum, "mode", None, None, None)]),
# Normal is handled belowbb
("NormalPointer",       None,       True,       "SpecialCL",    True,       tyVoid,     [(tyEnum, "type", None, None, None),
                                                                                         (tySizei, "stride", None, None, None),
                                                                                         (tyCPVoid, "pointer", None, None, None)]),
("NumVerticesSWR",      None,       False,      "Always",       False,      tyVoid,     [(tyUint, "num", None, None, None),
                                                                                                                                                                                 (tyULL, "count", None, None, None)]),
("Ortho",               None,       True,       "Always",       True,       tyVoid,     [(tyDouble, "left", None, None, None),
                                                                                                                                                                                 (tyDouble, "right", None, None, None),
                                                                                                                                                                                 (tyDouble, "bottom", None, None, None),
                                                                                                                                                                                 (tyDouble, "top", None, None, None),
                                                                                                                                                                                 (tyDouble, "zNear", None, None, None),
                                                                                                                                                                                 (tyDouble, "zFar", None, None, None)]),
("PixelStoref",                 None,           True,           "IgnoreCL",             True,           tyVoid,         [(tyEnum, "pname", None, None, None),
                                                                                                                                                                                 (tyFloat, "param", None, None, None)]),
("PixelStorei",                 None,           True,           "IgnoreCL",             True,           tyVoid,         [(tyEnum, "pname", None, None, None),
                                                                                                                                                                                 (tyInt, "param", None, None, None)]),
("PolygonMode",                 None,           True,           "Always",               True,           tyVoid,         [(tyEnum, "face", None, None, None),
                                                                                                                                                                                 (tyEnum, "mode", None, None, None)]),
("PopAttrib",           None,       True,       "Always",       True,       tyVoid,     []),
("PopMatrix",           None,       True,       "Always",       True,       tyVoid,     []),
("PushAttrib",          None,       True,       "Always",       True,       tyVoid,     [(tyBitfield, "mask", None, None, None)]),
("PushMatrix",          None,       True,       "Always",       True,       tyVoid,     []),
("ReadPixels",                  None,           True,           "NOCL",                 True,           tyVoid,         [(tyInt, "x", None, None, None),
                                                                                                                                                                                 (tyInt, "y", None, None, None),
                                                                                                                                                                                 (tySizei, "width", None, None, None),
                                                                                                                                                                                 (tySizei, "height", None, None, None),
                                                                                                                                                                                 (tyEnum, "format", None, None, None),
                                                                                                                                                                                 (tyEnum, "type", None, None, None),
                                                                                                                                                                                 (tyPVoid, "pixels", None, None, None)]),
("ResetMainVBSWR",      None,       True,       "Always",       True,       tyVoid,     []),
# Rotate is handled below
# Scale is handled below
("Scissor",                             None,           True,           "Always",               True,           tyVoid,         [(tyInt, "x", None, None, None),
                                                                                                                                                                                (tyInt, "y", None, None, None),
                                                                                                                                                                                (tySizei, "width", None, None, None),
                                                                                                                                                                                (tySizei, "height", None, None, None)]),
("SetClearMaskSWR",     None,       False,      "Always",       True,       tyVoid,     [(tyBitfield, "mask", None, None, None)]),
("SetTopologySWR",      None,       False,      "Always",       True,       tyVoid,     [(tyEnum, "topology", None, None, None)]),
("ShadeModel",          None,       True,       "Always",       True,       tyVoid,     [(tyEnum, "mode", None, None, None)]),
("SubstituteVBSWR",     None,       True,       "Always",       True,       tyVoid,     [(tySizei, "which", None, None, None)]),
# TexCoord is handled below
# TexEnv is handled below
("TexImage2D",          None,       True,       "Always",       True,       tyVoid,     [(tyEnum, "target", None, None, None),
                                                                                                                                                                                 (tyInt, "level", None, None, None),
                                                                                                                                                                                 (tyInt, "internalFormat", None, None, None),
                                                                                                                                                                                 (tySizei, "width", None, None, None),
                                                                                                                                                                                 (tySizei, "height", None, None, None),
                                                                                                                                                                                 (tyInt, "border", None, None, None),
                                                                                                                                                                                 (tyEnum, "format", None, None, None),
                                                                                                                                                                                 (tyEnum, "type", None, None, None),
                                                                                                                                                                                 (tyCPVoid, "data", None, None, None)]),
("TexSubImage2D",       None,       True,       "Always",       True,       tyVoid,     [(tyEnum, "target", None, None, None),
                                                                                                                                                                                 (tyInt, "level", None, None, None),
                                                                                                                                                                                 (tyInt, "xoffset", None, None, None),
                                                                                                                                                                                 (tyInt, "yoffset", None, None, None),
                                                                                                                                                                                 (tySizei, "width", None, None, None),
                                                                                                                                                                                 (tySizei, "height", None, None, None),
                                                                                                                                                                                 (tyEnum, "format", None, None, None),
                                                                                                                                                                                 (tyEnum, "type", None, None, None),
                                                                                                                                                                                 (tyCPVoid, "pixels", None, None, None)]),
# TexParameter is handled below
# Translate is handled below
("UnlockArraysEXT",     None,       True,       "Always",       True,       tyVoid,     []),
("Viewport",            None,       True,       "Always",       True,       tyVoid,     [(tyInt, "x", None, None, None),
                                                                                                                                                                                 (tyInt, "y", None, None, None),
                                                                                                                                                                                 (tySizei, "width", None, None, None),
                                                                                                                                                                                 (tySizei, "height", None, None, None)]),
# Vertex is handled below
# VertexPointer is handled below

# GL_ARB_vertex_buffer_object extension API
("BindBufferARB",               None,           True,           "NOCL",                 True,           tyVoid,         [(tyEnum, "target", None, None, None),
                                                                                                                                                                                 (tyUint, "buffer", None, None, None)]),
("DeleteBuffersARB",    None,           True,           "NOCL",                 True,           tyVoid,         [(tySizei, "n", None, None, None),
                                                                                                                                                                                 (tyCPUint, "buffers", None, None, None)]),
("GenBuffersARB",               None,           True,           "NOCL",                 True,           tyVoid,         [(tySizei, "n", None, None, None),
                                                                                                                                                                                 (tyPUint, "buffers", None, None, None)]),
("IsBufferARB",                 None,           True,           "NOCL",                 True,           tyBoolean,  [(tyUint, "buffer", None, None, None)]),
("BufferDataARB",               None,           True,           "NOCL",                 True,           tyVoid,         [(tyEnum, "target", None, None, None),
                                                                                                                                                                                 (tySizeIptrARB, "size", None, None, None),
                                                                                                                                                                                 (tyCPVoid, "data", None, None, None),
                                                                                                                                                                                 (tyEnum, "usage", None, None, None)]),
("BufferSubDataARB",    None,           True,           "NOCL",                 True,           tyVoid,         [(tyEnum, "target", None, None, None),
                                                                                                                                                                                 (tyIntPtrARB, "offset", None, None, None),
                                                                                                                                                                                 (tySizeIptrARB, "size", None, None, None),
                                                                                                                                                                                 (tyCPVoid, "data", None, None, None)]),
("MapBufferARB",                None,           True,           "NOCL",                 True,           tyPVoid,        [(tyEnum, "target", None, None, None),
                                                                                                                                                                                 (tyEnum, "access", None, None, None)]),
("UnmapBufferARB",              None,           True,           "NOCL",                 True,           tyBoolean,      [(tyEnum, "target", None, None, None)]),

("BindBuffer",          None,           True,           "NOCL",                 True,           tyVoid,         [(tyEnum, "target", None, None, None),
                                                                                                                                                                                 (tyUint, "buffer", None, None, None)]),
("DeleteBuffers",       None,           True,           "NOCL",                 True,           tyVoid,         [(tySizei, "n", None, None, None),
                                                                                                                                                                                 (tyCPUint, "buffers", None, None, None)]),
("GenBuffers",          None,           True,           "NOCL",                 True,           tyVoid,         [(tySizei, "n", None, None, None),
                                                                                                                                                                                 (tyPUint, "buffers", None, None, None)]),
("IsBuffer",                    None,           True,           "NOCL",                 True,           tyBoolean,  [(tyUint, "buffer", None, None, None)]),
("BufferData",          None,           True,           "NOCL",                 True,           tyVoid,         [(tyEnum, "target", None, None, None),
                                                                                                                                                                                 (tySizeIptr, "size", None, None, None),
                                                                                                                                                                                 (tyCPVoid, "data", None, None, None),
                                                                                                                                                                                 (tyEnum, "usage", None, None, None)]),
("BufferSubData",       None,           True,           "NOCL",                 True,           tyVoid,         [(tyEnum, "target", None, None, None),
                                                                                                                                                                                 (tyIntPtr, "offset", None, None, None),
                                                                                                                                                                                 (tySizeIptr, "size", None, None, None),
                                                                                                                                                                                 (tyCPVoid, "data", None, None, None)]),
("MapBuffer",           None,           True,           "NOCL",                 True,           tyPVoid,        [(tyEnum, "target", None, None, None),
                                                                                                                                                                                 (tyEnum, "access", None, None, None)]),
("UnmapBuffer",         None,           True,           "NOCL",                 True,           tyBoolean,      [(tyEnum, "target", None, None, None)])
]

glsl_functions = [
# GLSL
("CreateShader",                None,           True,           "NOCL",                 True,           tyUint,         [(tyEnum, "type", None, None, None)]),
("DeleteShader",                None,           True,           "NOCL",                 True,           tyVoid,         [(tyUint, "shader", None, None, None)]),
("ShaderSource",                None,           True,           "NOCL",                 True,           tyVoid,         [(tyUint, "shader", None, None, None),
                                                                                                                                                                                        (tySizei, "count", None, None, None),
                                                                                                                                                                                        (tyCPPChar, "string", None, None, None),
                                                                                                                                                                                        (tyCPInt, "length", None, None, None)]),
("CompileShader",               None,           True,           "NOCL",                 True,           tyVoid,         [(tyUint, "shader", None, None, None)]),
("AttachShader",                None,           True,           "NOCL",                 True,           tyVoid,         [(tyUint, "program", None, None, None),
                                                                                                                                                                                        (tyUint, "shader", None, None, None)]),
("DetachShader",                None,           True,           "NOCL",                 True,           tyVoid,         [(tyUint, "program", None, None, None),
                                                                                                                                                                                        (tyUint, "shader", None, None, None)]),
("GetShaderiv",                 None,           True,           "NOCL",                 True,           tyVoid,         [(tyUint, "shader", None, None, None),
                                                                                                                                                                                        (tyEnum, "pname", None, None, None),
                                                                                                                                                                                        (tyIntPtr, "params", None, None, None)]),
("GetShaderInfoLog",    None,           True,           "NOCL",                 True,           tyVoid,         [(tyUint, "shader", None, None, None),
                                                                                                                                                                                        (tySizei, "bufSize", None, None, None),
                                                                                                                                                                                        (tySizeIptr, "length", None, None, None),
                                                                                                                                                                                        (tyPChar, "infoLog", None, None, None)]),
("GetProgramiv",                None,           True,           "NOCL",                 True,           tyVoid,         [(tyUint, "program", None, None, None),
                                                                                                                                                                                        (tyEnum, "pname", None, None, None),
                                                                                                                                                                                        (tyIntPtr, "params", None, None, None)]),
("GetProgramInfoLog",   None,           True,           "NOCL",                 True,           tyVoid,         [(tyUint, "program", None, None, None),
                                                                                                                                                                                        (tySizei, "bufSize", None, None, None),
                                                                                                                                                                                        (tySizeIptr, "length", None, None, None),
                                                                                                                                                                                        (tyPChar, "infoLog", None, None, None)]),
("CreateProgram",               None,           True,           "NOCL",                 True,           tyUint,         []),
("DeleteProgram",               None,           True,           "NOCL",                 True,           tyVoid,         [(tyUint, "program", None, None, None)]),
("LinkProgram",                 None,           True,           "NOCL",                 True,           tyVoid,         [(tyUint, "program", None, None, None)]),
("UseProgram",                  None,           True,           "NOCL",                 True,           tyVoid,         [(tyUint, "program", None, None, None)]),
("GetAttribLocation",   None,           True,           "NOCL",                 True,           tyInt,          [(tyUint, "program", None, None, None),
                                                                                                                                                                                        (tyCPChar, "name", None, None, None)]),
("GetUniformLocation",  None,           True,           "NOCL",                 True,           tyInt,          [(tyUint, "program", None, None, None),
                                                                                                                                                                                        (tyCPChar, "name", None, None, None)]),
("VertexAttribPointer", None,           True,           "NOCL",                 True,           tyVoid,         [(tyUint, "index", None, None, None),
                                                                                                                                                                                        (tyEnum, "type", None, None, None),
                                                                                                                                                                                        (tyBoolean, "normalized", None, None, None),
                                                                                                                                                                                        (tySizei, "stride", None, None, None),
                                                                                                                                                                                        (tyCPVoid, "pointer", None, None, None)]),
("EnableVertexAttribArray",     None,   True,           "NOCL",                 True,           tyVoid,         [(tyUint, "index", None, None, None)]),
("DisableVertexAttribArray",    None,   True,   "NOCL",                 True,           tyVoid,         [(tyUint, "index", None, None, None)]),
# XXX - will need the full set generated, this is a development hook
("Uniform4fv",                  None,           True,           "NOCL",                 True,           tyVoid,         [(tyInt, "location", None, None, None),
                                                                                                                                                                                        (tySizei, "count", None, None, None),
                                                                                                                                                                                        (tyPFloat, "value", None, None, None)]),
]

# Color
names = ["red", "green", "blue", "alpha"]
functions.extend(
        [("Color%d%s"%(i,suf), "Color", True, "SpecialCL", False, tyVoid,
                [(ty if p < i else None, names[p] if p < i else 0, tyFloat, names[p], 'ConvertColorValue('+names[p]+')' if p < i else '1') for p in range(4)])
                for i in range(3,5)
                        for ty,suf in [(tyByte, "b"), (tyUbyte, "ub"), (tyShort, "s"), (tyUshort, "us"), (tyInt, "i"), (tyUint, "ui"), (tyFloat, "f"), (tyDouble, "d")]])
functions.extend(
        [("Color%d%s"%(i,suf), "Color", True, "SpecialCL", False, tyVoid,
                [(ty if p == 0 else None, "v" if p == 0 else None, tyFloat, names[p], "ConvertColorValue(v[%d])"%p if p < i else '1') for p in range(4)])
                for i in range(3,5)
                        for ty,suf in [(tyCPByte, "bv"), (tyCPUbyte, "ubv"), (tyCPShort, "sv"), (tyCPUshort, "usv"), (tyCPInt, "iv"), (tyCPUint, "uiv"), (tyCPFloat, "fv"), (tyCPDouble, "dv")]])

# LightModel
functions.extend(
        [("LightModel%s"%suf, "LightModel", True, "Always", True, tyVoid, [(tyEnum, "pname", None, None, None),
                                                                                                                                           (ty, "param", tyArrF4, "param", "ConvertLightModelArgs(pname, param)"),
                                                                                                                                           (None, None, "bool", "isScalar", num)])
                for ty,suf,num in [(tyInt, "i", "true"), (tyFloat, "f", "true"), (tyCPInt, "iv", "false"), (tyCPFloat, "fv", "false")]])

# Light, and Material
for name in ["Material", "Light"]:
        functions.extend(
                [("%s%s"%(name,suf), name, True, "Always", True, tyVoid, [(tyEnum, "name", None, None, None),
                                                                                                                                  (tyEnum, "pname", None, None, None),
                                                                                                                                  (ty, "param", tyArrF4, "param", "Convert%sArgs(name, pname, param)"%name),
                                                                                                                                  (None, None, "bool", "isScalar", num)])
                        for ty,suf,num in [(tyInt, "i", "true"), (tyFloat, "f", "true"), (tyCPInt, "iv", "false"), (tyCPFloat, "fv", "false")]])

# LoadMatrix
functions.extend(
        [("LoadMatrix%s"%suf, "LoadMatrix", True, "Always", True, tyVoid, [(ty, "m", tyCRM44F, "m", "ConvertMatrixArgs(m)")])
                for ty,suf in [(tyCPDouble, "d"), (tyCPFloat, "f")]])


# MultMatrix
functions.extend(
        [("MultMatrix%s"%suf, "MultMatrix", True, "Always", True, tyVoid, [(ty, "m", tyCRM44F, "m", "ConvertMatrixArgs(m)")])
                for ty,suf in [(tyCPDouble, "d"), (tyCPFloat, "f")]])

# Normal
functions.extend(
        [("Normal3%s"%suf, "Normal", True, "SpecialCL", False, tyVoid, [(ty, "x", tyFloat, "x", "(GLfloat)x"),
                                                                                                                                (ty, "y", tyFloat, "y", "(GLfloat)y"),
                                                                                                                                (ty, "z", tyFloat, "z", "(GLfloat)z")])
                for ty,suf in [(tyByte, "b"), (tyDouble, "d"), (tyFloat, "f"), (tyInt, "i"), (tyShort, "s")]])
functions.extend(
        [("Normal3%s"%suf, "Normal", True, "SpecialCL", False, tyVoid, [(ty, "v", tyFloat, "x", "(GLfloat)v[0]"),
                                                                                                                                (None, None, tyFloat, "y", "(GLfloat)v[1]"),
                                                                                                                                (None, None, tyFloat, "z", "(GLfloat)v[2]")])
                for ty,suf in [(tyCPByte, "bv"), (tyCPDouble, "dv"), (tyCPFloat, "fv"), (tyCPInt, "iv"), (tyCPShort, "sv")]])

# Rotate
for name in ["Rotate"]:
        functions.extend(
                [("%s%s"%(name,suf), name, True, "Always", True, tyVoid, [(ty, "angle", tyDouble, "angle", "angle"),
                                                                                                                                  (ty, "x", tyDouble, "x", "x"),
                                                                                                                                  (ty, "y", tyDouble, "y", "y"),
                                                                                                                                  (ty, "z", tyDouble, "z", "z")])
                        for ty,suf in [(tyDouble, "d"), (tyFloat, "f")]])

# TexCoord
names = ["s", "t", "r", "q"]
cases = ["0", "0", "0", "1"]
for g,name in enumerate(["MultiTexCoord", "TexCoord"]):
        functions.extend(
                [("%s%d%s"%(name,i,suf), "TexCoord", True, "SpecialCL", False, tyVoid,
                        [(None if g != 0 else tyEnum, None if g != 0 else "target", tyEnum, "target", "GL_TEXTURE0" if g != 0 else "target"),
                         (ty if 0 < i else None, "x" if 0 < i else None, tyFloat, "x", "(GLfloat)x"),
                         (ty if 1 < i else None, "y" if 1 < i else None, tyFloat, "y", "(GLfloat)y" if i > 1 else cases[1]),
                         (ty if 2 < i else None, "z" if 2 < i else None, tyFloat, "z", "(GLfloat)z" if i > 2 else cases[2]),
                         (ty if 3 < i else None, "w" if 3 < i else None, tyFloat, "w", "(GLfloat)w" if i > 3 else cases[3])])
                        for i in range(1,5)
                                for ty,suf in [(tyShort, "s"), (tyInt, "i"), (tyFloat, "f"), (tyDouble, "d")]])
        functions.extend(
                [("%s%d%s"%(name,i,suf), "TexCoord", True, "SpecialCL", False, tyVoid,
                        [(None if g != 0 else tyEnum, None if g != 0 else "target", tyEnum, "target", "GL_TEXTURE0" if g != 0 else "target"),
                         (ty, "v", tyFloat, "x", "(GLfloat)v[0]"),
                         (None, None, tyFloat, "y", "(GLfloat)v[1]" if i > 1 else cases[1]),
                         (None, None, tyFloat, "z", "(GLfloat)v[2]" if i > 2 else cases[2]),
                         (None, None, tyFloat, "w", "(GLfloat)v[3]" if i > 3 else cases[3])])
                for i in range(1,5)
                        for ty,suf in [(tyCPShort, "sv"), (tyCPInt, "iv"), (tyCPFloat, "fv"), (tyCPDouble, "dv")]])

# TexEnv
functions.extend(
        [("TexEnv%s"%suf, "TexEnv", True, "Always", True, tyVoid,
                [(tyEnum, "target", None, None, None),
                 (tyEnum, "pname", None, None, None),
                 (ty, "param", tyArrF4, "param", "ConvertTexEnvArgs(pname, param)")])
        for suf,ty in [("f", tyFloat), ("i", tyInt), ("fv", tyCPFloat), ("iv", tyCPInt)]])

# TexParameter
functions.extend(
        [("TexParameter%s"%suf, "TexParameter", True, "Always", True, tyVoid,
                [(tyEnum, "target", None, None, None),
                 (tyEnum, "pname", None, None, None),
                 (ty, "param", tyArrF4, "param", "ConvertTexParameterArgs(pname, param)")])
        for suf,ty in [("f", tyFloat), ("i", tyInt), ("fv", tyCPFloat), ("iv", tyCPInt)]])

# Scale, Translate
for name in ["Scale","Translate"]:
        functions.extend(
                [("%s%s"%(name,suf), name, True, "Always", True, tyVoid, [(ty, "x", tyDouble, "x", "x"),
                                                                                                                                  (ty, "y", tyDouble, "y", "y"),
                                                                                                                                  (ty, "z", tyDouble, "z", "z")])
                        for ty,suf in [(tyDouble, "d"), (tyFloat, "f")]])

# Vertex
names = ["x", "y", "z", "w"]
cases = ["0", "0", "0", "1"]
functions.extend(
        [("Vertex%d%s"%(i,suf), "Vertex", True, "SpecialCL", False, tyVoid,
                [(ty if p < i else None, names[p] if p < i else 0, tyFloat, names[p], "(GLfloat)"+names[p] if p < i else cases[p]) if p < 4 else (None, None, tyUint, "count", str(i)) for p in range(5)])
                for i in range(2,5)
                        for ty,suf in [(tyShort, "s"), (tyInt, "i"), (tyFloat, "f"), (tyDouble, "d")]])
functions.extend(
        [("Vertex%d%s"%(i,suf), "Vertex", True, "SpecialCL", False, tyVoid,
                [(ty if p == 0 else None, "v" if p == 0 else None, tyFloat, names[p], "(GLfloat)v[%d]"%p if p < i else cases[p]) if p < 4 else (None, None, tyUint, "count", str(i)) for p in range(5)])
                for i in range(2,5)
                        for ty,suf in [(tyCPShort, "sv"), (tyCPInt, "iv"), (tyCPFloat, "fv"), (tyCPDouble, "dv")]])

# VertexPointer
arrayTypes = ["Vertex", "Color", "TexCoord"]
functions.extend(
        [("%sPointer"%arrTy, None, True, "SpecialCL", True, tyVoid, [(tyInt, "size", None, None, None),
                                                                                                                   (tyEnum, "type", None, None, None),
                                                                                                                   (tySizei, "stride", None, None, None),
                                                                                                                   (tyCPVoid, "pointer", None, None, None)])
                for arrTy in arrayTypes])


gStubs = [
("void", "glAccum", ["GLenum op", "GLfloat value"]),
("void", "glActiveTexture", ["GLenum texture"]),
("void", "glAlphaFunc", ["GLenum func", "GLclampf ref"]),
("GLboolean", "glAreTexturesResident", ["GLsizei n", "const GLuint* textures", "GLboolean* residences"]),
("void", "glArrayElement", ["GLint i"]),
("void", "glBegin", ["GLenum mode"]),
("void", "glBindTexture", ["GLenum target", "GLuint texture"]),
("void", "glBitmap", ["GLsizei width", "GLsizei height", "GLfloat xorig", "GLfloat yorig", "GLfloat xmove", "GLfloat ymove", "const GLubyte* bitmap"]),
("void", "glBlendFunc", ["GLenum sfactor", "GLenum dfactor"]),
("void", "glBlendColor", ["GLclampf red", "GLclampf  green", "GLclampf  blue", "GLclampf  alpha",]),
("void", "glBlendEquation", ["GLenum  mode"]),
("void", "glCallList", ["GLuint list"]),
("void", "glCallLists", ["GLsizei n", "GLenum type", "const GLvoid* lists"]),
("void", "glClear", ["GLbitfield mask"]),
("void", "glClearAccum", ["GLfloat red", "GLfloat green", "GLfloat blue", "GLfloat alpha"]),
("void", "glClearColor", ["GLclampf red", "GLclampf green", "GLclampf blue", "GLclampf alpha"]),
("void", "glClearDepth", ["GLclampd depth"]),
("void", "glClearIndex", ["GLfloat c"]),
("void", "glClearStencil", ["GLint s"]),
("void", "glClientActiveTexture", ["GLenum texture"]),
("void", "glClipPlane", ["GLenum plane", "const GLdouble* equation"]),
("void", "glColor3b", ["GLbyte red", "GLbyte green", "GLbyte blue"]),
("void", "glColor3bv", ["const GLbyte* v"]),
("void", "glColor3d", ["GLdouble red", "GLdouble green", "GLdouble blue"]),
("void", "glColor3dv", ["const GLdouble* v"]),
("void", "glColor3f", ["GLfloat red", "GLfloat green", "GLfloat blue"]),
("void", "glColor3fv", ["const GLfloat* v"]),
("void", "glColor3i", ["GLint red", "GLint green", "GLint blue"]),
("void", "glColor3iv", ["const GLint* v"]),
("void", "glColor3s", ["GLshort red", "GLshort green", "GLshort blue"]),
("void", "glColor3sv", ["const GLshort* v"]),
("void", "glColor3ub", ["GLubyte red", "GLubyte green", "GLubyte blue"]),
("void", "glColor3ubv", ["const GLubyte* v"]),
("void", "glColor3ui", ["GLuint red", "GLuint green", "GLuint blue"]),
("void", "glColor3uiv", ["const GLuint* v"]),
("void", "glColor3us", ["GLushort red", "GLushort green", "GLushort blue"]),
("void", "glColor3usv", ["const GLushort* v"]),
("void", "glColor4b", ["GLbyte red", "GLbyte green", "GLbyte blue", "GLbyte alpha"]),
("void", "glColor4bv", ["const GLbyte* v"]),
("void", "glColor4d", ["GLdouble red", "GLdouble green", "GLdouble blue", "GLdouble alpha"]),
("void", "glColor4dv", ["const GLdouble* v"]),
("void", "glColor4f", ["GLfloat red", "GLfloat green", "GLfloat blue", "GLfloat alpha"]),
("void", "glColor4fv", ["const GLfloat* v"]),
("void", "glColor4i", ["GLint red", "GLint green", "GLint blue", "GLint alpha"]),
("void", "glColor4iv", ["const GLint* v"]),
("void", "glColor4s", ["GLshort red", "GLshort green", "GLshort blue", "GLshort alpha"]),
("void", "glColor4sv", ["const GLshort* v"]),
("void", "glColor4ub", ["GLubyte red", "GLubyte green", "GLubyte blue", "GLubyte alpha"]),
("void", "glColor4ubv", ["const GLubyte* v"]),
("void", "glColor4ui", ["GLuint red", "GLuint green", "GLuint blue", "GLuint alpha"]),
("void", "glColor4uiv", ["const GLuint* v"]),
("void", "glColor4us", ["GLushort red", "GLushort green", "GLushort blue", "GLushort alpha"]),
("void", "glColor4usv", ["const GLushort* v"]),
("void", "glColorMask", ["GLboolean red", "GLboolean green", "GLboolean blue", "GLboolean alpha"]),
("void", "glColorMaterial", ["GLenum face", "GLenum mode"]),
("void", "glColorPointer", ["GLint size", "GLenum type", "GLsizei stride", "const GLvoid* pointer"]),
("void", "glCompressedTexImage1D", ["GLenum     target", "GLint level", "GLenum         internalformat", "GLsizei               width", "GLint  border", "GLsizei               imageSize", "const GLvoid*              data"]),
("void", "glCompressedTexImage2D", ["GLenum     target", "GLint level", "GLenum         internalformat", "GLsizei               width", "GLsizei                height", "GLint border", "GLsizei               imageSize", "const GLvoid*              data"]),
("void", "glCompressedTexImage3D", ["GLenum     target", "GLint level", "GLenum         internalformat", "GLsizei               width", "GLsizei                height", "GLsizei depth", "GLint        border", "GLsizei               imageSize", "const GLvoid*              data"]),
("void", "glCompressedTexSubImage1D", ["GLenum          target", "GLint level", "GLint  xoffset", "GLsizei              width", "GLenum         format", "GLsizei               imageSize", "const GLvoid*              data"]),
("void", "glCompressedTexSubImage2D", ["GLenum          target", "GLint level", "GLint  xoffset", "GLint        yoffset", "GLsizei              width", "GLsizei                height", "GLenum                format", "GLsizei               imageSize", "const GLvoid*              data"]),
("void", "glCompressedTexSubImage3D", ["GLenum          target", "GLint level", "GLint  xoffset", "GLint        yoffset", "GLint        zoffset", "GLsizei              width", "GLsizei                height", "GLsizei               depth", "GLenum         format", "GLsizei               imageSize", "const GLvoid*              data"]),
("void", "glCopyPixels", ["GLint x", "GLint y", "GLsizei width", "GLsizei height", "GLenum type"]),
("void", "glCopyTexImage1D", ["GLenum target", "GLint level", "GLenum internalFormat", "GLint x", "GLint y", "GLsizei width", "GLint border"]),
("void", "glCopyTexImage2D", ["GLenum target", "GLint level", "GLenum internalFormat", "GLint x", "GLint y", "GLsizei width", "GLsizei height", "GLint border"]),
("void", "glCopyTexSubImage1D", ["GLenum target", "GLint level", "GLint xoffset", "GLint x", "GLint y", "GLsizei width"]),
("void", "glCopyTexSubImage2D", ["GLenum target", "GLint level", "GLint xoffset", "GLint yoffset", "GLint x", "GLint y", "GLsizei width", "GLsizei height"]),
("void", "glCopyTexSubImage3D", ["GLenum target", "GLint level", "GLint xoffset", "GLint yoffset", "GLint zoffset", "GLint x", "GLint y", "GLsizei width", "GLsizei height"]),
("void", "glCopyTexSubImage3DEXT", ["GLenum target", "GLint level", "GLint xoffset", "GLint yoffset", "GLint zoffset", "GLint x", "GLint y", "GLsizei width", "GLsizei height"]),
("void", "glCullFace", ["GLenum mode"]),
("void", "glDeleteLists", ["GLuint list", "GLsizei range"]),
("void", "glDeleteTextures", ["GLsizei n", "const GLuint* textures"]),
("void", "glDepthFunc", ["GLenum func"]),
("void", "glDepthMask", ["GLboolean flag"]),
("void", "glDepthRange", ["GLclampd zNear", "GLclampd zFar"]),
("void", "glDisable", ["GLenum cap"]),
("void", "glDisableClientState", ["GLenum array"]),
("void", "glDrawArrays", ["GLenum mode", "GLint first", "GLsizei count"]),
("void", "glDrawBuffer", ["GLenum mode"]),
("void", "glDrawElements", ["GLenum mode", "GLsizei count", "GLenum type", "const GLvoid* indices"]),
("void", "glDrawPixels", ["GLsizei width", "GLsizei height", "GLenum format", "GLenum type", "const GLvoid* pixels"]),
("void", "glDrawRangeElements", ["GLenum mode", "GLuint start", "GLuint end", "GLsizei count", "GLenum type", "const GLvoid * indices"]),
("void", "glEdgeFlag", ["GLboolean flag"]),
("void", "glEdgeFlagPointer", ["GLsizei stride", "const GLvoid* pointer"]),
("void", "glEdgeFlagv", ["const GLboolean* flag"]),
("void", "glEnable", ["GLenum cap"]),
("void", "glEnableClientState", ["GLenum array"]),
("void", "glEnd", []),
("void", "glEndList", []),
("void", "glEvalCoord1d", ["GLdouble u"]),
("void", "glEvalCoord1dv", ["const GLdouble* u"]),
("void", "glEvalCoord1f", ["GLfloat u"]),
("void", "glEvalCoord1fv", ["const GLfloat* u"]),
("void", "glEvalCoord2d", ["GLdouble u", "GLdouble v"]),
("void", "glEvalCoord2dv", ["const GLdouble* u"]),
("void", "glEvalCoord2f", ["GLfloat u", "GLfloat v"]),
("void", "glEvalCoord2fv", ["const GLfloat* u"]),
("void", "glEvalMesh1", ["GLenum mode", "GLint i1", "GLint i2"]),
("void", "glEvalMesh2", ["GLenum mode", "GLint i1", "GLint i2", "GLint j1", "GLint j2"]),
("void", "glEvalPoint1", ["GLint i"]),
("void", "glEvalPoint2", ["GLint i", "GLint j"]),
("void", "glFeedbackBuffer", ["GLsizei size", "GLenum type", "GLfloat* buffer"]),
("void", "glFinish", []),
("void", "glFlush", []),
("void", "glFogf", ["GLenum pname", "GLfloat param"]),
("void", "glFogfv", ["GLenum pname", "const GLfloat* params"]),
("void", "glFogi", ["GLenum pname", "GLint param"]),
("void", "glFogiv", ["GLenum pname", "const GLint* params"]),
("void", "glFrontFace", ["GLenum mode"]),
("void", "glFrustum", ["GLdouble left", "GLdouble right", "GLdouble bottom", "GLdouble top", "GLdouble zNear", "GLdouble zFar"]),
("GLuint", "glGenLists", ["GLsizei range"]),
("void", "glGenTextures", ["GLsizei n", "GLuint* textures"]),
("void", "glGetBooleanv", ["GLenum pname", "GLboolean* params"]),
("void", "glGetClipPlane", ["GLenum plane", "GLdouble* equation"]),
("void", "glGetCompressedTexImage", ["GLenum target", "GLint lod", "GLvoid* img"]),
("void", "glGetDoublev", ["GLenum pname", "GLdouble* params"]),
("GLenum", "glGetError", []),
("void", "glGetFloatv", ["GLenum pname", "GLfloat* params"]),
("void", "glGetIntegerv", ["GLenum pname", "GLint* params"]),
("void", "glGetLightfv", ["GLenum light", "GLenum pname", "GLfloat* params"]),
("void", "glGetLightiv", ["GLenum light", "GLenum pname", "GLint* params"]),
("void", "glGetMapdv", ["GLenum target", "GLenum query", "GLdouble* v"]),
("void", "glGetMapfv", ["GLenum target", "GLenum query", "GLfloat* v"]),
("void", "glGetMapiv", ["GLenum target", "GLenum query", "GLint* v"]),
("void", "glGetMaterialfv", ["GLenum face", "GLenum pname", "GLfloat* params"]),
("void", "glGetMaterialiv", ["GLenum face", "GLenum pname", "GLint* params"]),
("void", "glGetPixelMapfv", ["GLenum map", "GLfloat* values"]),
("void", "glGetPixelMapuiv", ["GLenum map", "GLuint* values"]),
("void", "glGetPixelMapusv", ["GLenum map", "GLushort* values"]),
("void", "glGetPointerv", ["GLenum pname", "GLvoid** params"]),
("void", "glGetPolygonStipple", ["GLubyte* mask"]),
("const GLubyte*", "glGetString", ["GLenum name"]),
("void", "glGetTexEnvfv", ["GLenum target", "GLenum pname", "GLfloat* params"]),
("void", "glGetTexEnviv", ["GLenum target", "GLenum pname", "GLint* params"]),
("void", "glGetTexGendv", ["GLenum coord", "GLenum pname", "GLdouble* params"]),
("void", "glGetTexGenfv", ["GLenum coord", "GLenum pname", "GLfloat* params"]),
("void", "glGetTexGeniv", ["GLenum coord", "GLenum pname", "GLint* params"]),
("void", "glGetTexImage", ["GLenum target", "GLint level", "GLenum format", "GLenum type", "GLvoid* pixels"]),
("void", "glGetTexLevelParameterfv", ["GLenum target", "GLint level", "GLenum pname", "GLfloat* params"]),
("void", "glGetTexLevelParameteriv", ["GLenum target", "GLint level", "GLenum pname", "GLint* params"]),
("void", "glGetTexParameterfv", ["GLenum target", "GLenum pname", "GLfloat* params"]),
("void", "glGetTexParameteriv", ["GLenum target", "GLenum pname", "GLint* params"]),
("void", "glHint", ["GLenum target", "GLenum mode"]),
("void", "glIndexMask", ["GLuint mask"]),
("void", "glIndexPointer", ["GLenum type", "GLsizei stride", "const GLvoid* pointer"]),
("void", "glIndexd", ["GLdouble c"]),
("void", "glIndexdv", ["const GLdouble* c"]),
("void", "glIndexf", ["GLfloat c"]),
("void", "glIndexfv", ["const GLfloat* c"]),
("void", "glIndexi", ["GLint c"]),
("void", "glIndexiv", ["const GLint* c"]),
("void", "glIndexs", ["GLshort c"]),
("void", "glIndexsv", ["const GLshort* c"]),
("void", "glIndexub", ["GLubyte c"]),
("void", "glIndexubv", ["const GLubyte* c"]),
("void", "glInitNames", []),
("void", "glInterleavedArrays", ["GLenum format", "GLsizei stride", "const GLvoid* pointer"]),
("GLboolean", "glIsEnabled", ["GLenum cap"]),
("GLboolean", "glIsList", ["GLuint list"]),
("GLboolean", "glIsTexture", ["GLuint texture"]),
("void", "glLightModelf", ["GLenum pname", "GLfloat param"]),
("void", "glLightModelfv", ["GLenum pname", "const GLfloat* params"]),
("void", "glLightModeli", ["GLenum pname", "GLint param"]),
("void", "glLightModeliv", ["GLenum pname", "const GLint* params"]),
("void", "glLightf", ["GLenum light", "GLenum pname", "GLfloat param"]),
("void", "glLightfv", ["GLenum light", "GLenum pname", "const GLfloat* params"]),
("void", "glLighti", ["GLenum light", "GLenum pname", "GLint param"]),
("void", "glLightiv", ["GLenum light", "GLenum pname", "const GLint* params"]),
("void", "glLineStipple", ["GLint factor", "GLushort pattern"]),
("void", "glLineWidth", ["GLfloat width"]),
("void", "glListBase", ["GLuint base"]),
("void", "glLoadIdentity", []),
("void", "glLoadMatrixd", ["const GLdouble* m"]),
("void", "glLoadMatrixf", ["const GLfloat* m"]),
("void", "glLoadName", ["GLuint name"]),
("void", "glLoadTransposeMatrixd", ["const GLdouble* m"]),
("void", "glLoadTransposeMatrixf", ["const GLfloat* m"]),
("void", "glLockArraysEXT", ["GLint first", "GLsizei count"]),
("void", "glLogicOp", ["GLenum opcode"]),
("void", "glMap1d", ["GLenum target", "GLdouble u1", "GLdouble u2", "GLint stride", "GLint order", "const GLdouble* points"]),
("void", "glMap1f", ["GLenum target", "GLfloat u1", "GLfloat u2", "GLint stride", "GLint order", "const GLfloat* points"]),
("void", "glMap2d", ["GLenum target", "GLdouble u1", "GLdouble u2", "GLint ustride", "GLint uorder", "GLdouble v1", "GLdouble v2", "GLint vstride", "GLint vorder", "const GLdouble* points"]),
("void", "glMap2f", ["GLenum target", "GLfloat u1", "GLfloat u2", "GLint ustride", "GLint uorder", "GLfloat v1", "GLfloat v2", "GLint vstride", "GLint vorder", "const GLfloat* points"]),
("void", "glMapGrid1d", ["GLint un", "GLdouble u1", "GLdouble u2"]),
("void", "glMapGrid1f", ["GLint un", "GLfloat u1", "GLfloat u2"]),
("void", "glMapGrid2d", ["GLint un", "GLdouble u1", "GLdouble u2", "GLint vn", "GLdouble v1", "GLdouble v2"]),
("void", "glMapGrid2f", ["GLint un", "GLfloat u1", "GLfloat u2", "GLint vn", "GLfloat v1", "GLfloat v2"]),
("void", "glMaterialf", ["GLenum face", "GLenum pname", "GLfloat param"]),
("void", "glMaterialfv", ["GLenum face", "GLenum pname", "const GLfloat* params"]),
("void", "glMateriali", ["GLenum face", "GLenum pname", "GLint param"]),
("void", "glMaterialiv", ["GLenum face", "GLenum pname", "const GLint* params"]),
("void", "glMatrixMode", ["GLenum mode"]),
("void", "glMultiTexCoord1f", ["GLenum target", "GLfloat x"]),
("void", "glMultiTexCoord1d", ["GLenum target", "GLdouble x"]),
("void", "glMultiTexCoord2f", ["GLenum target", "GLfloat x", "GLfloat y"]),
("void", "glMultiTexCoord2d", ["GLenum target", "GLdouble x", "GLdouble y"]),
("void", "glMultiTexCoord3f", ["GLenum target", "GLfloat x", "GLfloat y", "GLfloat z"]),
("void", "glMultiTexCoord3d", ["GLenum target", "GLdouble x", "GLdouble y", "GLdouble z"]),
("void", "glMultiTexCoord4f", ["GLenum target", "GLfloat x", "GLfloat y", "GLfloat z", "GLfloat w"]),
("void", "glMultiTexCoord4d", ["GLenum target", "GLdouble x", "GLdouble y", "GLdouble z", "GLdouble w"]),
("void", "glMultMatrixd", ["const GLdouble* m"]),
("void", "glMultMatrixf", ["const GLfloat* m"]),
("void", "glMultTransposeMatrixd", ["const GLdouble* m"]),
("void", "glMultTransposeMatrixf", ["const GLfloat* m"]),
("void", "glNewList", ["GLuint list", "GLenum mode"]),
("void", "glNormal3b", ["GLbyte nx", "GLbyte ny", "GLbyte nz"]),
("void", "glNormal3bv", ["const GLbyte* v"]),
("void", "glNormal3d", ["GLdouble nx", "GLdouble ny", "GLdouble nz"]),
("void", "glNormal3dv", ["const GLdouble* v"]),
("void", "glNormal3f", ["GLfloat nx", "GLfloat ny", "GLfloat nz"]),
("void", "glNormal3fv", ["const GLfloat* v"]),
("void", "glNormal3i", ["GLint nx", "GLint ny", "GLint nz"]),
("void", "glNormal3iv", ["const GLint* v"]),
("void", "glNormal3s", ["GLshort nx", "GLshort ny", "GLshort nz"]),
("void", "glNormal3sv", ["const GLshort* v"]),
("void", "glNormalPointer", ["GLenum type", "GLsizei stride", "const GLvoid* pointer"]),
("void", "glOrtho", ["GLdouble left", "GLdouble right", "GLdouble bottom", "GLdouble top", "GLdouble zNear", "GLdouble zFar"]),
("void", "glPassThrough", ["GLfloat token"]),
("void", "glPixelMapfv", ["GLenum map", "GLsizei mapsize", "const GLfloat* values"]),
("void", "glPixelMapuiv", ["GLenum map", "GLsizei mapsize", "const GLuint* values"]),
("void", "glPixelMapusv", ["GLenum map", "GLsizei mapsize", "const GLushort* values"]),
("void", "glPixelStoref", ["GLenum pname", "GLfloat param"]),
("void", "glPixelStorei", ["GLenum pname", "GLint param"]),
("void", "glPixelTransferf", ["GLenum pname", "GLfloat param"]),
("void", "glPixelTransferi", ["GLenum pname", "GLint param"]),
("void", "glPixelZoom", ["GLfloat xfactor", "GLfloat yfactor"]),
("void", "glPointSize", ["GLfloat size"]),
("void", "glPolygonMode", ["GLenum face", "GLenum mode"]),
("void", "glPolygonOffset", ["GLfloat factor", "GLfloat units"]),
("void", "glPolygonStipple", ["const GLubyte* mask"]),
("void", "glPopAttrib", []),
("void", "glPopClientAttrib", []),
("void", "glPopMatrix", []),
("void", "glPopName", []),
("void", "glPrioritizeTextures", ["GLsizei n", "const GLuint* textures", "const GLclampf* priorities"]),
("void", "glPushAttrib", ["GLbitfield mask"]),
("void", "glPushClientAttrib", ["GLbitfield mask"]),
("void", "glPushMatrix", []),
("void", "glPushName", ["GLuint name"]),
("void", "glRasterPos2d", ["GLdouble x", "GLdouble y"]),
("void", "glRasterPos2dv", ["const GLdouble* v"]),
("void", "glRasterPos2f", ["GLfloat x", "GLfloat y"]),
("void", "glRasterPos2fv", ["const GLfloat* v"]),
("void", "glRasterPos2i", ["GLint x", "GLint y"]),
("void", "glRasterPos2iv", ["const GLint* v"]),
("void", "glRasterPos2s", ["GLshort x", "GLshort y"]),
("void", "glRasterPos2sv", ["const GLshort* v"]),
("void", "glRasterPos3d", ["GLdouble x", "GLdouble y", "GLdouble z"]),
("void", "glRasterPos3dv", ["const GLdouble* v"]),
("void", "glRasterPos3f", ["GLfloat x", "GLfloat y", "GLfloat z"]),
("void", "glRasterPos3fv", ["const GLfloat* v"]),
("void", "glRasterPos3i", ["GLint x", "GLint y", "GLint z"]),
("void", "glRasterPos3iv", ["const GLint* v"]),
("void", "glRasterPos3s", ["GLshort x", "GLshort y", "GLshort z"]),
("void", "glRasterPos3sv", ["const GLshort* v"]),
("void", "glRasterPos4d", ["GLdouble x", "GLdouble y", "GLdouble z", "GLdouble w"]),
("void", "glRasterPos4dv", ["const GLdouble* v"]),
("void", "glRasterPos4f", ["GLfloat x", "GLfloat y", "GLfloat z", "GLfloat w"]),
("void", "glRasterPos4fv", ["const GLfloat* v"]),
("void", "glRasterPos4i", ["GLint x", "GLint y", "GLint z", "GLint w"]),
("void", "glRasterPos4iv", ["const GLint* v"]),
("void", "glRasterPos4s", ["GLshort x", "GLshort y", "GLshort z", "GLshort w"]),
("void", "glRasterPos4sv", ["const GLshort* v"]),
("void", "glReadBuffer", ["GLenum mode"]),
("void", "glReadPixels", ["GLint x", "GLint y", "GLsizei width", "GLsizei height", "GLenum format", "GLenum type", "GLvoid* pixels"]),
("void", "glRectd", ["GLdouble x1", "GLdouble y1", "GLdouble x2", "GLdouble y2"]),
("void", "glRectdv", ["const GLdouble* v1", "const GLdouble* v2"]),
("void", "glRectf", ["GLfloat x1", "GLfloat y1", "GLfloat x2", "GLfloat y2"]),
("void", "glRectfv", ["const GLfloat* v1", "const GLfloat* v2"]),
("void", "glRecti", ["GLint x1", "GLint y1", "GLint x2", "GLint y2"]),
("void", "glRectiv", ["const GLint* v1", "const GLint* v2"]),
("void", "glRects", ["GLshort x1", "GLshort y1", "GLshort x2", "GLshort y2"]),
("void", "glRectsv", ["const GLshort* v1", "const GLshort* v2"]),
("GLint", "glRenderMode", ["GLenum mode"]),
("void", "glRotated", ["GLdouble angle", "GLdouble x", "GLdouble y", "GLdouble z"]),
("void", "glRotatef", ["GLfloat angle", "GLfloat x", "GLfloat y", "GLfloat z"]),
("void", "glSampleCoverage", ["GLclampf value", "GLboolean invert"]),
("void", "glScaled", ["GLdouble x", "GLdouble y", "GLdouble z"]),
("void", "glScalef", ["GLfloat x", "GLfloat y", "GLfloat z"]),
("void", "glScissor", ["GLint x", "GLint y", "GLsizei width", "GLsizei height"]),
("void", "glSelectBuffer", ["GLsizei size", "GLuint* buffer"]),
("void", "glShadeModel", ["GLenum mode"]),
("void", "glStencilFunc", ["GLenum func", "GLint ref", "GLuint mask"]),
("void", "glStencilMask", ["GLuint mask"]),
("void", "glStencilOp", ["GLenum fail", "GLenum zfail", "GLenum zpass"]),
("void", "glTexCoord1d", ["GLdouble s"]),
("void", "glTexCoord1dv", ["const GLdouble* v"]),
("void", "glTexCoord1f", ["GLfloat s"]),
("void", "glTexCoord1fv", ["const GLfloat* v"]),
("void", "glTexCoord1i", ["GLint s"]),
("void", "glTexCoord1iv", ["const GLint* v"]),
("void", "glTexCoord1s", ["GLshort s"]),
("void", "glTexCoord1sv", ["const GLshort* v"]),
("void", "glTexCoord2d", ["GLdouble s", "GLdouble t"]),
("void", "glTexCoord2dv", ["const GLdouble* v"]),
("void", "glTexCoord2f", ["GLfloat s", "GLfloat t"]),
("void", "glTexCoord2fv", ["const GLfloat* v"]),
("void", "glTexCoord2i", ["GLint s", "GLint t"]),
("void", "glTexCoord2iv", ["const GLint* v"]),
("void", "glTexCoord2s", ["GLshort s", "GLshort t"]),
("void", "glTexCoord2sv", ["const GLshort* v"]),
("void", "glTexCoord3d", ["GLdouble s", "GLdouble t", "GLdouble r"]),
("void", "glTexCoord3dv", ["const GLdouble* v"]),
("void", "glTexCoord3f", ["GLfloat s", "GLfloat t", "GLfloat r"]),
("void", "glTexCoord3fv", ["const GLfloat* v"]),
("void", "glTexCoord3i", ["GLint s", "GLint t", "GLint r"]),
("void", "glTexCoord3iv", ["const GLint* v"]),
("void", "glTexCoord3s", ["GLshort s", "GLshort t", "GLshort r"]),
("void", "glTexCoord3sv", ["const GLshort* v"]),
("void", "glTexCoord4d", ["GLdouble s", "GLdouble t", "GLdouble r", "GLdouble q"]),
("void", "glTexCoord4dv", ["const GLdouble* v"]),
("void", "glTexCoord4f", ["GLfloat s", "GLfloat t", "GLfloat r", "GLfloat q"]),
("void", "glTexCoord4fv", ["const GLfloat* v"]),
("void", "glTexCoord4i", ["GLint s", "GLint t", "GLint r", "GLint q"]),
("void", "glTexCoord4iv", ["const GLint* v"]),
("void", "glTexCoord4s", ["GLshort s", "GLshort t", "GLshort r", "GLshort q"]),
("void", "glTexCoord4sv", ["const GLshort* v"]),
("void", "glTexCoordPointer", ["GLint size", "GLenum type", "GLsizei stride", "const GLvoid* pointer"]),
("void", "glTexEnvf", ["GLenum target", "GLenum pname", "GLfloat param"]),
("void", "glTexEnvfv", ["GLenum target", "GLenum pname", "const GLfloat* params"]),
("void", "glTexEnvi", ["GLenum target", "GLenum pname", "GLint param"]),
("void", "glTexEnviv", ["GLenum target", "GLenum pname", "const GLint* params"]),
("void", "glTexGend", ["GLenum coord", "GLenum pname", "GLdouble param"]),
("void", "glTexGendv", ["GLenum coord", "GLenum pname", "const GLdouble* params"]),
("void", "glTexGenf", ["GLenum coord", "GLenum pname", "GLfloat param"]),
("void", "glTexGenfv", ["GLenum coord", "GLenum pname", "const GLfloat* params"]),
("void", "glTexGeni", ["GLenum coord", "GLenum pname", "GLint param"]),
("void", "glTexGeniv", ["GLenum coord", "GLenum pname", "const GLint* params"]),
("void", "glTexImage1D", ["GLenum target", "GLint level", "GLint internalformat", "GLsizei width", "GLint border", "GLenum format", "GLenum type", "const GLvoid* pixels"]),
("void", "glTexImage2D", ["GLenum target", "GLint level", "GLint internalformat", "GLsizei width", "GLsizei height", "GLint border", "GLenum format", "GLenum type", "const GLvoid* pixels"]),
("void", "glTexImage3D", ["GLenum target", "GLint level", "GLint internalformat", "GLsizei width", "GLsizei height", "GLsizei depth", "GLint border", "GLenum format", "GLenum type", "const GLvoid* pixels"]),
("void", "glTexImage3DEXT", ["GLenum target", "GLint level", "GLint internalformat", "GLsizei width", "GLsizei height", "GLsizei depth", "GLint border", "GLenum format", "GLenum type", "const GLvoid* pixels"]),
("void", "glTexParameterf", ["GLenum target", "GLenum pname", "GLfloat param"]),
("void", "glTexParameterfv", ["GLenum target", "GLenum pname", "const GLfloat* params"]),
("void", "glTexParameteri", ["GLenum target", "GLenum pname", "GLint param"]),
("void", "glTexParameteriv", ["GLenum target", "GLenum pname", "const GLint* params"]),
("void", "glTexSubImage1D", ["GLenum target", "GLint level", "GLint xoffset", "GLsizei width", "GLenum format", "GLenum type", "const GLvoid* pixels"]),
("void", "glTexSubImage2D", ["GLenum target", "GLint level", "GLint xoffset", "GLint yoffset", "GLsizei width", "GLsizei height", "GLenum format", "GLenum type", "const GLvoid* pixels"]),
("void", "glTexSubImage3D", ["GLenum target", "GLint level", "GLint xoffset", "GLint yoffset", "GLint zoffset", "GLsizei width", "GLsizei height", "GLsizei depth", "GLenum format", "GLenum type", "const GLvoid* pixels"]),
("void", "glTexSubImage3DEXT", ["GLenum target", "GLint level", "GLint xoffset", "GLint yoffset", "GLint zoffset", "GLsizei width", "GLsizei height", "GLsizei depth", "GLenum format", "GLenum type", "const GLvoid* pixels"]),
("void", "glTranslated", ["GLdouble x", "GLdouble y", "GLdouble z"]),
("void", "glTranslatef", ["GLfloat x", "GLfloat y", "GLfloat z"]),
("void", "glUnlockArraysEXT", []),
("void", "glVertex2d", ["GLdouble x", "GLdouble y"]),
("void", "glVertex2dv", ["const GLdouble* v"]),
("void", "glVertex2f", ["GLfloat x", "GLfloat y"]),
("void", "glVertex2fv", ["const GLfloat* v"]),
("void", "glVertex2i", ["GLint x", "GLint y"]),
("void", "glVertex2iv", ["const GLint* v"]),
("void", "glVertex2s", ["GLshort x", "GLshort y"]),
("void", "glVertex2sv", ["const GLshort* v"]),
("void", "glVertex3d", ["GLdouble x", "GLdouble y", "GLdouble z"]),
("void", "glVertex3dv", ["const GLdouble* v"]),
("void", "glVertex3f", ["GLfloat x", "GLfloat y", "GLfloat z"]),
("void", "glVertex3fv", ["const GLfloat* v"]),
("void", "glVertex3i", ["GLint x", "GLint y", "GLint z"]),
("void", "glVertex3iv", ["const GLint* v"]),
("void", "glVertex3s", ["GLshort x", "GLshort y", "GLshort z"]),
("void", "glVertex3sv", ["const GLshort* v"]),
("void", "glVertex4d", ["GLdouble x", "GLdouble y", "GLdouble z", "GLdouble w"]),
("void", "glVertex4dv", ["const GLdouble* v"]),
("void", "glVertex4f", ["GLfloat x", "GLfloat y", "GLfloat z", "GLfloat w"]),
("void", "glVertex4fv", ["const GLfloat* v"]),
("void", "glVertex4i", ["GLint x", "GLint y", "GLint z", "GLint w"]),
("void", "glVertex4iv", ["const GLint* v"]),
("void", "glVertex4s", ["GLshort x", "GLshort y", "GLshort z", "GLshort w"]),
("void", "glVertex4sv", ["const GLshort* v"]),
("void", "glVertexPointer", ["GLint size", "GLenum type", "GLsizei stride", "const GLvoid* pointer"]),
("void", "glViewport", ["GLint x", "GLint y", "GLsizei width", "GLsizei height"]),
]

# dispatch table needed by Microsoft ICD
# order is important
gDispatch = [
"NewList",
"EndList",
"CallList",
"CallLists",
"DeleteLists",
"GenLists",
"ListBase",
"Begin",
"Bitmap",
"Color3b",
"Color3bv",
"Color3d",
"Color3dv",
"Color3f",
"Color3fv",
"Color3i",
"Color3iv",
"Color3s",
"Color3sv",
"Color3ub",
"Color3ubv",
"Color3ui",
"Color3uiv",
"Color3us",
"Color3usv",
"Color4b",
"Color4bv",
"Color4d",
"Color4dv",
"Color4f",
"Color4fv",
"Color4i",
"Color4iv",
"Color4s",
"Color4sv",
"Color4ub",
"Color4ubv",
"Color4ui",
"Color4uiv",
"Color4us",
"Color4usv",
"EdgeFlag",
"EdgeFlagv",
"End",
"Indexd",
"Indexdv",
"Indexf",
"Indexfv",
"Indexi",
"Indexiv",
"Indexs",
"Indexsv",
"Normal3b",
"Normal3bv",
"Normal3d",
"Normal3dv",
"Normal3f",
"Normal3fv",
"Normal3i",
"Normal3iv",
"Normal3s",
"Normal3sv",
"RasterPos2d",
"RasterPos2dv",
"RasterPos2f",
"RasterPos2fv",
"RasterPos2i",
"RasterPos2iv",
"RasterPos2s",
"RasterPos2sv",
"RasterPos3d",
"RasterPos3dv",
"RasterPos3f",
"RasterPos3fv",
"RasterPos3i",
"RasterPos3iv",
"RasterPos3s",
"RasterPos3sv",
"RasterPos4d",
"RasterPos4dv",
"RasterPos4f",
"RasterPos4fv",
"RasterPos4i",
"RasterPos4iv",
"RasterPos4s",
"RasterPos4sv",
"Rectd",
"Rectdv",
"Rectf",
"Rectfv",
"Recti",
"Rectiv",
"Rects",
"Rectsv",
"TexCoord1d",
"TexCoord1dv",
"TexCoord1f",
"TexCoord1fv",
"TexCoord1i",
"TexCoord1iv",
"TexCoord1s",
"TexCoord1sv",
"TexCoord2d",
"TexCoord2dv",
"TexCoord2f",
"TexCoord2fv",
"TexCoord2i",
"TexCoord2iv",
"TexCoord2s",
"TexCoord2sv",
"TexCoord3d",
"TexCoord3dv",
"TexCoord3f",
"TexCoord3fv",
"TexCoord3i",
"TexCoord3iv",
"TexCoord3s",
"TexCoord3sv",
"TexCoord4d",
"TexCoord4dv",
"TexCoord4f",
"TexCoord4fv",
"TexCoord4i",
"TexCoord4iv",
"TexCoord4s",
"TexCoord4sv",
"Vertex2d",
"Vertex2dv",
"Vertex2f",
"Vertex2fv",
"Vertex2i",
"Vertex2iv",
"Vertex2s",
"Vertex2sv",
"Vertex3d",
"Vertex3dv",
"Vertex3f",
"Vertex3fv",
"Vertex3i",
"Vertex3iv",
"Vertex3s",
"Vertex3sv",
"Vertex4d",
"Vertex4dv",
"Vertex4f",
"Vertex4fv",
"Vertex4i",
"Vertex4iv",
"Vertex4s",
"Vertex4sv",
"ClipPlane",
"ColorMaterial",
"CullFace",
"Fogf",
"Fogfv",
"Fogi",
"Fogiv",
"FrontFace",
"Hint",
"Lightf",
"Lightfv",
"Lighti",
"Lightiv",
"LightModelf",
"LightModelfv",
"LightModeli",
"LightModeliv",
"LineStipple",
"LineWidth",
"Materialf",
"Materialfv",
"Materiali",
"Materialiv",
"PointSize",
"PolygonMode",
"PolygonStipple",
"Scissor",
"ShadeModel",
"TexParameterf",
"TexParameterfv",
"TexParameteri",
"TexParameteriv",
"TexImage1D",
"TexImage2D",
"TexEnvf",
"TexEnvfv",
"TexEnvi",
"TexEnviv",
"TexGend",
"TexGendv",
"TexGenf",
"TexGenfv",
"TexGeni",
"TexGeniv",
"FeedbackBuffer",
"SelectBuffer",
"RenderMode",
"InitNames",
"LoadName",
"PassThrough",
"PopName",
"PushName",
"DrawBuffer",
"Clear",
"ClearAccum",
"ClearIndex",
"ClearColor",
"ClearStencil",
"ClearDepth",
"StencilMask",
"ColorMask",
"DepthMask",
"IndexMask",
"Accum",
"Disable",
"Enable",
"Finish",
"Flush",
"PopAttrib",
"PushAttrib",
"Map1d",
"Map1f",
"Map2d",
"Map2f",
"MapGrid1d",
"MapGrid1f",
"MapGrid2d",
"MapGrid2f",
"EvalCoord1d",
"EvalCoord1dv",
"EvalCoord1f",
"EvalCoord1fv",
"EvalCoord2d",
"EvalCoord2dv",
"EvalCoord2f",
"EvalCoord2fv",
"EvalMesh1",
"EvalPoint1",
"EvalMesh2",
"EvalPoint2",
"AlphaFunc",
"BlendFunc",
"LogicOp",
"StencilFunc",
"StencilOp",
"DepthFunc",
"PixelZoom",
"PixelTransferf",
"PixelTransferi",
"PixelStoref",
"PixelStorei",
"PixelMapfv",
"PixelMapuiv",
"PixelMapusv",
"ReadBuffer",
"CopyPixels",
"ReadPixels",
"DrawPixels",
"GetBooleanv",
"GetClipPlane",
"GetDoublev",
"GetError",
"GetFloatv",
"GetIntegerv",
"GetLightfv",
"GetLightiv",
"GetMapdv",
"GetMapfv",
"GetMapiv",
"GetMaterialfv",
"GetMaterialiv",
"GetPixelMapfv",
"GetPixelMapuiv",
"GetPixelMapusv",
"GetPolygonStipple",
"GetString",
"GetTexEnvfv",
"GetTexEnviv",
"GetTexGendv",
"GetTexGenfv",
"GetTexGeniv",
"GetTexImage",
"GetTexParameterfv",
"GetTexParameteriv",
"GetTexLevelParameterfv",
"GetTexLevelParameteriv",
"IsEnabled",
"IsList",
"DepthRange",
"Frustum",
"LoadIdentity",
"LoadMatrixf",
"LoadMatrixd",
"MatrixMode",
"MultMatrixf",
"MultMatrixd",
"Ortho",
"PopMatrix",
"PushMatrix",
"Rotated",
"Rotatef",
"Scaled",
"Scalef",
"Translated",
"Translatef",
"Viewport",
"ArrayElement",
"BindTexture",
"ColorPointer",
"DisableClientState",
"DrawArrays",
"DrawElements",
"EdgeFlagPointer",
"EnableClientState",
"IndexPointer",
"Indexub",
"Indexubv",
"InterleavedArrays",
"NormalPointer",
"PolygonOffset",
"TexCoordPointer",
"VertexPointer",
"AreTexturesResident",
"CopyTexImage1D",
"CopyTexImage2D",
"CopyTexSubImage1D",
"CopyTexSubImage2D",
"DeleteTextures",
"GenTextures",
"GetPointerv",
"IsTexture",
"PrioritizeTextures",
"TexSubImage1D",
"TexSubImage2D",
"PopClientAttrib",
"PushClientAttrib"
]

#
#-------------- END BIG GL DEFINITION TABLE ------------------
#

SV_STRUCTS = """struct BitProxy
{
        BitProxy() {}
        BitProxy(uint32_t* p, uint32_t o) : ptr(p), offset(o) {}

        uint32_t*  ptr;
        uint32_t   offset;

        inline bool operator*() { return (ptr[0] & (1 << offset)) != 0; }
        inline BitProxy& operator=(BitProxy& bp) { ptr = bp.ptr; offset = bp.offset; return *this; }
        inline BitProxy& operator=(bool b) { if (b) ptr[0] |= (1 << offset); else ptr[0] &= ~(1 << offset); return *this; }
};

template <GLenum field>
struct field_map
{
        typedef uint32_t* type;
        enum { offset = 0, suboffset = 0 };
};

template <typename T>
struct field_access_type
{
        typedef T* type;
};

template <>
struct field_access_type<bool>
{
        typedef BitProxy type;
};

template <GLenum field>
struct field_access_type<field_map<field>>
{
        typedef typename field_map<field>::type type;
};

template <typename T>
inline T make_field_accessor(GLuint* base, GLuint offset, GLuint suboffset, T tag)
{
        return (T)(base + offset);
}

inline BitProxy make_field_accessor(GLuint* base, GLuint offset, GLuint suboffset, BitProxy)
{
        return BitProxy(base + offset, suboffset);
}"""

autogenmsg  = "/* This file is procedurally generated by 'ogl_generate.py', do not modify. (C) 2012-2013 JNSmith, Intel. */"

externName  = 0
internName  = 1
exported    = 2
legality    = 3
autoGenFZ   = 4
rType       = 5
fParams     = 6
fpExtType   = 0
fpExtName   = 1
fpCallType  = 2
fpCallName  = 3
fpCallArg   = 4

def get_intern_name(fn):
        name = fn[externName]
        if fn[internName] is not None:
                name = fn[internName]
        return name

def get_intern_param_type(param):
        if param[fpCallType] is None: return param[fpExtType]
        return param[fpCallType]

def get_intern_param_name(param):
        if param[fpCallName] is None: return param[fpExtName]
        return param[fpCallName]

def get_intern_param_arg(param):
        if param[fpCallArg] is None: return get_intern_param_name(param)
        return param[fpCallArg]

def extract_internal_functions(fns, output_dir=os.path.dirname(__file__)):
        if not os.path.exists(output_dir):
                os.makedirs(output_dir)
        gen_files = []
        uniquefns = {}
        for fn in fns:
                uniquefns[get_intern_name(fn)] = fn
        keys    = sorted(uniquefns.keys())
        glimH   = []
        glimCpp = []    # stubs
        glstH   = []
        glclH   = []
        glclCpp = []
        glCmds  = []
        glCBs   = []
        glOPs   = []
        glfzH   = []
        glfzCpp = []
        glsxH   = []
        maxKeyLen = 0
        for key in keys:
                if maxKeyLen < len(key):
                        maxKeyLen = len(key)
        for key in keys:
                fn = uniquefns[key]

                glCmds.append("CMD_"+key.upper())
                finalTabStop = int((maxKeyLen + 9 + 3)/4)
                numTabStops = int((finalTabStop * 4 - 9 - len(key) + 3)/4)
                glCBs.append("\t\t\tcase CMD_"+key.upper()+"\t"*numTabStops + ": pCursor = executeCommand({1}, pCursor, &{0}"+key+"); break;")
                glOPs.append("\t\t\tcase CMD_"+key.upper()+" : pCursor = executeCommand({1}, pCursor, &{0}"+key+"); break;")

                params  = ["{1}"]
                params.extend([get_intern_param_type(param) + " " + get_intern_param_name(param) for param in fn[fParams]])
                szArgs  = ["sizeof(CMD_"+key.upper()+")"]
                szArgs.extend(["sizeof("+get_intern_param_name(param)+")" for param in fn[fParams]])
                insArgs = ["CMD_"+key.upper(), "mybuffer"]
                insArgs.extend(get_intern_param_name(param) for param in fn[fParams])
                actArgs = ["os"]
                actArgs.extend(insArgs[2:])

                fstr    = fn[rType] + " {0}" + get_intern_name(fn) + "(" + ", ".join(params) + "){2}"
                xstr    = "void {0}" + get_intern_name(fn) + "(" + ", ".join(params) + "){2}"

                glfzH.append(fstr.format("glfz", "OptState& os", ";"))

                glfzBody    = "\n{\n"
                glfzArgs    = ["*os.mpState"]
                glfzArgs.extend([get_intern_param_name(param) for param in fn[fParams]])
                glfzBody   += "\tglim"+get_intern_name(fn)+"("+", ".join(glfzArgs)+");\n"
                glfzArgs[0] = "os"
                glfzBody   += "\tglcl"+get_intern_name(fn)+"("+", ".join(glfzArgs)+");\n"
                if fn[rType] != tyVoid:
                        glfzBody   += "\treturn 0;\n"
                glfzBody   += "}"

                if fn[autoGenFZ]:
                        glfzCpp.append(fstr.format("glfz", "OptState& os", glfzBody))

                glimH.append(fstr.format("glim", "State&", ";"))
                glstH.append(fstr.format("glst", "State&", ";"))

                glclBody    = "\n{{\n"
                glclBody   += "\tGLubyte* mybuffer = os.mpDL->RequestBuffer(" + " + ".join(szArgs) + ");\n"
                glclBody   += "\tinsertCommand(" + ", ".join(insArgs) + ");\n"
                if fn[legality] in ["SpecialCL"]:
                        glclBody   += "\tglsx"+key+"("+", ".join(actArgs)+");\n"
                        glsxH.append(xstr.format("glsx", "OptState&", ";"))
                if fn[legality] in ["InsteadCL"]:
                        glclBody    = "\n{{\n\tglsx"+key+"("+", ".join(actArgs)+");\n"
                        glsxH.append(xstr.format("glsx", "OptState&", ";"))
                if fn[rType] != tyVoid:
                        glclBody   += "\treturn 0;\n"
                glclBody   += "}}"

                glclH.append(fstr.format("glcl", "OptState&", ";"))
                glclCpp.append(fstr.format("glcl", "OptState& os", glclBody.format("", "os")))

        of = "%s/glcmds.inl" % output_dir
        with open(of, "w") as fn:
                print(autogenmsg, file=fn)
                print(",\n".join(glCmds)+",", file=fn)
                gen_files.append(of)

        of = "%s/executedl.inl" % output_dir
        with open(of, "w") as fn:
                print(autogenmsg, file=fn)
                print("\n".join(glCBs).format("glim", "s"), file=fn)
                gen_files.append(of)

        of = "%s/generatedl.inl" % output_dir
        with open(of, "w") as fn:
                print(autogenmsg, file=fn)
                print("\n".join(glCBs).format("glcl", "s"), file=fn)
                gen_files.append(of)

        of = "%s/optimizedl.inl" % output_dir
        with open(of, "w") as fn:
                print(autogenmsg, file=fn)
                print("\n".join(glOPs).format("glfz", "os"), file=fn)
                gen_files.append(of)

        of = "%s/glfz.hpp" % output_dir
        with open(of, "w") as fn:
                print(autogenmsg, file=fn)
                print("#ifndef OGL_GLFZ_HPP", file=fn)
                print("#define OGL_GLFZ_HPP", file=fn)
                print("", file=fn)
                print('#include "oglstate.hpp"', file=fn)
                print('#include "ogldisplaylist.hpp"', file=fn)
                print("", file=fn)
                print("namespace OGL", file=fn)
                print("{", file=fn)
                print("\n".join(glfzH), file=fn)
                print("}", file=fn)
                print("", file=fn)
                print("#endif//OGL_GLFZ_HPP", file=fn)
                gen_files.append(of)

        of = "%s/glfz.inl" % output_dir
        with open(of, "w") as fn:
                print("\n\n".join(glfzCpp), file=fn)
                gen_files.append(of)

        of = "%s/glsx.hpp" % output_dir
        with open(of, "w") as fn:
                print(autogenmsg, file=fn)
                print("", file=fn)
                print("#ifndef OGL_GLSX_HPP", file=fn)
                print("#define OGL_GLSX_HPP", file=fn)
                print("", file=fn)
                print('#include "ogldisplaylist.hpp"', file=fn)
                print("", file=fn)
                print("namespace OGL", file=fn)
                print("{", file=fn)
                print("", file=fn)
                print("\n".join(glsxH), file=fn)
                print("", file=fn)
                print("}", file=fn)
                print("", file=fn)
                print("#endif//OGL_GLSX_HPP", file=fn)
                gen_files.append(of)

        for imms in [("glim.hpp", "GLIM", glimH), ("glst.hpp", "GLST", glstH)]:
                of = '%s/%s' % (output_dir, imms[0])
                with open(of, "w") as fn:
                        print(autogenmsg, file=fn)
                        print("", file=fn)
                        print("#ifndef OGL_%s_HPP"%imms[1], file=fn)
                        print("#define OGL_%s_HPP"%imms[1], file=fn)
                        print("", file=fn)
                        print('#include "oglstate.hpp"', file=fn)
                        print("", file=fn)
                        print("namespace OGL", file=fn)
                        print("{", file=fn)
                        print("", file=fn)
                        print("\n".join(imms[2]), file=fn)
                        print("", file=fn)
                        print("}", file=fn)
                        print("", file=fn)
                        print("#endif//OGL_%s_HPP"%imms[1], file=fn)
                        gen_files.append(of)

        of = "%s/glcl.cpp" % output_dir
        with open(of, "w") as fn:
                print(autogenmsg, file=fn)
                print("", file=fn)
                print('#include "glcl.hpp"', file=fn)
                print('#include "glsx.hpp"', file=fn)
                print('#include "serdeser.hpp"', file=fn)
                print("", file=fn)
                print("namespace OGL", file=fn)
                print("{", file=fn)
                print("", file=fn)
                print("\n\n".join(glclCpp), file=fn)
                print("", file=fn)
                print("}", file=fn)
                gen_files.append(of)

        of = "%s/glcl.hpp" % output_dir
        with open(of, "w") as fn:
                print(autogenmsg, file=fn)
                print("", file=fn)
                print("#ifndef OGL_GLCL_HPP", file=fn)
                print("#define OGL_GLCL_HPP", file=fn)
                print("", file=fn)
                print('#include "ogldisplaylist.hpp"', file=fn)
                print("", file=fn)
                print("namespace OGL", file=fn)
                print("{", file=fn)
                print("", file=fn)
                print("\n".join(glclH), file=fn)
                print("", file=fn)
                print("}", file=fn)
                print("", file=fn)
                print("#endif//OGL_GLCL_HPP", file=fn)
                gen_files.append(of)

        return gen_files

def extract_external_functions(fns, output_dir=os.path.dirname(__file__)):
        if not os.path.exists(output_dir):
                os.makedirs(output_dir)
        gen_files = []
        fns     = {}
        for fn in functions:
                fns[fn[externName]] = fn

        keys    = sorted(fns.keys())

        glH     = []
        glCpp   = []

        glEntryPoints = []
        for key in keys:
                fn      = fns[key]

                if not fn[exported]: continue

                fparams = fn[fParams]
                params  = [param[fpExtType] + " " + param[fpExtName] for param in fparams if param[fpExtType] is not None]

                fstr    = fn[rType] + " __stdcall gl" + fn[externName] + "(" + ", ".join(params) + "){0}"
                glEntryPoints.append("gl" + fn[externName])

                bdArgs  = ["{0}"]#get_intern_name(fn)]
                bdArgs.extend([get_intern_param_arg(param) for param in fparams])
                for idx, bdArg in enumerate(bdArgs[1:]):
                        if "GLenum" in get_intern_param_type(fparams[idx]):
                                bdArgs[idx+1] = "(OGL::EnumTy)"+bdArg
                        elif "GLbitfield" in get_intern_param_type(fparams[idx]):
                                bdArgs[idx+1] = "(OGL::BitfieldTy)"+bdArg

                glBody  = "\n{{\n\t{0}(" + ", ".join(bdArgs) + ");\n{1}}}"

                rVal    = ""
                if fn[rType] != tyVoid:
                        rVal    = "\treturn 0;\n"

                glBody          = "\n"
                glBody         += "{\n"
                glBody         += "    auto& s = GetOGL();\n"
                glBody         += "    OGL_TRACE(" + get_intern_name(fn) + ", " + (", ".join(bdArgs)).format("s") + ");\n"
                # Don't generate display list switch for functions that ignore display list state.
                if fn[legality] in ["IgnoreCL"]:
                        glBody     += "    return glim" + get_intern_name(fn) + "(" + (", ".join(bdArgs)).format("s") + ");\n"
                else:
                        glBody     += "    auto& d = CompilingDL();\n"
                        glBody     += "    OGL::OptState os = { &s, &d };\n"
                        glBody     += "    switch (s.mDisplayListMode)\n"
                        glBody     += "    {\n"
                        if fn[legality] in ["Always", "InsteadCL", "SpecialCL"]:
                                glBody += "    case GL_COMPILE             : return glcl" + get_intern_name(fn) + "(" + (", ".join(bdArgs)).format("os") + ");\n"
                                glBody += "    case GL_COMPILE_AND_EXECUTE : glcl" + get_intern_name(fn) + "(" + (", ".join(bdArgs)).format("os") + ");\n"
                        elif fn[legality] in ["IgnoreCL"]:
                                glBody += "    case GL_COMPILE             :\n"
                                glBody += "    case GL_COMPILE_AND_EXECUTE :\n"
                        elif fn[legality] in ["NOCL"]:
                                glBody += "    case GL_COMPILE             :\n"
                                glBody += "    case GL_COMPILE_AND_EXECUTE : s.mLastError = GL_INVALID_OPERATION; break;\n"
                        else:
                                raise "No legality determinable!"
                        glBody     += "    case GL_NONE                : return glim" + get_intern_name(fn) + "(" + (", ".join(bdArgs)).format("s") + ");\n"
                        glBody     += "    default                     : s.mLastError = GL_INVALID_OPERATION;\n"
                        glBody     += "    }\n"
                        if fn[rType] != 'void':
                                glBody += "    return (" + fn[rType] + ")0;\n"
                glBody         += "}\n"

                glH.append(fstr.format(";"))
                glCpp.append(fstr.format(glBody))

        of = "%s/gl.inl" % output_dir
        with open(of, "w") as fn:
                print(autogenmsg, file=fn)
                print("", file=fn)
                print("\n\n".join(glCpp), file=fn)
                gen_files.append(of)

        glEntryPoints = set(glEntryPoints)
        of = "%s/stubs.cpp" % output_dir
        with open(of, "w") as fn:
                glStubs = []
                for stub in gStubs:
                        if stub[1] not in glEntryPoints:
                                glFun = stub[0] + " __stdcall " + stub[1] + "(" + ", ".join(stub[2]) + ")\n{\n"
                                glFun += '\tASSERT_STUB(false && "' + stub[1] + '");\n'
                                bdArgs = ['GetOGL()']
                                for arg in stub[2]:
                                        param=arg.split()[-1]
                                        if "GLenum" in arg.split()[0]:
                                                param = "(OGL::EnumTy)"+param
                                        if "GLbitfield" in arg.split()[0]:
                                                param = "(OGL::BitfieldTy)"+param
                                        bdArgs += [param]
                                glFun += '\tOGL_TRACE('+stub[1][2:]+'(stub), ' + ", ".join(bdArgs) +');\n'
                                if "void" != stub[0]:
                                        glFun += "\treturn ("+stub[0]+")0;\n"
                                glFun += "}"
                                glStubs.append(glFun)
                print(autogenmsg, file=fn)
                print("", file=fn)
                print('#include "gl/gl.h"', file=fn)
                print('#include "oglglobals.h"', file=fn)
                print('#include "oglstate.hpp"', file=fn)
                print("#include <cassert>", file=fn)
                print("#include <map>", file=fn)
                print("#include <unordered_map>", file=fn)
                print("", file=fn)
                print("namespace OGL", file=fn)
                print('{', file=fn)
                print('#include "gltrace.inl"', file=fn)
                print('}', file=fn)
                print("", file=fn)
                print('extern "C"', file=fn)
                print('{', file=fn)
                print('', file=fn)
                print("\n\n".join(glStubs), file=fn)
                print('', file=fn)
                print('}', file=fn)
                gen_files.append(of)

        of = "%s/dispatch.h" % output_dir
        with open(of, "w") as fn:
                print(autogenmsg, file=fn)
                print("", file=fn)
                print("#include \"gl.h\"", file=fn)
                print("", file=fn)
                print("struct GLdispatch {", file=fn)
                print("    GLint numEntries;", file=fn)
                for entry in gDispatch:
                        print ("    void* " + entry + ";", file=fn)
                print("};", file=fn);

        of = "%s/dispatch.cpp" % output_dir
        with open(of, "w") as fn:
                print(autogenmsg, file=fn)
                print("", file=fn)
                print("#include \"dispatch.h\"", file=fn)
                print("#include \"glim.hpp\"", file=fn)
                print ("", file=fn)
                print("GLdispatch gDispatch = {", file=fn)
                print ("    " + str(len(gDispatch)) + ",", file=fn)
                for entry in gDispatch:
                        print ("    &gl" + entry + ",", file=fn)

                print("};", file=fn);

        # create def file for exported functions on windows
        if sys.platform == 'win32':
                of = "%s/driver.def" % output_dir
                with open(of, "w") as fn:
                        print("; This file is procedurally generated by 'generateogl.py', do not modify. (C) 2012-2013 Intel.", file=fn)
                        print("LIBRARY", file=fn)
                        print("EXPORTS", file=fn)
                        # WGL export
                        # print("  wglChoosePixelFormat", file=fn)
                        # print("  wglCopyContext", file=fn)
                        # print("  wglCreateContext", file=fn)
                        # print("  wglCreateLayerContext", file=fn)
                        # print("  wglDeleteContext", file=fn)
                        # print("  wglDescribeLayerPlane", file=fn)
                        # print("  wglDescribePixelFormat", file=fn)
                        # print("  wglGetCurrentContext", file=fn)
                        # print("  wglGetCurrentDC", file=fn)
                        # print("  wglGetLayerPaletteEntries", file=fn)
                        # print("  wglGetPixelFormat", file=fn)
                        # print("  wglGetProcAddress", file=fn)
                        # print("  wglMakeCurrent", file=fn)
                        # print("  wglRealizeLayerPalette", file=fn)
                        # print("  wglSetLayerPaletteEntries", file=fn)
                        # print("  wglSetPixelFormat", file=fn)
                        # print("  wglShareLists", file=fn)
                        # print("  wglSwapBuffers", file=fn)
                        # print("  wglSwapLayerBuffers", file=fn)
                        # print("  wglUseFontBitmapsA", file=fn)
                        # print("  wglUseFontBitmapsW", file=fn)
                        # print("  wglUseFontOutlinesA", file=fn)
                        # print("  wglUseFontOutlinesW", file=fn)

                        # WGL_EXT_extensions_string extension
                        # print("  wglGetExtensionsStringEXT", file=fn)

                        # WGL_ARB_extensions_string extension
                        # print("  wglGetExtensionsStringARB", file=fn)

                        # WGL_ARB_pixel_format extension
                        # print("  wglGetPixelFormatAttribivARB", file=fn)
                        # print("  wglGetPixelFormatAttribfvARB", file=fn)
                        # print("  wglChoosePixelFormatARB", file=fn)

                        # driver ogl exports
                        print("  DrvCopyContext", file=fn)
                        print("  DrvCreateContext", file=fn)
                        print("  DrvCreateLayerContext", file=fn)
                        print("  DrvDeleteContext", file=fn)
                        print("  DrvDescribeLayerPlane", file=fn)
                        print("  DrvDescribePixelFormat", file=fn)
                        print("  DrvGetLayerPaletteEntries", file=fn)
                        print("  DrvGetProcAddress", file=fn)
                        print("  DrvPresentBuffers", file=fn)
                        print("  DrvReleaseContext", file=fn)
                        print("  DrvRealizeLayerPalette", file=fn)
                        print("  DrvSetCallbackProcs", file=fn)
                        print("  DrvSetContext", file=fn)
                        print("  DrvSetLayerPaletteEntries", file=fn)
                        print("  DrvSetPixelFormat", file=fn)
                        print("  DrvShareLists", file=fn)
                        print("  DrvSwapBuffers", file=fn)
                        print("  DrvSwapLayerBuffers", file=fn)
                        print("  DrvValidateVersion", file=fn)
                        # additional exports
                        print("  SwapBuffers", file=fn)
                        # GL exports
                        for stub in gStubs:
                                print("  %s" % stub[1], file=fn)
                        # add to list of generated files
                        gen_files.append(of)

        return gen_files

def extract_state_vector(svector, output_dir=os.path.dirname(__file__)):
        if not os.path.exists(output_dir):
                os.makedirs(output_dir)
        gen_files = []

        typeSizeMap     = {
                tyBoolean       : 1,
                tyEnum          : 32,
                tyClampf        : 32,
                tyFloat         : 32,
                tyUint          : 32,
                tyInt           : 32,
                tySizei         : 64,
                tyPVoid         : 64,
                tyV4            : 128,
                tyM44           : 512,
        }

        Groups  = {}
        BitRefs = {}
        for idx,sdef in enumerate(svector):
                name            = sdef[0]
                group           = sdef[1]
                type            = sdef[2]
                cachelvl        = sdef[3]
                arrayLen        = sdef[4]
                cls                     = sdef[5]
                deflt           = sdef[6]
                if group is None:
                        if (typeSizeMap[type] == 1) and (arrayLen > 1):
                                group   = name
                        else:
                                group   = "_%03d"%typeSizeMap[type]
                if group not in Groups:
                        Groups[group] = {}
                Groups[group][idx]      = sdef

        locIndex                = 0
        with open('%s/oglstatevectortable.inl' % output_dir, 'w') as tbl, open('%s/oglstatevector.inl' % output_dir, 'w') as sv:
                gen_files = ['%s/oglstatevectortable.inl' % output_dir, '%s/oglstatevector.inl' % output_dir]
                grps    = list(Groups.keys())
                grps.sort()
                print("struct StateVector\n{", file=sv)
                print("/*            NAME                                                                  Local  Bits     Type      Abs  XXX: grab the value 'deflt' from below, and write the default value into the last column */", file=tbl)
                for grp in grps:
                        if (locIndex % 32) != 0:
                                locIndex        = int((locIndex+31)/32)*32
                        print("    struct %s_t\n    {"%grp, file=sv)
                        # see if every element in the group has the same array length...
                        allLength = 0
                        for elt in Groups[grp]:
                                entry           = Groups[grp][elt]
                                arrayLen        = entry[4]
                                type            = entry[2]
                                if allLength == 0:
                                        allLength = arrayLen
                                if allLength != arrayLen:
                                        allLength = -1
                                if type == tyBoolean:
                                        allLength = -1
                        for elt in Groups[grp]:
                                entry           = Groups[grp][elt]
                                name            = entry[0]
                                group           = entry[1]
                                type            = entry[2]
                                cachelvl        = entry[3]
                                arrayLen        = entry[4]
                                cls                     = entry[5]
                                deflt           = entry[6]
                                if (locIndex % typeSizeMap[type]) != 0:
                                        locIndex        = int((locIndex+typeSizeMap[type]-1)/typeSizeMap[type])*typeSizeMap[type]
                                if group is None:
                                        longName        = name.upper()
                                else:
                                        longName        = group.upper() + "_" + name.upper()
                                if typeSizeMap[type] > 1:
                                        if allLength <= 1:
                                                print("OGL_SV_ENTRY(%-63s % 8d, % 8d, %12s, offsetof(StateVector, %s) + offsetof(StateVector::%s_t, %s))"%(longName+",", 0, typeSizeMap[type], type, grp, grp, name), file = tbl)
                                        else:
                                                for idx in range(arrayLen):
                                                        print("OGL_SV_ENTRY(%-63s % 8d, % 8d, %12s, offsetof(StateVector, %s) + sizeof(StateVector::%s_t) * %d + offsetof(StateVector::%s_t, %s))"%("%s_%d,"%(longName, idx), 0, typeSizeMap[type], type, grp, grp, idx, grp, name), file = tbl)
                                else:
                                        print("OGL_SV_ENTRY(%-63s % 8d, % 8d, %12s, %d)"%("%s,"%longName, locIndex%32, typeSizeMap[type] * arrayLen, type, int(locIndex/32)), file = tbl)
                                if type == tyBoolean:
                                        print("        %-11s %-16s : %d;"%("GLuint",name,arrayLen), file=sv)
                                else:
                                        if (arrayLen == 1) or (allLength != -1):
                                                print("        %-11s %s;"%(type,name), file=sv)
                                        else:
                                                print("        %-11s %s[%d];"%(type,name,arrayLen), file=sv)
                                locIndex        += typeSizeMap[type] * arrayLen
                        if (allLength == -1) or (allLength == 1):
                                print("    } %s;"%grp, file=sv)
                        else:
                                print("    } %s[%d];"%(grp,allLength), file=sv)
                print("};\n", file=sv)

        return gen_files

def list_generated_files(output_dir):
        gen_files = [
                '%s/glcmds.inl' % output_dir,
                '%s/executedl.inl' % output_dir,
                '%s/generatedl.inl' % output_dir,
                '%s/optimizedl.inl' % output_dir,
                '%s/glfz.hpp' % output_dir,
                '%s/glfz.inl' % output_dir,
                '%s/glsx.hpp' % output_dir,
                '%s/glim.hpp' % output_dir,
                '%s/glst.hpp' % output_dir,
                '%s/glcl.cpp' % output_dir,
                '%s/glcl.hpp' % output_dir,
                '%s/gl.inl' % output_dir,
                '%s/stubs.cpp' % output_dir,
        ]
        if sys.platform == 'win32':
                gen_files.append('%s/driver.def' % output_dir)

        return gen_files

def main(args):
        parser = ArgumentParser(prog='<python32> ogl_generate.py', usage='%(prog)s [options]', formatter_class=RawTextHelpFormatter)
        
        parser.add_argument('--output-dir', action='store', dest='output_dir', metavar='PATH',
                help='output directory for the generated files\n[DEFAULT=%(default)s]', default=os.path.dirname(__file__))
        
        parser.add_argument('--verbose', '-v', action='store_true', dest='verbose',
                help='print generated file paths to stdout\n[DEFAULT=%(default)s]', default=False)

        parser.add_argument('--glsl', action='store_true', dest='glsl',
                help='enable generation of glsl API', default=False)
        
        options = parser.parse_args(args)

        if options.glsl:
                functions.extend(glsl_functions)
        
        gen_files = extract_internal_functions(functions, options.output_dir)
        gen_files += extract_external_functions(functions, options.output_dir)
        
        if sorted(gen_files) != sorted(list_generated_files(options.output_dir)):
                print("ALERT: Update the list of generated files as the output has changed!")
                if False:
                        aa = sorted(gen_files)
                        bb = sorted(list_generated_files(options.output_dir))
                        for a in aa: print(a)
                        for b in bb: print(b)

        if options.verbose:
                for f in sorted(gen_files): print(f)

if __name__ == '__main__':
        main(sys.argv[1:])
