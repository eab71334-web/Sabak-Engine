#include "platform_egl.h"
#include <EGL/egl.h>
#include <cstdio>
#include <cstdlib>
bool eglHeadlessInit(){
  setenv("EGL_PLATFORM","surfaceless",0);
  EGLDisplay d=eglGetDisplay(EGL_DEFAULT_DISPLAY);EGLint M,m;if(!eglInitialize(d,&M,&m)){std::fprintf(stderr,"eglInitialize failed 0x%x\n",eglGetError());return false;}
  eglBindAPI(EGL_OPENGL_ES_API);EGLint ca[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,EGL_NONE};EGLConfig c;EGLint n=0;
  if(!eglChooseConfig(d,ca,&c,1,&n)||n<1){std::fprintf(stderr,"no EGL config\n");return false;}
  EGLint pa[]={EGL_WIDTH,16,EGL_HEIGHT,16,EGL_NONE};EGLSurface s=eglCreatePbufferSurface(d,c,pa);
  EGLint xa[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};EGLContext x=eglCreateContext(d,c,EGL_NO_CONTEXT,xa);
  return eglMakeCurrent(d,s,s,x)==EGL_TRUE;}
