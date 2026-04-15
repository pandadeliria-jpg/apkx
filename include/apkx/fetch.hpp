#pragma once

#include <filesystem>
#include <string>

namespace apkx {

// Download URL to dest (overwrites).
void fetch_to_file(const std::string& url, const std::filesystem::path& dest);

} // namespace apkx

