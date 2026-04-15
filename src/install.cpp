#include "apkx/install.hpp"

#include "apkx/paths.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
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

static std::string guess_package_from_filename(const fs::path& apk_path) {
  // Mirrors python fallback in android_compat/runtime/app_manager.py
  auto stem = apk_path.stem().string();
  if (stem.find('.') != std::string::npos) return stem;
  return "com.app." + stem;
}

static void ensure_data_layout(const fs::path& data_dir) {
  const char* subdirs[] = {
      "files", "cache", "databases", "shared_prefs", "lib", "code_cache", "no_backup",
      "app_textures", "app_webview",
      "files/usr/bin", "files/usr/lib", "files/usr/etc", "files/usr/tmp",
      "files/usr/var/log", "files/usr/share", "files/usr/include", "files/home",
  };
  for (auto s : subdirs) fs::create_directories(data_dir / s);
}

InstalledApp install_apk(const SandboxPaths& paths, Registry& reg, const fs::path& apk_path) {
  if (!fs::exists(apk_path)) throw std::runtime_error("apk not found: " + apk_path.string());
  auto pkg = guess_package_from_filename(apk_path);

  auto install_dir = app_install_dir(paths, pkg);
  auto data_dir = app_data_dir(paths, pkg);
  auto cache_dir = app_cache_dir(paths, pkg);

  // Remove existing
  std::error_code ec;
  fs::remove_all(install_dir, ec);
  fs::remove_all(data_dir, ec);
  fs::remove_all(cache_dir, ec);

  fs::create_directories(install_dir);
  fs::create_directories(data_dir);
  fs::create_directories(cache_dir);

  ensure_data_layout(data_dir);

  InstalledApp app;
  app.package_name = pkg;
  app.app_name = pkg.substr(pkg.find_last_of('.') == std::string::npos ? 0 : pkg.find_last_of('.') + 1);
  app.install_path = install_dir.string();
  app.data_path = data_dir.string();
  app.cache_path = cache_dir.string();
  app.install_time = now_iso();

  // Copy APK to installation directory
  auto ext_str = apk_path.extension().string();
  auto installed_apk = install_dir / (pkg + ext_str);
  fs::copy_file(apk_path, installed_apk, fs::copy_options::overwrite_existing);

  // Extract APK contents:
  if (ext_str == ".xapk" || ext_str == ".apks" || ext_str == ".zip") {
      std::cout << "[apkx] Extracting bundle " << ext_str << "...\n";
      std::string cmd = "unzip -o -q \"" + installed_apk.string() + "\" -d \"" + install_dir.string() + "\"";
      system(cmd.c_str());
      
      // Look for the largest .apk file inside as the main one, or one named base.apk
      fs::path main_apk;
      uintmax_t max_size = 0;
      for (const auto& entry : fs::recursive_directory_iterator(install_dir)) {
          if (entry.path().extension() == ".apk") {
              if (entry.path().filename() == "base.apk") {
                  main_apk = entry.path();
                  break;
              }
              if (entry.file_size() > max_size) {
                  max_size = entry.file_size();
                  main_apk = entry.path();
              }
          }
      }
      if (!main_apk.empty()) {
          std::cout << "[apkx] Found main APK: " << main_apk.filename() << "\n";
          // We keep the bundle structure but track the main APK
          app.apk_path = main_apk.string();
      } else {
          app.apk_path = installed_apk.string();
      }
  } else {
      std::string cmd = "unzip -o -q \"" + installed_apk.string() + "\" -d \"" + install_dir.string() + "\"";
      int rc = std::system(cmd.c_str());
      if (rc != 0) throw std::runtime_error("failed to unzip apk (rc=" + std::to_string(rc) + ")");
      app.apk_path = installed_apk.string();
  }

  return app;
}

} // namespace apkx

