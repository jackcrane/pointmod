#include "CocoaMenu.hpp"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_NATIVE_INCLUDE_NONE
#include <windows.h>
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <unordered_map>
#include <utility>
#endif

namespace pointmod {

#if defined(_WIN32)

namespace {

constexpr UINT_PTR kOpenMenuCommandId = 1001;
constexpr UINT_PTR kSaveMenuCommandId = 1002;
constexpr UINT_PTR kResetViewMenuCommandId = 1003;
constexpr UINT_PTR kQuitMenuCommandId = 1004;

struct NativeMenuState {
  std::function<void()> onOpenRequested;
  std::function<void()> onSaveRequested;
  std::function<void()> onResetViewRequested;
  WNDPROC previousWindowProc = nullptr;
  HMENU menuBar = nullptr;
};

std::unordered_map<HWND, NativeMenuState>& NativeMenuStates() {
  static std::unordered_map<HWND, NativeMenuState> states;
  return states;
}

LRESULT CALLBACK NativeMenuWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
  auto& states = NativeMenuStates();
  const auto stateIt = states.find(hwnd);
  if (stateIt != states.end() && message == WM_COMMAND) {
    switch (LOWORD(wParam)) {
      case kOpenMenuCommandId:
        if (stateIt->second.onOpenRequested) {
          stateIt->second.onOpenRequested();
        }
        return 0;
      case kSaveMenuCommandId:
        if (stateIt->second.onSaveRequested) {
          stateIt->second.onSaveRequested();
        }
        return 0;
      case kResetViewMenuCommandId:
        if (stateIt->second.onResetViewRequested) {
          stateIt->second.onResetViewRequested();
        }
        return 0;
      case kQuitMenuCommandId:
        PostMessageW(hwnd, WM_CLOSE, 0, 0);
        return 0;
      default:
        break;
    }
  }

  if (stateIt != states.end() && stateIt->second.previousWindowProc != nullptr) {
    return CallWindowProcW(stateIt->second.previousWindowProc, hwnd, message, wParam, lParam);
  }

  return DefWindowProcW(hwnd, message, wParam, lParam);
}

}  // namespace

bool InstallNativeMenu(
  GLFWwindow* window,
  const std::function<void()>& onOpenRequested,
  const std::function<void()>& onSaveRequested,
  const std::function<void()>& onResetViewRequested) {
  if (window == nullptr) {
    return false;
  }

  HWND hwnd = glfwGetWin32Window(window);
  if (hwnd == nullptr) {
    return false;
  }

  UninstallNativeMenu(window);

  HMENU menuBar = CreateMenu();
  HMENU fileMenu = CreatePopupMenu();
  HMENU viewMenu = CreatePopupMenu();
  if (menuBar == nullptr || fileMenu == nullptr || viewMenu == nullptr) {
    if (viewMenu != nullptr) {
      DestroyMenu(viewMenu);
    }
    if (fileMenu != nullptr) {
      DestroyMenu(fileMenu);
    }
    if (menuBar != nullptr) {
      DestroyMenu(menuBar);
    }
    return false;
  }

  AppendMenuW(fileMenu, MF_STRING, kOpenMenuCommandId, L"&Open...\tCtrl+O");
  AppendMenuW(fileMenu, MF_STRING, kSaveMenuCommandId, L"&Save As...\tCtrl+S");
  AppendMenuW(fileMenu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(fileMenu, MF_STRING, kQuitMenuCommandId, L"E&xit");
  AppendMenuW(viewMenu, MF_STRING, kResetViewMenuCommandId, L"&Reset view");
  AppendMenuW(menuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(fileMenu), L"&File");
  AppendMenuW(menuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(viewMenu), L"&View");

  if (SetMenu(hwnd, menuBar) == FALSE) {
    DestroyMenu(menuBar);
    return false;
  }

  NativeMenuState state;
  state.onOpenRequested = onOpenRequested;
  state.onSaveRequested = onSaveRequested;
  state.onResetViewRequested = onResetViewRequested;
  state.menuBar = menuBar;
  state.previousWindowProc = reinterpret_cast<WNDPROC>(
    SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(NativeMenuWindowProc)));
  NativeMenuStates()[hwnd] = std::move(state);

  DrawMenuBar(hwnd);
  return true;
}

void UninstallNativeMenu(GLFWwindow* window) {
  if (window == nullptr) {
    return;
  }

  HWND hwnd = glfwGetWin32Window(window);
  if (hwnd == nullptr) {
    return;
  }

  auto& states = NativeMenuStates();
  const auto stateIt = states.find(hwnd);
  if (stateIt == states.end()) {
    return;
  }

  if (stateIt->second.previousWindowProc != nullptr) {
    SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(stateIt->second.previousWindowProc));
  }
  SetMenu(hwnd, nullptr);
  if (stateIt->second.menuBar != nullptr) {
    DestroyMenu(stateIt->second.menuBar);
  }
  DrawMenuBar(hwnd);
  states.erase(stateIt);
}

#else

bool InstallNativeMenu(GLFWwindow*, const std::function<void()>&, const std::function<void()>&, const std::function<void()>&) {
  return false;
}

void UninstallNativeMenu(GLFWwindow*) {
}

#endif

}  // namespace pointmod
