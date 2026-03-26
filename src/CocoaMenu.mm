#include "CocoaMenu.hpp"

#import <AppKit/AppKit.h>

#include <unordered_map>

#if defined(__APPLE__)

namespace {

struct NativeMenuState {
  std::function<void()> onOpenRequested;
  std::function<void()> onSaveRequested;
  std::function<void()> onResetViewRequested;
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

@end

namespace pointmod {

bool InstallNativeMenu(
  GLFWwindow* window,
  const std::function<void()>& onOpenRequested,
  const std::function<void()>& onSaveRequested,
  const std::function<void()>& onResetViewRequested) {
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

  NativeMenuState state;
  state.onOpenRequested = onOpenRequested;
  state.onSaveRequested = onSaveRequested;
  state.onResetViewRequested = onResetViewRequested;
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

bool InstallNativeMenu(GLFWwindow*, const std::function<void()>&, const std::function<void()>&, const std::function<void()>&) {
  return false;
}

void UninstallNativeMenu(GLFWwindow*) {
}

}  // namespace pointmod

#endif
