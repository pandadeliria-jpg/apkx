#include "apkx/registry.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace apkx {

using nlohmann::json;

static InstalledApp app_from_json(const json& j) {
  InstalledApp a;
  a.package_name = j.value<std::string>("package_name", std::string{});
  a.app_name = j.value<std::string>("app_name", std::string{});
  a.version_code = j.value("version_code", 1);
  a.version_name = j.value<std::string>("version_name", std::string{"1.0"});
  a.install_path = j.value<std::string>("install_path", std::string{});
  a.data_path = j.value<std::string>("data_path", std::string{});
  a.cache_path = j.value<std::string>("cache_path", std::string{});
  a.apk_path = j.value<std::string>("apk_path", std::string{});
  a.install_time = j.value<std::string>("install_time", std::string{});
  if (j.contains("last_launch") && !j["last_launch"].is_null()) a.last_launch = j["last_launch"].get<std::string>();
  a.launch_count = j.value("launch_count", 0);
  a.main_activity = j.value<std::string>("main_activity", std::string{});
  a.application_name = j.value<std::string>("application_name", std::string{});
  if (j.contains("permissions") && j["permissions"].is_array()) a.permissions = j["permissions"].get<std::vector<std::string>>();
  if (j.contains("native_libs") && j["native_libs"].is_array()) a.native_libs = j["native_libs"].get<std::vector<std::string>>();
  if (j.contains("dex_files") && j["dex_files"].is_array()) a.dex_files = j["dex_files"].get<std::vector<std::string>>();
  if (j.contains("icon_path") && !j["icon_path"].is_null()) a.icon_path = j["icon_path"].get<std::string>();
  return a;
}

static json app_to_json(const InstalledApp& a) {
  json j;
  j["package_name"] = json(a.package_name);
  j["app_name"] = json(a.app_name);
  j["version_code"] = json((json::number_integer_t)a.version_code);
  j["version_name"] = json(a.version_name);
  j["install_path"] = json(a.install_path);
  j["data_path"] = json(a.data_path);
  j["cache_path"] = json(a.cache_path);
  j["apk_path"] = json(a.apk_path);
  j["install_time"] = json(a.install_time);
  j["last_launch"] = a.last_launch ? json(*a.last_launch) : json(nullptr);
  j["launch_count"] = json((json::number_integer_t)a.launch_count);
  j["main_activity"] = json(a.main_activity);
  j["application_name"] = json(a.application_name);

  auto perms = json::array();
  for (const auto& s : a.permissions) perms.get_array().push_back(json(s));
  j["permissions"] = perms;

  auto nlibs = json::array();
  for (const auto& s : a.native_libs) nlibs.get_array().push_back(json(s));
  j["native_libs"] = nlibs;

  auto dex = json::array();
  for (const auto& s : a.dex_files) dex.get_array().push_back(json(s));
  j["dex_files"] = dex;
  j["icon_path"] = a.icon_path ? json(*a.icon_path) : json(nullptr);
  return j;
}

Registry load_registry(const std::filesystem::path& json_path) {
  Registry r;
  std::ifstream f(json_path);
  if (!f) return r;
  std::ostringstream ss;
  ss << f.rdbuf();
  json j = json::parse(ss.str());
  if (!j.is_object()) return r;
  for (auto it = j.begin(); it != j.end(); ++it) {
    const std::string pkg = (*it).first;
    const json& val = (*it).second;
    if (!val.is_object()) continue;
    InstalledApp app = app_from_json(val);
    if (app.package_name.empty()) app.package_name = pkg;
    r.apps[app.package_name] = app;
  }
  return r;
}

void save_registry(const std::filesystem::path& json_path, const Registry& r) {
  std::filesystem::create_directories(json_path.parent_path());
  std::ofstream f(json_path, std::ios::trunc);
  if (!f) throw std::runtime_error("failed to write registry");
  json j = json::object();
  for (const auto& [pkg, app] : r.apps) {
    j[pkg] = app_to_json(app);
  }
  f << j.dump(2) << "\n";
}

} // namespace apkx

