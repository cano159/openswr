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

#define _GDI32_

#undef APIENTRY
#include <windows.h>
#undef min
#undef max
#undef APIENTRY

#include "gl.h"
#include "wglext.h"

#include "dispatch.h"
#include "oglglobals.h"
#include "oglstate.hpp"
#include "gldd.h"

#include <map>

namespace OGL
{
#include "gltrace.inl"
}

extern GLdispatch gDispatch;

typedef void *DHGLRC;

static int ghglrc = 1;

struct SWRContext
{
    HDC hdc;
    bool initialized;
    OGL::State *pState;
};

typedef std::unordered_map<int, SWRContext> Contexts;

Contexts DummyContexts()
{
    Contexts ctxts;
    SWRContext c = { 0 };

    ctxts.insert(std::make_pair(0, (SWRContext)c));
    return ctxts;
}

static Contexts gContexts = DummyContexts();
static bool gCurrentContextSet = false;
static Contexts::iterator gCurrentContext = gContexts.end();

// We support ONE format.

static const PIXELFORMATDESCRIPTOR gPxFmts[] =
    {
      { // This is a dummy descriptor.
      },
      {
        sizeof(PIXELFORMATDESCRIPTOR), // nSize
        0xBAAB,                        // nVersion
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_GENERIC_ACCELERATED | PFD_DOUBLEBUFFER | PFD_SWAP_EXCHANGE | PFD_SUPPORT_COMPOSITION,
        PFD_TYPE_RGBA, // iPixelType
        32,            // cColorBits
        8,             // cRedBits
        16,            // cRedShift
        8,             // cGreenBits
        8,             // cGreenShift
        8,             // cBlueBits
        0,             // cBlueShift
        8,             // cAlphaBits
        24,            // cAlphaShift
        0,             // cAccumBits
        0,             // cAccumRedBits
        0,             // cAccumGreenBits
        0,             // cAccumBlueBits
        0,             // cAccumAlphaBits
        32,            // cDepthBits
        0,             // cStencilBits
        0,             // cAuxBuffers
        0,             // iLayerType
        0,             // bReserved
        0,             // dwLayerMask
        0,             // dwVisibleMask
        0,             // dwDamageMask
      }
    };
static const GLuint gNumPxFmts = sizeof(gPxFmts) / sizeof(gPxFmts[0]) - 1; // do not include reserved 0 format
static GLuint gCurrentPxFmt = 0;

WINGDIAPI int WINAPI wglChoosePixelFormat(HDC hdc, CONST PIXELFORMATDESCRIPTOR *ppfd)
{
    //return memcmp(ppfd, &gPxFmts[1], sizeof(PIXELFORMATDESCRIPTOR)) == 0 ? 1 : 0;
    // only support 32bit ARGB with 32bit depth
    if (ppfd->cColorBits == 32 &&
        (ppfd->cDepthBits == 32 || ppfd->cDepthBits == 24))
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

WINGDIAPI BOOL WINAPI wglCopyContext(HGLRC, HGLRC, UINT)
{
    return FALSE;
}

WINGDIAPI BOOL WINAPI wglDescribeLayerPlane(HDC, int, int, UINT, LPLAYERPLANEDESCRIPTOR)
{
    return FALSE;
}

WINGDIAPI int WINAPI wglDescribePixelFormat(HDC hdc, int pixelFormat, UINT numBytes, LPPIXELFORMATDESCRIPTOR ppfd)
{
    if (ppfd && (numBytes == sizeof(PIXELFORMATDESCRIPTOR)) && (pixelFormat <= gNumPxFmts))
    {
        memcpy(ppfd, &gPxFmts[pixelFormat], sizeof(PIXELFORMATDESCRIPTOR));
    }
    return gNumPxFmts;
}

WINGDIAPI HGLRC WINAPI wglGetCurrentContext(VOID)
{
    if (!gCurrentContextSet)
    {
        gCurrentContext = gContexts.end();
        gCurrentContextSet = true;
    }

    if (gCurrentContext == gContexts.end())
    {
        return NULL;
    }
    else
    {
        return reinterpret_cast<HGLRC>(gCurrentContext->first);
    }
}

WINGDIAPI HDC WINAPI wglGetCurrentDC(VOID)
{
    if (!gCurrentContextSet)
    {
        gCurrentContext = gContexts.end();
        gCurrentContextSet = true;
    }

    if (gCurrentContext == gContexts.end())
    {
        return NULL;
    }
    else
    {
        return gCurrentContext->second.hdc;
    }
}

WINGDIAPI int WINAPI wglGetLayerPaletteEntries(HDC, int, int, int, COLORREF *)
{
    return 0;
}

WINGDIAPI int WINAPI wglGetPixelFormat(HDC hdc)
{
    return gCurrentPxFmt;
}

WINGDIAPI PROC WINAPI wglGetProcAddress(LPCSTR address)
{
#ifdef _UNICODE
    return GetProcAddress(GetModuleHandle(L"opengl32"), address);
#else
    return GetProcAddress(GetModuleHandle("opengl32"), address);
#endif // _UNICODE
}

WINGDIAPI const char *wglGetExtensionsStringEXT()
{
    return "WGL_EXT_extensions_string WGL_ARB_extensions_string WGL_ARB_pixel_format";
}

WINGDIAPI const char *wglGetExtensionsStringARB()
{
    return wglGetExtensionsStringEXT();
}

WINGDIAPI BOOL WINAPI wglMakeCurrent(HDC hdc, DHGLRC hglrc)
{
    if (!gCurrentContextSet)
    {
        gCurrentContext = gContexts.end();
        gCurrentContextSet = true;
    }

    if (!hdc || !hglrc)
    {
        gCurrentContext = gContexts.end();
        return 1;
    }

    int rc = (int)hglrc;

    if (gContexts.find(rc) == gCurrentContext)
    {
        return 1;
    }

    gCurrentContext = gContexts.find(rc);

    auto &xCtxt = gContexts[rc];
    OGL::State *pState = xCtxt.pState;

    HWND hWnd = WindowFromDC(hdc);
    RECT rect;
    GetClientRect(hWnd, &rect);

    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;

    SetOGL(pState);
    OGL::GetDDProcTable().pfnBindContext(OGL::GetDDHandle(), NULL, hWnd, NULL, width, height);

    // according to spec, only set current viewport to draw buffer dimensions if the context hasn't
    // been initialized
    if (!xCtxt.initialized)
    {
        pState->mViewport.x = 0;
        pState->mViewport.y = 0;
        pState->mViewport.width = width;
        pState->mViewport.height = height;
        pState->mScissor.x = 0;
        pState->mScissor.y = 0;
        pState->mScissor.width = width;
        pState->mScissor.height = height;
        xCtxt.initialized = true;
    }
    return 1;
}

WINGDIAPI BOOL WINAPI wglRealizeLayerPalette(HDC, int, BOOL)
{
    return FALSE;
}

WINGDIAPI int WINAPI wglSetLayerPaletteEntries(HDC, int, int, int, CONST COLORREF *)
{
    return FALSE;
}

WINGDIAPI BOOL WINAPI wglSetPixelFormat(HDC hdc, int pixelFormat, const PIXELFORMATDESCRIPTOR *ppfd)
{
    // only support the one pixel format
    if (pixelFormat == 1)
    {
        gCurrentPxFmt = 1;
        return TRUE;
    }
    else
    {
        gCurrentPxFmt = 0;
        return FALSE;
    }
}

WINGDIAPI BOOL WINAPI wglSwapBuffers(HDC hdc)
{
    return SwapBuffers(hdc);
}

WINGDIAPI BOOL WINAPI wglSwapLayerBuffers(HDC hdc)
{
    return FALSE;
}

WINGDIAPI BOOL WINAPI wglShareLists(HGLRC, HGLRC)
{
    return 1;
}

WINGDIAPI BOOL WINAPI wglUseFontBitmapsA(HDC, DWORD, DWORD, DWORD)
{
    return FALSE;
}

WINGDIAPI BOOL WINAPI wglUseFontBitmapsW(HDC, DWORD, DWORD, DWORD)
{
    return FALSE;
}

WINGDIAPI BOOL WINAPI wglUseFontOutlinesA(HDC, DWORD, DWORD, DWORD, FLOAT, FLOAT, int, LPGLYPHMETRICSFLOAT)
{
    return FALSE;
}

WINGDIAPI BOOL WINAPI wglUseFontOutlinesW(HDC, DWORD, DWORD, DWORD, FLOAT, FLOAT, int, LPGLYPHMETRICSFLOAT)
{
    return FALSE;
}

template <typename T>
T getPixelFormatAttrib(const int attribName, int iPixelFormat, int iLayerPlane)
{
    switch (attribName)
    {
    case WGL_NUMBER_PIXEL_FORMATS_ARB:
        return (T)gNumPxFmts;
    case WGL_DRAW_TO_WINDOW_ARB:
        return (T)((gPxFmts[iPixelFormat].dwFlags & PFD_DRAW_TO_WINDOW) != 0);
    case WGL_DRAW_TO_BITMAP_ARB:
        return (T)((gPxFmts[iPixelFormat].dwFlags & PFD_DRAW_TO_BITMAP) != 0);
    case WGL_ACCELERATION_ARB:
    {
        if (gPxFmts[iPixelFormat].dwFlags & PFD_GENERIC_ACCELERATED)
        {
            return WGL_FULL_ACCELERATION_ARB;
        }
        else
        {
            return WGL_NO_ACCELERATION_ARB;
        }
    }
    case WGL_NEED_PALETTE_ARB:
    case WGL_NEED_SYSTEM_PALETTE_ARB:
    case WGL_SWAP_LAYER_BUFFERS_ARB:
        return 0;
    case WGL_SWAP_METHOD_ARB:
    {
        if (gPxFmts[iPixelFormat].dwFlags & PFD_SWAP_COPY)
        {
            return WGL_SWAP_COPY_ARB;
        }
        else if (gPxFmts[iPixelFormat].dwFlags & PFD_SWAP_EXCHANGE)
        {
            return WGL_SWAP_EXCHANGE_ARB;
        }
        else
            return WGL_SWAP_UNDEFINED_ARB;
    }

    case WGL_NUMBER_OVERLAYS_ARB:
    case WGL_NUMBER_UNDERLAYS_ARB:
    case WGL_TRANSPARENT_ARB:
    case WGL_TRANSPARENT_RED_VALUE_ARB:
    case WGL_TRANSPARENT_GREEN_VALUE_ARB:
    case WGL_TRANSPARENT_BLUE_VALUE_ARB:
    case WGL_TRANSPARENT_ALPHA_VALUE_ARB:
    case WGL_TRANSPARENT_INDEX_VALUE_ARB:
    case WGL_SHARE_DEPTH_ARB:
    case WGL_SHARE_STENCIL_ARB:
    case WGL_SHARE_ACCUM_ARB:
    case WGL_SUPPORT_GDI_ARB:
        return 0;
    case WGL_SUPPORT_OPENGL_ARB:
        return (T)((gPxFmts[iPixelFormat].dwFlags & PFD_SUPPORT_OPENGL) != 0);
    case WGL_DOUBLE_BUFFER_ARB:
        return (T)((gPxFmts[iPixelFormat].dwFlags & PFD_DOUBLEBUFFER) != 0);
    case WGL_STEREO_ARB:
        return 0;
    case WGL_PIXEL_TYPE_ARB:
    {
        if (gPxFmts[iPixelFormat].iPixelType == PFD_TYPE_RGBA)
        {
            return WGL_TYPE_RGBA_ARB;
        }
        else
            return WGL_TYPE_COLORINDEX_ARB;
    }
    case WGL_COLOR_BITS_ARB:
        return (T)gPxFmts[iPixelFormat].cColorBits;
    case WGL_RED_BITS_ARB:
        return (T)gPxFmts[iPixelFormat].cRedBits;
    case WGL_RED_SHIFT_ARB:
        return (T)gPxFmts[iPixelFormat].cRedShift;
    case WGL_GREEN_BITS_ARB:
        return (T)gPxFmts[iPixelFormat].cGreenBits;
    case WGL_GREEN_SHIFT_ARB:
        return (T)gPxFmts[iPixelFormat].cGreenShift;
    case WGL_BLUE_BITS_ARB:
        return (T)gPxFmts[iPixelFormat].cBlueBits;
    case WGL_BLUE_SHIFT_ARB:
        return (T)gPxFmts[iPixelFormat].cBlueShift;
    case WGL_ALPHA_BITS_ARB:
        return (T)gPxFmts[iPixelFormat].cAlphaBits;
    case WGL_ALPHA_SHIFT_ARB:
        return (T)gPxFmts[iPixelFormat].cAlphaShift;
    case WGL_ACCUM_BITS_ARB:
        return (T)gPxFmts[iPixelFormat].cAccumBits;
    case WGL_ACCUM_RED_BITS_ARB:
        return (T)gPxFmts[iPixelFormat].cAccumRedBits;
    case WGL_ACCUM_GREEN_BITS_ARB:
        return (T)gPxFmts[iPixelFormat].cAccumGreenBits;
    case WGL_ACCUM_BLUE_BITS_ARB:
        return (T)gPxFmts[iPixelFormat].cAccumBlueBits;
    case WGL_ACCUM_ALPHA_BITS_ARB:
        return (T)gPxFmts[iPixelFormat].cAccumAlphaBits;
    case WGL_DEPTH_BITS_ARB:
        return (T)gPxFmts[iPixelFormat].cDepthBits;
    case WGL_STENCIL_BITS_ARB:
        return (T)gPxFmts[iPixelFormat].cStencilBits;
    case WGL_AUX_BUFFERS_ARB:
        return (T)gPxFmts[iPixelFormat].cAuxBuffers;

    default:
        assert(0);
    }
    return 0;
}

// WGL_ARB_pixel_format extension
WINGDIAPI BOOL WINAPI wglGetPixelFormatAttribivARB(HDC hdc,
                                                   int iPixelFormat,
                                                   int iLayerPlane,
                                                   UINT nAttributes,
                                                   const int *piAttributes,
                                                   int *piValues)
{
    for (UINT i = 0; i < nAttributes; ++i)
    {
        piValues[i] = getPixelFormatAttrib<int>(piAttributes[i], iPixelFormat, iLayerPlane);
    }
    return TRUE;
}

WINGDIAPI BOOL WINAPI wglGetPixelFormatAttribfvARB(HDC hdc,
                                                   int iPixelFormat,
                                                   int iLayerPlane,
                                                   UINT nAttributes,
                                                   const int *piAttributes,
                                                   FLOAT *pfValues)
{
    for (UINT i = 0; i < nAttributes; ++i)
    {
        pfValues[i] = getPixelFormatAttrib<FLOAT>(piAttributes[i], iPixelFormat, iLayerPlane);
    }
    return TRUE;
}

WINGDIAPI BOOL WINAPI wglChoosePixelFormatARB(HDC hdc,
                                              const int *piAttribIList,
                                              const FLOAT *pfAttribFList,
                                              UINT nMaxFormats,
                                              int *piFormats,
                                              UINT *nNumFormats)
{
    // only support one pix format
    piFormats[0] = 1;
    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////
// ICD.

WINGDIAPI BOOL WINAPI DrvCopyContext(HGLRC, HGLRC, UINT)
{
    return FALSE;
}

WINGDIAPI HGLRC WINAPI DrvCreateContext(HDC hdc)
{
    void *oglbuffer = _aligned_malloc(sizeof(OGL::State), KNOB_VS_SIMD_WIDTH * 4);
    memset(oglbuffer, 0, sizeof(OGL::State));
    OGL::State *pState = new (oglbuffer) OGL::State();
    OGL::Initialize(*pState);

    SWRContext ctx = { hdc, false, pState };

    int newrc = ghglrc++;

    gContexts[newrc] = ctx;

    return reinterpret_cast<HGLRC>(newrc);
}

WINGDIAPI BOOL WINAPI DrvDeleteContext(HGLRC hglrc)
{
    int rc = (int)hglrc;
    SWRContext &context = gContexts[rc];

    OGL::State *pState = context.pState;

    OGL::GetDDProcTable().pfnDestroyContext(OGL::GetDDHandle());

    gContexts.erase(rc);

    pState->~State();
    _aligned_free(pState);

    OGL::DestroyGlobals();

    return 1;
}

WINGDIAPI HGLRC WINAPI DrvCreateLayerContext(HDC hdc, int layerPlane)
{
    if (layerPlane == 0)
    {
        return DrvCreateContext(hdc);
    }

    return 0;
}

typedef void *PGLCLPROCTABLE;
WINGDIAPI PGLCLPROCTABLE WINAPI DrvSetContext(HDC hdc, DHGLRC hglrc, void *)
{
    BOOL result = wglMakeCurrent(hdc, hglrc);
    if (result)
    {
        return &gDispatch;
    }
    else
    {
        return NULL;
    }
}

WINGDIAPI void WINAPI DrvReleaseContext(DHGLRC hglrc)
{
    DrvSetContext(0, 0, 0);
}

WINGDIAPI BOOL WINAPI DrvShareLists(HGLRC, HGLRC)
{
    return 1;
}

WINGDIAPI BOOL WINAPI DrvDescribeLayerPlane(HDC, int, int, UINT, LPLAYERPLANEDESCRIPTOR)
{
    return FALSE;
}

WINGDIAPI int WINAPI DrvSetLayerPaletteEntries(HDC, int, int, int, CONST COLORREF *)
{
    return 0;
}

WINGDIAPI int WINAPI DrvGetLayerPaletteEntries(HDC, int, int, int, COLORREF *)
{
    return 0;
}

WINGDIAPI BOOL WINAPI DrvRealizeLayerPalette(HDC, int, BOOL)
{
    return FALSE;
}

WINGDIAPI BOOL WINAPI DrvSwapLayerBuffers(HDC hdc, UINT fuPlanes)
{
    if (hdc != 0)
    {
        SwapBuffers(hdc);
        return TRUE;
    }

    return FALSE;
}

WINGDIAPI int WINAPI DrvDescribePixelFormat(HDC hdc, int pixelFormat, UINT numBytes, LPPIXELFORMATDESCRIPTOR ppFD)
{
    return wglDescribePixelFormat(hdc, pixelFormat, numBytes, ppFD);
}

WINGDIAPI PROC WINAPI DrvGetProcAddress(LPCSTR address)
{
    return NULL;
}

WINGDIAPI BOOL WINAPI DrvSetPixelFormat(HDC hdc, int pixelFormat)
{
    return wglSetPixelFormat(hdc, pixelFormat, NULL);
}

WINGDIAPI BOOL WINAPI DrvSwapBuffers(HDC hdc)
{
    SwapBuffers(hdc);
    return TRUE;
}

typedef void *LPDIRECTDRAWSURFACE;

// Vista specific types
typedef struct _PRESENTBUFFERSCB
{
    IN UINT nVersion;
    IN UINT syncType; // See PRESCB_SYNCTYPE_NONE and PRESCB_SYNCTYPE_VSYNC
    IN LUID luidAdapter;
    IN LPVOID pPrivData;
    IN RECT updateRect; // Update rectangle in the coordinate system of the window, whose HDC is passed to PFN_PRESENTBUFFERS
} PRESENTBUFFERSCB, *LPPRESENTBUFFERSCB;

typedef void(WINAPI *PFN_SETCURRENTVALUE)(VOID *pv);
typedef void *(WINAPI *PFN_GETCURRENTVALUE)(VOID);
typedef DHGLRC(WINAPI *PFN_GETDHGLRC)(HGLRC hrc);
typedef HANDLE(WINAPI *PFN_GETDDHANDLE)(LPDIRECTDRAWSURFACE pdds);
typedef BOOL(WINAPI *PFN_PRESENTBUFFERS)(HDC hdc, LPPRESENTBUFFERSCB pprsbcbData);

// Some are not in use.
PFN_SETCURRENTVALUE __wglSetCurrentValueCallback = 0;
PFN_GETCURRENTVALUE __wglGetCurrentValueCallback = 0;
PFN_GETDHGLRC __wglGetDHGLRCCallback = 0;
PFN_GETDDHANDLE __wglGetDDHandleCallback = 0;
PFN_PRESENTBUFFERS __wglPresentBuffersCallback = 0;

WINGDIAPI void WINAPI DrvSetCallbackProcs(int nProcs, PROC *pProcs)
{
    if (nProcs >= 1)
    {
        __wglSetCurrentValueCallback = (PFN_SETCURRENTVALUE)pProcs[0];
    }

    if (nProcs >= 2)
    {
        __wglGetCurrentValueCallback = (PFN_GETCURRENTVALUE)pProcs[1];
    }

    if (nProcs >= 3)
    {
        __wglGetDHGLRCCallback = (PFN_GETDHGLRC)pProcs[2];
    }

    if (nProcs >= 4)
    {
        __wglGetDDHandleCallback = (PFN_GETDDHANDLE)pProcs[3];
    }

    if (nProcs >= 5)
    {
        __wglPresentBuffersCallback = (PFN_PRESENTBUFFERS)pProcs[4];
    }

    return;
}

typedef struct _PRESENTBUFFERS
{
    IN HANDLE hSurface;
    IN OUT LUID luidAdapter;
    IN UINT64 ullPresentToken;
    IN PVOID pPrivData;
} PRESENTBUFFERS, *LPPRESENTBUFFERS;

WINGDIAPI BOOL WINAPI DrvPresentBuffers(HDC hdc, LPPRESENTBUFFERS pprsbData)
{
    return TRUE;
}

WINGDIAPI BOOL WINAPI DrvValidateVersion(DWORD)
{
    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////
// Misc.

WINGDIAPI BOOL WINAPI SwapBuffers(HDC hdc)
{
    auto &ss = GetOGL();

    OGL_TRACE(SwapBuffers, GetOGL());

    OGL::GetDDProcTable().pfnPresent(OGL::GetDDHandle());

    // XXX: Now's our chance to do a little clean up.

    OGL::GetDDProcTable().pfnSwapBuffer(OGL::GetDDHandle());

    // XXX: final cleanup.

    return TRUE;
}
