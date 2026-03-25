#pragma once

#if defined(__APPLE__)
#include <OpenGL/gl3.h>
#else
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <GL/gl.h>
#include <GL/glext.h>
#endif

namespace pointmod {

bool InitializeOpenGLBindings();

}  // namespace pointmod

#if !defined(__APPLE__)
#define POINTMOD_OPENGL_FUNCTIONS(X) \
  X(PFNGLATTACHSHADERPROC, glAttachShader) \
  X(PFNGLBINDATTRIBLOCATIONPROC, glBindAttribLocation) \
  X(PFNGLBINDBUFFERPROC, glBindBuffer) \
  X(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray) \
  X(PFNGLBUFFERDATAPROC, glBufferData) \
  X(PFNGLCOMPILESHADERPROC, glCompileShader) \
  X(PFNGLCREATEPROGRAMPROC, glCreateProgram) \
  X(PFNGLCREATESHADERPROC, glCreateShader) \
  X(PFNGLDELETEBUFFERSPROC, glDeleteBuffers) \
  X(PFNGLDELETEPROGRAMPROC, glDeleteProgram) \
  X(PFNGLDELETESHADERPROC, glDeleteShader) \
  X(PFNGLDELETEVERTEXARRAYSPROC, glDeleteVertexArrays) \
  X(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray) \
  X(PFNGLGENBUFFERSPROC, glGenBuffers) \
  X(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays) \
  X(PFNGLGETPROGRAMINFOLOGPROC, glGetProgramInfoLog) \
  X(PFNGLGETPROGRAMIVPROC, glGetProgramiv) \
  X(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog) \
  X(PFNGLGETSHADERIVPROC, glGetShaderiv) \
  X(PFNGLGETUNIFORMLOCATIONPROC, glGetUniformLocation) \
  X(PFNGLLINKPROGRAMPROC, glLinkProgram) \
  X(PFNGLSHADERSOURCEPROC, glShaderSource) \
  X(PFNGLUNIFORM1FPROC, glUniform1f) \
  X(PFNGLUNIFORMMATRIX4FVPROC, glUniformMatrix4fv) \
  X(PFNGLUSEPROGRAMPROC, glUseProgram) \
  X(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer)

namespace pointmod::gl {

#define POINTMOD_DECLARE_GL_FUNCTION(type, name) extern type name;
POINTMOD_OPENGL_FUNCTIONS(POINTMOD_DECLARE_GL_FUNCTION)
#undef POINTMOD_DECLARE_GL_FUNCTION

}  // namespace pointmod::gl

#define glAttachShader pointmod::gl::glAttachShader
#define glBindAttribLocation pointmod::gl::glBindAttribLocation
#define glBindBuffer pointmod::gl::glBindBuffer
#define glBindVertexArray pointmod::gl::glBindVertexArray
#define glBufferData pointmod::gl::glBufferData
#define glCompileShader pointmod::gl::glCompileShader
#define glCreateProgram pointmod::gl::glCreateProgram
#define glCreateShader pointmod::gl::glCreateShader
#define glDeleteBuffers pointmod::gl::glDeleteBuffers
#define glDeleteProgram pointmod::gl::glDeleteProgram
#define glDeleteShader pointmod::gl::glDeleteShader
#define glDeleteVertexArrays pointmod::gl::glDeleteVertexArrays
#define glEnableVertexAttribArray pointmod::gl::glEnableVertexAttribArray
#define glGenBuffers pointmod::gl::glGenBuffers
#define glGenVertexArrays pointmod::gl::glGenVertexArrays
#define glGetProgramInfoLog pointmod::gl::glGetProgramInfoLog
#define glGetProgramiv pointmod::gl::glGetProgramiv
#define glGetShaderInfoLog pointmod::gl::glGetShaderInfoLog
#define glGetShaderiv pointmod::gl::glGetShaderiv
#define glGetUniformLocation pointmod::gl::glGetUniformLocation
#define glLinkProgram pointmod::gl::glLinkProgram
#define glShaderSource pointmod::gl::glShaderSource
#define glUniform1f pointmod::gl::glUniform1f
#define glUniformMatrix4fv pointmod::gl::glUniformMatrix4fv
#define glUseProgram pointmod::gl::glUseProgram
#define glVertexAttribPointer pointmod::gl::glVertexAttribPointer
#endif
