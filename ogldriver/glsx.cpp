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

#include "glsx.hpp"
#include "glcl.hpp"
#include "glim.hpp"
#include "oglglobals.h"
#include "gldd.h"
#undef max
#undef min
#include <cmath>

#include "serdeser.hpp"

namespace OGL
{

AttributeFormat _glsxNumComponentsToFormat(GLuint nc)
{
    static const AttributeFormat formats[] =
        {
          OGL_R32_FLOAT,
          OGL_RG32_FLOAT,
          OGL_RGB32_FLOAT,
          OGL_RGBA32_FLOAT,
        };
    return formats[nc - 1];
}

void glsxArrayElement(OptState &os, GLint index)
{
    ++*(os.mpDL->mpNumVertices);
}

void glsxBegin(OptState &os, GLenum topology)
{
    GLubyte *buffer = os.mpDL->RequestBuffer(sizeof(COMMAND) + sizeof(GLuint) + sizeof(VertexAttributeFormats));
    ((COMMAND *)buffer)[0] = CMD_NUMVERTICESSWR;
    buffer += sizeof(COMMAND);

    os.mpDL->mpNumVertices = (GLuint *)buffer;

    ((GLuint *)buffer)[0] = 0;
    buffer += sizeof(GLuint);

    os.mpDL->mpAttrFormatsSeen = (VertexAttributeFormats *)buffer;

    ((VertexAttributeFormats *)buffer)[0].mask = 0;
    buffer += sizeof(VertexAttributeFormats);

    InitializeVertexAttrFmts(*(os.mpDL->mpAttrFormatsSeen));
    os.mpDL->mpAttrFormatsSeen->position = OGL_R32_FLOAT;

    // Position is the indicator.
    os.mpDL->mSafeAttributes.position = 0;
}

void glsxColor(OptState &os, GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
}

void glsxClientActiveTexture(OptState &os, GLenum texture)
{
    glimClientActiveTexture(*os.mpState, texture);
}

void glsxEnableClientState(OptState &os, GLenum array)
{
    glimEnableClientState(*os.mpState, array);
}

void glsxDisableClientState(OptState &os, GLenum array)
{
    glimDisableClientState(*os.mpState, array);
}

// for glDrawArrays within a display list, we need to store off any enabled client side data.
// we will store into a new VertexBuffer object store in the display list, then mark the array as locked.
// execution of glDrawArrays will pull the attribute data directly from the VB
void glsxDrawArrays(OptState &os, GLenum mode, GLint first, GLsizei count)
{
    if (count <= 0)
        return;

    os.mpDL->mHasDrawArray = 1;

    GLuint idx = os.mpDL->AddVB();
    auto oldVB = os.mpState->mpDrawingVB;

    os.mpState->mpDrawingVB = os.mpDL->mVertexBuffers[idx];

    // record switch to new VB
    glclSubstituteVBSWR(os, idx);

    // fill new VB using normal LockArrays path
    glimLockArraysEXT(*os.mpState, first, count);

    // record locked arrays
    glclSetLockedArraysSWR(os, os.mpState->mArraysLocked);

    // record DrawArrays
    GLubyte *mybuffer = os.mpDL->RequestBuffer(sizeof(CMD_DRAWARRAYS) + sizeof(mode) + sizeof(first) + sizeof(count));
    insertCommand(CMD_DRAWARRAYS, mybuffer, mode, first, count);

    // reset locked arrays state
    glimUnlockArraysEXT(*os.mpState);

    // record reset
    glclUnlockArraysEXT(os);

    // record switch back to main VB
    glclResetMainVBSWR(os);
    os.mpState->mpDrawingVB = oldVB;
}

void glsxDrawElements(OptState &os, GLenum mode, GLsizei count, GLenum type, const GLvoid *indices)
{
}

void glsxEnd(OptState &os)
{
    os.mpDL->mpNumVertices = 0;
}

void glsxNormal(OptState &os, GLfloat x, GLfloat y, GLfloat z)
{
}

void glsxTexCoord(OptState &os, GLenum target, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
}

void glsxVertex(OptState &os, GLfloat x, GLfloat y, GLfloat z, GLfloat w, GLuint count)
{
    ++*(os.mpDL->mpNumVertices);

    auto &vAttrFmts = *(os.mpDL->mpAttrFormatsSeen);
    vAttrFmts.position = std::max((AttributeFormat)vAttrFmts.position, _glsxNumComponentsToFormat(count));
}

void glsxColorPointer(OptState &os, GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
    glimColorPointer(*os.mpState, size, type, stride, pointer);
}

void glsxNormalPointer(OptState &os, GLenum type, GLsizei stride, const GLvoid *pointer)
{
    glimNormalPointer(*os.mpState, type, stride, pointer);
}

void glsxTexCoordPointer(OptState &os, GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
    glimTexCoordPointer(*os.mpState, size, type, stride, pointer);
}

void glsxVertexPointer(OptState &os, GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
    glimVertexPointer(*os.mpState, size, type, stride, pointer);
}
}
