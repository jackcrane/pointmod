#include "FileDialog.hpp"

#include <array>
#include <cstdio>
#include <memory>
#include <string>

namespace pointmod {

namespace {

std::optional<std::string> RunDialogCommand(const char* command) {
  std::array<char, 1024> buffer{};
  std::string output;
  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command, "r"), pclose);
  if (!pipe) {
    return std::nullopt;
  }

  while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr) {
    output += buffer.data();
  }

  const int exitCode = pclose(pipe.release());
  if (exitCode != 0 || output.empty()) {
    return std::nullopt;
  }

  while (!output.empty() && (output.back() == '\n' || output.back() == '\r')) {
    output.pop_back();
  }

  if (output.empty()) {
    return std::nullopt;
  }

  return output;
}

}  // namespace

std::optional<std::filesystem::path> OpenPointCloudDialog() {
  constexpr const char* kZenityCommand =
    "zenity --file-selection --title='Open Point Cloud' "
    "--file-filter='Point Clouds | *.ply' 2>/dev/null";
  constexpr const char* kKDialogCommand =
    "kdialog --getopenfilename \"$PWD\" '*.ply|Point Clouds (*.ply)' 2>/dev/null";

  if (std::optional<std::string> selectedPath = RunDialogCommand(kZenityCommand)) {
    return std::filesystem::path(*selectedPath);
  }

  if (std::optional<std::string> selectedPath = RunDialogCommand(kKDialogCommand)) {
    return std::filesystem::path(*selectedPath);
  }

  return std::nullopt;
}

}  // namespace pointmod
