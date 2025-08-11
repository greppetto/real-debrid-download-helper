#pragma once

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <mutex>
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

// The following class helps with parallelization of link unrestriction
class TokenBucket {
public:
  // Constructor
  TokenBucket(size_t capacity, double refill_rate_per_sec);

  // Consume token
  void consume();

private:
  // Refill bucket
  void refill();

  size_t capacity;
  size_t tokens;
  double refill_rate;
  std::chrono::steady_clock::time_point last_refill;
  std::mutex bucket_mtx;
  std::condition_variable cv;
};

} // namespace util
