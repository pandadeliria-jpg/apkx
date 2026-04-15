#pragma once

#include "apkx/paths.hpp"
#include "apkx/registry.hpp"

#include <filesystem>
#include <string>

namespace apkx {

// Minimal install: copy APK, unzip to install_dir, set up data/cache layout, update registry.
InstalledApp install_apk(const SandboxPaths& paths, Registry& reg, const std::filesystem::path& apk_path);

} // namespace apkx

