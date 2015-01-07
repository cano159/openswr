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

#include <X11/Xlibint.h>
#include <gl/glx.h>
#include <dlfcn.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>
#include <sys/shm.h>
#include <sys/ipc.h>

#include "oglglobals.h"
#include "oglstate.hpp"
#include "gldd.h"

#include "rdtsc.h"

#include <cassert>
#include <map>

namespace OGL
{
#include "gltrace.inl"
}

enum
{
    GLX_MAJOR_VERSION = 1,
    GLX_MINOR_VERSION = 3,
};

struct SWRContext
{
    Display *pDisplay;
    XVisualInfo *pVisInfo;
    GLXContext shareList;
    int direct;
    GLXDrawable drawable;
    bool initialized;
};

typedef std::map<OGL::State *, SWRContext> Contexts;

static Contexts contexts;
static Contexts::iterator currentContext = contexts.end();
static Visual gXVisual =
    {
      NULL,       // ext_data
      0xFF,       // visualid
      TrueColor,  // c_class
      0x00FF0000, // red mask
      0x0000FF00, // green mask
      0x000000FF, // blue mask
      8,          // bits_per_rgb,
      256,        // colormap
    };

static XVisualInfo gXVisInfo =
    {
      &gXVisual,  // visual
      0xFF,       // visual id
      0,          // screen
      24,         // depth
      TrueColor,  // class
      0x00FF0000, // red mask
      0x0000FF00, // green mask
      0x000000FF, // blue mask
      256,        // colormap size
      8,          // bits per rgb
    };

struct MyGLXFBConfig
{
    int visualType;
    int transparentType;
    /*    colors are floats scaled to ints */
    int transparentRed, transparentGreen, transparentBlue, transparentAlpha;
    int transparentIndex;

    int visualCaveat;

    int associatedVisualId;
    int screen;

    int drawableType;
    int renderType;

    int maxPbufferWidth, maxPbufferHeight, maxPbufferPixels;
    int optimalPbufferWidth, optimalPbufferHeight; /* for SGIX_pbuffer */

    int visualSelectGroup; /* visuals grouped by select priority */

    unsigned int id;
    GLboolean xRenderable;

    GLboolean rgbMode;
    GLboolean colorIndexMode;
    GLboolean doubleBufferMode;
    GLboolean stereoMode;
    GLboolean haveAccumBuffer;
    GLboolean haveDepthBuffer;
    GLboolean haveStencilBuffer;

    /* The number of bits present in various buffers */
    GLint accumRedBits, accumGreenBits, accumBlueBits, accumAlphaBits;
    GLint depthBits;
    GLint stencilBits;
    GLint indexBits;
    GLint rgbaBits; /* rgba total bits */
    GLint redBits, greenBits, blueBits, alphaBits;
    GLuint redMask, greenMask, blueMask, alphaMask;

    GLuint multiSampleSize; /* Number of samples per pixel (0 if no ms) */

    GLuint nMultiSampleBuffers; /* Number of availble ms buffers */
    GLint maxAuxBuffers;

    /* frame buffer level */
    GLint level;

    /* color ranges (for SGI_color_range) */
    GLboolean extendedRange;
    GLdouble minRed, maxRed;
    GLdouble minGreen, maxGreen;
    GLdouble minBlue, maxBlue;
    GLdouble minAlpha, maxAlpha;
};

// The one and only FBConfig
static MyGLXFBConfig gFBConfig =
    {
      GLX_TRUE_COLOR, // visualType
      GLX_NONE,       // transparentType
      0, 0, 0, 0, 0,  // transparent[Red|Green|Blue|Alpha|Index]
      GLX_NONE,       // visualCaveat
      33,             // associatedVisualId
      0,              // screen
      GLX_WINDOW_BIT, // drawableType
      GLX_RGBA_BIT,   // renderType
      4096,           // maxPbufferWidth
      4096,           // maxPbufferHeight
      16777216,       // maxPbufferPixels
      4096,           // optimalPbufferWidth
      4096,           // optimalPbufferHeight
      0,              // visualSelectGroup
      0,              // id
      True,           // xRenderable
      True,           // rgbMode
      False,          // colorIndexMode
      True,           // doubleBufferMode
      False,          // stereoMode
      False,          // haveAccumBuffer
      True,           // haveDepthBuffer
      False,          // haveStencilBuffer

      0, 0, 0, 0, // accum[Red|Green|Blue|Alpha]Bits
      24,         // depthBits
      0,          // stencilBits
      0,          // indexBits
      32,         // rgbaBits
      8, 8, 8, 8, // [red|green|blue|alpha]Bits
      0x00FF0000, // redMAsk
      0x0000FF00, // greenMAsk
      0x000000FF, // blueMask
      0xFF000000, // alphaMask

      0,     // multiSampleSize
      0,     // nMultiSampleBuffers
      0,     // maxAuxBuffers
      0,     // level
      False, // extendedRange
      0, 0,  // [min|max]Red
      0, 0,  // [min|max]Green
      0, 0,  // [min|max]Blue
      0, 0,  // [min|max]Alpha
    };

struct SWRPBuffer
{
    GLuint width, height;
};

enum SWRDrawable
{
    X,
    PBUFFER
};

std::unordered_map<XID, SWRDrawable> gDrawables;

extern "C" {
static bool xerrorFlag = false;
static int HandleXError(Display *dpy, XErrorEvent *event)
{
    xerrorFlag = true;
    return 0;
}
}

// Manages the SWR and OS resource set for a given drawable bound to the pipeline
// Responsible for flipping double buffers and displaying the contents on flush/flip
struct FrameBuffer
{
    bool mIsDisplay;
    XID mDrawable;
    GLuint mWidth;
    GLuint mHeight;

    UINT mCurBackBuffer;

    // SWR render target resources
    HANDLE mRenderBuffers[2];
    HANDLE mDepthBuffer;
    HANDLE mStencilBuffer;

    // OS backed resources for display, can be NULL for off-screen surfaces
    Display *mpDisplay;
    XVisualInfo *mvi;
    GC fontGC, swapGC;
    XFontStruct *fontinfo;
    XShmSegmentInfo mShmInfo[2];
    XImage *mpImages[2] = { 0 };
    bool useShm;

    // @todo support render targets of different formats - currently only support BGRA8 format
    void Initialize(GLuint width, GLuint height, XID drawable, Display *pDisplay, XVisualInfo *vi, bool isDisplay)
    {
        mpDisplay = pDisplay;
        mvi = vi;
        mWidth = width;
        mHeight = height;
        mIsDisplay = isDisplay;
        mDrawable = drawable;
        useShm = true;

        mCurBackBuffer = 0;

        DDProcTable &procTable = OGL::GetDDProcTable();

        mRenderBuffers[0] = procTable.pfnCreateRenderTarget(OGL::GetDDHandle(), width, height, BGRA8_UNORM);
        mRenderBuffers[1] = procTable.pfnCreateRenderTarget(OGL::GetDDHandle(), width, height, BGRA8_UNORM);
        mDepthBuffer = procTable.pfnCreateRenderTarget(OGL::GetDDHandle(), width, height, R32_FLOAT);

        // if displayable surface, set up X resources for display
        if (mIsDisplay)
        {
            assert(gDrawables[mDrawable] == X);

            for (UINT i = 0; i < 2; ++i)
            {
                if (useShm)
                {
                    mpImages[i] = XShmCreateImage(mpDisplay, mvi->visual, mvi->depth, ZPixmap, 0, &mShmInfo[i], width, height);
                    mShmInfo[i].shmid = shmget(IPC_PRIVATE, mpImages[i]->bytes_per_line * mpImages[i]->height, IPC_CREAT | 0777);
                    mShmInfo[i].shmaddr = mpImages[i]->data = (char *)shmat(mShmInfo[i].shmid, 0, 0);
                    mShmInfo[i].readOnly = False;

                    int (*oldHandler)(Display *, XErrorEvent *) = XSetErrorHandler(HandleXError);
                    XShmAttach(mpDisplay, &mShmInfo[i]);
                    XSync(mpDisplay, False);
                    XSetErrorHandler(oldHandler);

                    if (xerrorFlag)
                    {
                        fprintf(stderr, "SWR: MIT-SHM not usable, falling back to XImage\n");
                        useShm = false;
                        XDestroyImage(mpImages[i]);
                        shmdt(mShmInfo[i].shmaddr);
                        shmctl(mShmInfo[i].shmid, IPC_RMID, NULL);
                        mpImages[i] = NULL;
                    }
                }
                if (mpImages[i] == NULL)
                {
                    char *buffer = (char *)malloc(width * height * 4);
                    mpImages[i] = XCreateImage(mpDisplay, mvi->visual, mvi->depth, ZPixmap, 0, buffer, width, height, 32, 4 * width);
                }
            }

            // create swap GC
            swapGC = XCreateGC(mpDisplay, mDrawable, 0, NULL);

            XGCValues swapValues;
            swapValues.function = GXcopy;
            swapValues.graphics_exposures = False;
            XChangeGC(mpDisplay, swapGC, GCFunction, &swapValues);
            XChangeGC(mpDisplay, swapGC, GCGraphicsExposures, &swapValues);

            // Create font GC for text output
            fontinfo = XLoadQueryFont(mpDisplay, "6x10");
            XGCValues gr_values;
            gr_values.font = fontinfo->fid;
            gr_values.function = GXcopy;
            gr_values.plane_mask = AllPlanes;
            gr_values.foreground = BlackPixel(mpDisplay, 0);
            gr_values.background = WhitePixel(mpDisplay, 0);
            fontGC = XCreateGC(mpDisplay, mDrawable, GCFont | GCFunction | GCPlaneMask | GCForeground | GCBackground, &gr_values);
        }
    }

    void Destroy()
    {
        DDProcTable &procTable = OGL::GetDDProcTable();

        procTable.pfnDestroyRenderTarget(OGL::GetDDHandle(), mRenderBuffers[0]);
        procTable.pfnDestroyRenderTarget(OGL::GetDDHandle(), mRenderBuffers[1]);
        procTable.pfnDestroyRenderTarget(OGL::GetDDHandle(), mDepthBuffer);

        mRenderBuffers[0] = mRenderBuffers[1] = NULL;
        mDepthBuffer = NULL;

        if (mIsDisplay)
        {
            for (UINT i = 0; i < 2; ++i)
            {
                if (useShm)
                {
                    XShmDetach(mpDisplay, &mShmInfo[i]);
                    XDestroyImage(mpImages[i]);
                    shmdt(mShmInfo[i].shmaddr);
                    shmctl(mShmInfo[i].shmid, IPC_RMID, NULL);
                }
                else
                {
                    XDestroyImage(mpImages[i]);
                }
            }
            XFreeGC(mpDisplay, swapGC);
            XFreeGC(mpDisplay, fontGC);
            XFreeFont(mpDisplay, fontinfo);
        }
    }

    void Flip()
    {
        RDTSC_START(APIFlip);

        if (mIsDisplay)
        {
            XSync(mpDisplay, False);
            // copy render target to Xshm surface, mirroring on Y to account for X/GL origin differences (X is top-left, GL is bottom-left)
            UINT pitch = mWidth * 4;
            OGL::GetDDProcTable().pfnPresent2(OGL::GetDDHandle(), mpImages[mCurBackBuffer]->data, pitch);

            // copy to display surface
            if (useShm)
                XShmPutImage(mpDisplay, mDrawable, swapGC, mpImages[mCurBackBuffer], 0, 0, 0, 0, mWidth, mHeight, False);
            else
                XPutImage(mpDisplay, mDrawable, swapGC, mpImages[mCurBackBuffer], 0, 0, 0, 0, mWidth, mHeight);

            // flip back buffer
            mCurBackBuffer ^= 1;
        }

        RDTSC_STOP(APIFlip, 1, 0);
    }

    void Flush();
    void Resize(GLuint width, GLuint height)
    {
        // check to see if the dimensions changed on the X window
        if (mIsDisplay)
        {
            if (mWidth == width && mHeight == height)
            {
                return;
            }

            XWindowAttributes xWindowAttributes = { 0 };
            XGetWindowAttributes(mpDisplay, mDrawable, &xWindowAttributes);
            if (mWidth != (GLuint)xWindowAttributes.width || mHeight != (GLuint)xWindowAttributes.height)
            {
                Destroy();
                Initialize(xWindowAttributes.width, xWindowAttributes.height, mDrawable, mpDisplay, mvi, mIsDisplay);
            }
        }
    }

    HANDLE GetBackBuffer()
    {
        return mRenderBuffers[mCurBackBuffer];
    }

    HANDLE GetFrontBuffer()
    {
        return mRenderBuffers[mCurBackBuffer ^ 1];
    }

    HANDLE GetDepthBuffer()
    {
        return mDepthBuffer;
    }
};

std::unordered_map<XID, FrameBuffer *> gDrawableBuffers;

extern "C" {

void _glXResize(GLuint width, GLuint height);

// @todo generate real FB configs
GLXFBConfig *glXChooseFBConfig(Display *pDisplay, int screen, const int *pAttribList, int *nelements)
{
    MyGLXFBConfig *result = (MyGLXFBConfig *)Xmalloc(sizeof(MyGLXFBConfig));
    memcpy(result, &gFBConfig, sizeof(MyGLXFBConfig));

    MyGLXFBConfig **fbResult = (MyGLXFBConfig **)Xmalloc(sizeof(MyGLXFBConfig *));
    *nelements = 1;
    *fbResult = result;
    return (GLXFBConfig *)fbResult;
}

static void initVisualID(Display *pDisplay, int screen)
{
    static int foundID = 0;
    if (foundID)
        return;

    XVisualInfo info;
    if (!XMatchVisualInfo(pDisplay, screen, 24, TrueColor, &info))
    {
        fprintf(stderr, "initVisualID: No visual to match required\n");
        exit(1);
    }

    gXVisual.visualid = info.visualid;
    gXVisInfo.visualid = info.visualid;
    foundID = 1;
}

XVisualInfo *glXChooseVisual(Display *pDisplay, int screen, int *pAttribList)
{
    (void)pDisplay, (void)pAttribList;

    initVisualID(pDisplay, screen);

    // Need to make copy since consumer may free it
    XVisualInfo *vi = (XVisualInfo *)Xmalloc(sizeof(XVisualInfo));
    memcpy(vi, &gXVisInfo, sizeof(XVisualInfo));
    vi->screen = screen;

    return vi;
}

GLXFBConfig *glXGetFBConfigs(Display *pDisplay, int screen, int *nelements)
{
    *nelements = 1;
    GLXFBConfig *cfg = (GLXFBConfig *)Xmalloc(sizeof(GLXFBConfig));
    *cfg = (GLXFBConfig)&gFBConfig;
    return cfg;
}

int glXGetFBConfigAttrib(Display *pDisplay, GLXFBConfig config, int attribute, int *value)
{
    MyGLXFBConfig *myConfig = (MyGLXFBConfig *)config;
    int retVal = Success;
    switch (attribute)
    {
    case GLX_FBCONFIG_ID:
        *value = myConfig->id;
        break;
    case GLX_BUFFER_SIZE:
        *value = myConfig->rgbaBits;
        break;
    case GLX_LEVEL:
        *value = myConfig->level;
        break;
    case GLX_DOUBLEBUFFER:
        *value = myConfig->doubleBufferMode;
        break;
    case GLX_STEREO:
        *value = myConfig->stereoMode;
        break;
    case GLX_AUX_BUFFERS:
        *value = myConfig->maxAuxBuffers;
        break;
    case GLX_RED_SIZE:
        *value = myConfig->redBits;
        break;
    case GLX_GREEN_SIZE:
        *value = myConfig->greenBits;
        break;
    case GLX_BLUE_SIZE:
        *value = myConfig->blueBits;
        break;
    case GLX_ALPHA_SIZE:
        *value = myConfig->alphaBits;
        break;
    case GLX_DEPTH_SIZE:
        *value = myConfig->depthBits;
        break;
    case GLX_STENCIL_SIZE:
        *value = myConfig->stencilBits;
        break;
    case GLX_ACCUM_RED_SIZE:
        *value = myConfig->accumRedBits;
        break;
    case GLX_ACCUM_GREEN_SIZE:
        *value = myConfig->accumGreenBits;
        break;
    case GLX_ACCUM_BLUE_SIZE:
        *value = myConfig->accumBlueBits;
        break;
    case GLX_ACCUM_ALPHA_SIZE:
        *value = myConfig->accumAlphaBits;
        break;
    case GLX_RENDER_TYPE:
        *value = myConfig->renderType;
        break;
    case GLX_DRAWABLE_TYPE:
        *value = myConfig->drawableType;
        break;
    case GLX_X_RENDERABLE:
        *value = myConfig->xRenderable;
        break;
    case GLX_VISUAL_ID:
        *value = myConfig->associatedVisualId;
        break;
    case GLX_X_VISUAL_TYPE:
        *value = myConfig->visualType;
        break;
    case GLX_CONFIG_CAVEAT:
        *value = myConfig->visualCaveat;
        break;
    case GLX_TRANSPARENT_TYPE:
        *value = myConfig->transparentType;
        break;
    case GLX_TRANSPARENT_INDEX_VALUE:
        *value = myConfig->transparentIndex;
        break;
    case GLX_TRANSPARENT_RED_VALUE:
        *value = myConfig->transparentRed;
        break;
    case GLX_TRANSPARENT_GREEN_VALUE:
        *value = myConfig->transparentGreen;
        break;
    case GLX_TRANSPARENT_BLUE_VALUE:
        *value = myConfig->transparentBlue;
        break;
    case GLX_TRANSPARENT_ALPHA_VALUE:
        *value = myConfig->transparentAlpha;
        break;
    case GLX_MAX_PBUFFER_WIDTH:
        *value = myConfig->maxPbufferWidth;
        break;
    case GLX_MAX_PBUFFER_HEIGHT:
        *value = myConfig->maxPbufferHeight;
        break;
    case GLX_MAX_PBUFFER_PIXELS:
        *value = myConfig->maxPbufferPixels;
        break;

    default:
        retVal = GLX_BAD_ATTRIBUTE;
        break;
    }
    return retVal;
}

int glXSwapIntervalSGI(int interval)
{
    return Success;
}

void *glXGetProcAddressARB(const GLubyte *procName)
{
    static void *handle = dlopen("libGL.so.1", RTLD_LOCAL | RTLD_LAZY);
    void *procAddr = dlsym(handle, (const char *)procName);
    return procAddr;
}

XVisualInfo *glXGetVisualFromFBConfig(Display *pDisplay, GLXFBConfig config)
{
    return glXChooseVisual(pDisplay, 0, NULL);
}

GLXContext glXCreateContext(Display *pDisplay, XVisualInfo *pVisInfo, GLXContext shareList, int direct)
{
    //assert(memcmp(&gXVisInfo, pVisInfo, sizeof(XVisualInfo)) == 0 && "Your visual info doesn\'t match our visual info, yo!");

    void *oglbuffer = _aligned_malloc(sizeof(OGL::State) + sizeof(void *), KNOB_VS_SIMD_WIDTH * 4); // because
    memset(oglbuffer, 0, sizeof(OGL::State));
    OGL::State *pState = new (oglbuffer) OGL::State();
    OGL::Initialize(*pState);
    pState->pfnResize = _glXResize;

    initVisualID(pDisplay, pVisInfo->screen);

    SWRContext xctxt =
        {
          pDisplay,
          &gXVisInfo,
          shareList,
          direct,
          0,
          false
        };

    contexts[pState] = xctxt;

    return oglbuffer;
}

GLXContext glXCreateNewContext(Display *pDisplay, GLXFBConfig config, int render_type, GLXContext share_list, Bool direct)
{
    initVisualID(pDisplay, gXVisInfo.screen);
    return glXCreateContext(pDisplay, &gXVisInfo, share_list, direct);
}

Display *glXGetCurrentDisplay()
{
    if (currentContext == contexts.end())
    {
        return NULL;
    }
    auto &xCtxt = currentContext->second;
    return xCtxt.pDisplay;
}

void glXDestroyContext(Display *pDisplay, GLXContext ctx)
{
    auto *pState = reinterpret_cast<OGL::State *>(ctx);

    if (contexts.find(pState) == currentContext)
    {
        currentContext = contexts.end();
    }

    contexts.erase(pState);

    OGL::Destroy(*pState);
    pState->~State();

    _aligned_free(pState);
}

Bool glXMakeContextCurrent(Display *pDisplay, GLXDrawable draw, GLXDrawable read, GLXContext ctx)
{
    // unbind current context if NULL ctx passed in
    if (ctx == NULL)
    {
        currentContext = contexts.end();
        return 1;
    }

    auto *pState = reinterpret_cast<OGL::State *>(ctx);
    if (contexts.find(pState) == currentContext)
    {
        return 1;
    }

    auto &xCtxt = contexts[pState];
    xCtxt.drawable = draw;

    SetOGL(pState);

    GLuint width, height;
    XWindowAttributes xWindowAttributes = { 0 };
    SWRPBuffer *buf;
    bool isDisplay;
    switch (gDrawables[draw])
    {
    case X:
        XGetWindowAttributes(pDisplay, draw, &xWindowAttributes);
        width = xWindowAttributes.width;
        height = xWindowAttributes.height;
        isDisplay = true;
        break;
    case PBUFFER:
        buf = (SWRPBuffer *)draw;
        width = buf->width;
        height = buf->height;
        isDisplay = false;
        break;
    default:
        assert(0 && "Invalid buffer type");
        return 0;
    }

    // bind render target set to SWR pipeline
    FrameBuffer *fb;
    if (gDrawableBuffers.find(draw) == gDrawableBuffers.end())
    {
        // we haven't seen this drawable yet, create a new frame buffer
        fb = new FrameBuffer();
        fb->Initialize(width, height, draw, xCtxt.pDisplay, xCtxt.pVisInfo, isDisplay);
        gDrawableBuffers[draw] = fb;
    }
    else
    {
        fb = gDrawableBuffers[draw];
    }

    OGL::GetDDProcTable().pfnSetRenderTarget(OGL::GetDDHandle(), fb->GetBackBuffer(), fb->GetDepthBuffer());

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

    currentContext = contexts.find(pState);

    return 1;
}

int glXMakeCurrent(Display *pDisplay, GLXDrawable drawable, GLXContext ctx)
{
    // assume drawable is an Window or Pixmap if we haven't set it's type yet
    if (gDrawables.find(drawable) == gDrawables.end())
    {
        gDrawables[drawable] = X;
    }
    return glXMakeContextCurrent(pDisplay, drawable, drawable, ctx);
}

GLXWindow glXCreateWindow(Display *pDisplay, GLXFBConfig config, Window win, const int *attrib_list)
{
    gDrawables[win] = X;
    return win;
}

void glXDestroyWindow(Display *pDisplay, GLXWindow win)
{
    return;
}

GLXPixmap glXCreatePixmap(Display *pDisplay, GLXFBConfig config, Pixmap pixmap, const int *attrib_list)
{
    gDrawables[pixmap] = X;
    return pixmap;
}

void glXDestroyPixmap(Display *pDisplay, GLXPixmap pixmap)
{
    return;
}

GLXPbuffer glXCreatePbuffer(Display *pDisplay, GLXFBConfig config, const int *attrib_list)
{
    // pbuffers are created as BGRA8 render targets always, the same as the display surface, so we
    // ignore the incoming config for now
    SWRPBuffer *swrPbuf = new SWRPBuffer();
    while (*attrib_list != None)
    {
        switch (*attrib_list++)
        {
        case GLX_PBUFFER_WIDTH:
            swrPbuf->width = *attrib_list++;
            break;
        case GLX_PBUFFER_HEIGHT:
            swrPbuf->height = *attrib_list++;
            break;
        default:
            attrib_list++;
        }
    }

    XID result = (XID)swrPbuf;
    gDrawables[result] = PBUFFER;
    return result;
}

void glXDestroyPbuffer(Display *pDisplay, GLXPbuffer pixmap)
{
    // destroy underlying framebuffer resources
    FrameBuffer *fb = gDrawableBuffers[pixmap];
    fb->Destroy();
    delete fb;
    gDrawableBuffers.erase(pixmap);

    SWRPBuffer *swrPbuf = (SWRPBuffer *)pixmap;
    delete swrPbuf;

    return;
}

void glXQueryDrawable(Display *pDisplay, GLXDrawable draw, int attribute, unsigned int *value)
{
    return;
}

GLXDrawable glXGetCurrentReadDrawable()
{
    return currentContext->second.drawable;
}

int glXQueryContext(Display *pDisplay, GLXContext ctx, int attribute, int *value)
{
    return Success;
}

void glXSelectEvent(Display *pDisplay, GLXDrawable drawable, unsigned long eventmask)
{
}

void glXGetSelectedEvent(Display *pDisplay, GLXDrawable draw, unsigned long *eventmask)
{
}

void glXCopyContext(Display *pDisplay, GLXContext src, GLXContext dst, GLuint mask)
{
    (void)pDisplay, (void)src, (void)dst, (void)mask;
}

void glXSwapBuffers(Display *pDisplay, GLXDrawable drawable)
{
    assert(gDrawableBuffers.find(drawable) != gDrawableBuffers.end());

    OGL_TRACE(SwapBuffers, GetOGL());
    FrameBuffer *fb = gDrawableBuffers[drawable];
    fb->Flip();
    OGL::GetDDProcTable().pfnSetRenderTarget(OGL::GetDDHandle(), fb->GetBackBuffer(), fb->GetDepthBuffer());
}

GLXPixmap glXCreateGLXPixmap(Display *pDisplay, XVisualInfo *pVisInfo, Pixmap pixmap)
{
    (void)pDisplay, (void)pVisInfo, (void)pixmap;
    return 0;
}

void glXDestroyGLXPixmap(Display *pDisplay, GLXPixmap pixmap)
{
    (void)pDisplay, (void)pixmap;
}

int glXQueryExtension(Display *pDisplay, int *pErrorB, int *pEvent)
{
    return GL_TRUE;
}

int glXQueryVersion(Display *pDisplay, int *pMajorVersion, int *pMinorVersion)
{
    *pMajorVersion = GLX_MAJOR_VERSION;
    *pMinorVersion = GLX_MINOR_VERSION;
    return GL_TRUE;
}

int glXIsDirect(Display *pDisplay, GLXContext ctx)
{
    return GL_TRUE;
}

int glXGetConfig(Display *pDisplay, XVisualInfo *pVisInfo, int attrib, int *pValue)
{
    switch (attrib)
    {
    case GLX_USE_GL:
        *pValue = GL_TRUE;
        break;
    case GLX_BUFFER_SIZE:
        *pValue = 32;
        break;
    case GLX_LEVEL:
        *pValue = 0;
        break;
    case GLX_RGBA:
        *pValue = GL_TRUE;
        break;
    case GLX_DOUBLEBUFFER:
        *pValue = GL_TRUE;
        break;
    case GLX_STEREO:
        *pValue = GL_FALSE;
        break;
    case GLX_AUX_BUFFERS:
        *pValue = 0;
        break;
    case GLX_RED_SIZE:
    case GLX_GREEN_SIZE:
    case GLX_BLUE_SIZE:
    case GLX_ALPHA_SIZE:
        *pValue = 8;
        break;
    case GLX_DEPTH_SIZE:
        *pValue = 32;
        break;
    case GLX_STENCIL_SIZE:
        *pValue = 32;
        break;
    case GLX_ACCUM_RED_SIZE:
    case GLX_ACCUM_GREEN_SIZE:
    case GLX_ACCUM_BLUE_SIZE:
    case GLX_ACCUM_ALPHA_SIZE:
        *pValue = 8;
        break;
    default:
        return 0;
    }

    return 1;
}

GLXContext glXGetCurrentContext()
{
    return currentContext->first;
}

GLXDrawable glXGetCurrentDrawable()
{
    return currentContext->second.drawable;
}

void glXWaitGL()
{
    FrameBuffer *fb = gDrawableBuffers[glXGetCurrentDrawable()];
    OGL::GetDDProcTable().pfnLockBuffer(OGL::GetDDHandle(), fb->mRenderBuffers[fb->mCurBackBuffer]);
    OGL::GetDDProcTable().pfnUnlockBuffer(OGL::GetDDHandle(), fb->mRenderBuffers[fb->mCurBackBuffer]);
}

void glXWaitX()
{
    Display *display = glXGetCurrentDisplay();
    if (display)
        XSync(display, False);
}

void glXUseXFont(Font font, int first, int count, int list)
{
    assert(false && "Cannot choose that font");
}

const char *glXQueryExtensionsString(Display *pDisplay, int screen)
{
    (void)pDisplay, (void)screen;
    return "ARB_get_proc_address";
}

const char *glXQueryServerString(Display *pDisplay, int screen, int name)
{
    (void)pDisplay, (void)screen, (void)name;
    return "";
}

const char *glXGetClientString(Display *pDisplay, int name)
{
    (void)pDisplay, (void)name;
    return "";
}

void _glXResize(GLuint width, GLuint height)
{
    XID drawable = currentContext->second.drawable;
    FrameBuffer *fb = gDrawableBuffers[drawable];

    fb->Resize(width, height);

    // reset render target in case swap chain was destroyed and recreated on resize
    OGL::GetDDProcTable().pfnSetRenderTarget(OGL::GetDDHandle(), fb->GetBackBuffer(), fb->GetDepthBuffer());
}

} // end extern "C"
