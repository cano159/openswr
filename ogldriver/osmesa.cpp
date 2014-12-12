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

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include <gl/osmesa.h>

#include "oglglobals.h"
#include "oglstate.hpp"
#include "gldd.h"

/* Currently just the API used by VisIt 2.7.3, plus OSMesaFinishSWR */

static OSMesaContext gCurrentCtx = NULL;

struct osmesa_context
{
    OGL::State *pState;
    GLenum mFormat;
    HANDLE mRenderBuffer;
    HANDLE mDepthBuffer;
    void *pBuffer;
    GLenum mType;
    GLenum mWidth;
    GLenum mHeight;

    GLint mDepthBits;
    GLint mStencilBits;
    GLint mAccumBits;

    osmesa_context()
        : pState(NULL), mRenderBuffer(0), mDepthBuffer(0)
    {
    }
    ~osmesa_context()
    {
        free(pState);

        DDProcTable &procTable = OGL::GetDDProcTable();
        if (mRenderBuffer)
            procTable.pfnDestroyRenderTarget(OGL::GetDDHandle(), mRenderBuffer);
        if (mDepthBuffer)
            procTable.pfnDestroyRenderTarget(OGL::GetDDHandle(), mDepthBuffer);
    }
};

GLAPI OSMesaContext GLAPIENTRY
    OSMesaCreateContext(GLenum format, OSMesaContext sharelist)
{
    /* not handling sharing */
    assert(sharelist == NULL);

    osmesa_context *ctx = new osmesa_context();

    void *buf = _aligned_malloc(sizeof(OGL::State), KNOB_VS_SIMD_WIDTH * 4);
    memset(buf, 0, sizeof(OGL::State));
    ctx->pState = new (buf) OGL::State();
    OGL::Initialize(*(ctx->pState));

    ctx->mDepthBits = 32;

    return ctx;
}

GLAPI OSMesaContext GLAPIENTRY
    OSMesaCreateContextExt(GLenum format, GLint depthBits, GLint stencilBits,
                           GLint accumBits, OSMesaContext sharelist)
{
    /* no stencil or accum */
    assert(stencilBits == 0);
    assert(accumBits == 0);

    OSMesaContext ctx = OSMesaCreateContext(format, sharelist);
    ctx->mDepthBits = depthBits;
    return ctx;
}

GLAPI void GLAPIENTRY
    OSMesaDestroyContext(OSMesaContext ctx)
{
    OGL::Destroy(*(ctx->pState));
    ctx->pState->~State();
    _aligned_free(ctx->pState);
    delete ctx;
}

GLAPI GLboolean GLAPIENTRY
    OSMesaMakeCurrent(OSMesaContext ctx, void *buffer, GLenum type,
                      GLsizei width, GLsizei height)
{
    ctx->pBuffer = buffer;
    ctx->mType = type;
    ctx->mWidth = width;
    ctx->mHeight = height;

    DDProcTable &procTable = OGL::GetDDProcTable();

    if (ctx->mRenderBuffer)
        procTable.pfnDestroyRenderTarget(OGL::GetDDHandle(), ctx->mRenderBuffer);
    if (ctx->mDepthBuffer)
        procTable.pfnDestroyRenderTarget(OGL::GetDDHandle(), ctx->mDepthBuffer);

    ctx->mRenderBuffer = procTable.pfnCreateRenderTarget(OGL::GetDDHandle(),
                                                         width, height,
                                                         BGRA8_UNORM);
    if (ctx->mDepthBits)
        ctx->mDepthBuffer = procTable.pfnCreateRenderTarget(OGL::GetDDHandle(),
                                                            width, height,
                                                            R32_FLOAT);

    SetOGL(ctx->pState);

    procTable.pfnSetRenderTarget(OGL::GetDDHandle(),
                                 ctx->mRenderBuffer,
                                 ctx->mDepthBuffer);

    ctx->pState->mViewport.x = 0;
    ctx->pState->mViewport.y = 0;
    ctx->pState->mViewport.width = width;
    ctx->pState->mViewport.height = height;
    ctx->pState->mScissor.x = 0;
    ctx->pState->mScissor.y = 0;
    ctx->pState->mScissor.width = width;
    ctx->pState->mScissor.height = height;

    gCurrentCtx = ctx;

    return GL_TRUE;
}

GLAPI OSMesaContext GLAPIENTRY
    OSMesaGetCurrentContext(void)
{
    return gCurrentCtx;
}

GLAPI void GLAPIENTRY
    OSMesaGetIntegerv(GLint pname, GLint *value)
{
    *value = 0;

    switch (pname)
    {
    case OSMESA_ROW_LENGTH:
    case OSMESA_WIDTH:
        if (gCurrentCtx)
            *value = gCurrentCtx->mWidth;
        break;
    case OSMESA_HEIGHT:
        if (gCurrentCtx)
            *value = gCurrentCtx->mHeight;
        break;
    case OSMESA_FORMAT:
        if (gCurrentCtx)
            *value = gCurrentCtx->mFormat;
        break;
    case OSMESA_TYPE:
        if (gCurrentCtx)
            *value = gCurrentCtx->mType;
        break;
    case OSMESA_MAX_WIDTH:
    case OSMESA_MAX_HEIGHT:
        *value = 2048;
        break;
    default:
        break;
    }
}

GLAPI OSMESAproc GLAPIENTRY
    OSMesaGetProcAddress(const char *funcName)
{
#if _WIN32
#ifdef _UNICODE
    return (OSMESAproc)GetProcAddress(GetModuleHandle(L"opengl32"), funcName);
#else
    return (OSMESAproc)GetProcAddress(GetModuleHandle("opengl32"), funcName);
#endif // _UNICODE
#else
    static void *handle = dlopen("libGL.so.1", RTLD_LOCAL | RTLD_LAZY);
    void *procAddr = dlsym(handle, (const char *)funcName);
    return (OSMESAproc)procAddr;
#endif
}

GLAPI void GLAPIENTRY
    OSMesaFinishSWR(void)
{
    if (!gCurrentCtx)
        return;

    OGL::GetDDProcTable().pfnPresent2(OGL::GetDDHandle(),
                                      gCurrentCtx->pBuffer,
                                      gCurrentCtx->mWidth * 4);
}
