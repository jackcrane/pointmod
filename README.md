# pointmod

Minimal native point cloud viewer scaffold built with Dear ImGui, GLFW and OpenGL.

Current scope:

- Native `File > Open…` menu wired through Cocoa on macOS and the common file picker on Windows.
- ASCII `.ply` point cloud loading on a background thread.
- Progressive chunk streaming so points appear on screen while the file is still loading.
- Chunked GPU uploads with adaptive full/balanced/interaction detail paths to keep giant clouds responsive.
- Orbit/pan/zoom camera with a simple OpenGL point renderer.

Build on macOS/Linux:

```bash
cmake -S . -B build
cmake --build build
./build/pointmod
```

Build on Windows from `cmd.exe`, PowerShell, or a Visual Studio Developer Prompt:

```bat
build-windows.bat
```

That writes the executable to either `build-windows\Release\pointmod.exe` or `build-windows\pointmod.exe`, depending on the generator.
A published GitHub Release also triggers `.github/workflows/release-windows.yml`, which uploads a portable `pointmod-windows-x64-portable.zip` asset to that release.

Notes:

- CMake is set to fetch GLFW and Dear ImGui during configure.
- MSVC builds statically link the C/C++ runtime so the zipped Windows executable does not require a separate VC++ redistributable install.
- Windows builds use the system file picker via `GetOpenFileNameW`.
- The loader currently supports ASCII PLY vertex clouds only.
- The default resident render budget is `80,000,000` points; larger files are sampled with a fixed stride while streaming.
- The UI reports FPS and the currently selected display detail level.
