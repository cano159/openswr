#ifndef SWR_GLX_H
#define SWR_GLX_H

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "gl.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define GLX_VERSION_1_1				1
#define GLX_VERSION_1_2				1
#define GLX_VERSION_1_3				1
#define GLX_VERSION_1_4				1

#define GLX_EXTENSION_NAME   		"GLX"

#define GLX_USE_GL					1
#define GLX_BUFFER_SIZE				2
#define GLX_LEVEL					3
#define GLX_RGBA					4
#define GLX_DOUBLEBUFFER			5
#define GLX_STEREO					6
#define GLX_AUX_BUFFERS				7
#define GLX_RED_SIZE				8
#define GLX_GREEN_SIZE				9
#define GLX_BLUE_SIZE				10
#define GLX_ALPHA_SIZE				11
#define GLX_DEPTH_SIZE				12
#define GLX_STENCIL_SIZE			13
#define GLX_ACCUM_RED_SIZE			14
#define GLX_ACCUM_GREEN_SIZE		15
#define GLX_ACCUM_BLUE_SIZE			16
#define GLX_ACCUM_ALPHA_SIZE		17

#define GLX_BAD_SCREEN				1
#define GLX_BAD_ATTRIBUTE			2
#define GLX_NO_EXTENSION			3
#define GLX_BAD_VISUAL				4
#define GLX_BAD_CONTEXT				5
#define GLX_BAD_VALUE       		6
#define GLX_BAD_ENUM				7

#define GLX_VENDOR					1
#define GLX_VERSION					2
#define GLX_EXTENSIONS 				3
#define GLX_CONFIG_CAVEAT			0x20
#define GLX_DONT_CARE				0xFFFFFFFF
#define GLX_X_VISUAL_TYPE			0x22
#define GLX_TRANSPARENT_TYPE		0x23
#define GLX_TRANSPARENT_INDEX_VALUE	0x24
#define GLX_TRANSPARENT_RED_VALUE	0x25
#define GLX_TRANSPARENT_GREEN_VALUE	0x26
#define GLX_TRANSPARENT_BLUE_VALUE	0x27
#define GLX_TRANSPARENT_ALPHA_VALUE	0x28
#define GLX_WINDOW_BIT				0x00000001
#define GLX_PIXMAP_BIT				0x00000002
#define GLX_PBUFFER_BIT				0x00000004
#define GLX_AUX_BUFFERS_BIT			0x00000010
#define GLX_FRONT_LEFT_BUFFER_BIT	0x00000001
#define GLX_FRONT_RIGHT_BUFFER_BIT	0x00000002
#define GLX_BACK_LEFT_BUFFER_BIT	0x00000004
#define GLX_BACK_RIGHT_BUFFER_BIT	0x00000008
#define GLX_DEPTH_BUFFER_BIT		0x00000020
#define GLX_STENCIL_BUFFER_BIT		0x00000040
#define GLX_ACCUM_BUFFER_BIT		0x00000080
#define GLX_NONE					0x8000
#define GLX_SLOW_CONFIG				0x8001
#define GLX_TRUE_COLOR				0x8002
#define GLX_DIRECT_COLOR			0x8003
#define GLX_PSEUDO_COLOR			0x8004
#define GLX_STATIC_COLOR			0x8005
#define GLX_GRAY_SCALE				0x8006
#define GLX_STATIC_GRAY				0x8007
#define GLX_TRANSPARENT_RGB			0x8008
#define GLX_TRANSPARENT_INDEX		0x8009
#define GLX_VISUAL_ID				0x800B
#define GLX_SCREEN					0x800C
#define GLX_NON_CONFORMANT_CONFIG	0x800D
#define GLX_DRAWABLE_TYPE			0x8010
#define GLX_RENDER_TYPE				0x8011
#define GLX_X_RENDERABLE			0x8012
#define GLX_FBCONFIG_ID				0x8013
#define GLX_RGBA_TYPE				0x8014
#define GLX_COLOR_INDEX_TYPE		0x8015
#define GLX_MAX_PBUFFER_WIDTH		0x8016
#define GLX_MAX_PBUFFER_HEIGHT		0x8017
#define GLX_MAX_PBUFFER_PIXELS		0x8018
#define GLX_PRESERVED_CONTENTS		0x801B
#define GLX_LARGEST_PBUFFER			0x801C
#define GLX_WIDTH					0x801D
#define GLX_HEIGHT					0x801E
#define GLX_EVENT_MASK				0x801F
#define GLX_DAMAGED					0x8020
#define GLX_SAVED					0x8021
#define GLX_WINDOW					0x8022
#define GLX_PBUFFER					0x8023
#define GLX_PBUFFER_HEIGHT          0x8040
#define GLX_PBUFFER_WIDTH           0x8041
#define GLX_RGBA_BIT				0x00000001
#define GLX_COLOR_INDEX_BIT			0x00000002
#define GLX_PBUFFER_CLOBBER_MASK	0x08000000
#define GLX_SAMPLE_BUFFERS          0x186a0 /*100000*/
#define GLX_SAMPLES                 0x186a1 /*100001*/



typedef void*	GLXContext;
typedef XID		GLXPixmap;
typedef XID		GLXDrawable;
typedef void*	GLXFBConfig;
typedef XID		GLXFBConfigID;
typedef XID		GLXContextID;
typedef XID		GLXWindow;
typedef XID		GLXPbuffer;

XVisualInfo* glXChooseVisual(
	Display* pDisplay,
	int screen,
	int* pAttribList);

GLXContext glXCreateContext(
	Display* pDisplay,
	XVisualInfo* pVisInfo,
	GLXContext shareList,
	int direct);

void glXDestroyContext(
	Display* pDisplay,
	GLXContext ctx);

int glXMakeCurrent(
	Display* pDisplay,
	GLXDrawable drawable,
	GLXContext ctx);

void glXCopyContext(
	Display* pDisplay,
	GLXContext src,
	GLXContext dst,
	GLuint mask);

void glXSwapBuffers(
	Display* pDisplay,
	GLXDrawable drawable);

GLXPixmap glXCreateGLXPixmap(
	Display* pDisplay,
	XVisualInfo* pVisInfo,
	Pixmap pixmap);

void glXDestroyGLXPixmap(
	Display* pDisplay,
	GLXPixmap pixmap);

Bool glXQueryExtension(
	Display* pDisplay,
	int* pErrorB,
	int* pEvent);

Bool glXQueryVersion(
	Display* pDisplay,
	int* pMajorVersion,
	int* pMinorVersion);

Bool glXIsDirect(
	Display* pDisplay,
	GLXContext ctx);

int glXGetConfig(
	Display* pDisplay,
	XVisualInfo* pVisInfo,
	int attrib,
	int* pValue);

GLXContext glXGetCurrentContext();

GLXDrawable glXGetCurrentDrawable();

void glXWaitGL();

void glXWaitX();

void glXUseXFont(
	Font font,
	int first,
	int count,
	int list);

/* GLX 1.1 and later */
const char* glXQueryExtensionsString(
	Display* pDisplay,
	int screen);

const char* glXQueryServerString(
	Display* pDisplay,
	int screen,
	int name);

const char* glXGetClientString(
	Display* pDisplay,
	int name);

/* GLX 1.2 and later */
Display* glXGetCurrentDisplay();

/* GLX 1.3 and later */
GLXFBConfig* glXChooseFBConfig(
	Display* pDisplay,
	int screen,
	const int* pAttribList,
	int* ielements);

int glXGetFBConfigAttrib(
	Display* pDisplay,
	GLXFBConfig config,
	int attribute,
	int* value);

GLXFBConfig* glXGetFBConfigs(
	Display* pDisplay,
	int screen,
	int* nelements);

XVisualInfo* glXGetVisualFromFBConfig(
	Display*dpy,
	GLXFBConfig config);

GLXWindow glXCreateWindow(
	Display* pDisplay,
	GLXFBConfig config,
	Window win,
	const int* pAttribList);

void glXDestroyWindow(
	Display* pDisplay,
	GLXWindow window);

GLXPixmap glXCreatePixmap(
	Display* pDisplay,
	GLXFBConfig config,
	Pixmap pixmap,
	const int* pAttribList);

void glXDestroyPixmap(
	Display* pDisplay,
	GLXPixmap pixmap);

GLXPbuffer glXCreatePbuffer(
	Display* pDisplay,
	GLXFBConfig config,
	const int* pAttribList);

void glXDestroyPbuffer(
	Display* pDisplay,
	GLXPbuffer pbuf);

void glXQueryDrawable(
	Display* pDisplay,
	GLXDrawable draw,
	int attribute,
	unsigned int* value);

GLXContext glXCreateNewContext(
	Display* pDisplay,
	GLXFBConfig config,
	int renderType,
	GLXContext shareList,
	Bool direct);

Bool glXMakeContextCurrent(
	Display* pDisplay,
	GLXDrawable draw,
	GLXDrawable read,
	GLXContext ctx);

GLXDrawable glXGetCurrentReadDrawable();

int glXQueryContext(
	Display* pDisplay,
	GLXContext ctx,
	int attribute,
	int* value);

void glXSelectEvent(
	Display* pDisplay,
	GLXDrawable drawable,
	unsigned long mask);

void glXGetSelectedEvent(
	Display* pDisplay,
	GLXDrawable drawable,
        unsigned long* mask);

/* ARB 2. GLX_ARB_get_proc_address */
void* glXGetProcAddressARB(
	const GLubyte* procName);

/* ARB 40. GLX_SGI_swap_control */
int glXSwapIntervalSGI(
	int interval);

#ifdef __cplusplus
} // end "C"
#endif

#endif//SWR_GLX_H
