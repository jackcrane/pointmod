#include "OpenGL.hpp"

namespace pointmod {

bool InitializeOpenGLBindings() {
#if defined(__APPLE__)
  return true;
#else
  return imgl3wInit() == 0;
#endif
}

}  // namespace pointmod
