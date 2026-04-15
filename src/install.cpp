#include "apkx/install.hpp"

#include "apkx/paths.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
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

  // Copy APK to installation directory
  auto installed_apk = install_dir / (pkg + ".apk");
  fs::copy_file(apk_path, installed_apk, fs::copy_options::overwrite_existing);

  // Extract APK contents:
  // For now, we shell out to /usr/bin/unzip (present on macOS) to avoid vendoring a zip lib in scaffold step.
  // This keeps Phase 1 shippable; Phase 2 ports this to native C++ zip parsing.
  std::string cmd = "unzip -o -q \"" + installed_apk.string() + "\" -d \"" + install_dir.string() + "\"";
  int rc = std::system(cmd.c_str());
  if (rc != 0) throw std::runtime_error("failed to unzip apk (rc=" + std::to_string(rc) + ")");

  ensure_data_layout(data_dir);

  InstalledApp app;
  app.package_name = pkg;
  app.app_name = pkg.substr(pkg.find_last_of('.') == std::string::npos ? 0 : pkg.find_last_of('.') + 1);
  app.install_path = install_dir.string();
  app.data_path = data_dir.string();
  app.cache_path = cache_dir.string();
  app.apk_path = installed_apk.string();
  app.install_time = now_iso();

  return app;
}

} // namespace apkx

