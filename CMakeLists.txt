cmake_minimum_required(VERSION 3.25)

project(pointmod LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(APPLE)
  enable_language(OBJCXX)
endif()

option(POINTMOD_FETCH_DEPS "Fetch Dear ImGui and GLFW during CMake configure." ON)

include(FetchContent)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

if(UNIX AND NOT APPLE)
  set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_X11 ON CACHE BOOL "" FORCE)
endif()

if(POINTMOD_FETCH_DEPS)
  FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
    GIT_SHALLOW TRUE
  )

  FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.8
    GIT_SHALLOW TRUE
  )

  FetchContent_MakeAvailable(glfw imgui)
else()
  message(FATAL_ERROR "POINTMOD_FETCH_DEPS=OFF is not wired yet. Either turn it back ON or vendor GLFW/ImGui first.")
endif()

add_library(pointmod_imgui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_demo.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(pointmod_imgui
  PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(pointmod_imgui PUBLIC glfw)

if(APPLE)
  target_compile_definitions(pointmod_imgui PRIVATE GL_SILENCE_DEPRECATION)
endif()

set(POINTMOD_PLATFORM_SOURCES
  src/CocoaMenu.cpp
  src/FileDialog.cpp
)

set(POINTMOD_PLATFORM_LIBS "")

if(APPLE)
  set(POINTMOD_PLATFORM_SOURCES
    src/CocoaMenu.mm
    src/FileDialog.mm
  )

  find_library(COCOA_FRAMEWORK Cocoa REQUIRED)
  find_library(OPENGL_FRAMEWORK OpenGL REQUIRED)
  find_library(IOKIT_FRAMEWORK IOKit REQUIRED)
  find_library(COREVIDEO_FRAMEWORK CoreVideo REQUIRED)
  find_library(UNIFORMTYPEIDENTIFIERS_FRAMEWORK UniformTypeIdentifiers REQUIRED)

  list(APPEND POINTMOD_PLATFORM_LIBS
    ${COCOA_FRAMEWORK}
    ${OPENGL_FRAMEWORK}
    ${IOKIT_FRAMEWORK}
    ${COREVIDEO_FRAMEWORK}
    ${UNIFORMTYPEIDENTIFIERS_FRAMEWORK}
  )
else()
  find_package(OpenGL REQUIRED)
  list(APPEND POINTMOD_PLATFORM_LIBS OpenGL::GL)
endif()

if(WIN32)
  list(APPEND POINTMOD_PLATFORM_LIBS Comdlg32)
endif()

add_executable(pointmod
  src/main.cpp
  src/Application.cpp
  src/AsyncPointCloudLoader.cpp
  src/OpenGL.cpp
  src/OrbitCamera.cpp
  src/PointCloud.cpp
  src/PlyAsciiLoader.cpp
  src/PointCloudRenderer.cpp
  ${POINTMOD_PLATFORM_SOURCES}
)

target_include_directories(pointmod PRIVATE src)

if(APPLE)
  target_compile_definitions(pointmod
    PRIVATE
      GL_SILENCE_DEPRECATION
  )
endif()

target_link_libraries(pointmod
  PRIVATE
    pointmod_imgui
    ${POINTMOD_PLATFORM_LIBS}
)
