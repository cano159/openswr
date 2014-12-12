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

#include "glcl.hpp"
#include "glim.hpp"
#include "glfz.hpp"
#include "oglglobals.h"
#include "gldd.h"

#include <cmath>

namespace OGL
{

// XXX: all the push/pop matrix calls need to be in here.

void glfzArrayElement(OptState &os, GLint index)
{
    glimAddVertexSWR(*os.mpState, 0, 0, 0, 0, index, false, (GLenum)OGL_NO_INDEX_BUFFER_GENERATION);
}

void glfzBegin(OptState &os, GLenum topology)
{
    // create new VB for this glBegin/glEnd pair
    GLuint idx = os.mpDL->AddVB();
    glimSubstituteVBSWR(*os.mpState, idx);
    glclSubstituteVBSWR(os, idx);

    glimSetTopologySWR(*os.mpState, topology);
    glclSetTopologySWR(os, topology);
    AnalyzeVertexAttributes(*os.mpState);
    RecordState(os, *os.mpDL);
    GetDDProcTable().pfnNewDraw(GetDDHandle());
    glclSetUpDrawSWR(os, GL_NONE);
}

void glfzClear(OptState &os, GLbitfield mask)
{
    glimSetClearMaskSWR(*os.mpState, mask);
    RecordState(os, *os.mpDL);
    glclClear(os, mask);
}

void glfzColor(OptState &os, GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
    glimColor(*os.mpState, red, green, blue, alpha);
}

void glfzDrawSWR(OptState &os)
{
    glclDrawSWR(os);
}

void glfzEnd(OptState &os)
{
    glfzDrawSWR(os);

    // reset to main VB
    glclResetMainVBSWR(os);
    glimResetMainVBSWR(*os.mpState);
}

void glfzNormal(OptState &os, GLfloat x, GLfloat y, GLfloat z)
{
    glimNormal(*os.mpState, x, y, z);
}

void glfzNumVerticesSWR(OptState &os, GLuint num, VertexAttributeFormats vAttrFmts)
{
    State &s = *os.mpState;
    AnalyzeVertexAttributes(s);

    if (num > 0)
    {
        // XXX: adding padding to help in SWR.
        num += KNOB_VS_SIMD_WIDTH * 2;
        for (GLuint i = 0; i < GetVB(s).mNumAttributes; ++i)
        {
            GetVB(s).getBuffer(i, num * sizeof(GLfloat) * 4);
            GetVB(s).mpfBuffers[i] = (GLfloat *)GetDDProcTable().pfnLockBufferDiscard(GetDDHandle(), GetVB(s).mhBuffers[i]);
        }
        for (GLuint i = 0; i < GetVB(s).mNumSideAttributes; ++i)
        {
            GetVB(s).mhSideBuffers[i] = GetDDProcTable().pfnCreateBuffer(GetDDHandle(), num * sizeof(GLfloat) * 4, NULL);
            GetVB(s).mpfSideBuffers[i] = (GLfloat *)GetDDProcTable().pfnLockBufferDiscard(GetDDHandle(), GetVB(s).mhSideBuffers[i]);
        }
        GetVB(s).mhNIB8 = GetDDProcTable().pfnCreateBuffer(GetDDHandle(), num * sizeof(GLubyte), NULL);
        GetVB(s).mpNIB8 = (GLubyte *)GetDDProcTable().pfnLockBufferDiscard(GetDDHandle(), GetVB(s).mhNIB8);
        GetVB(s).mNumVertices = 0;
    }

    GetVB(s).mAttrFormats = vAttrFmts;
}

void glfzTexCoord(OptState &os, GLenum target, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
    glimTexCoord(*os.mpState, target, x, y, z, w);
}

void glfzVertex(OptState &os, GLfloat x, GLfloat y, GLfloat z, GLfloat w, GLuint count)
{
    glimAddVertexSWR(*os.mpState, x, y, z, w, -1, false, (GLenum)OGL_NO_INDEX_BUFFER_GENERATION);
}

#include "glfz.inl"
}
