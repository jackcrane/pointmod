#pragma once

#if defined(__APPLE__)
#include <OpenGL/gl3.h>
#else
#include <backends/imgui_impl_opengl3_loader.h>

#ifndef GL_DEPTH_BUFFER_BIT
#define GL_DEPTH_BUFFER_BIT 0x00000100
#endif

#ifndef GL_POINTS
#define GL_POINTS 0x0000
#endif

#ifndef GL_STATIC_DRAW
#define GL_STATIC_DRAW 0x88E4
#endif

#ifndef GL_DYNAMIC_DRAW
#define GL_DYNAMIC_DRAW 0x88E8
#endif

#ifndef GL_PROGRAM_POINT_SIZE
#define GL_PROGRAM_POINT_SIZE 0x8642
#endif

#ifndef GL_LINES
#define GL_LINES 0x0001
#endif

#ifndef GL_NEAREST
#define GL_NEAREST 0x2600
#endif

#ifndef GL_RGBA32F
#define GL_RGBA32F 0x8814
#endif

typedef void(APIENTRYP PFNPOINTMODGLBINDATTRIBLOCATIONPROC)(GLuint program, GLuint index, const GLchar* name);
typedef void(APIENTRYP PFNPOINTMODGLUNIFORM1FPROC)(GLint location, GLfloat v0);
typedef void(APIENTRYP PFNPOINTMODGLUNIFORM2FPROC)(GLint location, GLfloat v0, GLfloat v1);
typedef void(APIENTRYP PFNPOINTMODGLUNIFORM3FPROC)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
typedef void(APIENTRYP PFNPOINTMODGLUNIFORM1FVPROC)(GLint location, GLsizei count, const GLfloat* value);
typedef void(APIENTRYP PFNPOINTMODGLDRAWARRAYSPROC)(GLenum mode, GLint first, GLsizei count);
typedef void(APIENTRYP PFNPOINTMODGLBLENDFUNCPROC)(GLenum sfactor, GLenum dfactor);
typedef void(APIENTRYP PFNPOINTMODGLDEPTHMASKPROC)(GLboolean flag);

namespace pointmod::gl {

extern PFNPOINTMODGLBINDATTRIBLOCATIONPROC BindAttribLocation;
extern PFNPOINTMODGLUNIFORM1FPROC Uniform1f;
extern PFNPOINTMODGLUNIFORM2FPROC Uniform2f;
extern PFNPOINTMODGLUNIFORM3FPROC Uniform3f;
extern PFNPOINTMODGLUNIFORM1FVPROC Uniform1fv;
extern PFNPOINTMODGLDRAWARRAYSPROC DrawArrays;
extern PFNPOINTMODGLBLENDFUNCPROC BlendFunc;
extern PFNPOINTMODGLDEPTHMASKPROC DepthMask;

}  // namespace pointmod::gl

#define glBindAttribLocation pointmod::gl::BindAttribLocation
#define glUniform1f pointmod::gl::Uniform1f
#define glUniform2f pointmod::gl::Uniform2f
#define glUniform3f pointmod::gl::Uniform3f
#define glUniform1fv pointmod::gl::Uniform1fv
#define glDrawArrays pointmod::gl::DrawArrays
#define glBlendFunc pointmod::gl::BlendFunc
#define glDepthMask pointmod::gl::DepthMask
#endif

namespace pointmod {

bool InitializeOpenGLBindings();

}  // namespace pointmod
