# pointmod

Minimal native macOS point cloud viewer scaffold built with Dear ImGui, GLFW and OpenGL.

Current scope:

- Native `File > Open…` menu wired through Cocoa.
- ASCII `.ply` point cloud loading on a background thread.
- Progressive chunk streaming so points appear on screen while the file is still loading.
- Chunked GPU uploads with adaptive full/balanced/interaction detail paths to keep giant clouds responsive.
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
- The default resident render budget is `80,000,000` points; larger files are sampled with a fixed stride while streaming.
- The UI reports FPS and the currently selected display detail level.
