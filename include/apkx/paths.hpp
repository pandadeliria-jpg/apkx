#pragma once

#include <filesystem>
#include <string>

namespace apkx {

struct SandboxPaths {
  std::filesystem::path root;          // ~/.android_compat
  std::filesystem::path apps_root;     // ~/.android_compat/apps
  std::filesystem::path installed_dir; // ~/.android_compat/apps/installed
  std::filesystem::path data_dir;      // ~/.android_compat/apps/data
  std::filesystem::path cache_dir;     // ~/.android_compat/apps/cache
  std::filesystem::path registry_json; // ~/.android_compat/apps/apps.json
};

SandboxPaths default_paths();

std::filesystem::path app_install_dir(const SandboxPaths& p, const std::string& package_name);
std::filesystem::path app_data_dir(const SandboxPaths& p, const std::string& package_name);
std::filesystem::path app_cache_dir(const SandboxPaths& p, const std::string& package_name);

} // namespace apkx

