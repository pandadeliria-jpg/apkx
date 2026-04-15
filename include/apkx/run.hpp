#pragma once

#include "apkx/paths.hpp"
#include "apkx/registry.hpp"

#include <string>

namespace apkx {

int run_package(const SandboxPaths& paths, Registry& reg, const std::string& package_name);
int uninstall_package(const SandboxPaths& paths, Registry& reg, const std::string& package_name);
void print_list(const Registry& reg);
void print_info(const Registry& reg, const std::string& package_name);

} // namespace apkx

