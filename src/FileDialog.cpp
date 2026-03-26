#include "FileDialog.hpp"

#include <array>
#include <cstdlib>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#else
#include <cstdio>
#include <memory>
#include <string>
#endif

namespace pointmod {

#if defined(_WIN32)

std::optional<std::filesystem::path> OpenPointCloudDialog() {
  std::array<wchar_t, 32768> selectedPath{};
  OPENFILENAMEW dialogConfig{};
  dialogConfig.lStructSize = sizeof(dialogConfig);
  dialogConfig.lpstrFilter = L"Point Clouds (*.ply)\0*.ply\0All Files (*.*)\0*.*\0";
  dialogConfig.lpstrFile = selectedPath.data();
  dialogConfig.nMaxFile = static_cast<DWORD>(selectedPath.size());
  dialogConfig.lpstrTitle = L"Open Point Cloud";
  dialogConfig.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
  dialogConfig.lpstrDefExt = L"ply";

  if (GetOpenFileNameW(&dialogConfig) == FALSE) {
    return std::nullopt;
  }

  return std::filesystem::path(selectedPath.data());
}

std::optional<std::filesystem::path> SavePointCloudDialog(const std::filesystem::path& suggestedPath) {
  std::array<wchar_t, 32768> selectedPath{};
  const std::wstring suggestedPathString = suggestedPath.wstring();
  if (!suggestedPathString.empty()) {
    suggestedPathString.copy(selectedPath.data(), selectedPath.size() - 1);
  }

  OPENFILENAMEW dialogConfig{};
  dialogConfig.lStructSize = sizeof(dialogConfig);
  dialogConfig.lpstrFilter = L"Point Clouds (*.ply)\0*.ply\0All Files (*.*)\0*.*\0";
  dialogConfig.lpstrFile = selectedPath.data();
  dialogConfig.nMaxFile = static_cast<DWORD>(selectedPath.size());
  dialogConfig.lpstrTitle = L"Export Point Cloud";
  dialogConfig.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_EXPLORER;
  dialogConfig.lpstrDefExt = L"ply";

  if (GetSaveFileNameW(&dialogConfig) == FALSE) {
    return std::nullopt;
  }

  return std::filesystem::path(selectedPath.data());
}

#else

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

std::string ShellEscapeSingleQuoted(const std::string& value) {
  std::string escaped;
  escaped.reserve(value.size() + 2);
  escaped += '\'';
  for (char ch : value) {
    if (ch == '\'') {
      escaped += "'\\''";
    } else {
      escaped += ch;
    }
  }
  escaped += '\'';
  return escaped;
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

std::optional<std::filesystem::path> SavePointCloudDialog(const std::filesystem::path& suggestedPath) {
  const std::string escapedPath = ShellEscapeSingleQuoted(suggestedPath.string());
  const std::string zenityCommand =
    "zenity --file-selection --save --confirm-overwrite "
    "--title='Export Point Cloud' "
    "--filename=" + escapedPath + " "
    "--file-filter='Point Clouds | *.ply' 2>/dev/null";
  const std::string kdialogCommand =
    "kdialog --getsavefilename " + escapedPath + " '*.ply|Point Clouds (*.ply)' 2>/dev/null";

  if (std::optional<std::string> selectedPath = RunDialogCommand(zenityCommand.c_str())) {
    return std::filesystem::path(*selectedPath);
  }

  if (std::optional<std::string> selectedPath = RunDialogCommand(kdialogCommand.c_str())) {
    return std::filesystem::path(*selectedPath);
  }

  return std::nullopt;
}

#endif

}  // namespace pointmod
