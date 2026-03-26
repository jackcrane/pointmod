#include "OpenGL.hpp"

#if !defined(__APPLE__)
namespace pointmod::gl {

PFNPOINTMODGLBINDATTRIBLOCATIONPROC BindAttribLocation = nullptr;
PFNPOINTMODGLUNIFORM1FPROC Uniform1f = nullptr;
PFNPOINTMODGLUNIFORM2FPROC Uniform2f = nullptr;
PFNPOINTMODGLUNIFORM3FPROC Uniform3f = nullptr;
PFNPOINTMODGLUNIFORM1FVPROC Uniform1fv = nullptr;
PFNPOINTMODGLDRAWARRAYSPROC DrawArrays = nullptr;
PFNPOINTMODGLBLENDFUNCPROC BlendFunc = nullptr;
PFNPOINTMODGLDEPTHMASKPROC DepthMask = nullptr;
PFNPOINTMODGLCULLFACEPROC CullFace = nullptr;

}  // namespace pointmod::gl
#endif

namespace pointmod {

bool InitializeOpenGLBindings() {
#if defined(__APPLE__)
  return true;
#else
  if (imgl3wInit() != 0) {
    return false;
  }

  gl::BindAttribLocation = reinterpret_cast<PFNPOINTMODGLBINDATTRIBLOCATIONPROC>(imgl3wGetProcAddress("glBindAttribLocation"));
  gl::Uniform1f = reinterpret_cast<PFNPOINTMODGLUNIFORM1FPROC>(imgl3wGetProcAddress("glUniform1f"));
  gl::Uniform2f = reinterpret_cast<PFNPOINTMODGLUNIFORM2FPROC>(imgl3wGetProcAddress("glUniform2f"));
  gl::Uniform3f = reinterpret_cast<PFNPOINTMODGLUNIFORM3FPROC>(imgl3wGetProcAddress("glUniform3f"));
  gl::Uniform1fv = reinterpret_cast<PFNPOINTMODGLUNIFORM1FVPROC>(imgl3wGetProcAddress("glUniform1fv"));
  gl::DrawArrays = reinterpret_cast<PFNPOINTMODGLDRAWARRAYSPROC>(imgl3wGetProcAddress("glDrawArrays"));
  gl::BlendFunc = reinterpret_cast<PFNPOINTMODGLBLENDFUNCPROC>(imgl3wGetProcAddress("glBlendFunc"));
  gl::DepthMask = reinterpret_cast<PFNPOINTMODGLDEPTHMASKPROC>(imgl3wGetProcAddress("glDepthMask"));
  gl::CullFace = reinterpret_cast<PFNPOINTMODGLCULLFACEPROC>(imgl3wGetProcAddress("glCullFace"));
  return gl::BindAttribLocation != nullptr &&
    gl::Uniform1f != nullptr &&
    gl::Uniform2f != nullptr &&
    gl::Uniform3f != nullptr &&
    gl::Uniform1fv != nullptr &&
    gl::DrawArrays != nullptr &&
    gl::BlendFunc != nullptr &&
    gl::DepthMask != nullptr &&
    gl::CullFace != nullptr;
#endif
}

}  // namespace pointmod
