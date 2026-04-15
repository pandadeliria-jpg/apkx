#include "apkx/run.hpp"

#include "apkx/paths.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace apkx {
namespace fs = std::filesystem;

static std::string now_iso() {
  using namespace std::chrono;
  auto t = system_clock::to_time_t(system_clock::now());
  std::tm tm{};
#ifdef _WIN32
  localtime_s(&tm, &t);
#else
  localtime_r(&t, &tm);
#endif
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d",
                tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                tm.tm_hour, tm.tm_min, tm.tm_sec);
  return std::string(buf);
}

void print_list(const Registry& reg) {
  if (reg.apps.empty()) {
    std::cout << "No apps installed.\n";
    return;
  }
  for (const auto& [pkg, app] : reg.apps) {
    std::cout << pkg << "\n";
  }
}

void print_info(const Registry& reg, const std::string& package_name) {
  auto it = reg.apps.find(package_name);
  if (it == reg.apps.end()) throw std::runtime_error("not installed: " + package_name);
  const auto& a = it->second;
  std::cout << "Package: " << a.package_name << "\n";
  std::cout << "APK: " << a.apk_path << "\n";
  std::cout << "Install path: " << a.install_path << "\n";
  std::cout << "Data path: " << a.data_path << "\n";
  std::cout << "Cache path: " << a.cache_path << "\n";
  std::cout << "Install time: " << a.install_time << "\n";
  std::cout << "Launch count: " << a.launch_count << "\n";
  if (a.last_launch) std::cout << "Last launch: " << *a.last_launch << "\n";
}

int uninstall_package(const SandboxPaths& paths, Registry& reg, const std::string& package_name) {
  auto it = reg.apps.find(package_name);
  if (it == reg.apps.end()) throw std::runtime_error("not installed: " + package_name);
  std::error_code ec;
  fs::remove_all(app_install_dir(paths, package_name), ec);
  fs::remove_all(app_data_dir(paths, package_name), ec);
  fs::remove_all(app_cache_dir(paths, package_name), ec);
  reg.apps.erase(it);
  std::cout << "[apkx] uninstalled " << package_name << "\n";
  return 0;
}

// External function from app_execution.cpp
extern "C" int execute_android_app(const char* package, const char* apk_path, const char* data_dir);

int run_package(const SandboxPaths& paths, Registry& reg, const std::string& package_name) {
  auto it = reg.apps.find(package_name);
  if (it == reg.apps.end()) throw std::runtime_error("not installed: " + package_name);

  // Execute the Android app using our compatibility layer
  int result = execute_android_app(
    package_name.c_str(),
    it->second.install_path.c_str(),
    it->second.data_path.c_str()
  );

  // Update registry stats
  it->second.launch_count += 1;
  it->second.last_launch = now_iso();
  return result;
}

} // namespace apkx

