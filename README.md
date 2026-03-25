# pointmod

Minimal native macOS point cloud viewer scaffold built with Dear ImGui, GLFW and OpenGL.

Current scope:

- Native `File > Open…` menu wired through Cocoa.
- ASCII `.ply` point cloud loading on a background thread.
- Reservoir-sampled preview path so very large files do not try to upload every point to the GPU.
- Orbit/pan/zoom camera with a simple OpenGL point renderer.

Build:

```bash
cmake -S . -B build
cmake --build build
./build/pointmod
```

Notes:

- CMake is set to fetch GLFW and Dear ImGui during configure.
- The loader currently supports ASCII PLY vertex clouds only.
- The preview cap is `5,000,000` points; larger files are sampled into that bounded preview buffer.
