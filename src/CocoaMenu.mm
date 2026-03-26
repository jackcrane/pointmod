#include "CocoaMenu.hpp"

#import <AppKit/AppKit.h>

static std::function<void()> gOnOpenRequested;
static std::function<void()> gOnResetViewRequested;

@interface PointmodMenuTarget : NSObject
@end

@implementation PointmodMenuTarget

- (void)openDocument:(id)sender {
  if (gOnOpenRequested) {
    gOnOpenRequested();
  }
}

- (void)resetView:(id)sender {
  if (gOnResetViewRequested) {
    gOnResetViewRequested();
  }
}

@end

namespace pointmod {

void InstallNativeMenu(
  const std::function<void()>& onOpenRequested,
  const std::function<void()>& onResetViewRequested) {
  static PointmodMenuTarget* target = nil;

  gOnOpenRequested = onOpenRequested;
  gOnResetViewRequested = onResetViewRequested;

  [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

  if (target == nil) {
    target = [[PointmodMenuTarget alloc] init];
  }

  NSMenu* mainMenu = [[NSMenu alloc] init];
  [NSApp setMainMenu:mainMenu];

  NSMenuItem* appMenuItem = [[NSMenuItem alloc] init];
  [mainMenu addItem:appMenuItem];
  NSMenu* appMenu = [[NSMenu alloc] initWithTitle:@"pointmod"];
  NSString* quitTitle = [@"Quit pointmod" stringByAppendingString:@""];
  NSMenuItem* quitItem = [[NSMenuItem alloc] initWithTitle:quitTitle action:@selector(terminate:) keyEquivalent:@"q"];
  [appMenu addItem:quitItem];
  [mainMenu setSubmenu:appMenu forItem:appMenuItem];

  NSMenuItem* fileMenuItem = [[NSMenuItem alloc] init];
  [mainMenu addItem:fileMenuItem];

  NSMenu* fileMenu = [[NSMenu alloc] initWithTitle:@"File"];
  NSMenuItem* openItem = [[NSMenuItem alloc] initWithTitle:@"Open…" action:@selector(openDocument:) keyEquivalent:@"o"];
  [openItem setTarget:target];
  [fileMenu addItem:openItem];
  [mainMenu setSubmenu:fileMenu forItem:fileMenuItem];

  NSMenuItem* viewMenuItem = [[NSMenuItem alloc] init];
  [mainMenu addItem:viewMenuItem];

  NSMenu* viewMenu = [[NSMenu alloc] initWithTitle:@"View"];
  NSMenuItem* resetViewItem = [[NSMenuItem alloc] initWithTitle:@"Reset View" action:@selector(resetView:) keyEquivalent:@"0"];
  [resetViewItem setKeyEquivalentModifierMask:NSEventModifierFlagCommand];
  [resetViewItem setTarget:target];
  [viewMenu addItem:resetViewItem];
  [mainMenu setSubmenu:viewMenu forItem:viewMenuItem];
}

}  // namespace pointmod
