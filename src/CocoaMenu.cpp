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
constexpr UINT_PTR kTaskManagerMenuCommandId = 1004;
constexpr UINT_PTR kQuitMenuCommandId = 1005;
constexpr UINT_PTR kYUpMenuCommandId = 1006;
constexpr UINT_PTR kNegativeYUpMenuCommandId = 1007;
constexpr UINT_PTR kZUpMenuCommandId = 1008;
constexpr UINT_PTR kNegativeZUpMenuCommandId = 1009;

struct NativeMenuState {
  std::function<void()> onOpenRequested;
  std::function<void()> onSaveRequested;
  std::function<void()> onResetViewRequested;
  std::function<void()> onTaskManagerRequested;
  std::function<void(int)> onSetUpAxisRequested;
  std::function<int()> selectedUpAxisIndex;
  WNDPROC previousWindowProc = nullptr;
  HMENU menuBar = nullptr;
  HMENU viewMenu = nullptr;
};

std::unordered_map<HWND, NativeMenuState>& NativeMenuStates() {
  static std::unordered_map<HWND, NativeMenuState> states;
  return states;
}

UINT_PTR MenuCommandIdForUpAxisIndex(int index) {
  switch (index) {
    case 0:
      return kYUpMenuCommandId;
    case 1:
      return kNegativeYUpMenuCommandId;
    case 2:
      return kZUpMenuCommandId;
    case 3:
      return kNegativeZUpMenuCommandId;
    default:
      return kZUpMenuCommandId;
  }
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
      case kTaskManagerMenuCommandId:
        if (stateIt->second.onTaskManagerRequested) {
          stateIt->second.onTaskManagerRequested();
        }
        return 0;
      case kYUpMenuCommandId:
      case kNegativeYUpMenuCommandId:
      case kZUpMenuCommandId:
      case kNegativeZUpMenuCommandId:
        if (stateIt->second.onSetUpAxisRequested) {
          int menuIndex = 2;
          if (LOWORD(wParam) == kYUpMenuCommandId) {
            menuIndex = 0;
          } else if (LOWORD(wParam) == kNegativeYUpMenuCommandId) {
            menuIndex = 1;
          } else if (LOWORD(wParam) == kZUpMenuCommandId) {
            menuIndex = 2;
          } else if (LOWORD(wParam) == kNegativeZUpMenuCommandId) {
            menuIndex = 3;
          }
          stateIt->second.onSetUpAxisRequested(menuIndex);
        }
        if (stateIt->second.viewMenu != nullptr) {
          CheckMenuRadioItem(
            stateIt->second.viewMenu,
            kYUpMenuCommandId,
            kNegativeZUpMenuCommandId,
            MenuCommandIdForUpAxisIndex(
              stateIt->second.selectedUpAxisIndex ? stateIt->second.selectedUpAxisIndex() : 2),
            MF_BYCOMMAND);
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
  const std::function<void()>& onResetViewRequested,
  const std::function<void()>& onTaskManagerRequested,
  const std::function<void(int)>& onSetUpAxisRequested,
  const std::function<int()>& selectedUpAxisIndex) {
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
  AppendMenuW(viewMenu, MF_STRING, kTaskManagerMenuCommandId, L"&Task manager");
  AppendMenuW(viewMenu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(viewMenu, MF_STRING, kYUpMenuCommandId, L"&Y-up");
  AppendMenuW(viewMenu, MF_STRING, kNegativeYUpMenuCommandId, L"&Negative Y-up");
  AppendMenuW(viewMenu, MF_STRING, kZUpMenuCommandId, L"&Z-up");
  AppendMenuW(viewMenu, MF_STRING, kNegativeZUpMenuCommandId, L"&Negative Z-up");
  CheckMenuRadioItem(
    viewMenu,
    kYUpMenuCommandId,
    kNegativeZUpMenuCommandId,
    MenuCommandIdForUpAxisIndex(selectedUpAxisIndex ? selectedUpAxisIndex() : 2),
    MF_BYCOMMAND);
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
  state.onTaskManagerRequested = onTaskManagerRequested;
  state.onSetUpAxisRequested = onSetUpAxisRequested;
  state.selectedUpAxisIndex = selectedUpAxisIndex;
  state.menuBar = menuBar;
  state.viewMenu = viewMenu;
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

bool InstallNativeMenu(
  GLFWwindow*,
  const std::function<void()>&,
  const std::function<void()>&,
  const std::function<void()>&,
  const std::function<void()>&,
  const std::function<void(int)>&,
  const std::function<int()>&) {
  return false;
}

void UninstallNativeMenu(GLFWwindow*) {
}

#endif

}  // namespace pointmod
