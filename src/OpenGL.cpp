#include "OpenGL.hpp"

#if !defined(__APPLE__)
namespace pointmod::gl {

PFNPOINTMODGLBINDATTRIBLOCATIONPROC BindAttribLocation = nullptr;
PFNPOINTMODGLUNIFORM1FPROC Uniform1f = nullptr;
PFNPOINTMODGLDRAWARRAYSPROC DrawArrays = nullptr;

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
  gl::DrawArrays = reinterpret_cast<PFNPOINTMODGLDRAWARRAYSPROC>(imgl3wGetProcAddress("glDrawArrays"));
  return gl::BindAttribLocation != nullptr &&
    gl::Uniform1f != nullptr &&
    gl::DrawArrays != nullptr;
#endif
}

}  // namespace pointmod
