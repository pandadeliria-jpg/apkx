#include "apkx/paths.hpp"

#include <cstdlib>

namespace apkx {

static std::filesystem::path home_dir() {
  const char* home = std::getenv("HOME");
  if (home && *home) return std::filesystem::path(home);
  return std::filesystem::current_path();
}

SandboxPaths default_paths() {
  SandboxPaths p;
  p.root = home_dir() / ".android_compat";
  p.apps_root = p.root / "apps";
  p.installed_dir = p.apps_root / "installed";
  p.data_dir = p.apps_root / "data";
  p.cache_dir = p.apps_root / "cache";
  p.registry_json = p.apps_root / "apps.json";
  return p;
}

std::filesystem::path app_install_dir(const SandboxPaths& p, const std::string& package_name) {
  return p.installed_dir / package_name;
}
std::filesystem::path app_data_dir(const SandboxPaths& p, const std::string& package_name) {
  return p.data_dir / package_name;
}
std::filesystem::path app_cache_dir(const SandboxPaths& p, const std::string& package_name) {
  return p.cache_dir / package_name;
}

} // namespace apkx

