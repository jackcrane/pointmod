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

#ifndef GL_PROGRAM_POINT_SIZE
#define GL_PROGRAM_POINT_SIZE 0x8642
#endif

typedef void(APIENTRYP PFNPOINTMODGLBINDATTRIBLOCATIONPROC)(GLuint program, GLuint index, const GLchar* name);
typedef void(APIENTRYP PFNPOINTMODGLUNIFORM1FPROC)(GLint location, GLfloat v0);
typedef void(APIENTRYP PFNPOINTMODGLDRAWARRAYSPROC)(GLenum mode, GLint first, GLsizei count);

namespace pointmod::gl {

extern PFNPOINTMODGLBINDATTRIBLOCATIONPROC BindAttribLocation;
extern PFNPOINTMODGLUNIFORM1FPROC Uniform1f;
extern PFNPOINTMODGLDRAWARRAYSPROC DrawArrays;

}  // namespace pointmod::gl

#define glBindAttribLocation pointmod::gl::BindAttribLocation
#define glUniform1f pointmod::gl::Uniform1f
#define glDrawArrays pointmod::gl::DrawArrays
#endif

namespace pointmod {

bool InitializeOpenGLBindings();

}  // namespace pointmod
