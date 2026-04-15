#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace apkx {

struct InstalledApp {
  std::string package_name;
  std::string app_name;
  int version_code = 1;
  std::string version_name = "1.0";
  std::string install_path;
  std::string data_path;
  std::string cache_path;
  std::string apk_path;
  std::string install_time; // ISO-ish string
  std::optional<std::string> last_launch;
  int launch_count = 0;
  std::string main_activity;
  std::string application_name;
  std::vector<std::string> permissions;
  std::vector<std::string> native_libs;
  std::vector<std::string> dex_files;
  std::optional<std::string> icon_path;
};

struct Registry {
  std::map<std::string, InstalledApp> apps; // package -> app
};

Registry load_registry(const std::filesystem::path& json_path);
void save_registry(const std::filesystem::path& json_path, const Registry& r);

} // namespace apkx

