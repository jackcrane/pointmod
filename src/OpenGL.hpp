#pragma once

#if defined(__APPLE__)
#include <OpenGL/gl3.h>
#else
#include <backends/imgui_impl_opengl3_loader.h>
#endif

namespace pointmod {

bool InitializeOpenGLBindings();

}  // namespace pointmod
