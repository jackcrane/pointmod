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

std::optional<std::filesystem::path> SavePointCloudDialog(const std::filesystem::path& suggestedPath) {
  NSSavePanel* panel = [NSSavePanel savePanel];
  UTType* plyType = [UTType typeWithFilenameExtension:@"ply"];
  if (plyType != nil) {
    [panel setAllowedContentTypes:@[ plyType ]];
  }
  if (!suggestedPath.filename().empty()) {
    [panel setNameFieldStringValue:[NSString stringWithUTF8String:suggestedPath.filename().string().c_str()]];
  }
  if (!suggestedPath.parent_path().empty()) {
    NSURL* directoryUrl =
      [NSURL fileURLWithPath:[NSString stringWithUTF8String:suggestedPath.parent_path().string().c_str()]
                isDirectory:YES];
    [panel setDirectoryURL:directoryUrl];
  }
  [panel setCanCreateDirectories:YES];
  [panel setExtensionHidden:NO];
  [panel setPrompt:@"Export"];
  [panel setTitle:@"Export Point Cloud"];

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
