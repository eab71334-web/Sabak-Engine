// OpenGL ES 3 header shim (iOS uses Apple's OpenGLES framework, others use Khronos headers)
#pragma once
#ifdef __APPLE__
#include <TargetConditionals.h>
#include <OpenGLES/ES3/gl.h>
#include <OpenGLES/ES3/glext.h>
#else
#include <GLES3/gl3.h>
#endif
