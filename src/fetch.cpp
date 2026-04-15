#include "apkx/fetch.hpp"

#include <curl/curl.h>

#include <cstdio>
#include <filesystem>
#include <stdexcept>

namespace apkx {

static size_t write_cb(void* ptr, size_t size, size_t nmemb, void* stream) {
  return std::fwrite(ptr, size, nmemb, static_cast<FILE*>(stream));
}

void fetch_to_file(const std::string& url, const std::filesystem::path& dest) {
  CURL* curl = curl_easy_init();
  if (!curl) throw std::runtime_error("curl_easy_init failed");

  std::filesystem::create_directories(dest.parent_path());
  FILE* fp = std::fopen(dest.string().c_str(), "wb");
  if (!fp) {
    curl_easy_cleanup(curl);
    throw std::runtime_error("failed to open dest for write");
  }

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);

  CURLcode res = curl_easy_perform(curl);
  std::fclose(fp);
  curl_easy_cleanup(curl);
  if (res != CURLE_OK) {
    throw std::runtime_error(std::string("curl_easy_perform failed: ") + curl_easy_strerror(res));
  }
}

} // namespace apkx

