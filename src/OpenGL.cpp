#include "OpenGL.hpp"

#if !defined(__APPLE__)
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace pointmod::gl {

#define POINTMOD_DEFINE_GL_FUNCTION(type, name) type name = nullptr;
POINTMOD_OPENGL_FUNCTIONS(POINTMOD_DEFINE_GL_FUNCTION)
#undef POINTMOD_DEFINE_GL_FUNCTION

}  // namespace pointmod::gl
#endif

namespace pointmod {

bool InitializeOpenGLBindings() {
#if defined(__APPLE__)
  return true;
#else
  bool loaded = true;

#define POINTMOD_LOAD_GL_FUNCTION(type, name) \
  gl::name = reinterpret_cast<type>(glfwGetProcAddress(#name)); \
  loaded = loaded && gl::name != nullptr;

  POINTMOD_OPENGL_FUNCTIONS(POINTMOD_LOAD_GL_FUNCTION)

#undef POINTMOD_LOAD_GL_FUNCTION

  return loaded;
#endif
}

}  // namespace pointmod
