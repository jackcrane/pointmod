#include "CocoaMenu.hpp"

#import <AppKit/AppKit.h>

#include <unordered_map>

#if defined(__APPLE__)

namespace {

struct NativeMenuState {
  std::function<void()> onOpenRequested;
  std::function<void()> onSaveRequested;
  std::function<void()> onResetViewRequested;
  std::function<void()> onTaskManagerRequested;
  std::function<void(int)> onSetUpAxisRequested;
  std::function<int()> selectedUpAxisIndex;
  NSMenu* previousMainMenu = nil;
  id handler = nil;
};

std::unordered_map<GLFWwindow*, NativeMenuState>& NativeMenuStates() {
  static std::unordered_map<GLFWwindow*, NativeMenuState> states;
  return states;
}

NSMenuItem* AddMenuItem(
  NSMenu* menu,
  NSString* title,
  SEL action,
  NSString* keyEquivalent,
  NSEventModifierFlags modifierFlags,
  id target) {
  NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:title action:action keyEquivalent:keyEquivalent];
  [item setTarget:target];
  [item setKeyEquivalentModifierMask:modifierFlags];
  [menu addItem:item];
  return item;
}

}  // namespace

@interface PointmodMenuHandler : NSObject {
 @public
  GLFWwindow* window_;
}
- (IBAction)handleOpen:(id)sender;
- (IBAction)handleSave:(id)sender;
- (IBAction)handleResetView:(id)sender;
- (IBAction)handleTaskManager:(id)sender;
- (IBAction)handleSetYUp:(id)sender;
- (IBAction)handleSetNegativeYUp:(id)sender;
- (IBAction)handleSetZUp:(id)sender;
- (IBAction)handleSetNegativeZUp:(id)sender;
@end

@implementation PointmodMenuHandler

- (IBAction)handleOpen:(id)sender {
  (void)sender;
  const auto stateIt = NativeMenuStates().find(window_);
  if (stateIt != NativeMenuStates().end() && stateIt->second.onOpenRequested) {
    stateIt->second.onOpenRequested();
  }
}

- (IBAction)handleSave:(id)sender {
  (void)sender;
  const auto stateIt = NativeMenuStates().find(window_);
  if (stateIt != NativeMenuStates().end() && stateIt->second.onSaveRequested) {
    stateIt->second.onSaveRequested();
  }
}

- (IBAction)handleResetView:(id)sender {
  (void)sender;
  const auto stateIt = NativeMenuStates().find(window_);
  if (stateIt != NativeMenuStates().end() && stateIt->second.onResetViewRequested) {
    stateIt->second.onResetViewRequested();
  }
}

- (IBAction)handleTaskManager:(id)sender {
  (void)sender;
  const auto stateIt = NativeMenuStates().find(window_);
  if (stateIt != NativeMenuStates().end() && stateIt->second.onTaskManagerRequested) {
    stateIt->second.onTaskManagerRequested();
  }
}

- (IBAction)handleSetYUp:(id)sender {
  (void)sender;
  const auto stateIt = NativeMenuStates().find(window_);
  if (stateIt != NativeMenuStates().end() && stateIt->second.onSetUpAxisRequested) {
    stateIt->second.onSetUpAxisRequested(0);
  }
}

- (IBAction)handleSetNegativeYUp:(id)sender {
  (void)sender;
  const auto stateIt = NativeMenuStates().find(window_);
  if (stateIt != NativeMenuStates().end() && stateIt->second.onSetUpAxisRequested) {
    stateIt->second.onSetUpAxisRequested(1);
  }
}

- (IBAction)handleSetZUp:(id)sender {
  (void)sender;
  const auto stateIt = NativeMenuStates().find(window_);
  if (stateIt != NativeMenuStates().end() && stateIt->second.onSetUpAxisRequested) {
    stateIt->second.onSetUpAxisRequested(2);
  }
}

- (IBAction)handleSetNegativeZUp:(id)sender {
  (void)sender;
  const auto stateIt = NativeMenuStates().find(window_);
  if (stateIt != NativeMenuStates().end() && stateIt->second.onSetUpAxisRequested) {
    stateIt->second.onSetUpAxisRequested(3);
  }
}

- (BOOL)validateMenuItem:(NSMenuItem*)menuItem {
  const auto stateIt = NativeMenuStates().find(window_);
  if (stateIt == NativeMenuStates().end()) {
    return YES;
  }

  if (
    menuItem.action == @selector(handleSetYUp:) ||
    menuItem.action == @selector(handleSetNegativeYUp:) ||
    menuItem.action == @selector(handleSetZUp:) ||
    menuItem.action == @selector(handleSetNegativeZUp:)) {
    const int selectedIndex = stateIt->second.selectedUpAxisIndex ? stateIt->second.selectedUpAxisIndex() : 2;
    int menuIndex = 2;
    if (menuItem.action == @selector(handleSetYUp:)) {
      menuIndex = 0;
    } else if (menuItem.action == @selector(handleSetNegativeYUp:)) {
      menuIndex = 1;
    } else if (menuItem.action == @selector(handleSetZUp:)) {
      menuIndex = 2;
    } else if (menuItem.action == @selector(handleSetNegativeZUp:)) {
      menuIndex = 3;
    }
    menuItem.state = selectedIndex == menuIndex ? NSControlStateValueOn : NSControlStateValueOff;
  }

  return YES;
}

@end

namespace pointmod {

bool InstallNativeMenu(
  GLFWwindow* window,
  const std::function<void()>& onOpenRequested,
  const std::function<void()>& onSaveRequested,
  const std::function<void()>& onResetViewRequested,
  const std::function<void()>& onTaskManagerRequested,
  const std::function<void(int)>& onSetUpAxisRequested,
  const std::function<int()>& selectedUpAxisIndex) {
  if (window == nullptr || NSApp == nil) {
    return false;
  }

  UninstallNativeMenu(window);

  PointmodMenuHandler* handler = [[PointmodMenuHandler alloc] init];
  handler->window_ = window;

  NSString* processName = [[NSProcessInfo processInfo] processName];
  NSMenu* mainMenu = [[NSMenu alloc] initWithTitle:@""];

  NSMenuItem* appMenuItem = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
  [mainMenu addItem:appMenuItem];
  NSMenu* appMenu = [[NSMenu alloc] initWithTitle:processName];
  [appMenuItem setSubmenu:appMenu];
  AddMenuItem(
    appMenu,
    [NSString stringWithFormat:@"Quit %@", processName],
    @selector(terminate:),
    @"q",
    NSEventModifierFlagCommand,
    nil);

  NSMenuItem* fileMenuItem = [[NSMenuItem alloc] initWithTitle:@"File" action:nil keyEquivalent:@""];
  [mainMenu addItem:fileMenuItem];
  NSMenu* fileMenu = [[NSMenu alloc] initWithTitle:@"File"];
  [fileMenuItem setSubmenu:fileMenu];
  AddMenuItem(fileMenu, @"Open...", @selector(handleOpen:), @"o", NSEventModifierFlagCommand, handler);
  AddMenuItem(fileMenu, @"Save As...", @selector(handleSave:), @"s", NSEventModifierFlagCommand, handler);

  NSMenuItem* viewMenuItem = [[NSMenuItem alloc] initWithTitle:@"View" action:nil keyEquivalent:@""];
  [mainMenu addItem:viewMenuItem];
  NSMenu* viewMenu = [[NSMenu alloc] initWithTitle:@"View"];
  [viewMenuItem setSubmenu:viewMenu];
  AddMenuItem(viewMenu, @"Reset View", @selector(handleResetView:), @"r", NSEventModifierFlagCommand, handler);
  AddMenuItem(viewMenu, @"Task Manager", @selector(handleTaskManager:), @"", 0, handler);
  [viewMenu addItem:[NSMenuItem separatorItem]];
  AddMenuItem(viewMenu, @"Y-up", @selector(handleSetYUp:), @"", 0, handler);
  AddMenuItem(viewMenu, @"Negative Y-up", @selector(handleSetNegativeYUp:), @"", 0, handler);
  AddMenuItem(viewMenu, @"Z-up", @selector(handleSetZUp:), @"", 0, handler);
  AddMenuItem(viewMenu, @"Negative Z-up", @selector(handleSetNegativeZUp:), @"", 0, handler);

  NativeMenuState state;
  state.onOpenRequested = onOpenRequested;
  state.onSaveRequested = onSaveRequested;
  state.onResetViewRequested = onResetViewRequested;
  state.onTaskManagerRequested = onTaskManagerRequested;
  state.onSetUpAxisRequested = onSetUpAxisRequested;
  state.selectedUpAxisIndex = selectedUpAxisIndex;
  state.previousMainMenu = [NSApp mainMenu];
  state.handler = handler;
  NativeMenuStates()[window] = std::move(state);

  [NSApp setMainMenu:mainMenu];
  return true;
}

void UninstallNativeMenu(GLFWwindow* window) {
  if (window == nullptr) {
    return;
  }

  auto& states = NativeMenuStates();
  const auto stateIt = states.find(window);
  if (stateIt == states.end()) {
    return;
  }

  if (NSApp != nil) {
    [NSApp setMainMenu:stateIt->second.previousMainMenu];
  }
  states.erase(stateIt);
}

}  // namespace pointmod

#else

namespace pointmod {

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

}  // namespace pointmod

#endif
