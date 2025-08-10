#pragma once

#include <cstdlib>
#include <iostream>
#include <string>

namespace util {

bool validate_magnet_link(const std::string& magnet);

inline void set_env_variable(const std::string& key, const std::string& value) {
#ifdef _WIN32
  _putenv_s(key.c_str(), value.c_str());
#else
  setenv(key.c_str(), value.c_str(), 1);
#endif
}

void load_env_file(const std::string& path);

inline std::string get_rd_token() {
#ifndef NDEBUG
  // Load .env file only in debug/dev builds
  load_env_file(".env");
#endif
  const char* raw_token = std::getenv("REAL_DEBRID_API_TOKEN");
  if (raw_token == nullptr) {
    std::cerr << "[ERROR] No API token found!\n";
    return std::string{};
    // throw std::runtime_error("REAL_DEBRID_API_TOKEN not set.");
  }
  return std::string{raw_token};
}

} // namespace util
