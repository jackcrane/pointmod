#include "Application.hpp"

#include <cstdio>
#include <exception>

int main() {
  try {
    pointmod::Application app;
    return app.Run();
  } catch (const std::exception& exception) {
    std::fprintf(stderr, "pointmod failed: %s\n", exception.what());
    return 1;
  }
}
