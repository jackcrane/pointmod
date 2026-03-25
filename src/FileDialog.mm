#include "FileDialog.hpp"

#import <AppKit/AppKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

namespace pointmod {

std::optional<std::filesystem::path> OpenPointCloudDialog() {
  NSOpenPanel* panel = [NSOpenPanel openPanel];
  [panel setCanChooseFiles:YES];
  [panel setCanChooseDirectories:NO];
  [panel setAllowsMultipleSelection:NO];
  UTType* plyType = [UTType typeWithFilenameExtension:@"ply"];
  if (plyType != nil) {
    [panel setAllowedContentTypes:@[ plyType ]];
  }
  [panel setPrompt:@"Open"];
  [panel setTitle:@"Open Point Cloud"];

  const NSModalResponse response = [panel runModal];
  if (response != NSModalResponseOK) {
    return std::nullopt;
  }

  NSURL* url = panel.URL;
  if (url == nil) {
    return std::nullopt;
  }

  return std::filesystem::path([[url path] UTF8String]);
}

}  // namespace pointmod
